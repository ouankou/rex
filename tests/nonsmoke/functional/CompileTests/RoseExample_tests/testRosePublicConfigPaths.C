#include "rosePublicConfig.h"
#include "rosedefs.h"

#include <cstddef>

template <std::size_t N> constexpr bool isPathLiteral(const char (&)[N]) {
  return N > 1;
}

static_assert(isPathLiteral(ROSE_COMPILE_TREE_PATH),
              "ROSE_COMPILE_TREE_PATH must be a string literal");
static_assert(isPathLiteral(ROSE_SOURCE_TREE_PATH),
              "ROSE_SOURCE_TREE_PATH must be a string literal");
