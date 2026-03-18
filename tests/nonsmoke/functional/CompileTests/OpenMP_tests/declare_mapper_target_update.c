typedef struct Vec {
  int len;
  float *data;
} Vec;

#pragma omp declare mapper(default : Vec v)                                    \
    map(tofrom : v.len, v.data[0 : v.len])

void touch(Vec *v) {
#pragma omp target data map(mapper(default), tofrom : v[0 : 1])
  {
#pragma omp target update to(mapper(default) : v[0 : 1])                       \
    from(mapper(default) : v[0 : 1])
#pragma omp target map(mapper(default), tofrom : v[0 : 1])
    {
      v->len = v->len + 1;
    }
  }
}
