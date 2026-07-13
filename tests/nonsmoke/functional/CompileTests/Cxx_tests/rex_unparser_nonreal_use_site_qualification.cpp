#include <cstddef>

using std::size_t;

std::size_t rex_qualified_size = 1;

std::size_t rex_qualified_functional_cast() { return std::size_t(-1); }

size_t rex_unqualified_functional_cast(size_t value) { return size_t(value); }

template <typename T> struct RexUseSiteBox {
  explicit RexUseSiteBox(T input) : value(input) {}
  T value;
};

template <typename T> struct RexUseSiteFactory {
  using Result = RexUseSiteBox<T>;

  static Result build(T value) { return Result(value); }
};

template <typename T> struct RexUseSiteOuter {
  struct Inner {
    T value;
  };
};

template <typename T> struct RexUseSiteWrap {
  T value;
};

template <typename T> struct RexUseSiteSelect {
  using Type = T;
};

template <typename T>
typename RexUseSiteOuter<T>::Inner *rex_make_qualified_inner(T value) {
  static typename RexUseSiteOuter<T>::Inner result{value};
  return &result;
}

template <typename T>
typename RexUseSiteSelect<RexUseSiteWrap<RexUseSiteWrap<T>>>::Type
rex_nested_qualified_return(T value) {
  return {{value}};
}

int main() {
  return RexUseSiteFactory<int>::build(3).value ==
                     static_cast<int>(rex_qualified_size + 2) &&
                 rex_make_qualified_inner(4)->value == 4 &&
                 rex_nested_qualified_return(5).value.value == 5 &&
                 rex_qualified_functional_cast() > rex_qualified_size
             ? 0
             : 1;
}
