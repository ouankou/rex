#include <regex>
#include <string>

int main() {
  std::regex re("[a-z]+");
  std::string s = "abc";
  return std::regex_match(s, re) ? 0 : 1;
}
