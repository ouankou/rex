int forward_decl(int x);

int forward_decl(int x) { return x; }

int expr_and_return_semicolons(int x) {
  x = 42;
  return x;
}
