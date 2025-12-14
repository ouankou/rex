// Regression: Clang represents variadic template arguments as
// TemplateArgument::Pack. The CFE must flatten packs so template argument lists
// are spelled correctly (no nested angle brackets) and compile.

#include <tuple>

template <template <typename...> class C, typename... Args> struct UsePack {
  using type = C<Args...>;
};

using T = UsePack<std::tuple, int, double>::type;

int main() {
  T t{};
  (void)t;
  return 0;
}
