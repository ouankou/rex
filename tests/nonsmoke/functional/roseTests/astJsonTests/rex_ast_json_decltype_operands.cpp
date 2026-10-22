// No class, enum, or typedef may seed the lexical type-copy transaction.
int main() {
  int rex_value = 17;
  decltype(rex_value) rex_plain = 23;
  decltype((rex_value)) rex_reference = rex_value;
  decltype(17) rex_literal = 17;
  __typeof__(17) rex_typeof_literal = 29;
  rex_reference = 31;
  return rex_value != 31 || rex_plain != 23 || rex_literal != 17 ||
         rex_typeof_literal != 29;
}
