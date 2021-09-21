#include <__/syscall.h>
#include <stddef.h>

extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

//TODO: Exit all threads
_Noreturn void exit(int status) {
  for (long i = __fini_array_end - __fini_array_start - 1; i >= 0; i--) {
    __fini_array_start[i]();
  }
  _syscall(_SYSCALL_EXIT, status, 0, 0, 0, 0);
  while (1)
    ;
}
