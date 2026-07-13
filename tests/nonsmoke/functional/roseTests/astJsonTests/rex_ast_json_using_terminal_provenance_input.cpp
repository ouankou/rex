struct RexJsonUsingOverloads {
  void rex_call(int);
  void rex_call(double);
  int operator()(int) const;
};

struct RexJsonUsingDerived : RexJsonUsingOverloads {
  using RexJsonUsingOverloads::rex_call;
  using RexJsonUsingOverloads::operator ();
};

template <class T> struct RexJsonUsingDependent : T {
  using T::rex_call;
  using T::T;
};

template <class T> struct RexJsonUsingTemplateBase {
  RexJsonUsingTemplateBase(T);
  void rex_call(T);
};

struct RexJsonUsingConcrete : RexJsonUsingTemplateBase<long> {
  using RexJsonUsingTemplateBase<long>::RexJsonUsingTemplateBase;
  using RexJsonUsingTemplateBase<long>::rex_call;
};
