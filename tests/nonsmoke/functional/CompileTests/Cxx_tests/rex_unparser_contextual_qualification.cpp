namespace model {

struct Value {};

template <class T> struct Box {
  T value;
};

Box<Value> make_local() { return {}; }

struct Base {
  void select() {}
  union {
    int anonymous_member;
  };
};

struct Derived : Base {
  void select(int) {}
  void call_base() { Base::select(); }
  int read_anonymous() const { return anonymous_member; }
};

} // namespace model

model::Box<model::Value> make_global() { return {}; }

namespace semantic_outer_tag {

struct Owner {
  struct Introduced *pointer;
  struct Introduced {};
};

} // namespace semantic_outer_tag

semantic_outer_tag::Introduced *semantic_outer_pointer = nullptr;

template <class Callable> int accept_callable(Callable callable) {
  callable();
  return 0;
}

template <class T> int lambda_default_argument(int = accept_callable([] {})) {
  return sizeof(T);
}

template <class T> void assign_dependent_references(T &lhs, T &rhs) {
  lhs = rhs;
}

template <class T> struct ExplicitSpecializationOwner {
  static int value();
};

template <> int ExplicitSpecializationOwner<int>::value();

template <> int ExplicitSpecializationOwner<int>::value() { return 7; }

int main() {
  model::Derived value;
  value.call_base();
  (void)make_global();
  int lhs = 0;
  int rhs = 1;
  assign_dependent_references(lhs, rhs);
  return value.read_anonymous() + lambda_default_argument<long>() +
         ExplicitSpecializationOwner<int>::value() + lhs;
}
