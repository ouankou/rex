int rex_statement_numbering(int limit) {
  int sum = 0;
  for (int index = 0; index < limit; ++index) {
    if (index % 2 == 0) {
      sum += index;
    }
  }
  return sum;
}
