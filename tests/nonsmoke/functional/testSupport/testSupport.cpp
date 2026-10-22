#include "testSupport.h"

bool isNull(SgNode *n) { return n == nullptr; }

CleanFunctionTypeTableFixture::CleanFunctionTypeTableFixture() {
  SgFunctionTypeTable *table = SgNode::get_globalFunctionTypeTable();
  assert(table != nullptr);
  table->clear();
  assert(table->size() == 0);
}
