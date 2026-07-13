#include "rose.h"

#include <algorithm>

namespace {

  void requireSemanticProvenance(SgNonrealDecl *declaration) {
    ROSE_ASSERT(declaration != nullptr);
    for (Sg_File_Info *file_info :
         {declaration->get_file_info(), declaration->get_startOfConstruct(),
          declaration->get_endOfConstruct()}) {
      ROSE_ASSERT(file_info != nullptr);
      ROSE_ASSERT(file_info->get_parent() == declaration);
      ROSE_ASSERT(file_info->isCompilerGenerated());
      ROSE_ASSERT(file_info->isFrontendSpecific());
      ROSE_ASSERT(!file_info->isTransformation());
      ROSE_ASSERT(!file_info->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(file_info->isOutputInCodeGeneration());
      ROSE_ASSERT(file_info->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(file_info->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(file_info->get_source_sequence_number() == 0);
    }
  }

  bool requireSemanticNonrealIdentity(SgNonrealDecl *declaration) {
    SgDeclarationScope *scope = isSgDeclarationScope(declaration->get_parent());
    if (scope == nullptr) {
      return false;
    }

    ROSE_ASSERT(declaration->get_scope() == scope);
    ROSE_ASSERT(std::count(scope->get_declarations().begin(),
                           scope->get_declarations().end(), declaration) == 1);
    ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() == declaration);
    ROSE_ASSERT(declaration->get_definingDeclaration() == nullptr);
    ROSE_ASSERT(!declaration->get_translation_unit_source_order().has_value());
    requireSemanticProvenance(declaration);

    SgNonrealType *type = isSgNonrealType(declaration->get_type());
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(type->get_declaration() == declaration);
    SgDeclarationScope *child_scope =
        SageBuilder::getNonrealDeclarationScope(declaration);
    ROSE_ASSERT(child_scope != nullptr);
    ROSE_ASSERT(child_scope->get_parent() == declaration);

    SgNonrealSymbol *symbol =
        isSgNonrealSymbol(scope->find_symbol_from_declaration(declaration));
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbol->get_declaration() == declaration);
    ROSE_ASSERT(symbol->get_symbol_basis() == declaration);
    ROSE_ASSERT(symbol->get_parent() == scope->get_symbol_table());
    ROSE_ASSERT(scope->get_symbol_table()->exists(symbol));
    ROSE_ASSERT(declaration->get_symbol_from_symbol_table() == symbol);
    return declaration->get_templateDeclaration() != nullptr;
  }

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t semantic_nonreal_count = 0;
  size_t linked_nonreal_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgNonrealDecl)) {
    SgNonrealDecl *declaration = isSgNonrealDecl(node);
    ROSE_ASSERT(declaration != nullptr);
    if (isSgDeclarationScope(declaration->get_parent()) == nullptr) {
      continue;
    }
    ++semantic_nonreal_count;
    linked_nonreal_count += requireSemanticNonrealIdentity(declaration) ? 1 : 0;
  }
  ROSE_ASSERT(semantic_nonreal_count > 0);
  ROSE_ASSERT(linked_nonreal_count > 0);

  return backend(project);
}
