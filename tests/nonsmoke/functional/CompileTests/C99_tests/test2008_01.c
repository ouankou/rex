// #include <stdio.h>

// Test of designators in structs
struct T {
  int w;
  char x;
};

struct S {
  int a[5];
  double b;
  struct T c;
};

int main(int argc, char** argv) {
  struct S x = {
    .b = 3.14,
    .c = {.x = 7, .w = 8},
    a : {// An obsolete GCC syntax
         [1] = 1,
         [3] = 2,
         3},
  };
  return x.b == 3.14 && x.c.w == 8 && x.c.x == 7 && x.a[1] == 1 && x.a[3] == 2 && x.a[4] == 3 ? 0 : 1;
}
