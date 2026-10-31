// The defaulted default constructor of Holder<NoDefault> is never used, so
// its exception specification is never needed ([except.spec]) and its
// default member initializer is never instantiated.  Translating the class
// must not evaluate that specification: doing so instantiates the ill-formed
// initializer and rejects this valid program.

struct NoDefault {
  explicit NoDefault(int) {}
};

template <class T> struct Holder {
  T value{};
  Holder() = default;
  explicit Holder(T initial) : value(initial) {}
};

struct User : Holder<NoDefault> {
  User() : Holder<NoDefault>(NoDefault(0)) {}
};

int main() {
  User user;
  (void)user;
  return 0;
}
