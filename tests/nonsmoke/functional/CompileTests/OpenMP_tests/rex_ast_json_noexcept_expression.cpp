int rex_noexcept_operand(int value) noexcept { return value + 1; }

void rex_ast_json_noexcept_expression(int *value) {
  const bool does_not_throw = noexcept(rex_noexcept_operand(*value));
#pragma omp parallel if (does_not_throw)
  {
    *value = rex_noexcept_operand(*value);
  }
}
