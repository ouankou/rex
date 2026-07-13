#include <math.h>

enum { test = 1 };

static void simd4_body(int n, int m, float *a, float *b) {
  int i;
#pragma omp simd if (simd : test) simdlen(8) safelen(8)
  {
    for (i = 1; i < n; i++)
      b[i] = ((a[i] + a[i - 1]) / 2.0);
  }
}

int main(void) { return 0; }
