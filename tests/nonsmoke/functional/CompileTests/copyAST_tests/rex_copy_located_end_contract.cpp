#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  if (argc == 2 &&
      std::string(argv[1]) == "--reject-nondefining-class-definition") {
    SgClassDeclaration *declaration =
        new SgClassDeclaration(SgName("rex_malformed_class"),
                               SgClassDeclaration::e_class, nullptr, nullptr);
    ROSE_ASSERT(declaration != nullptr);
    SageInterface::setSourcePosition(declaration);
    declaration->set_firstNondefiningDeclaration(declaration);
    declaration->set_definingDeclaration(nullptr);

    SgClassDefinition *definition = new SgClassDefinition(declaration);
    ROSE_ASSERT(definition != nullptr);
    SageInterface::setSourcePosition(definition);
    ROSE_ASSERT(declaration->get_definition() == definition);
    ROSE_ASSERT(definition->get_parent() == declaration);

    SgTreeCopy copy;
    (void)declaration->copy(copy);
    return 1;
  }

  SgNullStatement *statement = SageBuilder::buildNullStatement_nfi();
  ROSE_ASSERT(statement != nullptr);
  SageInterface::setSourcePosition(statement);
  ROSE_ASSERT(statement->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(statement->get_endOfConstruct() != nullptr);

  if (argc == 2 && std::string(argv[1]) == "--reject-missing-end-position") {
    statement->set_endOfConstruct(nullptr);
    SgTreeCopy copy;
    (void)statement->copy(copy);
    return 1;
  }

  SgTreeCopy copy;
  SgNullStatement *copied = isSgNullStatement(statement->copy(copy));
  ROSE_ASSERT(copied != nullptr);
  ROSE_ASSERT(copied != statement);
  ROSE_ASSERT(copied->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(copied->get_endOfConstruct() != statement->get_endOfConstruct());
  ROSE_ASSERT(copied->get_endOfConstruct()->get_parent() == copied);
  return 0;
}
