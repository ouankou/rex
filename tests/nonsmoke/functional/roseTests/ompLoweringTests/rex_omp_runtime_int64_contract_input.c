typedef int int64_t;

void rex_omp_runtime_int64_contract(int *value) {
#pragma omp target map(tofrom : value[0 : 1])
  {
    value[0] += 1;
  }
}
