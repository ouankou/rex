int old_style_sum(a, b)
int a;
int b;
{
  return a + b;
}

int switch_on_expression(int x) {
  switch (x) {
  case 0:
    return old_style_sum(1, 2);
  default:
    return x;
  }
}

int while_with_multi_decl(int limit) {
  int i = 0, step = 1;
  while (i < limit) {
    i += step;
  }
  return i;
}

int inline_enum_decl_group(void) {
  enum {
    rex_ast_json_enum_a = 1,
    rex_ast_json_enum_b = rex_ast_json_enum_a + 1
  } first,
      second;
  first = rex_ast_json_enum_a;
  second = rex_ast_json_enum_b;
  return first + second;
}

int nested_array_subscript_defined_read(int *values, int index) {
  if (index > 0) {
    if (values[index] != 0) {
      return values[index - 1];
    }
  }
  return values[0];
}
