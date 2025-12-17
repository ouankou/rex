// REX Issue #113: Template instantiation spelling must preserve template
// arguments when generating type/qualified-name strings.

namespace ns {
template <typename T> struct Container {
  using value_type = T;
};
} // namespace ns

using IntContainer = ns::Container<int>;

int main() {
  IntContainer::value_type x = 0;
  return x;
}
