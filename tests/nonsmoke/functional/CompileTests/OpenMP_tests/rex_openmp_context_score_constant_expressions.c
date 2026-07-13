enum { rex_openmp_score_seed = 3 };

int rex_openmp_score_variant_arithmetic(int value);
int rex_openmp_score_variant_conditional(int value);
int rex_openmp_score_variant_bitwise(int value);

#pragma omp declare variant(rex_openmp_score_variant_arithmetic)               \
    match(implementation = {                                                   \
                  vendor(score((((rex_openmp_score_seed + 2) * 3 - 4) << 1) |  \
                                   (~0 & 1)) : llvm)})
int rex_openmp_score_base_arithmetic(int value);

#pragma omp declare variant(rex_openmp_score_variant_conditional)              \
    match(implementation = {vendor(score(                                      \
                  (0 || 4 > 3) ? (8 >> 1) + 6 / 3 + 5 % 2 : -1) : llvm)})
int rex_openmp_score_base_conditional(int value);

#pragma omp declare variant(rex_openmp_score_variant_bitwise)                  \
    match(implementation = {vendor(score((15 ^ 3) & 7) : llvm)})
int rex_openmp_score_base_bitwise(int value);

int rex_openmp_score_variant_arithmetic(int value) { return value + 1; }
int rex_openmp_score_variant_conditional(int value) { return value + 2; }
int rex_openmp_score_variant_bitwise(int value) { return value + 3; }
int rex_openmp_score_base_arithmetic(int value) { return value; }
int rex_openmp_score_base_conditional(int value) { return value; }
int rex_openmp_score_base_bitwise(int value) { return value; }
