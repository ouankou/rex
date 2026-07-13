enum { rex_openmp_sage_binding_value = 3 };

void rex_openmp_sage_binding(int *value) {
  const int rex_openmp_sage_binding_value = 7;

#pragma omp atomic hint(rex_openmp_sage_binding_value)
  (*value)++;
}
