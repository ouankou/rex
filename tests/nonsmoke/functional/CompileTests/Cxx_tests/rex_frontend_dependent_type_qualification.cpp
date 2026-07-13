template <typename T> struct rex_wrapper {
  using difference_type = long;

  explicit rex_wrapper(T value) : value(value) {}

  T value;
};

template <typename T>
rex_wrapper<T> rex_add(typename rex_wrapper<T>::difference_type offset,
                       const rex_wrapper<T> &value) {
  return rex_wrapper<T>(value.value + static_cast<T>(offset));
}

template <bool Condition, typename TrueType, typename FalseType>
struct rex_conditional_type {
  using type = TrueType;
};

template <typename TrueType, typename FalseType>
struct rex_conditional_type<false, TrueType, FalseType> {
  using type = FalseType;
};

template <typename T>
struct rex_traits
    : rex_conditional_type<true, rex_wrapper<T>, rex_wrapper<int>>::type {
  explicit rex_traits(T value)
      : rex_conditional_type<true, rex_wrapper<T>, rex_wrapper<int>>::type(
            value) {}
};

int main() {
  rex_wrapper<int> value(3);
  return rex_add(2, value).value + sizeof(rex_traits<int>);
}
