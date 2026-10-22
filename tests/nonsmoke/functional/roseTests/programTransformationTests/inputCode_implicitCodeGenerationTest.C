/*
 * This code tests a number of features of the implicit call generator.
 * It contains a tricky edge case involving nested constructors.
 */
#define CLS(A) \
struct A \
   { \
\
     bool b; \
\
     A() : b(false) \
	{ \
	} \
\
     A(bool b) : b(b) \
	{ \
	} \
\
     ~A() \
	{ \
	} \
\
     operator bool() \
	{ \
	  return true; \
	} \
\
   }

CLS(A);
CLS(B);
CLS(C);
CLS(D);
CLS(E);

struct RexBase {};

struct RexIcgBindingPair {
  int first;
  int second;
};

int rex_icg_binding_initializer(int limit) {
  for (auto [first, second] = RexIcgBindingPair{0, 1}; first < limit; ++first)
    if (second > limit)
      break;
  for (int current = 0; current < limit; ++current)
    if (current == 1 && limit > 1)
      return current;
  return limit;
}

struct RexDelegatingBase : RexBase {
  explicit RexDelegatingBase(int) {}
  RexDelegatingBase() : RexDelegatingBase(1) {}
};

using RexBaseAlias = RexBase;
struct RexOtherBase {};

struct RexShadowedBaseAlias : RexBaseAlias {
  using RexBaseAlias = RexOtherBase;
  RexShadowedBaseAlias() {}
};

#define REX_ICG_MACRO_BASE ::RexBase
struct RexMacroBase : REX_ICG_MACRO_BASE {
  RexMacroBase() {}
};

struct RexDerived : RexBase {
  RexDerived(RexDerived *const &, int) {}
  RexDerived(const RexDerived &, int) {}
  RexDerived(const RexDerived &, long = 0) {}
  ~RexDerived() {}
};

struct RexDeclarationDefaults : RexBase {
  using Self = RexDeclarationDefaults;
  RexDeclarationDefaults(const Self &, long = 0);
  ~RexDeclarationDefaults() {}
};

RexDeclarationDefaults::RexDeclarationDefaults(const Self &, long) {}

int rex_increment_continue(bool take_first) {
  int steps = 0;
  for (int i = 0; i != 3; take_first ? (++i, ++steps) : (++steps, ++i)) {
    switch (i) {
    case 0:
      continue;
    }
    for (int j = 0; j != 1; ++j) {
      continue;
    }
    int values[1] = {0};
    for (int value : values) {
      (void)value;
      continue;
    }
  }
  for (int i = 0; i != 3; take_first ? (++i, ++steps) : (++steps, ++i))
    continue;
  return steps;
}

int main() {
  RexDelegatingBase delegating_base;
  RexShadowedBaseAlias shadowed_base;
  RexMacroBase macro_base;
  if (rex_increment_continue(true) != 6 || rex_increment_continue(false) != 6) {
    return 1;
  }
  int a;
  if ((A() && B(C() && D())) || E()) {
    return 42;
  }
}
