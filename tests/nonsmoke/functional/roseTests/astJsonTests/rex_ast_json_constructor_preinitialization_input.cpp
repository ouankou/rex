struct VirtualBase {};
struct NonvirtualBase {};

struct Derived : virtual VirtualBase, NonvirtualBase {
  int value;

  explicit Derived(int input) : VirtualBase{}, NonvirtualBase{}, value(input) {}
  Derived() : Derived(7) {}
};

Derived object;
