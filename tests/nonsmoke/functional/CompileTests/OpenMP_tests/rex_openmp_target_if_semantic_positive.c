void rex_openmp_target_if_semantic_positive(int *values, int count) {
#pragma omp target if (target : count > 0) map(tofrom : values[0 : count])
  {
    for (int index = 0; index < count; ++index) {
      values[index] += 1;
    }
  }
}
