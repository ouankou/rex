#include <algorithm>
#include <numeric>
#include <vector>

template <typename T> using Vec = std::vector<T>;

int main() {
  Vec<int> values{4, 1, 3, 2};
  values.emplace_back(5);
  std::sort(values.begin(), values.end());
  int sum = std::accumulate(values.begin(), values.end(), 0);
  return sum;
}
