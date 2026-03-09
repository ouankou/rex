// C++17-valid defaulted special members and exception specifications.

#include <type_traits>
#include <utility>

struct S {
  S() = default;
  explicit constexpr S(int value) noexcept : i(value) {}
  S(const S &) = default;
  S &operator=(const S &) = default;
  ~S() noexcept = default;

  constexpr int value() const noexcept { return i; }

private:
  int i = 0;
};

constexpr int construct_value() {
  S s(17);
  return s.value();
}

static_assert(std::is_same_v<
              decltype(std::declval<S &>() = std::declval<const S &>()), S &>);
static_assert(std::is_nothrow_destructible_v<S>);
static_assert(construct_value() == 17);

int use(S lhs, const S &rhs) {
  lhs = rhs;
  return lhs.value();
}
