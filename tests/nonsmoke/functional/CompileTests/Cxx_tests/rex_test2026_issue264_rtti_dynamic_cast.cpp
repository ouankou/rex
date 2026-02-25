struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {
  int payload = 7;
};

int probe(Base *base) {
  Derived *derived = dynamic_cast<Derived *>(base);
  return derived ? derived->payload : 0;
}

int main() {
  Derived d;
  return probe(&d) == 7 ? 0 : 1;
}
