template <typename T> struct Traits {
  using type = T;
};

template <> struct Traits<int> {
  using type = double;
};

Traits<int> t;
