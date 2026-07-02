static void rex_test2026_root_order_first(int *out, int n) {
#pragma omp parallel shared(out, n)
  {
#pragma omp single
    out[0] = n + 1;
  }

#pragma omp task shared(out, n)
  {
    out[1] = n + 2;
  }
}

static void rex_test2026_root_order_second(int *out, int n) {
#pragma omp parallel shared(out, n)
  {
#pragma omp single
    out[2] = n + 3;
  }

#pragma omp parallel shared(out, n)
  {
#pragma omp single
    out[3] = n + 4;
  }
}

void rex_test2026_omp_lowering_deterministic_root_order(int *out, int n) {
  rex_test2026_root_order_first(out, n);
  rex_test2026_root_order_second(out, n);
}
