#include <stdio.h>

static int add(int a, int b);

int main(void) {
  int i = 1;
  int sum = 0;
  while (i < 11) {
    sum = add(sum, i);
    i = add(i, 1);
  }
  printf("sum = %d\n", sum);

#pragma SliceTarget
  i;
  printf("i = %d\n", i);
  return 0;
}

static int add(int a, int b) { return (a + b); }
