void rex_omp_simd_mixed_type_invalid(float *input, int *offset, float *output) {
#pragma omp simd
  for (int i = 0; i < 16; ++i)
    output[i] = input[i] + offset[i];
}
