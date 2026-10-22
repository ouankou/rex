int normalize_single_statement_bodies(int n) {
  int total = 0;

  if (n > 0)
    total += n;
  else
    total -= n;

  for (int i = 0; i < n; ++i)
    total += i;

  while (n > 0)
    total += --n;

  do
    total += 1;
  while (total < 3);

  switch (total) {
  case 0:
    total = 1;
    break;
  default:
    total += 2;
    break;
  }

  return total;
}

int rex_normalize_range_body(int n) {
  int values[] = {1, 2};
  for (int value : values)
    n += value;
  return n;
}

int main() {
  return rex_normalize_range_body(normalize_single_statement_bodies(4)) - 3;
}
