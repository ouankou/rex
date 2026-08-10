#include <utility>

template <class Pair> int rex_sum_pair(Pair pair) {
  auto [first, second] = pair;
  return first + second;
}

int rex_frontend_structured_binding_template_body() {
  return rex_sum_pair(std::pair<int, int>{20, 22});
}
