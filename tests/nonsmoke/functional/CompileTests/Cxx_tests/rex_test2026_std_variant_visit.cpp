#include <string>
#include <variant>

namespace vendor {
namespace conversion {

using Scalar = std::variant<char, int, double, std::string>;

struct ToLongDouble {
  long double operator()(char value) const {
    return static_cast<long double>(value - '0');
  }

  long double operator()(int value) const {
    return static_cast<long double>(value);
  }

  long double operator()(double value) const {
    return static_cast<long double>(value);
  }

  long double operator()(const std::string &value) const {
    return std::stold(value);
  }
};

long double toLongDouble(const Scalar &value) {
  return std::visit(ToLongDouble(), value);
}

} // namespace conversion
} // namespace vendor

long double sumScalars() {
  vendor::conversion::Scalar first('3');
  vendor::conversion::Scalar second(std::string("10.5"));
  vendor::conversion::Scalar third(2);

  return vendor::conversion::toLongDouble(first) +
         vendor::conversion::toLongDouble(second) +
         vendor::conversion::toLongDouble(third);
}
