int main(void) {
#pragma omp parallel
  {
    int value = 0;

#pragma omp parallel
    {
#pragma omp single copyprivate(value)
      {
        value = 42;
      }
    }
  }
  return 0;
}
