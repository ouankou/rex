template <class T> class rex_pair {
public:
  rex_pair(T first, T second) : first_(first), second_(second) {}

  T maximum() const { return first_ > second_ ? first_ : second_; }

private:
  T first_;
  T second_;
};

class rex_base {
public:
  virtual int evaluate(int value) { return value; }
};

class rex_derived : public rex_base {
public:
  template <class T> int evaluate(T value) { return static_cast<int>(value); }
  int evaluate(int value) override { return evaluate<>(value); }
};

int foo(int argc, char **) { return argc; }

int test_moveVariableDeclaration(int input) {
  int index;

  for (index = 0; index < 4; ++index) {
    input += index;
  }
  return input;
}

int rex_interface_function_coverage(int input) {
  rex_pair<int> values(input, 3);
  rex_derived derived;
  int result = values.maximum();
  int marker = 12345;

  for (int index = 0; index < 4; ++index) {
    result += index;
  }

  while (result < 16) {
    ++result;
  }

  switch (input) {
  case 0:
    result += marker;
    break;
  default:
    result += derived.evaluate(input);
    break;
  }

  return result;
}

int main(int argc, char **) { return rex_interface_function_coverage(argc); }
