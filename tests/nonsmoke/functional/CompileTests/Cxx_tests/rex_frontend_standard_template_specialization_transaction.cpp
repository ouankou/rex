#include <iostream>
#include <string>

using RexStreamBuffer = std::basic_ostream<char>::__streambuf_type;

void rex_standard_template_specialization_transaction(std::istream &input) {
  RexStreamBuffer *buffer = input.rdbuf();
  std::string line;
  std::getline(input, line, '\n');
  (void)buffer;
}
