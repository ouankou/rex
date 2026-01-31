#include <type_traits>

template <typename...> using void_t = void;

template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, void_t<typename T::value_type>> : std::true_type {};

struct WithValueType {
  using value_type = int;
};

struct WithoutValueType {};

template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template <typename T>
constexpr enable_if_t<std::is_integral<T>::value, int> pick(T) {
  return 1;
}

template <typename T>
constexpr enable_if_t<std::is_floating_point<T>::value, int> pick(T) {
  return 2;
}

template <typename T>
constexpr enable_if_t<has_value_type<T>::value, int> uses_value_type(T) {
  return 3;
}

template <typename T>
constexpr enable_if_t<!has_value_type<T>::value, int> uses_value_type(T) {
  return 4;
}

int main() {
  static_assert(has_value_type<WithValueType>::value, "expected value_type");
  static_assert(!has_value_type<WithoutValueType>::value,
                "unexpected value_type");
  return pick(1) + pick(1.0) + uses_value_type(WithValueType{}) +
         uses_value_type(WithoutValueType{});
}
