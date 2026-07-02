template <typename T> struct RexTest2026InlineConstructorBase {
  T *ptr;
  unsigned offset;

  RexTest2026InlineConstructorBase(T *value, unsigned index)
      : ptr(value), offset(index) {}
};

template <typename T>
struct RexTest2026InlineConstructorIterator
    : RexTest2026InlineConstructorBase<T> {
  RexTest2026InlineConstructorIterator(T *value)
      : RexTest2026InlineConstructorBase<T>(value, 0) {}
};

int rex_test2026_inline_constructor_preserved() {
  int value = 3;
  RexTest2026InlineConstructorIterator<int> iter(&value);
  return iter.offset;
}
