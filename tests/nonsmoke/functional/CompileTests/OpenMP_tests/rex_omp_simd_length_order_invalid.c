void rex_omp_simd_length_order_invalid(float *input, float *output) {
#pragma omp simd simdlen(16) safelen(8)
  for (int i = 0; i < 16; ++i)
    output[i] = input[i];
}
