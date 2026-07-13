namespace rex_typed_type_context_types {
using Lane = unsigned int;

struct Payload {
  int value;
};
} // namespace rex_typed_type_context_types

typedef rex_typed_type_context_types::Lane RexQualifiedVector
    __attribute__((__vector_size__(16)));

namespace rex_typed_type_context_owner {
struct Owner {
  using Callback = rex_typed_type_context_types::Payload (Owner::*)(
      rex_typed_type_context_types::Payload, decltype(nullptr),
      RexQualifiedVector) const &;

  rex_typed_type_context_types::Payload
  invoke(rex_typed_type_context_types::Payload payload, decltype(nullptr),
         RexQualifiedVector) const & {
    return payload;
  }
};
} // namespace rex_typed_type_context_owner

int main() {
  rex_typed_type_context_owner::Owner owner;
  rex_typed_type_context_owner::Owner::Callback callback =
      &rex_typed_type_context_owner::Owner::invoke;
  RexQualifiedVector lanes = {};
  return (owner.*callback)({17}, nullptr, lanes).value == 17 ? 0 : 1;
}
