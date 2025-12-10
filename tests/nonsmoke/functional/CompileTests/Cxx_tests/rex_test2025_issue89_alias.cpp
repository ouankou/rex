// REX Issue #89: Type alias template arguments must be preserved.
// Expected: using IntContainer = Container<int>

template <typename T> struct Container {
  using value_type = T;
  typedef T type;
};

using IntContainer = Container<int>;

int main() {
  IntContainer c;
  IntContainer::value_type x = 5;
  return x;
}
