void rex_openmp_simd_semantic_positive(int count, float *input, float *output) {
  int index;
#pragma omp simd if (simd : count > 8) simdlen(8) safelen(8)
  for (index = 1; index < count; ++index) {
    output[index] = (input[index] + input[index - 1]) / 2.0f;
  }
}
