template <typename T> struct Box {
  T value;
};

#if __cplusplus >= 202002L
template <typename T> explicit Box(T) -> Box<T>;

template <typename T> struct RexConditionalExplicit {
  explicit(sizeof(T) > 1) RexConditionalExplicit(T) {}
  explicit(sizeof(T) > 1) operator T() const { return T{}; }
};

template <typename T> void rex_implicit_accept(T);

template <typename From, typename To>
concept RexImplicitlyConvertible =
    requires(From from) { rex_implicit_accept<To>(from); };

static_assert(RexImplicitlyConvertible<char, RexConditionalExplicit<char>>);
static_assert(!RexImplicitlyConvertible<int, RexConditionalExplicit<int>>);
static_assert(RexImplicitlyConvertible<RexConditionalExplicit<char>, char>);
static_assert(!RexImplicitlyConvertible<RexConditionalExplicit<int>, int>);
#else
template <typename T> Box(T) -> Box<T>;
#endif

auto rex_deduced_value() {
  using Local = int;
  return Local{42};
}

int main() {
  Box b{rex_deduced_value()};
  return b.value;
}
