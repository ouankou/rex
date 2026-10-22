#include <cstddef>

template <typename... Ts> constexpr std::size_t pack_count() {
  return sizeof...(Ts);
}

template <int... Values> constexpr std::size_t value_pack_count() {
  return sizeof...(Values);
}

template <template <class> class... Templates>
constexpr std::size_t template_pack_count() {
  return sizeof...(Templates);
}

template <class> struct pack_box {};

template <typename... Ts>
constexpr std::size_t function_pack_count(Ts... values) {
  return sizeof...(values);
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
  static_assert(value_pack_count<1, 2>() == 2, "");
  static_assert(template_pack_count<pack_box>() == 1, "");
  static_assert(function_pack_count(1, 2, 3) == 3, "");
  return fold_sum(1, 2, 3) + static_cast<int>(count);
}
