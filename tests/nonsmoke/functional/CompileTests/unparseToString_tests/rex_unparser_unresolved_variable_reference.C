#include "rose.h"

int main() {
  SgDeclarationScope *scope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(scope != nullptr);
  SageBuilder::buildVarRefExp("rex_missing_variable", scope);
  return 0;
}
