extern int rex_ast_json_attribute_target(int);

int rex_ast_json_statement_attributes(int value) {
  [[assume(value >= 0)]];
  if (value != 0) [[likely]]
    return value;
  [[clang::nomerge]] rex_ast_json_attribute_target(value);
  return value;
}

void rex_ast_json_loop_attribute(int *values) {
#pragma clang loop vectorize_width(4, fixed) interleave(disable)
  for (int index = 0; index < 8; ++index)
    values[index] = index;
}
