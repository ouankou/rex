#include <omp.h>

#include <stdio.h>
int main(void) {

  int i, myval;
#pragma omp parallel for private(myval) ordered
  for (i = 1; i <= 100; i++) {
    myval = i * i;
#pragma omp ordered
    {
      printf("%d %d\n", i, myval);
    }
  }

#pragma omp parallel for ordered schedule(static, 3)
  for (int j = 0; j < 16; ++j) {
#pragma omp ordered
    printf("static %d\n", j);
  }

#pragma omp parallel for ordered schedule(monotonic : dynamic, 2)
  for (long long j = 20; j > 0; --j) {
    if (j % 3 == 0)
      continue;
    for (int k = 0; k < 2; ++k) {
      if (k == 0)
        continue;
    }
#pragma omp ordered
    printf("dynamic %lld\n", j);
  }
  return 0;
}
