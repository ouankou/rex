#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

int main() {
  std::vector<int> values = {4, 1, 3, 2};
  std::string tag = "rex";

  std::sort(values.begin(), values.end());
  int sum = std::accumulate(values.begin(), values.end(), 0);

  return sum + static_cast<int>(tag.size());
}
