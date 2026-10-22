#include "rose.h"

namespace {
SgTypedefType *requireAlias(SgProject *project) {
  SgTypedefDeclaration *alias = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    auto *declaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(alias == nullptr);
    alias = declaration;
  }
  ROSE_ASSERT(alias != nullptr && alias->get_type() != nullptr);
  ROSE_ASSERT(alias->get_type()->get_declaration() == alias);
  return alias->get_type();
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  SgTypedefType *original = requireAlias(project);
  const size_t types = SgTypedefType::numberOfNodes();
  const size_t declarations = SgTypedefDeclaration::numberOfNodes();
  for (int iteration = 0; iteration < 20; ++iteration) {
    SgTreeCopy help;
    auto *copy = isSgProject(project->copy(help));
    ROSE_ASSERT(copy != nullptr && copy != project);
    SgTypedefType *copied = requireAlias(copy);
    ROSE_ASSERT(copied != original);
    ROSE_ASSERT(help.get_copiedNodeMap().at(original) == copied);
    SageInterface::deleteAST(copy,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(!SgNode::isLiveNode(copy));
    ROSE_ASSERT(!SgNode::isLiveNode(copied));
    ROSE_ASSERT(SgTypedefType::numberOfNodes() == types);
    ROSE_ASSERT(SgTypedefDeclaration::numberOfNodes() == declarations);
    ROSE_ASSERT(requireAlias(project) == original);
    AstTests::runAllTests(project);
  }
  SageInterface::tearDownAst(project);
  return 0;
}
