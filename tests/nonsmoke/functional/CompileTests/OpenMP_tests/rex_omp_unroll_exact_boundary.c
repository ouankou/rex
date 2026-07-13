#include <limits.h>
#include <stdio.h>

int main(void) {
  unsigned long long result = 0;

#pragma omp unroll full
  for (unsigned long long i = ULLONG_MAX - 4; i < ULLONG_MAX; ++i)
    result += ULLONG_MAX - i;

#pragma omp unroll partial(1)
  for (int i = 0; i < 8; ++i) {
    if ((i & 1) != 0)
      continue;
    result += (unsigned)i;
  }

#pragma omp unroll full
  for (unsigned i = 7; i < 7; ++i)
    result += i;

  printf("%llu\n", result);
  return 0;
}
