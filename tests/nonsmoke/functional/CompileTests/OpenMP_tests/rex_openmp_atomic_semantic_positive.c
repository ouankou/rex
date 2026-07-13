int rex_openmp_atomic_semantic_positive(int *value) {
#pragma omp atomic update release hint(1)
  *value += 1;
  return *value;
}
