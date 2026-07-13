void rex_openmp_sage_name_missing_rejected() {
#pragma omp parallel if (rex_openmp_sage_missing_identity)
  {
  }
}
