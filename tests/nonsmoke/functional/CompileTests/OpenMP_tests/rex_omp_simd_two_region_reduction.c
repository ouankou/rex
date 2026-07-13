float rex_omp_simd_two_region_reduction(const float *left_values,
                                        const float *right_values, int count) {
  float left_sum = 0.0f;
  float right_sum = 0.0f;

#pragma omp simd reduction(+ : left_sum)
  for (int i = 0; i < count; ++i)
    left_sum += left_values[i];

#pragma omp simd reduction(+ : right_sum)
  for (int i = 0; i < count; ++i)
    right_sum += right_values[i];

  return left_sum + right_sum;
}
