struct S {
  union {
    int a;
    int b;
  };
  S() = default;
  S(const S &) = default;
  S &operator=(const S &) = default;
};

int main() {
  S x{};
  S y{};
  y = x;
  return y.a;
}
