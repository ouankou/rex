#include <concepts>

namespace RexConstraints {
template <class T>
concept Small = sizeof(T) <= sizeof(int);
}

int rex_choose(RexConstraints::Small auto) { return 1; }
int rex_choose(auto) { return 2; }
int rex_pointer(const RexConstraints::Small auto *) { return 3; }
int rex_pointer(const auto *) { return 4; }
void rex_only(RexConstraints::Small auto);
int rex_integral(std::integral auto value) { return value; }

template <class T>
concept RexAccepted = requires(T value) { rex_only(value); };
static_assert(RexAccepted<int>);
static_assert(!RexAccepted<long long>);

int main() {
  int small = 0;
  long long large = 0;
  return rex_integral(7) == 7 && rex_choose(small) == 1 &&
                 rex_choose(large) == 2 && rex_pointer(&small) == 3 &&
                 rex_pointer(&large) == 4
             ? 0
             : 1;
}
namespace rex_template_default {
template <class T> struct Box {};
} // namespace rex_template_default

template <template <class> class Container = rex_template_default::Box>
struct rex_default_template_owner {
  Container<int> value;
};

rex_default_template_owner<> rex_default_template_instance;
