template <typename T> struct Holder {
  explicit Holder(T v) : value(v) {}
  T value;
};

int main() {
  Holder<int> h(13);
  return h.value - 13;
}
