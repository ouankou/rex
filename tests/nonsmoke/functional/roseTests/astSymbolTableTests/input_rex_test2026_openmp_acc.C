int g = 0;

#pragma omp threadprivate(g)

void foo(int *data, int n) {
  int local = 0;

#pragma omp flush(g, local)
#pragma omp allocate(local) allocator(omp_default_mem_alloc)
#pragma omp parallel for private(local) firstprivate(n)
  for (int i = 0; i < n; ++i) {
    data[i] = i + local;
  }

#pragma omp target map(tofrom : data[0 : n])
  for (int i = 0; i < n; ++i) {
    data[i] += 1;
  }
}

void bar(int *data, int n) {
#pragma acc parallel loop copy(data) copyin(n)
  for (int i = 0; i < n; ++i) {
    data[i] += 1;
  }

#pragma acc parallel loop copyout(data) num_gangs(2) num_workers(4)            \
    vector_length(8)
  for (int i = 0; i < n; ++i) {
    data[i] += 2;
  }
}

int main() {
  int values[4] = {0, 0, 0, 0};
  foo(values, 4);
  bar(values, 4);
  return values[0];
}
