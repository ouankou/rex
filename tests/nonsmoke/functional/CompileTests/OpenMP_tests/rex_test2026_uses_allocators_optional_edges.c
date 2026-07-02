int rex_test2026_uses_allocators_optional_edges(int value) {
  int result = 0;

#pragma omp target map(from : result)                                          \
    uses_allocators(omp_default_mem_alloc, omp_const_mem_alloc(value))
  {
    result = value + 1;
  }

  return result;
}
