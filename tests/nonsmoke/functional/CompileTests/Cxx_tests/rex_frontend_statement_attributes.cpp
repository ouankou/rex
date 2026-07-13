extern int rex_statement_attribute_callee(int);

int rex_statement_attributes(int value) {
  [[assume(value >= 0)]];
  [[clang::nomerge]] rex_statement_attribute_callee(value);
  [[clang::always_inline]] rex_statement_attribute_callee(value);

  if (value > 8) [[likely]]
    return value;
  if (value < 0) [[unlikely]]
    return -value;

  switch (value) {
  case 0:
    ++value;
    [[fallthrough]];
  default:
    return value;
  }
}

int rex_statement_attribute_tail(int value) {
  [[clang::musttail]] return rex_statement_attribute_callee(value);
}

int rex_statement_attribute_gnu_fallthrough(int value) {
  switch (value) {
  case 0:
    ++value;
    __attribute__((fallthrough));
  default:
    return value;
  }
}

void rex_statement_attribute_loops(int *values) {
#pragma clang loop vectorize_width(4, fixed) interleave(disable)
  for (int index = 0; index < 8; ++index)
    values[index] = index;

#pragma unroll 4
  for (int index = 0; index < 8; ++index)
    values[index] += 1;
}
