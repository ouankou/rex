#include <cassert>
#include <omp.h>

int main() {
  int values[8] = {};
  int value = 7;
  int temporary = 0;
  [[omp::directive(parallel num_threads(2) shared(values)
                       firstprivate(value) private(temporary))]] {
    assert(omp_in_parallel() && omp_get_num_threads() == 2);
    ++value;
    temporary = value;
    [[omp::directive(for nowait)]]
    for (int i = 0; i < 8; ++i)
      values[i] = temporary + i;
  }
  [[omp::sequence(directive(parallel num_threads(2)), directive(for))]]
  for (int i = 0; i < 8; ++i) {
    assert(omp_in_parallel() && omp_get_num_threads() == 2);
    values[i] += 1;
  }
  for (int i = 0; i < 8; ++i)
    assert(values[i] == value + i + 2);
  assert(value == 7 && temporary == 0);
}
