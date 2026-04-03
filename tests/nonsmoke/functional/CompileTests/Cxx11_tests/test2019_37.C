namespace A {
  template <class T> int c(T const &t) { return 0; }

  // This line did not compile with older GNU versions.
  template <class T> int e(int = c([] {})) { return 0; }
} // namespace A

void foo() { A::e<long>(); }
