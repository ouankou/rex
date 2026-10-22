int rex_loop(int incoming, int count) {
  int total = 0;
  while (count > 0) {
    if (count > 1)
      total += incoming;
    --count;
  }
  return total;
}
