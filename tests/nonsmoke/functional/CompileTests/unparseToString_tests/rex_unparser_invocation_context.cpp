int rex_context_value = 7;
int rex_context_result = rex_context_value;

void rex_context_function(int rex_context_parameter);

namespace rex_context {
struct Value {
  int value;
};
enum Choice { delta = 7 };
}

static_assert(alignof(rex_context::Value) > 0, "record alignment");
rex_context::Value rex_context_record{rex_context::delta};
