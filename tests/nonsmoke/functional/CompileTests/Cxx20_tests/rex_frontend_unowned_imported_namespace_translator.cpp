#include "clang-frontend-private.hpp"
#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <clang/AST/ASTContext.h>
#include <clang/Basic/Builtins.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <string>

namespace {
bool hasSuffix(const SgStringList &paths, const std::string &suffix) {
  return std::count_if(paths.begin(), paths.end(),
                       [&](const std::string &path) {
                         return path.size() >= suffix.size() &&
                                path.compare(path.size() - suffix.size(),
                                             suffix.size(), suffix) == 0;
                       }) == 1;
}

void validateUnownedImportedNamespaceClassification() {
  clang::LangOptions language_options;
  clang::DiagnosticOptions diagnostic_options;
  auto diagnostic_ids = llvm::makeIntrusiveRefCnt<clang::DiagnosticIDs>();
  clang::DiagnosticsEngine diagnostics(diagnostic_ids, diagnostic_options);
  clang::FileSystemOptions file_system_options;
  clang::FileManager file_manager(file_system_options);
  clang::SourceManager source_manager(diagnostics, file_manager);
  clang::IdentifierTable identifiers(language_options);
  clang::SelectorTable selectors;
  clang::Builtin::Context builtins;
  clang::ASTContext ast_context(language_options, source_manager, identifiers,
                                selectors, builtins, clang::TU_Complete);
  clang::IdentifierInfo &identifier =
      identifiers.get("rex_synthetic_unowned_imported_namespace");
  clang::NamespaceDecl *namespace_decl = clang::NamespaceDecl::Create(
      ast_context, ast_context.getTranslationUnitDecl(), false,
      clang::SourceLocation(), clang::SourceLocation(), &identifier, nullptr,
      false);
  ROSE_ASSERT(namespace_decl != nullptr);
  namespace_decl->setFromASTFile();
  ROSE_ASSERT(namespace_decl->isFromASTFile());
  ROSE_ASSERT(!namespace_decl->hasOwningModule());
  ROSE_ASSERT(clangFrontendImportedNamespaceModule(namespace_decl) == nullptr);
}
} // namespace

int main(int argc, char **argv) {
  validateUnownedImportedNamespaceClassification();
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgSourceFile *source = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source != nullptr);

  std::size_t namespace_count = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgNamespaceDeclarationStatement)) {
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(node);
    if (declaration == nullptr ||
        declaration->get_name() != "rex_unowned_imported_namespace") {
      continue;
    }
    ++namespace_count;
    SgNamespaceDefinitionStatement *definition = declaration->get_definition();
    ROSE_ASSERT(definition != nullptr);
    ROSE_ASSERT(!declaration->has_source_fragments());
    SgAuxiliaryDeclarationList *owner =
        isSgAuxiliaryDeclarationList(declaration->get_parent());
    ROSE_ASSERT(owner != nullptr);
    ROSE_ASSERT(owner->get_parent() == declaration->get_scope());
    ROSE_ASSERT(declaration->get_file_info() != nullptr);
    ROSE_ASSERT(declaration->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(declaration->get_file_info()->isFrontendSpecific());
    ROSE_ASSERT(definition->get_file_info() != nullptr);
    ROSE_ASSERT(definition->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(definition->get_file_info()->isFrontendSpecific());
  }
  ROSE_ASSERT(namespace_count == 1);
  const SgStringList &externals =
      source->get_frontendExternalOwnershipPathList();
  ROSE_ASSERT(
      hasSuffix(externals, "rex_frontend_unowned_imported_namespace.hpp"));
  return 0;
}
