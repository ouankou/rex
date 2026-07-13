namespace rex_function_argument_target {
struct Payload {
  int value;
};

Payload pass(Payload payload) { return payload; }

struct Owner {
  Payload member(Payload payload) const { return payload; }
};
} // namespace rex_function_argument_target

namespace rex_function_argument_use {
struct Payload {};

using Callback = rex_function_argument_target::Payload (*)(
    rex_function_argument_target::Payload);
using NestedCallback = rex_function_argument_target::Payload (*)(
    rex_function_argument_target::Payload (*)(
        rex_function_argument_target::Payload));
using MemberPointerConsumer = void (*)(rex_function_argument_target::Payload (
    rex_function_argument_target::Owner::*)(
    rex_function_argument_target::Payload) const);
static_assert(__is_same(Callback, rex_function_argument_target::Payload (*)(
                                      rex_function_argument_target::Payload)));

rex_function_argument_target::Payload exercise() {
  rex_function_argument_target::Payload (*callback)(
      rex_function_argument_target::Payload) =
      &rex_function_argument_target::pass;
  Callback cast_callback =
      static_cast<rex_function_argument_target::Payload (*)(
          rex_function_argument_target::Payload)>(callback);
  const auto function_pointer_size =
      sizeof(rex_function_argument_target::Payload (*)(
          rex_function_argument_target::Payload));
  const auto function_pointer_alignment =
      alignof(rex_function_argument_target::Payload (*)(
          rex_function_argument_target::Payload));
  (void)function_pointer_size;
  (void)function_pointer_alignment;
  return cast_callback({17});
}
} // namespace rex_function_argument_use

int main() { return rex_function_argument_use::exercise().value == 17 ? 0 : 1; }
