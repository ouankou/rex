// Synthetic qualification stressor: this intentionally mirrors old vendor-style
// namespace patterns without depending on external libraries.
namespace vendor {
namespace lambda {

namespace {
using Integer = int;
int placeholder_1 = 0;
} // namespace

Integer transform(Integer value) { return value + placeholder_1; }

} // namespace lambda

namespace bind {
int placeholder_1 = 0;
} // namespace bind
} // namespace vendor

int placeholder_1 = 0;

int useVendorPlaceholders() {
  placeholder_1 = 1;
  vendor::lambda::placeholder_1 = 2;
  vendor::bind::placeholder_1 = 3;

  vendor::lambda::Integer number = vendor::lambda::transform(4);
  return placeholder_1 + vendor::lambda::placeholder_1 +
         vendor::bind::placeholder_1 + number;
}
