template <typename T> struct Holder {
  int templ_field;
  template <typename U> static constexpr U templ_value = U{3};
  template <typename U = int> static constexpr U templ_default = U{4};
};

template <typename T, typename U> struct Pair {
  static constexpr int tag = 0;
};

template <typename T> struct Pair<T, int> {
  static constexpr int tag = 1;
};

template <typename T> constexpr int value_of = 1;

template <typename T = long> constexpr T default_value = T{5};

template <typename T> constexpr T explicit_identity(T value) { return value; }

template <> constexpr int value_of<int> = 2;

int use_field(const Holder<int> &h) { return h.templ_field; }

struct DerivedHolder : Holder<int> {};

int use_converted_field(DerivedHolder *derived) { return derived->templ_field; }

int main() {
  Holder<int> h{42};
  Holder<int> *hp = &h;
  return use_field(h) + Pair<char, int>::tag + value_of<int> + default_value<> +
         h.templ_value<short> + hp->templ_value<long> + h.templ_default<> +
         explicit_identity<int>(1);
}
