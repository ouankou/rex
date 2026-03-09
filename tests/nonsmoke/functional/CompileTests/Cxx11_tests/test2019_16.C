
#include <stdarg.h>
// #define test__builtin_va_arg(v,l) (l)(sizeof(l))
// #define test__builtin_va_arg(v,l) (__typeof__(l))(sizeof(l))
// #define test__builtin_va_arg(v,l) (unsigned long)(sizeof(l))

struct X_ {
  int a, b, c;
};

X_ x_ = {1, 2, 3};

void foobar(int x, ...) {
  int value = 0;
  va_list ap;
  va_start(ap, x);

  value = x_.*__builtin_va_arg(ap, int X_::*);

  va_end(ap);
}
