template <typename T> struct RexTest2026MemberExprValue {
  T value;

  T get() const { return static_cast<T>(this->value); }
};

int rex_test2026_member_expr_valgrind_defined_reads() {
  RexTest2026MemberExprValue<int> item{7};
  return item.get();
}
