template <typename DefinitionLeft, typename DefinitionRight>
void rex_exchange(DefinitionLeft &left, DefinitionRight &right) {
  DefinitionLeft temporary = left;
  left = right;
  right = temporary;
}

template <typename RedeclarationLeft, typename RedeclarationRight>
void rex_exchange(RedeclarationLeft &, RedeclarationRight &);

int main() {
  int left = 1;
  int right = 2;
  rex_exchange(left, right);
  return left == 2 && right == 1 ? 0 : 1;
}
