void count(int *value) {
  for (int index = 0; index < 8; ++index) {
    __typeof__(index) current = index;
    *value += current;
  }
}
