/*! \brief Test SageBuilder::buildAssignStatement() and buildVarRefExp().
 *
 * Variable references are built only after their declaration is installed in
 * the owning scope, so malformed placeholder symbols are never required.
 */
#include "rose.h"

#include <iostream>
using namespace std;
using namespace SageInterface;
using namespace SageBuilder;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  // go to the function body
  SgFunctionDeclaration *mainFunc = findMain(project);
  cout << mainFunc->unparseToString() << endl;
  SgBasicBlock *body = mainFunc->get_definition()->get_body();
  pushScopeStack(body);
  // int i;
  SgVariableDeclaration *varDecl2 =
      buildVariableDeclaration(SgName("i"), buildIntType(), nullptr, body);

  // insert before the last return statement
  SgStatement *lastStmt = getLastStatement(topScopeStack());
  insertStatement(lastStmt, varDecl2);

  // i=9;
  SgExpression *lhs = buildVarRefExp(string("i"));
  SgExpression *rhs = buildIntVal(9);
  SgExprStatement *assignStmt = buildAssignStatement(lhs, rhs);

  lastStmt = getLastStatement(topScopeStack());
  insertStatement(lastStmt, assignStmt);

  SgVariableDeclaration *varDecl_j =
      buildVariableDeclaration(SgName("j"), buildIntType(), nullptr, body);

  // insert after the first statement
  SgStatement *firstStmt = getFirstStatement(topScopeStack());
  // void   insertStatement (SgStatement *targetStmt, SgStatement *newStmt, bool
  // insertBefore=true, bool autoMovePreprocessingInfo=true)
  insertStatement(firstStmt, varDecl_j, false);

  SgExpression *lhs2 = buildVarRefExp(varDecl_j);
  SgExpression *rhs2 = buildIntVal(-1);
  SgExprStatement *assignStmt2 = buildAssignStatement(lhs2, rhs2);
  lastStmt = getLastStatement(topScopeStack());
  insertStatement(lastStmt, assignStmt2);

  popScopeStack();

  AstTests::runAllTests(project);

  // invoke backend compiler to generate object/binary files
  return backend(project);
}
