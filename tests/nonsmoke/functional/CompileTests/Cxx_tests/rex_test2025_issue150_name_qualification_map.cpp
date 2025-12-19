namespace Outer {
struct Host {
  struct Nested {};
};

namespace Inner {
using NestedAlias = Host::Nested;
} // namespace Inner
} // namespace Outer

using Outer::Host;

Outer::Host::Nested global_nested;
Host::Nested using_nested;
Outer::Inner::NestedAlias alias_nested;

void consume_global(Outer::Host::Nested) {}
void consume_using(Host::Nested) {}
void consume_alias(Outer::Inner::NestedAlias) {}

int main() {
  consume_global(global_nested);
  consume_using(using_nested);
  consume_alias(alias_nested);
  return 0;
}
