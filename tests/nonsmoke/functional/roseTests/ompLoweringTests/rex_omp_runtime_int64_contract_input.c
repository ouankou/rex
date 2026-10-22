typedef int int64_t;

void rex_omp_runtime_int64_contract(int *value) {
#pragma omp target map(tofrom : value[0 : 1])
  {
    value[0] += 1;
  }
}

void rex_omp_wide_induction(int *values) {
#pragma omp for schedule(dynamic, 2)
  for (long long i = 2147483648LL; i < 2147483656LL; ++i) {
    values[i - 2147483648LL] += sizeof(i) == sizeof(long long);
  }
}
