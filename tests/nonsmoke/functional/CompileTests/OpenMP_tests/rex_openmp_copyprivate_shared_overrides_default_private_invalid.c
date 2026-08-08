int main(void) {
  int value = 0;

#pragma omp parallel default(private) shared(value)
  {
#pragma omp single copyprivate(value)
    {
      value = 42;
    }
  }
  return 0;
}
