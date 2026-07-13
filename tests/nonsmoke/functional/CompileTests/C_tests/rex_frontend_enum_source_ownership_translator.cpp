#include "rose.h"

#include <algorithm>
#include <string>

namespace {
SgSourceFile *mainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile != nullptr && !sourceFile->get_isHeaderFile()) {
      return sourceFile;
    }
  }
  return nullptr;
}

SgEnumDeclaration *findEnum(SgGlobal *global, const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
    if (enumDeclaration == nullptr || enumDeclaration->get_name() != name) {
      continue;
    }
    if (SgEnumDeclaration *defining =
            isSgEnumDeclaration(enumDeclaration->get_definingDeclaration())) {
      return defining;
    }
    return enumDeclaration;
  }
  return nullptr;
}

bool isBoundaryDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const std::string text = info->getString();
  return text.find("REX_EXTERNAL_ENUMERATOR") != std::string::npos ||
         text.find("rex_unparser_enum_include_entries.def") !=
             std::string::npos ||
         text.find("REX_TERMINAL_EXTERNAL_ENUMERATOR") != std::string::npos ||
         text.find("rex_unparser_enum_terminal_include_entries.def") !=
             std::string::npos;
}
} // namespace

int main(int argc, char **argv) {
  bool corruptSourceRole = false;
  if (argc > 1 && std::string(argv[1]) == "--corrupt-source-role") {
    corruptSourceRole = true;
    for (int index = 1; index + 1 < argc; ++index) {
      argv[index] = argv[index + 1];
    }
    --argc;
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = mainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgEnumDeclaration *enumDeclaration =
      findEnum(sourceFile->get_globalScope(), "RexExternalEnumeratorBoundary");
  ROSE_ASSERT(enumDeclaration != nullptr);
  ROSE_ASSERT(enumDeclaration->get_field_type() != nullptr);
  ROSE_ASSERT(!enumDeclaration->get_underlying_type_source_spelled());
  ROSE_ASSERT(enumDeclaration->get_enumerators().size() == 4);
  enumDeclaration->validate_enumerator_source_ownership();

  size_t directCount = 0;
  size_t externalCount = 0;
  SgInitializedName *directLast = nullptr;
  for (SgInitializedName *enumerator : enumDeclaration->get_enumerators()) {
    ROSE_ASSERT(enumerator != nullptr);
    switch (enumerator->get_enum_constant_source_ownership()) {
    case SgInitializedName::e_enum_constant_source_body:
      ++directCount;
      if (enumerator->get_name() == "rex_enum_direct_last") {
        directLast = enumerator;
      }
      break;
    case SgInitializedName::e_enum_constant_source_external:
      ++externalCount;
      break;
    case SgInitializedName::e_enum_constant_source_unclassified:
    case SgInitializedName::e_enum_constant_semantic_only:
    case SgInitializedName::e_last_enum_constant_source_ownership:
    default:
      ROSE_ABORT();
    }
  }
  ROSE_ASSERT(directCount == 2);
  ROSE_ASSERT(externalCount == 2);
  ROSE_ASSERT(directLast != nullptr);

  AttachedPreprocessingInfoType *directives =
      directLast->getAttachedPreprocessingInfo();
  ROSE_ASSERT(directives != nullptr);
  size_t boundaryDirectiveCount = 0;
  for (PreprocessingInfo *directive : *directives) {
    if (!isBoundaryDirective(directive)) {
      continue;
    }
    ++boundaryDirectiveCount;
    ROSE_ASSERT(directive->getRelativePosition() == PreprocessingInfo::before);
  }
  ROSE_ASSERT(boundaryDirectiveCount == 2);

  AttachedPreprocessingInfoType *enumDirectives =
      enumDeclaration->getAttachedPreprocessingInfo();
  ROSE_ASSERT(enumDirectives == nullptr ||
              std::none_of(enumDirectives->begin(), enumDirectives->end(),
                           isBoundaryDirective));

  SgEnumDeclaration *terminalEnum = findEnum(
      sourceFile->get_globalScope(), "RexTerminalExternalEnumeratorBoundary");
  ROSE_ASSERT(terminalEnum != nullptr);
  ROSE_ASSERT(terminalEnum->get_field_type() != nullptr);
  ROSE_ASSERT(!terminalEnum->get_underlying_type_source_spelled());
  ROSE_ASSERT(terminalEnum->get_enumerators().size() == 3);
  terminalEnum->validate_enumerator_source_ownership();
  ROSE_ASSERT(terminalEnum->get_enumerators()
                  .front()
                  ->get_enum_constant_source_ownership() ==
              SgInitializedName::e_enum_constant_source_body);
  for (size_t index = 1; index < terminalEnum->get_enumerators().size();
       ++index) {
    ROSE_ASSERT(terminalEnum->get_enumerators()[index]
                    ->get_enum_constant_source_ownership() ==
                SgInitializedName::e_enum_constant_source_external);
  }
  AttachedPreprocessingInfoType *terminalDirectives =
      terminalEnum->getAttachedPreprocessingInfo();
  ROSE_ASSERT(terminalDirectives != nullptr);
  boundaryDirectiveCount = 0;
  for (PreprocessingInfo *directive : *terminalDirectives) {
    if (!isBoundaryDirective(directive)) {
      continue;
    }
    ++boundaryDirectiveCount;
    ROSE_ASSERT(directive->getRelativePosition() == PreprocessingInfo::inside);
  }
  ROSE_ASSERT(boundaryDirectiveCount == 2);

  if (corruptSourceRole) {
    enumDeclaration->get_enumerators()
        .front()
        ->set_enum_constant_source_ownership(
            SgInitializedName::e_enum_constant_source_unclassified);
  }
  return backend(project);
}
