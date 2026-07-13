void rex_openacc_unsupported_data_modifier(int *values, int count) {
#pragma acc parallel copyin(readonly : values[0 : count])
  {
    values[0] = count;
  }
}
