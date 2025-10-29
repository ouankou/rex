#include <stdio.h>

int main()
{
#pragma omp parallel
{
    printf("Normal spacing\n");
  }
  return 0;
}
