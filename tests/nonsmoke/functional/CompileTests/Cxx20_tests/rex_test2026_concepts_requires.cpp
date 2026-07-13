// Concept + requires regression: exercises declaration scopes and constraint
// satisfaction.

template <typename T>
concept HasValueType = requires { typename T::value_type; };

template <typename T>
concept HasMember = requires(T t) { t.member; };

template <typename>
concept AnyType = true;

template <typename T>
concept FullRequirementSurface = requires(T t) {
  t.member;
  typename T::value_type;
  { t.member } noexcept -> AnyType;
  requires HasValueType<T>;
};

template <typename T, typename U>
concept SameType = __is_same(T, U);

SameType<int> auto constrained_auto_value = 7;

SameType<int> auto constrained_auto_result() { return 9; }

template <HasValueType T, SameType<int> U, HasValueType auto Value>
int constrained_parameters(T, U) {
  return Value.member;
}

struct WithValue {
  using value_type = int;
  int member = 1;
};

struct WithoutValue {};

template <typename T>
  requires HasValueType<T>
int constrained(T) {
  return 1;
}

template <typename T> int constrained(T) { return 2; }

int main() {
  static_assert(HasValueType<WithValue>);
  static_assert(!HasValueType<WithoutValue>);
  static_assert(HasMember<WithValue>);
  static_assert(!HasMember<WithoutValue>);
  static_assert(FullRequirementSurface<WithValue>);

  WithValue w{};
  WithoutValue u{};
  return constrained(w) + constrained(u) + w.member + constrained_auto_value +
         constrained_auto_result();
}
