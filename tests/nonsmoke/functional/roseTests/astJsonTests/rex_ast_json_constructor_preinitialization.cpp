#include "rose.h"

#include <stdexcept>

namespace {

void validateConstructorPreinitializers(SgProject *project) {
  unsigned virtualBases = 0;
  unsigned nonvirtualBases = 0;
  unsigned dataMembers = 0;
  unsigned delegatingConstructors = 0;

  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCtorInitializerList)) {
    SgCtorInitializerList *list = isSgCtorInitializerList(node);
    ROSE_ASSERT(list != nullptr);
    for (SgInitializedName *name : list->get_ctors()) {
      if (name == nullptr || name->get_parent() != list ||
          name->get_initializer() == nullptr ||
          name->get_initializer()->get_parent() != name) {
        throw std::runtime_error(
            "constructor preinitializer has no exact structural owner");
      }
      switch (name->get_preinitialization()) {
      case SgInitializedName::e_virtual_base_class:
        ++virtualBases;
        break;
      case SgInitializedName::e_nonvirtual_base_class:
        ++nonvirtualBases;
        break;
      case SgInitializedName::e_data_member:
        ++dataMembers;
        break;
      case SgInitializedName::e_delegation_constructor:
        ++delegatingConstructors;
        break;
      case SgInitializedName::e_unknown_preinitialization:
      case SgInitializedName::e_last_preinitialization:
      default:
        throw std::runtime_error(
            "constructor preinitializer lost its exact typed role");
      }
    }
  }

  if (virtualBases == 0 || nonvirtualBases == 0 || dataMembers == 0 ||
      delegatingConstructors == 0) {
    throw std::runtime_error(
        "constructor preinitializer role coverage is incomplete");
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);
  validateConstructorPreinitializers(project);
  AstTests::runAllTests(project);
  return backend(project);
}
