typedef struct Vec {
  int len;
  float *data;
} Vec;

#pragma omp declare mapper(default : Vec v)                                    \
    map(tofrom : v.len, v.data[0 : v.len])

typedef struct RexMotionValue {
  int sent;
  int received;
} RexMotionValue;

#pragma omp declare mapper(default : RexMotionValue value)                     \
    map(to : value.sent) map(from : value.received)

void touch(Vec *v, Vec *w, int n, RexMotionValue outgoing,
           RexMotionValue incoming) {
#pragma omp target data map(mapper(default), tofrom : v[0 : n], w[0 : n],      \
                                outgoing, incoming)
  {
#pragma omp target update to(mapper(default) : v[0 : n], outgoing)             \
    from(mapper(default) : w[0 : n], incoming)
#pragma omp target map(mapper(default), tofrom : v[0 : n], w[0 : n])
    {
      v->len = w->len;
    }
  }
}
