typedef float __m128 __attribute__((__vector_size__(16)));

void rex_omp_intel_simd_vec_identity(float *output, float __vec0) {
#pragma omp simd simdlen(4)
  for (int index = 0; index < 4; index += 1)
    output[index] = __vec0;
}

void rex_omp_intel_simd_ptr_identity(float *output, float __ptr0) {
  {
    float __ptr0 = 7.0f;
#pragma omp simd simdlen(4)
    for (int index = 0; index < 4; index += 1)
      output[index] = __ptr0;
  }
  output[0] += __ptr0;
}

void rex_omp_intel_simd_part_identity(float *output, float __part0) {
#pragma omp simd simdlen(4)
  for (int index = 0; index < 4; index += 1)
    output[index] = __part0;
}

void rex_omp_intel_simd_missing_output(float *output, float source_value) {
#pragma omp simd simdlen(4)
  for (int index = 0; index < 4; index += 1)
    output[index] = source_value;
}
