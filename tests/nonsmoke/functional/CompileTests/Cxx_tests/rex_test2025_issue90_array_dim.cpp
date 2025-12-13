// REX Issue #90: Array dimension expressions using non-type template parameters
// must be preserved (e.g., T data[N] must not become T data[]).

template <typename T, int N> struct Container {
  T data[N];

  void push(T val) { data[0] = val; }
};

int main() {
  Container<int, 10> c;
  c.push(5);
  return 0;
}
