namespace rex_test2026_template_scope {
template <typename T> struct Box {
  T value;
};

template <typename T> struct Holder {
  Box<T> box;
};
} // namespace rex_test2026_template_scope

using RexTest2026Holder = rex_test2026_template_scope::Holder<int>;

int rex_test2026_template_instantiation_reachable_scope() {
  RexTest2026Holder holder = {{7}};
  return holder.box.value;
}
