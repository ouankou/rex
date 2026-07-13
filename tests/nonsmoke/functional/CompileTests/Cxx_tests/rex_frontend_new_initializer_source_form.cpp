#include <new>

struct RexNewObject {
  RexNewObject() : value(0) {}
  explicit RexNewObject(int input) : value(input) {}

  int value;
};

void rex_new_initializer_source_forms(void *storage) {
  RexNewObject *rex_default = new RexNewObject;
  RexNewObject *rex_value = new RexNewObject();
  RexNewObject *rex_braced = new RexNewObject{};
  RexNewObject *rex_argument = new RexNewObject(1);
  RexNewObject *rex_placement = new (storage) RexNewObject(2);
  int *rex_scalar_default = new int;
  int *rex_scalar_value = new int();
  int *rex_scalar_braced = new int{};

  delete rex_default;
  delete rex_value;
  delete rex_braced;
  delete rex_argument;
  rex_placement->~RexNewObject();
  delete rex_scalar_default;
  delete rex_scalar_value;
  delete rex_scalar_braced;
}
