// Test Issue 87: Functions Inside Namespaces Lost
// Expected: namespace should contain the function
// Actual: namespace ns {} - function completely lost

namespace ns {
void bar() { int x = 42; }
} // namespace ns

int main() {
  ns::bar();
  return 0;
}
