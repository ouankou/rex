void rex_omp_simd_length_absent(float *input, float *output) {
#pragma omp simd
  for (int index = 0; index < 16; ++index)
    output[index] = input[index];
}
