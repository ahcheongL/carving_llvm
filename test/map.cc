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
  srand(0);
  unsigned int idx;
  for (idx = 0; idx < 1024; idx++) {
    int random_val = rand() % 256;
    fprintf(stderr, "inserting key : %d\n", random_val);
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

  for (auto key : keys) {
    fprintf(stderr, "key : %d\n",key);
    assert(*tmp.find(key) == key + 1);
  }

  fprintf(stderr, "size of map : %d\n", tmp.size());
  fprintf(stderr, " # of keys : %lu\n", keys.size());

  for (idx = 0; idx < 256; idx++) {
    int random_val = rand() % 256;
    fprintf(stderr, "random_val : %d\n",random_val);
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

  fprintf(stderr, "size of map : %d\n", tmp.size());
  fprintf(stderr, " # of keys : %lu\n", keys.size());

  for (auto key : keys) {
    fprintf(stderr, "key : %d\n",key);
    assert(*tmp.find(key) == key + 1);
  }

  for (auto key : removed_keys) {
    assert(tmp.find(key) == NULL);
  }

  fprintf(stderr, "size of map : %d\n", tmp.size());
  fprintf(stderr, " # of keys : %lu\n", keys.size());

  for (auto key : keys) {
    fprintf(stderr, "key2 : %d\n",key);

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