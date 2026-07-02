#pragma omp requires unified_shared_memory

typedef struct RexTest2026Vec {
  int len;
  float *data;
} RexTest2026Vec;

int rex_test2026_global;
#pragma omp threadprivate(rex_test2026_global)

int rex_test2026_variant(int x);
#pragma omp declare variant(rex_test2026_variant) match(construct = {parallel})
int rex_test2026_base(int x);

#pragma omp declare mapper(default : RexTest2026Vec v)                         \
    map(tofrom : v.len, v.data[0 : v.len])

#pragma omp declare simd
int rex_test2026_simd(int x);

#pragma omp declare target
int rex_test2026_target_value;
#pragma omp end declare target

void rex_test2026_work(RexTest2026Vec *v) {
  int local = 0;
#pragma omp allocate(local) allocator(omp_default_mem_alloc)
#pragma omp taskwait
  v->len += local;
}

int rex_test2026_variant(int x) { return x + 1; }
int rex_test2026_base(int x) { return x; }
int rex_test2026_simd(int x) { return x * 2; }
