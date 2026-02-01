#include "rose.h"

static bool has_two_type_args(const SgTemplateArgumentPtrList &args,
                              const std::string &first,
                              const std::string &second) {
  if (args.size() != 2) {
    return false;
  }
  auto *first_arg = args[0];
  auto *second_arg = args[1];
  if (first_arg == nullptr || second_arg == nullptr) {
    return false;
  }
  if (first_arg->get_argumentType() != SgTemplateArgument::type_argument ||
      second_arg->get_argumentType() != SgTemplateArgument::type_argument) {
    return false;
  }
  std::string first_type = first_arg->get_type()->unparseToString();
  std::string second_type = second_arg->get_type()->unparseToString();
  return first_type.find(first) != std::string::npos &&
         second_type.find(second) != std::string::npos;
}

static bool has_one_type_arg(const SgTemplateArgumentPtrList &args,
                             const std::string &expected) {
  if (args.size() != 1) {
    return false;
  }
  auto *arg = args[0];
  if (arg == nullptr) {
    return false;
  }
  if (arg->get_argumentType() != SgTemplateArgument::type_argument) {
    return false;
  }
  std::string type_str = arg->get_type()->unparseToString();
  return type_str.find(expected) != std::string::npos;
}

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
    if (!has_two_type_args(inst->get_templateArguments(), "double", "int")) {
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
  ROSE_ASSERT(!partial_inst->get_deducedTemplateArguments().empty());
  ROSE_ASSERT(
      has_one_type_arg(partial_inst->get_deducedTemplateArguments(), "double"));

  AstTests::runAllTests(project);
  return backend(project);
}
