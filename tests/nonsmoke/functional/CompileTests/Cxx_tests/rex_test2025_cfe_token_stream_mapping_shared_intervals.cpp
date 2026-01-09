int a, b, c;
int *p, q;

int main() {
  return a + b + c + (p ? *p : 0) + q;
}
