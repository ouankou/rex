// Liao, 7/10/2008
// Demonstrate how to build sizeof() expressions
//-------------------------------------------------------------------
#include "rose.h"

#include <string>
using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  // grab the scope in which AST will be added
  SgProject *project = frontend(argc, argv);
  SgGlobal *globalScope = getFirstGlobalScope(project);
  SgType *target_size_type = requireTargetSizeType(globalScope);
  pushScopeStack(globalScope);

  // int j;
  SgVariableDeclaration *varDecl_0 =
      buildVariableDeclaration("j", buildIntType(), nullptr, globalScope);
  appendStatement(varDecl_0, globalScope);

  // int jc = sizeof(j);
  SgType *jc_type = buildIntType();
  SgVariableDeclaration *varDecl_1 = buildVariableDeclaration(
      "jc", jc_type,
      buildAssignInitializer(
          buildSizeOfOp(buildVarRefExp("j"), target_size_type), jc_type),
      globalScope);
  appendStatement(varDecl_1, globalScope);

  // int p = sizeof(int);
  SgType *jp_type = buildIntType();
  SgVariableDeclaration *varDecl_2 = buildVariableDeclaration(
      "jp", jp_type,
      buildAssignInitializer(buildSizeOfOp(buildIntType(), target_size_type),
                             jp_type),
      globalScope);
  appendStatement(varDecl_2, globalScope);

  popScopeStack();

  AstTests::runAllTests(project);
  return backend(project);
}
