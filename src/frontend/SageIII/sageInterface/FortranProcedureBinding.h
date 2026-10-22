#ifndef ROSE_FORTRAN_PROCEDURE_BINDING_H
#define ROSE_FORTRAN_PROCEDURE_BINDING_H

#include "Cxx_Grammar.h"

#include <algorithm>

namespace SageInterface {
namespace Detail {

inline bool
isExactFortranSemanticProcedurePublication(SgFunctionSymbol *symbol) {
  auto *declaration =
      symbol != nullptr
          ? isSgProcedureHeaderStatement(symbol->get_declaration())
          : nullptr;
  auto *scope = symbol != nullptr ? symbol->get_scope() : nullptr;
  auto *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  auto *owner = declaration != nullptr
                    ? isSgAuxiliaryDeclarationList(declaration->get_parent())
                    : nullptr;
  auto *source =
      declaration != nullptr ? declaration->get_file_info() : nullptr;
  return declaration != nullptr && scope != nullptr && table != nullptr &&
         owner != nullptr && owner->get_parent() == scope &&
         scope->get_auxiliary_declarations() == owner &&
         declaration->get_scope() == scope &&
         declaration->get_firstNondefiningDeclaration() == declaration &&
         declaration->get_fortran_procedure_source_form() ==
             SgProcedureHeaderStatement::
                 e_fortran_procedure_source_form_semantic_only &&
         source != nullptr && source->isCompilerGenerated() &&
         source->isOutputInCodeGeneration() &&
         std::count(owner->get_declarations().begin(),
                    owner->get_declarations().end(), declaration) == 1 &&
         symbol->get_parent() == table && table->exists(symbol) &&
         scope->find_symbol_from_declaration(declaration) == symbol;
}

inline bool hasExactFortranFunctionReferenceBindings(
    SgFunctionSymbol *semantic, SgFunctionSymbol *visible,
    SgFunctionRefExp::fortran_source_visible_binding_kind_enum kind) {
  auto *declaration =
      semantic != nullptr ? semantic->get_declaration() : nullptr;
  auto *scope = declaration != nullptr ? declaration->get_scope() : nullptr;
  auto *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  auto *visibleDeclaration =
      visible != nullptr ? visible->get_declaration() : nullptr;
  auto *visibleScope = visible != nullptr ? visible->get_scope() : nullptr;
  auto *visibleTable =
      visibleScope != nullptr ? visibleScope->get_symbol_table() : nullptr;
  if (declaration == nullptr || visibleDeclaration == nullptr ||
      declaration->get_type() == nullptr ||
      visibleDeclaration->get_type() == nullptr ||
      declaration->get_firstNondefiningDeclaration() != declaration ||
      table == nullptr || table->get_parent() != scope ||
      semantic->get_parent() != table || !table->exists(semantic) ||
      scope->find_symbol_from_declaration(declaration) != semantic ||
      visibleTable == nullptr || visibleTable->get_parent() != visibleScope ||
      visible->get_parent() != visibleTable || !visibleTable->exists(visible))
    return false;

  switch (kind) {
  case SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed:
    return visible == semantic;
  case SgFunctionRefExp::e_fortran_source_visible_binding_use_rename:
  case SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload: {
    auto *rename = isSgRenameSymbol(visible);
    if (rename == nullptr || rename->get_original_symbol() != semantic ||
        rename->get_type() != semantic->get_type())
      return false;
    const auto &causes = rename->get_causal_nodes();
    const bool use =
        std::any_of(causes.begin(), causes.end(), [](SgNode *cause) {
          return isSgUseStatement(cause) != nullptr;
        });
    const bool generic =
        std::find(causes.begin(), causes.end(), declaration) != causes.end();
    const bool validCauses =
        std::all_of(causes.begin(), causes.end(), [declaration](SgNode *cause) {
          return isSgUseStatement(cause) != nullptr || cause == declaration;
        });
    return validCauses &&
           (use ||
            (kind == SgFunctionRefExp::
                         e_fortran_source_visible_binding_generic_overload &&
             generic));
  }
  case SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow:
    return visible != semantic && visible->get_name() == semantic->get_name();
  case SgFunctionRefExp::e_fortran_source_visible_binding_semantic_publication:
    return visible == semantic &&
           isExactFortranSemanticProcedurePublication(semantic);
  case SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable:
    return false;
  }
  return false;
}

} // namespace Detail
} // namespace SageInterface

#endif
