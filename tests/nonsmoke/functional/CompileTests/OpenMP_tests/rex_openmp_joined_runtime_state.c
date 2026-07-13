void rex_openmp_joined_runtime_state(int *values, int count) {
#pragma omp parallel for
  for (int index = 0; index < count; ++index) {
    values[index] += index;
  }
}
