#ifndef REX_UNPARSE_HEADERS_TEST19_VECTOR_BOOL_TEMPLATE_H
#define REX_UNPARSE_HEADERS_TEST19_VECTOR_BOOL_TEMPLATE_H

#include <vector>

template <typename T> struct VectorBoolTemplate {
  std::vector<bool> bits;

  void touch();
};

#endif
