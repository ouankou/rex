template <typename T> struct RexTest2026DeclarationScope {
  using type = typename T::type;

  template <typename U = type> struct Inner {
    U value;
  };
};

struct RexTest2026DeclarationScopeArg {
  using type = int;
};

int rex_test2026_declaration_scope_structural_successor() {
  RexTest2026DeclarationScope<RexTest2026DeclarationScopeArg>::Inner<> inner = {
      3};
  return inner.value;
}
