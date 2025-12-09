// test2025_issue83_redeclarations.cpp
// Tests explicit template specialization with separate forward declaration and
// definition.

template <typename T> struct MyTemplate {
  T value;
};

// 1. Forward declaration of specialization
template <> struct MyTemplate<int>;

// 2. Definition of the same specialization
template <> struct MyTemplate<int> {
  int x;
};

void foo() {
  // This usage requires the definition to be visible and correctly linked.
  MyTemplate<int> obj;
  obj.x = 42;
}
