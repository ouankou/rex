// Issue 368: lambda-instantiated template helpers must be defined in-TU.
template <typename _Tp> struct remove_reference {
  typedef _Tp type;
};

template <typename _Tp>
constexpr _Tp &&forward(typename remove_reference<_Tp>::type &__t) {
  return static_cast<_Tp &&>(__t);
}

template <typename _Tp>
constexpr typename remove_reference<_Tp>::type &&move(_Tp &&__t) {
  return static_cast<typename remove_reference<_Tp>::type &&>(__t);
}

template <typename func_type> struct Class_1 {};

template <typename func_type>
typename ::Class_1<typename remove_reference<func_type>::type>
method_2(func_type &&func) {
  (void)move(func);
  (void)forward<func_type>(func);
  return ::Class_1<typename remove_reference<func_type>::type>();
}

void method_1() {
  auto local_1 = ::method_2<>([&](int ns) {});
}
