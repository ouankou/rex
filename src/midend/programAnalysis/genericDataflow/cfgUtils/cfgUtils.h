#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef CFGUTILS_H
#define CFGUTILS_H

#include "DataflowCFG.h"

#include "VirtualCFGIterator.h"

#include "variables.h"

class SgFunctionDefinition;

namespace cfgUtils {
// pulls off all the SgCastExps that may be wrapping the given expression,
// returning the expression that is being wrapped
const SgExpression *unwrapCasts(const SgExpression *e);

// returns the DataflowNode that represents that start of the CFG of the given
// function's body
VirtualCFG::DataflowNode
getFuncStartCFG(SgFunctionDefinition *func,
                bool (*f)(VirtualCFG::CFGNode) = VirtualCFG::defaultFilter);

// returns the DataflowNode that represents that end of the CFG of the given
// function's body
VirtualCFG::DataflowNode
getFuncEndCFG(SgFunctionDefinition *func,
              bool (*f)(VirtualCFG::CFGNode) = VirtualCFG::defaultFilter);

} // namespace cfgUtils

#endif
#endif
