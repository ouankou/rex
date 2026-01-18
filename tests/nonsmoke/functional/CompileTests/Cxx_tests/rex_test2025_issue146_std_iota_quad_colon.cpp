#include <array>

#include <numeric>

int main() {
  std::array<double, 4> x{};
  std::iota(x.begin(), x.end(), 0.0);
  return 0;
}
