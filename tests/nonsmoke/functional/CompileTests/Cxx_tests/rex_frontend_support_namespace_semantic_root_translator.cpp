#include "rose.h"

#include <algorithm>

namespace {
SgNamespaceDeclarationStatement *findSemanticRoot(SgGlobal *global) {
  ROSE_ASSERT(global != nullptr);
  SgAuxiliaryDeclarationList *auxiliary = global->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == global);

  SgNamespaceDeclarationStatement *semanticRoot = nullptr;
  for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
    SgNamespaceDeclarationStatement *candidate =
        isSgNamespaceDeclarationStatement(declaration);
    if (candidate == nullptr || candidate->get_name() != "std")
      continue;
    ROSE_ASSERT(semanticRoot == nullptr);
    semanticRoot = candidate;
  }
  ROSE_ASSERT(semanticRoot != nullptr);
  return semanticRoot;
}

void requireSemanticProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  for (Sg_File_Info *position :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  }
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);

  SgSourceFile *source = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source != nullptr);
  SgGlobal *global = source->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgAuxiliaryDeclarationList *auxiliary = global->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == global);

  SgNamespaceDeclarationStatement *semantic_root = findSemanticRoot(global);
  ROSE_ASSERT(!semantic_root->has_source_fragments());
  ROSE_ASSERT(semantic_root->get_parent() == auxiliary);
  ROSE_ASSERT(semantic_root->get_scope() == global);
  ROSE_ASSERT(semantic_root->get_firstNondefiningDeclaration() ==
              semantic_root);
  ROSE_ASSERT(semantic_root->get_definition() != nullptr);
  ROSE_ASSERT(semantic_root->get_definition()->get_parent() == semantic_root);
  requireSemanticProvenance(semantic_root);
  requireSemanticProvenance(semantic_root->get_definition());

  SgNamespaceDeclarationStatement *source_reopening = nullptr;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgNamespaceDeclarationStatement *candidate =
        isSgNamespaceDeclarationStatement(declaration);
    if (candidate == nullptr || candidate->get_name() != "std")
      continue;
    ROSE_ASSERT(source_reopening == nullptr);
    source_reopening = candidate;
  }
  ROSE_ASSERT(source_reopening != nullptr);
  ROSE_ASSERT(source_reopening->has_source_fragments());
  source_reopening->validate_source_fragments();
  ROSE_ASSERT(source_reopening->get_parent() == global);
  ROSE_ASSERT(source_reopening->get_scope() == global);
  ROSE_ASSERT(source_reopening->get_firstNondefiningDeclaration() ==
              semantic_root);
  ROSE_ASSERT(source_reopening->get_definition() != nullptr);
  ROSE_ASSERT(source_reopening->get_definition()->get_parent() ==
              source_reopening);
  ROSE_ASSERT(source_reopening->get_definition()->get_global_definition() ==
              semantic_root->get_definition());
  ROSE_ASSERT(
      source_reopening->get_definition()->get_previousNamespaceDefinition() ==
      semantic_root->get_definition());
  ROSE_ASSERT(semantic_root->get_definition()->get_nextNamespaceDefinition() ==
              source_reopening->get_definition());
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         source_reopening) == 1);

  SgNamespaceSymbol *symbol = global->get_symbol_table()->find_namespace("std");
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == semantic_root);
  ROSE_ASSERT(symbol->get_symbol_basis() == semantic_root);

  // Deep-copy the global scope to exercise the symbol construction order for
  // a semantic namespace root plus its lexical reopenings.  Every symbol in
  // the root's union table must be copied exactly once before reference fixup;
  // a non-empty partial table is not a completed copy transaction.
  SgTreeCopy copy_help;
  SgGlobal *copied_global = isSgGlobal(global->copy(copy_help));
  ROSE_ASSERT(copied_global != nullptr);
  ROSE_ASSERT(copied_global != global);
  SgNamespaceDeclarationStatement *copied_semantic_root =
      findSemanticRoot(copied_global);
  ROSE_ASSERT(copied_semantic_root != semantic_root);
  ROSE_ASSERT(copy_help.get_copiedNodeMap().at(semantic_root) ==
              copied_semantic_root);

  SgNamespaceDefinitionStatement *semantic_definition =
      semantic_root->get_definition();
  SgNamespaceDefinitionStatement *copied_semantic_definition =
      copied_semantic_root->get_definition();
  ROSE_ASSERT(semantic_definition != nullptr);
  ROSE_ASSERT(copied_semantic_definition != nullptr);
  ROSE_ASSERT(copied_semantic_definition != semantic_definition);
  ROSE_ASSERT(copy_help.get_copiedNodeMap().at(semantic_definition) ==
              copied_semantic_definition);
  ROSE_ASSERT(copied_semantic_definition->symbol_table_size() ==
              semantic_definition->symbol_table_size());

  SgSymbolTable *semantic_table = semantic_definition->get_symbol_table();
  SgSymbolTable *copied_semantic_table =
      copied_semantic_definition->get_symbol_table();
  ROSE_ASSERT(semantic_table != nullptr);
  ROSE_ASSERT(copied_semantic_table != nullptr);
  for (SgNode *entry : semantic_table->get_symbols()) {
    SgSymbol *semantic_symbol = isSgSymbol(entry);
    ROSE_ASSERT(semantic_symbol != nullptr);
    auto mapped = copy_help.get_copiedNodeMap().find(semantic_symbol);
    ROSE_ASSERT(mapped != copy_help.get_copiedNodeMap().end());
    SgSymbol *copied_symbol = isSgSymbol(mapped->second);
    ROSE_ASSERT(copied_symbol != nullptr);
    ROSE_ASSERT(copied_symbol != semantic_symbol);
    ROSE_ASSERT(copied_symbol->variantT() == semantic_symbol->variantT());
    ROSE_ASSERT(copied_symbol->get_parent() == copied_semantic_table);
    ROSE_ASSERT(copied_semantic_table->exists(copied_symbol));
  }

  return backend(project);
}
