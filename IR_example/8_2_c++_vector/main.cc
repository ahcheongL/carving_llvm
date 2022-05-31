#include<iostream>
#include<vector>
#include<cstdlib>

class Shape {
public:
  Shape(int a, char b) { width = a; area = b; }
  int width;
  char area;
};

int foo(std::vector<class Shape> shapevec) {
  return 0;
}

int main(int argc, char * argv[]) {

  std::vector<class Shape> vec1;
  vec1.push_back(Shape(2,3));
  vec1.push_back(Shape(1,2));
  foo(vec1); //vec1 : {shape1, shape2}

  vec1.erase(vec1.begin()); 
  foo(vec1); //vec1 : {shape2}

  return 0;
}
