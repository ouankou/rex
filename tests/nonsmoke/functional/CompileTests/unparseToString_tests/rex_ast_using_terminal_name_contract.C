#include "rose.h"

#include <cstdlib>
#include <string>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgUsingDeclarationStatement *sourceUsing = nullptr;
  SgUsingDeclarationStatement *inheritingUsing = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgUsingDeclarationStatement)) {
    SgUsingDeclarationStatement *declaration =
        isSgUsingDeclarationStatement(node);
    ROSE_ASSERT(declaration != nullptr);
    const std::string terminal =
        declaration->get_source_terminal_name().getString();
    if (declaration->get_is_inheriting_constructor()) {
      if (terminal == "RexUsingBase") {
        inheritingUsing = declaration;
      }
    } else if (terminal == "value") {
      sourceUsing = declaration;
    }
  }

  if (sourceUsing == nullptr || inheritingUsing == nullptr) {
    fprintf(stderr,
            "The using-terminal fixture has no exact source and inheriting "
            "using declarations.\n");
    return 2;
  }

  const char *malformedMode = std::getenv("REX_TEST_CLEAR_USING_TERMINAL");
  if (malformedMode != nullptr) {
    const std::string mode(malformedMode);
    if (mode == "source") {
      sourceUsing->set_source_terminal_name(SgName());
    } else if (mode == "inheriting") {
      inheritingUsing->set_source_terminal_name(SgName());
    } else if (mode == "compiler-generated") {
      sourceUsing->setCompilerGenerated();
      sourceUsing->set_source_terminal_name(SgName());
    } else {
      fprintf(stderr, "Invalid malformed using-terminal mode: %s\n",
              malformedMode);
      return 3;
    }
  }

  if (!AstTests::isCorrectAst(project)) {
    fprintf(stderr,
            "The using-terminal fixture failed the exact AST consistency "
            "contract.\n");
    ROSE_ABORT();
  }
  return 0;
}
