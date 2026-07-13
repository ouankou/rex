#define REX_AST_JSON_MACRO_CALL(value) rex_ast_json_target(value)

int rex_ast_json_target(int value) { return value + 1; }

const char *rex_ast_json_raw_string = u8R"json_tag(alpha  beta
gamma)json_tag";

static_assert(true, u8"ast json unevaluated");

struct RexAstJsonMacroResult {
  int value;
};

RexAstJsonMacroResult rex_ast_json_make_result() {
  return {REX_AST_JSON_MACRO_CALL(41)};
}

int main() { return rex_ast_json_make_result().value == 42 ? 0 : 1; }
