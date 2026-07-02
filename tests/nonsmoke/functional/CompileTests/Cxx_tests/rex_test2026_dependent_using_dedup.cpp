template <typename T> struct RexTest2026DependentUsingBase {
  void touch();
};

template <typename T>
struct RexTest2026DependentUsingDerived : RexTest2026DependentUsingBase<T> {
  using RexTest2026DependentUsingBase<T>::touch;

  void run() { this->touch(); }
};

template struct RexTest2026DependentUsingDerived<int>;
