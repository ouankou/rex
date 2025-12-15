namespace Lib {
template <class... Ts> struct Tuple {};
} // namespace Lib

namespace A {
namespace Lib {
template <class... Ts> struct Tuple {};
} // namespace Lib

template <template <class...> class TT, class... Ts> struct UsePack {};

UsePack<::Lib::Tuple, int, double> v{};
} // namespace A

int main() { return 0; }
