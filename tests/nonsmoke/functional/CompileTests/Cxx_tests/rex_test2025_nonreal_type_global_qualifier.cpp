namespace Lib {
template <typename T> struct Vec {
  using value_type = T;
};
} // namespace Lib

namespace ns {
namespace Lib {}

template <typename T> struct S {
  using X = typename ::Lib::Vec<T>::value_type;
};
} // namespace ns

int main() {
  ns::S<int>::X x = 42;
  (void)x;
}
