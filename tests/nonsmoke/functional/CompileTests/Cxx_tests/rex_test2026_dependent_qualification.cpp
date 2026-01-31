namespace ns {

template <typename T> struct Outer {
  template <typename U> struct Inner {
    using type = U;
  };
  template <typename U> using Alias = Inner<U>;
};

} // namespace ns

template <template <typename> class TT> struct Wrap {
  using type = typename TT<int>::type;
};

template <typename T> struct Identity {
  using type = T;
};

template <typename T>
typename ns::Outer<T>::template Inner<int>::type make_inner();

template <typename T>
typename ns::Outer<T>::template Alias<double>::type make_alias();

int main() {
  using Wrapped = Wrap<Identity>::type;
  return static_cast<int>(sizeof(make_inner<int>()) +
                          sizeof(make_alias<int>()) + sizeof(Wrapped));
}
