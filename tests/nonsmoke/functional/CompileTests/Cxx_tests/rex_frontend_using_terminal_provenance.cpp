struct RexUsingOverloads {
  void rex_call(int);
  void rex_call(double);
  int operator()(int) const;
};

struct RexUsingDerived : RexUsingOverloads {
  using RexUsingOverloads::rex_call;
  using RexUsingOverloads::operator();
};

template <class T> struct RexUsingDependent : T {
  using T::rex_call;
  using T::T;
};

template <class T> struct RexUsingTemplateBase {
  RexUsingTemplateBase(T);
  void rex_call(T);
};

struct RexUsingConcrete : RexUsingTemplateBase<long> {
  using RexUsingTemplateBase<long>::RexUsingTemplateBase;
  using RexUsingTemplateBase<long>::rex_call;
};
