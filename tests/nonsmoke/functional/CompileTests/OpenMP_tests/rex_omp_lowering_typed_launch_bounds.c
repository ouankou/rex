int rex_omp_lowering_typed_launch_bounds(void) {
  int value = 0;

#pragma omp target parallel num_threads(32 + 16) map(tofrom : value)
  {
    value = 1;
  }

  return value;
}
