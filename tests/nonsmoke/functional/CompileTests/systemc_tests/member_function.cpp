#include "rose.h"

int main(int argc, char **argv) {
  // Build the AST used by ROSE
  SgProject *project = frontend(argc, argv);

  // Run internal consistency tests on AST
  AstTests::runAllTests(project);

  // get class definition of Foo
  Rose_STL_Container<SgNode *> class_definitions =
      NodeQuery::querySubTree(project, V_SgClassDefinition);
  SgClassDefinition *class_def_foo =
      isSgClassDefinition(class_definitions.at(0));

  // get the defining function declaration of Foo::bar
  SgFunctionDeclaration *func_decl =
      SageInterface::findFunctionDeclaration(class_def_foo, "bar", NULL, true);

  return 0;
}
