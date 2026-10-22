// Regression: Clang represents variadic template arguments as
// TemplateArgument::Pack. The CFE must flatten packs so template argument lists
// are spelled correctly (no nested angle brackets) and compile.

#include <tuple>

template <template <typename...> class C, typename... Args> struct UsePack {
  using type = C<Args...>;
};

using T = UsePack<std::tuple, int, double>::type;

template <int... Values> struct RexValues {};
template <int... Outer> struct RexOuter {
  template <int... Inner> using Result = RexValues<(Outer + Inner)...>;
};
using RexResult = RexOuter<1, 2>::Result<3, 4>;
RexResult rex_result;
using RexEmptyResult = RexOuter<>::Result<>;
RexEmptyResult rex_empty_result;

int main() {
  T t{};
  (void)t;
  return 0;
}
