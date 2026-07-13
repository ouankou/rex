// Liao, 12/8/2010
//-------------------------------------------------------------------
#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  // grab the scope in which AST will be added
  SgProject *project = frontend(argc, argv);

  SgGlobal *globalScope = getFirstGlobalScope(project);
  pushScopeStack(isSgScopeStatement(globalScope));

  //---------------subroutine test1(arg1, arg2)---------
  // build parameter list first
  SgFunctionParameterList *paraList = buildFunctionParameterList();

  // build a Fortran subroutine declaration
  SgProcedureHeaderStatement *func1 = buildProcedureHeaderStatement(
      function_declaration_ownership::sourceLexical(), "TEST1", buildVoidType(),
      paraList, buildSemanticFunctionParameterList(paraList),
      SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
      SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
      globalScope);

  // build a statement inside the function body
  SgBasicBlock *func_body = func1->get_definition()->get_body();
  ROSE_ASSERT(func_body);
  pushScopeStack(isSgScopeStatement(func_body));

  // Build and publish the declaration identity before constructing any use.
  // The source statement can still be inserted after the COMMON block.
  SgIntVal *integerKind = buildIntVal_nfi("4");
  initializeSemanticExpressionSourceProvenance(integerKind);
  SgVariableDeclaration *var_decl = buildVariableDeclaration(
      "x", buildIntType(integerKind), nullptr, func_body);

  // build a common statement
  SgExprListExp *exp_list = buildExprListExp(buildVarRefExp("x", func_body));
  SgCommonBlockObject *cb1 = buildCommonBlockObject("omp_cb1", exp_list);
  SgCommonBlock *stmt = buildCommonBlock(cb1);
  // Insert the statement
  appendStatement(stmt, func_body);

  // Insert the already-published variable used by the common block.
  appendStatement(var_decl, func_body);

  popScopeStack();
  popScopeStack();

  AstTests::runAllTests(project);
  return backend(project);
}
