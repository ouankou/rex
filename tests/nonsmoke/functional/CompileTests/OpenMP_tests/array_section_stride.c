void update_stride(int *a, int n) {
#pragma omp target map(tofrom : a[0 : n : 2])
  {
#pragma omp parallel for
    for (int i = 0; i < n; i += 2) {
      a[i] = i;
    }
  }
}
