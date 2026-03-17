namespace A {
  inline namespace B {
    template <class T> int c(T const &t) {
      d(t);
      return 0;
    }

    // This line will not compile with GNU version 5.1 (at least).
    template <class T> int e(int = c([] {}));
  } // namespace B

  template <class T> int d(T const &);
  template <class T> int d(T const &) { return 0; }
} // namespace A

void foo() { A::B::e<long>(); }
