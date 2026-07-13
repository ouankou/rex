template <class... Values> int rex_fold_sum(Values... values) {
  return (0 + ... + values);
}

template <class... Values> void rex_consume(Values...) {}

template <class... Values> void rex_expand(Values... values) {
  rex_consume((values + 1)...);
}

int *rex_statement_expression_pointer() {
  return ({
    static int values[2] = {};
    values;
  });
}

void rex_statement_expression_void() {
  ({ [[maybe_unused]] int declaration_is_last = 0; });
}

int main() {
  rex_expand(1, 2);
  rex_statement_expression_void();
  return rex_fold_sum(1, 2, 3) + rex_statement_expression_pointer()[0];
}
