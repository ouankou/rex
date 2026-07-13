void rex_omp_simd_length_both(float *input, float *output) {
#pragma omp simd simdlen(4) safelen(8)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
