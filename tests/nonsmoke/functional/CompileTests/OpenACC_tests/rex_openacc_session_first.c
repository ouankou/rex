void rex_openacc_session_first(int *values, int count) {
#pragma acc parallel default(present) copy(values[0 : count]) async(count)     \
    num_gangs(2) reduction(+ : count)
  {
    values[0] = count;
  }
}
