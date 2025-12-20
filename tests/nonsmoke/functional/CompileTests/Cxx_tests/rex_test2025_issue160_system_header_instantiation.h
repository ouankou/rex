#pragma clang system_header
#ifndef REX_TEST2025_ISSUE160_SYSTEM_HEADER_INSTANTIATION_H
#define REX_TEST2025_ISSUE160_SYSTEM_HEADER_INSTANTIATION_H

namespace rex_test2025_issue160 {

template <typename T> T scale_and_shift(T value, T scale) {
  return value * scale + value;
}

struct Wrapper {
  int value;
  int bump() const { return value + 1; }
};

} // namespace rex_test2025_issue160

#endif
