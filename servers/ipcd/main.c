#include <capability.h>
#include <ipccalls.h>
#include <priority.h>

struct ipc_process {
  pid_t pid;
  char name[6];
  bool started;
};

static struct ipc_process processes[64];
static size_t blocked_processes;

static void unblock_all(void) {
  for (size_t i = 0; i < blocked_processes; i++) {
    ipc_unblock(0);
  }
  blocked_processes = 0;
}
int64_t registration_handler(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
  if (!has_ipc_caller_capability(CAP_NAMESPACE_SERVERS, CAP_IPCD_REGISTER)) {
    return -IPC_ERR_INSUFFICIENT_PRIVILEGE;
  }
  pid_t pid = get_caller_spawned_pid();
  if (!pid) {
    return -IPC_ERR_PROGRAM_DEFINED;
  }
  for (size_t i = 0; i < 64; i++) {
    if (!processes[i].pid || (processes[i].name[0] == (char) arg0 && processes[i].name[1] == (char) arg1 && processes[i].name[2] == (char) arg2 && processes[i].name[3] == (char) arg3 && processes[i].name[4] == (char) arg4)) {
      processes[i].pid = pid;
      processes[i].name[0] = arg0;
      processes[i].name[1] = arg1;
      processes[i].name[2] = arg2;
      processes[i].name[3] = arg3;
      processes[i].name[4] = arg4;
      return 0;
    }
  }
  return -IPC_ERR_PROGRAM_DEFINED;
}
int64_t set_started_handler(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
  if (arg0 || arg1 || arg2 || arg3 || arg4) {
    return -IPC_ERR_INVALID_ARGUMENTS;
  }
  pid_t pid = get_ipc_caller_pid();
  for (size_t i = 0; i < 64; i++) {
    if (processes[i].pid == pid && !processes[i].started) {
      processes[i].started = 1;
      unblock_all();
      return 0;
    }
  }
  return -IPC_ERR_PROGRAM_DEFINED;
}
int64_t discovery_handler(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
  for (size_t i = 0; i < 64; i++) {
    if (processes[i].name[0] == (char) arg0 && processes[i].name[1] == (char) arg1 && processes[i].name[2] == (char) arg2 && processes[i].name[3] == (char) arg3 && processes[i].name[4] == (char) arg4) {
      if (processes[i].started) {
        return processes[i].pid;
      } else {
        blocked_processes++;
        return -IPC_ERR_BLOCK;
      }
    }
  }
  return -IPC_ERR_INVALID_ARGUMENTS;
}
int main(void) {
  change_priority(PRIORITY_SYSTEM_HIGH);
  drop_capability(CAP_NAMESPACE_KERNEL, CAP_KERNEL_PRIORITY);
  register_ipc(0);
  ipc_handlers[IPC_IPCD_REGISTER] = registration_handler;
  ipc_handlers[IPC_IPCD_SET_STARTED] = set_started_handler;
  ipc_handlers[IPC_IPCD_DISCOVER] = discovery_handler;
  while (1) {
    handle_ipc();
  }
}
