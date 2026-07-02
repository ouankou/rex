int length(int value) {
  switch (value) {
  case 0:
    return 1;
  case 1:
    return 2;
  default:
    return value;
  }
}

int main() {
  int value = length(1);
  return value == 2 ? 0 : 1;
}
