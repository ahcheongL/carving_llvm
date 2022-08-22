#include<stdio.h>
#include<stdlib.h>

typedef int (*gooptr) (int *);
typedef int (gootype) (int *);

struct stra{
  gooptr goop;
  int * a;
};

gooptr global_goop;

int foo (gooptr ptr, int * intptr, struct stra * strptr){
  return ptr(intptr) + global_goop(intptr);
}

int goo (int * ptr) {
  return ptr[4];
}

int goo2 (int * ptr) {
  return ptr[2];
}


int main(int argc, char * argv[]) {
 int a[5] = {0, 1, 2, 3, 4};

 global_goop = goo2;

 struct stra s;
 s.goop = goo;
 s.a = 0;

 return foo(goo, a, &s);
}
