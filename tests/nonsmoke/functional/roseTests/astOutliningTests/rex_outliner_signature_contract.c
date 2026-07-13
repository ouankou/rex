int rex_outliner_signature_contract(int input) {
  int left = input + 1;
  int right = input + 2;
  int result = 0;

#pragma rose_outline
  {
    result = left + right;
    left = result + input;
  }

  return left + result;
}
