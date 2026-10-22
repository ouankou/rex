int rex_outliner_signature_contract(int input) {
  int rex_signature_external(int value);
  int left = input + 1;
  int right = input + 2;
  int result = 0;

#pragma rose_outline
  {
    rex_signature_external(input);
    rex_signature_external(left);
    result = left + right;
    left = result + input;
  }

  return left + result;
}
