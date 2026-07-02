void rex_test2026_array_section_optional_bounds(int *a, int n, int start) {
#pragma omp target map(tofrom : a[ : n])
  {
    a[0] = n;
  }

#pragma omp target map(tofrom : a[start : ])
  {
    a[start] = n;
  }

#pragma omp target map(tofrom : a[::2])
  {
    a[0] = start;
  }
}
