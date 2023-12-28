
#include <assert.h>

#include <chrono>
#include <iostream>
#include <map>

#include "utils/data_utils.hpp"
#include "utils/ptr_map.hpp"

#define NUM_INSERTION_TRY 100000

int main() {
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
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

    assert(node->key_ <= (void *)(0x1234 + (unsigned long)i));

    assert(((char *)node->key_ + node->alloc_size_) >=
           ((char *)(0x1234 + (unsigned long)i)));
  }

  // return 0;

  delete map1;

  std::chrono::steady_clock::time_point ptr_map_end =
      std::chrono::steady_clock::now();

  std::cout << "new map (ptr_map) time taken : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   ptr_map_end - begin)
                   .count()
            << " ms" << std::endl;

  // old map

  begin = std::chrono::steady_clock::now();
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

  std::chrono::steady_clock::time_point map_end =
      std::chrono::steady_clock::now();
  std::cout << "old map time taken: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(map_end -
                                                                     begin)
                   .count()
            << " ms" << std::endl;

  // std::map

  begin = std::chrono::steady_clock::now();
  std::map<void *, int> *map3 = new std::map<void *, int>();

  map3->insert(std::make_pair((void *)0x1234, 4));

  map3->insert(std::make_pair((void *)0x1235, 4));

  auto search1 = map3->find((void *)0x1234);
  assert(search1->second == 4);

  search1 = map3->find((void *)0x122);
  assert(search1 == map3->end());

  map3->erase((void *)0x1234);
  map3->erase((void *)0x1235);

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    info = {0, 4 + i};
    map3->insert(std::make_pair((void *)(0x1234 + (unsigned long)i), 4 + i));
  }

  for (int i = 0; i < NUM_INSERTION_TRY; i++) {
    search1 = map3->find((void *)(0x1234 + (unsigned long)i));
    assert(search1 != map3->end());
    assert(search1->second == 4 + i);
  }

  delete map3;

  std::chrono::steady_clock::time_point std_map_end =
      std::chrono::steady_clock::now();
  std::cout << "std map time taken: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std_map_end - begin)
                   .count()
            << " ms" << std::endl;

  return 0;
}
