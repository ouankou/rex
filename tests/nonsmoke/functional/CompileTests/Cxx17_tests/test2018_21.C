// Constexpr lambda composition

constexpr auto monoid = [](int v) constexpr {
  return [=]() constexpr { return v; };
};

constexpr auto add = [](auto m1) constexpr {
  return [=](auto m2) constexpr { return m1() + m2(); };
};

constexpr auto zero = monoid(0);
constexpr auto one = monoid(1);
constexpr auto two = add(one)(one);

static_assert(add(one)(zero) == one());
static_assert(two == 2);
static_assert(add(one)(one) == two);
