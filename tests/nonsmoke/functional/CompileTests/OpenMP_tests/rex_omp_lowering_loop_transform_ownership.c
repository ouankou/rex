#include <stdio.h>

static int rex_omp_threadprivate_value;
#pragma omp threadprivate(rex_omp_threadprivate_value)

enum { rex_tile_rows = 1 + 1, rex_tile_columns = 1 << 1 };

long long rex_omp_lowering_loop_transform_ownership(int values[16][16]) {
  long long result = 0;

#pragma omp tile sizes(rex_tile_rows, rex_tile_columns)
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 9; ++j)
      values[i][j] += i + j;

#pragma omp unroll full
  for (int i = 3; i < 11; i += 2)
    result += i;

#pragma omp unroll partial(1 << 1)
  for (int i = 0; i < 8; ++i)
    result += values[i][0];

#pragma omp unroll partial(+3)
  for (int i = 0; i < 8; ++i)
    result += values[i][1];

#pragma omp unroll partial((int)(2u + 2u))
  for (int i = 10; i >= 0; i -= 2)
    result += i;

#pragma omp unroll partial(2 + 3)
  for (int i = 20; i >= 0; i -= 2)
    result += i;

#pragma omp tile sizes((int)(1u << 1))
  for (int i = 0; i < 16; i += 2)
    values[i][10] += i;

#pragma omp tile sizes(2)
  for (int i = 14; i >= 0; i -= 2)
    values[i][11] += i;

#pragma omp tile sizes(2u)
  for (unsigned i = 0; i < 16u; i += 2u)
    values[i][12] += (int)i;

#pragma omp unroll partial((int)(1LL << 1))
  for (long long i = 0; i < 16; i += 2)
    result += values[i][10] + values[i][11] + values[i][12];

  {
    enum { rex_shadow_factor = 2 };
#pragma omp unroll partial(rex_shadow_factor)
    for (int i = 0; i < 4; ++i) {
      long long result_shadow = values[i][i];
      result += result_shadow;
    }
  }

  rex_omp_threadprivate_value = (int)result;
  return rex_omp_threadprivate_value;
}

int main(void) {
  int values[16][16];
  for (int i = 0; i < 16; ++i)
    for (int j = 0; j < 16; ++j)
      values[i][j] = i * 31 + j * 7;

  const long long result = rex_omp_lowering_loop_transform_ownership(values);
  long long checksum = result;
  for (int i = 0; i < 16; ++i)
    for (int j = 0; j < 16; ++j)
      checksum += values[i][j];
  printf("%lld %lld\n", result, checksum);
  return 0;
}
