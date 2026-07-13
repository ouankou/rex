#include "rose.h"

#include <string>

std::string unparse_asm_clobber_name(const std::string &register_name);

int main() {
  (void)unparse_asm_clobber_name("invalid\"clobber");
  return 0;
}
