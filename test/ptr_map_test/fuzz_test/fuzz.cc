#include <assert.h>

#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include "utils/ptr_map.hpp"

using namespace std;

int main(int argc, char **argv) {
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <fuzz_binary_filename>" << std::endl;
    return 1;
  }

  char *fn = argv[1];
  ifstream input_f(fn, ios::binary | ios::ate);
  if (!input_f.is_open()) {
    cout << "Cannot open file " << fn << std::endl;
    return 1;
  }

  int size = input_f.tellg();
  char *buffer = new char[size];
  input_f.seekg(0, ios::beg);
  input_f.read(buffer, size);
  input_f.close();

  ptr_map *map1 = new ptr_map();
  map<void *, int> std_map;
  set<void *> std_keys;

  int buf_idx = 0;

  for (int i = 0; i < 100; i++) {
    if (buf_idx >= size) {
      break;
    }

    char op_idx = buffer[buf_idx] % 3;
    buf_idx++;

    switch (op_idx) {
      case 0: {
        // Align to 8 bytes
        if (buf_idx % sizeof(unsigned long) != 0) {
          buf_idx += sizeof(unsigned long) - (buf_idx % sizeof(unsigned long));
        }

        if ((buf_idx + sizeof(unsigned long) + sizeof(unsigned int)) >= size) {
          break;
        }

        unsigned long key = *(unsigned long *)(buffer + buf_idx);
        buf_idx += sizeof(unsigned long);
        unsigned int alloc_size = *(unsigned int *)(buffer + buf_idx);
        buf_idx += sizeof(unsigned int);

        alloc_size = alloc_size % 2048;

        if (alloc_size == 0) {
          break;
        }

        if (key + alloc_size < key) {
          break;
        }

        cerr << "[Test] Inserting key : " << ((void *)key)
             << ", alloc_size : " << alloc_size << endl;

        map1->insert((void *)key, 0, alloc_size);

        if (std_map.find((void *)key) != std_map.end()) {
          std_map.erase((void *)key);
        }

        std_map.insert(make_pair((void *)key, alloc_size));
        std_keys.insert((void *)key);
        break;
      }

      case 1: {
        if (std_keys.size() == 0) {
          break;
        }

        // Align to 4 bytes
        if (buf_idx % sizeof(int) != 0) {
          buf_idx += sizeof(int) - (buf_idx % sizeof(int));
        }

        if ((buf_idx + sizeof(int)) >= size) {
          break;
        }

        int rand_idx = *(int *)(buffer + buf_idx);
        buf_idx += sizeof(int);
        rand_idx = rand_idx % std_keys.size();

        auto it = std_keys.begin();
        for (int i = 0; i < rand_idx; i++) {
          it++;
        }
        void *key = *it;

        cerr << "[Test] Checking key : " << key << endl;

        ptr_map::rbtree_node *node = map1->find(key);
        assert(node != nullptr);

        assert(node->key_ <= key);
        assert(((char *)node->key_ + node->alloc_size_) >= key);

        std::map<void *, int>::iterator std_it = std_map.find(key);
        assert(std_it != std_map.end());

        // False ...
        // assert(node->alloc_size_ == std_it->second);
        break;
      }

      case 2: {
        if (std_keys.size() == 0) {
          break;
        }

        // Align to 4 bytes
        if (buf_idx % sizeof(int) != 0) {
          buf_idx += sizeof(int) - (buf_idx % sizeof(int));
        }

        if ((buf_idx + sizeof(int)) >= size) {
          break;
        }

        int rand_idx = *(int *)(buffer + buf_idx);
        buf_idx += sizeof(int);
        rand_idx = rand_idx % std_keys.size();

        auto it = std_keys.begin();
        for (int i = 0; i < rand_idx; i++) {
          it++;
        }
        void *key = *it;

        cerr << "[Test] Removing key : " << key << endl;

        map1->remove(key);
        std_map.erase(key);
        std_keys.erase(key);
        break;
      }
    }
  }

  delete map1;
  delete[] buffer;

  return 0;
}
