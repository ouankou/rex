using RexPointer = int *;

int rex_scalar_value_init() { return int(); }

RexPointer rex_pointer_value_init() { return RexPointer(); }

template <class T> T rex_dependent_value_init() { return T(); }

int rex_instantiate_dependent_value_init() {
  return rex_dependent_value_init<int>();
}
