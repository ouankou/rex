// Valid switch/case coverage with an init-statement and declarations scoped
// inside each case block.
void foobar() {
  int result = 0;

  switch (int i = 42) {
  case 1: {
    int x = i;
    result = x + 1;
    break;
  }
  case 2: {
    int y = 4;
    int z = i + y;
    result = z;
    break;
  }
  case 3:
  default: {
    int fallback = i;
    result = fallback;
    break;
  }
  }

  (void)result;
}
