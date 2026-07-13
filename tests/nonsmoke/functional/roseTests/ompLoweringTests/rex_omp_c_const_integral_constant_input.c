void rex_omp_c_const_integral_constant(int *values) {
  const int unroll_factor = 2;
#pragma omp unroll partial(unroll_factor)
  for (int induction = 0; induction < 8; ++induction)
    values[induction] += induction;
}
