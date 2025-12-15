namespace A {
void f();
}

namespace A {
void g();
}

int main() {
  A::f();
  A::g();
}
