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

int main() {
  std::tuple<int, double> t{1, 2.0};
  auto idx = std::make_index_sequence<3>{};
  RebindAlloc<int> alloc;
  (void)alloc;
  constexpr std::size_t sum = sum_index(idx);
  return std::get<0>(t) + static_cast<int>(sum);
}
