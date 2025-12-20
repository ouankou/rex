#pragma clang system_header
#ifndef REX_TEST2025_ISSUE160_SYSTEM_HEADER_CLASS_INSTANTIATION_H
#define REX_TEST2025_ISSUE160_SYSTEM_HEADER_CLASS_INSTANTIATION_H

namespace rex_test2025_issue160 {

template <typename T> struct Box {
  T value;
  T get() const { return value; }
};

} // namespace rex_test2025_issue160

#endif
