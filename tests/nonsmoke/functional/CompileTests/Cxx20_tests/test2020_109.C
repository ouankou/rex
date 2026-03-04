#include <compare>

struct Example {
  int x;
  int y;

  // Compare each member in declaration order and return on first mismatch.
  std::strong_ordering operator<=>(const Example &other) const {
    if (auto cmp = x <=> other.x; cmp != 0) {
      return cmp;
    }
    if (auto cmp = y <=> other.y; cmp != 0) {
      return cmp;
    }
    return std::strong_ordering::equal;
  }
};

// DQ (7/21/2020): Moved function calls into a function.
void foobar1() {
  Example lhs{1, 2};
  Example rhs{1, 3};
  [[maybe_unused]] std::strong_ordering cmp = lhs <=> rhs;
}
