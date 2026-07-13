/*
 */
#include <assert.h>

#include <omp.h>

#include <stdio.h>

int a;
int b;
int main(void) {

#pragma omp parallel
  {

#pragma omp single
    {
      int num_threads = 2;
    }
#pragma omp single copyprivate(a, b)
    {
      int num_threads = 3;
    }
  }
  return 0;
}
