#include <__/syscall.h>
#include <unistd.h>

pid_t get_caller_spawned_pid(void) {
  return _syscall(_SYSCALL_GET_CALLER_SPAWNED_PID, 0, 0, 0, 0, 0);
}
