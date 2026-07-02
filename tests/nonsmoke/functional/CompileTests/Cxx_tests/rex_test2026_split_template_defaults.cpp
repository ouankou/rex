namespace RexTest2026SplitTemplateDefaults {

enum { A = 2 };

template <int m> struct C {
  typedef int B;
};

template <int n, C<4>::B m = 6> class X;

template <int n = A<3, C<4>::B m> class X {
public:
  enum { value = n + m };
};

X<> x;

} // namespace RexTest2026SplitTemplateDefaults

int rex_test2026_split_template_defaults() {
  return RexTest2026SplitTemplateDefaults::x.value;
}
