void rex_target(int) {}
void rex_target(double) {}

int f1(int);

namespace first {
void rex_target(int) {}
} // namespace first

namespace second {
void rex_target(int) {}
} // namespace second

int main() {
  rex_target(1);
  rex_target(1.0);
  first::rex_target(1);
  second::rex_target(1);
  f1(1);
}
