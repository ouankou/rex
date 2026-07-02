template <class T> class RexTest2026AliasBase {
public:
  explicit RexTest2026AliasBase(bool rex_test2026_ctor_parameter)
      : flag(rex_test2026_ctor_parameter) {}

private:
  bool flag;
};

template <class T>
class RexTest2026AliasDerived : public RexTest2026AliasBase<T> {
public:
  RexTest2026AliasDerived() : RexTest2026AliasBase<T>(false) {}
};

class RexTest2026AliasFinal : public RexTest2026AliasDerived<int> {
public:
  RexTest2026AliasFinal() {}
};

int rex_test2026_base_alias_skips_ctor_parameter() {
  RexTest2026AliasFinal value;
  (void)value;
  return 0;
}
