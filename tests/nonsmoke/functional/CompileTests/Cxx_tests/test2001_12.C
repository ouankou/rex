struct Pair {
  int a;
  int b;
  Pair(int x, int y) : a(x), b(y) {}
};

int sum(const Pair &p) { return p.a + p.b; }

int main() {
  Pair p(1, 2);
  return sum(p) - 3;
}
