#include "sage3basic.h"

#include <iostream>

#include <map>

#include <string>

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
  return p_semantic_wrapper_mask;
}

void SgExpression::set_semantic_wrapper_mask(semantic_wrapper_mask_enum mask) {
  validateSemanticWrapperMask(mask, false, this);
  p_semantic_wrapper_mask = mask;
}

void SgExpression::add_semantic_wrapper(semantic_wrapper_mask_enum wrapper) {
  validateSemanticWrapperMask(wrapper, true, this);
  const unsigned int bits = semanticWrapperBits(get_semantic_wrapper_mask()) |
                            semanticWrapperBits(wrapper);
  p_semantic_wrapper_mask = static_cast<semantic_wrapper_mask_enum>(bits);
}

bool SgExpression::has_semantic_wrapper(
    semantic_wrapper_mask_enum wrapper) const {
  validateSemanticWrapperMask(wrapper, true, this);
  return (semanticWrapperBits(get_semantic_wrapper_mask()) &
          semanticWrapperBits(wrapper)) != 0;
}

bool SgExpression::has_semantic_value_type() const {
  const SgStringVal *stringLiteral = isSgStringVal(this);
  return (stringLiteral == nullptr || !stringLiteral->get_cxx_unevaluated()) &&
         isSgExprListExp(this) == nullptr &&
         isSgCudaKernelExecConfig(this) == nullptr &&
         isSgRangeExp(this) == nullptr &&
         isSgSubscriptExpression(this) == nullptr &&
         isSgDesignator(this) == nullptr && isSgAsmOp(this) == nullptr &&
         isSgSimpleRequirement(this) == nullptr &&
         isSgTypeRequirement(this) == nullptr &&
         isSgCompoundRequirement(this) == nullptr &&
         isSgRequirementSubstitutionFailure(this) == nullptr &&
         isSgNestedRequirement(this) == nullptr &&
         isSgTypeExpression(this) == nullptr &&
         isSgNullExpression(this) == nullptr &&
         isSgFortranCommonBlockRefExp(this) == nullptr &&
         isSgAsteriskShapeExp(this) == nullptr &&
         isSgColonShapeExp(this) == nullptr &&
         isSgAssumedRankExp(this) == nullptr &&
         isSgImpliedDo(this) == nullptr &&
         isSgCAFImageSelectorExp(this) == nullptr &&
         isSgOmpNameExpression(this) == nullptr &&
         isSgOmpSourceExpression(this) == nullptr &&
         isSgOmpInductionItem(this) == nullptr &&
         isSgOmpApplyTransformation(this) == nullptr &&
         isSgOmpInitModifier(this) == nullptr &&
         isSgOmpInitModifierList(this) == nullptr &&
         isSgOmpAppendArgsOperation(this) == nullptr &&
         isSgOmpMapDistDataPolicy(this) == nullptr &&
         isSgOmpMapItem(this) == nullptr;
}

Sg_File_Info *SgExpression::get_file_info() const {
  // DQ (11/8/2006): Note that the frontend translation only uses
  // set_startOfConstruct() and set_endOfConstruct().

  // This redefines get_file_info() as it is implemented for a SgLocatedNode
  // to use the "get_operatorPosition()" instead of the get_startOfConstruct()"
  // Most if not all operator positions will associated with syntax for the
  // operator. return get_operatorPosition();
  Sg_File_Info *returnFileInfo = get_operatorPosition();
  if (returnFileInfo == NULL)
    returnFileInfo = get_startOfConstruct();
  return returnFileInfo;
}

void SgExpression::set_file_info(Sg_File_Info *fileInfo) {
  // DQ (11/8/2006): Note that the frontend translation only uses
  // set_startOfConstruct() and set_endOfConstruct().

  // This redefines get_file_info() as it is implemented for a SgLocatedNode
  // to use the "get_operatorPosition()" instead of the get_startOfConstruct()"
  // Most if not all operator positions will associated with syntax for the
  // operator.
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
  // DQ: With this function defined we can be sure that we don't call it by
  // accident. This catches any IR nodes where the fucntion should have been
  // overwritten and was not.

  std::cerr << "Call to undefined SgExpression::get_type(): aborting\n";
  std::cerr << "dynamic type: " << this->class_name() << "\n";
  ROSE_ABORT();

  return 0;
}

int SgExpression::precedence() const { return 0; }

int SgExpression::get_name_qualification_length() const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return 0; // p_name_qualification_length;
}

void SgExpression::set_name_qualification_length(
    int /*name_qualification_length*/) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();
}

bool SgExpression::get_type_elaboration_required() const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false; // p_type_elaboration_required;
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
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  // p_type_elaboration_required = type_elaboration_required;
}

bool SgExpression::get_global_qualification_required() const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false; // p_global_qualification_required;
}

void SgExpression::set_global_qualification_required(
    bool /*global_qualification_required*/) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  // p_global_qualification_required = global_qualification_required;
}

int SgExpression::get_name_qualification_for_pointer_to_member_class_length()
    const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return 0; // p_name_qualification_length;
}

void SgExpression::set_name_qualification_for_pointer_to_member_class_length(
    int /*name_qualification_length*/) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  // p_name_qualification_length = name_qualification_length;
}

bool SgExpression::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false; // p_type_elaboration_required;
}

void SgExpression::set_type_elaboration_for_pointer_to_member_class_required(
    bool /*type_elaboration_required*/) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  // p_type_elaboration_required = type_elaboration_required;
}

bool SgExpression::
    get_global_qualification_for_pointer_to_member_class_required() const {
  ROSE_ASSERT(this != NULL);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  return false; // p_global_qualification_required;
}

void SgExpression::
    set_global_qualification_for_pointer_to_member_class_required(
        bool /*global_qualification_required*/) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  printf("Error: base class virtual function called by mistake on node = %p = "
         "%s \n",
         this, this->class_name().c_str());
  ROSE_ABORT();

  // p_global_qualification_required = global_qualification_required;
}

// DQ (9/23/2011): Use the vitual function version so that we can test within
// ROSE (part of incremental testing of new original expression tree support).
// DQ (9/19/2011): Put back the original code (non-virtual functions) so that we
// can test against previously passing tests.
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
  // DQ (3/7/2014):  This could be implemented as a virtual function but would
  // require 11 functions to be implemented. I have thus instead implemented it
  // as a single function on the SgType instead. We can review this if it is
  // important.

  // This function returns true only if this is either a SgTemplateParameterVal,
  // SgComplexVal, SgThisExp, SgSizeOfOp, SgAlignOfOp, SgTypeIdOp,
  // SgVarArgStartOp,
  // SgVarArgStartOneOperandOp, SgVarArgOp, SgVarArgEndOp, SgVarArgCopyOp,
  // SgNewExp, SgRefExp, SgAggregateInitializer, SgCastExp,
  // SgConstructorInitializer, SgAssignInitializer, SgPseudoDestructorRefExp.

  bool returnValue = false;

  // DQ (11/10/2014): Added support for SgFunctionParameterRefExp node to store
  // the type explicitly.

  if (isSgTemplateParameterVal(this) != NULL || isSgComplexVal(this) != NULL ||
      isSgThisExp(this) != NULL || isSgSizeOfOp(this) != NULL ||
      isSgAlignOfOp(this) != NULL || isSgTypeIdOp(this) != NULL ||
      isSgVarArgStartOp(this) != NULL ||
      isSgVarArgStartOneOperandOp(this) != NULL || isSgVarArgOp(this) != NULL ||
      isSgVarArgEndOp(this) != NULL || isSgVarArgCopyOp(this) != NULL ||
      isSgNewExp(this) != NULL || isSgRefExp(this) != NULL ||
      isSgAggregateInitializer(this) != NULL || isSgCastExp(this) != NULL ||
      // isSgConstructorInitializer(this) != NULL   ||
      // isSgAssignInitializer(this) != NULL        ||
      // isSgPseudoDestructorRefExp(this)  != NULL  ||
      isSgConstructorInitializer(this) != NULL ||
      isSgPseudoDestructorRefExp(this) != NULL ||
      isSgFunctionParameterRefExp(this) != NULL) {
    returnValue = true;
  }

  return returnValue;
}

void SgExpression::set_explicitly_stored_type(SgType *type) {
  // DQ (3/7/2014): Some expressions store internal SgType pointers explicitly,
  // this allows these IR nodes to be reset with new types (used in the snippet
  // support).

  switch (this->variantT()) {
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
    exp->set_valueType(type);
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
