#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  bool found_sfinae = false;
  bool found_concept = false;

  std::vector<SgNode *> insts =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);
  for (SgNode *node : insts) {
    SgTemplateInstantiationFunctionDecl *inst =
        isSgTemplateInstantiationFunctionDecl(node);
    if (inst == nullptr) {
      continue;
    }
    std::string tmpl_name = inst->get_templateName().getString();
    if (tmpl_name.empty()) {
      tmpl_name = inst->get_name().getString();
    }
    if (tmpl_name == "sfinae_pick") {
      found_sfinae = true;
      ROSE_ASSERT(inst->get_sfinaeEvaluated());
      ROSE_ASSERT(!inst->get_sfinaeSubstitutionFailure());
    } else if (tmpl_name == "concept_pick") {
      found_concept = true;
      ROSE_ASSERT(inst->get_constraintSatisfactionEvaluated());
      ROSE_ASSERT(inst->get_constraintSatisfactionSatisfied());
      ROSE_ASSERT(!inst->get_constraintSatisfactionContainsErrors());
    }
  }

  ROSE_ASSERT(found_sfinae);
  ROSE_ASSERT(found_concept);

  AstTests::runAllTests(project);
  return backend(project);
}
