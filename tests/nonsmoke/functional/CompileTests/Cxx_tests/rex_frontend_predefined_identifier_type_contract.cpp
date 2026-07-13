#include "clang-stmt-contract.hpp"
#include "rose.h"

#include <cstring>

int main(int argc, char **argv) {
  SgType *characterType = SageBuilder::buildCharType();
  SgArrayType *canonicalType =
      new SgArrayType(characterType, SageBuilder::buildIntVal(4));
  SgArrayType *equivalentType =
      new SgArrayType(characterType, SageBuilder::buildIntVal(4));
  ROSE_ASSERT(canonicalType != equivalentType);
  ROSE_ASSERT(SageInterface::isEquivalentType(canonicalType, equivalentType));

  SgInitializedName *name = SageBuilder::buildSemanticInitializedName(
      "__func__", canonicalType, nullptr);
  name->set_is_predefined_identifier(true);

  if (argc == 1) {
    Rose::ClangFrontend::requireExactPredefinedIdentifierType(name,
                                                              canonicalType);
    return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "equivalent-distinct") == 0) {
    Rose::ClangFrontend::requireExactPredefinedIdentifierType(name,
                                                              equivalentType);
  }
  return 2;
}
