
#include <assert.h>

#include <chrono>
#include <iostream>
#include <map>

#include "utils/ptr_map.hpp"

int main() {
  ptr_map *map1 = new ptr_map();

  unsigned int i;

  for (i = 0; i < 65536; i++) {
    map1->insert((void *)(0x1000ul + (i * 1024)), 0, 200);
  }

  for (i = 0; i < 65536; i++) {
    ptr_map::rbtree_node *node =
        map1->find((void *)(0x1000ul + (i * 1024) + 100));

    assert(node != nullptr);
    assert(node->key_ == (void *)(0x1000ul + (i * 1024)));
  }

  delete map1;

  return 0;
}
