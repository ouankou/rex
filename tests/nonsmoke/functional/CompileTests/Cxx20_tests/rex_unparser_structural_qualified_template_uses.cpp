namespace rex_outer {

struct payload {
  using value_type = int;
};

template <class T> struct wrapper {
  using value_type = typename T::value_type;

  template <class U>
  static typename U::value_type convert(typename U::value_type value) {
    return value;
  }
};

template <class T> typename T::value_type make(typename T::value_type value) {
  return value;
}

} // namespace rex_outer

namespace rex_client {

using concrete_type = rex_outer::wrapper<rex_outer::payload>::value_type;

template <class T>
auto route(typename rex_outer::wrapper<T>::value_type value) ->
    typename rex_outer::wrapper<T>::value_type {
  return rex_outer::wrapper<T>::template convert<T>(rex_outer::make<T>(value));
}

} // namespace rex_client

int main() {
  rex_client::concrete_type value = 7;
  return rex_client::route<rex_outer::payload>(value) == 7 ? 0 : 1;
}
