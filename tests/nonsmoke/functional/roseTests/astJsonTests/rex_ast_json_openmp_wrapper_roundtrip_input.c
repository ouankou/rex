int rex_ast_json_group_sum;

#pragma omp groupprivate(rex_ast_json_group_sum) device_type(host)

void rex_ast_json_openmp_wrapper_roundtrip(int *values, int count) {
  int local = count;

#pragma omp allocate(local)
#pragma omp flush(local, values)
#pragma omp target update to(values[0 : count])
#pragma omp parallel shared(values, count)
  {
    if (count > 0) {
      values[0] += local;
    }
  }
}
