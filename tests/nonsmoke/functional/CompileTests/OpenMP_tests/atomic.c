#include <omp.h>

enum { test = 1 };

int main(void) {

  int a;
#pragma omp parallel
  {
#pragma omp atomic release hint(test) read
    a += 1;
  }
  return 0;
}
