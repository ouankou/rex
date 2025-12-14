template <typename T, int N> struct Container {
  T data[N];
};

int main() {
  Container<int, 10> c;
  (void)c;
  return 0;
}
