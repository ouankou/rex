#include "AstTerm.h"

#include "sage3basic.h"

std::string AstTerm::astTermWithNullValuesToString(SgNode *node) {
  if (node == nullptr)
    return "null";

  std::string result = node->class_name();
  const int arity = node->get_numberOfTraversalSuccessors();
  if (arity > 0) {
    result += "(";
    for (int i = 0; i < arity; ++i) {
      if (i != 0)
        result += ",";
      result +=
          astTermWithNullValuesToString(node->get_traversalSuccessorByIndex(i));
    }
    result += ")";
  } else if (auto *expression = isSgExpression(node)) {
    result += ":" + expression->unparseToString();
  }
  return result;
}
