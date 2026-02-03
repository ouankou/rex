#include <tuple>
#include <utility>

template <std::size_t... I>
constexpr int sum_indices(std::index_sequence<I...>) {
  return (0 + ... + static_cast<int>(I));
}

int main() {
  auto t = std::make_tuple(1, 2.0, 3);
  constexpr int sum = sum_indices(std::make_index_sequence<3>{});
  return std::get<0>(t) + std::get<2>(t) + sum;
}
