namespace A {
template <typename T> bool operator==(T, T);
}

namespace A {
template <typename T> bool operator==(T, int);

class X {
public:
  template <typename T> friend bool operator==(T, T);
};
//   }
// namespace A
//   {
template <typename T> bool operator==(T, T) { return false; }
} // namespace A

namespace A {}
