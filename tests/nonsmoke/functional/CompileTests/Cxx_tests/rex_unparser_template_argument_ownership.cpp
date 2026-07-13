template <class T> struct box {
  T value;
};

template <class T> box<T> identity(box<T>);

template <class T> box<T> identity(box<T> input) { return input; }

template <class T, template <class> class Wrapper> struct owner {
  Wrapper<T> wrapped;
};

template <class T> struct traits;

template <> struct traits<box<int>> {
  static constexpr int value = 1;
};

template <class T, class Allocator = T *> struct defaulted_box {
  T value;
};

template <class Facet> const Facet &get_facet();

// A normalized AST unparse must emit the complete nested class-template type
// identity even though only the outer function-template argument is marked as
// source-explicit.  Dropping the class's semantic arguments produces the
// ill-formed get_facet<defaulted_box<>>() spelling.
extern template const defaulted_box<int> &get_facet<defaulted_box<int>>();

int template_argument_value() {
  box<int> input{2};
  box<int> output = identity(input);
  owner<long, box> nested{{3}};
  return output.value + static_cast<int>(nested.wrapped.value) +
         traits<box<int>>::value;
}
