template <class T = void> struct A {
  using type = T;
};

A<> a;

int main() {
  (void)a;
  return 0;
}
