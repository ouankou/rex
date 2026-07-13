#include "rex_unparse_implicit_control_flow_header_plan.hpp"

int main() {
  int value = 0;
  return rex_unparse_implicit_control_flow_header_plan(&value) == &value ? 0
                                                                         : 1;
}
