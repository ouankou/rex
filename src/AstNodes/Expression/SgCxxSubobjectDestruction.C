#include "ExpressionTypeClassification.h"
#include "sage3basic.h"

#include <algorithm>
#include <unordered_set>

SgClassType *
SgCxxSubobjectDestruction::get_member_class(SgInitializedName *member) {
  ROSE_ASSERT(member != nullptr && member->get_type() != nullptr);
  ROSE_ASSERT(!SageInterface::containsUnknownType(member->get_type()));
  auto *object = member->get_type()->stripTypedefsAndModifiers();
  std::unordered_set<SgType *> visited;
  while (auto *array = isSgArrayType(object)) {
    ROSE_ASSERT(visited.insert(array).second);
    auto *index = array->get_index();
    if (index == nullptr)
      return nullptr;
    if (auto *absent = isSgNullExpression(index)) {
      ROSE_ASSERT(absent->get_role() ==
                  SgNullExpression::e_null_expression_syntactic_absence);
      return nullptr;
    }
    const auto extent = SageInterface::evaluateConstIntegerExpression(index);
    ROSE_ASSERT(extent.hasValue_);
    if (extent.value_ == 0)
      return nullptr;
    ROSE_ASSERT(array->get_base_type() != nullptr);
    object = array->get_base_type()->stripTypedefsAndModifiers();
  }
  return Rose::AST::resolvedClassType(object);
}

bool SgCxxSubobjectDestruction::is_valid_target(
    SgMemberFunctionDeclaration *destructor, SgInitializedName *member,
    SgClassDeclaration *base, target_kind_enum kind, SgType *type) {
  auto *owner = Rose::AST::selectedDestructorClass(destructor);
  if (owner == nullptr || isSgTypeVoid(type) == nullptr)
    return false;
  SgClassType *target = nullptr;
  if (kind == e_member) {
    if (member == nullptr || !SgNode::isLiveNode(member) || base != nullptr)
      return false;
    auto *variable = isSgVariableDeclaration(member->get_parent());
    auto *scope = isSgClassDefinition(member->get_scope());
    if (variable == nullptr || member->get_declptr() != variable ||
        scope == nullptr || scope->get_declaration() == nullptr ||
        scope->get_declaration()->get_class_type() ==
            SgClassDeclaration::e_union ||
        !member->get_cxx_field_index().has_value() ||
        variable->get_declarationModifier().get_storageModifier().isStatic() ||
        std::count(variable->get_variables().begin(),
                   variable->get_variables().end(), member) != 1)
      return false;
    if (member->get_type() == nullptr)
      return false;
    target = get_member_class(member);
  } else if (kind == e_nonvirtual_base || kind == e_virtual_base) {
    if (base == nullptr || !SgNode::isLiveNode(base) || member != nullptr)
      return false;
    return base->get_firstNondefiningDeclaration() ==
           owner->get_declaration()->get_firstNondefiningDeclaration();
  } else {
    return false;
  }
  return target != nullptr && target == owner;
}

void SgCxxSubobjectDestruction::post_construction_initialization() {
  SgExpression::post_construction_initialization();
  validate();
}

void SgCxxSubobjectDestruction::validate() const {
  ROSE_ASSERT(!get_lvalue());
  ROSE_ASSERT(is_valid_target(p_destructor_declaration, p_member_target,
                              p_base_target, p_target_kind, p_expression_type));
}
