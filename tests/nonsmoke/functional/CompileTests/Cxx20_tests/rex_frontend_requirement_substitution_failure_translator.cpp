#include "RoseAst.h"
#include "rose.h"

#include <array>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  std::array<std::size_t, 4> counts{};
  RoseAst ast(project);
  for (SgNode *node : ast) {
    SgRequirementSubstitutionFailure *failure =
        isSgRequirementSubstitutionFailure(node);
    if (failure == nullptr)
      continue;

    const int kind = static_cast<int>(failure->get_failure_kind());
    ROSE_ASSERT(kind >=
                SgRequirementSubstitutionFailure::e_simple_expression_failure);
    ROSE_ASSERT(
        kind <=
        SgRequirementSubstitutionFailure::e_compound_return_type_failure);
    ROSE_ASSERT(!failure->get_substituted_entity().empty());
    ROSE_ASSERT(failure->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(failure->get_startOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(failure->get_startOfConstruct()->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);

    if (failure->get_failure_kind() ==
        SgRequirementSubstitutionFailure::e_compound_return_type_failure) {
      ROSE_ASSERT(isSgCompoundRequirement(failure->get_parent()) != nullptr);
    } else {
      ROSE_ASSERT(isSgExprListExp(failure->get_parent()) != nullptr);
    }
    ++counts[static_cast<std::size_t>(kind)];
  }

  for (std::size_t count : counts)
    ROSE_ASSERT(count == 1);
  return 0;
}
