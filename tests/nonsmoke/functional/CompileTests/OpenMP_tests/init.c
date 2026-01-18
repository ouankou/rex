// Test the placement of XOMP_init() in C/C++ input
#include <omp.h>
int main(int argc, char *argv[])
#include <stdio.h>

#include <stdlib.h>
{
  if (argc < 2)
    exit(1);

#pragma omp parallel
#pragma omp master
  {
    printf("Number of threads = %d\n", omp_get_num_threads());
  }

  return 0;
}
