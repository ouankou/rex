// Regression for issue #203:
// Ensure mangling handles template-qualified names from default template
// arguments without asserting in mangleQualifiersToString.

template <typename ElementType> class Container {
private:
  using private_type = int;

public:
  template <typename ValueType = private_type> class Box {};

  void accept(Box<> *box) { (void)box; }
};

int main() {
  Container<int> container;
  container.accept(0);
  return 0;
}
