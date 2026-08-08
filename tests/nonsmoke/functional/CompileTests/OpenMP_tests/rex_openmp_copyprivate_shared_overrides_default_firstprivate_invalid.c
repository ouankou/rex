int main(void) {
  int value = 0;

#pragma omp parallel shared(value) default(firstprivate)
  {
#pragma omp single copyprivate(value)
    {
      value = 42;
    }
  }
  return 0;
}
