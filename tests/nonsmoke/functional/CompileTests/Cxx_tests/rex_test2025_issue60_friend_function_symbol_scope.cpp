namespace N {
struct C {
  friend int f(const C &) { return 1; }
  friend int g(const C &);
};

int g(const C &) { return 2; }

int use() {
  C c{};
  return f(c) + g(c);
}
} // namespace N
