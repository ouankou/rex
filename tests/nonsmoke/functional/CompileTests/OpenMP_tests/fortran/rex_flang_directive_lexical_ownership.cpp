#include <rose.h>

#include <cstddef>
#include <string>

namespace {

bool declares(const SgVariableDeclaration *declaration,
              const std::string &name) {
  if (declaration == nullptr) {
    return false;
  }
  for (const SgInitializedName *variable : declaration->get_variables()) {
    if (variable != nullptr && variable->get_name() == name) {
      return true;
    }
  }
  return false;
}

void verifyDirectiveLexicalOwnership(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  SgModuleStatement *canonicalModule = nullptr;
  SgModuleStatement *module = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgModuleStatement)) {
    SgModuleStatement *candidate = isSgModuleStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_name() == "rex_directive_owner_mod") {
      if (candidate->get_definition() != nullptr) {
        ROSE_ASSERT(module == nullptr);
        module = candidate;
      } else {
        ROSE_ASSERT(canonicalModule == nullptr);
        canonicalModule = candidate;
      }
    }
  }
  ROSE_ASSERT(canonicalModule != nullptr);
  ROSE_ASSERT(module != nullptr);
  ROSE_ASSERT(canonicalModule != module);
  ROSE_ASSERT(canonicalModule->get_firstNondefiningDeclaration() ==
              canonicalModule);
  ROSE_ASSERT(canonicalModule->get_definingDeclaration() == module);
  ROSE_ASSERT(module->get_firstNondefiningDeclaration() == canonicalModule);
  ROSE_ASSERT(module->get_definingDeclaration() == module);
  SgClassDefinition *definition = module->get_definition();
  ROSE_ASSERT(definition != nullptr);

  std::size_t beforeIndex = 0;
  std::size_t directiveIndex = 0;
  std::size_t afterIndex = 0;
  bool sawBefore = false;
  bool sawDirective = false;
  bool sawAfter = false;
  const SgStatementPtrList statements = definition->generateStatementList();
  for (std::size_t index = 0; index < statements.size(); ++index) {
    SgStatement *statement = statements[index];
    ROSE_ASSERT(statement != nullptr);
    if (declares(isSgVariableDeclaration(statement), "rex_before")) {
      ROSE_ASSERT(!sawBefore);
      sawBefore = true;
      beforeIndex = index;
    }
    if (SgOmpDeclareTargetStatement *directive =
            isSgOmpDeclareTargetStatement(statement)) {
      ROSE_ASSERT(!sawDirective);
      ROSE_ASSERT(directive->get_parent() == definition);
      ROSE_ASSERT(directive->get_scope() == definition);
      const Sg_File_Info *start = directive->get_startOfConstruct();
      ROSE_ASSERT(start != nullptr);
      ROSE_ASSERT(start->get_filenameString().find(
                      "rex_fortran_openmp_directive_lexical_ownership.inc") !=
                  std::string::npos);
      sawDirective = true;
      directiveIndex = index;
    }
    if (declares(isSgVariableDeclaration(statement), "rex_after")) {
      ROSE_ASSERT(!sawAfter);
      sawAfter = true;
      afterIndex = index;
    }
  }

  ROSE_ASSERT(sawBefore);
  ROSE_ASSERT(sawDirective);
  ROSE_ASSERT(sawAfter);
  ROSE_ASSERT(beforeIndex < directiveIndex);
  ROSE_ASSERT(directiveIndex < afterIndex);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyDirectiveLexicalOwnership(project);
  AstTests::runAllTests(project);
  return 0;
}
