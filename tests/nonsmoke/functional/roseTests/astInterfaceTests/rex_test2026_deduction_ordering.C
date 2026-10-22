#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgTemplateInstantiationDecl *partial_inst = nullptr;
  std::vector<SgNode *> insts =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl);
  for (SgNode *node : insts) {
    SgTemplateInstantiationDecl *inst = isSgTemplateInstantiationDecl(node);
    if (inst == nullptr) {
      continue;
    }
    std::string tmpl_name = inst->get_templateName().getString();
    if (tmpl_name.empty()) {
      tmpl_name = inst->get_name().getString();
    }
    if (tmpl_name != "Box") {
      continue;
    }
    const auto &arguments = inst->get_semanticTemplateArguments();
    if (arguments.size() != 2 || arguments[0] == nullptr ||
        arguments[1] == nullptr ||
        arguments[0]->get_argumentType() != SgTemplateArgument::type_argument ||
        arguments[1]->get_argumentType() != SgTemplateArgument::type_argument ||
        isSgTypeDouble(arguments[0]->get_type()) == nullptr ||
        isSgTypeInt(arguments[1]->get_type()) == nullptr) {
      continue;
    }
    SgDeclarationStatement *spec = inst->get_specializedTemplateDeclaration();
    if (spec == nullptr) {
      continue;
    }
    if (SgClassDeclaration *spec_class = isSgClassDeclaration(spec)) {
      if (spec_class->get_specialization() ==
          SgDeclarationStatement::e_partial_specialization) {
        partial_inst = inst;
        break;
      }
    }
  }

  ROSE_ASSERT(partial_inst != nullptr);
  ROSE_ASSERT(partial_inst->get_templateArguments().empty());
  for (SgTemplateArgument *argument :
       partial_inst->get_semanticTemplateArguments())
    ROSE_ASSERT(argument->get_parent() == partial_inst);
  const auto &deduced = partial_inst->get_deducedTemplateArguments();
  ROSE_ASSERT(deduced.size() == 1 && deduced[0] != nullptr);
  ROSE_ASSERT(deduced[0]->get_parent() == partial_inst);
  ROSE_ASSERT(deduced[0]->get_argumentType() ==
              SgTemplateArgument::type_argument);
  ROSE_ASSERT(isSgTypeDouble(deduced[0]->get_type()) != nullptr);

  AstTests::runAllTests(project);
  return backend(project);
}
