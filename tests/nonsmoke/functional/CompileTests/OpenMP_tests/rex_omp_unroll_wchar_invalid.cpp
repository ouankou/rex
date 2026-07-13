void rex_omp_unroll_wchar_invalid(int *values) {
  constexpr wchar_t factor = 2;
#pragma omp unroll partial(factor)
  for (int i = 0; i < 8; ++i)
    values[0] += 1;
}
