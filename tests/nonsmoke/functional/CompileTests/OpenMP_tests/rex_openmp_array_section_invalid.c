void rex_openmp_array_section_invalid(void) {
  int scalar = 0;
#pragma omp target map(to : scalar[0 : 1])
  {
    scalar = 1;
  }
}
