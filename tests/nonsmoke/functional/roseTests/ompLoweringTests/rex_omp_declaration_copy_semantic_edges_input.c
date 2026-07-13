#pragma omp requires unified_shared_memory

#pragma omp assumes absent(task)

#pragma omp begin assumes contains(parallel)
int rex_omp_copy_assumed_function(int value);
#pragma omp end assumes

typedef struct RexOmpCopyMapperValue {
  int member;
} RexOmpCopyMapperValue;

int rex_omp_copy_threadprivate_value;
#pragma omp threadprivate(rex_omp_copy_threadprivate_value)

int rex_omp_copy_groupprivate_value;
#pragma omp groupprivate(rex_omp_copy_groupprivate_value) device_type(host)

int rex_omp_copy_variant_function(int value);
#pragma omp declare variant(rex_omp_copy_variant_function)                     \
    match(construct = {parallel})
int rex_omp_copy_base_function(int value);

#pragma omp begin declare variant match(construct = {parallel})
int rex_omp_copy_region_variant(int value);
#pragma omp end declare variant

#pragma omp declare mapper(                                                    \
        rex_omp_copy_mapper : RexOmpCopyMapperValue rex_omp_copy_mapper_value) \
    map(tofrom : rex_omp_copy_mapper_value.member)

int rex_omp_copy_simd_uniform;
#pragma omp declare simd uniform(value)
int rex_omp_copy_simd_function(int value);

int rex_omp_copy_declare_target_value;
#pragma omp declare target to(rex_omp_copy_declare_target_value)               \
    device_type(host)

int rex_omp_copy_assumed_function(int value) { return value; }
int rex_omp_copy_variant_function(int value) { return value + 1; }
int rex_omp_copy_base_function(int value) { return value; }
int rex_omp_copy_region_variant(int value) { return value + 2; }
int rex_omp_copy_simd_function(int value) {
  return value + rex_omp_copy_simd_uniform;
}
