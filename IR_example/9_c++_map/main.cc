#include<iostream>
#include<map>
#include<vector>
#include<cstdlib>

int foo(std::map<int, int> intmap) {
  return intmap.size();
}

int main(int argc, char * argv[]) {

  std::map<int,int> int_map;
  int_map.insert(std::make_pair(4, 5));
  std::vector<int> int_vec {0, 1, 2, 3, 4};

  return foo(int_map);
}
