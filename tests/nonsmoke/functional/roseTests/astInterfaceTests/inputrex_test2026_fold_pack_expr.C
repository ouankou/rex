#include <cstddef>

template <typename... Ts> constexpr std::size_t pack_count() {
  return sizeof...(Ts);
}

template <typename... Ts> constexpr bool pack_noexcept() {
  return noexcept(sizeof...(Ts));
}

template <typename... Ts> constexpr int fold_sum(Ts... ts) {
  return (ts + ... + 0);
}

int main() {
  static_assert(pack_noexcept<int, double>(), "");
  constexpr std::size_t count = pack_count<int, double, char>();
  return fold_sum(1, 2, 3) + static_cast<int>(count);
}
