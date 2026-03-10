// Constexpr lambda with literal class values

auto ID = [](auto a) { return a; };
static_assert(ID(3) == 3);

struct Literal {
  constexpr explicit Literal(int value) : n(value) {}
  int n;
};

static_assert(ID(Literal{3}).n == 3);
