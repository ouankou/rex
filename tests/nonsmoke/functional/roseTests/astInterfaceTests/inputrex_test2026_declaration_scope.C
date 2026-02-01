template <template <typename> class TT, typename T> struct Wrapper {
  TT<T> value;
};

template <typename U> struct Inner {
  using type = U;
};

int main() {
  Wrapper<Inner, int> w;
  (void)w;
  return 0;
}
