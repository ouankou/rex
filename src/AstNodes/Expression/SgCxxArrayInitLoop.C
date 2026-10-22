#include "sage3basic.h"
#include "sageInterface.h"

namespace {

SgType *unqualifiedType(SgType *type) {
  return type == nullptr || !SgNode::isLiveNode(type)
             ? nullptr
             : type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                               SgType::STRIP_MODIFIER_TYPE);
}

bool exactExpression(SgExpression *expression) {
  if (expression == nullptr || !SgNode::isLiveNode(expression) ||
      !expression->has_semantic_value_type())
    return false;
  SgType *type = unqualifiedType(expression->get_type());
  return type != nullptr && isSgTypeUnknown(type) == nullptr &&
         isSgTypeDefault(type) == nullptr;
}

bool sameElementType(SgType *left, SgType *right) {
  if (left == nullptr || right == nullptr)
    return false;
  left = left->stripType(SgType::STRIP_TYPEDEF_TYPE);
  right = right->stripType(SgType::STRIP_TYPEDEF_TYPE);
  auto *left_modifier = isSgModifierType(left);
  auto *right_modifier = isSgModifierType(right);
  if (left_modifier == nullptr && right_modifier == nullptr)
    return SageInterface::isEquivalentType(left, right);
  SgTypeModifier left_qualifiers;
  SgTypeModifier right_qualifiers;
  if (left_modifier != nullptr)
    left_qualifiers = left_modifier->get_typeModifier();
  if (right_modifier != nullptr)
    right_qualifiers = right_modifier->get_typeModifier();
  left_qualifiers.get_constVolatileModifier().unsetConst();
  left_qualifiers.get_constVolatileModifier().unsetVolatile();
  right_qualifiers.get_constVolatileModifier().unsetConst();
  right_qualifiers.get_constVolatileModifier().unsetVolatile();
  if (left_modifier != nullptr && left_qualifiers.isIdentity())
    return sameElementType(left_modifier->get_base_type(), right);
  if (right_modifier != nullptr && right_qualifiers.isIdentity())
    return sameElementType(left, right_modifier->get_base_type());
  return left_modifier != nullptr && right_modifier != nullptr &&
         left_qualifiers == right_qualifiers &&
         sameElementType(left_modifier->get_base_type(),
                         right_modifier->get_base_type());
}

} // namespace

bool SgCxxArrayInitBinding::is_valid_source(
    SgExpression *source, SgCastExp::value_category_enum category) {
  if (!exactExpression(source) ||
      isSgArrayType(unqualifiedType(source->get_type())) == nullptr ||
      (category != SgCastExp::e_value_category_lvalue &&
       category != SgCastExp::e_value_category_xvalue &&
       category != SgCastExp::e_value_category_prvalue) ||
      source->get_lvalue() != (category == SgCastExp::e_value_category_lvalue))
    return false;
  if (auto *cast = isSgCastExp(source))
    return cast->get_value_category() == category;
  if (auto *reference = isSgCxxArrayInitReference(source))
    return reference->get_value_category() == category;
  return isSgCxxArrayInitLoop(source) == nullptr ||
         category == SgCastExp::e_value_category_prvalue;
}

void SgCxxArrayInitBinding::post_construction_initialization() {
  SgNode::post_construction_initialization();
  ROSE_ASSERT(is_valid_source(p_common_source, p_common_value_category));
  ROSE_ASSERT(p_common_source->get_parent() == nullptr);
  p_common_source->set_parent(this);
}

void SgCxxArrayInitBinding::validate() const {
  ROSE_ASSERT(is_valid_source(p_common_source, p_common_value_category));
  ROSE_ASSERT(p_common_source->get_parent() == this);
}

void SgCxxArrayInitBinding::replace_common_source(
    SgExpression *source, SgCastExp::value_category_enum category) {
  validate();
  ROSE_ASSERT(category == p_common_value_category);
  ROSE_ASSERT(is_valid_source(source, category));
  ROSE_ASSERT(source->get_parent() == nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(source->get_type(),
                                              p_common_source->get_type()));
  SgExpression *previous = p_common_source;
  set_common_source(source);
  source->set_parent(this);
  previous->set_parent(nullptr);
  validate();
}

bool SgCxxArrayInitLoop::is_valid_extent(SgType *type, uint64_t count) {
  auto *array = isSgArrayType(unqualifiedType(type));
  if (array == nullptr || array->get_is_variable_length_array() ||
      array->get_index() == nullptr)
    return false;
  const auto extent =
      SageInterface::evaluateConstIntegerExpression(array->get_index());
  return extent.hasValue_ && extent.value_ == count;
}

bool SgCxxArrayInitLoop::is_valid_loop(SgCxxArrayInitBinding *binding,
                                       SgExpression *element_initializer,
                                       SgType *type, uint64_t count) {
  if (binding == nullptr || !SgNode::isLiveNode(binding) ||
      !SgCxxArrayInitBinding::is_valid_source(
          binding->get_common_source(), binding->get_common_value_category()) ||
      !exactExpression(element_initializer) || !is_valid_extent(type, count) ||
      !is_valid_extent(binding->get_common_source()->get_type(), count))
    return false;
  auto *array = isSgArrayType(unqualifiedType(type));
  return sameElementType(array->get_base_type(),
                         element_initializer->get_type());
}

void SgCxxArrayInitLoop::post_construction_initialization() {
  ROSE_ASSERT(is_valid_loop(p_binding, p_element_initializer, p_expression_type,
                            p_element_count));
  p_binding->validate();
  ROSE_ASSERT(p_binding->get_parent() == nullptr);
  ROSE_ASSERT(p_element_initializer->get_parent() == nullptr);
  ROSE_ASSERT(p_binding->get_common_source() != p_element_initializer);
  p_binding->set_parent(this);
  p_element_initializer->set_parent(this);
}

void SgCxxArrayInitLoop::validate() const {
  ROSE_ASSERT(is_valid_loop(p_binding, p_element_initializer, p_expression_type,
                            p_element_count));
  p_binding->validate();
  ROSE_ASSERT(p_binding->get_parent() == this);
  ROSE_ASSERT(p_element_initializer->get_parent() == this);
  ROSE_ASSERT(!get_lvalue());
}

void SgCxxArrayInitLoop::validate_bindings() const {
  validate();
  std::vector<SgNode *> pending{const_cast<SgCxxArrayInitLoop *>(this)};
  while (!pending.empty()) {
    SgNode *node = pending.back();
    pending.pop_back();
    for (SgNode *child : node->get_traversalSuccessorContainer())
      if (child != nullptr)
        pending.push_back(child);
    auto *reference = isSgCxxArrayInitReference(node);
    if (reference == nullptr)
      continue;
    reference->validate();
    SgCxxArrayInitBinding *innermost = nullptr;
    bool found = false;
    for (SgNode *child = reference; child->get_parent() != nullptr;
         child = child->get_parent()) {
      auto *owner = isSgCxxArrayInitLoop(child->get_parent());
      if (owner == nullptr || owner->get_element_initializer() != child)
        continue;
      if (innermost == nullptr)
        innermost = owner->get_binding();
      found |= owner->get_binding() == reference->get_binding();
    }
    ROSE_ASSERT(found);
    ROSE_ASSERT(reference->get_reference_kind() !=
                    SgCxxArrayInitReference::e_current_index ||
                reference->get_binding() == innermost);
  }
}

int SgCxxArrayInitLoop::replace_expression(SgExpression *old_expression,
                                           SgExpression *new_expression) {
  validate();
  ROSE_ASSERT(old_expression == p_element_initializer);
  ROSE_ASSERT(is_valid_loop(p_binding, new_expression, p_expression_type,
                            p_element_count));
  if (old_expression == new_expression)
    return 1;
  ROSE_ASSERT(new_expression->get_parent() == nullptr);
  set_element_initializer(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}

bool SgCxxArrayInitReference::is_valid_reference(
    SgCxxArrayInitBinding *binding, reference_kind_enum kind, SgType *type,
    SgCastExp::value_category_enum category) {
  if (binding == nullptr || !SgNode::isLiveNode(binding) ||
      !SgCxxArrayInitBinding::is_valid_source(
          binding->get_common_source(), binding->get_common_value_category()) ||
      type == nullptr || !SgNode::isLiveNode(type) ||
      (category != SgCastExp::e_value_category_lvalue &&
       category != SgCastExp::e_value_category_xvalue &&
       category != SgCastExp::e_value_category_prvalue))
    return false;
  switch (kind) {
  case e_common_value:
    return SageInterface::cxxSourceTypeMatchesSemanticType(
               binding->get_common_source()->get_type(), type) &&
           category == binding->get_common_value_category();
  case e_current_index:
    return type->isUnsignedType() &&
           category == SgCastExp::e_value_category_prvalue;
  }
  return false;
}

void SgCxxArrayInitReference::post_construction_initialization() {
  ROSE_ASSERT(is_valid_reference(p_binding, p_reference_kind, p_expression_type,
                                 p_value_category));
  p_binding->validate();
  set_lvalue(p_value_category == SgCastExp::e_value_category_lvalue);
}

void SgCxxArrayInitReference::validate() const {
  ROSE_ASSERT(is_valid_reference(p_binding, p_reference_kind, p_expression_type,
                                 p_value_category));
  p_binding->validate();
  ROSE_ASSERT(get_lvalue() ==
              (p_value_category == SgCastExp::e_value_category_lvalue));
}
