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

  SgFunctionDeclaration *prepended_copy =
      isSgFunctionDeclaration(copyStatement(func));
  ROSE_ASSERT(prepended_copy != NULL && prepended_copy != func);
  ROSE_ASSERT(prepended_copy->get_parent() == NULL);
  prepended_copy->set_name("bar_prepended_copy");
  prependStatement(prepended_copy, glb);
  SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
      prepended_copy->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != NULL);
  ROSE_ASSERT(glb->get_declarations().front() == prepended_copy);
  ROSE_ASSERT(prepended_copy->get_parent() == glb);
  ROSE_ASSERT(canonical->get_parent() == glb->get_auxiliary_declarations());
  ROSE_ASSERT(canonical->get_scope() == glb);
  ROSE_ASSERT(canonical->get_name() == prepended_copy->get_name());
  ROSE_ASSERT(canonical->get_definingDeclaration() == prepended_copy);
  ROSE_ASSERT(glb->find_symbol_from_declaration(canonical) != NULL);

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
