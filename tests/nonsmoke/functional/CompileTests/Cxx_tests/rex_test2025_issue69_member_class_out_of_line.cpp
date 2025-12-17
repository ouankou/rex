namespace rex_issue69 {

template <class T> struct Outer {
  class Inner;
  typedef Inner InnerType;
};

template <class T> struct A {
  template <class U> struct B {
    class C;
  };
};

} // namespace rex_issue69

namespace rex_issue69 {

template <class T> class Outer<T>::Inner {
public:
  typedef T value_type;
};

template <class T> template <class U> class A<T>::B<U>::C {
public:
  typedef T outer_type;
  typedef U inner_type;
};

} // namespace rex_issue69

int main() {
  rex_issue69::Outer<int>::InnerType *p = 0;
  (void)p;

  rex_issue69::A<int>::B<double>::C *q = 0;
  (void)q;

  return 0;
}
