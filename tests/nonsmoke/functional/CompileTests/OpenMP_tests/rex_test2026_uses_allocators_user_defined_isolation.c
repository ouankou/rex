int rex_test2026_allocator;

int rex_test2026_uses_allocators_user_defined_isolation(int value) {
  int result = 0;

#pragma omp target map(from : result)                                          \
    uses_allocators(rex_test2026_allocator(value), omp_default_mem_alloc)
  {
    result = value + 1;
  }

  return result;
}
