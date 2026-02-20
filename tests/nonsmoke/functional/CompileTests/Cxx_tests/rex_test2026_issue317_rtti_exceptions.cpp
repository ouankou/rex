#include <typeinfo>

struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {};

int main() {
  Base *base = new Derived();
  Derived *derived = dynamic_cast<Derived *>(base);
  if (derived == 0) {
    return 1;
  }

  if (typeid(*base) != typeid(Derived)) {
    return 2;
  }

  try {
    throw 42;
  } catch (int value) {
    if (value != 42) {
      delete base;
      return 3;
    }
  }

  delete base;
  return 0;
}
