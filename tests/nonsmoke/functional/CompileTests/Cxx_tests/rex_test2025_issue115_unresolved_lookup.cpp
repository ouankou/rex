template <typename T> void f(T t) { foo(t); }

struct X {};
void foo(X) {}

int main() {
  X x{};
  f(x);
}
