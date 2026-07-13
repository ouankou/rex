typedef struct RexMapperIdentityA {
  int value;
} RexMapperIdentityA;

typedef struct RexMapperIdentityB {
  long value;
} RexMapperIdentityB;

RexMapperIdentityA item_a;
RexMapperIdentityA item_b;
RexMapperIdentityB item_c;

#pragma omp declare mapper(rex_mapper_alpha : RexMapperIdentityA item_a)       \
    map(tofrom : item_a)
#pragma omp declare mapper(rex_mapper_beta : RexMapperIdentityA item_b)        \
    map(tofrom : item_b)
#pragma omp declare mapper(rex_mapper_alpha : RexMapperIdentityB item_c)       \
    map(tofrom : item_c)

int rex_openmp_mapper_mangled_identity(void) { return 0; }
