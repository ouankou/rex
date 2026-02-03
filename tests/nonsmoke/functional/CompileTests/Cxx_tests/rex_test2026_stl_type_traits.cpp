#include <type_traits>

template <typename T> struct nested_alias {
  template <typename U>
  using alias =
      std::conditional_t<std::is_same_v<U, T>, std::integral_constant<int, 1>,
                         std::integral_constant<int, 0>>;
};

template <typename T> struct remove_all_pointers {
  using type = T;
};

template <typename T> struct remove_all_pointers<T *> {
  using type = typename remove_all_pointers<T>::type;
};

template <typename T>
using remove_all_pointers_t = typename remove_all_pointers<T>::type;

template <typename T>
using enable_if_integral_t = std::enable_if_t<std::is_integral_v<T>, int>;

template <typename T, typename = enable_if_integral_t<T>>
constexpr int trait_value(T) {
  return 1;
}

int main() {
  static_assert(std::integral_constant<int, 42>::value == 42, "");
  using nested = nested_alias<int>::alias<int>;
  static_assert(nested::value == 1, "");
  static_assert(std::is_same_v<remove_all_pointers_t<int ***>, int>, "");
  return trait_value(1) + nested::value;
}
