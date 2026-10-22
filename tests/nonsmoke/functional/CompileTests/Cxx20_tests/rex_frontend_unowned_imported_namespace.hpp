namespace rex_unowned_imported_namespace {
struct marker {
  int value;
  int operator+(int offset) const { return value + offset; }
};
inline int add_marker(const marker &input) { return input + 1; }
} // namespace rex_unowned_imported_namespace
