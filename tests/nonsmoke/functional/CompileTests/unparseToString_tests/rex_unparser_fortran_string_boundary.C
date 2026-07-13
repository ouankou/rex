#include "FortranLineWrapSupport.h"

#include <algorithm>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--malformed") {
    Rose::FortranLineWrapSupport::stringLiteralLexicalBoundaries("'a'''b'",
                                                                 '\'');
    return 1;
  }
  if (argc != 1) {
    return 2;
  }

  const std::vector<size_t> boundaries =
      Rose::FortranLineWrapSupport::stringLiteralLexicalBoundaries("'a''''b'",
                                                                   '\'');
  const std::vector<size_t> expected{0, 1, 2, 4, 6, 7, 8};
  if (boundaries != expected) {
    return 3;
  }
  if (std::binary_search(boundaries.begin(), boundaries.end(), 3) ||
      std::binary_search(boundaries.begin(), boundaries.end(), 5)) {
    return 4;
  }
  return 0;
}
