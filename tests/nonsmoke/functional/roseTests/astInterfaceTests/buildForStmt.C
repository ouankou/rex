// Liao, 8/27/2008
// Demonstrate how to build a for statement
/*

   void foo()
  {
    int i,j;
   for (i=0;i++;i<100)
     j++;
  }

 */
#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {

  SgProject *project = frontend(argc, argv);
  SgGlobal *global = getFirstGlobalScope(project);
  pushScopeStack(global);

  SgFunctionParameterList *paraList = buildFunctionParameterList();

  // build defining function declaration, void foo() {}
  SgFunctionDeclaration *func_def = buildDefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexical(), "foo", buildVoidType(),
      paraList, global);
  popScopeStack();

  // build a statement inside the function body
  SgBasicBlock *func_body = func_def->get_definition()->get_body();
  ROSE_ASSERT(func_body);
  pushScopeStack(func_body);
  // int i;
  SgVariableDeclaration *stmt1 =
      buildVariableDeclaration("i", buildIntType(), nullptr, func_body);
  appendStatement(stmt1, func_body);
  // int j;
  SgVariableDeclaration *stmt2 =
      buildVariableDeclaration("j", buildIntType(), nullptr, func_body);
  appendStatement(stmt2, func_body);
  // for(i=0;..)
  SgStatement *init_stmt =
      buildAssignStatement(buildVarRefExp("i"), buildIntVal(0));

  // for(..,i<100,...) It is an expression, not a statement!
  SgExprStatement *cond_stmt = NULL;
  cond_stmt = buildExprStatement(
      buildLessThanOp(buildVarRefExp("i"), buildIntVal(100), buildBoolType()));

  // for (..,;...;i++); not ++i;
  SgExpression *incr_exp = NULL;
  incr_exp =
      buildPlusPlusOp(buildVarRefExp("i"), buildIntType(), SgUnaryOp::postfix);
  // j++; as loop body statement
  SgStatement *loop_body = NULL;
  loop_body = buildExprStatement(
      buildPlusPlusOp(buildVarRefExp("j"), buildIntType(), SgUnaryOp::postfix));

  SgForStatement *for_stmt =
      buildForStatement(init_stmt, cond_stmt, incr_exp, loop_body);
  appendStatement(for_stmt, func_body);
  //-------- build for(int k=0;k<100;k++) j++;
  // Build the declaration-bearing loop scope first, then complete the one
  // typed for-statement transaction after every child has been constructed in
  // that exact scope.
  for_stmt = new SgForStatement(static_cast<SgStatement *>(nullptr),
                                static_cast<SgExpression *>(nullptr),
                                static_cast<SgStatement *>(nullptr));
  beginDetachedForStatementConstruction(for_stmt, func_body);
  SgForInitStatement *for_init = for_stmt->get_for_init_stmt();
  ROSE_ASSERT(for_init != nullptr);
  ROSE_ASSERT(for_init->get_parent() == for_stmt);
  ROSE_ASSERT(for_init->get_init_stmt().empty());
  SgType *loop_index_type = buildIntType();
  SgVariableDeclaration *loop_index_declaration = buildVariableDeclaration(
      "k", loop_index_type,
      buildAssignInitializer(buildIntVal(0), loop_index_type), for_stmt);
  appendStatement(loop_index_declaration, for_init);
  cond_stmt =
      buildExprStatement(buildLessThanOp(buildVarRefExp(loop_index_declaration),
                                         buildIntVal(100), buildBoolType()));
  incr_exp = buildPlusPlusOp(buildVarRefExp(loop_index_declaration),
                             buildIntType(), SgUnaryOp::postfix);
  loop_body = buildExprStatement(buildPlusPlusOp(
      buildVarRefExp(stmt2), buildIntType(), SgUnaryOp::postfix));
  buildForStatement_nfi(for_stmt, for_init, cond_stmt, incr_exp, loop_body);
  completeDetachedForStatementConstruction(for_stmt);
  appendStatement(for_stmt, func_body);
  popScopeStack();

  AstTests::runAllTests(project);
  project->unparse();

  return 0;
}
