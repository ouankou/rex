int rex_openmp_ordered_map_sections(void) {
  int values[8] = {0};

#pragma omp target map(tofrom : values[0 : 4], values[4 : 4])
  {
    values[0] = 11;
    values[4] = 31;
  }

  return values[0] + values[4];
}

int rex_openmp_multidimensional_map_sections(void) {
  int values[8][10] = {{0}};

#pragma omp target map(tofrom : values[1 : 2][ : ], values[3 : 2][0 : 10])
  {
    values[1][0] = 13;
    values[3][9] = 37;
  }

  return values[1][0] + values[3][9];
}

int rex_openmp_pointer_to_array_map_section(int (*values)[10]) {
#pragma omp target map(tofrom : values[0 : 2][0 : ])
  {
    values[0][0] = 17;
    values[1][9] = 41;
  }

  return values[0][0] + values[1][9];
}
