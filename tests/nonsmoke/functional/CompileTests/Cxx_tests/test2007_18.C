
template<typename T> class A;
template<typename T>
class B {
  template<typename T2> friend void f(A<T2>);
};
extern template class B<char>;
template<> void f(A<char>);
