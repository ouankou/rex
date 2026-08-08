#include <omp.h>
#include <stdio.h>

omp_allocator_handle_t user_modi = omp_default_mem_alloc;

int main() {
  int a, b, c;

#pragma omp parallel private(a, b, c) allocate(user_modi : a, b)               \
    allocate(user_modi : c)
  {
    printf("This tests parser and AST construction with valid allocation "
           "semantics.\n");
  }

  return 0;
}
