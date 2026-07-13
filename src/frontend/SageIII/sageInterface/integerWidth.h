#ifndef ROSE_SAGE_INTERFACE_INTEGER_WIDTH_H
#define ROSE_SAGE_INTERFACE_INTEGER_WIDTH_H

#include <limits>
#include <type_traits>

namespace SageInterface {
namespace Detail {

template <typename Target, typename Source>
constexpr bool unsignedValueFitsIn(Source value) noexcept {
  static_assert(std::is_integral_v<Target>,
                "integer-width target must be integral");
  static_assert(std::is_integral_v<Source>,
                "integer-width source must be integral");
  static_assert(std::is_unsigned_v<Source>,
                "integer-width source must be unsigned");

  if constexpr (std::numeric_limits<Source>::digits >
                std::numeric_limits<Target>::digits) {
    return value <= static_cast<Source>(std::numeric_limits<Target>::max());
  }
  return true;
}

} // namespace Detail
} // namespace SageInterface

#endif
