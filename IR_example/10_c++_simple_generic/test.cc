#include<iostream>
#include<cstdlib>

template <class T>
T add(T a, T b) {
  fprintf(stderr, "%d %d\n", a, b);
  return a + b;
}

int main(int argc, char * argv[]) {

  int d = add(2, 5);

  return d + 4;
}
