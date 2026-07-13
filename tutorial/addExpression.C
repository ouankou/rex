// Expressions can be built using both bottomup (recommended ) and topdown
// orders. Bottomup: build operands first, operation later Topdown: build
// operation first, set operands later on.

#include "rose.h"
using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  // go to the function body
  SgFunctionDeclaration *mainFunc = findMain(project);

  SgBasicBlock *body = mainFunc->get_definition()->get_body();
  pushScopeStack(body);

  // bottomup: build operands first, create expression later on
  //  double result = 2 * (1 - gama * gama);
  SgExpression *init_exp =
      buildMultiplyOp(buildDoubleVal(2.0),
                      buildSubtractOp(buildDoubleVal(1.0),
                                      buildMultiplyOp(buildVarRefExp("gama"),
                                                      buildVarRefExp("gama"),
                                                      buildDoubleType()),
                                      buildDoubleType()),
                      buildDoubleType());
  SgType *result_type = buildDoubleType();
  SgVariableDeclaration *decl = buildVariableDeclaration(
      "result", result_type, buildAssignInitializer(init_exp, result_type));

  SgStatement *laststmt = getLastStatement(topScopeStack());
  insertStatementBefore(laststmt, decl);

  // topdown: build expression first, set operands later on
  // double result2 = alpha * beta;
  SgExpression *init_exp2 = buildMultiplyOp(
      buildVarRefExp("alpha"), buildVarRefExp("beta"), buildDoubleType());

  SgType *result2_type = buildDoubleType();
  SgVariableDeclaration *decl2 = buildVariableDeclaration(
      "result2", result2_type, buildAssignInitializer(init_exp2, result2_type));
  laststmt = getLastStatement(topScopeStack());
  insertStatementBefore(laststmt, decl2);

  popScopeStack();
  AstTests::runAllTests(project);

  // invoke backend compiler to generate object/binary files
  return backend(project);
}
