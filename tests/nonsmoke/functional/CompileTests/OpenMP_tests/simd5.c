#include <math.h>

static void simd5_body(int n, int m, float *a, float *b) {
  int i;
#pragma omp simd order(concurrent)
  {
    for (i = 1; i < n; i++)
      b[i] = ((a[i] + a[i - 1]) / 2.0);
  }
}

int main(void) { return 0; }
