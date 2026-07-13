inline int rex_inline_scope_source(int value) {
  auto add_captured_value = [value](int argument) {
    int local_copy = argument;
    return value + local_copy;
  };
  return add_captured_value(2);
}

int main() { return rex_inline_scope_source(1) == 3 ? 0 : 1; }
