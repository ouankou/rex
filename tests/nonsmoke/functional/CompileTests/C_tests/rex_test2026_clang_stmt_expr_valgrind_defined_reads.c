int rex_stmt_expr_defined_read(int value) {
  int result = ({
    int temporary = value + 1;
    temporary;
  });
  return result;
}

int main(void) { return rex_stmt_expr_defined_read(0) != 1; }
