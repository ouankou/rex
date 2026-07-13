#include "rose.h"

int main() {
  SgBasicBlock *scope = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(scope->get_symbol_table() != nullptr);

  SgInitializedName *initialized_name = SageBuilder::buildInitializedName(
      SgName("rex_alias_target"), SageBuilder::buildIntType());
  ROSE_ASSERT(initialized_name != nullptr);

  SgVariableSymbol *target = new SgVariableSymbol(initialized_name);
  scope->get_symbol_table()->insert(SgName("rex_alias_target"), target);

  SgAliasSymbol *malformed_alias = new SgAliasSymbol(target);
  scope->get_symbol_table()->insert(SgName("rex_alias_without_cause"),
                                    malformed_alias);
  return 0;
}
