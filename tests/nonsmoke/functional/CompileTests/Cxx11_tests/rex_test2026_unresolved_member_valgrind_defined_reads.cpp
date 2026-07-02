template <typename T>
int rex_test2026_unresolved_member_valgrind_defined_reads(T value) {
  return value.template member<int>();
}

struct RexTest2026UnresolvedMember {
  template <typename U> int member() const { return sizeof(U); }
};

int rex_test2026_unresolved_member_valgrind_defined_reads_driver() {
  RexTest2026UnresolvedMember value;
  return rex_test2026_unresolved_member_valgrind_defined_reads(value);
}
