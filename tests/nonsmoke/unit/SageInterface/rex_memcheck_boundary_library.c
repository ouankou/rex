#include <stdio.h>
__attribute__((noinline)) static void consume(const int *value) {
  if (*value != 0)
    puts("nonzero");
}
__attribute__((visibility("default"), noinline)) void
rex_memcheck_boundary(const int *value) {
  consume(value);
}
