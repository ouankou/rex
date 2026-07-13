#include <rose.h>

#include <algorithm>
#include <filesystem>
#include <string>

#ifndef REX_FLANG_OMP_LIB_PATH
#error "REX_FLANG_OMP_LIB_PATH must identify the exact Flang omp_lib.h input"
#endif
#define REX_STRINGIFY_IMPL(value) #value
#define REX_STRINGIFY(value) REX_STRINGIFY_IMPL(value)

namespace {

std::string normalizePath(const std::filesystem::path &path) {
  const std::string result =
      std::filesystem::weakly_canonical(path).lexically_normal().string();
  ROSE_ASSERT(!result.empty());
  return result;
}

std::string sourcePath(const SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  const std::string startPath = normalizePath(start->get_filenameString());
  const std::string endPath = normalizePath(end->get_filenameString());
  ROSE_ASSERT(startPath == endPath);
  return startPath;
}

SgBasicBlock *findProgramBody(SgProject *project) {
  SgProgramHeaderStatement *canonicalProgram = nullptr;
  SgProgramHeaderStatement *definingProgram = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProgramHeaderStatement)) {
    SgProgramHeaderStatement *program = isSgProgramHeaderStatement(node);
    ROSE_ASSERT(program != nullptr);
    if (program->get_name() != "rex_flang_openmp_runtime_include_semantics") {
      continue;
    }
    if (program->get_definition() != nullptr) {
      ROSE_ASSERT(definingProgram == nullptr);
      definingProgram = program;
    } else {
      ROSE_ASSERT(canonicalProgram == nullptr);
      canonicalProgram = program;
    }
  }
  ROSE_ASSERT(canonicalProgram != nullptr);
  ROSE_ASSERT(definingProgram != nullptr);
  ROSE_ASSERT(canonicalProgram != definingProgram);
  ROSE_ASSERT(canonicalProgram->get_firstNondefiningDeclaration() ==
              canonicalProgram);
  ROSE_ASSERT(canonicalProgram->get_definingDeclaration() == definingProgram);
  ROSE_ASSERT(definingProgram->get_firstNondefiningDeclaration() ==
              canonicalProgram);
  ROSE_ASSERT(definingProgram->get_definingDeclaration() == definingProgram);
  SgFunctionDefinition *definition = definingProgram->get_definition();
  ROSE_ASSERT(definition != nullptr);
  SgBasicBlock *result = definition->get_body();
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verifyRuntimeInclude(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  const std::string expectedPath =
      normalizePath(REX_STRINGIFY(REX_FLANG_OMP_LIB_PATH));

  SgSourceFile *sourceFile = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *candidate = isSgSourceFile(file);
    if (candidate == nullptr) {
      continue;
    }
    const SgStringList &candidatePaths =
        candidate->get_frontendIncludeOwnershipPathList();
    if (std::any_of(candidatePaths.begin(), candidatePaths.end(),
                    [&](const std::string &path) {
                      return normalizePath(path) == expectedPath;
                    })) {
      ROSE_ASSERT(sourceFile == nullptr);
      sourceFile = candidate;
    }
  }
  ROSE_ASSERT(sourceFile != nullptr);
  const SgStringList &includePaths =
      sourceFile->get_frontendIncludeOwnershipPathList();
  ROSE_ASSERT(std::count_if(includePaths.begin(), includePaths.end(),
                            [&](const std::string &path) {
                              return normalizePath(path) == expectedPath;
                            }) == 1);
  const SgStringList &externalPaths =
      sourceFile->get_frontendExternalOwnershipPathList();
  ROSE_ASSERT(std::none_of(externalPaths.begin(), externalPaths.end(),
                           [&](const std::string &path) {
                             return normalizePath(path) == expectedPath;
                           }));

  SgBasicBlock *programBody = findProgramBody(project);
  SgFortranIncludeLine *includeLine = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFortranIncludeLine)) {
    SgFortranIncludeLine *candidate = isSgFortranIncludeLine(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_filename() == "omp_lib.h") {
      ROSE_ASSERT(includeLine == nullptr);
      includeLine = candidate;
    }
  }
  ROSE_ASSERT(includeLine != nullptr);
  ROSE_ASSERT(includeLine->get_parent() == programBody);

  SgVariableDeclaration *kindStatement = nullptr;
  SgInitializedName *kindDeclaration = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgVariableDeclaration)) {
    SgVariableDeclaration *candidate = isSgVariableDeclaration(node);
    ROSE_ASSERT(candidate != nullptr);
    for (SgInitializedName *variable : candidate->get_variables()) {
      ROSE_ASSERT(variable != nullptr);
      if (variable->get_name() == "omp_integer_kind") {
        ROSE_ASSERT(kindStatement == nullptr);
        ROSE_ASSERT(kindDeclaration == nullptr);
        ROSE_ASSERT(sourcePath(candidate) == expectedPath);
        kindStatement = candidate;
        kindDeclaration = variable;
      }
    }
  }
  ROSE_ASSERT(kindStatement != nullptr);
  ROSE_ASSERT(kindDeclaration != nullptr);
  ROSE_ASSERT(kindStatement->get_parent() == programBody);
  ROSE_ASSERT(kindStatement->get_scope() == programBody);
  ROSE_ASSERT(kindDeclaration->get_parent() == kindStatement);
  ROSE_ASSERT(kindDeclaration->get_scope() == programBody);
  SgVariableSymbol *kindSymbol =
      isSgVariableSymbol(kindDeclaration->get_symbol_from_symbol_table());
  ROSE_ASSERT(kindSymbol != nullptr);
  ROSE_ASSERT(kindSymbol->get_declaration() == kindDeclaration);
  ROSE_ASSERT(programBody->lookup_variable_symbol("omp_integer_kind") ==
              kindSymbol);

  std::size_t ownedImportCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *procedure = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(procedure != nullptr);
    SgFunctionParameterScope *scope = procedure->get_functionParameterScope();
    if (scope == nullptr) {
      continue;
    }
    for (SgStatement *statement : scope->generateStatementList()) {
      SgImportStatement *importStatement = isSgImportStatement(statement);
      if (importStatement == nullptr ||
          sourcePath(importStatement) != expectedPath) {
        continue;
      }
      ROSE_ASSERT(importStatement->get_parent() == scope);
      ROSE_ASSERT(importStatement->get_scope() == scope);
      ++ownedImportCount;
    }
  }
  ROSE_ASSERT(ownedImportCount > 0);

  SgFunctionRefExp *callReference = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionRefExp)) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    SgFunctionSymbol *symbol = reference->get_symbol();
    if (symbol == nullptr || symbol->get_name() != "omp_get_max_threads") {
      continue;
    }
    SgFunctionDeclaration *declaration = symbol->get_declaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(sourcePath(declaration) == expectedPath);
    if (SageInterface::getEnclosingNode<SgFunctionCallExp>(reference) !=
        nullptr) {
      ROSE_ASSERT(callReference == nullptr);
      callReference = reference;
    }
  }
  ROSE_ASSERT(callReference != nullptr);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyRuntimeInclude(project);
  AstTests::runAllTests(project);
  return 0;
}
