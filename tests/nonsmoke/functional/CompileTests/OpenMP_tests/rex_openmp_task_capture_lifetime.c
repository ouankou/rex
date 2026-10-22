#include <assert.h>

struct aligned_value {
  _Alignas(64) int value;
};

static int completed;

static void mark_completed(void) { completed = 1; }

static void produce(int *results) {
  int value = 37;
  int values[2] = {4, 9};
  struct aligned_value aligned = {23};
#pragma omp task firstprivate(value, values, aligned, results)
  {
    results[0] = value;
    results[1] = values[0];
    results[2] = values[1];
    results[4] = aligned.value;
  }
#pragma omp task if (0) firstprivate(results)
  {
    results[3] = 11;
  }
  assert(results[3] == 11);
#pragma omp task
  {
    mark_completed();
  }
  value = 0;
  values[0] = 0;
  values[1] = 0;
  aligned.value = 0;
}

int main(void) {
  int results[5] = {0};
#pragma omp parallel
  {
#pragma omp single
    {
      produce(results);
#pragma omp taskwait
      assert(results[0] == 37);
      assert(results[1] == 4);
      assert(results[2] == 9);
      assert(results[3] == 11);
      assert(results[4] == 23);
      assert(completed == 1);
    }
  }
  return 0;
}
