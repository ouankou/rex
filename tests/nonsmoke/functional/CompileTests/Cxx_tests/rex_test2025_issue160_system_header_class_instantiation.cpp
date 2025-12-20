#include "rex_test2025_issue160_system_header_class_instantiation.h"

template class rex_test2025_issue160::Box<int>;

int main() {
  rex_test2025_issue160::Box<int> box;
  box.value = 7;
  return box.get() == 7 ? 0 : 1;
}
