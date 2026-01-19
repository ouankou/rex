// #include <string>
// #include <ios>

#include <type_traits>

// type alias used to simplify the syntax of std::enable_if
template <typename T> using Invoke = typename T::type;
template <typename Condition>
using EnableIf = Invoke<std::enable_if<Condition::value>>;
template <typename T, typename = EnableIf<std::is_polymorphic<T>>>
int fpoly_only(T t) {
  return 1;
}

struct S {
  virtual ~S() {}
};
