template <typename Left, typename Right> struct RexSinglePassBase {};

template <typename Left, typename Right>
struct RexSinglePassDerived : RexSinglePassBase<Left, Right> {
  int value = sizeof(Left) + sizeof(Right);

  int add(int lhs, int rhs) { return lhs + rhs; }
};

int rex_unparser_single_pass_formatting(int value, int limit) {
  while (value < limit) {
    ++value;
  }

  switch (value) {
  case 0:
    return - -5;
  default:
    return value;
  }
}
