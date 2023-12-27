#include<stdio.h>
#include<stdlib.h>

char *foo() {
  fprintf(stderr, "foo called\n");
  char* a = (char *) malloc (5);
  fprintf(stderr, "foo called malloc\n");
  a[0] = 'a';
  a[1] = 'b';
  a[2] = 'c';
  a[3] = 'd';
  a[4] = 0;
  return a;
}

void goo(char * ptr) {
  fprintf(stderr, "goo called\n");

  char b = ptr[0];
  char c = ptr[1];

  if (b+c) {  return;
  }


  free(ptr);
  fprintf(stderr, "goo called free\n");
  return;
}
