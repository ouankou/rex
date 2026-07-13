void rex_omp_simd_mixed_lane_width_invalid(const float *float_input,
                                           const double *double_input,
                                           float *float_output,
                                           double *double_output) {
#pragma omp simd
  for (int i = 0; i < 16; ++i) {
    float_output[i] = float_input[i] + 1.0f;
    double_output[i] = double_input[i] + 1.0;
  }
}
