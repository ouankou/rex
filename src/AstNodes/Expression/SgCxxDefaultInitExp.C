#include "sage3basic.h"

#include <algorithm>

bool SgCxxDefaultInitExp::is_valid_use(SgInitializedName *source,
                                       SgExpression *expression,
                                       source_kind_enum kind) {
  if (source == nullptr || expression == nullptr ||
      SageInterface::containsUnknownType(source->get_type()) ||
      !expression->has_semantic_value_type() ||
      SageInterface::containsUnknownType(expression->get_type()))
    return false;
  if (kind == e_member) {
    auto *declaration = isSgVariableDeclaration(source->get_parent());
    auto *scope = isSgClassDefinition(source->get_scope());
    if (declaration == nullptr || scope == nullptr ||
        declaration->get_scope() != scope ||
        source->get_declptr() != declaration ||
        source->get_initializer() == nullptr ||
        declaration->get_declarationModifier().get_storageModifier().isStatic())
      return false;
    return SageInterface::getEnclosingClassDefinition(declaration) == scope &&
           std::count(declaration->get_variables().begin(),
                      declaration->get_variables().end(), source) == 1;
  }
  if (kind != e_parameter)
    return false;
  auto *list = isSgFunctionParameterList(source->get_parent());
  auto *declaration =
      list != nullptr ? isSgFunctionDeclaration(list->get_parent()) : nullptr;
  if (declaration == nullptr)
    return false;
  SgScopeStatement *scope = source->get_scope();
  auto *prototype_scope = declaration->get_functionParameterScope();
  const bool exact_scope =
      scope != nullptr &&
      ((declaration->get_definition() != nullptr &&
        scope == declaration->get_definition()) ||
       (prototype_scope != nullptr && scope == prototype_scope &&
        prototype_scope->get_parent() == declaration) ||
       (list == declaration->get_parameterList_syntax() &&
        list != declaration->get_parameterList() &&
        scope == declaration->get_function_declarator_scope() &&
        scope->get_parent() == declaration) ||
       (declaration->get_definition() == nullptr &&
        prototype_scope == nullptr && source->get_name().getString().empty() &&
        scope == declaration->get_scope()));
  return exact_scope && source->get_declptr() == declaration &&
         (declaration->get_parameterList() == list ||
          declaration->get_parameterList_syntax() == list) &&
         std::count(list->get_args().begin(), list->get_args().end(), source) ==
             1;
}

void SgCxxDefaultInitExp::post_construction_initialization() {
  SgNode::post_construction_initialization();
  ROSE_ASSERT(is_valid_use(p_source_declaration, p_expression, p_source_kind));
  ROSE_ASSERT(p_expression->get_parent() == nullptr);
  p_expression->set_parent(this);
  set_lvalue(p_expression->get_lvalue());
}

bool SgCxxDefaultInitExp::is_valid_binding(SgFunctionDeclaration *declaration,
                                           const SgExpressionPtrList &arguments,
                                           unsigned int first_parameter,
                                           SgType *constructed_type) {
  for (size_t index = 0; index < arguments.size(); ++index) {
    auto *use = isSgCxxDefaultInitExp(arguments[index]);
    if (use == nullptr)
      continue;
    if (use->get_source_kind() == e_member) {
      if (declaration != nullptr || first_parameter != 0 ||
          constructed_type == nullptr ||
          !is_valid_use(use->get_source_declaration(), use->get_expression(),
                        e_member))
        return false;
      auto *scope =
          isSgClassDefinition(use->get_source_declaration()->get_scope());
      if (constructed_type->stripTypedefsAndModifiers() !=
          scope->get_declaration()->get_type())
        return false;
      continue;
    }
    if (declaration == nullptr || use->get_source_kind() != e_parameter ||
        !is_valid_use(use->get_source_declaration(), use->get_expression()))
      return false;
    auto *source = use->get_source_declaration();
    auto *owner = isSgFunctionDeclaration(source->get_declptr());
    auto *list = isSgFunctionParameterList(source->get_parent());
    if (owner->get_firstNondefiningDeclaration() == nullptr ||
        owner->get_firstNondefiningDeclaration() !=
            declaration->get_firstNondefiningDeclaration() ||
        first_parameter > list->get_args().size() ||
        index >= list->get_args().size() - first_parameter ||
        list->get_args()[first_parameter + index] != source)
      return false;
  }
  return true;
}

void SgCxxDefaultInitExp::validate() const {
  ROSE_ASSERT(is_valid_use(p_source_declaration, p_expression, p_source_kind));
  ROSE_ASSERT(p_expression->get_parent() == this);
  ROSE_ASSERT(get_lvalue() == p_expression->get_lvalue());
}

SgType *SgCxxDefaultInitExp::get_type() const {
  validate();
  return p_expression->get_type();
}

int SgCxxDefaultInitExp::replace_expression(SgExpression *original,
                                            SgExpression *replacement) {
  validate();
  ROSE_ASSERT(original == p_expression && replacement != nullptr);
  if (original == replacement)
    return 1;
  ROSE_ASSERT(replacement->get_parent() == nullptr);
  ROSE_ASSERT(is_valid_use(p_source_declaration, replacement, p_source_kind));
  p_expression = replacement;
  replacement->set_parent(this);
  original->set_parent(nullptr);
  set_lvalue(replacement->get_lvalue());
  return 1;
}
