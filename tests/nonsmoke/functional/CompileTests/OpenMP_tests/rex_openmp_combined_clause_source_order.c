void rex_openmp_combined_clause_source_order(int *values, int count) {
#pragma omp parallel for schedule(static) num_threads(2) shared(values)        \
    collapse(1)
  for (int index = 0; index < count; ++index) {
    values[index] += index;
  }
}
