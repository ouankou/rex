template <class T> struct A {
  template <class U> struct rebind {
    using other = A<U>;
  };
};

template <class T> struct Use {
  using X = typename T::template rebind<int>::other;
  X value;
};

int main() {
  Use<A<double>> u{};
  (void)u;
}
