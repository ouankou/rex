void direct_loop(int *values) {
  constexpr int unroll_factor = 2;
#pragma omp unroll partial(unroll_factor)
  for (int i = 0; i < 8; ++i)
    values[i] += i;
}

void const_direct_loop(int *values) {
  const int const_unroll_factor = 2;
#pragma omp unroll partial(const_unroll_factor)
  for (int i = 0; i < 8; ++i)
    values[i] += i;
}

void nested_transform_loop(int *values) {
#pragma omp unroll partial(2)
#pragma omp tile sizes(2)
  for (int i = 0; i < 8; ++i)
    values[i] += i;
}

void continue_loop(int *values) {
#pragma omp unroll partial(2)
  for (int i = 0; i < 8; ++i) {
    if ((i & 1) != 0)
      continue;
    values[i] += i;
  }
}
