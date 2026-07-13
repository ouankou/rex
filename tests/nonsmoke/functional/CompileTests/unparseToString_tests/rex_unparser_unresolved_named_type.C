#include "rose.h"

int main() {
  SgDeclarationScope *scope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(scope != nullptr);
  SageInterface::requireNamedTypeInParentScopes("rex_missing_type", scope);
  return 0;
}
