// Liao, 1/7/2011
//-------------------------------------------------------------------
#include "rose.h"

#include <string>
using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  // grab the scope in which AST will be added
  SgProject *project = frontend(argc, argv);
  //------------------------------------------------
  // bottom up build, no previous knowledge of target scope
  SgLabelStatement *label_stmt_1 =
      buildLabelStatement("", buildFortranContinueStmt());

  SgFunctionDefinition *funcDef = findMain(project)->get_definition();
  ROSE_ASSERT(funcDef);
  SgBasicBlock *body = funcDef->get_body();
  prependStatement(label_stmt_1, body);
  int l_val = suggestNextNumericLabel(funcDef);
  assert(l_val == 10);
  setFortranNumericLabel(label_stmt_1, l_val, SgLabelSymbol::e_start_label_type,
                         funcDef);

  //------------------------------------------------
  // top down set implicit target scope info. in scope stack.
  pushScopeStack(body);
  SgLabelStatement *label_stmt_2 =
      buildLabelStatement("", buildFortranContinueStmt());
  // prependStatement(label_stmt_2, body);
  insertStatementAfter(label_stmt_1, label_stmt_2);
  l_val = suggestNextNumericLabel(funcDef);
  assert(l_val == 20);
  setFortranNumericLabel(label_stmt_2, l_val, SgLabelSymbol::e_start_label_type,
                         funcDef);
  popScopeStack();

  //-------------- if () goto
  SgExprStatement *cond_stmt = buildExprStatement(
      buildEqualityOp(buildIntVal(2), buildIntVal(1), buildBoolType()));
  SgIfStmt *if_stmt =
      buildIfStmt(cond_stmt, buildBasicBlock(), buildBasicBlock());
  appendStatement(if_stmt, body);
  SgGotoStatement *gt_stmt =
      buildGotoStatement(label_stmt_1->get_numeric_label()->get_symbol());
  appendStatement(gt_stmt, isSgScopeStatement(if_stmt->get_true_body()));

  AstTests::runAllTests(project);
  return backend(project);
}
