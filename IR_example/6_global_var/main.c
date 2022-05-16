#include<stdio.h>
#include<stdlib.h>

int global;

int foo (int a , int b) {
  return a + b + global;
}

int main(int argc, char * argv[]) {

  global = 3;
  return global + foo(4, global);

}
