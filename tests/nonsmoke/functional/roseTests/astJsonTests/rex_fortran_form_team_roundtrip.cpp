#include "astJson/sageAstJson.h"
#include "rose.h"

#include <memory>
#include <vector>

static std::vector<SgFormTeamStatement *> verify(SgProject *project) {
  std::vector<SgFormTeamStatement *> forms;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFormTeamStatement)) {
    auto *form = isSgFormTeamStatement(node);
    form->validate_operands();
    ROSE_ASSERT(isSgVarRefExp(form->get_team_number()) != nullptr);
    ROSE_ASSERT(isSgVarRefExp(form->get_team_variable()) != nullptr);
    forms.push_back(form);
  }
  ROSE_ASSERT(forms.size() == 2);
  ROSE_ASSERT(forms[0]->get_new_index() != nullptr);
  ROSE_ASSERT(forms[0]->get_stat() != nullptr);
  ROSE_ASSERT(forms[0]->get_err_msg() != nullptr);
  ROSE_ASSERT(forms[1]->get_new_index() == nullptr);
  ROSE_ASSERT(forms[1]->get_stat() == nullptr);
  ROSE_ASSERT(forms[1]->get_err_msg() == nullptr);
  AstTests::runAllTests(project);
  return forms;
}

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project->get_frontendErrorCode() == 0);
  auto original = verify(project.get());
  std::unique_ptr<SgProject> copy(SageInterface::deepCopy(project.get()));
  auto copied = verify(copy.get());
  ROSE_ASSERT(original[0] != copied[0]);
  ROSE_ASSERT(original[0]->get_team_number() != copied[0]->get_team_number());
  ROSE_ASSERT(original[0]->get_stat() != copied[0]->get_stat());
  SgProject *copied_project = copy.release();
  SgFileList *copied_files = copied_project->get_fileList_ptr();
  SageInterface::deleteAST(copied_project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_project));
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  verify(project.get());
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file->get_processedToIncludeCppDirectivesAndComments());
  file->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(restored != nullptr && restored != file);
  ROSE_ASSERT(restored->get_processedToIncludeCppDirectivesAndComments());
  verify(project.get());
  return backend(project.get());
}
