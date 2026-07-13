int rex_argument;

#pragma omp declare simd uniform(rex_argument)
#pragma omp declare simd linear(rex_argument : 1)
int rex_openmp_declare_simd_exact_target(int rex_argument);

int rex_openmp_declare_simd_exact_target(int rex_argument) {
  return rex_argument + 1;
}
