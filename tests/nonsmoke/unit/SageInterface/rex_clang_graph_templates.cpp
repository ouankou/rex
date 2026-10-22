#include <vector>

template <class T>
concept RexGraphIntegral = __is_integral(T);
template <RexGraphIntegral T> struct RexGraphBox {
  T value;
};
template <class T> struct RexGraphContainer {
  using type = T;
};
template <class T> struct RexGraphContainer<T *> {
  using type = T;
};
template <class T> using RexGraphAlias = typename RexGraphContainer<T>::type;
template <class T> constexpr T rex_graph_initial = T{2};
template <class T> T rex_graph_twice(T value) { return value * 2; }
template int rex_graph_twice(int);

const char *rex_graph_quoted_text = "graph \"quoted\" \\ path";
int rex_graph_template_value() {
  std::vector<int> values{rex_graph_initial<int>, 3};
  auto twice = [](int n) { return rex_graph_twice(n); };
  RexGraphBox<RexGraphAlias<int *>> box{values[0]};
  return twice(box.value) + values[1];
}
