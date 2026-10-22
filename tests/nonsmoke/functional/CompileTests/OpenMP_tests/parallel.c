#include <assert.h>

int main(void) {
  int i = 100;
  int j = 77;
  int *const original_i = &i;
#pragma omp parallel firstprivate(i)
  {
    assert(i == 100);
    assert(j == 77);
  }
#pragma omp parallel private(i)
  {
    // OpenMP leaves a private scalar uninitialized. Check distinct storage
    // and isolation of writes without reading its indeterminate initial value.
    assert(&i != original_i);
    i = 42;
  }
  assert(i == 100);

  return 0;
}
