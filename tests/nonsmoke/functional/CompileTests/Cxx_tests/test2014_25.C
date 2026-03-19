#include "test2014_IntVectSet_support.h"

void IntVectSet::clearStaticMemory() {
  TreeIntVectSet::treeNodePool->clear();
  TreeIntVectSet::index.clear();
  TreeIntVectSet::parents.clear();
  TreeIntVectSet::boxes.clear();
  TreeIntVectSet::bufferOffset.clear();
}
