#include <stdio.h>

int main(void) {
  float lhs[32];
  float rhs[32];
  float difference[32];
  float quotient[32];
  for (int i = 0; i < 32; ++i) {
    lhs[i] = (float)(i * 5 + 11);
    rhs[i] = (float)(i + 2);
  }

#pragma omp simd
  for (int i = 0; i < 32; ++i) {
    difference[i] = (float)lhs[i] - rhs[i];
    quotient[i] = lhs[i] / rhs[i];
  }

  double checksum = 0.0;
  for (int i = 0; i < 32; ++i)
    checksum += difference[i] * 3.0 + quotient[i] * 7.0;
  printf("%.9f\n", checksum);
  return 0;
}
