int helper(int value) {
  if (value > 0) {
    return value + 1;
  }

  return value - 1;
}

int main() { return helper(1); }
