#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "sage3basic.h"

#include "cfgUtils.h"

namespace cfgUtils {

const SgExpression *unwrapCasts(const SgExpression *expression) {
  if (isSgCastExp(expression))
    return unwrapCasts(isSgCastExp(expression)->get_operand());
  return expression;
}

VirtualCFG::DataflowNode getFuncStartCFG(SgFunctionDefinition *function,
                                         bool (*filter)(VirtualCFG::CFGNode)) {
  return VirtualCFG::DataflowNode(function->cfgForBeginning(), filter);
}

VirtualCFG::DataflowNode getFuncEndCFG(SgFunctionDefinition *function,
                                       bool (*filter)(VirtualCFG::CFGNode)) {
  return VirtualCFG::DataflowNode(function->cfgForEnd(), filter);
}

} // namespace cfgUtils

#endif
