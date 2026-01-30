#include <string>
#include <vector>

int main() {
  for (int i = 0; i < 3; ++i) {
    std::string s = "rex";
    std::vector<int> values;
    values.push_back(i);
    if (!values.empty()) {
      s += std::to_string(values.front());
    }
  }
  return 0;
}
