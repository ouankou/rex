#include <string>
#include <variant>

int main() {
  std::variant<int, std::string> v = std::string("rex");
  if (std::holds_alternative<std::string>(v)) {
    return static_cast<int>(std::get<std::string>(v).size());
  }
  return std::get<int>(v);
}
