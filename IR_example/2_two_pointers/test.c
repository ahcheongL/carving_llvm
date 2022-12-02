#include<stdio.h>
#include<stdlib.h>


int foo (int * a , int* b) {
  fprintf(stderr, "foo: {*a: %d, *b: %d}\n", *a, *b);
  return *a + *b;
}

int goo (int * a, int * b) {
  fprintf(stderr, "goo: {*a: %d, *b: %d}\n", *a, *b);
  return *a + *b;
}


int main(int argc, char * argv[]) {

 int a[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

 int d = foo(a, a + 4);

 int e[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
 int f = goo(e + 4, e);

 return d + f;

}