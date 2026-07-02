void rex_test2026_omp_expr_lookup_current_file_b(int *data) {
  int rex_test2026_extent = 5;

#pragma omp target map(tofrom : data[0 : rex_test2026_extent])
  {
    data[0] += rex_test2026_extent;
  }
}
