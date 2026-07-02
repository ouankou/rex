namespace rex_test2026_template_qualifier_friend {

  template <typename T> struct Friend;

  template <typename T> struct Outer {
    template <typename U> struct Inner {
      friend class Friend<T>;

      Inner();

      template <typename V> V convert(V value);
    };
  };

  template <typename T> struct Friend {
    typedef T value_type;
  };

  template <typename T> template <typename U> Outer<T>::Inner<U>::Inner() {}

  template <typename T>
  template <typename U>
  template <typename V>
  V Outer<T>::Inner<U>::convert(V value) {
    return value;
  }

  int rex_test2026_template_qualifier_friend_valgrind_defined_reads() {
    Outer<int>::Inner<long> value;
    return value.convert(42);
  }

} // namespace rex_test2026_template_qualifier_friend
