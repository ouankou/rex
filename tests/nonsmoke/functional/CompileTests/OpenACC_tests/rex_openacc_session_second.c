void rex_openacc_session_second(int *values, int count) {
#pragma acc data copy(values[0 : count])
  {
    values[0] = count;
  }
#pragma acc cache(readonly : values[0 : count])
#pragma acc wait(devnum:count:queues : count, count + 1)
}
