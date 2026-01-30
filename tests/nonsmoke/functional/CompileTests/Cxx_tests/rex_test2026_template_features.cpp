#include <cstddef>
#include <type_traits>

template <typename T, typename U> struct Pair {
  static constexpr int tag = 0;
  int field;
};

template <typename T> struct Pair<T, int> {
  static constexpr int tag = 1;
  int field;
};

template <typename T> constexpr int value_of = 1;

template <> constexpr int value_of<int> = 2;

template <typename T>
auto sfinae_probe(int) -> decltype((void)T::member, int{}) {
  return 1;
}

template <typename T> int sfinae_probe(...) { return 0; }

template <typename T>
std::enable_if_t<std::is_integral_v<T>, int> enable_if_probe(T) {
  return 1;
}

template <typename T>
std::enable_if_t<!std::is_integral_v<T>, int> enable_if_probe(T) {
  return 0;
}

struct HasMember {
  static constexpr int member = 1;
};

struct NoMember {};

template <typename T> struct Box {
  T value;
  explicit Box(T v) : value(v) {}
};

Box(int) -> Box<int>;

int main() {
  Pair<char, int> partial{3};
  Box deduced(7);
  return partial.field + Pair<char, int>::tag + value_of<int> +
         sfinae_probe<HasMember>(0) + sfinae_probe<NoMember>(0) +
         enable_if_probe(1) + enable_if_probe(1.5) + deduced.value;
}
