typedef struct Vec {
  int len;
  float *data;
} Vec;

#pragma omp declare mapper(default : Vec v)                                    \
    map(tofrom : v.len, v.data[0 : v.len])

void touch(Vec *v, int n) {
#pragma omp target data map(mapper(default), tofrom : v[0 : n])
  {
#pragma omp target update to(mapper(default) : v[0 : n])                       \
    from(mapper(default) : v[0 : n])
#pragma omp target map(mapper(default), tofrom : v[0 : n])
    {
      v->len = v->len + 1;
    }
  }
}
