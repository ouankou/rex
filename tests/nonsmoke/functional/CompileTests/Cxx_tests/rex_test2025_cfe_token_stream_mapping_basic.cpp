int nested_expr(int x) {
  if (x > 0) {
    x = x + 1;
  } else {
    x = x - 1;
  }

  for (int i = 0; i < 3; ++i) {
    x += i * (x - 1);
  }

  return x * (x + 2);
}

int main() { return nested_expr(3); }
