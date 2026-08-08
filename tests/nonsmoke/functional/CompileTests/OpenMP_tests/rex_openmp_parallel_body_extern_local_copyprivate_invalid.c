int value;

int main(void) {
#pragma omp parallel
  {
    extern int value;

#pragma omp single copyprivate(value)
    {
      value = 42;
    }
  }
  return 0;
}
