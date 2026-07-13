int rex_openmp_variant_parallel(int value);
int rex_openmp_variant_teams(int value);

#pragma omp declare variant(rex_openmp_variant_parallel)                       \
    match(construct = {parallel})
#pragma omp declare variant(rex_openmp_variant_teams) match(construct = {teams})
int rex_openmp_variant_base(int value);

int rex_openmp_variant_parallel(int value) { return value + 1; }
int rex_openmp_variant_teams(int value) { return value + 2; }
int rex_openmp_variant_base(int value) { return value; }
