#include "fixupTypeReferences.h"

#include "sage3basic.h"

#include "rose_config.h"

using namespace std;

FixupTypeReferencesOnMemoryPool::~FixupTypeReferencesOnMemoryPool() {}

// REX: Clang frontend doesn't require type reference fixup like legacy frontend
// did This was originally a workaround for legacy frontend type reference
// issues For now, keeping as no-op for compatibility
void FixupTypeReferencesOnMemoryPool::visit(SgNode *node) {
  (void)node; // Suppress unused parameter warning
}

void fixupTypeReferences() {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance fixupTypeReferences("Reset type references:");

  // printf ("Inside of fixupTypeReferences() \n");

  FixupTypeReferencesOnMemoryPool t;

  SgModifierType::traverseMemoryPoolNodes(t);

  // printf ("DONE: Inside of fixupTypeReferences() \n");
}
