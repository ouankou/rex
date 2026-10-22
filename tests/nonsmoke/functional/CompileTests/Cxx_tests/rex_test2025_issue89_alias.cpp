// REX Issue #89: Type alias template arguments must be preserved.
// Expected: using IntContainer = Container<int>

template <typename T> struct Container {
  using value_type = T;
  typedef T type;
};

using IntContainer = Container<int>;

namespace rex_alias_scope {
extern "C++" {
template <class T> using LinkedAlias = Container<T>;
}
} // namespace rex_alias_scope

using LinkedIntContainer = rex_alias_scope::LinkedAlias<int>;
static_assert(__is_same(LinkedIntContainer, IntContainer));

int main() {
  IntContainer c;
  IntContainer::value_type x = 5;
  return x;
}
