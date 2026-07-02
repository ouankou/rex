#include <locale>
#include <string>

std::string
rex_test2026_function_definition_valgrind_defined_reads(std::string text) {
  const std::locale loc;
  const std::ctype<char> &facet = std::use_facet<std::ctype<char>>(loc);
  for (char &ch : text) {
    ch = facet.tolower(ch);
  }
  return text;
}
