// A tuple-like structured binding introduces one holding variable per element,
// copy-initialized from its get<i> call.  [dcl.struct.bind]p7 (CWG3135) gives
// that variable type Ti when the call is a prvalue, and otherwise an lvalue or
// rvalue reference to Ti matching the call's value category.  The
// initialization may convert the call's result to Ti.
#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>

struct RexValueTuple {
  std::string text;
  int number;
  template <std::size_t Index> auto get() const {
    if constexpr (Index == 0)
      return text;
    else
      return number;
  }
};

struct RexBase {
  int value;
};
struct RexDerived : RexBase {};

struct RexConvertingTuple {
  RexDerived derived;
  template <std::size_t Index> decltype(auto) get() {
    if constexpr (Index == 0)
      return int{6};
    else
      return (derived);
  }
};

namespace std {
template <>
struct tuple_size<RexValueTuple> : integral_constant<std::size_t, 2> {};
template <> struct tuple_element<0, RexValueTuple> {
  using type = std::string;
};
template <> struct tuple_element<1, RexValueTuple> {
  using type = int;
};
template <>
struct tuple_size<RexConvertingTuple> : integral_constant<std::size_t, 2> {};
template <> struct tuple_element<0, RexConvertingTuple> {
  using type = long;
};
template <> struct tuple_element<1, RexConvertingTuple> {
  using type = RexBase;
};
} // namespace std

int main() {
  std::tuple<std::string, int> tuple{"rex", 1};
  // get<i>(tuple&) is an lvalue: each holding variable is a Ti&.
  auto &[lvalue_text, lvalue_number] = tuple;
  lvalue_number = 2;
  const bool lvalue_bound = std::get<1>(tuple) == 2 && lvalue_text == "rex";
  // The hidden variable is not a reference, so get<i> receives an xvalue and
  // each holding variable is a Ti&&.
  auto [xvalue_text, xvalue_number] = std::tuple<std::string, int>{"ast", 4};
  // A by-value get<i> is a prvalue: each holding variable is a Ti object.
  auto [prvalue_text, prvalue_number] = RexValueTuple{"llvm", 3};
  // The int prvalue converts to a long holding object, and the RexDerived
  // lvalue binds a RexBase& holding reference to its base subobject.
  RexConvertingTuple converting;
  converting.derived.value = 5;
  auto &[converted_number, base] = converting;
  return lvalue_bound && xvalue_text == "ast" && xvalue_number == 4 &&
                 prvalue_text == "llvm" && prvalue_number == 3 &&
                 converted_number == 6 && base.value == 5
             ? 0
             : 1;
}
