#include "frontend/Flang/FlangDirectiveProvenance.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  const std::string mode = argv[1];
  if (mode == "missing") {
    Rose::builder::detail::RejectMissingFlangDirectiveCharacterProvenance(
        7, "/tmp/rex_flang_expected.f90");
  }
  if (mode == "cross-file") {
    Rose::builder::detail::RejectCrossFileFlangDirectiveCharacterProvenance(
        11, "/tmp/rex_flang_expected.f90", "/tmp/rex_flang_actual.f90");
  }

  std::cerr << "unknown mode: " << mode << '\n';
  return 2;
}
