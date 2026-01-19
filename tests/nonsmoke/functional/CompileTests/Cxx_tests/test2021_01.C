
// This is a copy of test2007_158.C except for the added statement "x = 43;" (and the comment) below.

void foo() {
  if (int x = 42) {
    // This is required to pass the tests in
    x = 43;
  } else {
  }
  switch (int x = 7) {}
  while (int x = 7) {
  }
  for (int i = 1; i < 10; i++) {
  }
  for (int x = 1, y = 2; int test = 3; x++, y++) {
  }
}
