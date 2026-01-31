template <typename T, int N> struct Array {
  T data[N];
};

int main() {
  Array<int, 4> a;
  return a.data[0];
}
