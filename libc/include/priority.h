#pragma once

enum {
  PRIORITY_SYSTEM_HIGH,
  PRIORITY_SYSTEM_NORMAL,
  PRIORITY_SYSTEM_LOW,
  PRIORITY_ROOT_HIGH,
  PRIORITY_ROOT_NORMAL,
  PRIORITY_ROOT_LOW,
  PRIORITY_USER_HIGH,
  PRIORITY_USER_NORMAL,
  PRIORITY_USER_LOW,
  PRIORITY_IDLE
};

void change_priority(int);
