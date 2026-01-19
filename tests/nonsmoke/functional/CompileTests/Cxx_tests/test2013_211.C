// This test code generates a namespace name of
// _GLOBAL__N__19_test2013_apps_11_cc_b0e478c3 for the un-named namespace.  Any
// simpler test code will not generate the specially named namespace.

#include <vector>

namespace google {
namespace protobuf {

class DescriptorPool {
public:
  class Tables;
};

namespace {

struct Symbol {};

// typedef std::vector<Symbol> SymbolsByNameMap;
// typedef vector<Symbol> SymbolsByNameMap;
typedef std::pair<int, Symbol> SymbolsByNameMap;

} // anonymous namespace

class DescriptorPool::Tables {

private:
  SymbolsByNameMap symbols_by_name_;
};

} // namespace protobuf
} // namespace google
