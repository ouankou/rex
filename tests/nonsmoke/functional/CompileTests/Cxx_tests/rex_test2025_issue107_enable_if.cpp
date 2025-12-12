template <bool B, class T = void> struct enable_if {};

template <class T> struct enable_if<true, T> {
  typedef T type;
};

template <typename T> class A {
  template <typename U> typename enable_if<true, U>::type foo();
};

void bar() { A<int> a; }
