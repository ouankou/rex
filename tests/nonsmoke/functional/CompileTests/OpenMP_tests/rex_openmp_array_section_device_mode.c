void rex_openmp_array_section_device_mode(int matrix[4][8], int *values,
                                          int length) {
#pragma omp target device(*)                                                   \
    map(tofrom : matrix[1 : 2][2 : 3], values[ : length])
  {
    values[0] = matrix[1][2];
  }
}
