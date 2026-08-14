#include <variant>

using Value = std::variant<int, double>;

int visitValue(Value &value) {
  return std::visit([](auto item) { return static_cast<int>(item); }, value);
}
