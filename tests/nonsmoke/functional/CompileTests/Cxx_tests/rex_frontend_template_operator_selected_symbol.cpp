namespace rex_selected {
template <class T> struct Iterator;
template <class T> bool operator!=(const Iterator<T> &, const Iterator<T> &);
template <class T> struct Iterator {
  T value;
  friend bool operator!= <T>(const Iterator &, const Iterator &);
};
template <class T> bool operator!=(const Iterator<T> &a, const Iterator<T> &b) {
  return a.value != b.value;
}
template <class T> Iterator<T> operator<<(Iterator<T> value, int shift) {
  return {value.value << shift};
}
template <class T> T operator_named(T value) { return value; }
} // namespace rex_selected
template <class T> T rex_recursive_count(T value) {
  return value == 0 ? 0 : 1 + rex_recursive_count(value - 1);
}
template <class T> T rex_explicit(T value) { return value + 1; }
template int rex_explicit<int>(int);
template <> long rex_explicit<long>(long);
template <> long rex_explicit<long>(long value) { return value + 2; }
template <class T> T rex_external(T value) { return value; }
extern template int rex_external<int>(int);
struct RexExplicitMember {
  template <class T> T advance(T value) { return value + 1; }
  template <class T> T external(T value) { return value; }
};
template int RexExplicitMember::advance<int>(int);
template <> long RexExplicitMember::advance<long>(long);
template <> long RexExplicitMember::advance<long>(long value) {
  return value + 3;
}
extern template int RexExplicitMember::external<int>(int);
struct RexPlacement {
  template <class T>
  static void *operator new(decltype(sizeof(0)), T *address) {
    return address;
  }
  template <class T> static void operator delete(void *, T *) {}
};
template <class Allocation, class T> void *rex_allocate(T *address) {
  return Allocation::template operator new<T>(sizeof(T), address);
}
template <class Allocation, class T> void rex_release(T *address) {
  Allocation::template operator delete<T>(address, address);
}
int main() {
  using rex_selected::Iterator;
  Iterator<int> a{1}, b{2};
  RexExplicitMember member;
  auto identity = [](auto value) { return value; };
  auto selected = &rex_selected::operator!= <int>;
  int storage = 0;
  void *allocation = rex_allocate<RexPlacement>(&storage);
  rex_release<RexPlacement>(&storage);
  return selected(a, b) && a != b && rex_recursive_count<int>(5) == 5 &&
                 (a << 1).value == 2 && rex_selected::operator_named(9) == 9 &&
                 identity(7) == 7 && identity(8L) == 8L &&
                 rex_explicit(7) == 8 && rex_explicit(7L) == 9 &&
                 member.advance(7L) == 10 &&
                 sizeof(rex_external(7)) == sizeof(int) &&
                 member.advance(7) == 8 &&
                 sizeof(member.external(7)) == sizeof(int) &&
                 allocation == &storage
             ? 0
             : 1;
}
