typedef struct RexMapperValue {
  int len;
  float *data;
} RexMapperValue;

#pragma omp declare mapper(rex_whole : RexMapperValue v)                       \
    map(tofrom : v, v.data[0 : v.len])

void rex_openmp_declare_mapper_local_symbol(RexMapperValue *value) {
#pragma omp target map(mapper(rex_whole), tofrom : value[0 : 1])
  value->len += 1;
}
