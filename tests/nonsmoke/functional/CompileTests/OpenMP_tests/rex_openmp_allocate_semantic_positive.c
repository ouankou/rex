#include <omp.h>

void rex_openmp_allocate_semantic_positive(void) {
  int value;
#pragma omp allocate(value) allocator(omp_pteam_mem_alloc)
  value = 1;
}
