#ifndef REX_UNPARSE_EXPLICIT_SPECIALIZATION_HPP
#define REX_UNPARSE_EXPLICIT_SPECIALIZATION_HPP

#include <vector>

template <typename T> struct RexUnparseExplicitSpecialization {
  std::vector<bool> bits;

  void touch();
};

#endif
