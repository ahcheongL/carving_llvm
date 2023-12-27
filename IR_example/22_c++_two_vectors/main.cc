#include<vector>

class Shape {
public:
  Shape(int area, char width) : area(area), width(width) {}
  int area;
  char width;
};

int foo(std::vector<int> vec1, std::vector<Shape> vec2) {
  return 0;
}

int main (int argc, char * argv []) {
  std::vector<int> intvec {2, 45, 2};
  std::vector<Shape> shapevec;

  shapevec.push_back(Shape(1043, 24));
 
  foo( intvec, shapevec);


  int * a = (int *) malloc(10);

  return 0;
}

