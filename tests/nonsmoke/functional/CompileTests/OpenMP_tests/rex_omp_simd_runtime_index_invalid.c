void rex_omp_simd_runtime_index_invalid(float *input, float *output, int j) {
#pragma omp simd
  for (int i = 0; i < 16; ++i)
    output[i] = input[j];
}
