#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t specializations = 0;
  for (SgNode *node : RoseAst(project)) {
    SgMemberFunctionDeclaration *member = isSgMemberFunctionDeclaration(node);
    if (member == nullptr || member->get_name() != "value" ||
        member->get_definition() == nullptr ||
        member->get_specialization() !=
            SgDeclarationStatement::e_specialization) {
      continue;
    }
    ++specializations;
    ROSE_ASSERT(member->get_sourceSpelledTemplateHeaders().size() == 1);
    SgTemplateParameterList *header =
        member->get_sourceSpelledTemplateHeaders().front();
    ROSE_ASSERT(header != nullptr);
    ROSE_ASSERT(header->get_parent() == member);
    ROSE_ASSERT(header->get_args().empty());
  }
  ROSE_ASSERT(specializations == 1);

  AstTests::runAllTests(project);
  return backend(project);
}
