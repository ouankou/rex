
// #undef offsetof
// #undef __offsetof__
// #undef __builtin_offsetof

#include <stddef.h>

struct foo {
  char a[13];
  long b;
  char c[7];
  short d;
  char e[3];
};

int main() {
  int A, B, C, D, E;

#if ((__GNUC__ == 3) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 1))
  A = offsetof(struct foo, a[0]);
  // B = offsetof(struct foo, b);
  // C = offsetof(struct foo, c[0]);
  // D = offsetof(struct foo, d);
  // E = offsetof(struct foo, e[0]);
#else
#warning "offsetof macro or builtin function not defined in g++ 4.1.2 compiler."
#endif

  return 0;
}
