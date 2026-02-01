#include <type_traits>

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
int sfinae_pick(T) {
  return 1;
}

template <typename T>
concept Integral = std::is_integral_v<T>;

template <Integral T> int concept_pick(T) { return 2; }

int main() { return sfinae_pick(1) + concept_pick(2); }
