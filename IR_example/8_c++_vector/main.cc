#include<iostream>
#include<vector>
#include<cstdlib>

int foo(std::vector<int> intvec) {
  return intvec.size();
}

int main(int argc, char * argv[]) {

  std::vector<int> vec1 {0, 2, 3, 4, 5};  
  return foo(vec1);
}
