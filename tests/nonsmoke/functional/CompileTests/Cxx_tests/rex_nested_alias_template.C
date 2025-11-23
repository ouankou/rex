// Nested namespace alias templates should round-trip with correct qualification.
#include <chrono>
#include <ratio>
#include <vector>

namespace outer {
namespace inner {
template<class T>
using Vec = std::vector<T>;

template<class Rep>
using Milli = std::chrono::duration<Rep, std::ratio<1, 1000>>;

template<class T>
struct Holder {
    using value_type = T;
};
} // namespace inner
} // namespace outer

outer::inner::Vec<int> vi;
outer::inner::Milli<double> ms(1.5);
outer::inner::Holder<float> hf;

int main() {
    vi.push_back(42);
    (void)ms;
    (void)hf;
    return 0;
}
