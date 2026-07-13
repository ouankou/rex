#ifndef REX_FRONTEND_APPLICATION_HEADER_SEMANTIC_PRAGMA_OWNER_HPP
#define REX_FRONTEND_APPLICATION_HEADER_SEMANTIC_PRAGMA_OWNER_HPP

inline int rex_application_header_semantic_pragma_owner(int lhs, int rhs) {
  int result;
#pragma rose_outline
  result = lhs + rhs;
  return result;
}

#endif
