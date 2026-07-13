void rex_omp_simd_strided_invalid(float *input, float *output) {
#pragma omp simd
  for (int i = 0; i < 16; ++i)
    output[i] = input[2 * i];
}
