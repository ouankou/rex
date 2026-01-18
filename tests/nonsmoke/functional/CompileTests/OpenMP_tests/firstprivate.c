#include <assert.h>

#include <omp.h>

#include <stdio.h>

int main() {
  int i = 100, sum = 0;
// with it:
#pragma omp parallel firstprivate(i) reduction(+ : sum)
  {
    assert(i == 100);
    sum = sum + i;
  }

  i = 100;

// without it:
#pragma omp parallel private(i) reduction(+ : sum)
  {
    // assert(i != 100);
    sum = sum + i;
  }

  return 0;
}
