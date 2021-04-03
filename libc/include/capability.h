#pragma once

enum {
  CAP_NAMESPACE_KERNEL,
  CAP_NAMESPACE_DRIVERS,
  CAP_NAMESPACE_SERVERS,
  CAP_NAMESPACE_FILESYSTEMS
};
enum {
  CAP_KERNEL_MANAGE,
  CAP_KERNEL_IOPORT,
  CAP_KERNEL_IRQ,
  CAP_KERNEL_MAP_MEMORY,
  CAP_KERNEL_GET_FB_INFO,
  CAP_KERNEL_PRIORITY
};
enum {
  CAP_FBD_DRAW
};
enum {
  CAP_IPCD_REGISTER,
  CAP_KBDD_SEND_KEYPRESS,
  CAP_KBDD_RECEIVE_EVENTS,
  CAP_KBDD,
  CAP_LOGD_TTY,
  CAP_LOGD
};
enum {
  CAP_DEVD,
  CAP_VFSD_MOUNT,
  CAP_VFSD
};

void drop_capability(int namespace, int capability);
