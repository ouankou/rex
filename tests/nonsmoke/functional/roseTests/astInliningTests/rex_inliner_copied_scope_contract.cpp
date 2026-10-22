int rex_inline_external_value = 4;

inline int rex_inline_scope_source(int value) {
  extern int rex_inline_external_value;
  auto add_captured_value = [value](int argument) {
    int local_copy = argument;
    return value + local_copy;
  };
  return add_captured_value(2) + rex_inline_external_value;
}

class RexInlinePrivateContext {
  static int private_value() { return 5; }

public:
  static int captured_value() {
    auto read_private_value = []() { return private_value(); };
    return read_private_value();
  }
};

int main() {
  return rex_inline_scope_source(1) == 7 &&
                 RexInlinePrivateContext::captured_value() == 5
             ? 0
             : 1;
}
