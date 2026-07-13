#include <complex>

double rex_system_header_local_constructor_family(double real, double imag) {
  return std::abs(std::complex<double>(real, imag));
}
