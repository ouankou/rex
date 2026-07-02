template <typename T, typename Iter = T *> struct RexTest2026PriorDefault;

template <typename T, typename Iter> struct RexTest2026PriorDefault {
  Iter begin;
  T value;
};

int rex_test2026_template_definition_prior_default() {
  int value = 1;
  RexTest2026PriorDefault<int> item = {&value, value};
  return item.value + *item.begin;
}
