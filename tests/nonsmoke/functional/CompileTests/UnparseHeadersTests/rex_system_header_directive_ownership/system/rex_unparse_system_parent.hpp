#ifndef REX_UNPARSE_SYSTEM_PARENT_HPP
#define REX_UNPARSE_SYSTEM_PARENT_HPP

#include <rex_unparse_system_child.hpp> // rex_compare<void>, rex_less<int>

inline int rex_system_header_parent(int value) {
  return rex_system_header_child(value);
}

#endif
