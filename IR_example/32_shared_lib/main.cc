#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<dlfcn.h>

#include<unistd.h>

char * foo();

void goo(char *);

void f1(char * ptr) {

  char a = ptr[0];

  int b = ((int *) ptr)[0];

  int c = a + b;

  return;
}

int main() {

  char * a = (char *) malloc(10);
  fprintf(stderr, "main called malloc\n");

  free(a);
  fprintf(stderr, "main called free\n");

  char * ret = foo();

  f1(ret);

  goo(ret);

  return 0;
}
