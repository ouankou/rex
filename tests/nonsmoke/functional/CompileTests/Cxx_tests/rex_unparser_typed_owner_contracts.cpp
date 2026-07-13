namespace rex_unparser_typed_owner {

template <typename T> struct Box {
  void ordinary();

  template <typename U> void templated();
};

template <> struct Box<int> {
  void ordinary();

  template <typename U> void templated();
};

void Box<int>::ordinary() {}

template <> void Box<int>::templated<long>() {}

template <> void Box<double>::ordinary() {}

template <> template <> void Box<double>::templated<long>() {}

namespace base_ns {

struct Base {
  explicit Base(int);
};

} // namespace base_ns

base_ns::Base::Base(int) {}

struct Host : base_ns::Base {
  Host();
};

Host::Host() : base_ns::Base(1) {}

typedef struct InlineTypedef {
  int value;
} InlineAlias;

struct InlineVariable {
  int value;
} inline_variable;

} // namespace rex_unparser_typed_owner
