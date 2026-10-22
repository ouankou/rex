#include "rose.h"

int main() {
  SgDeclarationScope *scope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(scope != nullptr);

  SgTemplateArgumentPtrList arguments;
  SageBuilder::buildSemanticNonrealType(SgName("rex_malformed"), scope,
                                        &arguments);
  return 0;
}
