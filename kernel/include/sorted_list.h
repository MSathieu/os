#pragma once
#include <linked_list.h>

typedef int (*sorted_list_compare)(void*, void*);

struct sorted_list {
  struct linked_list_member* first;
  struct linked_list_member* last;
  sorted_list_compare compare;
};

void insert_sorted_list(struct sorted_list*, struct linked_list_member*, void*);
void remove_sorted_list(struct sorted_list*, struct linked_list_member*);
