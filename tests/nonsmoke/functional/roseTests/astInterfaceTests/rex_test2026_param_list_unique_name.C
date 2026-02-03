#include "rose.h"

#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> funcs =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  SgFunctionDeclaration *target = nullptr;
  for (SgNode *node : funcs) {
    SgFunctionDeclaration *func = isSgFunctionDeclaration(node);
    if (func == nullptr) {
      continue;
    }
    SgFunctionParameterList *params = func->get_parameterList();
    if (params != nullptr && !params->get_args().empty()) {
      target = func;
      break;
    }
  }

  ROSE_ASSERT(target != nullptr);
  SgFunctionParameterList *params = target->get_parameterList();
  ROSE_ASSERT(params != nullptr);

  std::string name =
      SageInterface::generateUniqueNameForUseAsIdentifier_support(params);
  ROSE_ASSERT(!name.empty());
  ROSE_ASSERT(name.find("_params") != std::string::npos);

  std::string mangled = target->get_mangled_name().getString();
  if (!mangled.empty()) {
    ROSE_ASSERT(name.find(mangled) != std::string::npos);
  }

  return 0;
}
