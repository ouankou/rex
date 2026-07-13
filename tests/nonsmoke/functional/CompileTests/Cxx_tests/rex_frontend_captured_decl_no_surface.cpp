int rex_captured_region(int value) {
#pragma clang __debug captured
  {
    value += 7;
  }
  return value;
}
