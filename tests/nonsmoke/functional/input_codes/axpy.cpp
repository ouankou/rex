static void axpy(double a, const double *x, double *y, int n) {
  for (int i = 0; i < n; ++i) {
    y[i] = a * x[i] + y[i];
  }
}

static double checksum(const double *values, int n) {
  double sum = 0.0;
  for (int i = 0; i < n; ++i) {
    sum += values[i];
  }
  return sum;
}

int main() {
  const int n = 1 << 10; // 1024 elements
  const double a = 2.5;

  double x[1 << 10];
  double y[1 << 10];

  for (int i = 0; i < n; ++i) {
    x[i] = (double)i;
    y[i] = (double)(2 * i);
  }

  axpy(a, x, y, n);

  const double result = checksum(y, n);
  const double expected = (a + 2.0) * (double)((n - 1) * n) * 0.5;

  double diff = result - expected;
  if (diff < 0.0) {
    diff = -diff;
  }
  const double rel_error = diff / expected;
  if (rel_error > 1e-9) {
    return 1;
  }

  return 0;
}
