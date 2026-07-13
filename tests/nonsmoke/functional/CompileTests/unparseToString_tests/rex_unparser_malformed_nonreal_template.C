#include "rose.h"

int main() {
  SgDeclarationScope *scope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(scope != nullptr);

  SgTemplateArgumentPtrList arguments;
  const SgName malformedSemanticName("rex_malformed");
  SageBuilder::buildSemanticNonrealType(SgName("rex_malformed"), scope,
                                        &arguments, &malformedSemanticName);
  return 0;
}
