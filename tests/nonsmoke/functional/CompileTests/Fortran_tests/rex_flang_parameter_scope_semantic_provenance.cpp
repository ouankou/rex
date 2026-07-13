#include "rose.h"

#include <set>

namespace {

void verifySemanticParameterScope(SgFunctionParameterScope *parameterScope) {
  ROSE_ASSERT(parameterScope != nullptr);
  SgFunctionDeclaration *procedure =
      isSgFunctionDeclaration(parameterScope->get_parent());
  ROSE_ASSERT(procedure != nullptr);
  ROSE_ASSERT(procedure->get_functionParameterScope() == parameterScope);
  ROSE_ASSERT(parameterScope->get_scope() == procedure->get_scope());
  ROSE_ASSERT(parameterScope->get_construction_physical_output_owner() ==
              nullptr);
  ROSE_ASSERT(parameterScope->get_construction_semantic_scope() == nullptr);

  for (Sg_File_Info *position :
       {parameterScope->get_file_info(), parameterScope->get_startOfConstruct(),
        parameterScope->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == parameterScope);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        parameterScope, position));
  }

  Sg_File_Info *ownerPosition = procedure->get_scope()->get_file_info();
  ROSE_ASSERT(ownerPosition != nullptr);
  ROSE_ASSERT(ownerPosition->get_physical_file_id() >= 0);
  for (SgDeclarationStatement *declaration :
       parameterScope->get_declarations()) {
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() == parameterScope);
    ROSE_ASSERT(declaration->get_scope() == parameterScope);
    ROSE_ASSERT(declaration->get_file_info() != nullptr);
    ROSE_ASSERT(declaration->get_file_info()->get_physical_file_id() ==
                ownerPosition->get_physical_file_id());
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  std::set<SgFunctionParameterScope *> parameterScopes;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *procedure = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(procedure != nullptr);
    SgFunctionParameterScope *parameterScope =
        procedure->get_functionParameterScope();
    if (parameterScope != nullptr) {
      parameterScopes.insert(parameterScope);
    }
  }
  ROSE_ASSERT(parameterScopes.size() >= 4);
  for (SgFunctionParameterScope *parameterScope : parameterScopes) {
    verifySemanticParameterScope(parameterScope);
  }

  AstTests::runAllTests(project);
  return 0;
}
