template <typename T> struct Holder {
  int templ_field;
};

template <typename T, typename U> struct Pair {
  static constexpr int tag = 0;
};

template <typename T> struct Pair<T, int> {
  static constexpr int tag = 1;
};

template <typename T> constexpr int value_of = 1;

template <> constexpr int value_of<int> = 2;

int use_field(const Holder<int> &h) { return h.templ_field; }

int main() {
  Holder<int> h{42};
  return use_field(h) + Pair<char, int>::tag + value_of<int>;
}
