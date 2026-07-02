template <typename T> struct RexTest2026Vec {
  int len;
  T *data;
};

#pragma omp declare mapper(default : RexTest2026Vec<float> v)                  \
    map(tofrom : v.len, v.data[0 : v.len])

void rex_test2026_mapper_template_reachable_scope(RexTest2026Vec<float> *v,
                                                  int n) {
#pragma omp target data map(mapper(default), tofrom : v[0 : n])
  {
#pragma omp target update to(mapper(default) : v[0 : n])
#pragma omp target map(mapper(default), tofrom : v[0 : n])
    {
      v->len += 1;
    }
  }
}
