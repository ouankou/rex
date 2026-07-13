int rex_openmp_sage_overload(int value) { return value; }
double rex_openmp_sage_overload(double value) { return value; }

void rex_openmp_sage_name_overload_rejected() {
#pragma omp parallel if (rex_openmp_sage_overload(1))
  {
  }
}
