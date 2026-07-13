int rex_openmp_group_sum;

typedef struct RexOpenMPOwnerValue {
  int value;
} RexOpenMPOwnerValue;

#pragma omp declare mapper(rex_owner : RexOpenMPOwnerValue value)              \
    map(tofrom : value)

#pragma omp groupprivate(rex_openmp_group_sum) device_type(any)

void rex_openmp_clause_list_ownership(void) {
  int first = 1;
  int second = 2;
  RexOpenMPOwnerValue mapped = {1};

#pragma omp parallel private(first)
  {
    first += 1;
  }

#pragma omp parallel private(second)
  {
    second += 1;
  }

#pragma omp target map(mapper(rex_owner), tofrom : mapped)
  {
    mapped.value += 1;
  }

#pragma omp target update to(first) nowait

#pragma omp flush acq_rel(first)

#pragma omp allocate(second)
}
