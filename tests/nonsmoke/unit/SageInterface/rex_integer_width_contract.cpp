#include "integerWidth.h"

#include <cstdint>
#include <limits>

using SageInterface::Detail::unsignedValueFitsIn;

static_assert(unsignedValueFitsIn<std::int64_t>(
    std::numeric_limits<std::uint32_t>::max()));
static_assert(unsignedValueFitsIn<std::int64_t>(
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
static_assert(!unsignedValueFitsIn<std::int64_t>(
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1));
static_assert(!unsignedValueFitsIn<std::int64_t>(
    std::numeric_limits<std::uint64_t>::max()));
static_assert(unsignedValueFitsIn<unsigned long long>(
    std::numeric_limits<std::size_t>::max()));

int main() {}
