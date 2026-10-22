#include "rose.h"
#include "sageInterface.h"

#include <cstddef>
#include <set>

namespace {

SgTemplateMemberFunctionDeclaration *
canonicalSourceTemplate(SgTemplateMemberFunctionDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgTemplateMemberFunctionDeclaration *canonical =
      isSgTemplateMemberFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  return canonical;
}

struct ReferenceIdentity {
  SgTemplateMemberFunctionDeclaration *source = nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic = nullptr;
  SgFunctionCallExp *call = nullptr;
};

ReferenceIdentity verifyReference(SgTemplateMemberFunctionRefExp *reference) {
  ROSE_ASSERT(reference != nullptr);
  SgTemplateMemberFunctionSymbol *source_symbol = reference->get_symbol();
  SgTemplateMemberFunctionDeclaration *source =
      source_symbol != nullptr ? isSgTemplateMemberFunctionDeclaration(
                                     source_symbol->get_declaration())
                               : nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic =
      isSgTemplateInstantiationMemberFunctionDecl(
          reference->get_semantic_member_function_declaration());
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(semantic != nullptr);
  ROSE_ASSERT(static_cast<SgMemberFunctionDeclaration *>(semantic) !=
              static_cast<SgMemberFunctionDeclaration *>(source));
  ROSE_ASSERT(reference->getAssociatedMemberFunctionDeclaration() == semantic);
  ROSE_ASSERT(reference->get_type() == semantic->get_type());
  ROSE_ASSERT(canonicalSourceTemplate(semantic->get_templateDeclaration()) ==
              canonicalSourceTemplate(source));

  SgExpression *access = isSgExpression(reference->get_parent());
  ROSE_ASSERT(isSgDotExp(access) != nullptr || isSgArrowExp(access) != nullptr);
  SgFunctionCallExp *call = isSgFunctionCallExp(access->get_parent());
  ROSE_ASSERT(call != nullptr);
  ROSE_ASSERT(call->get_function() == access);
  ROSE_ASSERT(call->getAssociatedFunctionDeclaration() == semantic);
  return {source, semantic, call};
}

void verifyCopiedCall(const ReferenceIdentity &original) {
  SgFunctionCallExp *copy =
      isSgFunctionCallExp(SageInterface::copyExpression(original.call));
  ROSE_ASSERT(copy != nullptr);
  auto copied_references =
      NodeQuery::querySubTree(copy, V_SgTemplateMemberFunctionRefExp);
  ROSE_ASSERT(copied_references.size() == 1);
  SgTemplateMemberFunctionRefExp *copied_reference =
      isSgTemplateMemberFunctionRefExp(copied_references.front());
  ReferenceIdentity copied = verifyReference(copied_reference);
  ROSE_ASSERT(copied.source == original.source);
  ROSE_ASSERT(copied.semantic == original.semantic);
  ROSE_ASSERT(copied.call == copy);
  auto *canonical = isSgMemberFunctionDeclaration(
      original.semantic->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  auto *symbol = isSgMemberFunctionSymbol(
      canonical->get_scope()->find_symbol_from_declaration(canonical));
  ROSE_ASSERT(symbol != nullptr);
  auto *sourceArgument = SageBuilder::buildTemplateArgument(
      SageInterface::copyExpression(copied_reference));
  auto *semanticArgument = SageBuilder::buildTemplateArgument(
      SageBuilder::buildSemanticMemberFunctionRefExp(
          symbol, copied_reference->get_virtual_call(),
          copied_reference->get_need_qualifier()));
  ROSE_ASSERT(SageInterface::templateArgumentEquivalence(sourceArgument,
                                                         semanticArgument));
  ROSE_ASSERT(SageInterface::templateArgumentEquivalence(semanticArgument,
                                                         sourceArgument));
  SageInterface::deleteAST(sourceArgument);
  SageInterface::deleteAST(semanticArgument);
  SageInterface::deleteAST(copy);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  using InlineKind = SgVariableDeclaration::inline_kind;
  std::size_t written_inline_variables = 0;
  std::size_t inherited_inline_variables = 0;
  std::size_t constexpr_inline_variables = 0;
  std::size_t written_inline_templates = 0;
  std::size_t instantiated_inline_templates = 0;
  std::size_t written_const_variables = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgVariableDeclaration)) {
    auto *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    const auto name = declaration->get_variables().front()->get_name();
    const auto kind = declaration->get_inline_kind();
    if (name == "rex_macro_explicit_const" ||
        name == "rex_macro_explicit_pointer_const" ||
        name == "rex_attribute_explicit_pointer_const") {
      auto *modifier =
          isSgModifierType(declaration->get_variables().front()->get_type());
      ROSE_ASSERT(modifier != nullptr);
      ROSE_ASSERT(
          modifier->get_typeModifier().get_constVolatileModifier().isConst());
      ++written_const_variables;
    }
    if (name == "rex_written_inline_variable") {
      if (declaration->get_declarationModifier()
              .get_storageModifier()
              .isExtern()) {
        ROSE_ASSERT(kind == InlineKind::implicit);
        ++inherited_inline_variables;
      } else {
        ROSE_ASSERT(kind == InlineKind::written);
        ++written_inline_variables;
      }
    } else if (name == "rex_implicit_inline_variable") {
      ROSE_ASSERT(kind == InlineKind::implicit);
      ++constexpr_inline_variables;
    } else if (name == "rex_inline_template") {
      ROSE_ASSERT(kind != InlineKind::none);
      if (kind == InlineKind::written)
        ++written_inline_templates;
      else
        ++instantiated_inline_templates;
    }
  }
  ROSE_ASSERT(written_inline_variables == 1);
  ROSE_ASSERT(inherited_inline_variables == 1);
  ROSE_ASSERT(constexpr_inline_variables == 1);
  ROSE_ASSERT(written_inline_templates == 3);
  ROSE_ASSERT(instantiated_inline_templates >= 2);
  ROSE_ASSERT(written_const_variables == 3);

  std::set<SgTemplateClassDeclaration *> member_templates;
  std::set<SgScopeStatement *> member_owners;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    auto *instantiation = isSgTemplateInstantiationDecl(node);
    if (instantiation->get_templateName() != "RexNestedSemanticMember") {
      continue;
    }
    auto *owner = isSgTemplateInstantiationDefn(instantiation->get_scope());
    ROSE_ASSERT(owner != nullptr);
    auto *member = instantiation->get_templateDeclaration();
    ROSE_ASSERT(member != nullptr);
    member =
        isSgTemplateClassDeclaration(member->get_firstNondefiningDeclaration());
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_scope() == owner);
    ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(member));
    auto *symbol = owner->find_symbol_from_declaration(member);
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbol->get_symbol_basis() == member);
    member_templates.insert(member);
    member_owners.insert(owner);
  }
  ROSE_ASSERT(member_templates.size() == 2);
  ROSE_ASSERT(member_owners.size() == 2);

  std::set<SgTemplateVariableDeclaration *> variable_templates;
  std::set<SgScopeStatement *> variable_owners;
  std::set<SgScopeStatement *> partial_owners;
  std::size_t external_partials = 0;
  std::size_t partial_redeclarations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    auto *member = isSgTemplateVariableDeclaration(node);
    ROSE_ASSERT(member->get_variables().size() == 1);
    auto *primary = isSgTemplateVariableDeclaration(
        member->get_specializedTemplateDeclaration());
    auto *owner = isSgTemplateInstantiationDefn(member->get_scope());
    auto *member_name = member->get_variables().front();
    if ((member_name->get_name() == "rex_external_partial" ||
         member_name->get_name() == "rex_outofline_partial") &&
        member->get_specialization() ==
            SgDeclarationStatement::e_partial_specialization) {
      auto *semantic_scope = isSgClassDefinition(member_name->get_scope());
      ROSE_ASSERT(semantic_scope != nullptr);
      ROSE_ASSERT(primary != nullptr && primary->get_scope() == semantic_scope);
      auto *canonical = isSgTemplateVariableDeclaration(
          member->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonical != nullptr);
      ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
      if (isSgGlobal(member->get_parent()) != nullptr) {
        ROSE_ASSERT(member->get_scope() == member->get_parent());
        ROSE_ASSERT(!semantic_scope->statementExistsInScope(member));
        ROSE_ASSERT(!member->get_declarationModifier()
                         .get_storageModifier()
                         .isStatic());
        ++external_partials;
      }
      if (member_name->get_prev_decl_item() != nullptr) {
        ROSE_ASSERT(member->get_firstNondefiningDeclaration() ==
                    member_name->get_prev_decl_item()
                        ->get_declaration()
                        ->get_firstNondefiningDeclaration());
        ROSE_ASSERT(semantic_scope->find_symbol_from_declaration(member_name) ==
                    nullptr);
        ++partial_redeclarations;
      } else {
        ROSE_ASSERT(canonical == member);
      }
    }
    if (member->get_variables().front()->get_name() ==
            "rex_nested_partial_value" &&
        member->get_specialization() ==
            SgDeclarationStatement::e_partial_specialization &&
        owner != nullptr) {
      ROSE_ASSERT(primary != nullptr && primary->get_scope() == owner);
      ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(member));
      ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(primary));
      ROSE_ASSERT(member->get_is_constexpr());
      ROSE_ASSERT(
          member->get_declarationModifier().get_storageModifier().isStatic());
      ROSE_ASSERT(member->get_templateParameters().size() == 1);
      ROSE_ASSERT(member->get_variables().front()->get_scope() == owner);
      partial_owners.insert(owner);
    }
    if (member->get_variables().front()->get_name() != "rex_nested_value" ||
        primary == nullptr) {
      continue;
    }
    if (owner == nullptr) {
      continue;
    }
    ROSE_ASSERT(primary->get_scope() == owner);
    ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(primary));
    ROSE_ASSERT(primary->get_variables().size() == 1);
    ROSE_ASSERT(primary->get_variables().front()->get_scope() == owner);
    auto *owner_declaration =
        isSgTemplateInstantiationDecl(owner->get_declaration());
    ROSE_ASSERT(owner_declaration != nullptr);
    ROSE_ASSERT(owner_declaration->get_semanticTemplateArguments().size() == 1);
    ROSE_ASSERT(
        primary->get_variables().front()->get_type()->stripType(
            SgType::STRIP_MODIFIER_TYPE) ==
        owner_declaration->get_semanticTemplateArguments().front()->get_type());
    auto *symbol =
        owner->find_symbol_from_declaration(primary->get_variables().front());
    ROSE_ASSERT(symbol != nullptr);
    variable_templates.insert(primary);
    variable_owners.insert(owner);
  }
  ROSE_ASSERT(variable_templates.size() == 2);
  ROSE_ASSERT(variable_owners == member_owners);
  ROSE_ASSERT(partial_owners == member_owners);
  ROSE_ASSERT(external_partials == 2);
  ROSE_ASSERT(partial_redeclarations == 1);

  auto references =
      NodeQuery::querySubTree(project, V_SgTemplateMemberFunctionRefExp);
  ROSE_ASSERT(references.size() == 2);

  std::size_t explicit_calls = 0;
  std::size_t operator_calls = 0;
  SgTemplateMemberFunctionDeclaration *source = nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic = nullptr;
  for (SgNode *node : references) {
    ReferenceIdentity identity =
        verifyReference(isSgTemplateMemberFunctionRefExp(node));
    if (identity.call->get_uses_operator_syntax()) {
      ++operator_calls;
    } else {
      ++explicit_calls;
    }
    if (source == nullptr) {
      source = identity.source;
      semantic = identity.semantic;
    } else {
      ROSE_ASSERT(canonicalSourceTemplate(identity.source) ==
                  canonicalSourceTemplate(source));
      ROSE_ASSERT(identity.semantic == semantic);
    }
    verifyCopiedCall(identity);
  }
  ROSE_ASSERT(explicit_calls == 1);
  ROSE_ASSERT(operator_calls == 1);

  AstTests::runAllTests(project);
  return backend(project);
}
