#include <capability.h>
#include <ipccalls.h>
#include <linked_list.h>
#include <priority.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>

struct environment_var {
  struct linked_list_member list_member;
  pid_t pid;
  size_t num;
  size_t size;
  void* value;
};

struct linked_list envs_list;

static int64_t get_num_envs_handler(__attribute__((unused)) uint64_t arg0, __attribute__((unused)) uint64_t arg1, __attribute__((unused)) uint64_t arg2, __attribute__((unused)) uint64_t arg3, __attribute__((unused)) uint64_t arg4) {
  pid_t caller_pid = get_ipc_caller_pid();
  size_t num_envs = 0;
  for (struct environment_var* env = (struct environment_var*) envs_list.first; env; env = (struct environment_var*) env->list_member.next) {
    if (env->pid == caller_pid) {
      num_envs++;
    }
  }
  return num_envs;
}
static int64_t get_env_size_handler(uint64_t num, __attribute__((unused)) uint64_t arg1, __attribute__((unused)) uint64_t arg2, __attribute__((unused)) uint64_t arg3, __attribute__((unused)) uint64_t arg4) {
  pid_t caller_pid = get_ipc_caller_pid();
  for (struct environment_var* env = (struct environment_var*) envs_list.first; env; env = (struct environment_var*) env->list_member.next) {
    if (env->pid == caller_pid && env->num == num) {
      return env->size;
    }
  }
  syslog(LOG_DEBUG, "Invalid environment variable number");
  return -IPC_ERR_INVALID_ARGUMENTS;
}
static int64_t get_env_handler(uint64_t num, __attribute__((unused)) uint64_t arg1, __attribute__((unused)) uint64_t arg2, uint64_t address, uint64_t size) {
  pid_t caller_pid = get_ipc_caller_pid();
  for (struct environment_var* env = (struct environment_var*) envs_list.first; env; env = (struct environment_var*) env->list_member.next) {
    if (env->pid == caller_pid && env->num == num) {
      if (env->size != size) {
        syslog(LOG_DEBUG, "Invalid environment variable size");
        return -IPC_ERR_INVALID_ARGUMENTS;
      }
      memcpy((void*) address, env->value, size);
      free(env->value);
      remove_linked_list(&envs_list, &env->list_member);
      free(env);
      return 0;
    }
  }
  syslog(LOG_DEBUG, "Invalid environment variable number");
  return -IPC_ERR_INVALID_ARGUMENTS;
}
static int64_t add_env_handler(__attribute__((unused)) uint64_t arg0, __attribute__((unused)) uint64_t arg1, __attribute__((unused)) uint64_t arg2, uint64_t address, uint64_t size) {
  pid_t pid = get_caller_spawned_pid();
  if (!pid) {
    syslog(LOG_DEBUG, "Not currently spawning a process");
    return -IPC_ERR_PROGRAM_DEFINED;
  }
  char* buffer = (char*) address;
  if (buffer[size - 1]) {
    syslog(LOG_DEBUG, "Buffer isn't null terminated");
    return -IPC_ERR_INVALID_ARGUMENTS;
  }
  if (!strchr(buffer, '=')) {
    syslog(LOG_DEBUG, "Buffer doesn't contain separator");
    return -IPC_ERR_INVALID_ARGUMENTS;
  }
  struct environment_var* env = calloc(1, sizeof(struct environment_var));
  env->pid = pid;
  size_t num_envs = 0;
  for (struct environment_var* env_i = (struct environment_var*) envs_list.first; env_i; env_i = (struct environment_var*) env_i->list_member.next) {
    if (env_i->pid == pid) {
      num_envs++;
    }
  }
  env->num = num_envs;
  env->size = size;
  env->value = strdup(buffer);
  insert_linked_list(&envs_list, &env->list_member);
  return 0;
}
int main(void) {
  change_priority(PRIORITY_SYSTEM_HIGH);
  drop_capability(CAP_NAMESPACE_KERNEL, CAP_KERNEL_PRIORITY);
  listen_exits();
  drop_capability(CAP_NAMESPACE_KERNEL, CAP_KERNEL_LISTEN_EXITS);
  register_ipc(true);
  ipc_set_started();
  register_ipc_call(IPC_ENVD_GET_NUM, get_num_envs_handler, 0);
  register_ipc_call(IPC_ENVD_GET_SIZE, get_env_size_handler, 1);
  register_ipc_call(IPC_ENVD_ADD, add_env_handler, 0);
  register_ipc_call(IPC_ENVD_GET, get_env_handler, 1);
  while (true) {
    handle_ipc();
    pid_t pid;
    while ((pid = get_exited_pid())) {
      struct environment_var* next_env;
      for (struct environment_var* env = (struct environment_var*) envs_list.first; env; env = next_env) {
        next_env = (struct environment_var*) env->list_member.next;
        if (env->pid == pid) {
          free(env->value);
          remove_linked_list(&envs_list, &env->list_member);
          free(env);
        }
      }
    }
  }
}
