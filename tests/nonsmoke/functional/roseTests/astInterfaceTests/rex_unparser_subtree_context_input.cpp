namespace rex_subtree_context {

struct value {
  value() = default;
};

} // namespace rex_subtree_context

void rex_subtree_sink(int);

void build_value() {
  rex_subtree_context::value *result = new rex_subtree_context::value;
  rex_subtree_sink(0);
  for (int index = 0; index < 1; ++index) {
#pragma rex_subtree_marker
    result = result;
  }
  delete result;
}
