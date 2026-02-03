struct Payload {
  int member;
  int arr[8];
};

Payload payload;
int g = 0;

#pragma omp threadprivate(g)

void foo(int *data, int n) {
  int local = 0;
  int local_arr[4] = {0, 1, 2, 3};

#pragma omp flush(g, local, payload.member, data[0], local_arr[1])
#pragma omp allocate(payload.member, local_arr[0])                             \
    allocator(omp_default_mem_alloc)
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
#pragma acc parallel loop copy(data[0 : n]) copyin(payload.member)             \
    copy(payload.arr[0 : 2])
  for (int i = 0; i < n; ++i) {
    data[i] += 1;
  }

#pragma acc parallel loop copyout(data[1 : n]) num_gangs(2) num_workers(4)     \
    vector_length(8)
  for (int i = 0; i < n; ++i) {
    data[i] += 2;
  }

#pragma acc parallel copy(payload.member) copy(payload.arr[0 : 2])
  {
    data[0] += payload.member;
  }
}

int main() {
  int values[4] = {0, 0, 0, 0};
  foo(values, 4);
  bar(values, 4);
  return values[0];
}
