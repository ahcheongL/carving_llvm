#include<memory>


class Shape {
public:
  Shape(int a , char b) : a(a), b(b) {}
  int a;
  char b;
  Shape * c;
};

int foo (char * ptr) {
    return 0;
}

int main (int argc, char * argv[]) {
  char * ptr = (char*) malloc(10000);

  *((Shape *) ptr) = Shape(10, 1);

  foo (ptr);
  free(ptr);
  return 0;
}