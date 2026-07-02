struct RexTest2026LookupCount {
  int value;
};

void rex_test2026_omp_expr_lookup_current_file_a(int *data) {
  RexTest2026LookupCount rex_test2026_extent = {3};
  int local_extent = rex_test2026_extent.value;

#pragma omp target map(tofrom : data[0 : local_extent])
  {
    data[0] += local_extent;
  }
}
