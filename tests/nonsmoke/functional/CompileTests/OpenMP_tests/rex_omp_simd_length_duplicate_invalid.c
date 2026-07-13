void rex_omp_simd_length_duplicate_invalid(float *input, float *output) {
#pragma omp simd simdlen(4) simdlen(8)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
