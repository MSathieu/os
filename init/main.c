#include <__/syscall.h>
#include <capability.h>
#include <ipccalls.h>
#include <memory.h>
#include <priority.h>
#include <spawn.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct service {
  char* name;
  bool raw;
  pid_t pid;
  size_t capabilities[64];
  char* ipc_name;
};

struct service services[] = {
  {"acpid", 1, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_IOPORT | 1 << CAP_KERNEL_MAP_MEMORY | 1 << CAP_KERNEL_ACPI, [CAP_NAMESPACE_DRIVERS] = 1 << CAP_PCID_ACCESS}, "acpid"},
  {"argd", 1, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_LISTEN_EXITS}, "argd"},
  {"atad", 1, 0, {[CAP_NAMESPACE_FILESYSTEMS] = 1 << CAP_VFSD_MOUNT}, 0},
  {"ipcd", 1, 0, {}, 0},
  {"logd", 1, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_LOG, [CAP_NAMESPACE_SERVERS] = 1 << CAP_LOGD}, "logd"},
  {"pcid", 1, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_IOPORT | 1 << CAP_KERNEL_IRQ | 1 << CAP_KERNEL_MAP_MEMORY}, "pcid"},
  {"/sbin/devd", 0, 0, {[CAP_NAMESPACE_FILESYSTEMS] = 1 << CAP_DEVD}, "devd"},
  {"/sbin/dev-nulld", 0, 0, {}, 0},
  {"/sbin/dev-zerod", 0, 0, {}, 0},
  {"/sbin/envd", 0, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_LISTEN_EXITS}, "envd"},
  {"/sbin/fbd", 0, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_GET_FB_INFO}, "fbd"},
  {"/sbin/kbdd", 0, 0, {[CAP_NAMESPACE_SERVERS] = 1 << CAP_KBDD}, "kbdd"},
  {"/sbin/ps2d", 0, 0, {[CAP_NAMESPACE_SERVERS] = 1 << CAP_KBDD_SEND_KEYPRESS}, 0},
  {"/sbin/ttyd", 0, 0, {[CAP_NAMESPACE_DRIVERS] = 1 << CAP_FBD_DRAW, [CAP_NAMESPACE_SERVERS] = 1 << CAP_KBDD_RECEIVE_EVENTS | 1 << CAP_LOGD_TTY}, "ttyd"},
  {"vfsd", 1, 0, {[CAP_NAMESPACE_KERNEL] = 1 << CAP_KERNEL_LISTEN_EXITS, [CAP_NAMESPACE_FILESYSTEMS] = 1 << CAP_VFSD}, "vfsd"},
};

int _noclonefds;

static void spawn(const char* name) {
  bool service_found = 0;
  for (size_t i = 0; i < sizeof(services) / sizeof(struct service); i++) {
    if (!strcmp(services[i].name, name)) {
      if (services[i].raw) {
        services[i].pid = spawn_process_raw(name);
      } else {
        services[i].pid = spawn_process(name);
      }
      for (size_t j = 0; j < 64; j++) {
        _syscall(_SYSCALL_GRANT_CAPABILITIES, j, services[i].capabilities[j], 0, 0, 0);
      }
      if (services[i].ipc_name) {
        register_ipc_name(services[i].ipc_name);
      }
      service_found = 1;
      break;
    }
  }
  if (!service_found) {
    return;
  }
  grant_capability(CAP_NAMESPACE_KERNEL, CAP_KERNEL_PRIORITY);
  if (!strcmp(name, "acpid")) {
    register_irq(253);
  } else if (!strcmp(name, "atad")) {
    for (size_t i = 0; i < 8; i++) {
      grant_ioport(0x1f0 + i);
      grant_ioport(0x170 + i);
    }
    grant_ioport(0x3f6);
    grant_ioport(0x376);
    register_irq(14);
    register_irq(15);
  } else if (!strcmp(name, "/sbin/devd")) {
    send_ipc_call("vfsd", IPC_VFSD_MOUNT, 0, 0, 0, (uintptr_t) "/dev/", 6);
  } else if (!strcmp(name, "/sbin/dev-nulld")) {
    send_ipc_call("devd", IPC_DEVD_REGISTER, 0, 0, 0, (uintptr_t) "null", 5);
  } else if (!strcmp(name, "/sbin/dev-zerod")) {
    send_ipc_call("devd", IPC_DEVD_REGISTER, 0, 0, 0, (uintptr_t) "zero", 5);
  } else if (!strcmp(name, "/sbin/fbd")) {
    uintptr_t fb_phys_addr = _syscall(_SYSCALL_GET_FB_INFO, 0, 0, 0, 0, 0);
    size_t height = _syscall(_SYSCALL_GET_FB_INFO, 2, 0, 0, 0, 0);
    size_t pitch = _syscall(_SYSCALL_GET_FB_INFO, 3, 0, 0, 0, 0);
    map_physical_memory(fb_phys_addr, height * pitch, 1);
  } else if (!strcmp(name, "/sbin/ps2d")) {
    grant_ioport(0x60);
    grant_ioport(0x64);
    register_irq(1);
  } else if (!strcmp(name, "/sbin/ttyd")) {
    send_ipc_call("devd", IPC_DEVD_REGISTER, 0, 0, 0, (uintptr_t) "tty", 4);
  }
  start_process();
}
int main(void) {
  if (getpid() != 1) {
    return 1;
  }
  spawn("ipcd");
  spawn("logd");
  spawn("argd");
  spawn("vfsd");
  spawn("acpid");
  spawn("pcid");
  spawn("atad");
  spawn("/sbin/devd");
  spawn("/sbin/dev-nulld");
  spawn("/sbin/dev-zerod");
  spawn("/sbin/envd");
  spawn("/sbin/fbd");
  spawn("/sbin/kbdd");
  spawn("/sbin/ttyd");
  spawn("/sbin/ps2d");
  time_t last_restart_time = 0;
  while (1) {
    pid_t pid = wait(0);
    if (time(0) - last_restart_time < 30) {
      sleep(10);
    }
    for (size_t i = 0; i < sizeof(services) / sizeof(struct service); i++) {
      if (services[i].pid == pid) {
        if (!strcmp(services[i].name, "ipcd")) {
          return 1;
        }
        spawn(services[i].name);
        break;
      }
    }
    last_restart_time = time(0);
  }
}
