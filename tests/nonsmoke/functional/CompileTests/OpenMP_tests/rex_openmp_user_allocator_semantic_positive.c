#include <omp.h>

int rex_openmp_user_allocator_semantic_positive(void) {
  int first = 0;
  int second = 0;
  omp_allocator_handle_t user_allocator = omp_default_mem_alloc;
#pragma omp parallel private(first, second)                                    \
    allocate(user_allocator : first, second)
  {
    first = 1;
    second = 2;
  }
  return first + second;
}
