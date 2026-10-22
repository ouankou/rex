struct RexOmpVariantMember {
  int rex_omp_variant_identity;
};

void rex_omp_variant_identity(int *value) { *value += 1; }

#pragma omp begin declare variant match(construct = {parallel})
void rex_omp_variant_identity(int *value) {
  if (*value < 0)
    rex_omp_variant_identity(value);
  struct RexOmpVariantMember member = {2};
  *value += member.rex_omp_variant_identity;
}
#pra\
gma omp end declare vari\ant

#pragma omp begin declare variant match(construct = {target})
void rex_omp_variant_identity(int *value) { *value += 3; }
#pragma omp end declare variant

int rex_omp_declare_variant_semantic_identity(int *value) {
  rex_omp_variant_identity(value);
  return *value;
}
