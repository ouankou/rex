int rex_openmp_critical_semantic_positive(void) {
  int value = 0;
#pragma omp parallel shared(value)
  {
#pragma omp critical(rex_lock) hint(1)
    value += 1;
  }
  return value;
}
