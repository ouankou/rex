void rex_openmp_vla_capture(int count) {
  int values[count];
#pragma omp task shared(values)
  {
    values[count - 1] = 7;
  }
}
