void rex_omp_simd_length_simdlen(float *input, float *output) {
#pragma omp simd simdlen(8)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
