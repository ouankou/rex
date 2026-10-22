int rex_safe_add() { return 3 + 4; }
int rex_safe_multiply() { return 6 * 7; }
double rex_double_add() { return 1.25 + 2.5; }
double rex_double_multiply() { return 1.5 * 4.0; }
int rex_overflow_add() { return __INT_MAX__ + 1; }
int rex_overflow_multiply() { return __INT_MAX__ * 2; }
int rex_nested_add(int input) { return (input + 2) + 3; }
int rex_nested_overflow_add(int input) { return (input + __INT_MAX__) + 1; }

int rex_operand_evaluations;
int rex_observed_operand() {
  ++rex_operand_evaluations;
  return 9;
}
int rex_zero_side_effects() {
  return 0 * rex_observed_operand() + rex_observed_operand() * 0;
}
int rex_distribution_side_effects(int input) {
  return (input + 2) * rex_observed_operand();
}
double rex_distribution_floating(double first, double second, double factor) {
  return (first + second) * factor;
}
double rex_reassociation_floating(double first, double second, double third) {
  return first + (second + third);
}

int rex_distribution_signed(int input) { return (input + -1) * (input + 2); }
int rex_reassociation_signed(int first, int second, int third) {
  return first + (second + third);
}
unsigned rex_distribution_unsigned(unsigned first, unsigned second,
                                   unsigned factor) {
  return (first + second) * factor;
}

int main() {
  if (rex_zero_side_effects() != 0 || rex_operand_evaluations != 2)
    return 1;
  rex_operand_evaluations = 0;
  if (rex_distribution_side_effects(4) != 54 || rex_operand_evaluations != 1 ||
      rex_distribution_floating(__DBL_MAX__, -__DBL_MAX__, 2.0) != 0.0 ||
      rex_reassociation_floating(1.0e16, -1.0e16, 1.0) != 0.0 ||
      rex_distribution_signed(-46341) != 2147441938 ||
      rex_reassociation_signed(__INT_MAX__, __INT_MAX__, -__INT_MAX__) !=
          __INT_MAX__ ||
      rex_distribution_unsigned(~0u, 2, 3) != 3)
    return 2;
  return rex_safe_add() != 7 || rex_safe_multiply() != 42 ||
         rex_double_add() != 3.75 || rex_double_multiply() != 6.0 ||
         rex_nested_add(4) != 9 || rex_nested_overflow_add(-__INT_MAX__) != 1;
}
