int rex_condition_source();

struct RexCondition {
  explicit operator bool() const;
  operator int() const;
};

RexCondition rex_class_condition_source();

int rex_condition_driver() {
  if (int rex_condition_if = rex_condition_source()) {
  }
  while (int rex_condition_while = rex_condition_source()) {
    break;
  }
  switch (int rex_condition_switch = rex_condition_source()) {
  default:
    break;
  }
  for (; int rex_condition_for = rex_condition_source();) {
    break;
  }
  if (RexCondition rex_condition_class_if = rex_class_condition_source()) {
  }
  while (RexCondition rex_condition_class_while =
             rex_class_condition_source()) {
    break;
  }
  switch (RexCondition rex_condition_class_switch =
              rex_class_condition_source()) {
  default:
    break;
  }
  for (; RexCondition rex_condition_class_for = rex_class_condition_source();) {
    break;
  }
  try {
    throw 1;
  } catch (int rex_condition_catch) {
    (void)rex_condition_catch;
  }
  return 0;
}
