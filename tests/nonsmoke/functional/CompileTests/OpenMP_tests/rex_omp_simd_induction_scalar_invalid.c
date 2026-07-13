void rex_omp_simd_induction_scalar_invalid(int *input, int *output) {
#pragma omp simd
  for (int i = 0; i < 16; ++i)
    output[i] = input[i] + i;
}
