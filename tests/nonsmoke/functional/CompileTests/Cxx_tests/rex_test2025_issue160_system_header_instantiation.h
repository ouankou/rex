#pragma clang system_header
#ifndef REX_TEST2025_ISSUE160_SYSTEM_HEADER_INSTANTIATION_H
#define REX_TEST2025_ISSUE160_SYSTEM_HEADER_INSTANTIATION_H

namespace rex_test2025_issue160 {

template <typename T> struct ForwardOnly;

template <typename T> T scale_and_shift(T value, T scale) {
  return value * scale + value;
}

template <typename T> T namespace_adjust(T value) { return value - 1; }

template <typename T> struct DefaultStorage {
  using type = T *;
};

template <typename T, typename Storage = typename DefaultStorage<T>::type>
struct DefaultedWrapper {
  Storage value;
};

template <typename Value> struct SystemHeaderIterator {
  Value *value;

  friend bool operator!=(const SystemHeaderIterator &lhs,
                         const SystemHeaderIterator &rhs) {
    return lhs.value != rhs.value;
  }
};

struct Wrapper {
  int value;
  int bump() const { return value + 1; }
};

} // namespace rex_test2025_issue160

#endif
