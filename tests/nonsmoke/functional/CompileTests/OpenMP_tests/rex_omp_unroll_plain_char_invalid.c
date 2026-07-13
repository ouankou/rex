void rex_omp_unroll_plain_char_invalid(int *values) {
  const char factor = 2;
#pragma omp unroll partial(factor)
  for (int i = 0; i < 8; ++i)
    values[0] += 1;
}
