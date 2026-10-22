#include "RoseAst.h"
#include "sage3basic.h"

#include <algorithm>

void SgFunctionCallExp::post_construction_initialization() {
  (void)get_type();
  if (p_args != nullptr &&
      std::any_of(p_args->get_expressions().begin(),
                  p_args->get_expressions().end(), [](SgExpression *argument) {
                    return isSgCxxDefaultInitExp(argument) != nullptr;
                  }))
    ROSE_ASSERT(SgCxxDefaultInitExp::is_valid_binding(
        getAssociatedFunctionDeclarationForCallee(p_function),
        p_args->get_expressions()));
  SgCallExpression::post_construction_initialization();
}

SgType *SgFunctionCallExp::get_type() const {
  return SgCallExpression::get_type();
}

const SgUnsignedCharList &
SgFunctionCallExp::get_source_operator_operand_roles() const {
  return p_source_operator_operand_roles;
}

void SgFunctionCallExp::set_source_operator_operand_roles(
    const SgUnsignedCharList &roles) {
  p_source_operator_operand_roles = roles;
  markSourceMetadataAsModified();
}

const SgUnsignedCharList &
SgFunctionCallExp::get_source_user_defined_literal_suffix_roles() const {
  return p_source_user_defined_literal_suffix_roles;
}

void SgFunctionCallExp::set_source_user_defined_literal_suffix_roles(
    const SgUnsignedCharList &roles) {
  p_source_user_defined_literal_suffix_roles = roles;
  markSourceMetadataAsModified();
}

bool SgFunctionCallExp::get_uses_operator_syntax() const {
  return p_uses_operator_syntax;
}
void SgFunctionCallExp::set_uses_operator_syntax(bool value) {
  p_uses_operator_syntax = value;
  markSourceMetadataAsModified();
}

SgFunctionCallExp::source_syntax_enum
SgFunctionCallExp::get_source_syntax() const {
  if (p_source_syntax == e_implicit_destruction) {
    auto *destructor =
        isSgMemberFunctionDeclaration(getAssociatedFunctionDeclaration());
    ROSE_ASSERT(destructor != nullptr &&
                destructor->get_specialFunctionModifier().isDestructor());
    ROSE_ASSERT(p_args != nullptr && p_args->get_parent() == this &&
                p_args->get_expressions().empty() &&
                isSgTypeVoid(get_type()) != nullptr);
    ROSE_ASSERT(!p_uses_operator_syntax &&
                p_source_operator_surface == e_no_operator_surface);
  }
  return p_source_syntax;
}
void SgFunctionCallExp::set_source_syntax(source_syntax_enum value) {
  p_source_syntax = value;
  markSourceMetadataAsModified();
}

SgFunctionCallExp::source_operator_surface_enum
SgFunctionCallExp::get_source_operator_surface() const {
  return p_source_operator_surface;
}
void SgFunctionCallExp::set_source_operator_surface(
    source_operator_surface_enum value) {
  p_source_operator_surface = value;
  markSourceMetadataAsModified();
}

SgFunctionCallExp::source_operator_callee_form_enum
SgFunctionCallExp::get_source_operator_callee_form() const {
  return p_source_operator_callee_form;
}
void SgFunctionCallExp::set_source_operator_callee_form(
    source_operator_callee_form_enum value) {
  p_source_operator_callee_form = value;
  markSourceMetadataAsModified();
}

SgName SgFunctionCallExp::get_source_user_defined_literal_suffix() const {
  return p_source_user_defined_literal_suffix;
}
void SgFunctionCallExp::set_source_user_defined_literal_suffix(SgName value) {
  p_source_user_defined_literal_suffix = value;
  markSourceMetadataAsModified();
}

SgCxxLiteralTokenList *
SgFunctionCallExp::get_source_user_defined_literal_operands() const {
  return p_source_user_defined_literal_operands;
}

void SgFunctionCallExp::set_source_user_defined_literal_operands(
    SgCxxLiteralTokenList *tokens) {
  if (p_source_user_defined_literal_operands == nullptr) {
    // Publish lexical ownership after the actual call arguments are built.
    // JSON's recorded edited-state flag remains independent of this binding.
    p_source_user_defined_literal_sequence =
        SgNode::get_globalAstModificationSequence();
  } else if (p_source_user_defined_literal_operands != tokens) {
    p_source_user_defined_literal_was_modified = true;
  }
  p_source_user_defined_literal_operands = tokens;
  markSourceMetadataAsModified();
}

bool SgFunctionCallExp::get_source_user_defined_literal_is_modified() const {
  ROSE_ASSERT(p_source_user_defined_literal_operands != nullptr);
  if (p_source_user_defined_literal_was_modified)
    return true;
  for (SgNode *node : RoseAst(const_cast<SgFunctionCallExp *>(this))) {
    if (node->get_lastMutationSequence() >
        p_source_user_defined_literal_sequence)
      return true;
  }
  if (auto *specialization = isSgTemplateInstantiationFunctionDecl(
          const_cast<SgFunctionCallExp *>(this)
              ->getAssociatedFunctionDeclaration())) {
    for (SgTemplateArgument *argument :
         specialization->get_deducedTemplateArguments()) {
      ROSE_ASSERT(argument != nullptr &&
                  argument->get_parent() == specialization);
      for (SgNode *node : RoseAst(argument))
        if (node->get_lastMutationSequence() >
            p_source_user_defined_literal_sequence)
          return true;
    }
  }
  return false;
}
