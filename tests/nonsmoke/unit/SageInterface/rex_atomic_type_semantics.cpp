#include "rose.h"

#include <map>
#include <string>

namespace {
void requireAtomicTypes(SgProject *project) {
  std::map<std::string, SgType *> types;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    types.emplace(name->get_name().getString(), name->get_type());
  }
  auto atomic = [&](const char *name) {
    auto *type = isSgModifierType(types.at(name));
    ROSE_ASSERT(type != nullptr && type->get_typeModifier().isAtomic());
    ROSE_ASSERT(!type->get_typeModifier().isIdentity());
    ROSE_ASSERT(!SageInterface::isEquivalentType(type, type->get_base_type()));
    return type;
  };
  auto *integer = atomic("rex_atomic_value");
  ROSE_ASSERT(isSgTypeInt(integer->get_base_type()) != nullptr);
  ROSE_ASSERT(types.at("rex_plain_value") != integer);
  ROSE_ASSERT(integer->get_mangled() !=
              types.at("rex_plain_value")->get_mangled());
  auto *pointer = atomic("rex_atomic_pointer");
  ROSE_ASSERT(isSgPointerType(pointer->get_base_type()) != nullptr);
  auto *to_atomic = isSgPointerType(types.at("rex_pointer_to_atomic"));
  ROSE_ASSERT(to_atomic != nullptr);
  ROSE_ASSERT(
      SageInterface::isEquivalentType(to_atomic->get_base_type(), integer));
  ROSE_ASSERT(!SageInterface::isEquivalentType(pointer, to_atomic));
}
} // namespace
int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  requireAtomicTypes(project);
  auto *copy = isSgProject(SageInterface::deepCopy(project));
  ROSE_ASSERT(copy != nullptr);
  requireAtomicTypes(copy);
  SageInterface::deleteAST(copy);
  project->unparse();
  SageInterface::deleteAST(project);
}
