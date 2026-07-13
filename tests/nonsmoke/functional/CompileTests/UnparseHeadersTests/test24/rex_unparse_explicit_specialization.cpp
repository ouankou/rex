#include "rex_unparse_explicit_specialization.hpp"

template <> void RexUnparseExplicitSpecialization<int>::touch() {
  bits.push_back(true);
}

int main() {
  RexUnparseExplicitSpecialization<int> value;
  value.touch();
  return value.bits.empty() ? 1 : 0;
}
