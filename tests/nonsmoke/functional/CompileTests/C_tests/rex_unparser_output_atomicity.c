int rex_unparser_output_atomicity(int value) {
  return value > 0 ? value + 1 : value - 1;
}
