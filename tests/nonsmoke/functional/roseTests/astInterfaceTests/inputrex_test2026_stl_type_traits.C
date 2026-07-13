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

namespace left_scope {
template <typename T> using scoped_alias = T *;
using scoped_result = scoped_alias<int>;
} // namespace left_scope

namespace right_scope {
template <typename T> using scoped_alias = const T *;
using scoped_result = scoped_alias<int>;
} // namespace right_scope

template <typename T> struct traits_user {
  static constexpr bool is_integral = std::is_integral_v<T>;
};

static_assert(traits_user<int>::is_integral, "expected integral");
static_assert(std::is_same_v<recursive_result, int>, "expected int");
static_assert(std::is_same_v<left_scope::scoped_result, int *>,
              "left alias must retain its source scope");
static_assert(std::is_same_v<right_scope::scoped_result, const int *>,
              "right alias must retain its source scope");

int main() { return ic::value + (std::is_same_v<alias_result, int *> ? 1 : 0); }
