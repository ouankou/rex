#include <vector>

int main() {
  std::vector<int> values;
  values.push_back(1);
  return values.size() == 1 ? 0 : 1;
}
