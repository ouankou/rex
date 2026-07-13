void rex_omp_simd_value_cast_invalid(float *input, float *output) {
#pragma omp simd
  for (int i = 0; i < 16; ++i)
    output[i] = (double)input[i];
}
