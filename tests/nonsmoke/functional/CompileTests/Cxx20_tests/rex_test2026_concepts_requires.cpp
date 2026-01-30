// Concept + requires regression: exercises declaration scopes and constraint
// satisfaction.

template <typename T>
concept HasValueType = requires { typename T::value_type; };

template <typename T>
concept HasMember = requires(T t) { t.member; };

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

  WithValue w{};
  WithoutValue u{};
  return constrained(w) + constrained(u) + w.member;
}
