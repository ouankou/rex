int main(void) {
  int i;
  int n = 32;
  int a[32];

  for (i = 0; i < n; ++i) {
    a[i] = i;
  }

#pragma omp target parallel for
  for (i = 0; i < n; ++i) {
    a[i] = a[i] + n;
  }

  return a[0];
}
