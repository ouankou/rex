#include "rose.h"

#include <algorithm>
#include <set>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t matchingRequiresExpressions = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgRequiresExpr)) {
    SgRequiresExpr *requiresExpression = isSgRequiresExpr(node);
    SgFunctionParameterList *parameters =
        requiresExpression != nullptr
            ? requiresExpression->get_local_parameter_list()
            : nullptr;
    if (parameters == nullptr || parameters->get_args().size() != 2) {
      continue;
    }
    SgFunctionParameterScope *scope =
        requiresExpression->get_local_parameter_scope();
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(scope->get_parent() == requiresExpression);
    ROSE_ASSERT(parameters->get_parent() == requiresExpression);

    std::set<SgInitializedName *> parameterIdentities;
    for (SgInitializedName *parameter : parameters->get_args()) {
      ROSE_ASSERT(parameter != nullptr);
      ROSE_ASSERT(parameter->get_parent() == parameters);
      ROSE_ASSERT(parameter->get_scope() == scope);
      ROSE_ASSERT(scope->find_symbol_from_declaration(parameter) != nullptr);
      ROSE_ASSERT(parameterIdentities.insert(parameter).second);
    }

    size_t exactReferences = 0;
    for (SgNode *referenceNode :
         NodeQuery::querySubTree(requiresExpression, V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(referenceNode);
      SgVariableSymbol *symbol =
          reference != nullptr ? reference->get_symbol() : nullptr;
      SgInitializedName *declaration =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      exactReferences += parameterIdentities.count(declaration);
    }
    // Each of the four requirements has one evaluated reference to each local
    // parameter.  References contained in decltype type operands are not AST
    // expression children and therefore are intentionally outside this query.
    ROSE_ASSERT(exactReferences >= 8);
    ++matchingRequiresExpressions;
  }
  ROSE_ASSERT(matchingRequiresExpressions >= 1);
  AstTests::runAllTests(project);
  return 0;
}
