// REX Issue #113: Template instantiation spelling must preserve template
// arguments (including namespace qualifiers) when generating
// type/qualified-name strings.

namespace ns {
struct Inner {};
} // namespace ns

template <typename T> struct Container {
  using value_type = T;
};

namespace use_site {
using InnerContainer = Container<ns::Inner>;
InnerContainer::value_type x{};
} // namespace use_site

int main() { return 0; }
