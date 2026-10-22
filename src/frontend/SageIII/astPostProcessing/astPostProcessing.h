#ifndef AST_POST_PROCESSING_H
#define AST_POST_PROCESSING_H

#include "rosedll.h"

class SgNode;

#include "checkPhysicalSourcePosition.h"
#include "detectTransformations.h"
#include "resetParentPointers.h"

// Validate frontend source positions without changing source edit state.
ROSE_DLL_API void AstPostProcessing(SgNode *node);

#endif
