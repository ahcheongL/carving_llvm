#include<stdio.h>
#include<stdlib.h>

typedef int (*gooptr) (int *);

int foo (gooptr ptr, int * intptr) {
  return ptr(intptr);
}

int goo (int * ptr) {
  return ptr[4];
}


int main(int argc, char * argv[]) {
 int a[5] = {0, 1, 2, 3, 4};

 return foo(goo, a);
}
