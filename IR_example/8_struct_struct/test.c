#include<stdio.h>
#include<stdlib.h>

typedef struct _b {
  int a;
  char b;
  int c[2][3];
};

typedef struct _a {
  int f1;
  char f2;
  // double f3;
  struct _b f4;
};

int foo (struct _a *s1) {
  return s1[0].f4.c[5][13];
}

int main(int argc, char * argv[]) {

  struct _a * ptr = malloc(sizeof(struct _a) * 1);
  ptr[0].f1 = 4; ptr[0].f2 = 5;
  // ptr[1].f1 = 6; ptr[1].f2 = 7;

  return foo(ptr);
}
