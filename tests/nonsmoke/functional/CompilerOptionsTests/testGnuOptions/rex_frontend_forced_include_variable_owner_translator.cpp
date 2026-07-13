#include "rose.h"

#include "nodeQuery.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
SgSourceFile *findMainSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source == nullptr || source->get_isHeaderFile()) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = source;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

bool listContainsPathSuffix(const SgStringList &paths,
                            const std::string &suffix) {
  return std::count_if(paths.begin(), paths.end(),
                       [&](const std::string &path) {
                         return path.size() >= suffix.size() &&
                                path.compare(path.size() - suffix.size(),
                                             suffix.size(), suffix) == 0;
                       }) == 1;
}

void requireApplicationTextualOwnership(SgSourceFile *source,
                                        const std::string &suffix) {
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(listContainsPathSuffix(
      source->get_frontendIncludeOwnershipPathList(), suffix));
  ROSE_ASSERT(!listContainsPathSuffix(
      source->get_frontendSystemIncludeOwnershipPathList(), suffix));
  ROSE_ASSERT(!listContainsPathSuffix(
      source->get_frontendExternalOwnershipPathList(), suffix));
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *source = findMainSourceFile(project);
  SgGlobal *global = source->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  requireApplicationTextualOwnership(
      source, "rex_frontend_forced_include_variable_owner.hpp");
  requireApplicationTextualOwnership(
      source, "rex_frontend_forced_include_nested_owner.hpp");

  std::vector<SgVariableDeclaration *> matches;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_variables().size() != 1) {
      continue;
    }
    SgInitializedName *initializedName = declaration->get_variables().front();
    if (initializedName != nullptr &&
        initializedName->get_name() ==
            "rex_frontend_forced_include_variable_owner") {
      matches.push_back(declaration);
    }
  }

  ROSE_ASSERT(matches.size() == 1);
  SgVariableDeclaration *declaration = matches.front();
  SgInitializedName *initializedName = declaration->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);

  // The declaration is physically owned by the translation-unit scope even
  // though its first use requests its translation while main's body is active.
  ROSE_ASSERT(declaration->get_parent() == global);
  ROSE_ASSERT(declaration->get_scope() == global);

  // The initialized name and its one symbol retain the exact semantic owner.
  ROSE_ASSERT(initializedName->get_parent() == declaration);
  ROSE_ASSERT(initializedName->get_scope() == global);
  SgVariableSymbol *symbol =
      isSgVariableSymbol(global->find_symbol_from_declaration(initializedName));
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == initializedName);
  ROSE_ASSERT(initializedName->get_symbol_from_symbol_table() == symbol);

  size_t references = 0;
  for (SgNode *node : NodeQuery::querySubTree(source, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    if (reference->get_symbol() != symbol) {
      continue;
    }
    ++references;
    ROSE_ASSERT(SageInterface::getEnclosingFunctionDefinition(reference) !=
                nullptr);
  }
  ROSE_ASSERT(references == 1);

  return 0;
}
