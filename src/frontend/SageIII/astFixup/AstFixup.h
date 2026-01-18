#ifndef AST_FIX_UP_H
#define AST_FIX_UP_H

// #define AST_FIXES_VERBOSE_LEVEL DIAGNOSTICS_VERBOSE_LEVEL
#define AST_FIXES_VERBOSE_LEVEL DIAGNOSTICS_VERBOSE_LEVEL + 1

#include "edge_ptr_repl.h"

#include "fixupEnumValues.h"

#include "fixupFriendTemplateDeclarations.h"

#include "fixupFunctionDefinitions.h"

#include "fixupInClassDataInitialization.h"

#include "fixupPrettyFunction.h"

#include "fixupSourcePositionInformation.h"

#include "fixupStorageAccessOfForwardTemplateDeclarations.h"

#include "fixupTemplateDeclarations.h"

#include "fixupforGnuBackendCompiler.h"

#include "removeInitializedNamePtr.h"

// Defined in AstFixup.C
void removeEmptyElses(SgNode *top);

// endif for ifndef AST_FIX_UP_H
#endif
