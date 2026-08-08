int main(void) {
  int value = 0;

#pragma omp parallel private(value)
  {
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
