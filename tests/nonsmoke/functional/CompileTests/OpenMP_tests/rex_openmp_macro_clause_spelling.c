#define NUM_THREADS 4
#define NUM_TASKS 2

void rex_openmp_macro_clause_spelling(int *values, int count) {
#pragma omp parallel num_threads(NUM_THREADS)
  {
#pragma omp single
    {
#pragma omp taskloop num_tasks(NUM_TASKS)
      for (int i = 0; i < count; ++i) {
        values[i] += i;
      }
    }
  }
}
