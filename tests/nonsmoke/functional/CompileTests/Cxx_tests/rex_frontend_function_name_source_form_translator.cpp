#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgFunctionDeclaration *guarded = nullptr;
  SgFunctionDeclaration *ordinary = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    const std::string name = declaration->get_name().getString();
    if (name == "rex_guarded_declaration") {
      guarded = declaration;
    } else if (name == "rex_direct_invoke") {
      ordinary = declaration;
    }
  }

  if (guarded == nullptr || ordinary == nullptr ||
      !guarded->get_source_name_parenthesized_for_macro() ||
      ordinary->get_source_name_parenthesized_for_macro()) {
    fprintf(stderr,
            "REX_TEST_FAILURE[function-name-source-form]: frontend did not "
            "preserve exact guarded and ordinary function-name forms\n");
    return 2;
  }

  AstTests::runAllTests(project);
  return backend(project);
}
