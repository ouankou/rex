#include <map>
#include <string>
#include <type_traits>

template <typename T>
constexpr bool is_string_int_map =
    std::is_same_v<T, std::map<std::string, int>>;

int main() {
  std::map<std::string, int> values;
  values["a"] = 1;
  values["b"] = 2;
  int sum = values["a"] + values["b"];
  return sum + (is_string_int_map<decltype(values)> ? 0 : 1);
}
