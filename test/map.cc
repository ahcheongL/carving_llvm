#include<utils.hpp>


#include<assert.h>
#include<vector>
#include<random>
#include<set>

map<int, int> tmp;

std::set<int> inserted_keys;
std::set<int> removed_keys;
std::set<int> keys;

int main(int argc, char * argv[]) {
  srand(time(NULL));
  unsigned int idx;
  for (idx = 0; idx < 100; idx++) {
    int random_val = rand() % 2048;
    tmp.insert(random_val, random_val + 1);
    assert(*tmp.find(random_val) == random_val + 1);
    keys.insert(random_val);
    inserted_keys.insert(random_val);
    int map_size = tmp.size();
    
    assert(tmp.nodes[0].parent == NULL);
    unsigned int idx2;
    for (idx2 = 0; idx2 < map_size; idx2++) {      
      if (tmp.nodes[idx2].parent != NULL) {
        map<int,int>::data_node * parent = tmp.nodes[idx2].parent;
        assert(parent >= tmp.nodes && parent < tmp.nodes + map_size);
        assert((parent->left == &tmp.nodes[idx2]) || (parent->right == &tmp.nodes[idx2]));
        assert(tmp.find(tmp.nodes[idx2].key) == &(tmp.nodes[idx2].elem));
      }

      if (tmp.nodes[idx2].left != NULL) {
        assert(tmp.nodes[idx2].key > tmp.nodes[idx2].left->key);
        assert (tmp.nodes[idx2].left >= tmp.nodes);
        assert (tmp.nodes[idx2].left < tmp.nodes + map_size);
      }

      if (tmp.nodes[idx2].right != NULL) {
        assert(tmp.nodes[idx2].key < tmp.nodes[idx2].right->key);
        assert (tmp.nodes[idx2].right >= tmp.nodes);
        assert (tmp.nodes[idx2].right < tmp.nodes + map_size);
      }
    }
  }

  for (auto key : keys) {
    assert(*tmp.find(key) == key + 1);
    std::cerr << "key: " << key << " value: " << *tmp.find(key) << std::endl;
  }

  for (idx = 0; idx < 1000000; idx++) {
    int random_val = rand() % 2048;
    tmp.remove(random_val);
    assert(tmp.find(random_val) == NULL);
    keys.erase(random_val);
    removed_keys.insert(random_val);

    assert(keys.size() == tmp.size());

    unsigned int idx2;
    for (idx2 = 0; idx2 < tmp.size(); idx2++) {      
      if (tmp.nodes[idx2].parent != NULL) {
        assert(tmp.nodes[idx2].parent - tmp.nodes < tmp.size());
        map<int,int>::data_node * parent = tmp.nodes[idx2].parent;
        assert((parent->left == &tmp.nodes[idx2]) || (parent->right == &tmp.nodes[idx2]));
        assert(tmp.find(tmp.nodes[idx2].key) == &(tmp.nodes[idx2].elem));
      }

      if (tmp.nodes[idx2].left != NULL) {
        assert(tmp.nodes[idx2].key > tmp.nodes[idx2].left->key);
      }

      if (tmp.nodes[idx2].right != NULL) {
        assert(tmp.nodes[idx2].key < tmp.nodes[idx2].right->key);
      }
    }

    
  }

  assert(keys.size() == tmp.size());
  for (auto key : keys) {
    assert(*tmp.find(key) == key + 1);
  }

  for (auto key : removed_keys) {
    assert(tmp.find(key) == NULL);
  }

  for (auto key : keys) {
    assert(tmp.find(key) != NULL);
    assert(*tmp.find(key) == key + 1);
    tmp.remove(key);

    unsigned int idx2;
    for (idx2 = 0; idx2 < tmp.size(); idx2++) {      
      if (tmp.nodes[idx2].parent != NULL) {
        assert(tmp.nodes[idx2].parent - tmp.nodes < tmp.size());
        map<int,int>::data_node * parent = tmp.nodes[idx2].parent;
        assert((parent->left == &tmp.nodes[idx2]) || (parent->right == &tmp.nodes[idx2]));
        assert(tmp.find(tmp.nodes[idx2].key) == &(tmp.nodes[idx2].elem));
      }

      if (tmp.nodes[idx2].left != NULL) {
        assert(tmp.nodes[idx2].key > tmp.nodes[idx2].left->key);
      }

      if (tmp.nodes[idx2].right != NULL) {
        assert(tmp.nodes[idx2].key < tmp.nodes[idx2].right->key);
      }
    }
  }

  assert(tmp.size() == 0);
  assert(tmp.root == NULL);
}