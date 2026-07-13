void rex_omp_simd_length_safelen(float *input, float *output) {
#pragma omp simd safelen(5)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
