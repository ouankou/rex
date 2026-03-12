// Templates are valid at namespace or class scope.
template <typename T> struct Container {
  using value_type = T;
};

template <typename ContainerType> void g(const ContainerType &c) {
  typename ContainerType::value_type n{};
  (void)c;
  (void)n;
}

void foobar() {
  // A member typedef introduced by a class template can be used generically.
  Container<int> c;
  g(c);
}
