struct rex_symbolic_record {
  int member;
};

int rex_symbolic_annotation_member(rex_symbolic_record object) {
  return object.member;
}

int rex_symbolic_annotation_variable(int value) { return value; }

int *rex_symbolic_annotation_address(int *pointer) { return &*pointer; }

double rex_symbolic_annotation_conversion(int value) { return value; }

int rex_symbolic_annotation_modulo(int left, int right) { return left % right; }

int rex_symbolic_annotation_bit_and(int left, int right) {
  return left & right;
}

int rex_symbolic_annotation_bit_or(int left, int right) { return left | right; }

int rex_symbolic_annotation_bit_complement(int value) { return ~value; }

int rex_symbolic_annotation_prefix_increment(int &value) { return ++value; }

int rex_symbolic_annotation_postfix_decrement(int &value) { return value--; }

int *rex_symbolic_annotation_allocation() { return new int(1); }

int rex_symbolic_annotation_observable_zero_product(int &value) {
  return 0 * value++;
}

int rex_symbolic_annotation_volatile_zero_product(volatile int *pointer) {
  return 0 * *pointer;
}

unsigned rex_symbolic_annotation_unsigned_minus(unsigned value) {
  return -value;
}

double rex_symbolic_annotation_floating_group(double left, double middle,
                                              double right) {
  return left + (middle + right);
}

int rex_symbolic_annotation_array_access(int (*array)[4], int row, int column) {
  return array[row][column];
}

_Complex double rex_symbolic_annotation_complex_floating_group(
    _Complex double left, _Complex double middle, _Complex double right) {
  return left + (middle + right);
}
