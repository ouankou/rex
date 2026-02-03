#include <regex>
#include <string>

int main() {
  std::regex pattern("(a+)(b*)");
  std::string value = "aaab";
  return std::regex_match(value, pattern) ? 0 : 1;
}
