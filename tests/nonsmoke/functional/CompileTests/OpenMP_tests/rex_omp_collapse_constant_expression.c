#include <stdio.h>

int main(void) {
  int values[2][3] = {{0}};

#pragma omp parallel for collapse(1 + 1)
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 3; ++j)
      values[i][j] = i * 3 + j + 1;

  int sum = 0;
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 3; ++j)
      sum += values[i][j];

  printf("%d\n", sum);
  return sum != 21;
}
