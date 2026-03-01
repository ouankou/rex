void foobar() {
  int count = 0;

  switch (count) {
  case 40:
    41;
    // #pragma XXX
    int abc;
    __attribute__((__fallthrough__));
  case 42:
    43;
  }
}
