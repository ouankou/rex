// Liao 4/22/2011
// test deepCopy on function declarations
#include "rose.h"

#include <stdio.h>
using namespace SageInterface;

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  AstTests::runAllTests(project);

  // Copy a defining function declaration
  SgFunctionDeclaration *func = findDeclarationStatement<SgFunctionDeclaration>(
      project, "bar", NULL, true);
  ROSE_ASSERT(func != NULL);

  printf("func->get_type() = %p = %s \n", func->get_type(),
         func->get_type()->class_name().c_str());

  SgFunctionDeclaration *func_copy =
      isSgFunctionDeclaration(copyStatement(func));
  func_copy->set_name("bar_copy");
  SgGlobal *glb = getFirstGlobalScope(project);
  appendStatement(func_copy, glb);

  // copy a non-defining function declaration

  SgFunctionDeclaration *nfunc =
      findDeclarationStatement<SgFunctionDeclaration>(project, "foo", NULL,
                                                      false);
  ROSE_ASSERT(nfunc != NULL);
  func_copy = isSgFunctionDeclaration(copyStatement(nfunc));
  glb = getFirstGlobalScope(project);
  appendStatement(func_copy, glb);
  // copy another non-defining function declaration

  nfunc = findDeclarationStatement<SgFunctionDeclaration>(project, "bar", NULL,
                                                          false);
  ROSE_ASSERT(nfunc != NULL);
  func_copy = isSgFunctionDeclaration(copyStatement(nfunc));
  glb = getFirstGlobalScope(project);
  appendStatement(func_copy, glb);

  AstTests::runAllTests(project);
  backend(project);
  return 0;
}
