#include "RoseAst.h"
#include "sage3basic.h"

#include <set>

namespace {
bool exactType(SgType *type) {
  return type != nullptr && SgNode::isLiveNode(type) &&
         !SageInterface::containsUnknownType(type);
}

bool exactExpression(SgExpression *expression) {
  return expression != nullptr && SgNode::isLiveNode(expression) &&
         exactType(expression->get_type());
}

bool validCategory(SgCastExp::value_category_enum category) {
  return category == SgCastExp::e_value_category_lvalue ||
         category == SgCastExp::e_value_category_xvalue ||
         category == SgCastExp::e_value_category_prvalue;
}

bool validObjectKind(SgOpaqueValueBinding::object_kind_enum kind) {
  return kind >= SgOpaqueValueBinding::e_ordinary_object &&
         kind <= SgOpaqueValueBinding::e_matrix_component;
}

using Bindings = std::set<SgOpaqueValueBinding *>;

bool referencesAreBound(SgExpression *expression, const Bindings &available) {
  std::vector<SgNode *> pending{expression};
  std::set<SgNode *> seen;
  while (!pending.empty()) {
    SgNode *node = pending.back();
    pending.pop_back();
    if (node == nullptr || !SgNode::isLiveNode(node) ||
        !seen.insert(node).second)
      return false;
    if (isSgOpaqueValueBinding(node) != nullptr)
      return false;
    if (auto *reference = isSgOpaqueValueReference(node)) {
      if (!SgOpaqueValueReference::is_valid_reference(
              reference->get_binding()) ||
          available.count(reference->get_binding()) != 1)
        return false;
      continue;
    }
    if (auto *nested = isSgPseudoObjectExp(node)) {
      for (auto *binding : nested->get_captured_bindings())
        if (available.count(binding) != 1)
          return false;
      continue;
    }
    for (SgNode *child : node->get_traversalSuccessorContainer()) {
      if (child == nullptr)
        continue;
      if (child->get_parent() != node)
        return false;
      pending.push_back(child);
    }
  }
  return true;
}
} // namespace

bool SgOpaqueValueBinding::is_valid_binding(
    SgExpression *source, SgType *type, SgCastExp::value_category_enum category,
    object_kind_enum kind) {
  return exactExpression(source) && exactType(type) &&
         validCategory(category) && validObjectKind(kind) &&
         SageInterface::isEquivalentType(source->get_type(), type) &&
         source->get_lvalue() ==
             (category == SgCastExp::e_value_category_lvalue);
}

void SgOpaqueValueBinding::post_construction_initialization() {
  SgExpression::post_construction_initialization();
  ROSE_ASSERT(is_valid_binding(p_source_expression, p_expression_type,
                               p_value_category, p_object_kind));
  ROSE_ASSERT(p_source_expression->get_parent() == nullptr);
  p_source_expression->set_parent(this);
  set_lvalue(p_value_category == SgCastExp::e_value_category_lvalue);
}

void SgOpaqueValueBinding::validate() const {
  ROSE_ASSERT(is_valid_binding(p_source_expression, p_expression_type,
                               p_value_category, p_object_kind));
  ROSE_ASSERT(p_source_expression->get_parent() == this);
  ROSE_ASSERT(get_lvalue() ==
              (p_value_category == SgCastExp::e_value_category_lvalue));
}

SgType *SgOpaqueValueBinding::get_type() const {
  ROSE_ASSERT(exactType(p_expression_type));
  return p_expression_type;
}

bool SgOpaqueValueReference::is_valid_reference(SgOpaqueValueBinding *binding) {
  return binding != nullptr && SgNode::isLiveNode(binding) &&
         SgOpaqueValueBinding::is_valid_binding(
             binding->get_source_expression(), binding->get_type(),
             binding->get_value_category(), binding->get_object_kind()) &&
         binding->get_source_expression()->get_parent() == binding;
}

void SgOpaqueValueReference::post_construction_initialization() {
  SgExpression::post_construction_initialization();
  ROSE_ASSERT(is_valid_reference(p_binding));
  set_lvalue(p_binding->get_value_category() ==
             SgCastExp::e_value_category_lvalue);
}

void SgOpaqueValueReference::validate() const {
  ROSE_ASSERT(is_valid_reference(p_binding));
  ROSE_ASSERT(get_lvalue() == (p_binding->get_value_category() ==
                               SgCastExp::e_value_category_lvalue));
}

SgType *SgOpaqueValueReference::get_type() const {
  ROSE_ASSERT(p_binding != nullptr && SgNode::isLiveNode(p_binding));
  return p_binding->get_type();
}

bool SgPseudoObjectExp::is_valid_sequence(
    SgExpression *written, SgExprListExp *evaluations, SgExpression *result,
    const SgOpaqueValueBindingPtrList &captures, SgType *type,
    SgCastExp::value_category_enum category,
    SgOpaqueValueBinding::object_kind_enum object_kind) {
  if (!exactExpression(written) || evaluations == nullptr ||
      !SgNode::isLiveNode(evaluations) || !exactType(type) ||
      !validCategory(category) || !validObjectKind(object_kind) ||
      !referencesAreBound(written, {}))
    return false;
  Bindings available;
  for (auto *binding : captures)
    if (!SgOpaqueValueReference::is_valid_reference(binding) ||
        binding->get_parent() == evaluations ||
        !available.insert(binding).second)
      return false;
  unsigned result_count = 0;
  for (SgExpression *evaluation : evaluations->get_expressions()) {
    if (!exactExpression(evaluation) || evaluation == written ||
        evaluation == evaluations->get_parent() ||
        evaluation->get_parent() != evaluations)
      return false;
    if (evaluation == result)
      ++result_count;
    if (auto *binding = isSgOpaqueValueBinding(evaluation)) {
      if (!SgOpaqueValueReference::is_valid_reference(binding) ||
          !referencesAreBound(binding->get_source_expression(), available) ||
          !available.insert(binding).second)
        return false;
    } else if (!referencesAreBound(evaluation, available)) {
      return false;
    }
  }
  return result == nullptr
             ? isSgTypeVoid(type) != nullptr &&
                   category == SgCastExp::e_value_category_prvalue
             : result_count == 1 &&
                   SageInterface::isEquivalentType(result->get_type(), type) &&
                   result->get_lvalue() ==
                       (category == SgCastExp::e_value_category_lvalue);
}

void SgPseudoObjectExp::post_construction_initialization() {
  SgExpression::post_construction_initialization();
  ROSE_ASSERT(is_valid_sequence(
      p_written_expression, p_semantic_evaluations, p_result_expression,
      p_captured_bindings, p_expression_type, p_value_category, p_object_kind));
  ROSE_ASSERT(p_written_expression->get_parent() == nullptr &&
              p_semantic_evaluations->get_parent() == nullptr);
  p_written_expression->set_parent(this);
  p_semantic_evaluations->set_parent(this);
  set_lvalue(p_value_category == SgCastExp::e_value_category_lvalue);
  p_source_sequence = SgNode::get_globalAstModificationSequence();
}

void SgPseudoObjectExp::validate() const {
  ROSE_ASSERT(is_valid_sequence(
      p_written_expression, p_semantic_evaluations, p_result_expression,
      p_captured_bindings, p_expression_type, p_value_category, p_object_kind));
  ROSE_ASSERT(p_written_expression->get_parent() == this &&
              p_semantic_evaluations->get_parent() == this);
}

SgExpression *SgPseudoObjectExp::get_written_expression_checked() const {
  validate();
  for (SgNode *root : {static_cast<SgNode *>(p_written_expression),
                       static_cast<SgNode *>(p_semantic_evaluations)})
    for (SgNode *node : RoseAst(root))
      ROSE_ASSERT(node->get_lastMutationSequence() <= p_source_sequence);
  return p_written_expression;
}

SgType *SgPseudoObjectExp::get_type() const {
  ROSE_ASSERT(exactType(p_expression_type));
  return p_expression_type;
}
