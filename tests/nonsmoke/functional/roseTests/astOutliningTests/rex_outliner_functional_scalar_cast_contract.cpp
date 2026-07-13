int rex_outliner_functional_scalar_cast(double input) {
  int result = 0;
#pragma rose_outline
  {
    result = int(input);
    (void)result;
  }
  return result;
}
