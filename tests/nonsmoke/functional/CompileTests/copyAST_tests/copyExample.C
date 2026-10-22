#include "rose.h"

using namespace std;

static void requireExactExplicitInstantiationOwners(SgNode *root) {
  ROSE_ASSERT(root != NULL);
  Rose_STL_Container<SgNode *> directives = NodeQuery::querySubTree(
      root, V_SgTemplateInstantiationDirectiveStatement);
  for (SgNode *node : directives) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    ROSE_ASSERT(directive != NULL);
    SgDeclarationStatement *declaration = directive->get_declaration();
    ROSE_ASSERT(declaration != NULL);
    size_t edges = 0;
    for (const auto &successor : directive->returnDataMemberPointers()) {
      if (successor.first == declaration) {
        ++edges;
      }
    }
    ROSE_ASSERT(declaration->get_parent() == directive);
    ROSE_ASSERT(edges == 1);
  }
}

SgNode *copyAST(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  // Exercise a recursive client-defined copy helper.
  class RestrictedCopyType : public SgCopyHelp {
  public:
    SgNode *copyAst(const SgNode *n) override { return n->copy(*this); }
  } restrictedCopyType;

  SgNode *copyOfNode = node->copy(restrictedCopyType);
  ROSE_ASSERT(copyOfNode != NULL);

  if (SgProject::get_verbose() > 0)
    printf(
        "Running the AST Post Processing Phase on the new copy of the AST! \n");

  AstPostProcessing(copyOfNode);
  requireExactExplicitInstantiationOwners(copyOfNode);

  if (SgProject::get_verbose() > 0)
    printf("DONE: Running the AST Post Processing Phase on the new copy of the "
           "AST! \n");

  if (SgProject::get_verbose() > 0)
    printf("\n\nCompare two generated ASTs ... \n");

  std::vector<SgNode *> intersectionNodeList =
      SageInterface::astIntersection(node, copyOfNode, &restrictedCopyType);

  if (SgProject::get_verbose() > 0)
    printf("DONE: Compare two generated ASTs ... \n");

  ROSE_ASSERT(intersectionNodeList.size() == 0);

  ROSE_ASSERT(copyOfNode != NULL);
  return copyOfNode;
}

int main(int argc, char *argv[]) {
  ios::sync_with_stdio(); // Syncs C++ and C I/O subsystems!

  if (SgProject::get_verbose() > 0)
    printf("In preprocessor.C: main() \n");

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  if (project->get_verbose() > 0)
    printf("\n\nRunning tests on the original AST (before copying) \n");

  AstTests::runAllTests(project);

  if (project->get_verbose() > 0)
    printf("Calling the AST copy mechanism \n");

  SgProject *newProject = static_cast<SgProject *>(copyAST(project));
  ROSE_ASSERT(newProject != NULL);

  AstTests::runAllTests(project);
  if (project->get_verbose() > 0)
    printf("\n\nRunning tests on the copy of the AST \n");

  AstTests::runAllTests(newProject);

  if (project->get_verbose() > 0)
    printf("Calling the backend() \n");

  int errorCode = backend(project);

  if (project->get_verbose() > 0) {
    AstPerformance::generateReport();
  }

  return errorCode;
}
