#include <type_traits>

template <typename T>
concept Integral = std::is_integral_v<T>;

template <Integral T> int pick(T) { return 1; }

template <typename T>
  requires(!Integral<T>)
int pick(T) {
  return 2;
}

template <typename T>
concept HasValueType = requires { typename T::value_type; };

struct WithValueType {
  using value_type = int;
};

struct WithoutValueType {};

template <typename T>
  requires HasValueType<T>
int has_value_type(T) {
  return 3;
}

int main() {
  return pick(1) + pick(1.5) + has_value_type(WithValueType{}) +
         (HasValueType<WithoutValueType> ? 1 : 0);
}
