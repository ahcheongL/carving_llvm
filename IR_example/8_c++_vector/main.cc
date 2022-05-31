#include<iostream>
#include<vector>
#include<cstdlib>

int foo(std::vector<int> intvec) {
  return 0;
}

int main(int argc, char * argv[]) {

  //std::vector<int> vec1 {0, 2, 3, 4, 5};  

  std::vector<int> vec1;
  vec1.push_back(12);
  vec1.push_back(13);
  foo(vec1); //vec1 : {12, 13}

  vec1.erase(vec1.begin()); 
  foo(vec1); //vec1 : {13}

  return 0;
}
