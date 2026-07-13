#include "rose.h"

#include <algorithm>
#include <array>

namespace {
bool hasExactSourceProvenance(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }
  const std::array<Sg_File_Info *, 3> positions = {node->get_file_info(),
                                                   node->get_startOfConstruct(),
                                                   node->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != node ||
        position->get_line() <= 0 || position->get_col() <= 0 ||
        position->get_physical_file_id() < 0 ||
        position->isCompilerGenerated() || position->isFrontendSpecific() ||
        position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration()) {
      return false;
    }
  }
  return true;
}

class ContractTraversal : public AstSimpleProcessing {
public:
  std::size_t matches = 0;

  void visit(SgNode *node) override {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name().getString() != "RexLocalValue") {
      return;
    }

    SgBasicBlock *body = isSgBasicBlock(declaration->get_parent());
    SgFunctionDefinition *definition =
        body != nullptr ? isSgFunctionDefinition(body->get_parent()) : nullptr;
    SgFunctionDeclaration *function =
        definition != nullptr ? definition->get_declaration() : nullptr;
    if (function == nullptr ||
        function->get_name().getString() != "rex_body_auxiliary_contract") {
      return;
    }

    ROSE_ASSERT(hasExactSourceProvenance(body));
    ROSE_ASSERT(declaration->get_scope() == body);
    ROSE_ASSERT(std::count(body->get_statements().begin(),
                           body->get_statements().end(), declaration) == 1);
    ROSE_ASSERT(hasExactSourceProvenance(declaration));

    SgAuxiliaryDeclarationList *container = body->get_auxiliary_declarations();
    if (container != nullptr) {
      ROSE_ASSERT(container->get_parent() == body);
      ROSE_ASSERT(std::find(container->get_declarations().begin(),
                            container->get_declarations().end(), declaration) ==
                  container->get_declarations().end());
      container->validate_semantic_non_output_role();
    }
    ++matches;
  }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  ContractTraversal contract;
  contract.traverse(project, preorder);
  ROSE_ASSERT(contract.matches == 1);
  return 0;
}
