#include <type_traits>

template <typename T>
using nested_alias = std::add_pointer_t<std::remove_reference_t<T>>;

template <typename T> struct remove_all_pointers {
  using type = T;
};

template <typename T> struct remove_all_pointers<T *> {
  using type = typename remove_all_pointers<T>::type;
};

using ic = std::integral_constant<int, 7>;
using alias_result = nested_alias<int &>;
using recursive_result = remove_all_pointers<int ***>::type;

template <typename T> struct traits_user {
  static constexpr bool is_integral = std::is_integral_v<T>;
};

static_assert(traits_user<int>::is_integral, "expected integral");
static_assert(std::is_same_v<recursive_result, int>, "expected int");

int main() { return ic::value + (std::is_same_v<alias_result, int *> ? 1 : 0); }
