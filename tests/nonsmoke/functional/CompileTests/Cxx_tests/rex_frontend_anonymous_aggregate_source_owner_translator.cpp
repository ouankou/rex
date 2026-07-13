#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <cstddef>

namespace {
void requireSourceNode(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  for (Sg_File_Info *fileInfo :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    ROSE_ASSERT(fileInfo != nullptr);
    ROSE_ASSERT(!fileInfo->isCompilerGenerated());
    ROSE_ASSERT(fileInfo->isOutputInCodeGeneration());
  }
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  std::size_t anonymousAggregateOwners = 0;
  for (SgNode *node : RoseAst(project)) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(node);
    if (variable == nullptr || variable->get_variables().size() != 1 ||
        !variable->get_variables().front()->get_name().getString().empty()) {
      continue;
    }

    SgClassDeclaration *definition =
        isSgClassDeclaration(variable->get_baseTypeDefiningDeclaration());
    if (definition == nullptr || !definition->get_isUnNamed()) {
      continue;
    }

    ROSE_ASSERT(definition->get_definingDeclaration() == definition);
    ROSE_ASSERT(definition->get_parent() == variable);
    ROSE_ASSERT(!definition->get_isAutonomousDeclaration());
    ROSE_ASSERT(definition->get_definition() != nullptr);
    ROSE_ASSERT(definition->get_definition()->get_parent() == definition);
    ROSE_ASSERT(variable->get_variables().front()->get_parent() == variable);
    const SgNodePtrList successors =
        variable->get_traversalSuccessorContainer();
    ROSE_ASSERT(std::count(successors.begin(), successors.end(), definition) ==
                1);
    requireSourceNode(variable);
    requireSourceNode(definition);
    requireSourceNode(definition->get_definition());
    ++anonymousAggregateOwners;
  }

  ROSE_ASSERT(anonymousAggregateOwners == 1);
  AstTests::runAllTests(project);
  return backend(project);
}
