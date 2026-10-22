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
  virtual int f(int value) { return value; }
};

class Derived : public rex_base {
public:
  template <class T> int f(T value) { return static_cast<int>(value); }
  int f(int value) override { return f<>(value); }
};

class MyList {
public:
  int &operator[](int index) { return values_[index]; }

private:
  int values_[4] = {0, 1, 2, 3};
};

struct rex_default_value {
  int value;
};

int rex_default_value_init(rex_default_value value = {3}) {
  return value.value;
}

int foo(int argc, char **) { return argc; }

int test_moveVariableDeclaration(int input) {
  int index;

  for (index = 0; index < 4; ++index) {
    input += index;
  }
  return input;
}

int rex_interface_function_coverage(int input) {
  rex_pair<int> values(input, rex_default_value_init());
  Derived derived;
  MyList indices;
  int result = values.maximum();
  int marker = 12345;

  for (int index = 0; index < 4; ++index) {
    result += indices[index];
  }

  while (result < 16) {
    ++result;
  }

  switch (input) {
  case 0:
    result += marker;
    break;
  default:
    result += derived.f(input);
    break;
  }

  return result;
}

int main(int argc, char **) { return rex_interface_function_coverage(argc); }
