#include <stdio.h>

int main()
{
#pragma    omp      parallel
{
    printf("Multiple spaces\n");
  }
  return 0;
}
