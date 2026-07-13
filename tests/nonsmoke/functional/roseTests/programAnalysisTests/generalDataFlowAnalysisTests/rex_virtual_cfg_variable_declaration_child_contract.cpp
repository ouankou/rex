#include "rose.h"

#include <string>

namespace {

struct VariableFixture {
  SgVariableDeclaration *declaration;
  SgBoolValExp *requiresClause;
  SgInitializedName *variable;
};

VariableFixture makeVariableFixture() {
  SgVariableDeclaration *declaration = new SgVariableDeclaration();
  ROSE_ASSERT(declaration != nullptr);

  SgBoolValExp *requiresClause = SageBuilder::buildBoolValExp_nfi(1);
  ROSE_ASSERT(requiresClause != nullptr);
  declaration->set_requiresClause(requiresClause);
  requiresClause->set_parent(declaration);

  SgInitializedName *variable = SageBuilder::buildInitializedName_nfi(
      "rex_cfg_variable", SageBuilder::buildIntType(), nullptr, nullptr);
  ROSE_ASSERT(variable != nullptr);
  declaration->get_variables().push_back(variable);
  variable->set_parent(declaration);

  return {declaration, requiresClause, variable};
}

} // namespace

int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "valid";
  VariableFixture fixture = makeVariableFixture();

  if (mode == "valid") {
    const SgNodePtrList successors =
        fixture.declaration->get_traversalSuccessorContainer();
    ROSE_ASSERT(successors.size() == 2);
    ROSE_ASSERT(successors[0] == fixture.requiresClause);
    ROSE_ASSERT(successors[1] == fixture.variable);
    ROSE_ASSERT(fixture.declaration->get_childIndex(fixture.variable) == 1);
    ROSE_ASSERT(fixture.declaration->cfgFindChildIndex(fixture.variable) == 0);
    return 0;
  }

  if (mode == "null") {
    fixture.declaration->cfgFindChildIndex(nullptr);
    return 1;
  }

  if (mode == "foreign") {
    fixture.declaration->cfgFindChildIndex(fixture.requiresClause);
    return 1;
  }

  if (mode == "wrong-parent") {
    SgVariableDeclaration *foreignOwner = new SgVariableDeclaration();
    ROSE_ASSERT(foreignOwner != nullptr);
    fixture.variable->set_parent(foreignOwner);
    fixture.declaration->cfgFindChildIndex(fixture.variable);
    return 1;
  }

  if (mode == "duplicate") {
    fixture.declaration->get_variables().push_back(fixture.variable);
    fixture.declaration->cfgFindChildIndex(fixture.variable);
    return 1;
  }

  return 2;
}
