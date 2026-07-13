int rex_openmp_taskgraph_target_valid(int y) {
#pragma omp taskgraph
  {
#pragma omp target nowait map(tofrom : y) depend(out : y)
    ++y;

#pragma omp task depend(inout : y) shared(y)
    ++y;

#pragma omp target nowait map(tofrom : y) depend(in : y)
    ++y;
  }
  return y;
}
