namespace ns {
template <typename T> struct Later;
}

template <typename T> using Alias = typename ns::Later<T>::type;

namespace ns {
template <typename T> struct Later {
  using type = T;
};
} // namespace ns

Alias<int> make_alias();

int main() { return sizeof(make_alias()); }
