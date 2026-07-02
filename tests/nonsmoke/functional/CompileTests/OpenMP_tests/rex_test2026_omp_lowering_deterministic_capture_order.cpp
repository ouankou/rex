int rex_test2026_omp_lowering_deterministic_capture_order(int *out, int n) {
  int alpha = n + 1;
  int beta = n + 2;
  int gamma = n + 3;
  int total = 0;

#pragma omp parallel firstprivate(gamma, alpha, beta) shared(out)              \
    reduction(+ : total)
  {
    int local = alpha + beta + gamma;
#pragma omp single
    out[0] = local;
    total += local;
  }

  return total;
}
