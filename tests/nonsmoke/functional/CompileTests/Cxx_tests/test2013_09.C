template <typename T, int N> struct Buffer {
  T data[N];
  constexpr int size() const { return N; }
};

int main() {
  Buffer<int, 3> buf{{1, 2, 3}};
  int sum = 0;
  for (int v : buf.data) {
    sum += v;
  }
  return sum - (buf.size() + 3);
}
