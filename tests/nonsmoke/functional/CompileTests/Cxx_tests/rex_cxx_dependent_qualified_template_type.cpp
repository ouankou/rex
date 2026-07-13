namespace rex_cxx_dependent_qualified_template_type_first {
template <class T> struct A {
  int x;
};

template <int N> struct B {
  template <int M> struct C {};
};

int const D = 1;

void assign(int value) {
  // clang-format off
  A < B < 3 > :: C < D < 3 > > a;
  // clang-format on
  a.x = value;
}
} // namespace rex_cxx_dependent_qualified_template_type_first

namespace rex_cxx_dependent_qualified_template_type_second {
template <int N> struct A {
  template <class U> struct C {
    int x;
  };
};

int const B = 2;

template <int M> struct D {};

void assign(int value) {
  // clang-format off
  A < B < 3 > :: C < D < 3 > > c;
  // clang-format on
  c.x = value;
}
} // namespace rex_cxx_dependent_qualified_template_type_second
