namespace rex_test2026_namespace_template_source_order {
template <typename T> inline void swap(T &lhs, T &rhs) {
  T tmp = lhs;
  lhs = rhs;
  rhs = tmp;
}
} // namespace rex_test2026_namespace_template_source_order

namespace rex_test2026_namespace_template_source_order {
template <typename T> struct DequeData {
  T value;

  void swap_data(DequeData &other) {
    rex_test2026_namespace_template_source_order::swap(*this, other);
  }
};
} // namespace rex_test2026_namespace_template_source_order

namespace rex_test2026_namespace_template_source_order {
template <typename T> struct VectorData {
  T value;

  void swap_data(VectorData &other) {
    rex_test2026_namespace_template_source_order::swap(*this, other);
  }
};
} // namespace rex_test2026_namespace_template_source_order

int rex_test2026_namespace_template_source_order_use() {
  rex_test2026_namespace_template_source_order::DequeData<int> lhs = {1};
  rex_test2026_namespace_template_source_order::DequeData<int> rhs = {2};
  lhs.swap_data(rhs);
  return lhs.value + rhs.value;
}
