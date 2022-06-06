#include<iostream>
#include<map>
#include<vector>
#include<cstdlib>

int foo(std::map<int, int> intmap) {
  return 0;
}

int main(int argc, char * argv[]) {

  std::map<int,int> int_map;
  int_map.insert(std::make_pair(4, 5));

  foo(int_map);

  return 0;;
}
