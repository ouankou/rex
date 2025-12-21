// Reuse explicit template function instantiations across multiple call sites.
#include <tuple>

template <typename T> T hypot_like(T x, T y) { return x * x + y * y; }

static float hypot_like_wrapper(float x) { return hypot_like<float>(x, x); }

static float hypot_like_wrapper_again(float x) {
  return hypot_like<float>(x, x);
}

static int tuple_get_sum() {
  std::tuple<int, double> values{1, 2.5};
  int first = std::get<0>(values);
  double second = std::get<1>(values);
  return first + static_cast<int>(second);
}

int main() {
  const bool ok = hypot_like_wrapper(1.0f) < hypot_like_wrapper_again(2.0f) &&
                  tuple_get_sum() > 0;
  return ok ? 0 : 1;
}
