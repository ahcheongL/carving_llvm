#include "boost/container/map.hpp"

#include<assert.h>
#include<vector>
#include<random>
#include<set>
#include<iostream>
#include<algorithm>

int main() {
  boost::container::map<int, int> tmp;

  std::set<int> inserted_keys;
  std::set<int> removed_keys;
  std::set<int> keys;
  std::vector<int> keys_vec;

  srand(time(nullptr));
  unsigned int idx;
  for (idx = 0; idx < 100; idx++) {
    int random_val = rand() % 2048;
    tmp[random_val] = random_val + 1;
    keys.insert(random_val);
    inserted_keys.insert(random_val);
    keys_vec.push_back(random_val);
  }

  auto it = tmp.find(102);

  if (it == tmp.end()) {
    std::cerr << "not found1" << std::endl;
  } else {
    std::cerr << "found1" << it->first << std::endl;
  }

  std::sort(keys_vec.begin(), keys_vec.end());

  for (auto key : keys_vec) {
    std::cerr << "key: " << key << " value: " << tmp[key] << std::endl;
  }

  auto it2 = tmp.upper_bound(102);
  if (it2 == tmp.end()) {
    std::cerr << "not found2" << std::endl;
  } else {
    std::cerr << "found2 " << it2->first << std::endl;
    it2 --;
    std::cerr << "found2 " << it2->first << std::endl;
  }
}