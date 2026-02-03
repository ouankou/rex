#include <string>
#include <tuple>
#include <variant>

using V = std::variant<int, std::string, std::tuple<int, double>>;

int main() {
  V v = 3;
  return std::holds_alternative<int>(v) ? 0 : 1;
}
