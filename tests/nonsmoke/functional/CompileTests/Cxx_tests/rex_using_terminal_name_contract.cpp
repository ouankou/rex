namespace rex_using_terminal_source {
inline int value = 0;
}

using rex_using_terminal_source::value;

struct RexUsingBase {
  explicit RexUsingBase(int input) : value(input) {}
  int value;
};

struct RexUsingDerived : RexUsingBase {
  using RexUsingBase::RexUsingBase;
};

int main() {
  RexUsingDerived derived(7);
  return derived.value + value;
}
