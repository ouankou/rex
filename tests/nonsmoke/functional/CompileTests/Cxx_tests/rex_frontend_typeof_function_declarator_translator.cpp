#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <array>
#include <string>

namespace {

bool hasExactSemanticProvenance(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }
  const std::array<Sg_File_Info *, 3> positions = {node->get_file_info(),
                                                   node->get_startOfConstruct(),
                                                   node->get_endOfConstruct()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != node ||
        !position->isCompilerGenerated() || !position->isFrontendSpecific() ||
        position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      return false;
    }
  }
  return true;
}

SgFunctionDeclaration *findSourceFunction(SgProject *project,
                                          const std::string &name) {
  SgFunctionDeclaration *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    Sg_File_Info *position =
        candidate != nullptr ? candidate->get_file_info() : nullptr;
    if (candidate == nullptr || candidate->get_name() != name ||
        position == nullptr || position->isCompilerGenerated() ||
        position->isFrontendSpecific()) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void validateTypeofAlias(SgProject *project, const std::string &name,
                         bool semantic_throw_specification) {
  SgFunctionDeclaration *alias = findSourceFunction(project, name);
  ROSE_ASSERT(alias->get_source_declarator_uses_wrapped_function_type());
  alias->validate_source_declarator_form();
  ROSE_ASSERT(alias->get_parameterList_syntax() == nullptr);
  SgFunctionParameterList *parameters = alias->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_parent() == alias);
  ROSE_ASSERT(parameters->get_args().size() == 1);
  ROSE_ASSERT(hasExactSemanticProvenance(parameters));
  ROSE_ASSERT(hasExactSemanticProvenance(parameters->get_args().front()));
  ROSE_ASSERT(alias->get_type_syntax_is_available());
  ROSE_ASSERT(alias->get_type_syntax() != nullptr);
  ROSE_ASSERT(alias->get_type_syntax() != alias->get_type());
  ROSE_ASSERT(alias->get_type_syntax()->get_parent() == alias);
  ROSE_ASSERT(isSgTypeOfType(alias->get_orig_return_type()) != nullptr);
  ROSE_ASSERT(alias->get_declarationModifier().isThrow() ==
              semantic_throw_specification);
}

void validateContracts(SgProject *project) {
  validateTypeofAlias(project, "rex_typeof_expression_alias", false);
  validateTypeofAlias(project, "rex_typeof_nothrow_alias", true);
  validateTypeofAlias(project, "rex_typeof_type_alias", false);
  ROSE_ASSERT(!findSourceFunction(project, "rex_typeof_target")
                   ->get_source_declarator_uses_wrapped_function_type());
}

void roundTrip(SgProject *project) {
  SgSourceFile *source = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *candidate = isSgSourceFile(file);
    if (candidate != nullptr && !candidate->get_isHeaderFile()) {
      ROSE_ASSERT(source == nullptr);
      source = candidate;
    }
  }
  ROSE_ASSERT(source != nullptr);

  using namespace Rose::AstJson;
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(source, checkpoint, source),
                                       checkpointName(checkpoint));
  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  validateContracts(project);
  roundTrip(project);
  validateContracts(project);
  AstTests::runAllTests(project);
  return backend(project);
}
