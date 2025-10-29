#include <stdio.h>

int main()
{
#  pragma omp parallel
{
    printf("Space after hash\n");
  }
  return 0;
}
