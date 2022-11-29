#include<iostream>
#include<map>
#include<vector>
#include<cstdlib>

int foo(std::map<int, int> intmap) {
  fprintf(stderr, "%d %d\n", intmap[4], intmap[7]);
  return 0;
}

int main(int argc, char * argv[]) {

  std::map<int,int> int_map;
  int_map.insert(std::make_pair(4, 5));
  int_map.insert(std::make_pair(7, 8));

  int a = int_map[4];

  foo(int_map);

  return 0;;
}
