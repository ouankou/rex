// Valid switch/case coverage with an init-statement and declarations scoped
// inside each case block.
void foobar() {
  int result = 0;

  for (int k = 1; k <= 4; ++k) {
    switch (int i = k) {
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
    case 3: // fall through
    default: {
      int fallback = i;
      result = fallback;
      break;
    }
    }
  }

  (void)result;
}
