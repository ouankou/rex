#include "RoseAst.h"
#include "rose.h"

#include <cstddef>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  std::size_t dependentCallArguments = 0;
  std::size_t dependentIntegralConversions = 0;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateArgument *argument = isSgTemplateArgument(node);
    if (argument == nullptr ||
        argument->get_argumentType() != SgTemplateArgument::nontype_argument) {
      continue;
    }

    SgExpression *expressionRoot = argument->get_expression();
    if (expressionRoot == nullptr) {
      continue;
    }

    if (SgCastExp *conversion = isSgCastExp(expressionRoot)) {
      SgIntVal *sourceLiteral = isSgIntVal(conversion->get_operand());
      if (conversion->get_cast_type() == SgCastExp::e_implicit_cast &&
          conversion->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_Dependent &&
          isSgTemplateType(conversion->get_type()) != nullptr &&
          sourceLiteral != nullptr && sourceLiteral->get_value() == 0) {
        ROSE_ASSERT(argument->get_type() == conversion->get_type());
        ROSE_ASSERT(sourceLiteral->get_type() == SageBuilder::buildIntType());
        ROSE_ASSERT(sourceLiteral->get_parent() == conversion);
        SgIntVal *sourceSurface =
            isSgIntVal(sourceLiteral->get_originalExpressionTree());
        ROSE_ASSERT(sourceSurface != nullptr);
        ROSE_ASSERT(sourceSurface->get_value() == 0);
        ROSE_ASSERT(sourceSurface->get_type() == SageBuilder::buildIntType());
        ROSE_ASSERT(sourceSurface->get_parent() == sourceLiteral);
        ++dependentIntegralConversions;
      }
    }

    Rose_STL_Container<SgNode *> calls =
        NodeQuery::querySubTree(expressionRoot, V_SgFunctionCallExp);
    if (calls.empty()) {
      continue;
    }

    ROSE_ASSERT(calls.size() == 1);
    ROSE_ASSERT(expressionRoot->get_parent() == argument);
    for (SgNode *expressionNode : RoseAst(expressionRoot)) {
      SgExpression *expression = isSgExpression(expressionNode);
      if (expression != nullptr) {
        ROSE_ASSERT(expression->get_originalExpressionTree() == nullptr);
      }
    }
    ++dependentCallArguments;
  }

  ROSE_ASSERT(dependentCallArguments > 0);
  ROSE_ASSERT(dependentIntegralConversions == 1);
  AstTests::runAllTests(project);
  return backend(project);
}
