namespace N {
class X {};
} // namespace N

using namespace N;

template <typename T> class X {};

void foo() {
  N::X abc;
  ::X<int> xyz;
}
