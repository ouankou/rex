int main() {
  int sum = 0;
  for (int i = 0; i < 3; ++i) {
    sum += i;
  }
  for (; sum < 6; ++sum) {
  }
  for (;;) {
    break;
  }
  int i = 7;
  return sum + i;
}
