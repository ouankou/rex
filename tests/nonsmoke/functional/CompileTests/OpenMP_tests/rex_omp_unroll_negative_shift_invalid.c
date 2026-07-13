void rex_omp_unroll_negative_shift_invalid(int *values) {
#pragma omp unroll partial((-4) >> 1)
  for (int i = 0; i < 8; ++i)
    values[0] += 1;
}
