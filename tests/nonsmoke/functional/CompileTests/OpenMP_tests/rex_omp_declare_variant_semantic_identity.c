void rex_omp_variant_identity(int *value) { *value += 1; }

#pragma omp begin declare variant match(construct = {parallel})
void rex_omp_variant_identity(int *value) {
  if (*value < 0)
    rex_omp_variant_identity(value);
  *value += 2;
}
#pragma omp end declare variant

#pragma omp begin declare variant match(construct = {target})
void rex_omp_variant_identity(int *value) { *value += 3; }
#pragma omp end declare variant

int rex_omp_declare_variant_semantic_identity(int *value) {
  rex_omp_variant_identity(value);
  return *value;
}
