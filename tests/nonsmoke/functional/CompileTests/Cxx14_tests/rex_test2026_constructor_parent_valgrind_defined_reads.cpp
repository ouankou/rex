#include <memory>

struct RexTest2026ConstructorParent {
  std::unique_ptr<int> value;

  RexTest2026ConstructorParent() : value(new int(5)) {}
};

int rex_test2026_constructor_parent_valgrind_defined_reads() {
  RexTest2026ConstructorParent item;
  return *item.value;
}
