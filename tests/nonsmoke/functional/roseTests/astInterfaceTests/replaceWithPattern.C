#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[])

{
  SgProject *project = frontend(argc, argv);
  SgFunctionDeclaration *func = SageInterface::findMain(project);
  ROSE_ASSERT(func != NULL);
  SgBasicBlock *body = func->get_definition()->get_body();
  ROSE_ASSERT(body != NULL);
  Rose_STL_Container<SgNode *> addOps =
      NodeQuery::querySubTree(body, V_SgAddOp);
  ROSE_ASSERT(addOps.size() == 1);

  SgAddOp *add_op = isSgAddOp(addOps[0]);
  ROSE_ASSERT(add_op != NULL);
  SgName s_name("foo");
  SgVariantExpression *variant_exp = buildVariantExpression(add_op->get_type());
  SgExprListExp *parameter_list = buildExprListExp(variant_exp);
  ROSE_ASSERT(variant_exp->get_parent() != NULL);
  SgFunctionCallExp *pattern_exp =
      buildFunctionCallExp(s_name, buildVoidType(), parameter_list, body);
  auto *reference = isSgFunctionRefExp(pattern_exp->get_function());
  ROSE_ASSERT(reference != NULL);
  ROSE_ASSERT(reference->get_symbol() != NULL);
  SgFunctionType *signature =
      reference->get_symbol()->get_declaration()->get_type();
  ROSE_ASSERT(signature->get_argument_list()->get_arguments().size() == 1);
  ROSE_ASSERT(signature->get_argument_list()->get_arguments().front() ==
              add_op->get_type());

  SgNode *result = SageInterface::replaceWithPattern(add_op, pattern_exp);
  ROSE_ASSERT(result == pattern_exp);
  // run all tests
  AstTests::runAllTests(project);

  // Generate source code from AST and call the vendor's compiler
  return backend(project);
}
