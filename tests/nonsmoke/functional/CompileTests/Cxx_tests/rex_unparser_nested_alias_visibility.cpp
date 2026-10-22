namespace rex_nested_alias_target {
struct Payload {
  int value;
};
} // namespace rex_nested_alias_target

namespace rex_nested_alias_name = rex_nested_alias_target;
namespace rex_nested_alias_chain = rex_nested_alias_name;

namespace rex_nested_alias_use {
using namespace rex_nested_alias_target;

using Callback = Payload (*)(Payload);
using AliasCallback =
    rex_nested_alias_name::Payload (*)(rex_nested_alias_name::Payload);
using ChainedPayload = rex_nested_alias_chain::Payload;

Callback callback = nullptr;
AliasCallback alias_callback = nullptr;
} // namespace rex_nested_alias_use

int main() {
  return rex_nested_alias_use::callback == nullptr &&
                 rex_nested_alias_use::alias_callback == nullptr
             ? 0
             : 1;
}
