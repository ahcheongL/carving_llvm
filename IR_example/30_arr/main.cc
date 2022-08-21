#include<stdlib.h>
#include<stdio.h>

struct stra {
  short fieldarr[3];
  long field2arr[5];
};

char arr[20][20];

char smallarr[2][2] = {{ 100, 101}, { 20, -21 }};

long longval;

struct stra strarr[2];

struct stra * strptr = &strarr[0];

void foo(char arrptr[20][20], struct stra * strptr, long * longval) {

  char local_arr[10];

  local_arr[4] = 4;

  if (arrptr[12][10] == 21){ 
    return;
  }

  if (strptr[2].fieldarr[1] == 40) { return; }

  if (strptr[1].field2arr[0] == 50) { return; }

  long * lptr = strptr[1].field2arr;

  if (lptr[0] == 60) { return; }

 return;

}

int main(int argc, char* argv[]) {
  if (arr[14][15] == 24) {
    return 0;
  } 

  struct stra * ptr = (struct stra *) malloc(sizeof(struct stra) * 4);

  ptr[0].fieldarr[0] = 1;
  ptr[0].fieldarr[1] = 2;
  ptr[0].fieldarr[2] = 3;
  ptr[0].field2arr[0] = 4;
  ptr[0].field2arr[1] = 5;
  ptr[0].field2arr[2] = 6;
  ptr[0].field2arr[3] = 7;
  ptr[0].field2arr[4] = 8;

  foo(arr, ptr, &longval);

  free(ptr);

  return 1;
}
