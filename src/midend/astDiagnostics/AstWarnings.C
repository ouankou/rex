

// DQ (3/6/2003): added from AstProcessing.h to avoid referencing
// the traversal classes in AstFixes.h before they are defined.
#include "roseInternal.h"

#include "sage3basic.h"

#include "AstWarnings.h"

#include <sstream>

//  NodeStatistics Constructors/Destructors
AstWarnings::AstWarnings() {}

AstWarnings::~AstWarnings() {}

void AstWarnings::visit(SgNode *) {}
