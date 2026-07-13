int rex_frontend_statement_expression_scope(int outer) {
  return ({
    int local = outer + 1;
    ({
      int nested = local + outer;
      nested;
    });
  });
}
