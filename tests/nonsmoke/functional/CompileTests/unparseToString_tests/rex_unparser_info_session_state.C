#include "rose.h"

int main() {
  SgUnparse_Info firstRoot;
  SgUnparse_Info secondRoot;
  SgUnparse_Info inherited(firstRoot);

  auto *typeMarker = reinterpret_cast<SgNamedType *>(&firstRoot);
  inherited.set_extern_C_with_braces(true);
  inherited.set_previousStatementUnparsedFromTokenStream(true);
  inherited.addStructureTag(typeMarker);

  if (!firstRoot.get_extern_C_with_braces() ||
      !firstRoot.get_previousStatementUnparsedFromTokenStream() ||
      firstRoot.getStructureTagList().size() != 1 ||
      firstRoot.getStructureTagList().front() != typeMarker) {
    return 1;
  }

  if (secondRoot.get_extern_C_with_braces() ||
      secondRoot.get_previousStatementUnparsedFromTokenStream() ||
      !secondRoot.getStructureTagList().empty()) {
    return 2;
  }

  SgUnparse_Info sibling(firstRoot);
  sibling.set_extern_C_with_braces(false);
  sibling.set_previousStatementUnparsedFromTokenStream(false);

  if (firstRoot.get_extern_C_with_braces() ||
      firstRoot.get_previousStatementUnparsedFromTokenStream()) {
    return 3;
  }

  return 0;
}
