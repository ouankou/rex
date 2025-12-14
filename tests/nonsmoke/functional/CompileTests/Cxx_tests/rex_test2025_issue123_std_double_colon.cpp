#include <tuple>

using T = std::tuple<int, double>;

int main() {
  T t{};
  return (int)std::get<0>(t);
}
