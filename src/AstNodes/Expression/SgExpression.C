#include "ExpressionTypeClassification.h"
#include "sage3basic.h"

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace {

unsigned int
semanticWrapperBits(SgExpression::semantic_wrapper_mask_enum mask) {
  return static_cast<unsigned int>(mask);
}

void validateSemanticWrapperMask(SgExpression::semantic_wrapper_mask_enum mask,
                                 bool require_single_flag,
                                 const SgExpression *expression) {
  const unsigned int bits = semanticWrapperBits(mask);
  const unsigned int allowed =
      semanticWrapperBits(SgExpression::e_all_semantic_wrappers);
  if ((bits & ~allowed) != 0 ||
      (require_single_flag && (bits == 0 || (bits & (bits - 1)) != 0))) {
    std::cerr << "REX_AST_INVARIANT[semantic-expression-wrapper]: node="
              << static_cast<const void *>(expression)
              << " type=" << expression->class_name()
              << " has invalid semantic-wrapper mask " << bits << "\n";
    ROSE_ABORT();
  }
}

} // namespace

SgExpression::semantic_wrapper_mask_enum
SgExpression::get_semantic_wrapper_mask() const {
  validateSemanticWrapperMask(p_semantic_wrapper_mask, false, this);
  ROSE_ASSERT(((semanticWrapperBits(p_semantic_wrapper_mask) &
                semanticWrapperBits(e_bound_temporary)) != 0) ==
              (p_bound_temporary_destructor != nullptr));
  ROSE_ASSERT(((semanticWrapperBits(p_semantic_wrapper_mask) &
                semanticWrapperBits(e_materialized_temporary)) != 0) ==
              (p_materialized_storage_duration != e_temporary_storage_none));
  return p_semantic_wrapper_mask;
}

bool SgExpression::is_valid_temporary_materialization(
    temporary_storage_duration_enum storage, SgInitializedName *owner) {
  if (storage == e_temporary_storage_none ||
      storage == e_temporary_storage_full_expression)
    return owner == nullptr;
  if (owner == nullptr || !SgNode::isLiveNode(owner))
    return false;
  auto *declaration = isSgVariableDeclaration(owner->get_parent());
  auto *scope = owner->get_scope();
  if (declaration == nullptr || owner->get_declptr() != declaration ||
      scope == nullptr ||
      std::count(declaration->get_variables().begin(),
                 declaration->get_variables().end(), owner) != 1)
    return false;
  if (storage == e_temporary_storage_containing_object) {
    const auto &field_storage =
        declaration->get_declarationModifier().get_storageModifier();
    return owner->get_cxx_field_index().has_value() &&
           isSgClassDefinition(scope) != nullptr &&
           (field_storage.isDefault() || field_storage.isMutable());
  }
  bool static_storage = false, thread_storage = false;
  std::unordered_set<const SgNode *> visited;
  for (auto *current = owner; current != nullptr;
       current = current->get_prev_decl_item()) {
    if (!SgNode::isLiveNode(current) || !visited.insert(current).second)
      return false;
    auto *variable = isSgVariableDeclaration(current->get_parent());
    if (variable == nullptr || current->get_declptr() != variable ||
        current->get_cxx_field_index().has_value() ||
        std::count(variable->get_variables().begin(),
                   variable->get_variables().end(), current) != 1)
      return false;
    const auto &modifier =
        variable->get_declarationModifier().get_storageModifier();
    static_storage |= modifier.isStatic() || modifier.isExtern();
    thread_storage |=
        modifier.get_thread_local_storage() || variable->get_is_thread_local();
  }
  while (auto *context = isSgDeclarationScope(scope)) {
    if (!visited.insert(scope).second)
      return false;
    scope = context->get_scope();
    if (scope == nullptr)
      return false;
  }
  return Rose::AST::temporaryStorageMatchesOwner(
      storage, scope->variantT(), static_storage, thread_storage);
}

bool SgExpression::is_valid_bound_temporary_destructor(
    SgType *type, SgMemberFunctionDeclaration *destructor) {
  auto *object = Rose::AST::selectedDestructorClass(destructor);
  return object != nullptr && type != nullptr &&
         Rose::AST::resolvedClassType(type) == object;
}

void SgExpression::set_semantic_wrapper_mask(
    semantic_wrapper_mask_enum mask,
    SgMemberFunctionDeclaration *bound_temporary_destructor,
    temporary_storage_duration_enum materialized_storage_duration,
    SgInitializedName *materialized_lifetime_owner) {
  validateSemanticWrapperMask(mask, false, this);
  const bool bound =
      (semanticWrapperBits(mask) & semanticWrapperBits(e_bound_temporary)) != 0;
  ROSE_ASSERT(bound == (bound_temporary_destructor != nullptr));
  ROSE_ASSERT(!bound || is_valid_bound_temporary_destructor(
                            get_type(), bound_temporary_destructor));
  ROSE_ASSERT(((semanticWrapperBits(mask) &
                semanticWrapperBits(e_materialized_temporary)) != 0) ==
              (materialized_storage_duration != e_temporary_storage_none));
  ROSE_ASSERT(is_valid_temporary_materialization(materialized_storage_duration,
                                                 materialized_lifetime_owner));
  p_semantic_wrapper_mask = mask;
  p_bound_temporary_destructor = bound_temporary_destructor;
  p_materialized_storage_duration = materialized_storage_duration;
  p_materialized_lifetime_owner = materialized_lifetime_owner;
}

void SgExpression::add_semantic_wrapper(semantic_wrapper_mask_enum wrapper) {
  validateSemanticWrapperMask(wrapper, true, this);
  const unsigned int bits = semanticWrapperBits(get_semantic_wrapper_mask()) |
                            semanticWrapperBits(wrapper);
  set_semantic_wrapper_mask(static_cast<semantic_wrapper_mask_enum>(bits),
                            p_bound_temporary_destructor,
                            p_materialized_storage_duration,
                            p_materialized_lifetime_owner);
}

bool SgExpression::has_semantic_wrapper(
    semantic_wrapper_mask_enum wrapper) const {
  validateSemanticWrapperMask(wrapper, true, this);
  return (semanticWrapperBits(get_semantic_wrapper_mask()) &
          semanticWrapperBits(wrapper)) != 0;
}

bool SgExpression::has_semantic_value_type() const {
  const SgStringVal *stringLiteral = isSgStringVal(this);
  return Rose::AST::expressionHasSemanticValueType(
      variantT(),
      stringLiteral != nullptr && stringLiteral->get_cxx_unevaluated());
}

Sg_File_Info *SgExpression::get_file_info() const {
  Sg_File_Info *returnFileInfo = get_operatorPosition();
  if (returnFileInfo == NULL)
    returnFileInfo = get_startOfConstruct();
  return returnFileInfo;
}

void SgExpression::set_file_info(Sg_File_Info *fileInfo) {
  return set_operatorPosition(fileInfo);
}

void SgExpression::post_construction_initialization() {}

int SgExpression::replace_expression(SgExpression *, SgExpression *) {
  std::cerr
      << "Call to undefined SgExpression::replace_expression(): aborting\n";
  std::cerr << "dynamic type: " << this->class_name() << "\n";
  ROSE_ABORT();
  return 0;
}

SgType *SgExpression::get_type() const {
  std::cerr << "Call to undefined SgExpression::get_type(): aborting\n";
  std::cerr << "dynamic type: " << this->class_name() << "\n";
  ROSE_ABORT();

  return 0;
}

int SgExpression::precedence() const { return 0; }

int SgExpression::get_name_qualification_length() const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return 0;
}

void SgExpression::set_name_qualification_length(
    int /*name_qualification_length*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

bool SgExpression::get_type_elaboration_required() const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false;
}

bool SgExpression::has_type_qualification_payload() const {
  switch (variantT()) {
  case V_SgVarRefExp:
  case V_SgNonrealRefExp:
  case V_SgFunctionRefExp:
  case V_SgMemberFunctionRefExp:
  case V_SgTemplateFunctionRefExp:
  case V_SgTemplateMemberFunctionRefExp:
  case V_SgEnumVal:
  case V_SgSizeOfOp:
  case V_SgAlignOfOp:
  case V_SgTypeIdOp:
  case V_SgVarArgOp:
  case V_SgCastExp:
  case V_SgNewExp:
  case V_SgConstructorInitializer:
  case V_SgPseudoDestructorRefExp:
  case V_SgTypeExpression:
  case V_SgTypeRequirement:
    return true;
  default:
    return false;
  }
}

void SgExpression::set_type_elaboration_required(
    bool /*type_elaboration_required*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

bool SgExpression::get_global_qualification_required() const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false;
}

void SgExpression::set_global_qualification_required(
    bool /*global_qualification_required*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

int SgExpression::get_name_qualification_for_pointer_to_member_class_length()
    const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return 0;
}

void SgExpression::set_name_qualification_for_pointer_to_member_class_length(
    int /*name_qualification_length*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

bool SgExpression::get_type_elaboration_for_pointer_to_member_class_required()
    const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false;
}

void SgExpression::set_type_elaboration_for_pointer_to_member_class_required(
    bool /*type_elaboration_required*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

bool SgExpression::
    get_global_qualification_for_pointer_to_member_class_required() const {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false;
}

void SgExpression::
    set_global_qualification_for_pointer_to_member_class_required(
        bool /*global_qualification_required*/) {

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

SgExpression *SgExpression::get_originalExpressionTree() const {
  return p_originalExpressionTree;
}

void SgExpression::set_originalExpressionTree(SgExpression *source) {
  if (source == this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[original-expression-owner]: expression=%p/%s "
            "cannot own itself as source spelling\n",
            static_cast<void *>(this), class_name().c_str());
    ROSE_ABORT();
  }
  if (p_originalExpressionTree != nullptr && source != nullptr &&
      p_originalExpressionTree != source) {
    fprintf(stderr,
            "REX_AST_INVARIANT[original-expression-owner]: expression=%p/%s "
            "already owns source=%p/%s and cannot publish source=%p/%s\n",
            static_cast<void *>(this), class_name().c_str(),
            static_cast<void *>(p_originalExpressionTree),
            p_originalExpressionTree->class_name().c_str(),
            static_cast<void *>(source), source->class_name().c_str());
    ROSE_ABORT();
  }
  p_originalExpressionTree = source;
}

bool SgExpression::hasExplicitType() {
  return isSgCxxArrayInitLoop(this) != nullptr ||
         isSgFunctionRefExp(this) != nullptr ||
         isSgCxxArrayInitReference(this) != nullptr ||
         isSgTemplateParameterVal(this) != nullptr ||
         isSgComplexVal(this) != nullptr || isSgThisExp(this) != nullptr ||
         isSgSizeOfOp(this) != nullptr || isSgAlignOfOp(this) != nullptr ||
         isSgTypeIdOp(this) != nullptr || isSgVarArgStartOp(this) != nullptr ||
         isSgVarArgStartOneOperandOp(this) != nullptr ||
         isSgVarArgOp(this) != nullptr || isSgVarArgEndOp(this) != nullptr ||
         isSgVarArgCopyOp(this) != nullptr || isSgNewExp(this) != nullptr ||
         isSgRefExp(this) != nullptr ||
         isSgAggregateInitializer(this) != nullptr ||
         isSgCastExp(this) != nullptr ||
         isSgConstructorInitializer(this) != nullptr ||
         isSgPseudoDestructorRefExp(this) != nullptr ||
         isSgFunctionParameterRefExp(this) != nullptr;
}

void SgExpression::set_explicitly_stored_type(SgType *type) {
  // DQ (3/7/2014): Some expressions store internal SgType pointers explicitly,
  // this allows these IR nodes to be reset with new types (used in the snippet
  // support).

  switch (this->variantT()) {
  case V_SgFunctionRefExp:
    isSgFunctionRefExp(this)->set_function_type(type);
    break;
  case V_SgCxxArrayInitLoop: {
    auto *loop = isSgCxxArrayInitLoop(this);
    loop->set_expression_type(type);
    loop->validate();
    break;
  }
  case V_SgCxxArrayInitReference: {
    auto *reference = isSgCxxArrayInitReference(this);
    reference->set_expression_type(type);
    reference->validate();
    break;
  }
  case V_SgNewExp: {
    SgNewExp *exp = isSgNewExp(this);
    ROSE_ASSERT(exp != NULL);
    // SgExpression's explicit-type API publishes the expression result type.
    // A new-expression stores the allocated type instead, and derives its
    // result as a pointer to that type.  Storing the caller's pointer directly
    // used to turn a translated `new T` from T* into T**.
    SgPointerType *resultType = isSgPointerType(type);
    if (resultType == NULL || resultType->get_base_type() == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[new-result-type-publication]: new=%p "
              "requires one exact pointer result type, got %p/%s\n",
              static_cast<void *>(exp), static_cast<void *>(type),
              type != NULL ? type->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    exp->set_specified_type(resultType->get_base_type());
    if (exp->get_type() != type) {
      fprintf(stderr,
              "REX_AST_INVARIANT[new-result-type-publication]: new=%p "
              "failed exact result=%p publication\n",
              static_cast<void *>(exp), static_cast<void *>(type));
      ROSE_ABORT();
    }
    break;
  }

  case V_SgConstructorInitializer: {
    SgConstructorInitializer *exp = isSgConstructorInitializer(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgTemplateParameterVal: {
    SgTemplateParameterVal *exp = isSgTemplateParameterVal(this);
    ROSE_ASSERT(exp != NULL);
    ROSE_ASSERT(exp->get_type() == type);
    break;
  }

  case V_SgFunctionParameterRefExp: {
    SgFunctionParameterRefExp *exp = isSgFunctionParameterRefExp(this);
    ROSE_ASSERT(exp != nullptr);
    ROSE_ASSERT(!SageInterface::containsUnknownType(type));
    exp->set_parameter_type(type);
    ROSE_ASSERT(exp->get_type() == type);
    break;
  }

  case V_SgComplexVal: {
    SgComplexVal *exp = isSgComplexVal(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_precisionType(type);
    break;
  }

  case V_SgThisExp: {
    SgThisExp *exp = isSgThisExp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    (void)exp->get_type();
    break;
  }

  case V_SgSizeOfOp: {
    SgSizeOfOp *exp = isSgSizeOfOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    (void)exp->get_type();
    break;
  }

  case V_SgAlignOfOp: {
    SgAlignOfOp *exp = isSgAlignOfOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    (void)exp->get_type();
    break;
  }

  case V_SgTypeIdOp: {
    SgTypeIdOp *exp = isSgTypeIdOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgVarArgStartOp: {
    SgVarArgStartOp *exp = isSgVarArgStartOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgVarArgStartOneOperandOp: {
    SgVarArgStartOneOperandOp *exp = isSgVarArgStartOneOperandOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgVarArgOp: {
    SgVarArgOp *exp = isSgVarArgOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgVarArgEndOp: {
    SgVarArgEndOp *exp = isSgVarArgEndOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgVarArgCopyOp: {
    SgVarArgCopyOp *exp = isSgVarArgCopyOp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgRefExp: {
    SgRefExp *exp = isSgRefExp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_type_name(type);
    break;
  }

  case V_SgAggregateInitializer: {
    SgAggregateInitializer *exp = isSgAggregateInitializer(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_expression_type(type);
    break;
  }

  case V_SgCastExp: {
    SgCastExp *exp = isSgCastExp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_type(type);
    exp->validate_semantic_conversion();
    break;
  }

  case V_SgPseudoDestructorRefExp: {
    SgPseudoDestructorRefExp *exp = isSgPseudoDestructorRefExp(this);
    ROSE_ASSERT(exp != NULL);
    exp->set_object_type(type);
    break;
  }

  default: {
    printf("Error: SgExpression::set_explicit_type(): default reached: "
           "expression = %p = %s \n",
           this, this->class_name().c_str());
    ROSE_ABORT();
  }
  }
}
