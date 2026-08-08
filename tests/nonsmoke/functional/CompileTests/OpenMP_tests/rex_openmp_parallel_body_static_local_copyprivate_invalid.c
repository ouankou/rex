int main(void) {
#pragma omp parallel
  {
    static int value;

#pragma omp single copyprivate(value)
    {
      value = 42;
    }
  }
  return 0;
}
