#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename T> using Alloc = std::allocator<T>;

template <typename T>
using RebindAlloc =
    typename std::allocator_traits<Alloc<T>>::template rebind_alloc<long>;

template <std::size_t... I>
constexpr std::size_t sum_index(std::index_sequence<I...>) {
  return (I + ... + 0);
}

template <int N>
struct factorial : std::integral_constant<int, N * factorial<N - 1>::value> {};

template <> struct factorial<0> : std::integral_constant<int, 1> {};

template <typename T> struct Outer {
  template <typename U> using Alias = typename std::add_pointer<U>::type;
};

using AliasType = Outer<int>::Alias<double>;
using Const42 = std::integral_constant<int, 42>;

int main() {
  std::tuple<int, double> t{1, 2.0};
  auto idx = std::make_index_sequence<3>{};
  RebindAlloc<int> alloc;
  (void)alloc;
  constexpr std::size_t sum = sum_index(idx);
  static_assert(Const42::value == 42, "integral_constant value mismatch");
  static_assert(std::is_same_v<AliasType, double *>, "alias mismatch");
  constexpr int fact = factorial<5>::value;
  return std::get<0>(t) + static_cast<int>(sum) + fact;
}
