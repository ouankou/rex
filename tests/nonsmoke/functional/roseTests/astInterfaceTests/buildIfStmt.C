// Liao, 5/1/2008
// Demonstrate how to build a if statement
// The code was originally from Thomas.
#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {

  SgProject *project = frontend(argc, argv);
  SgGlobal *global = getFirstGlobalScope(project);
  pushScopeStack(global);

  SgInitializedName *arg1 =
      buildInitializedName("n", buildPointerType(buildVoidType()));

  // C/C++ has no SgTypeString surface; model the textual buffer as an exact
  // pointer-to-char parameter.
  SgInitializedName *arg2 =
      buildInitializedName("desc", buildPointerType(buildCharType()));

  SgFunctionParameterList *paraList = buildFunctionParameterList();
  appendArg(paraList, arg1);
  appendArg(paraList, arg2);

  // build defining function declaration
  SgFunctionDeclaration *func_def = buildDefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexical(), "check_var",
      buildVoidType(), paraList, global);

  // DQ (11/18/2013): This is an assertion inside of
  // get_declaration_associated_with_symbol() which we are now failing (when
  // called from namequalification support).
  ROSE_ASSERT(func_def->get_firstNondefiningDeclaration() != NULL);
  ROSE_ASSERT(func_def->get_firstNondefiningDeclaration() ==
              func_def->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());

  // Build a corresponding prototype
  // Must not share a parameter list for different function declarations!
  SgFunctionParameterList *paraList2 = buildFunctionParameterList();
  appendArg(paraList2,
            buildInitializedName(SgName("n"), arg1->get_type(), nullptr));
  appendArg(paraList2,
            buildInitializedName(SgName("desc"), arg2->get_type(), nullptr));
  SgFunctionDeclaration *func_decl = buildNondefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexicalAtTop(global),
      SgName("check_var"), buildVoidType(), paraList2, global);

  // DQ (11/18/2013): This is an assertion inside of
  // get_declaration_associated_with_symbol() which we are now failing (when
  // called from namequalification support).
  ROSE_ASSERT(func_decl->get_firstNondefiningDeclaration() != NULL);
  ROSE_ASSERT(func_decl->get_firstNondefiningDeclaration() ==
              func_decl->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());

  // build a statement inside the function body
  SgBasicBlock *func_body = func_def->get_definition()->get_body();
  ROSE_ASSERT(func_body);
  pushScopeStack(func_body);

  SgBasicBlock *true_body = buildBasicBlock();
  SgBasicBlock *false_body = buildBasicBlock();
  SgVarRefExp *op1 = buildVarRefExp("n", isSgScopeStatement(func_body));
  SgExprStatement *conditional =
      buildExprStatement(buildEqualityOp(op1, buildIntVal(0), buildBoolType()));
  SgIfStmt *ifstmt = buildIfStmt(conditional, true_body, false_body);
  appendStatement(ifstmt, func_body);

  popScopeStack();

  // DQ (11/18/2013): This is an assertion inside of
  // get_declaration_associated_with_symbol() which we are now failing (when
  // called from namequalification support).
  ROSE_ASSERT(func_def->get_firstNondefiningDeclaration() ==
              func_def->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());
  ROSE_ASSERT(func_decl->get_firstNondefiningDeclaration() ==
              func_decl->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());

  // DQ (11/18/2013): This is an assertion inside of
  // get_declaration_associated_with_symbol() which we are now failing (when
  // called from namequalification support).
  ROSE_ASSERT(func_def->get_firstNondefiningDeclaration() ==
              func_def->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());
  ROSE_ASSERT(func_decl->get_firstNondefiningDeclaration() ==
              func_decl->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());

  // DQ (11/18/2013): This is an assertion inside of
  // get_declaration_associated_with_symbol() which we are now failing (when
  // called from namequalification support).
  ROSE_ASSERT(func_def->get_firstNondefiningDeclaration() ==
              func_def->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());
  ROSE_ASSERT(func_decl->get_firstNondefiningDeclaration() ==
              func_decl->get_firstNondefiningDeclaration()
                  ->get_firstNondefiningDeclaration());

  popScopeStack();
  AstTests::runAllTests(project);
  project->unparse();

  return 0;
}
