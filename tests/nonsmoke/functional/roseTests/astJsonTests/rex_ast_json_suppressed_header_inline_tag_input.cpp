#include <rex_ast_json_suppressed_header_inline_tag.hpp>

auto rex_suppressed_template_enum =
    RexSuppressedHeaderTemplateInt::rex_suppressed_template_enum_value;

int read_rex_suppressed_header_state(const RexSuppressedHeaderState *state) {
  return state->mode + state->rex_suppressed_nested_value.bytes[0] +
         rex_suppressed_template_enum;
}
