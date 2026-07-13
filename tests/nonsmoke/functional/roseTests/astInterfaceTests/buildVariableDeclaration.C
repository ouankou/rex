// Liao, 1/18/2008
// Demostrate how to build variable declaration statements
// - topdown construction: go to the target scope, build vardecl with scope
// info.
//   - extern int i;
//   - int i;
// Every declaration is built with its exact semantic scope and remains
// structurally detached until explicit insertion.
// SageBuilder contains the AST nodes/subtrees builders
// SageInterface contains any other AST utitily tools
//-------------------------------------------------------------------
#include "rose.h"

#include <string>
using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  // grab the scope in which AST will be added
  SgProject *project = frontend(argc, argv);
  SgGlobal *globalScope = getFirstGlobalScope(project);

  ROSE_ASSERT(globalScope != NULL);

  // DQ (9/28/2009): Tracking down GNU 4.0.x compiler problem!
  // SageBuilder::clearScopeStack();
  // SageBuilder::pushScopeStack (globalScope);

  // volatile int j;
  SgVariableDeclaration *varDecl0 = buildVariableDeclaration(
      "j", buildVolatileType(buildIntType()), nullptr, globalScope);

  // const int jc = 0;
  SgType *varDecl0c_type = buildConstType(buildIntType());
  SgVariableDeclaration *varDecl0c = buildVariableDeclaration(
      "jc", varDecl0c_type,
      buildAssignInitializer(buildIntVal(0), varDecl0c_type), globalScope);

  // int * restrict p;
  SgVariableDeclaration *varDecl0p = buildVariableDeclaration(
      "jp",
      //    buildRestrictType(buildIntType()));
      buildRestrictType(buildPointerType(buildIntType())), nullptr,
      globalScope);

  // top down for others; set implicit target scope info. in scope stack.
  pushScopeStack(isSgScopeStatement(globalScope));

  // extern int i;
  SgVariableDeclaration *varDecl = buildVariableDeclaration(
      SgName("i"), buildIntType(), nullptr, globalScope);
  ((varDecl->get_declarationModifier()).get_storageModifier()).setExtern();
  appendStatement(varDecl, globalScope);
  // two ways to build a same declaration
  // int i;
  SgVariableDeclaration *varDecl2 =
      buildVariableRedeclaration(SgName("i"), buildIntType(), nullptr,
                                 globalScope, varDecl->get_variables().front());

  appendStatement(varDecl2, globalScope);
  insertStatementAfter(varDecl2, varDecl0);
  prependStatement(varDecl0c, globalScope);
  prependStatement(varDecl0p, globalScope);

  popScopeStack();

  AstTests::runAllTests(project);
  return backend(project);
}
