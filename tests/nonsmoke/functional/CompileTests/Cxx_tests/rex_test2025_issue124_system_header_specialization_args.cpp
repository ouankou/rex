#include <vector>

template <typename T> struct Use {
  using type = T;
};

using X = Use<std::vector<int>>;

int main() {
  X::type v;
  return (int)v.size();
}
