int foo(int argc, char* argv[]) {
  return argc + (argv != 0);
}

int test_moveVariableDeclaration(int j) {
  int i;
  for (i = 0; i < 3; ++i) {
    j += i;
  }
  return j;
}

int main(int argc, char* argv[]) {
  return foo(argc, argv) + test_moveVariableDeclaration(argc);
}
