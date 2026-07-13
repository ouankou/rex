void rex_omp_simd_safelen_too_small_invalid(float *input, float *output) {
#pragma omp simd safelen(3)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
