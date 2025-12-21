#include "rex_test2025_issue160_system_header_instantiation.h"

template int rex_test2025_issue160::scale_and_shift<int>(int, int);

int main() {
  rex_test2025_issue160::Wrapper wrapper;
  wrapper.value = 41;

  int scaled = rex_test2025_issue160::scale_and_shift(2, 3);
  return (wrapper.bump() + scaled) > 0 ? 0 : 1;
}
