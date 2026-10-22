struct RexVariantValue {
  int rex_variant_base;
};
#ifdef REX_VARIANT_UNDEFINED_HELPER
static int rex_variant_helper(int value);
int rex_variant_base(int value) { return value + 1; }
#else
static int rex_variant_helper(int value) { return value + 1; }
int rex_variant_base(int value) { return rex_variant_helper(value); }
#endif
int (*rex_variant_before)(int) = rex_variant_base;
#ifdef REX_VARIANT_INSIDE_FUNCTION
int rex_variant_invalid(int value) {
#pragma omp begin declare variant match(construct = {parallel})
  value += 1;
#pragma omp end declare variant
  return value;
}
#else
#pragma omp begin declare variant match(construct = {parallel})
int rex_variant_global = 7;
int rex_variant_base(int value) {
  struct RexVariantValue field = {2};
  return rex_variant_helper(value) + field.rex_variant_base;
}
int rex_variant_later(int value) { return value + 3; }
#pragma omp begin declare variant match(construct = {target})
int rex_variant_base(int value) { return value + 4; }
#pragma omp end declare variant
#pragma omp end declare variant
int rex_variant_later(int value) { return value + 5; }
int main(void) {
  int parallel_result = 0;
#pragma omp parallel num_threads(2)
  {
#pragma omp single
    parallel_result = rex_variant_base(1);
  }
  return rex_variant_before(1) != 2 || rex_variant_base(1) != 2 ||
         rex_variant_global != 7 || rex_variant_later(1) != 6 ||
         parallel_result != 4;
}
#endif
