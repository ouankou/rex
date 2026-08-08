thread_local int rex_thread_value;

int main() {
#pragma omp parallel
#pragma omp single copyprivate(rex_thread_value)
  {
    rex_thread_value = 42;
  }
  return 0;
}
