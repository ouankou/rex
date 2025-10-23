#include <array>
#include <cstddef>
#include <cmath>
#include <numeric>

static constexpr std::size_t kElements = 1u << 10;

static void axpy(double a, const double *x, double *y, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    y[i] = a * x[i] + y[i];
  }
}

static double checksum(const double *values, std::size_t n) {
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    sum += values[i];
  }
  return sum;
}

int main() {
  const double a = 2.5;

  std::array<double, kElements> x{};
  std::array<double, kElements> y{};

  std::iota(x.begin(), x.end(), 0.0);
  std::iota(y.begin(), y.end(), 0.0);
  for (std::size_t i = 0; i < y.size(); ++i) {
    y[i] *= 2.0;
  }

  axpy(a, x.data(), y.data(), y.size());

  const double result = checksum(y.data(), y.size());
  const double expected =
      (a + 2.0) * static_cast<double>((kElements - 1) * kElements) * 0.5;

  const double rel_error = std::fabs(result - expected) / expected;
  if (rel_error > 1e-9) {
    return 1;
  }

  return 0;
}
