// Strange example of typedef and declaration from legacy frontend manual
// (section 11.3, page 270)

#include <stdio.h>

typedef int I;

int main() {
  I(i) = 42; // equivalent to "int i = 42;"
  int(j) = 43;

  printf("i = %d j = %d \n", i, j);
  return 0;
}
