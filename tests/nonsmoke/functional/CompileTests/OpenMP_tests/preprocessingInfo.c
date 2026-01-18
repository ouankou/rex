#include <omp.h>
int main(void)
#include <stdio.h>
{
  int mits = 5000;
#ifdef _OPENMP
#pragma omp parallel
  {
#pragma omp single
    printf("Running using %d threads...\n", omp_get_num_threads());
  }
#endif
  mits++;
  return 0;
}
