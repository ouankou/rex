#include "sage3basic.h"

void SgNewExp::post_construction_initialization() {
  ROSE_ASSERT(p_allocation_implicit_argument_count >= 1 &&
              p_allocation_implicit_argument_count <= 3);
  ROSE_ASSERT(p_placement_args == nullptr ||
              SgCxxDefaultInitExp::is_valid_binding(
                  p_newOperatorDeclaration, p_placement_args->get_expressions(),
                  p_allocation_implicit_argument_count));
  ROSE_ASSERT(p_constructor_args == nullptr || p_builtin_args == nullptr);
  ROSE_ASSERT(specifiedTypeMatchesResult(p_specified_type, p_expression_type,
                                         p_array_bound_source_form));
  ROSE_ASSERT((p_array_bound == nullptr) ==
              (p_array_bound_source_form == e_no_array_bound));
  if (p_array_bound_source_form == e_inferred_array_bound ||
      p_array_bound_source_form == e_type_array_bound) {
    auto *value = isSgValueExp(p_array_bound);
    ROSE_ASSERT(value != nullptr && value->get_type()->isIntegerType() &&
                value->get_literal_spelling_form() ==
                    SgValueExp::e_literal_canonical_generated &&
                value->get_file_info() != nullptr &&
                value->get_file_info()->isCompilerGenerated());
  }
  SgExpression *operands[] = {p_array_bound, p_constructor_args, p_builtin_args,
                              p_placement_args};
  for (unsigned i = 0; i < 4; ++i) {
    SgExpression *operand = operands[i];
    ROSE_ASSERT(operand == nullptr || operand->get_parent() == nullptr ||
                operand->get_parent() == this);
    for (unsigned j = 0; j < i; ++j)
      ROSE_ASSERT(operand == nullptr || operand != operands[j]);
  }

  if (p_constructor_args) {
    // The constructor initializer owns the exact new-initializer delimiters
    // through need_parenthesis_after_name/is_braced_initialized.  Generic
    // expression grouping belongs on the enclosing SgNewExp (for example,
    // `(new T())`), never on its constructor_args edge.  Setting need_paren
    // here used to make the unparser emit `new T(())`.
    if (p_constructor_args->get_need_paren()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[new-constructor-source-form]: constructor "
              "initializer cannot own generic expression parentheses\n");
      ROSE_ABORT();
    }
  }

  for (SgExpression *operand : operands)
    if (operand != nullptr)
      operand->set_parent(this);
}

SgExpression *SgNewExp::get_next(int &n) const {
  if (n == 0) {
    n++;
    if (get_placement_args())
      return get_placement_args();
  }
  if (n == 1) {
    n++;
    if (get_array_bound())
      return get_array_bound();
  }
  if (n == 2) {
    n++;
    if (get_constructor_args())
      return get_constructor_args();
    return get_builtin_args();
  }

  return NULL;
}

int SgNewExp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  const int edges = (get_placement_args() == o) + (get_array_bound() == o) +
                    (get_constructor_args() == o) + (get_builtin_args() == o);
  if (edges == 0)
    return 0;
  ROSE_ASSERT(edges == 1);
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;
  ROSE_ASSERT(n->get_parent() == nullptr);

  if (get_placement_args() == o) {
    auto *placement = isSgExprListExp(n);
    ROSE_ASSERT(placement != nullptr);
    set_placement_args(placement);
  } else if (get_array_bound() == o) {
    if (get_array_bound_source_form() == e_inferred_array_bound ||
        get_array_bound_source_form() == e_type_array_bound) {
      auto *value = isSgValueExp(n);
      ROSE_ASSERT(value != nullptr && value->get_type()->isIntegerType() &&
                  value->get_literal_spelling_form() ==
                      SgValueExp::e_literal_canonical_generated &&
                  value->get_file_info() != nullptr &&
                  value->get_file_info()->isCompilerGenerated());
    }
    set_array_bound(n);
  } else if (get_constructor_args() == o) {
    auto *constructor = isSgConstructorInitializer(n);
    ROSE_ASSERT(constructor != nullptr);
    ROSE_ASSERT(!constructor->get_need_paren());
    set_constructor_args(constructor);
  } else if (get_builtin_args() == o) {
    set_builtin_args(n);
  }
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
}

SgType *SgNewExp::get_type() const {
  ROSE_ASSERT(p_expression_type != nullptr);
  return p_expression_type;
}

namespace {
// [expr.new]p2: a placeholder in a new-type-id is deduced from the
// initializer.  The written declarator around it must match the deduced
// allocated type; the placeholder itself stands for the deduced component.
bool containsNewTypePlaceholder(SgType *type) {
  while (type != nullptr) {
    if (isSgAutoType(type) != nullptr)
      return true;
    if (auto *modifier = isSgModifierType(type))
      type = modifier->get_base_type();
    else if (auto *pointer = isSgPointerType(type))
      type = pointer->get_base_type();
    else
      return false;
  }
  return false;
}

bool newTypePlaceholderMatchesDeduced(SgType *specified, SgType *deduced) {
  if (specified == nullptr || deduced == nullptr)
    return false;
  if (isSgAutoType(specified) != nullptr)
    return true;
  if (auto *modifier = isSgModifierType(specified)) {
    auto *deduced_modifier = isSgModifierType(deduced);
    return deduced_modifier != nullptr &&
           modifier->get_typeModifier() ==
               deduced_modifier->get_typeModifier() &&
           newTypePlaceholderMatchesDeduced(modifier->get_base_type(),
                                            deduced_modifier->get_base_type());
  }
  if (auto *pointer = isSgPointerType(specified)) {
    auto *deduced_pointer = isSgPointerType(deduced);
    return isSgPointerMemberType(pointer) == nullptr &&
           deduced_pointer != nullptr &&
           isSgPointerMemberType(deduced_pointer) == nullptr &&
           newTypePlaceholderMatchesDeduced(pointer->get_base_type(),
                                            deduced_pointer->get_base_type());
  }
  return false;
}
} // namespace

bool SgNewExp::specifiedTypeMatchesResult(
    SgType *specified, SgPointerType *result,
    array_bound_source_form_enum source_form) {
  if (SageInterface::containsUnknownType(specified) || result == nullptr ||
      result->get_base_type() == nullptr ||
      result->get_base_type()->get_ptr_to() != result ||
      source_form < e_no_array_bound || source_form > e_type_array_bound)
    return false;
  if (containsNewTypePlaceholder(specified))
    return source_form == e_no_array_bound &&
           newTypePlaceholderMatchesDeduced(specified, result->get_base_type());
  SgType *semantic = result->get_base_type();
  SgType *source = specified;
  if (source_form == e_no_array_bound || source_form == e_type_array_bound) {
    std::set<SgType *> visited;
    while (visited.insert(specified).second) {
      specified = specified->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                       SgType::STRIP_DECLTYPE_TYPE);
      if (auto *modifier = isSgModifierType(specified)) {
        if (source_form == e_type_array_bound) {
          semantic = semantic->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                         SgType::STRIP_DECLTYPE_TYPE);
          auto *semantic_modifier = isSgModifierType(semantic);
          if (semantic_modifier == nullptr ||
              !(modifier->get_typeModifier() ==
                semantic_modifier->get_typeModifier()))
            return false;
          semantic = semantic_modifier->get_base_type();
        }
        specified = modifier->get_base_type();
        continue;
      }
      auto *nonreal = isSgNonrealType(specified);
      auto *declaration = nonreal != nullptr
                              ? isSgNonrealDecl(nonreal->get_declaration())
                              : nullptr;
      auto *alias =
          declaration != nullptr
              ? isSgTypedefDeclaration(declaration->get_templateDeclaration())
              : nullptr;
      if (alias == nullptr || isSgTemplateTypedefDeclaration(alias) != nullptr)
        break;
      if (!SageInterface::cxxSourceTypeMatchesSemanticType(
              specified, alias->get_base_type()))
        return false;
      specified = alias->get_base_type();
    }
    auto *array = isSgArrayType(specified);
    if (source_form == e_no_array_bound)
      return array == nullptr &&
             SageInterface::cxxSourceTypeMatchesSemanticType(
                 source, result->get_base_type());
    if (array == nullptr)
      return false;
    specified = array->get_base_type();
  }
  return SageInterface::cxxSourceTypeMatchesSemanticType(specified, semantic);
}

// Qualification setters preserve modification state for token unparsing.
int SgNewExp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgNewExp::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgNewExp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgNewExp::set_type_elaboration_required(bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgNewExp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgNewExp::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}

int SgNewExp::get_name_qualification_for_pointer_to_member_class_length()
    const {
  return p_name_qualification_for_pointer_to_member_class_length;
}

void SgNewExp::set_name_qualification_for_pointer_to_member_class_length(
    int name_qualification_length) {
  p_name_qualification_for_pointer_to_member_class_length =
      name_qualification_length;
}

bool SgNewExp::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  return p_type_elaboration_for_pointer_to_member_class_required;
}

void SgNewExp::set_type_elaboration_for_pointer_to_member_class_required(
    bool type_elaboration_required) {
  p_type_elaboration_for_pointer_to_member_class_required =
      type_elaboration_required;
}

bool SgNewExp::get_global_qualification_for_pointer_to_member_class_required()
    const {
  return p_global_qualification_for_pointer_to_member_class_required;
}

void SgNewExp::set_global_qualification_for_pointer_to_member_class_required(
    bool global_qualification_required) {

  p_global_qualification_for_pointer_to_member_class_required =
      global_qualification_required;
}
