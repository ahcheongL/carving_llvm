
#include <assert.h>

#include <chrono>
#include <iostream>

#include "utils/data_utils.hpp"
#include "utils/ptr_map.hpp"

using namespace std;

#define NUM_INSERTION_TRY 100000

int main() {
  chrono::steady_clock::time_point begin = chrono::steady_clock::now();
  ptr_map *map1 = new ptr_map();
  map1->insert((void *)0x1234, 0, 4);
  map1->insert((void *)0x1235, 0, 4);

  ptr_map::rbtree_node *node = map1->find((void *)0x1234);
  assert(node->key_ == (void *)0x1234);

  node = map1->find((void *)0x122);
  assert(node == nullptr);

  map1->remove((void *)0x1234);
  map1->remove((void *)0x1235);

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    map1->insert((void *)(0x1234 + (unsigned long)i), 0, 4 + i);
  }

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    node = map1->find((void *)(0x1234 + (unsigned long)i));
    assert(node != nullptr);
    assert(node->alloc_size_ == 4 + i);
  }

  delete map1;

  chrono::steady_clock::time_point ptr_map_end = chrono::steady_clock::now();

  cout << "new map (ptr_map) time taken : "
       << chrono::duration_cast<chrono::milliseconds>(ptr_map_end - begin)
              .count()
       << " ms" << endl;

  begin = chrono::steady_clock::now();
  map<void *, typeinfo> *map2 = new map<void *, typeinfo>();

  typeinfo info{0, 4};

  map2->insert((void *)0x1234, info);

  info = {0, 4};

  map2->insert((void *)0x1235, info);

  typeinfo *node2 = map2->find((void *)0x1234);
  assert(node2->size == 4);

  node2 = map2->find((void *)0x122);
  assert(node2 == nullptr);

  map2->remove((void *)0x1234);
  map2->remove((void *)0x1235);

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    info = {0, 4 + i};
    map2->insert((void *)(0x1234 + (unsigned long)i), info);
  }

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    node2 = map2->find((void *)(0x1234 + (unsigned long)i));
    assert(node2 != nullptr);
    assert(node2->size == 4 + i);
  }

  delete map2;

  chrono::steady_clock::time_point map_end = chrono::steady_clock::now();
  cout << "old map time taken: "
       << chrono::duration_cast<chrono::milliseconds>(map_end - begin).count()
       << " ms" << endl;

  return 0;
}