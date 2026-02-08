// Synthetic qualification stressor: this intentionally mirrors old vendor-style
// namespace patterns without depending on external libraries.
namespace vendor {
namespace lambda {

namespace {
using Integer = int;
int _1 = 0;
} // namespace

Integer transform(Integer value) { return value + _1; }

} // namespace lambda

namespace bind {
int _1 = 0;
} // namespace bind
} // namespace vendor

int _1 = 0;

int useVendorPlaceholders() {
  _1 = 1;
  vendor::lambda::_1 = 2;
  vendor::bind::_1 = 3;

  vendor::lambda::Integer number = vendor::lambda::transform(4);
  return _1 + vendor::lambda::_1 + vendor::bind::_1 + number;
}
