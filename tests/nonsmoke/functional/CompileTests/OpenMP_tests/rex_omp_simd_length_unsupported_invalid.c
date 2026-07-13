void rex_omp_simd_length_unsupported_invalid(float *input, float *output) {
#pragma omp simd simdlen(12)
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
