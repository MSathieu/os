#pragma once

struct layout_key {
  int type;
  int value;
  int modifier;
  int toggle;
};
struct layout_combination {
  int parts[16];
};
struct layout {
  char country[2];
  struct layout_key keys[16][128];
  struct layout_combination combinations[16];
};
enum {
  KBD_MODIFIER_SHIFT,
  KB_MODIFIER_PLACEHOLDER
};
enum {
  KBD_TOGGLE_CAPS_LOCK,
  KBD_TOGGLE_NUM_LOCK
};

extern const struct layout be_layout;
