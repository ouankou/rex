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

int main() { return normalize_single_statement_bodies(4); }
