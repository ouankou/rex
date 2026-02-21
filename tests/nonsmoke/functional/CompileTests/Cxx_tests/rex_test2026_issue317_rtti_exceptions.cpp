#include <memory>
#include <typeinfo>

struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {};

int main() {
  std::unique_ptr<Base> base = std::make_unique<Derived>();
  Derived *derived = dynamic_cast<Derived *>(base.get());
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
      return 3;
    }
  }

  return 0;
}
