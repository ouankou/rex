#include <cstddef>
#include <tuple>
#include <vector>

using Tup = std::tuple<int, double, std::vector<int>>;
using Nested = std::tuple<Tup, std::tuple<char, long>>;
constexpr std::size_t kSize = std::tuple_size_v<Tup>;

int main() {
  Tup t{1, 2.0, {3}};
  Nested nested{t, {'x', 42}};
  (void)nested;
  (void)kSize;
  return std::get<0>(t);
}
