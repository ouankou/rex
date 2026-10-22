#include "rose.h"

namespace {
void requireVectorTypes(SgProject *project) {
  bool found = false;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    auto *declaration = isSgTypedefDeclaration(node);
    if (declaration->get_name() != "rex_vector")
      continue;
    auto *type = isSgModifierType(declaration->get_base_type());
    ROSE_ASSERT(type != nullptr && type->get_typeModifier().isVectorType());
    ROSE_ASSERT(type->get_typeModifier().get_vector_size() == 4);
    ROSE_ASSERT(isSgTypeInt(type->get_base_type()) != nullptr);
    ROSE_ASSERT(!type->get_typeModifier().isIdentity());
    ROSE_ASSERT(!SageInterface::isEquivalentType(type, type->get_base_type()));
    ROSE_ASSERT(!SageInterface::isEquivalentType(type->get_base_type(), type));
    found = true;
  }
  ROSE_ASSERT(found);
  bool saw_components = false;
  bool saw_arrow = false;
  bool saw_repeated = false;
  bool selectors[5] = {};
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgVectorElementExp)) {
    auto *access = isSgVectorElementExp(node);
    ROSE_ASSERT(access != nullptr && access->get_operand() != nullptr);
    ROSE_ASSERT(access->get_operand()->get_parent() == access);
    ROSE_ASSERT(access->get_type() != nullptr);
    ROSE_ASSERT(!isSgTypeVoid(access->get_type()));
    ROSE_ASSERT(!access->get_components().empty());
    const auto selector = access->get_selector_kind();
    ROSE_ASSERT(selector >= SgVectorElementExp::e_components &&
                selector <= SgVectorElementExp::e_odd);
    selectors[selector] = true;
    saw_arrow |= access->get_is_arrow();
    if (access->get_is_arrow())
      ROSE_ASSERT(access->isLValue());
    if (access->get_components() == (std::vector<unsigned int>{0, 0})) {
      ROSE_ASSERT(!access->isLValue());
      saw_repeated = true;
    }
    saw_components |=
        access->get_components() == (std::vector<unsigned int>{1, 0});
  }
  ROSE_ASSERT(saw_components && saw_arrow && saw_repeated);
  for (bool present : selectors)
    ROSE_ASSERT(present);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  requireVectorTypes(project);
  auto *copy = isSgProject(SageInterface::deepCopy(project));
  ROSE_ASSERT(copy != nullptr);
  requireVectorTypes(copy);
  SageInterface::deleteAST(copy);
  project->unparse();
  SageInterface::deleteAST(project);
}
