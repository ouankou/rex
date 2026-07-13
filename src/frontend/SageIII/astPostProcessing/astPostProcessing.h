#ifndef AST_POST_PROCESSING_H
#define AST_POST_PROCESSING_H

#define AST_POST_PROCESSING_VERBOSE_LEVEL DIAGNOSTICS_VERBOSE_LEVEL + 1

class SgNode;

#include "checkIsModifiedFlag.h"
#include "checkPhysicalSourcePosition.h"
#include "detectTransformations.h"
#include "fixupTypes.h"
#include "markLhsValues.h"
#include "propagateHiddenListData.h"
#include "resetParentPointers.h"

// Frontends must provide a complete AST. This boundary performs mutation
// bookkeeping and validation only; it never repairs frontend output.
void postProcessingSupport(SgNode *node);
ROSE_DLL_API void AstPostProcessing(SgNode *node);

#endif
