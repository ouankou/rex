#ifndef REX_EXPRESSION_TYPE_CLASSIFICATION_H
#define REX_EXPRESSION_TYPE_CLASSIFICATION_H

#include "Cxx_Grammar.h"
#include "ROSE_ASSERT.h"

#include <unordered_set>

namespace Rose {
namespace AST {

inline SgClassType *resolvedClassType(SgType *type,
                                      bool strip_indirection = false) {
  std::unordered_set<SgType *> visited;
  while (true) {
    ASSERT_not_null(type);
    type = strip_indirection ? type->findBaseType()
                             : type->stripTypedefsAndModifiers();
    ROSE_ASSERT(visited.insert(type).second);
    if (auto *record = isSgClassType(type))
      return record;
    auto *use = isSgNonrealType(type);
    if (use == nullptr)
      return nullptr;
    auto *declaration = isSgNonrealDecl(use->get_declaration());
    ASSERT_not_null(declaration);
    auto *target = declaration->get_templateDeclaration();
    if (auto *usingDeclaration = isSgUsingDeclarationStatement(target)) {
      target = usingDeclaration->get_declaration();
      ASSERT_not_null(target);
    }
    if (auto *alias = isSgTypedefDeclaration(target))
      type = alias->get_base_type();
    else if (auto *record = isSgClassDeclaration(target))
      type = record->get_type();
    else
      return nullptr;
  }
}

inline SgClassType *
selectedDestructorClass(SgMemberFunctionDeclaration *destructor) {
  if (destructor == nullptr || !SgNode::isLiveNode(destructor) ||
      !destructor->get_specialFunctionModifier().isDestructor() ||
      destructor->get_functionModifier().isDeleted())
    return nullptr;
  const auto selection =
      destructor->get_functionModifier().getDestructorProperties();
  if (selection != SgFunctionModifier::e_destructor_selected_trivial &&
      selection != SgFunctionModifier::e_destructor_selected_nontrivial)
    return nullptr;
  auto *signature = isSgMemberFunctionType(destructor->get_type());
  auto *owner = signature != nullptr
                    ? isSgClassType(signature->get_class_type())
                    : nullptr;
  auto *declaration = owner != nullptr
                          ? isSgClassDeclaration(owner->get_declaration())
                          : nullptr;
  auto *canonical = declaration != nullptr
                        ? declaration->get_firstNondefiningDeclaration()
                        : nullptr;
  auto *scope = isSgClassDefinition(destructor->get_scope());
  if (owner == nullptr ||
      isSgTypeVoid(signature->get_return_type()) == nullptr ||
      !signature->get_arguments().empty() ||
      signature->get_mfunc_specifier() != 0 || scope == nullptr ||
      scope->get_declaration() == nullptr || canonical == nullptr ||
      scope->get_declaration()->get_firstNondefiningDeclaration() != canonical)
    return nullptr;
  return owner;
}

inline bool
namedTypeScopeHasCrossTranslationUnitIdentity(SgScopeStatement *scope) {
  std::unordered_set<SgScopeStatement *> visited;
  while (true) {
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(visited.insert(scope).second);
    if (isSgGlobal(scope) != nullptr)
      return true;
    if (auto *definition = isSgNamespaceDefinitionStatement(scope)) {
      auto *declaration = definition->get_namespaceDeclaration();
      ASSERT_not_null(declaration);
      if (declaration->get_isUnnamedNamespace())
        return false;
      ROSE_ASSERT(!declaration->get_name().is_null());
      scope = declaration->get_scope();
    } else if (auto *definition = isSgClassDefinition(scope)) {
      auto *declaration = definition->get_declaration();
      ASSERT_not_null(declaration);
      if (declaration->get_isUnNamed())
        return false;
      ROSE_ASSERT(!declaration->get_name().is_null());
      scope = declaration->get_scope();
    } else {
      return false;
    }
  }
}

inline bool temporaryStorageMatchesOwner(
    SgExpression::temporary_storage_duration_enum storage, VariantT scope,
    bool static_storage, bool thread_storage) {
  if (thread_storage)
    return storage == SgExpression::e_temporary_storage_thread;
  if (static_storage || scope == V_SgGlobal ||
      scope == V_SgNamespaceDefinitionStatement)
    return storage == SgExpression::e_temporary_storage_static;
  switch (scope) {
  case V_SgBasicBlock:
  case V_SgFunctionDefinition:
  case V_SgIfStmt:
  case V_SgSwitchStatement:
  case V_SgWhileStmt:
  case V_SgDoWhileStmt:
  case V_SgForStatement:
  case V_SgRangeBasedForStatement:
  case V_SgCatchOptionStmt:
    return storage == SgExpression::e_temporary_storage_automatic;
  default:
    return false;
  }
}

inline bool expressionHasSemanticValueType(VariantT kind,
                                           bool unevaluated_string) {
  switch (kind) {
  case V_SgStringVal:
    return !unevaluated_string;
  case V_SgExprListExp:
  case V_SgCudaKernelExecConfig:
  case V_SgRangeExp:
  case V_SgSubscriptExpression:
  case V_SgDesignator:
  case V_SgAsmOp:
  case V_SgSimpleRequirement:
  case V_SgTypeRequirement:
  case V_SgCompoundRequirement:
  case V_SgRequirementSubstitutionFailure:
  case V_SgNestedRequirement:
  case V_SgTypeExpression:
  case V_SgNullExpression:
  case V_SgFortranCommonBlockRefExp:
  case V_SgAsteriskShapeExp:
  case V_SgColonShapeExp:
  case V_SgAssumedRankExp:
  case V_SgImpliedDo:
  case V_SgCAFImageSelectorExp:
  case V_SgOmpNameExpression:
  case V_SgOmpSourceExpression:
  case V_SgOmpInductionItem:
  case V_SgOmpApplyTransformation:
  case V_SgOmpInitModifier:
  case V_SgOmpInitModifierList:
  case V_SgOmpAppendArgsOperation:
  case V_SgOmpMapDistDataPolicy:
  case V_SgOmpMapItem:
    return false;
  default:
    return true;
  }
}

} // namespace AST
} // namespace Rose

#endif
