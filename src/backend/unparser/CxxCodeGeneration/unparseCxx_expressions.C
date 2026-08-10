/* unparse_expr.C
 *
 * This C file contains the general unparse function for expressions and
 * functions to unparse every kind of expression. Note that there are no
 * definitions for the following functions (Sage didn't provide this): AssnExpr,
 * ExprRoot, AbstractOp, ClassInit, DyCast, ForDecl, VConst, and ExprInit.
 *
 * NOTE: Look over WCharVal. Sage provides no public function to access
 * p_valueUL, so just use p_value for now. When Sage is rebuilt, we should be
 * able to fix this.
 *
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"
#include "sageInterface.h"

#include "unparser.h"

#include <algorithm>
#include <cctype>
#include <limits>

// DQ (2/21/2019): Added to support remove_substring function.
#include <iostream>

#include <string>
#include <vector>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0
#define OUTPUT_HIDDEN_LIST_DATA 0
#define OUTPUT_DEBUGGING_INFORMATION 0

namespace {
[[noreturn]] void
failBuiltinExpressionContract(const char *detail,
                              const SgTypeTraitBuiltinOperator *builtin) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[typed-builtin-expression]: kind=%d name=%s "
          "%s\n",
          static_cast<int>(builtin->get_builtin_operator_kind()),
          builtin->get_name().getString().c_str(), detail);
  ROSE_ABORT();
}

void validateOffsetofDesignator(SgExpression *designator,
                                SgNode *expected_parent,
                                SgTypeTraitBuiltinOperator *builtin) {
  if (designator == nullptr || designator->get_parent() != expected_parent) {
    failBuiltinExpressionContract(
        "has a missing or non-exclusively-owned member designator", builtin);
  }

  if (SgVarRefExp *field = isSgVarRefExp(designator)) {
    if (field->get_symbol() == nullptr ||
        field->get_symbol()->get_declaration() == nullptr) {
      failBuiltinExpressionContract(
          "has a field designator without an exact variable declaration",
          builtin);
    }
    return;
  }
  if (SgNonrealRefExp *field = isSgNonrealRefExp(designator)) {
    if (field->get_symbol() == nullptr) {
      failBuiltinExpressionContract(
          "has a dependent field designator without an exact nonreal symbol",
          builtin);
    }
    return;
  }
  if (SgDotExp *field_path = isSgDotExp(designator)) {
    validateOffsetofDesignator(field_path->get_lhs_operand(), field_path,
                               builtin);
    SgExpression *field = field_path->get_rhs_operand();
    if (isSgVarRefExp(field) == nullptr &&
        isSgNonrealRefExp(field) == nullptr) {
      failBuiltinExpressionContract(
          "has a non-field right operand in its member designator", builtin);
    }
    validateOffsetofDesignator(field, field_path, builtin);
    return;
  }
  if (SgPntrArrRefExp *array = isSgPntrArrRefExp(designator)) {
    validateOffsetofDesignator(array->get_lhs_operand(), array, builtin);
    SgExpression *index = array->get_rhs_operand();
    if (index == nullptr || index->get_parent() != array) {
      failBuiltinExpressionContract(
          "has a missing or non-exclusively-owned array index", builtin);
    }
    return;
  }

  failBuiltinExpressionContract(
      "has an unsupported node in its typed member designator", builtin);
}

SgDeclarationStatement *typeDeclarationForInlineDefinition(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  SgType *base = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if (SgClassType *class_type = isSgClassType(base)) {
    return class_type->get_declaration();
  }
  if (SgEnumType *enum_type = isSgEnumType(base)) {
    return enum_type->get_declaration();
  }
  return nullptr;
}

SgDeclarationStatement *
canonicalDeclarationChainMember(SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }
  return declaration->get_firstNondefiningDeclaration() != nullptr
             ? declaration->get_firstNondefiningDeclaration()
             : declaration;
}

bool expressionOwnsInlineTypeDefinition(
    SgExpression *expression, SgType *type,
    SgDeclarationStatement *type_defining_declaration,
    const char *expression_kind) {
  ASSERT_not_null(expression);
  ASSERT_not_null(expression_kind);
  if (type_defining_declaration == nullptr) {
    SgDeclarationStatement *type_declaration =
        typeDeclarationForInlineDefinition(type);
    SgDeclarationStatement *defining_declaration =
        type_declaration != nullptr
            ? type_declaration->get_definingDeclaration()
            : nullptr;
    if (defining_declaration != nullptr &&
        defining_declaration->get_parent() == expression) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[inline-type-definition-owner]: %s=%p "
              "owns defining declaration=%p without its typed edge\n",
              expression_kind, static_cast<void *>(expression),
              static_cast<void *>(defining_declaration));
      ROSE_ABORT();
    }
    return false;
  }

  SgDeclarationStatement *type_declaration =
      typeDeclarationForInlineDefinition(type);
  SgDeclarationStatement *defining_declaration =
      type_declaration != nullptr ? type_declaration->get_definingDeclaration()
                                  : nullptr;
  if (type_declaration == nullptr || defining_declaration == nullptr ||
      type_defining_declaration != defining_declaration ||
      type_defining_declaration->get_parent() != expression ||
      canonicalDeclarationChainMember(type_declaration) !=
          canonicalDeclarationChainMember(type_defining_declaration)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[inline-type-definition-owner]: %s=%p "
            "type=%p declaration=%p defining=%p owner=%p has no exact "
            "typed definition edge\n",
            expression_kind, static_cast<void *>(expression),
            static_cast<void *>(type), static_cast<void *>(type_declaration),
            static_cast<void *>(type_defining_declaration),
            type_defining_declaration != nullptr
                ? static_cast<void *>(type_defining_declaration->get_parent())
                : nullptr);
    ROSE_ABORT();
  }
  return true;
}

const char *templateParameterKeywordSpelling(
    SgTemplateParameter::template_parameter_keyword_enum keyword) {
  switch (keyword) {
  case SgTemplateParameter::keyword_class:
    return "class";
  case SgTemplateParameter::keyword_typename:
    return "typename";
  case SgTemplateParameter::keyword_unspecified:
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-parameter-keyword]: template "
            "parameter has no exact source keyword\n");
    ROSE_ABORT();
  }
}

SgStatement *templateArgumentQualificationContext(const SgUnparse_Info &info) {
  return info.get_template_argument_qualification_context();
}

SgStatement *exactNameQualificationUseSite(const SgNode *node,
                                           const SgUnparse_Info &info) {
  return exactQualificationUseSiteForEmission(
      node, info.get_template_argument_qualification_context());
}

NameQualificationResult exactNameQualification(Unparser *unparser,
                                               const SgNode *node,
                                               const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  return unparser->u_name->lookup_name_qualification(
      node, exactNameQualificationUseSite(node, info));
}

NameQualificationResult exactTypeQualification(Unparser *unparser,
                                               const SgNode *node,
                                               const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  return unparser->u_name->lookup_type_qualification(
      node, exactNameQualificationUseSite(node, info));
}

bool typeEndsWithTemplateIdClose(const SgType *type) {
  if (type == nullptr) {
    return false;
  }

  if (const SgModifierType *modifier_type = isSgModifierType(type)) {
    return typeEndsWithTemplateIdClose(modifier_type->get_base_type());
  }

  if (isSgPointerType(type) != nullptr ||
      isSgPointerMemberType(type) != nullptr ||
      isSgReferenceType(type) != nullptr ||
      isSgRvalueReferenceType(type) != nullptr ||
      isSgArrayType(type) != nullptr || isSgFunctionType(type) != nullptr ||
      isSgPartialFunctionType(type) != nullptr ||
      isSgMemberFunctionType(type) != nullptr) {
    return false;
  }

  if (isSgTemplateType(type) != nullptr) {
    return true;
  }

  if (const SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    const SgNonrealDecl *decl =
        isSgNonrealDecl(nonreal_type->get_declaration());
    return decl != nullptr && (!decl->get_tpl_args().empty() ||
                               decl->get_nonreal_template_role() ==
                                   SgNonrealDecl::e_nonreal_template_id);
  }

  if (const SgClassType *class_type = isSgClassType(type)) {
    return isSgTemplateInstantiationDecl(class_type->get_declaration()) !=
           nullptr;
  }

  return false;
}

bool templateArgumentEndsWithTemplateIdClose(const SgTemplateArgument *arg) {
  if (arg == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[template-argument]: null template "
                    "argument while classifying closing syntax\n");
    ROSE_ABORT();
  }

  switch (arg->get_argumentType()) {
  case SgTemplateArgument::type_argument:
    return typeEndsWithTemplateIdClose(arg->get_type());

  case SgTemplateArgument::template_template_argument: {
    const SgNonrealDecl *decl = isSgNonrealDecl(arg->get_templateDeclaration());
    return decl != nullptr && (!decl->get_tpl_args().empty() ||
                               decl->get_nonreal_template_role() ==
                                   SgNonrealDecl::e_nonreal_template_id);
  }

  default:
    return false;
  }
}

bool sourceFileRequiresSeparatedTemplateClosers(
    const SgSourceFile *source_file) {
  return source_file != nullptr &&
         (source_file->get_Cxx98_only() || source_file->get_Cxx98_gnu_only() ||
          source_file->get_Cxx03_only() || source_file->get_Cxx03_gnu_only());
}

bool templateParameterEndsWithTemplateIdClose(
    const SgTemplateParameter *template_parameter) {
  if (template_parameter == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[template-parameter]: null template "
                    "parameter while classifying closing syntax\n");
    ROSE_ABORT();
  }

  switch (template_parameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter:
    return typeEndsWithTemplateIdClose(
        template_parameter->get_defaultTypeParameter());

  default:
    return false;
  }
}

std::string exactFunctionBaseName(SgFunctionDeclaration *declaration) {
  if (declaration == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[function-base-name]: null function "
                    "declaration\n");
    ROSE_ABORT();
  }

  SgName name;
  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    name = instantiation->get_templateName();
  } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                 isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    name = instantiation->get_templateName();
  } else {
    name = declaration->get_name();
  }
  if (name.is_null() || name.getString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-base-name]: declaration=%p "
            "type=%s has no exact base name\n",
            static_cast<void *>(declaration),
            declaration->class_name().c_str());
    ROSE_ABORT();
  }
  if ((declaration->get_specialFunctionModifier().isOperator() ||
       declaration->get_specialFunctionModifier().isConversion() ||
       declaration->get_specialFunctionModifier().isUldOperator()) &&
      name.getString().rfind("operator", 0) != 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[operator-base-name]: declaration=%p "
            "type=%s has operator metadata but exact base name='%s'\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            name.getString().c_str());
    ROSE_ABORT();
  }
  return name.getString();
}

bool isBinaryOperatorName(const string &func_name) {
  return func_name == "operator+" || func_name == "operator-" ||
         func_name == "operator*" || func_name == "operator/" ||
         func_name == "operator%" || func_name == "operator^" ||
         func_name == "operator&" || func_name == "operator|" ||
         func_name == "operator=" || func_name == "operator<" ||
         func_name == "operator>" || func_name == "operator+=" ||
         func_name == "operator-=" || func_name == "operator*=" ||
         func_name == "operator/=" || func_name == "operator%=" ||
         func_name == "operator^=" || func_name == "operator&=" ||
         func_name == "operator|=" || func_name == "operator<<" ||
         func_name == "operator>>" || func_name == "operator>>=" ||
         func_name == "operator<<=" || func_name == "operator==" ||
         func_name == "operator!=" || func_name == "operator<=" ||
         func_name == "operator>=" || func_name == "operator<=>" ||
         func_name == "operator&&" || func_name == "operator||" ||
         func_name == "operator," || func_name == "operator->*" ||
         func_name == "operator->" || func_name == "operator()" ||
         func_name == "operator[]";
}

bool isMemberOperatorCall(SgFunctionCallExp *func_call,
                          SgFunctionDeclaration *decl) {
  SgExpression *function = func_call->get_function();
  if (SgDotExp *dot = isSgDotExp(function)) {
    return isSgMemberFunctionRefExp(dot->get_rhs_operand()) != nullptr;
  }
  if (SgArrowExp *arrow = isSgArrowExp(function)) {
    return isSgMemberFunctionRefExp(arrow->get_rhs_operand()) != nullptr;
  }
  if (isSgDotStarOp(function) != nullptr ||
      isSgArrowStarOp(function) != nullptr) {
    return true;
  }

  if (decl == nullptr) {
    return false;
  }

  if (!isNonFriendMemberFunctionDeclaration(decl)) {
    return false;
  }

  return isSgMemberFunctionRefExp(function) == nullptr &&
         isSgTemplateMemberFunctionRefExp(function) == nullptr;
}

SgExpression *directOperatorReference(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgDotExp *dot = isSgDotExp(expr)) {
    return dot->get_rhs_operand();
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    return arrow->get_rhs_operand();
  }
  if (SgDotStarOp *dot_star = isSgDotStarOp(expr)) {
    return dot_star->get_rhs_operand();
  }
  if (SgArrowStarOp *arrow_star = isSgArrowStarOp(expr)) {
    return arrow_star->get_rhs_operand();
  }

  return expr;
}

bool isOverloadedOperatorReference(SgExpression *expr) {
  SgFunctionDeclaration *declaration = nullptr;
  if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    ASSERT_not_null(ref->get_symbol());
    declaration = ref->get_symbol()->get_declaration();
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    ASSERT_not_null(ref->get_symbol());
    declaration = ref->get_symbol()->get_declaration();
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    ASSERT_not_null(ref->get_symbol());
    declaration = ref->get_symbol()->get_declaration();
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    ASSERT_not_null(ref->get_symbol());
    declaration = ref->get_symbol()->get_declaration();
  }

  return declaration != nullptr &&
         declaration->get_specialFunctionModifier().isOperator();
}

bool isUldOperatorCall(const SgUnparse_Info &info,
                       const SgFunctionDeclaration *decl) {
  const SgFunctionCallExp *call = info.get_current_function_call();
  if (call == nullptr ||
      call->get_source_user_defined_literal_suffix().getString().empty()) {
    return false;
  }

  if (decl == nullptr || !call->get_uses_operator_syntax() ||
      !decl->get_specialFunctionModifier().isUldOperator()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[user-defined-literal-call]: typed literal "
            "call has no exact literal-operator declaration\n");
    ROSE_ABORT();
  }
  return true;
}

template <typename RefType>
SgFunctionDeclaration *getReferencedFunctionDeclaration(RefType *ref) {
  if (ref == nullptr || ref->get_symbol() == nullptr ||
      ref->get_symbol()->get_declaration() == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-reference]: reference has no "
            "exact function symbol/declaration\n");
    ROSE_ABORT();
  }

  return ref->get_symbol()->get_declaration();
}

void applyTypeReferenceInfoFromExpression(Unparser *unparser,
                                          SgExpression *expr,
                                          SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  if (expr == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[type-reference]: null expression use "
            "site\n");
    ROSE_ABORT();
  }

  const NameQualificationResult qualification =
      exactTypeQualification(unparser, expr, info);
  info.set_name_qualification_length(qualification.length);
  info.set_global_qualification_required(qualification.global);
  info.set_type_elaboration_required(qualification.typeElaboration);

  if (SgAggregateInitializer *aggregate_init = isSgAggregateInitializer(expr)) {
    if (aggregate_init->get_requiresGlobalNameQualificationOnType()) {
      info.set_requiresGlobalNameQualification();
    }
  }
}
} // namespace

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
void Unparse_ExprStmt::unparseLanguageSpecificExpression(SgExpression *expr,
                                                         SgUnparse_Info &info) {
  // This is the C and C++ specific expression code generation
  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  switch (expr->variant()) {
    // DQ (4/18/2013): I don't think this is ever called this way, IR node
    // resolve to the derived classes not the base classes.
  case UNARY_EXPRESSION: {
    printf("This should never be called: case UNARY_EXPRESSION\n");
    ROSE_ABORT();
  }

    // DQ (4/18/2013): I don't think this is ever called this way, IR node
    // resolve to the derived classes not the base classes.
  case BINARY_EXPRESSION: {
    printf("This should never be called: case BINARY_EXPRESSION \n");
    ROSE_ABORT();
  }

  case VAR_REF: {
    unparseVarRef(expr, info);
    break;
  }
  case OMP_NAME_EXPRESSION: {
    SgOmpNameExpression *name = isSgOmpNameExpression(expr);
    ASSERT_not_null(name);
    if (name->get_spelling().empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-name]: empty OpenMP syntax "
                      "identifier\n");
      ROSE_ABORT();
    }
    curprintLiteral(name->get_spelling());
    break;
  }
  case OMP_SOURCE_EXPRESSION: {
    SgOmpSourceExpression *source = isSgOmpSourceExpression(expr);
    ASSERT_not_null(source);
    if (source->get_spelling().empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-source-expression]: empty "
                      "source spelling\n");
      ROSE_ABORT();
    }
    curprintLiteral(source->get_spelling());
    break;
  }
  case FORTRAN_COMMON_BLOCK_REF_EXP: {
    SgFortranCommonBlockRefExp *reference = isSgFortranCommonBlockRefExp(expr);
    ASSERT_not_null(reference);
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-common-block-language]: /%s/ "
            "reached the C/C++ unparser\n",
            reference->get_use_name().str());
    ROSE_ABORT();
  }
  case MACRO_EXPANSION_EXPR: {
    SgMacroExpansionExp *macro = isSgMacroExpansionExp(expr);
    ASSERT_not_null(macro);
    macro->get_expanded_expression_checked();
    curprintLiteral(macro->get_spelling());
    break;
  }
  case SOURCE_LOCATION_BUILTIN_EXP: {
    SgSourceLocationBuiltinExp *builtin = isSgSourceLocationBuiltinExp(expr);
    ASSERT_not_null(builtin);
    if (builtin->get_expression_type() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-location-builtin-type]: builtin "
              "expression has no semantic result type\n");
      ROSE_ABORT();
    }
    const char *spelling = nullptr;
    switch (builtin->get_kind()) {
    case SgSourceLocationBuiltinExp::e_function:
      spelling = "__builtin_FUNCTION";
      break;
    case SgSourceLocationBuiltinExp::e_function_signature:
      spelling = "__builtin_FUNCSIG";
      break;
    case SgSourceLocationBuiltinExp::e_file:
      spelling = "__builtin_FILE";
      break;
    case SgSourceLocationBuiltinExp::e_file_name:
      spelling = "__builtin_FILE_NAME";
      break;
    case SgSourceLocationBuiltinExp::e_line:
      spelling = "__builtin_LINE";
      break;
    case SgSourceLocationBuiltinExp::e_column:
      spelling = "__builtin_COLUMN";
      break;
    case SgSourceLocationBuiltinExp::e_source_location:
      spelling = "__builtin_source_location";
      break;
    case SgSourceLocationBuiltinExp::e_last_source_location_builtin:
      break;
    }
    if (spelling == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-location-builtin-kind]: invalid "
              "kind=%d\n",
              static_cast<int>(builtin->get_kind()));
      ROSE_ABORT();
    }
    curprint(spelling);
    curprint("()");
    break;
  }
  case CLASSNAME_REF: {
    unparseClassRef(expr, info);
    break;
  }
  case FUNCTION_REF: {
    unparseFuncRef(expr, info);
    break;
  }
  case MEMBER_FUNCTION_REF: {
    unparseMFuncRef(expr, info);
    break;
  }
  case UNSIGNED_INT_VAL: {
    unparseUIntVal(expr, info);
    break;
  }
  case LONG_INT_VAL: {
    unparseLongIntVal(expr, info);
    break;
  }
  case LONG_LONG_INT_VAL: {
    unparseLongLongIntVal(expr, info);
    break;
  }
  case UNSIGNED_LONG_LONG_INT_VAL: {
    unparseULongLongIntVal(expr, info);
    break;
  }
  case UNSIGNED_LONG_INT_VAL: {
    unparseULongIntVal(expr, info);
    break;
  }
  case FLOAT_VAL: {
    unparseFloatVal(expr, info);
    break;
  }
  case LONG_DOUBLE_VAL: {
    unparseLongDoubleVal(expr, info);
    break;
  }
  case FLOAT_16_VAL: {
    unparseFloat16Val(expr, info);
    break;
  }
  case BFLOAT_16_VAL: {
    unparseBFloat16Val(expr, info);
    break;
  }
  case FLOAT_32_VAL: {
    unparseFloat32Val(expr, info);
    break;
  }
  case FLOAT_64_VAL: {
    unparseFloat64Val(expr, info);
    break;
  }
  case FLOAT_80_VAL: {
    unparseFloat80Val(expr, info);
    break;
  }
  case FLOAT_128_VAL: {
    unparseFloat128Val(expr, info);
    break;
  }
  case FUNC_CALL: {
    unparseFuncCall(expr, info);
    break;
  }
  case POINTST_OP: {
    unparsePointStOp(expr, info);
    break;
  }
  case RECORD_REF: {
    unparseRecRef(expr, info);
    break;
  }
  case SUBSCRIPT_EXPR: {
    SgSubscriptExpression *sub_expr = isSgSubscriptExpression(expr);
    ROSE_ASSERT(sub_expr != NULL);
    SgExpression *lower = sub_expr->get_lowerBound();
    SgExpression *upper = sub_expr->get_upperBound();
    SgExpression *stride = sub_expr->get_stride();

    if (isSgNullExpression(lower) == NULL) {
      unparseExpression(lower, info);
    }
    curprint(":");
    if (isSgNullExpression(upper) == NULL) {
      unparseExpression(upper, info);
    }
    if (stride != NULL && isSgNullExpression(stride) == NULL) {
      SgIntVal *integer_stride = isSgIntVal(stride);
      const bool is_unit_stride =
          integer_stride != NULL && integer_stride->get_value() == 1;
      if (!is_unit_stride) {
        curprint(":");
        unparseExpression(stride, info);
      }
    }
    break;
  }
  case DOTSTAR_OP: {
    unparseDotStarOp(expr, info);
    break;
  }
  case ARROWSTAR_OP: {
    unparseArrowStarOp(expr, info);
    break;
  }
  case EQ_OP: {
    unparseEqOp(expr, info);
    break;
  }
  case LT_OP: {
    unparseLtOp(expr, info);
    break;
  }
  case GT_OP: {
    unparseGtOp(expr, info);
    break;
  }
  case NE_OP: {
    unparseNeOp(expr, info);
    break;
  }
  case LE_OP: {
    unparseLeOp(expr, info);
    break;
  }
  case GE_OP: {
    unparseGeOp(expr, info);
    break;
  }
  case ADD_OP: {
    unparseAddOp(expr, info);
    break;
  }
  case SUBT_OP: {
    unparseSubtOp(expr, info);
    break;
  }
  case MULT_OP: {
    unparseMultOp(expr, info);
    break;
  }
  case DIV_OP: {
    unparseDivOp(expr, info);
    break;
  }
  case INTEGER_DIV_OP: {
    unparseIntDivOp(expr, info);
    break;
  }
  case MOD_OP: {
    unparseModOp(expr, info);
    break;
  }
  case AND_OP: {
    unparseAndOp(expr, info);
    break;
  }
  case OR_OP: {
    unparseOrOp(expr, info);
    break;
  }
  case BITXOR_OP: {
    unparseBitXOrOp(expr, info);
    break;
  }
  case BITAND_OP: {
    unparseBitAndOp(expr, info);
    break;
  }
  case BITOR_OP: {
    unparseBitOrOp(expr, info);
    break;
  }
  case COMMA_OP: {
    unparseCommaOp(expr, info);
    break;
  }
  case LSHIFT_OP: {
    unparseLShiftOp(expr, info);
    break;
  }
  case RSHIFT_OP: {
    unparseRShiftOp(expr, info);
    break;
  }
  case UNARY_MINUS_OP: {
    unparseUnaryMinusOp(expr, info);
    break;
  }
  case UNARY_ADD_OP: {
    unparseUnaryAddOp(expr, info);
    break;
  }

  case SIZEOF_OP: {
    unparseSizeOfOp(expr, info);
    break;
  }
    // DQ (6/20/2013): Added alignof operator to support C/C++ extensions.
  case ALIGNOF_OP: {
    unparseAlignOfOp(expr, info);
    break;
  }

    // DQ (2/5/2015): Added missing C++11 support.
  case NOEXCEPT_OP: {
    unparseNoexceptOp(expr, info);
    break;
  }

  case TYPEID_OP: {
    unparseTypeIdOp(expr, info);
    break;
  }
  case NOT_OP: {
    unparseNotOp(expr, info);
    break;
  }
  case DEREF_OP: {
    unparseDerefOp(expr, info);
    break;
  }
  case ADDRESS_OP: {
    unparseAddrOp(expr, info);
    break;
  }
  case MINUSMINUS_OP: {
    unparseMinusMinusOp(expr, info);
    break;
  }
  case PLUSPLUS_OP: {
    unparsePlusPlusOp(expr, info);
    break;
  }
  case BIT_COMPLEMENT_OP: {
    unparseBitCompOp(expr, info);
    break;
  }
  case REAL_PART_OP: {
    unparseRealPartOp(expr, info);
    break;
  }
  case IMAG_PART_OP: {
    unparseImagPartOp(expr, info);
    break;
  }
  case CONJUGATE_OP: {
    unparseConjugateOp(expr, info);
    break;
  }
  case EXPR_CONDITIONAL: {
    unparseExprCond(expr, info);
    break;
  }
  case CAST_OP: {
    unparseCastOp(expr, info);
    break;
  }
  case ARRAY_OP: {
    unparseArrayOp(expr, info);
    break;
  }
  case NEW_OP: {
    applyTypeReferenceInfoFromExpression(unp, expr, info);
    unparseNewOp(expr, info);
    break;
  }
  case DELETE_OP: {
    unparseDeleteOp(expr, info);
    break;
  }
  case THIS_NODE: {
    unparseThisNode(expr, info);
    break;
  }
  case SCOPE_OP: {
    unparseScopeOp(expr, info);
    break;
  }
  case ASSIGN_OP: {
    unparseAssnOp(expr, info);
    break;
  }
  case PLUS_ASSIGN_OP: {
    unparsePlusAssnOp(expr, info);
    break;
  }
  case MINUS_ASSIGN_OP: {
    unparseMinusAssnOp(expr, info);
    break;
  }
  case AND_ASSIGN_OP: {
    unparseAndAssnOp(expr, info);
    break;
  }
  case IOR_ASSIGN_OP: {
    unparseIOrAssnOp(expr, info);
    break;
  }
  case MULT_ASSIGN_OP: {
    unparseMultAssnOp(expr, info);
    break;
  }
  case DIV_ASSIGN_OP: {
    unparseDivAssnOp(expr, info);
    break;
  }
  case MOD_ASSIGN_OP: {
    unparseModAssnOp(expr, info);
    break;
  }
  case XOR_ASSIGN_OP: {
    unparseXorAssnOp(expr, info);
    break;
  }
  case LSHIFT_ASSIGN_OP: {
    unparseLShiftAssnOp(expr, info);
    break;
  }
  case RSHIFT_ASSIGN_OP: {
    unparseRShiftAssnOp(expr, info);
    break;
  }
  case TYPE_REF: {
    unparseTypeRef(expr, info);
    break;
  }
  case EXPR_INIT: {
    unparseExprInit(expr, info);
    break;
  }
  case AGGREGATE_INIT: {
    unparseAggrInit(expr, info);
    break;
  }
  case CONSTRUCTOR_INIT: {
    unparseCtorInit(expr, info);
    break;
  }
  case ASSIGN_INIT: {
    unparseAssnInit(expr, info);
    break;
  }

    // DQ (11/15/2016): Adding support for braced initializer node.
  case BRACED_INIT: {
    unparseBracedInit(expr, info);
    break;
  }

  case THROW_OP: {
    unparseThrowOp(expr, info);
    break;
  }
  case VA_START_OP: {
    unparseVarArgStartOp(expr, info);
    break;
  }
  case VA_START_ONE_OPERAND_OP: {
    unparseVarArgStartOneOperandOp(expr, info);
    break;
  }
  case VA_OP: {
    unparseVarArgOp(expr, info);
    break;
  }
  case VA_END_OP: {
    unparseVarArgEndOp(expr, info);
    break;
  }
  case VA_COPY_OP: {
    unparseVarArgCopyOp(expr, info);
    break;
  }
  case NULL_EXPR: {
    unparseNullExpression(expr, info);
    break;
  }
  case STMT_EXPR: {
    unparseStatementExpression(expr, info);
    break;
  }
  case ASM_OP: {
    unparseAsmOp(expr, info);
    break;
  }
  case DESIGNATED_INITIALIZER: {
    unparseDesignatedInitializer(expr, info);
    break;
  }
  case PSEUDO_DESTRUCTOR_REF: {
    unparsePseudoDtorRef(expr, info);
    break;
  }
  case KERN_CALL: {
    unparseCudaKernelCall(expr, info);
    break;
  }

    // DQ (2/26/2012): Added support for template function calls (member and
    // non-member).
  case TEMPLATE_FUNCTION_REF: {
    unparseTemplateFuncRef(expr, info);
    break;
  }
  case TEMPLATE_MEMBER_FUNCTION_REF: {
    unparseTemplateMFuncRef(expr, info);
    break;
  }

    // DQ (7/21/2012): This is only called if we process C++ code using the
    // Cxx11 option. This can be demonstrated on test2012_133.C (any maybe many
    // other places too).
  case TEMPLATE_PARAMETER_VAL: {
    unparseTemplateParameterValue(expr, info);
    break;
  }

    // DQ (7/12/2013): Added support for unparsing type trait builtin
    // expressions (operators).
  case TYPE_TRAIT_BUILTIN_OPERATOR: {
    unparseTypeTraitBuiltinOperator(expr, info);
    break;
  }

    // DQ (9/4/2013): Added support for compund literals.
  case COMPOUND_LITERAL: {
    unparseCompoundLiteral(expr, info);
    break;
  }

    // DQ (7/24/2014): Added more general support for type expressions (required
    // for C11 generic macro support.
  case TYPE_EXPRESSION: {
    unparseTypeExpression(expr, info);
    break;
  }

    // DQ (7/24/2014): Added more general support for type expressions (required
    // for C11 generic macro support.
  case FUNCTION_PARAMETER_REF_EXP: {
    unparseFunctionParameterRefExpression(expr, info);
    break;
  }

  case LAMBDA_EXP: {
    unparseLambdaExpression(expr, info);
    break;
  }

    // DQ (11/21/2017): Adding support for GNU C/C++ extension for computed goto
    // (and using what was previously only a Fortran IR node to support this).
  case LABEL_REF: {
    unparseLabelRefExpression(expr, info);
    break;
  }
  case NONREAL_REF: {
    unparseNonrealRefExpression(expr, info);
    break;
  }

  case PACK_EXPANSION_EXPR: {
    unparsePackExpansionExpression(expr, info);
    break;
  }

  case REQUIRES_EXPR: {
    unparseRequiresExpr(expr, info);
    break;
  }
  case SIMPLE_REQUIREMENT:
  case TYPE_REQUIREMENT:
  case COMPOUND_REQUIREMENT:
  case NESTED_REQUIREMENT: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[requirement-owner]: %s must be emitted by "
            "its owning SgRequiresExpr\n",
            expr->class_name().c_str());
    ROSE_ABORT();
  }

    // DQ (2/14/2019): Adding support for C++14 void values.
  case VOID_VAL: {
    unparseVoidValue(expr, info);
    break;
  }

    // DQ (7/26/2020): Adding support for C++20 spaceship operator.
  case SPACESHIP_OP: {
    unparseSpaceshipOp(expr, info);
    break;
  }

    // DQ (7/26/2020): Adding support for C++17 fold operator.
  case FOLD_EXPR: {
    unparseFoldExpression(expr, info);
    break;
  }

    // DQ (7/26/2020): Adding support for C++20 fold operator.
  case AWAIT_EXPR: {
    unparseAwaitExpression(expr, info);
    break;
  }

    // DQ (7/26/2020): Adding support for C++20 fold operator.
  case CHOOSE_EXPR: {
    unparseChooseExpression(expr, info);
    break;
  }

  default: {
    // printf ("Default reached in switch statement for unparsing expressions!
    // expr = %p = %s \n",expr,expr->class_name().c_str());
    printf("Default reached in switch statement for unparsing expressions! "
           "expr = %p = %s \n",
           expr, expr->class_name().c_str());
    ROSE_ABORT();
  }
  }

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
}

void Unparse_ExprStmt::unparseVoidValue(SgExpression *, SgUnparse_Info &) {
  fprintf(
      stderr,
      "REX_UNPARSE_INVARIANT[void-value]: SgVoidVal has no source syntax\n");
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseRequiresExpr(SgExpression *expr,
                                           SgUnparse_Info &info) {
  SgRequiresExpr *requires_expr = isSgRequiresExpr(expr);
  ASSERT_not_null(requires_expr);

  SgExprListExp *requirements = requires_expr->get_requirements();
  if (requirements == nullptr || requirements->get_parent() != requires_expr ||
      requirements->get_expressions().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[requires-structure]: requires-expression "
            "has no nonempty owned requirement list\n");
    ROSE_ABORT();
  }

  curprint("requires");
  if (SgFunctionParameterList *parameters =
          requires_expr->get_local_parameter_list()) {
    if (parameters->get_parent() != requires_expr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[requires-parameters]: local parameter "
              "list is not owned by its requires-expression\n");
      ROSE_ABORT();
    }
    curprint("(");
    const SgInitializedNamePtrList &args = parameters->get_args();
    for (size_t index = 0; index < args.size(); ++index) {
      SgInitializedName *parameter = args[index];
      if (parameter == nullptr || parameter->get_parent() != parameters ||
          parameter->get_type() == nullptr ||
          parameter->get_initializer() != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[requires-parameter]: malformed local "
                "parameter at index=%zu\n",
                index);
        ROSE_ABORT();
      }
      if (index != 0) {
        curprint(", ");
      }
      SgUnparse_Info parameter_info(info);
      parameter_info.set_SkipClassDefinition();
      parameter_info.set_SkipEnumDefinition();
      parameter_info.set_template_argument_qualification_context(parameters);
      unp->u_type->outputType<SgInitializedName>(
          parameter, parameter->get_type(), parameter_info);
    }
    curprint(")");
  }

  curprint(" { ");
  for (SgExpression *requirement : requirements->get_expressions()) {
    if (requirement == nullptr || requirement->get_parent() != requirements) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[requires-requirement]: null or unowned "
              "requirement\n");
      ROSE_ABORT();
    }

    SgUnparse_Info requirement_info(info);
    requirement_info.set_SkipClassDefinition();
    requirement_info.set_SkipEnumDefinition();
    if (SgSimpleRequirement *simple = isSgSimpleRequirement(requirement)) {
      if (simple->get_expression() == nullptr ||
          simple->get_expression()->get_parent() != simple) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[simple-requirement]: missing exact "
                "owned expression\n");
        ROSE_ABORT();
      }
      unparseExpression(simple->get_expression(), requirement_info);
      curprint("; ");
    } else if (SgTypeRequirement *type_req = isSgTypeRequirement(requirement)) {
      if (type_req->get_required_type() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[type-requirement]: missing exact "
                "required type\n");
        ROSE_ABORT();
      }
      requirement_info.set_reference_node_for_qualification(type_req);
      applyTypeReferenceInfoFromExpression(unp, type_req, requirement_info);
      // `typename` belongs to the type-requirement grammar, independently of
      // whether the required type is qualified.  The nonreal type unparser
      // recognizes this typed occurrence and must not emit a second keyword.
      curprint("typename ");
      unp->u_type->unparseType(type_req->get_required_type(), requirement_info);
      curprint("; ");
    } else if (SgCompoundRequirement *compound =
                   isSgCompoundRequirement(requirement)) {
      if (compound->get_expression() == nullptr ||
          compound->get_expression()->get_parent() != compound ||
          (compound->get_type_constraint() != nullptr &&
           compound->get_type_constraint()->get_parent() != compound)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[compound-requirement]: malformed "
                "owned expression or type constraint\n");
        ROSE_ABORT();
      }
      curprint("{ ");
      unparseExpression(compound->get_expression(), requirement_info);
      curprint(" }");
      if (compound->get_noexcept_required()) {
        curprint(" noexcept");
      }
      if (SgExpression *constraint = compound->get_type_constraint()) {
        curprint(" -> ");
        unparseExpression(constraint, requirement_info);
      }
      curprint("; ");
    } else if (SgNestedRequirement *nested =
                   isSgNestedRequirement(requirement)) {
      if (nested->get_constraint() == nullptr ||
          nested->get_constraint()->get_parent() != nested) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[nested-requirement]: missing exact "
                "owned constraint\n");
        ROSE_ABORT();
      }
      curprint("requires ");
      unparseExpression(nested->get_constraint(), requirement_info);
      curprint("; ");
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[requires-requirement]: unsupported "
              "typed requirement=%s\n",
              requirement->class_name().c_str());
      ROSE_ABORT();
    }
  }
  curprint("}");
}

void Unparse_ExprStmt::unparseLabelRefExpression(SgExpression *expr,
                                                 SgUnparse_Info &) {
  // DQ (11/21/2017): Adding support for GNU C extension for computed goto.

  SgLabelRefExp *labelRefExp = isSgLabelRefExp(expr);
  ASSERT_not_null(labelRefExp);

  ASSERT_not_null(labelRefExp->get_symbol());

  SgName name = labelRefExp->get_symbol()->get_name();

  curprint("&&");
  curprint(name);
}

void Unparse_ExprStmt::unparseNonrealRefExpression(SgExpression *expr,
                                                   SgUnparse_Info &info) {
  SgNonrealRefExp *nr_refexp = isSgNonrealRefExp(expr);
  ASSERT_not_null(nr_refexp);
  if (nr_refexp->get_resolved_variable_declaration() != nullptr) {
    SageInterface::requireResolvedVariableTemplateReference(nr_refexp,
                                                            "C++ unparser");
  }

  bool uses_operator_syntax = false;
  if (SgFunctionCallExp *call_exp =
          isSgFunctionCallExp(nr_refexp->get_parent())) {
    uses_operator_syntax = call_exp->get_uses_operator_syntax();
  } else if (SgBinaryOp *member_access =
                 isSgBinaryOp(nr_refexp->get_parent())) {
    if (isSgDotExp(member_access) != nullptr ||
        isSgArrowExp(member_access) != nullptr) {
      if (SgFunctionCallExp *call_exp =
              isSgFunctionCallExp(member_access->get_parent())) {
        uses_operator_syntax = call_exp->get_uses_operator_syntax();
      }
    }
  }

  SgName nameQualifier(exactNameQualification(unp, nr_refexp, info).qualifier);
  curprint(nameQualifier.str());

  SgNonrealSymbol *nrsym = nr_refexp->get_symbol();
  ASSERT_not_null(nrsym);

  SgNonrealDecl *nrdecl = nrsym->get_declaration();
  ASSERT_not_null(nrdecl);

  SgTemplateArgumentPtrList &expr_args = nr_refexp->get_templateArguments();
  SgTemplateArgumentPtrList &tpl_args = expr_args;

  string func_name = nrsym->get_name().str();
  if (!unp->opt.get_overload_opt() && uses_operator_syntax &&
      func_name.compare(0, 8, "operator") == 0) {
    const bool is_new_operator = func_name.compare(0, 12, "operator new") == 0;
    const bool is_delete_operator =
        func_name.compare(0, 15, "operator delete") == 0;
    if (!is_new_operator && !is_delete_operator && func_name.size() > 8) {
      string tail = func_name.substr(8);
      size_t first_non_space = tail.find_first_not_of(' ');
      if (first_non_space != string::npos) {
        tail = tail.substr(first_non_space);
      }
      func_name = tail;
    }
  }
  if (nrdecl->get_has_template_keyword()) {
    curprint("template ");
  }
  curprint(func_name);

  if ((!tpl_args.empty() || nr_refexp->get_explicit_template_argument_list()) &&
      !uses_operator_syntax) {
    SgStatement *use_site = info.get_template_argument_qualification_context();
    if (use_site == nullptr) {
      std::cerr << "REX_UNPARSE_INVARIANT[template-reference-use-site]: "
                   "SgNonrealRefExp has no explicit qualification context"
                << std::endl;
      ROSE_ABORT();
    }
    SgUnparse_Info argument_info(info);
    argument_info.set_template_argument_qualification_context(use_site);
    unparseTemplateArgumentList(
        tpl_args, argument_info,
        TemplateArgumentEmission::explicit_source_prefix);
  }
}

void Unparse_ExprStmt::unparsePackExpansionExpression(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  SgPackExpansionExpr *pack_expansion = isSgPackExpansionExpr(expr);
  ASSERT_not_null(pack_expansion);
  ASSERT_not_null(pack_expansion->get_pattern_expression());

  unparseExpression(pack_expansion->get_pattern_expression(), info);
  curprint("...");
}

void Unparse_ExprStmt::unparseLambdaExpression(SgExpression *expr,
                                               SgUnparse_Info &info) {
  SgLambdaExp *lambdaExp = isSgLambdaExp(expr);
  ASSERT_not_null(lambdaExp);

  if (info.SkipBaseType()) {
    fprintf(stderr,
            "REX_UNPARSER_INVARIANT[lambda-unparse-context]: lambda "
            "expression %p inherited the declarator-only SkipBaseType flag\n",
            static_cast<void *>(lambdaExp));
    ROSE_ABORT();
  }

  SgUnparse_Info lambdaInfo(info);

  curprint(" [");
  // if '=' or '&' exists
  bool hasCaptureCharacter = false;
  int commaCounter = 0;

  // schroder3 (2016-08-23): Do not print "&" AND "=" (because "[&=](){}" is
  // ill-formed):
  if (lambdaExp->get_capture_default() == true) {
    if (lambdaExp->get_default_is_by_reference() == true) {
      curprint("&");
    } else {
      curprint("=");
    }
    hasCaptureCharacter = true;
  } else {
    // schroder3 (2016-08-23): Consistency check: If there is no capture default
    // then there should be no
    //  by-reference-capture default:
    ROSE_ASSERT(!lambdaExp->get_default_is_by_reference());
  }

  ASSERT_not_null(lambdaExp->get_lambda_capture_list());
  size_t bound =
      lambdaExp->get_lambda_capture_list()->get_capture_list().size();
  for (size_t i = 0; i < bound; i++) {
    SgLambdaCapture *lambdaCapture =
        lambdaExp->get_lambda_capture_list()->get_capture_list()[i];
    ASSERT_not_null(lambdaCapture);

    // schroder3 (2016-08-23): Do not print implicit captures because this
    // generates ill-formed code if
    //  there is a capture default (C++ standard section [expr.prim.lambda]
    //  point 8) (g++ allows this in non-pedantic mode, clang++ does not).
    //  Example: do not transform "int i; [&](){i;};" to ill-formed "int i; [&,
    //  &i](){i;}"). In addition, this change prevents the printing of "&this"
    //  (which is ill-formed too) when "this" is implicitly captured.
    if (!lambdaCapture->get_implicit() &&
        lambdaCapture->get_capture_variable() != NULL) {

      // Liao 6/24/2016, we output ",item" when
      // When not output , : first comma and there is no previous = or &
      // character
      if (commaCounter == 0) // look backwards one identifier
      {
        if (hasCaptureCharacter)
          curprint(",");
        commaCounter++;
      } else {
        curprint(",");
      }

      SgExpression *capt_var_expr = lambdaCapture->get_capture_variable();
      ASSERT_not_null(capt_var_expr);

      // TV (11/14/2018): ROSE-1525: Handle explicit "this" capture.
      if (isSgThisExp(capt_var_expr)) {
        if (lambdaCapture->get_capture_by_reference() == false) {
          curprint("*");
        }
        curprint("this");
        if (lambdaCapture->get_pack_expansion()) {
          curprint("...");
        }
      } else {
        SgExpression *capture_init_expr =
            lambdaCapture->get_source_closure_variable();
        bool is_init_capture = (capture_init_expr != nullptr);

        if (lambdaCapture->get_capture_by_reference() == true) {
          curprint("&");
        }
        if (is_init_capture && lambdaCapture->get_pack_expansion()) {
          curprint("...");
        }
        unp->u_exprStmt->unparseExpression(capt_var_expr, lambdaInfo);
        if (!is_init_capture && lambdaCapture->get_pack_expansion()) {
          curprint("...");
        }
        if (is_init_capture) {
          curprint("=");
          unp->u_exprStmt->unparseExpression(capture_init_expr, lambdaInfo);
        }
      }
    }
  }
  curprint("] ");

  SgFunctionDeclaration *lambdaFunction = lambdaExp->get_lambda_function();
  ASSERT_not_null(lambdaFunction);
  ASSERT_not_null(lambdaFunction->get_firstNondefiningDeclaration());
  ASSERT_not_null(lambdaFunction->get_definingDeclaration());

  if (lambdaFunction->get_functionModifier().isCudaHost()) {
    curprint("__host__ ");
  }
  if (lambdaFunction->get_functionModifier().isCudaKernel()) {
    curprint("__global__ ");
  }
  if (lambdaFunction->get_functionModifier().isCudaDevice()) {
    curprint("__device__ ");
  }

  if (lambdaExp->get_has_parameter_decl() == true) {
    // Output the function parameters
    curprint("(");
    unparseFunctionArgs(lambdaFunction, lambdaInfo);
    curprint(")");
  }

  if (lambdaExp->get_is_mutable() == true) {
    curprint(" mutable ");
  }

  if (lambdaExp->get_explicit_return_type() == true) {
    curprint(" -> ");
    ASSERT_not_null(lambdaFunction);
    SgType *returnType = lambdaFunction->get_orig_return_type();
    ASSERT_not_null(returnType);
    SgUnparse_Info returnTypeInfo(lambdaInfo);
    returnTypeInfo.set_reference_node_for_qualification(lambdaFunction);
    const NameQualificationResult returnTypeQualification =
        unp->u_name->lookup_type_qualification(
            lambdaFunction,
            exactNameQualificationUseSite(lambdaExp, lambdaInfo));
    returnTypeInfo.set_name_qualification_length(
        returnTypeQualification.length);
    returnTypeInfo.set_global_qualification_required(
        returnTypeQualification.global);
    returnTypeInfo.set_type_elaboration_required(
        returnTypeQualification.typeElaboration);
    if (lambdaFunction->get_requiresNameQualificationOnReturnType()) {
      returnTypeInfo.set_requiresGlobalNameQualification();
    }
    unp->u_type->unparseType(returnType, returnTypeInfo);
  }

  // Use a new SgUnparse_Info object to support supression of the SgThisExp
  // where compiler generated. This is required because the function is
  // internally a member function but can't explicitly refer to a "this"
  // expression.
  SgUnparse_Info ninfo(lambdaInfo);
  ninfo.set_supressImplicitThisOperator();

  // DQ (2/19/2018): Need to unset the support to skip the function definitions
  // so that the unparsing of the block will allow comments and CPP directives
  // to be output.
  ninfo.unset_SkipEnumDefinition();
  ninfo.unset_SkipClassDefinition();
  ninfo.unset_SkipFunctionDefinition();
  // A lambda body is a complete statement context even when the surrounding
  // expression is emitted as a declaration initializer or another context
  // that suppresses its own trailing semicolon.
  ninfo.unset_SkipSemiColon();

  // Output the function definition
  ASSERT_not_null(lambdaFunction->get_definition());
  unparseStatement(lambdaFunction->get_definition()->get_body(), ninfo);
}

// DQ (8/11/2014): Added more general support for function parameter expressions
// (required for C++11 support).
void Unparse_ExprStmt::unparseFunctionParameterRefExpression(SgExpression *expr,
                                                             SgUnparse_Info &) {
  ASSERT_not_null(expr);

  SgFunctionParameterRefExp *functionParameterRefExp =
      isSgFunctionParameterRefExp(expr);
  ASSERT_not_null(functionParameterRefExp);

  // DQ (2/14/2015): We at least require this sort of funcationality for C++11
  // test2015_13.C.
  if (functionParameterRefExp->get_parameter_number() == 0 &&
      functionParameterRefExp->get_parameter_levels_up() == 0) {
    unp->u_exprStmt->curprint("this ");
    return;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[function-parameter-reference]: unsupported "
          "parameter=%d levels-up=%d cannot be emitted as empty text\n",
          functionParameterRefExp->get_parameter_number(),
          functionParameterRefExp->get_parameter_levels_up());
  ROSE_ABORT();
}

// DQ (7/24/2014): Added more general support for type expressions (required for
// C11 generic macro support).
void Unparse_ExprStmt::unparseTypeExpression(SgExpression *expr,
                                             SgUnparse_Info &info) {
  SgTypeExpression *type_exp = isSgTypeExpression(expr);
  ASSERT_not_null(type_exp);
  SgType *type = type_exp->get_represented_type();
  ASSERT_not_null(type);

  SgUnparse_Info info_(info);
  info_.set_SkipClassDefinition();
  info_.set_SkipEnumDefinition();
  info_.unset_SkipBaseType();
  info_.unset_isTypeFirstPart();
  info_.unset_isTypeSecondPart();
  info_.set_reference_node_for_qualification(type_exp);
  applyTypeReferenceInfoFromExpression(unp, type_exp, info_);
  unp->u_type->unparseType(type, info_);
}

namespace {

std::string instantiatedConversionOperatorName(
    SgFunctionDeclaration *function_declaration) {
  if (function_declaration == nullptr ||
      !function_declaration->get_specialFunctionModifier().isConversion()) {
    return "";
  }

  const std::string function_name = exactFunctionBaseName(function_declaration);
  if (function_name.rfind("operator ", 0) != 0 ||
      function_name.size() == std::string("operator ").size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[conversion-operator-name]: "
            "declaration=%p type=%s has conversion metadata but exact base "
            "name='%s'\n",
            static_cast<void *>(function_declaration),
            function_declaration->class_name().c_str(), function_name.c_str());
    ROSE_ABORT();
  }
  return function_name;
}

} // namespace

// DQ (7/21/2012): Added support for new template IR nodes (only used in C++11
// code so far, see test2012_133.C).
void Unparse_ExprStmt::unparseTemplateParameterValue(SgExpression *expr,
                                                     SgUnparse_Info &) {
  SgTemplateParameterVal *template_parameter_value =
      isSgTemplateParameterVal(expr);
  ASSERT_not_null(template_parameter_value);
  if (template_parameter_value->get_valueString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-parameter-value]: typed template "
            "parameter value has no exact token spelling\n");
    ROSE_ABORT();
  }
  curprint(template_parameter_value->get_valueString());
}

// DQ (4/25/2012): Added support for new template IR nodes.
void Unparse_ExprStmt::unparseTemplateFuncRef(SgExpression *expr,
                                              SgUnparse_Info &info) {
  SgTemplateFunctionRefExp *func_ref = isSgTemplateFunctionRefExp(expr);
  ASSERT_not_null(func_ref);

  if (isUldOperatorCall(info, getReferencedFunctionDeclaration(func_ref))) {
    return;
  }

  // Calling the template function unparseFuncRef<SgFunctionRefExp>(func_ref);
  unparseFuncRefSupport<SgTemplateFunctionRefExp>(expr, info);
}

// DQ (4/25/2012): Added support for new template IR nodes.
void Unparse_ExprStmt::unparseTemplateMFuncRef(SgExpression *expr,
                                               SgUnparse_Info &info) {
  SgTemplateMemberFunctionRefExp *mfunc_ref =
      isSgTemplateMemberFunctionRefExp(expr);
  ASSERT_not_null(mfunc_ref);

  if (isUldOperatorCall(info, getReferencedFunctionDeclaration(mfunc_ref))) {
    return;
  }

  unparseMFuncRefSupport<SgTemplateMemberFunctionRefExp>(expr, info);
}

std::string Unparse_ExprStmt::requireCompleteClassTemplateId(
    SgTemplateInstantiationDecl *declaration) const {
  ASSERT_not_null(declaration);

  const std::string template_name = declaration->get_templateName().str();
  if (template_name.empty()) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[template-instantiation-identity]: "
                    "template name is empty\n");
    ROSE_ABORT();
  }
  if (template_name.find_first_of("<>") != std::string::npos) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[template-instantiation-identity]: "
                    "template name contains template-id punctuation\n");
    ROSE_ABORT();
  }

  const std::string template_id = declaration->get_name().str();
  const size_t payload_begin = template_name.size() + 1;
  if (template_id.size() < template_name.size() + 2 ||
      template_id.compare(0, template_name.size(), template_name) != 0 ||
      template_id[template_name.size()] != '<' || template_id.back() != '>') {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-instantiation-identity]: stored "
            "declaration name is not a complete template-id rooted at the "
            "template name\n");
    ROSE_ABORT();
  }

  const bool stored_payload_is_empty = payload_begin + 1 == template_id.size();
  const bool injected_class_name_spelling =
      declaration->get_sourceSpellsInjectedClassName();
  if (injected_class_name_spelling) {
    if (stored_payload_is_empty ||
        !declaration->get_templateArguments().empty() ||
        declaration->get_semanticTemplateArguments().empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[injected-class-name-spelling]: "
                      "declaration does not preserve one complete semantic "
                      "template-id, an empty written argument surface, and a "
                      "nonempty semantic argument surface\n");
      ROSE_ABORT();
    }
  } else if (stored_payload_is_empty !=
             declaration->get_templateArguments().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-instantiation-identity]: typed "
            "argument list disagrees with the stored template-id payload\n");
    ROSE_ABORT();
  }
  for (SgTemplateArgument *argument : declaration->get_templateArguments()) {
    if (argument == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-instantiation-identity]: typed "
              "argument list contains a null argument\n");
      ROSE_ABORT();
    }
  }

  return template_id;
}

void Unparse_ExprStmt::unparseTemplateName(
    SgTemplateInstantiationDecl *templateInstantiationDeclaration,
    SgUnparse_Info &info) {
  ASSERT_not_null(templateInstantiationDeclaration);

  (void)requireCompleteClassTemplateId(templateInstantiationDeclaration);

  // DQ (5/7/2013): I think these should be false so that the full type will be
  // output.
  ROSE_ASSERT(info.isTypeSecondPart() == false);

  unp->u_exprStmt->curprint(
      templateInstantiationDeclaration->get_templateName().str());

  if (templateInstantiationDeclaration->get_sourceSpellsInjectedClassName()) {
    return;
  }

  if (templateInstantiationDeclaration->get_templateArguments().empty()) {
    unp->u_exprStmt->curprint("<>");
    return;
  }

  // DQ (6/21/2011): Refactored this code to generate more than templated class
  // names.
  unparseTemplateArgumentList(
      templateInstantiationDeclaration->get_templateArguments(), info,
      TemplateArgumentEmission::complete_typed_identity);
}

void Unparse_ExprStmt::unparseTemplateFunctionName(
    SgTemplateInstantiationFunctionDecl
        *templateInstantiationFunctionDeclaration,
    SgUnparse_Info &info) {
  // DQ (6/21/2011): Generated this function from refactored call to
  // unparseTemplateArgumentList
  ASSERT_not_null(templateInstantiationFunctionDeclaration);

  if (isUldOperatorCall(info, templateInstantiationFunctionDeclaration)) {
    return;
  }

  const std::string function_name =
      exactFunctionBaseName(templateInstantiationFunctionDeclaration);

  unp->u_exprStmt->curprint(function_name);

  bool unparseTemplateArguments =
      templateInstantiationFunctionDeclaration
          ->get_template_argument_list_is_explicit();

  // DQ (6/29/2013): Use the information recorded in the AST as to if this
  // function has been used with template arguments in the original code.  If so
  // then we always unparse the template arguments, if not then we never unparse
  // the template arguments.  See test2013_242.C for an example of where this is
  // significant in the generated code.  Note that this goes a long way toward
  // making the generated code look more like the original input code (where
  // before we have always unparsed template arguments resulting in some very
  // long function calls in the generated code).  Note that if some template
  // arguments are specified and some are not then control over not unparsing
  // template arguments that where not explicit in the original code will be
  // handled separately in the near future (in the SgTemplateArgument IR nodes).
  if (unparseTemplateArguments == true) {
    if (templateInstantiationFunctionDeclaration->get_specialFunctionModifier()
            .isOperator()) {
      curprint(" ");
    }
    if (templateInstantiationFunctionDeclaration->get_templateArguments()
            .empty()) {
      unp->u_exprStmt->curprint("<>");
      return;
    }
    unparseTemplateArgumentList(
        templateInstantiationFunctionDeclaration->get_templateArguments(), info,
        TemplateArgumentEmission::explicit_source_prefix);
  }
}

void Unparse_ExprStmt::unparseTemplateMemberFunctionName(
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDeclaration,
    SgUnparse_Info &info) {
  // DQ (5/25/2013): Generated this function to match that of
  // unparseTemplateFunctionName().
  ASSERT_not_null(templateInstantiationMemberFunctionDeclaration);

  if (isUldOperatorCall(info, templateInstantiationMemberFunctionDeclaration)) {
    return;
  }

  const string function_name =
      exactFunctionBaseName(templateInstantiationMemberFunctionDeclaration);

  unp->u_exprStmt->curprint(function_name);

  // DQ (5/26/2013): test2013_194.C demonstrates that we need to drop the
  // template argument list for the case of a constructor (I think). I think
  // that this applies to constructors, destructors, and conversion operators,
  // but I am not sure...
  bool isConstructor = templateInstantiationMemberFunctionDeclaration
                           ->get_specialFunctionModifier()
                           .isConstructor();
  bool isDestructor = templateInstantiationMemberFunctionDeclaration
                          ->get_specialFunctionModifier()
                          .isDestructor();
  bool isConversionOperator = templateInstantiationMemberFunctionDeclaration
                                  ->get_specialFunctionModifier()
                                  .isConversion();

  // DQ (5/26/2013): Output output the template argument list when this is not a
  // constructor, destructor, or conversion operator.
  bool skipTemplateArgumentList =
      (isConstructor == true || isDestructor == true ||
       isConversionOperator == true);

  // DQ (6/29/2013): Use the information recorded in the AST as to if this
  // function has been used with template arguments in the original code.  See
  // note in unparseTemplateFunctionName().
  bool unparseTemplateArguments =
      templateInstantiationMemberFunctionDeclaration
          ->get_template_argument_list_is_explicit();
  if (unparseTemplateArguments == false) {
    skipTemplateArgumentList = true;
  }

  if (skipTemplateArgumentList == false) {
    if (templateInstantiationMemberFunctionDeclaration
            ->get_specialFunctionModifier()
            .isOperator()) {
      curprint(" ");
    }
    if (templateInstantiationMemberFunctionDeclaration->get_templateArguments()
            .empty()) {
      unp->u_exprStmt->curprint("<>");
      return;
    }
    unparseTemplateArgumentList(
        templateInstantiationMemberFunctionDeclaration->get_templateArguments(),
        info, TemplateArgumentEmission::explicit_source_prefix);
  }
}

void Unparse_ExprStmt::unparseTemplateArgumentList(
    const SgTemplateArgumentPtrList &input_templateArgListPtr,
    SgUnparse_Info &info, TemplateArgumentEmission emission) {
#define DEBUG_TEMPLATE_ARGUMENT_LIST 0

#if DEBUG_TEMPLATE_ARGUMENT_LIST
  printf(
      "In unparseTemplateArgumentList(): templateArgListPtr.size() = %" PRIuPTR
      " \n",
      input_templateArgListPtr.size());
#endif

  SgUnparse_Info ninfo(info);
  SgStatement *qualification_context =
      templateArgumentQualificationContext(info);
  if (!input_templateArgListPtr.empty() && !info.SkipQualifiedNames()) {
    ASSERT_not_null(qualification_context);
  }
  ninfo.set_template_argument_qualification_context(qualification_context);

  // DQ (5/6/2013): This fixes the test2013_153.C test code.
  if (ninfo.isTypeFirstPart() == true) {
    ninfo.unset_isTypeFirstPart();
  }

  if (ninfo.isTypeSecondPart() == true) {
    ninfo.unset_isTypeSecondPart();
  }

  // DQ (5/6/2013): I think these should be false so that the full type will be
  // output.
  ROSE_ASSERT(ninfo.isTypeFirstPart() == false);
  ROSE_ASSERT(ninfo.isTypeSecondPart() == false);

  // DQ (2/10/2019): Make a copy to support removing the
  // start_of_pack_expansion_argument which has been complicccated to deal with
  // in unparsing. const SgTemplateArgumentPtrList templateArgListPtr =
  // input_templateArgListPtr;
  SgTemplateArgumentPtrList templateArgListPtr;
  SgTemplateArgumentPtrList::const_iterator copy_iter =
      input_templateArgListPtr.begin();

#if DEBUG_TEMPLATE_ARGUMENT_LIST
  printf("In unparseTemplateArgumentList(): iterate over list: \n");
#endif

  // DQ (2/11/2019): Need to control use of empty <> in template argument list
  // handling. Even if we filter out template arguments, it should not be
  // considered an empty list.
  bool isEmptyTemplateArgumentList = true;

  bool saw_non_explicit_argument = false;
  while (copy_iter != input_templateArgListPtr.end()) {
    // DQ (2/11/2019): Need to control use of empty <> in template argument list
    // handling.
    isEmptyTemplateArgumentList = false;

    SgTemplateArgument *tplarg = *copy_iter;
    ASSERT_not_null(tplarg);
#if DEBUG_TEMPLATE_ARGUMENT_LIST
    printf(" - tplarg = %p argument kind = %d\n", tplarg,
           static_cast<int>(tplarg->get_argumentType()));
#endif

    if (tplarg->get_argumentType() ==
        SgTemplateArgument::start_of_pack_expansion_argument) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-argument-prefix]: raw pack "
              "boundary marker reached the typed argument list\n");
      ROSE_ABORT();
    }

    if (tplarg->get_sourceSpelledType() != nullptr &&
        (tplarg->get_argumentType() != SgTemplateArgument::type_argument ||
         !tplarg->get_explicitlySpecified())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-argument-source-type]: exact "
              "source type belongs only to an explicit type argument\n");
      ROSE_ABORT();
    }

    if (!tplarg->get_explicitlySpecified()) {
      saw_non_explicit_argument = true;
    } else {
      if (saw_non_explicit_argument) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-argument-prefix]: explicit "
                "argument follows a non-explicit semantic argument\n");
        ROSE_ABORT();
      }
    }

    if (emission == TemplateArgumentEmission::complete_typed_identity ||
        tplarg->get_explicitlySpecified()) {
      templateArgListPtr.push_back(tplarg);
    }

    copy_iter++;
  }

  bool use_compact_template_brackets = true;
  // Template-closer syntax belongs to the output language mode. A moved or
  // synthesized declaration can consume arguments whose semantic owner is a
  // different source file, so that owner is only a last-resort API context.
  SgSourceFile *source_file = info.get_current_source_file();
  if (source_file == nullptr &&
      info.get_reference_node_for_qualification() != nullptr) {
    source_file = SageInterface::getEnclosingSourceFile(
        info.get_reference_node_for_qualification(), true);
  }
  if (source_file == nullptr && !templateArgListPtr.empty()) {
    source_file = SageInterface::getEnclosingSourceFile(
        templateArgListPtr.front()->get_parent(), true);
  }
  if (sourceFileRequiresSeparatedTemplateClosers(source_file)) {
    use_compact_template_brackets = false;
  }

  // DQ (2/11/2019): Need to control use of empty <> in template argument list
  // handling.
  if (isEmptyTemplateArgumentList == false) {
    // DQ (2/11/2019): Moved to outside of the loop over all template
    // parameters.
    unp->u_exprStmt->curprint("<");
    // DQ (2/22/2019): Added assertion.  This fails for test2019_93.C and
    // test2019_100.C E.g. template<class ... Types> struct Tuple {}; Tuple<>
    // t0; ROSE_ASSERT(templateArgListPtr.empty() == false);
  } else {
    // DQ (2/22/2019): Added assertion.
    ROSE_ASSERT(templateArgListPtr.empty() == true);
  }

  bool last_argument_requires_spacing_before_close = false;
  bool emitted_arg = false;
  if (templateArgListPtr.empty() == false) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST
    printf("In unparseTemplateArgumentList(): templateArgListPtr.empty() NOT "
           "EMPTY: templateArgListPtr.size() = %" PRIuPTR " \n",
           templateArgListPtr.size());
#endif
#if DEBUG_TEMPLATE_ARGUMENT_LIST || 0
    printf("In unparseTemplateArgumentList(): iterate over list: \n");
    SgTemplateArgumentPtrList::const_iterator iter = templateArgListPtr.begin();
    while (iter != templateArgListPtr.end()) {
      ASSERT_not_null(*iter);

      // printf ("In unparseTemplateArgumentList(): iterate over list: *iter =
      // %p = %s \n",*iter,(*iter)->class_name().c_str());
      printf(" --- *iter = %p = %s \n", *iter, (*iter)->class_name().c_str());
      printf(" --- *iter kind = %s \n",
             (*iter)->template_argument_kind().c_str());

      iter++;
    }

    // printf ("Calling unparseToStringSupport(): \n");
    // templateArgListPtr.unparseToStringSupport();
#endif
    // DQ (4/18/2005): We would like to avoid output of "<>" if possible so
    // verify that there are template arguments
    ROSE_ASSERT(templateArgListPtr.size() > 0);

    // DQ (5/6/2013): I think these should be false so that the full type will
    // be output.
    ROSE_ASSERT(ninfo.isTypeFirstPart() == false);
    ROSE_ASSERT(ninfo.isTypeSecondPart() == false);

    // DQ (2/11/2019): Moved to outside of the loop over all template
    // parameters. unp->u_exprStmt->curprint ( "< ");
    SgTemplateArgumentPtrList::const_iterator i = templateArgListPtr.begin();
    while (i != templateArgListPtr.end()) {
      if ((*i)->get_argumentType() ==
          SgTemplateArgument::start_of_pack_expansion_argument) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-argument]: raw pack-start "
                "sentinel reached the emitted argument list\n");
        ROSE_ABORT();
      }

#if DEBUG_TEMPLATE_ARGUMENT_LIST
      printf("In unparseTemplateArgumentList(): templateArgList element *i = "
             "%p = %s explicitlySpecified = %s \n",
             *i, (*i)->class_name().c_str(),
             ((*i)->get_explicitlySpecified() == true) ? "true" : "false");
#endif

#if DEBUG_TEMPLATE_ARGUMENT_LIST
      printf("In unparseTemplateArgumentList(): Calling "
             "unparseTemplateArgument(): *i = %p = %s \n",
             *i, (*i)->class_name().c_str());
#endif
      SgUnparse_Info arg_info(ninfo);
      arg_info.set_declstatement_ptr(NULL);
      arg_info.set_current_context(NULL);
      arg_info.set_reference_node_for_qualification(*i);
      arg_info.set_SkipClassDefinition();
      arg_info.set_SkipEnumDefinition();
      arg_info.set_use_generated_name_for_template_arguments(true);
      // Emit the argument within this unparser invocation. Calling the public
      // `unparseToString` API here starts a second name-qualification traversal
      // while the first traversal is still computing this exact use site.
      unparseTemplateArgument(*i, arg_info);
      last_argument_requires_spacing_before_close =
          templateArgumentEndsWithTemplateIdClose(*i);
      emitted_arg = true;

#if DEBUG_TEMPLATE_ARGUMENT_LIST
      printf("In unparseTemplateArgumentList(): DONE: Calling "
             "unparseTemplateArgument(): *i = %p = %s \n",
             *i, (*i)->class_name().c_str());
#endif

      i++;

      // When to output , ?  the argument must not be the last one.
      if (i != templateArgListPtr.end()) {
        // check if this is a class type for C++ 11 lambda function
        // bool hasLambdaFollowed = false;
        SgTemplateArgument *arg = *i;
        if (arg != NULL) {
          // DQ (1/25/2019): Added assertion.
          // ASSERT_not_null(arg->get_type());
          if (SgClassType *ctype = isSgClassType(arg->get_type())) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST
            printf("ctype != NULL \n");
#endif
            if (SgNode *pnode = ctype->get_declaration()->get_parent()) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST
              printf("pnode != NULL \n");
#endif
              if (isSgLambdaExp(pnode)) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST || 0
                printf("isSgLambdaExp(pnode) != NULL (also set "
                       "hasLambdaFollowed = true) \n");
#endif
                // hasLambdaFollowed = true;
              }
            }

            // DQ (1/21/2018): Check if this is an unnamed class (used as a
            // template argument, which is not alloweded, so we should not
            // unparse it).
            bool isAnonymous = isAnonymousClass(ctype);
            if (isAnonymous == true) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST || 0
              printf(
                  "isAnonymous == true (also set hasLambdaFollowed = true) \n");
#endif
              // DQ (1/21/2018): This is mixing logic for explicitlySpecified
              // with something Liao introduced which checks for a trailing
              // lambda function.  So we should fix this up later.
              // hasLambdaFollowed = true;
            }
          }
        }

#if DEBUG_TEMPLATE_ARGUMENT_LIST
        // printf ("In unparseTemplateArgumentList(): templateArgList element *i
        // = %p = %s hasLambdaFollowed = %s \n",*i,(*i)->class_name().c_str(),
        // hasLambdaFollowed ? "true" : "false");
        printf("In unparseTemplateArgumentList(): templateArgList element *i = "
               "%p = %s \n",
               *i, (*i)->class_name().c_str());
        printf("In unparseTemplateArgumentList(): explicitlySpecified = %s \n",
               (*i)->get_explicitlySpecified() ? "true" : "false");
#endif

        // When to skip , ?
        // condition 1: next item is a lambda function
        //  Or condition 2:  next item is an ending parameter pack argument
        //  (parameter pack argument in the middle should have , )
        //
        //  Negate this , we get when to output ,
        //

        // DQ (2/8/2019): The start_of_pack_expansion_argument can appear
        // anywhere in the list (see Cxx11_tests/test2019_97.C), so we can't
        // break out of the loop the first time we see it.

        // DQ (1/25/2019): This might be the simpliest way to exit once we see a
        // start_of_pack_expansion_argument.
        if ((*i)->get_argumentType() ==
            SgTemplateArgument::start_of_pack_expansion_argument) {
#if DEBUG_TEMPLATE_ARGUMENT_LIST
          printf("Calling break: This might be the simpliest way to exit once "
                 "we see a start_of_pack_expansion_argument \n");
#endif
          break;
        }

#if DEBUG_TEMPLATE_ARGUMENT_LIST || 0
        // printf ("In unparseTemplateArgumentList(): hasLambdaFollowed = %s
        // \n",hasLambdaFollowed ? "true" : "false");
        printf("(i+1) == templateArgListPtr.end() = %s \n",
               (i + 1) == templateArgListPtr.end() ? "true" : "false");
#endif
        // DQ (2/11/2019): With the simpler logic we don't have to have this be
        // anything more than true. if (!(hasLambdaFollowed  ||
        // ((*i)->get_argumentType() ==
        // SgTemplateArgument::start_of_pack_expansion_argument && ((i+1)==
        // templateArgListPtr.end())  )) ) if ( !(hasLambdaFollowed  ||
        // isStartOfPragmaPackAtEndOfList) )
        if (true) {
          // unp->u_exprStmt->curprint(" /* output comma: part 1 */ ");
          unp->u_exprStmt->curprint(", ");
        } else {
#if DEBUG_TEMPLATE_ARGUMENT_LIST
          printf("In unparseTemplateArgumentList(): Skipping output of a "
                 "specific template argument \n");
#endif
        }
      }
    }

    // DQ (2/11/2019): Moved to outside of the loop over all template
    // parameters. unp->u_exprStmt->curprint(" > ");
  } else {
    // DQ (5/26/2014): In the case of a template instantiation with empty
    // template argument list, output a " " to be consistent with the behavior
    // when there is a non-empty template argument list. This is a better fix
    // for the template issue that Robb pointed out and that was fixed last
    // week.
    if (isEmptyTemplateArgumentList == true) {
      unp->u_exprStmt->curprint(use_compact_template_brackets ? "<>" : "< > ");
    } else {
      unp->u_exprStmt->curprint(" ");
    }
  }

  // DQ (2/11/2019): Need to control use of empty <> in template argument list
  // handling.
  if (isEmptyTemplateArgumentList == false) {
    // DQ (2/11/2019): Moved to outside of the loop over all template
    // parameters.
    bool needs_space_before_close = false;
    if (!use_compact_template_brackets && emitted_arg) {
      needs_space_before_close = last_argument_requires_spacing_before_close;
    }
    unp->u_exprStmt->curprint(needs_space_before_close ? " >" : ">");
  }

#if DEBUG_TEMPLATE_ARGUMENT_LIST
  printf("Leaving Unparse_ExprStmt::unparseTemplateArgumentList(): CRITICAL "
         "FUNCTION TO BE REFACTORED \n");
#endif
}

void Unparse_ExprStmt::unparseTemplateParameterList(
    const SgTemplateParameterPtrList &templateParameterList,
    SgUnparse_Info &info, bool is_template_header,
    SgDeclarationStatement *template_declaration) {

  if (templateParameterList.empty() == false) {
    bool use_compact_template_brackets = true;
    // As for template arguments above, the active output file owns the
    // language mode; a parameter's semantic declaration is not an emission
    // context.
    SgSourceFile *source_file = info.get_current_source_file();
    if (source_file == nullptr &&
        info.get_reference_node_for_qualification() != nullptr) {
      source_file = SageInterface::getEnclosingSourceFile(
          info.get_reference_node_for_qualification(), true);
    }
    if (source_file == nullptr && info.get_declstatement_ptr() != nullptr) {
      source_file = SageInterface::getEnclosingSourceFile(
          info.get_declstatement_ptr(), true);
    }
    if (sourceFileRequiresSeparatedTemplateClosers(source_file)) {
      use_compact_template_brackets = false;
    }

    curprint("<");
    SgTemplateParameterPtrList::const_iterator i =
        templateParameterList.begin();
    while (i != templateParameterList.end()) {
      SgTemplateParameter *templateParameter = *i;
      ASSERT_not_null(templateParameter);
      SgUnparse_Info parameter_info(info);
      if (template_declaration != nullptr) {
        parameter_info.set_declstatement_ptr(template_declaration);
        parameter_info.set_template_argument_qualification_context(
            template_declaration);
      }
      unparseTemplateParameter(templateParameter, parameter_info,
                               is_template_header, template_declaration);

      i++;

      if (i != templateParameterList.end()) {
        curprint(", ");
      }
    }

    const bool needs_space_before_close =
        !use_compact_template_brackets &&
        templateParameterEndsWithTemplateIdClose(templateParameterList.back());
    curprint(needs_space_before_close ? " >" : ">");
  }
}

void Unparse_ExprStmt::unparseTemplateParameter(
    SgTemplateParameter *templateParameter, SgUnparse_Info &info,
    bool is_template_header, SgDeclarationStatement *template_declaration) {
  ASSERT_not_null(templateParameter);

  // The frontend publishes declaration-local template parameter surfaces:
  // inherited defaults are absent, and only defaults spelled on this source
  // declaration remain attached. The unparser must emit that exact structure
  // without scanning redeclaration peers or suppressing repeated pointers.
  const bool emit_default_template_arg = is_template_header;

  switch (templateParameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter: {
    // DQ (9/7/2014): Added support for case
    // SgTemplateParameter::type_parameter.
    SgType *type = templateParameter->get_type();
    ASSERT_not_null(type);

    bool is_pack = templateParameter->get_is_parameter_pack();
    if (SgTemplateType *ttype = isSgTemplateType(type)) {
      is_pack = is_pack || ttype->get_packed();
    }

    std::string parameter_name;
    if (SgNonrealType *nrtype = isSgNonrealType(type)) {
      parameter_name = nrtype->get_name().getString();
    } else if (SgTemplateType *ttype = isSgTemplateType(type)) {
      parameter_name = ttype->get_name().getString();
    }

    if (is_template_header) {
      SgExpression *semantic_constraint =
          templateParameter->get_typeConstraint();
      SgExpression *source_constraint =
          templateParameter->get_sourceTypeConstraint();
      if ((semantic_constraint == nullptr) != (source_constraint == nullptr)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-type-constraint]: "
                "parameter=%p has mismatched semantic/source constraints\n",
                static_cast<void *>(templateParameter));
        ROSE_ABORT();
      }
      if (source_constraint != nullptr) {
        SgUnparse_Info cinfo(info);
        cinfo.set_SkipClassDefinition();
        cinfo.set_SkipEnumDefinition();
        unparseExpression(source_constraint, cinfo);
        curprint(" ");
      } else {
        curprint(templateParameterKeywordSpelling(
            SageInterface::getTemplateParameterKeyword(templateParameter)));
      }
      if (is_pack) {
        curprint("... ");
      } else if (!parameter_name.empty() ||
                 templateParameter->get_defaultTypeParameter() != NULL) {
        curprint(" ");
      }
    }

    if (SgNonrealType *nrtype = isSgNonrealType(type)) {
      curprint(nrtype->get_name());
    } else if (SgTemplateType *ttype = isSgTemplateType(type)) {
      curprint(ttype->get_name());
    } else {
      SgUnparse_Info ninfo(info);
      unp->u_type->unparseType(type, ninfo);
    }

    SgType *default_type = templateParameter->get_defaultTypeParameter();
    if (emit_default_template_arg && default_type != NULL) {
      curprint(" = ");
      SgUnparse_Info dinfo(info);
      dinfo.set_SkipClassDefinition();
      dinfo.set_SkipEnumDefinition();
      dinfo.set_SkipQualifiedNames();
      dinfo.set_reference_node_for_qualification(templateParameter);
      unp->u_type->unparseType(default_type, dinfo);
    }
    break;
  }

  case SgTemplateParameter::nontype_parameter: {
    if (templateParameter->get_expression() != NULL) {
      unp->u_exprStmt->unparseExpression(templateParameter->get_expression(),
                                         info);
    } else {
      ASSERT_not_null(templateParameter->get_initializedName());

      SgType *type = templateParameter->get_initializedName()->get_type();
      ASSERT_not_null(type);
      // DQ (9/10/2014): Note that this will unparse "int T" which we want in
      // the template header, but not in the template parameter list.
      // unp->u_type->outputType<SgInitializedName>(templateParameter->get_initializedName(),type,info);
      // TV (03/20/2018) only if it is a template header (not a specialization)
      bool printed_name_with_type = false;
      SgInitializedName *parameter_name =
          templateParameter->get_initializedName();
      SgName output_parameter_name = parameter_name->get_name();
      if (is_template_header) {
        if (templateParameter->get_sourceTypeConstraint() != nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[template-nontype-constraint]: "
                  "parameter=%p must spell its placeholder constraint through "
                  "the exact declared type\n",
                  static_cast<void *>(templateParameter));
          ROSE_ABORT();
        }

        auto is_function_pointer_like = [](SgType *t) {
          if (isSgPointerMemberType(t) != NULL || isSgFunctionType(t) != NULL ||
              isSgMemberFunctionType(t) != NULL) {
            return true;
          }
          if (SgPointerType *ptr = isSgPointerType(t)) {
            SgType *base = ptr->get_base_type();
            return isSgFunctionType(base) != NULL ||
                   isSgMemberFunctionType(base) != NULL;
          }
          return false;
        };

        {
          SgUnparse_Info ninfo(info);
          ninfo.set_SkipClassDefinition();
          ninfo.set_SkipEnumDefinition();
          ninfo.set_reference_node_for_qualification(parameter_name);
          if (is_function_pointer_like(type)) {
            ninfo.set_inArgList();
          }
          unp->u_type->outputType<SgInitializedName>(
              templateParameter->get_initializedName(), type, ninfo,
              &output_parameter_name);
          printed_name_with_type = true;
        }
      }
      if (!printed_name_with_type) {
        if (is_template_header) {
          // Ensure a separator between the type spelling and parameter name for
          // generated/qualified type spellings that omit trailing whitespace.
          curprint(" ");
        }
        curprint(output_parameter_name);
      }
      if (emit_default_template_arg) {
        if (SgExpression *default_expr =
                templateParameter->get_defaultExpressionParameter()) {
          curprint(" = ");
          SgUnparse_Info einfo(info);
          einfo.set_SkipClassDefinition();
          einfo.set_SkipEnumDefinition();
          unparseExpression(default_expr, einfo);
        }
      }
    }

    break;
  }

  case SgTemplateParameter::template_parameter: {
    ASSERT_not_null(templateParameter->get_templateDeclaration());
    SgTemplateDeclaration *semantic_template_decl =
        isSgTemplateDeclaration(templateParameter->get_templateDeclaration());
    ASSERT_not_null(semantic_template_decl);
    SgTemplateDeclaration *source_template_decl =
        templateParameter->get_sourceSpelledTemplateDeclaration();
    if (source_template_decl != nullptr &&
        (source_template_decl == semantic_template_decl ||
         source_template_decl->get_parent() != templateParameter ||
         source_template_decl->get_scope() == nullptr)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-spelled-template-parameter]: "
              "parameter=%p semantic=%p source=%p owner=%p scope=%p\n",
              static_cast<void *>(templateParameter),
              static_cast<void *>(semantic_template_decl),
              static_cast<void *>(source_template_decl),
              static_cast<void *>(source_template_decl->get_parent()),
              static_cast<void *>(source_template_decl->get_scope()));
      ROSE_ABORT();
    }
    SgTemplateDeclaration *template_decl = source_template_decl != nullptr
                                               ? source_template_decl
                                               : semantic_template_decl;
    curprint("template ");

    SgTemplateParameterPtrList &templateParameterList =
        template_decl->get_templateParameters();
    Unparse_ExprStmt::unparseTemplateParameterList(templateParameterList, info,
                                                   true, template_decl);

    curprint(" ");
    curprint(templateParameterKeywordSpelling(
        SageInterface::getTemplateParameterKeyword(templateParameter)));
    curprint(" ");
    if (templateParameter->get_is_parameter_pack()) {
      curprint("... ");
    }

    SgTemplateType *parameter_type =
        isSgTemplateType(templateParameter->get_type());
    ASSERT_not_null(parameter_type);
    curprint(parameter_type->get_name());
    break;
  }

  default: {
    printf("Error: default reached \n");
    ROSE_ABORT();
  }
  }
}

bool Unparse_ExprStmt::isAnonymousClass(SgType *templateArgumentType) {
  SgClassType *classType = isSgClassType(templateArgumentType);
  if (classType == nullptr) {
    return false;
  }

  SgClassDeclaration *classDeclaration =
      isSgClassDeclaration(classType->get_declaration());
  if (classDeclaration == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-argument-type]: class type has "
            "no exact class declaration\n");
    ROSE_ABORT();
  }
  return classDeclaration->get_isUnNamed();
}

#define DEBUG_UNPARSE_TEMPLATE_ARGUMENT 0

void Unparse_ExprStmt::unparseTemplateArgument(
    SgTemplateArgument *templateArgument, SgUnparse_Info &info) {
  ASSERT_not_null(templateArgument);

#define DEBUG_TEMPLATE_ARGUMENT 0

#if DEBUG_TEMPLATE_ARGUMENT
  printf("In unparseTemplateArgument() = %p (explicitlySpecified = %s) \n",
         templateArgument,
         (templateArgument->get_explicitlySpecified() == true) ? "true"
                                                               : "false");
#endif

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || DEBUG_TEMPLATE_ARGUMENT
  printf("Unparse TemplateArgument (%p) \n", templateArgument);
  unp->u_exprStmt->curprint("\n/* Unparse TemplateArgument */ \n");
  unp->u_exprStmt->curprint("\n");
#endif

#if DEBUG_TEMPLATE_ARGUMENT
  unp->u_exprStmt->curprint(
      string("/* unparseTemplateArgument(): templateArgument is "
             "explicitlySpecified = ") +
      ((templateArgument->get_explicitlySpecified() == true) ? "true"
                                                             : "false") +
      " */");
#endif

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgUnparse_Info newInfo(info);
  SgStatement *qualification_context =
      templateArgumentQualificationContext(info);
  if (!info.SkipQualifiedNames()) {
    ASSERT_not_null(qualification_context);
  }
  newInfo.set_template_argument_qualification_context(qualification_context);
  NameQualificationResult qualification = {"", 0, false, false};
  if (templateArgument->get_argumentType() ==
      SgTemplateArgument::type_argument) {
    qualification = unp->u_name->lookup_type_qualification_for_output(
        templateArgument, qualification_context, info.SkipQualifiedNames());
  } else if (!info.SkipQualifiedNames()) {
    qualification = unp->u_name->lookup_qualification(templateArgument,
                                                      qualification_context);
  }

  // DQ (8/6/2007): Turn this off now that we have a more sophisticated hidden
  // declaration and hidden type list mechanism. DQ (10/13/2006): Force template
  // arguments to be fully qualified! (else they can now be turned off where the
  // template instantiation appears in a namespace)! DQ (10/14/2006): Since
  // template can appear anywhere types referenced in template instantiation
  // declarations have to be fully qualified.  We can't tell from the template
  // argument if it requires qualification we would need the type and the
  // function declaration (and then some analysis).  So fully qualify all
  // function parameter types.  This is a special case (documented in the
  // Unparse_ExprStmt::generateNameQualifier() member function.
  // newInfo.set_forceQualifiedNames();

#if DEBUG_UNPARSE_TEMPLATE_ARGUMENT
  printf(" -- newInfo.forceQualifiedNames()                 = %s \n",
         newInfo.forceQualifiedNames() ? "true" : "false");
  printf(" -- newInfo.requiresGlobalNameQualification()     = %s \n",
         newInfo.requiresGlobalNameQualification() ? "true" : "false");
  printf(" -- newInfo.get_name_qualification_length()       = %d \n",
         newInfo.get_name_qualification_length());
  printf(" -- newInfo.get_global_qualification_required()   = %s \n",
         newInfo.get_global_qualification_required() ? "true" : "false");
  printf(" -- newInfo.get_type_elaboration_required()       = %s \n",
         newInfo.get_type_elaboration_required() ? "true" : "false");
  printf(" -- contextual template argument qualification length    = %d \n",
         qualification.length);
  printf(" -- contextual template argument global qualification    = %s \n",
         qualification.global ? "true" : "false");
  printf(" -- contextual template argument type elaboration        = %s \n",
         qualification.typeElaboration ? "true" : "false");
#endif

  // DQ (5/14/2011): Added support for newer name qualification implementation.
  // printf ("In unparseTemplateArgument():
  // templateArgument->get_name_qualification_length() = %d
  // \n",templateArgument->get_name_qualification_length());
  newInfo.set_name_qualification_length(qualification.length);
  newInfo.set_global_qualification_required(qualification.global);
  newInfo.set_type_elaboration_required(qualification.typeElaboration);

  // DQ (5/30/2011): Added support for name qualification.
  newInfo.set_reference_node_for_qualification(templateArgument);
  ASSERT_not_null(newInfo.get_reference_node_for_qualification());

  if (newInfo.requiresGlobalNameQualification()) {
    newInfo.set_global_qualification_required(true);
    newInfo.set_reference_node_for_qualification(NULL);
  }

#if DEBUG_UNPARSE_TEMPLATE_ARGUMENT
  printf(" -- newInfo.get_reference_node_for_qualification() = %p \n",
         newInfo.get_reference_node_for_qualification());
  if (newInfo.get_reference_node_for_qualification() != NULL) {
    printf(
        " -- newInfo.get_reference_node_for_qualification() = %p = %s \n",
        newInfo.get_reference_node_for_qualification(),
        newInfo.get_reference_node_for_qualification()->class_name().c_str());
    unp->u_exprStmt->curprint(
        string("/* -- newInfo.get_reference_node_for_qualification() = ") +
        StringUtility::numberToString(
            newInfo.get_reference_node_for_qualification()) +
        " */");
  }
#endif

  // ROSE_ASSERT(newInfo.isTypeFirstPart() == false);
  // ROSE_ASSERT(newInfo.isTypeSecondPart() == false);

  if (newInfo.SkipBaseType() == true) {
#if DEBUG_TEMPLATE_ARGUMENT
    printf("In unparseTemplateArgument(): unset SkipBaseType() (how was this "
           "set? Maybe from the function reference expression?) \n");
#endif
    newInfo.unset_SkipBaseType();
  }

#if DEBUG_TEMPLATE_ARGUMENT
  printf("In unparseTemplateArgument(): templateArgument->get_argumentType() = "
         "%d = %s \n",
         templateArgument->get_argumentType(),
         templateArgument->template_argument_kind().c_str());
#endif

  switch (templateArgument->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    ASSERT_not_null(templateArgument->get_type());

    SgType *templateArgumentType = templateArgument->get_sourceSpelledType();
    if (templateArgumentType == nullptr) {
      templateArgumentType = templateArgument->get_type();
    }

    // DQ (1/21/2018): Check if this is an unnamed class (used as a template
    // argument, which is not alloweded, so we should not unparse it).
    bool isAnonymous = isAnonymousClass(templateArgumentType);
    if (isAnonymous == true) {
      SgClassType *anonymousType = isSgClassType(templateArgumentType);
      SgClassDeclaration *anonymousDeclaration =
          anonymousType != nullptr
              ? isSgClassDeclaration(anonymousType->get_declaration())
              : nullptr;
      SgNonrealDecl *nonrealOwner =
          isSgNonrealDecl(templateArgument->get_parent());
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[template-argument-type]: anonymous "
          "class argument=%p parent=%p/%s semantic-type=%p/%s "
          "source-type=%p declaration=%p name=%s owner=%p/%s "
          "explicit=%d nonreal-name=%s nonreal-template=%p/%s has no "
          "exact source-spelled alias\n",
          static_cast<void *>(templateArgument),
          static_cast<void *>(templateArgument->get_parent()),
          templateArgument->get_parent() != nullptr
              ? templateArgument->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(templateArgument->get_type()),
          templateArgument->get_type() != nullptr
              ? templateArgument->get_type()->class_name().c_str()
              : "<null>",
          static_cast<void *>(templateArgument->get_sourceSpelledType()),
          static_cast<void *>(anonymousDeclaration),
          anonymousDeclaration != nullptr
              ? anonymousDeclaration->get_name().str()
              : "<null>",
          static_cast<void *>(anonymousDeclaration != nullptr
                                  ? anonymousDeclaration->get_parent()
                                  : nullptr),
          anonymousDeclaration != nullptr &&
                  anonymousDeclaration->get_parent() != nullptr
              ? anonymousDeclaration->get_parent()->class_name().c_str()
              : "<null>",
          templateArgument->get_explicitlySpecified() ? 1 : 0,
          nonrealOwner != nullptr ? nonrealOwner->get_name().str() : "<null>",
          static_cast<void *>(nonrealOwner != nullptr
                                  ? nonrealOwner->get_templateDeclaration()
                                  : nullptr),
          nonrealOwner != nullptr &&
                  nonrealOwner->get_templateDeclaration() != nullptr
              ? nonrealOwner->get_templateDeclaration()->class_name().c_str()
              : "<null>");
      ROSE_ABORT();
    }

#if OUTPUT_DEBUGGING_INFORMATION
    printf("In unparseTemplateArgument(): templateArgument->get_type() = %s \n",
           templateArgumentType->class_name().c_str());
    unp->u_exprStmt->curprint("\n /* templateArgument->get_type() */ \n");
#endif
    // curprint ( "\n /* SgTemplateArgument::type_argument */ \n");

    // DQ (7/24/2011): Comment out to test going back to previous version befor
    // unparsing array types correctly.
    newInfo.set_SkipClassDefinition();
    SgType *stripped_template_argument_type =
        templateArgumentType != nullptr
            ? templateArgumentType->stripType(
                  SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
                  SgType::STRIP_RVALUE_REFERENCE_TYPE)
            : nullptr;
    const bool qualified_class_template_argument =
        isSgClassType(stripped_template_argument_type) != nullptr &&
        (qualification.length > 0 || qualification.global);
    if (!qualification.typeElaboration && !qualified_class_template_argument) {
      newInfo.set_SkipClassSpecifier();
    } else {
      newInfo.unset_SkipClassSpecifier();
    }

    // DQ (7/24/2011): Added to prevent output of enum declarations with enum
    // fields in template argument.
    newInfo.set_SkipEnumDefinition();

    // DQ (7/23/2011): These are required to unparse the full type directly
    // (e.g. SgArrayType (see test2011_117.C). DQ (11/27/2004): Set these
    // (though I am not sure that they help!) newInfo.unset_isTypeFirstPart();
    // newInfo.unset_isTypeSecondPart();

    // DQ (5/28/2011): We have to handle the name qualification directly since
    // types can be qualified different and so it depends upon where the type is
    // referenced. Thus the qualified name is stored in a map to the IR node
    // that references the type.
    const bool needs_declaration_style_type_output =
        qualification.length > 0 || qualification.global ||
        qualification.typeElaboration ||
        isSgPointerType(templateArgumentType) != nullptr ||
        isSgPointerMemberType(templateArgumentType) != nullptr ||
        isSgReferenceType(templateArgumentType) != nullptr ||
        isSgRvalueReferenceType(templateArgumentType) != nullptr ||
        isSgArrayType(templateArgumentType) != nullptr;
    if (needs_declaration_style_type_output) {
      unp->u_type->outputType<SgTemplateArgument>(
          templateArgument, templateArgumentType, newInfo);
    } else {
      // This will unparse the type with any required subtype qualification
      // without introducing declaration-style separator whitespace.
      unp->u_type->unparseType(templateArgumentType, newInfo);
    }
    break;
  }

  case SgTemplateArgument::nontype_argument: {
    // DQ (8/12/2013): This can be either an SgExpression or SgInitializedName.
    // ASSERT_not_null(templateArgument->get_expression());
    ROSE_ASSERT(templateArgument->get_expression() != NULL ||
                templateArgument->get_initializedName() != NULL);
    ROSE_ASSERT(templateArgument->get_expression() == NULL ||
                templateArgument->get_initializedName() == NULL);
    if (templateArgument->get_expression() != NULL) {
#if OUTPUT_DEBUGGING_INFORMATION
      printf("In unparseTemplateArgument(): templateArgument->get_expression() "
             "= %s \n",
             templateArgument->get_expression()->class_name().c_str());
      unp->u_exprStmt->curprint(
          "\n /* templateArgument->get_expression() */ \n");
#endif
      // curprint ( "\n /* SgTemplateArgument::nontype_argument */ \n");

      // DQ (8/7/2013): Adding support for template functions overloaded on
      // template parameters. This should be present, but we don't use it
      // directly in the name generation. We want to use the template arguments
      // in the symbol table lookup, but not the name generation.
      ASSERT_not_null(templateArgument->get_expression()->get_type());
      // unp->u_type->unparseType(templateArgument->get_expression()->get_type(),newInfo);

      // DQ (1/5/2007): test2007_01.C demonstrated where this expression
      // argument requires qualification.
      SgExpression *template_arg_expr = templateArgument->get_expression();
      const bool need_paren = template_arg_expr->get_need_paren() ||
                              isSgCommaOpExp(template_arg_expr) != NULL;
      if (need_paren) {
        curprint("(");
      }
      unp->u_exprStmt->unparseExpression(template_arg_expr, newInfo);
      if (need_paren) {
        curprint(")");
      }
    } else {
      // Unparse this case of a SgInitializedName.
      SgType *type = templateArgument->get_initializedName()->get_type();
      ASSERT_not_null(type);

      // DQ (9/10/2014): Note that this will unparse "int T" which we want in
      // the template header, but not in the template parameter or argument
      // list.
      // unp->u_type->outputType<SgInitializedName>(templateArgument->get_initializedName(),type,newInfo);
      // SgUnparse_Info ninfo(info);
      // unp->u_type->unparseType(type,ninfo);
      curprint(templateArgument->get_initializedName()->get_name());
    }
    break;
  }

  case SgTemplateArgument::template_template_argument: {
    SgDeclarationStatement *decl = templateArgument->get_templateDeclaration();
    ASSERT_not_null(decl);

    SgTemplateDeclaration *tpl_decl = isSgTemplateDeclaration(decl);
    if (tpl_decl != NULL) {
      SgScopeStatement *scope = tpl_decl->get_scope();
      SgTemplateSymbol *symbol =
          scope != NULL
              ? isSgTemplateSymbol(tpl_decl->get_symbol_from_symbol_table())
              : NULL;
      const std::string name = tpl_decl->get_name().getString();
      if (tpl_decl->get_template_kind() !=
              SgTemplateDeclaration::e_template_class ||
          SageBuilder::getTemplateParameterList(tpl_decl) == NULL ||
          name.empty() ||
          name.rfind("__unnamed_template_template_parameter_", 0) == 0 ||
          scope == NULL || tpl_decl->get_parent() != scope || symbol == NULL ||
          symbol->get_declaration() != tpl_decl ||
          symbol->get_parent() != scope->get_symbol_table() ||
          !scope->symbol_exists(symbol)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-template-argument]: "
                "argument=%p declaration=%p has no exact named template "
                "parameter identity\n",
                static_cast<void *>(templateArgument),
                static_cast<void *>(tpl_decl));
        ROSE_ABORT();
      }
      curprint(name);
      break;
    }

    SgTemplateClassDeclaration *tpl_cdel = isSgTemplateClassDeclaration(decl);
    SgTemplateTypedefDeclaration *tpl_typedef =
        isSgTemplateTypedefDeclaration(decl);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(decl);

    SgType *assoc_type = NULL;
    if (tpl_cdel != NULL) {
      assoc_type = tpl_cdel->get_type();
    } else if (tpl_typedef != NULL) {
      assoc_type = tpl_typedef->get_type();
    } else if (nrdecl != NULL) {
      assoc_type = nrdecl->get_type();
    } else {
      printf("Error: Unexpected declaration %p (%s) for template template "
             "argument %p\n",
             decl, decl->class_name().c_str(), templateArgument);
      ROSE_ABORT();
    }

    ASSERT_not_null(assoc_type);

    newInfo.set_SkipClassDefinition();
    newInfo.set_SkipClassSpecifier();
    newInfo.set_SkipEnumDefinition();
    unp->u_type->outputType<SgTemplateArgument>(templateArgument, assoc_type,
                                                newInfo);

    break;
  }

  case SgTemplateArgument::start_of_pack_expansion_argument: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-pack-marker]: a synthetic "
            "start-of-pack marker reached template argument emission\n");
    ROSE_ABORT();
  }

  case SgTemplateArgument::argument_undefined: {
    printf("Error argument_undefined in "
           "Unparse_ExprStmt::unparseTemplateArgument \n");
    ROSE_ABORT();
    break;
  }

  default: {
    printf("Error default reached in Unparse_ExprStmt::unparseTemplateArgument "
           "\n");
    ROSE_ABORT();
  }
  }

  if (templateArgument->get_is_pack_element()) {
    curprint("...");
  }

#if DEBUG_TEMPLATE_ARGUMENT
  printf("Leaving unparseTemplateArgument (%p) \n", templateArgument);
#endif
#if DEBUG_TEMPLATE_ARGUMENT
  curprint("\n/* Bottom of unparseTemplateArgument */ \n");
#endif

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  printf("Leaving unparseTemplateArgument (%p) \n", templateArgument);
  unp->u_exprStmt->curprint(
      string("\n/* Bottom of unparseTemplateArgument */ \n"));
#endif
}

string
unparse_operand_constraint(SgAsmOp::asm_operand_constraint_enum constraint) {
  static constexpr char
      asm_operand_constraint_letters[static_cast<int>(SgAsmOp::e_last) + 1] = {
          /* aoc_invalid */ '@',
          /* aoc_end_of_constraint */ ',',
          /* aoc_mod_earlyclobber */ '&',
          /* aoc_mod_commutative_ops */ '%',
          /* aoc_mod_ignore */ '#',
          /* aoc_mod_ignore_char */ '*',
          /* aoc_mod_disparage_slightly */ '?',
          /* aoc_mod_disparage_severely */ '!',
          /* aoc_any */ 'X',
          /* aoc_general */ 'g',
          /* aoc_match_0 */ '0',
          /* aoc_match_1 */ '1',
          /* aoc_match_2 */ '2',
          /* aoc_match_3 */ '3',
          /* aoc_match_4 */ '4',
          /* aoc_match_5 */ '5',
          /* aoc_match_6 */ '6',
          /* aoc_match_7 */ '7',
          /* aoc_match_8 */ '8',
          /* aoc_match_9 */ '9',
          /* aoc_reg_integer */ 'r',
          /* aoc_reg_float */ 'f',
          /* aoc_mem_any */ 'm',
          /* aoc_mem_load */ 'p',
          /* aoc_mem_offset */ 'o',
          /* aoc_mem_nonoffset */ 'V',
          /* aoc_mem_autoinc */ '>',
          /* aoc_mem_autodec */ '<',
          /* aoc_imm_int */ 'i',
          // DQ (1/10/2009): The code 'n' is not understood by gnu, so use 'r'
          // aoc_imm_number             'n',
          /* aoc_imm_number */ 'r',
          /* aoc_imm_symbol */ 's',
          /* aoc_imm_float */ 'F',
          /* aoc_reg_a */ 'a',
          /* aoc_reg_b */ 'b',
          /* aoc_reg_c */ 'c',
          /* aoc_reg_d */ 'd',
          /* aoc_reg_si */ 'S',
          /* aoc_reg_di */ 'D',
          /* aoc_reg_legacy */ 'R',
          // DQ (8/10/2006): Change case of register name, but I'm unclear if
          // this required for any others (OK for GNU, but required for Intel).
          /* aoc_reg_q */ 'q',
          /* aoc_reg_Q */ 'Q',
          /* aoc_reg_ad */ 'A',
          /* aoc_reg_float_tos */ 't',
          /* aoc_reg_float_second */ 'u',
          /* aoc_reg_sse */ 'x',
          /* aoc_reg_sse2 */ 'Y',
          /* aoc_reg_mmx */ 'y',
          /* aoc_imm_short_shift */ 'I',
          /* aoc_imm_long_shift */ 'J',
          /* aoc_imm_lea_shift */ 'M',
          /* aoc_imm_signed8 */ 'K',
          /* aoc_imm_unsigned8 */ 'N',
          /* aoc_imm_and_zext */ 'L',
          /* aoc_imm_80387 */ 'G',
          /* aoc_imm_sse */ 'H',
          /* aoc_imm_sext32 */ 'e',
          /* aoc_imm_zext32 */ 'z',
          /* aoc_last */ '~'};

  const int constraint_index = static_cast<int>(constraint);
  if (constraint_index < 0 ||
      constraint_index > static_cast<int>(SgAsmOp::e_last)) {
    fprintf(stderr, "Error: invalid assembly operand constraint value: %d\n",
            constraint_index);
    ROSE_ABORT();
  }

  return string(1, asm_operand_constraint_letters[constraint_index]);
}

void Unparse_ExprStmt::unparse_asm_operand_modifier(
    SgAsmOp::asm_operand_modifier_enum flags) {
  // Modifiers to asm operand strings.  These are all machine independent.
  // Many of them do not make sense in asm() but are included anyway for
  // completeness.  Note that these are bitmasks, and that
  // aom_input + aom_output == aom_modify.

  // e_invalid           = 0x00,  error
  // e_input             = 0x01,  no mod: input operand
  // e_output            = 0x02,  =: output operand
  // e_modify            = 0x03,  +: read-mod-write operand
  // e_earlyclobber      = 0x04,  &: modified early, cannot overlap inputs
  // e_commutative       = 0x08,  %: commutative with next operand
  // e_ignore_next       = 0x10,  *: ignore next letter as a register pref
  // e_ignore_till_comma = 0x20,  #: ignore up to comma as a register pref
  // e_poor_choice       = 0x40,  ?: avoid choosing this
  // e_bad_choice        = 0x80   !: really avoid choosing this

  if (flags == SgAsmOp::e_unknown) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[asm-operand-modifier]: invalid assembly "
            "operand modifier\n");
    ROSE_ABORT();
  }

  // DQ (7/23/2006): The coding of these is a bit more complex, get it? :-)
  // if (flags & SgAsmOp::e_input)             curprint ( "";
  // if (flags & SgAsmOp::e_output)            curprint ( "=";
  // if (flags & SgAsmOp::e_modify)            curprint ( "+";

  if (((flags & SgAsmOp::e_input) != 0) && ((flags & SgAsmOp::e_output) != 0)) {
    // This is how "modify" is coded!
#if PRINT_DEVELOPER_WARNINGS
    printf("This is how modify is coded \n");
#endif
    // DQ (8/10/2006): Intel compiler can not handle the output of modifier for
    // input operand:
    //      error: an asm input operand may not have the '=' or '+' modifiers
    // curprint ( "+";
    // if ( string(BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH) != string("icpc") &&
    // (flags & SgAsmOp::e_output) ) if (
    // string(BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH) != string("icpc") )
    curprint("+");
  } else {
    // Only one of these are true
    if (flags & SgAsmOp::e_output)
      curprint("=");

    // We need an exact match not a partial match!

#if PRINT_DEVELOPER_WARNINGS
    printf("We need an exact match not a partial match \n");
#endif
    // DQ (8/10/2006): Intel compiler can not handle the output of modifier for
    // input operand:
    //      error: an asm input operand may not have the '=' or '+' modifiers
    // if ( string(BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH) != string("icpc") )
    {
      if ((flags & SgAsmOp::e_modify) == SgAsmOp::e_modify)
        curprint("+");
    }
  }

  if (flags & SgAsmOp::e_earlyclobber)
    curprint("&");
  if (flags & SgAsmOp::e_commutative)
    curprint("%");
  if (flags & SgAsmOp::e_ignore_next)
    curprint("*");
  if (flags & SgAsmOp::e_ignore_till_comma)
    curprint("#");
  if (flags & SgAsmOp::e_poor_choice)
    curprint("?");
  if (flags & SgAsmOp::e_bad_choice)
    curprint("!");
}

void Unparse_ExprStmt::unparseAsmOp(SgExpression *expr, SgUnparse_Info &info) {
  // Just call unparse on the statement.
  SgAsmOp *asmOp = isSgAsmOp(expr);
  ASSERT_not_null(asmOp);

  // printf ("In unparseAsmOp(): asmOp->get_recordRawAsmOperandDescriptions() =
  // %s \n",asmOp->get_recordRawAsmOperandDescriptions() ? "true" : "false");

  SgExpression *expression = asmOp->get_expression();
  ASSERT_not_null(expression);

  if (asmOp->get_name().empty() == false) {
    // This is symbolic name indicated for this operand (using the "[
    // <identifier> ]" syntax, if present).
    curprint("[" + asmOp->get_name() + "] ");
  }

  curprint("\"");
  if (asmOp->get_recordRawAsmOperandDescriptions() == false) {
    // This is only set to non-invalid state when
    // RECORD_RAW_ASM_OPERAND_DESCRIPTIONS == FALSE.
    unparse_asm_operand_modifier(asmOp->get_modifiers());
    curprint(unparse_operand_constraint(asmOp->get_constraint()));
  } else {
    // The modifier is part of the constraint, and it is output in the
    // constraintString when recordRawAsmOperandDescriptions() == true.
    curprint(asmOp->get_constraintString());
  }

  // This is usually a SgVarRefExp
  curprint("\"");
  curprint(" (");
  unparseExpression(expression, info);
  curprint(")");
}

void Unparse_ExprStmt::unparseStatementExpression(SgExpression *expr,
                                                  SgUnparse_Info &info) {
  // Just call unparse on the statement.
  SgStatementExpression *statementExpression = isSgStatementExpression(expr);
  ASSERT_not_null(statementExpression);
  SgStatement *statement = statementExpression->get_statement();
  ASSERT_not_null(statement);

  // DQ (10/7/2006): Even if we are in a conditional the statements appearing in
  // the statement expression must have ";" output (here we have to turn off the
  // flags to both SkipSemiColon and inConditional).  See test2006_148.C for an
  // example.
  SgUnparse_Info info2(info);
  info2.unset_SkipSemiColon();
  info2.unset_inConditional();

  // Expressions are another place where a class definition should NEVER be
  // unparsed DQ (5/23/2007): Note that statement expressions can have class
  // definition (so they are exceptions, see test2007_51.C).
  info2.unset_SkipClassDefinition();

  // DQ (1/9/2014): We have to make the handling of enum definitions consistant
  // with that of class definitions.
  info2.unset_SkipEnumDefinition();

  curprint("(");
  unparseStatement(statement, info2);
  curprint(")");
}

void Unparse_ExprStmt::unparseUnaryOperator(SgExpression *expr, const char *op,
                                            SgUnparse_Info &info) {
  //
  // Flag to keep to original state of the "this" option
  //
  bool orig_this_opt = unp->opt.get_this_opt();
  SgUnparse_Info newinfo(info);
  newinfo.set_operator_name(op);
  //
  // If the "this" option was originally false, then we shouldn't print "this."
  // however, this only applies when the "this" is part of a binary expression.
  // In the unary case, we must print "this," otherwise a syntax error will be
  // produced. (i.e. *this)
  //
  if (!orig_this_opt)
    unp->opt.set_this_opt(true);

  unparseUnaryExpr(expr, newinfo);

  //
  // Now set the "this" option back to its original state
  //
  if (!orig_this_opt)
    unp->opt.set_this_opt(false);
}

#define DEBUG__Unparse_ExprStmt__unparseBinaryOperator 0

void Unparse_ExprStmt::unparseBinaryOperator(SgExpression *expr, const char *op,
                                             SgUnparse_Info &info) {
  SgUnparse_Info newinfo(info);
  newinfo.set_operator_name(op);

#if DEBUG__Unparse_ExprStmt__unparseBinaryOperator
  printf("In unparseBinaryOperator(): expr = %p op = %s \n", expr, op);
  curprint(string("\n /* Inside of unparseBinaryOperator(expr = ") +
           StringUtility::numberToString(expr) + " = " +
           expr->sage_class_name() + "," + op + ",SgUnparse_Info) */ \n");
#endif

#if DEBUG__Unparse_ExprStmt__unparseBinaryOperator
  printf("In unparseBinaryOperator(): info.SkipClassDefinition() = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
  printf("In unparseBinaryOperator(): info.SkipEnumDefinition()  = %s \n",
         (info.SkipEnumDefinition() == true) ? "true" : "false");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if DEBUG__Unparse_ExprStmt__unparseBinaryOperator
  curprint(string("\n /* Inside of unparseBinaryOperator(expr = ") +
           StringUtility::numberToString(expr) + " = " +
           expr->sage_class_name() + "," + op +
           ",SgUnparse_Info) : calling unparseBinaryExpr() */ \n");
#endif
  unparseBinaryExpr(expr, newinfo);
#if DEBUG__Unparse_ExprStmt__unparseBinaryOperator
  curprint(string("\n /* Inside of unparseBinaryOperator(expr = ") +
           StringUtility::numberToString(expr) + " = " +
           expr->sage_class_name() + "," + op +
           ",SgUnparse_Info) : DONE: unparseBinaryExpr() */ \n");
#endif

#if DEBUG__Unparse_ExprStmt__unparseBinaryOperator
  printf("Leaving unparseBinaryOperator(): expr = %p op = %s \n", expr, op);
  curprint(string("\n /* Leaving unparseBinaryOperator(expr = ") +
           StringUtility::numberToString(expr) + " = " +
           expr->sage_class_name() + "," + op + ",SgUnparse_Info) */ \n");
#endif
}

void Unparse_ExprStmt::unparseAssnExpr(SgExpression *, SgUnparse_Info &) {}

void Unparse_ExprStmt::unparseVarRef(SgExpression *expr, SgUnparse_Info &info) {
  SgVarRefExp *var_ref = isSgVarRefExp(expr);
  ASSERT_not_null(var_ref);

  // todo: when get_parent() works for this class we can
  // get back to the lhs of the SgArrowExp or SgDotExp that
  // may be a parent of this expression.  This will let
  // us avoid outputting the class qualifier when its not needed.

  // For now we always output the class qualifier.

  if (var_ref->get_symbol() == NULL) {
    printf("Error in unparseVarRef() at line %d column %d \n",
           var_ref->get_file_info()->get_line(),
           var_ref->get_file_info()->get_col());
  }
  ASSERT_not_null(var_ref->get_symbol());

  SgInitializedName *theName = var_ref->get_symbol()->get_declaration();
  ASSERT_not_null(theName);

  // DQ (1/7/2007): Much simpler version of code!
  // SgScopeStatement* declarationScope = theName->get_scope();
  // ASSERT_not_null(declarationScope);
  // SgUnparse_Info ninfo(info);

  SgName nameQualifier(exactNameQualification(unp, var_ref, info).qualifier);

  // DQ (1/22/2014): Adding support for generated names used in un-named
  // variables.
  const bool isAnonymousName =
      var_ref->get_symbol()->get_declaration()->get_name().is_null();

  // DQ (8/19/2014): This causes output such as:
  // "XXX::isValidDomainSize(domain_extents . Extents_s::imin);" with the
  // function parameter's SgVarRefExp qualified un-nessesarily (see
  // test2014_116.C).
  curprint(nameQualifier.str());

  if (isAnonymousName == false) {
    curprint(var_ref->get_symbol()->get_name().str());
  }
}

#define DEBUG_unparseCompoundLiteral 0

void Unparse_ExprStmt::unparseCompoundLiteral(SgExpression *expr,
                                              SgUnparse_Info &info) {
#if DEBUG_unparseCompoundLiteral
  printf("Enter unparseCompoundLiteral() \n");
#endif

  SgCompoundLiteralExp *compoundLiteral = isSgCompoundLiteralExp(expr);
  ASSERT_not_null(compoundLiteral);

  SgVariableSymbol *variableSymbol = compoundLiteral->get_symbol();
  ASSERT_not_null(variableSymbol);

  SgInitializedName *initializedName = variableSymbol->get_declaration();
  ASSERT_not_null(initializedName);

  if (initializedName->get_initptr() == NULL) {
    printf("Error: In unparseCompoundLiteral(): initializedName->get_initptr() "
           "== NULL: initializedName = %p name = %s \n",
           initializedName, initializedName->get_name().str());
  }

  ASSERT_not_null(initializedName->get_initptr());

  SgAggregateInitializer *aggregateInitializer =
      isSgAggregateInitializer(initializedName->get_initptr());
  ASSERT_not_null(aggregateInitializer);
  if (aggregateInitializer->get_source_form() !=
      SgAggregateInitializer::e_aggregate_initializer_source_compound_literal) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[compound-literal-source-form]: hidden "
            "compound-literal initializer has source form=%d\n",
            static_cast<int>(aggregateInitializer->get_source_form()));
    ROSE_ABORT();
  }

  unparseAggrInit(aggregateInitializer, info);

#if DEBUG_unparseCompoundLiteral
  printf("Leave unparseCompoundLiteral() \n");
#endif
}

void Unparse_ExprStmt::unparseClassRef(SgExpression *expr, SgUnparse_Info &) {
  SgClassNameRefExp *classname_ref = isSgClassNameRefExp(expr);
  ASSERT_not_null(classname_ref);

  curprint(classname_ref->get_symbol()->get_declaration()->get_name().str());
}

void Unparse_ExprStmt::unparseFuncRef(SgExpression *expr,
                                      SgUnparse_Info &info) {
  SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
  ASSERT_not_null(func_ref);

  // Calling the template function unparseFuncRef<SgFunctionRefExp>(func_ref);
  unparseFuncRefSupport<SgFunctionRefExp>(expr, info);
}

template <class T>
void Unparse_ExprStmt::unparseFuncRefSupport(SgExpression *expr,
                                             SgUnparse_Info &info) {
  // DQ (4/25/2012): since these IR nodes have the same API, we can use a
  // templated function to avoid the dublication of code.

#define DEBUG_FUNCTION_REFERENCE_SUPPORT 0

  // SgFunctionRefExp* func_ref = isSgFunctionRefExp(expr);
  T *func_ref = dynamic_cast<T *>(expr);
  ASSERT_not_null(func_ref);

  // DQ (4/14/2013): Added support for unparsing "operator+(x,y)" in place of
  // "x+y".  This is required in places even though we have historically
  // defaulted to the generation of the operator syntax (e.g. "x+y"), see
  // test2013_100.C for an example of where this is required.
  ASSERT_not_null(func_ref->get_parent());
  // SgNode* possibleFunctionCall = func_ref->get_parent()->get_parent();
  SgNode *possibleFunctionCall = func_ref->get_parent();
  ASSERT_not_null(possibleFunctionCall);
  SgFunctionCallExp *functionCallExp =
      isSgFunctionCallExp(possibleFunctionCall);

  // This fails for test2005_112.C.
  // ASSERT_not_null(functionCallExp);

  bool uses_operator_syntax = false;
  if (functionCallExp != NULL) {
    uses_operator_syntax = functionCallExp->get_uses_operator_syntax();
  }

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
  printf("In unparseFuncRefSupport(): uses_operator_syntax = %s \n",
         uses_operator_syntax ? "true" : "false");
  curprint(
      string("\n /* Inside of unparseFuncRefSupport: uses_operator_syntax = ") +
      (uses_operator_syntax ? "true" : "false") + " */ \n");
#endif

  // DQ: This acceses the string pointed to by the pointer in the SgName
  // object directly ans is thus UNSAFE! A copy of the string should be made.
  // char* func_name = func_ref->get_symbol()->get_name();
  // char* func_name = strdup (func_ref->get_symbol()->get_name().str());
  ASSERT_not_null(func_ref->get_symbol());
  string func_name = func_ref->get_symbol()->get_name().str();
  int diff = 0; // the length difference between "operator" and function

#if DEBUG_FUNCTION_REFERENCE_SUPPORT || 0
  printf("Inside of Unparse_ExprStmt::unparseFuncRef(): func_name = %s \n",
         func_name.c_str());
#endif

  ASSERT_not_null(func_ref->get_symbol());
  ASSERT_not_null(func_ref->get_symbol()->get_declaration());

  SgDeclarationStatement *declaration =
      func_ref->get_symbol()->get_declaration();
  if (SgFunctionDeclaration *function_declaration =
          isSgFunctionDeclaration(declaration)) {
    const bool has_omp_declare_variant_source_name =
        !function_declaration->get_omp_declare_variant_source_name()
             .getString()
             .empty();
    if (has_omp_declare_variant_source_name !=
        function_declaration->get_omp_declare_variant_region_ordinal()
            .has_value()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[omp-declare-variant-reference]: "
              "function=%s has incomplete typed source identity\n",
              function_declaration->get_name().str());
      ROSE_ABORT();
    }
    if (has_omp_declare_variant_source_name) {
      func_name =
          function_declaration->get_omp_declare_variant_source_name().str();
    }
  }

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
  // DQ (7/26/2012): Test the function name (debuging test2009_31.C:
  // "operator<<" output as "operator")
  printf("declaration = %p = %s \n", declaration,
         declaration->class_name().c_str());
  SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDecl =
      isSgTemplateInstantiationFunctionDecl(declaration);
  if (templateInstantiationFunctionDecl != NULL) {
    printf("templateInstantiationFunctionDecl->get_name() = %p = %s \n",
           templateInstantiationFunctionDecl,
           templateInstantiationFunctionDecl->get_name().str());
  } else {
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDecl =
            isSgTemplateInstantiationMemberFunctionDecl(declaration);
    if (templateInstantiationMemberFunctionDecl != NULL) {
      printf("templateInstantiationMemberFunctionDecl->get_name() = %p = %s \n",
             templateInstantiationMemberFunctionDecl,
             templateInstantiationMemberFunctionDecl->get_name().str());
    } else {
      printf("This is not a template function instantation (member nor "
             "non-member function) \n");
    }
  }
#endif

  // DQ (2/12/2019): Adding support for C++11 user-defined literal operators.
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(declaration);
  ASSERT_not_null(functionDeclaration);
  func_name = exactFunctionBaseName(functionDeclaration);

  bool is_literal_operator = false;
  if (functionDeclaration->get_specialFunctionModifier().isUldOperator() ==
      true) {
    is_literal_operator = true;
  }

  if (uses_operator_syntax == true && is_literal_operator == true) {
    return;
  }

  const std::string &operator_name = func_name;

  // check that this an operator overloading function
  if (!unp->opt.get_overload_opt() &&
      !strncmp(operator_name.c_str(), "operator", 8)) {
    // set the length difference between "operator" and function
    diff = (uses_operator_syntax == true)
               ? strlen(operator_name.c_str()) - strlen("operator")
               : 0;

    // DQ (1/6/2006): trap out cases of global new and delete functions called
    // using ("::operator new" or "::operator delete" syntax).  In these cases
    // the function are treated as normal function calls and not classified in
    // the AST as SgNewExp and SgDeleteExp.  See test2006_04.C.
    bool isNewOperator =
        (strncmp(operator_name.c_str(), "operator new", 12) == 0) ? true
                                                                  : false;
    bool isDeleteOperator =
        (strncmp(operator_name.c_str(), "operator delete", 15) == 0) ? true
                                                                     : false;

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
    printf("isNewOperator    = %s \n", isNewOperator ? "true" : "false");
    printf("isDeleteOperator = %s \n", isDeleteOperator ? "true" : "false");
#endif
    // DQ (1/6/2006): Only do this if not the "operator new" or "operator
    // delete" functions. now we check if the difference is larger than 0. If
    // it is, that means that there is something following "operator". Then we
    // can get the substring after "operator." If the diff is not larger than
    // 0, then don't get the substring.
    if ((isNewOperator == false) && (isDeleteOperator == false) && (diff > 0)) {
      // get the substring after "operator." If you are confused with how
      // strchr works, look up the man page for it. func_name =
      // strchr(func_name.c_str(), func_name[8]);
      if (uses_operator_syntax == true) {
        func_name = operator_name;
        func_name = strchr(func_name.c_str(), func_name[8]);
        if (is_literal_operator == true) {
          // func_name = strchr(func_name.c_str(), func_name[8]);
          // func_name = strchr(func_name.c_str(), "\"\"");
          func_name = strchr(func_name.c_str(), func_name[4]);
        }

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("In unparseFuncRef(): using operator syntax: func_name = %s \n",
               func_name.c_str());
#endif
      } else {
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("In unparseFuncRef(): using full operator name: func_name = "
               "%s \n",
               func_name.c_str());
#endif
      }
    }
  }

  // if func_name is not "()", print it. Otherwise, we don't print it because
  // we want to print out, for example, A(0) = 5, not A()(0) = 5.

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
  printf("func_name = %s uses_operator_syntax = %s \n", func_name.c_str(),
         uses_operator_syntax ? "true" : "false");
  printf("   --- strcmp(func_name.c_str(), \"()\") = %s \n",
         strcmp(func_name.c_str(), "()") ? "true" : "false");
#endif

  // DQ (4/14/2013): Modified to handle conditional use of
  // uses_operator_syntax. if (strcmp(func_name.c_str(), "()"))
  if ((strcmp(func_name.c_str(), "()") && (uses_operator_syntax == true)) ||
      (strcmp(func_name.c_str(), "operator()") &&
       (uses_operator_syntax == false))) {
    // DQ (10/21/2006): Only do name qualification of function names for C++
    if (SageInterface::is_Cxx_language() == true) {
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
      printf("declaration->get_declarationModifier().isFriend() = %s \n",
             declaration->get_declarationModifier().isFriend() ? "true"
                                                               : "false");
#endif
      // DQ (12/2/2004): Added diff == 0 to avoid qualification of operators
      // (avoids "i__gnu_cxx::!=0") added some extra spaces to make it more
      // clear if it is ever wrong again (i.e. "i __gnu_cxx:: != 0") DQ
      // (11/13/2004) Modified to avoid qualified name for friend functions DQ
      // (11/12/2004) Added support for qualification of function names output
      // as function calls if (
      // (declaration->get_declarationModifier().isFriend() == false) && (diff
      // == 0) )
      bool useNameQualification =
          ((declaration->get_declarationModifier().isFriend() == false) &&
           (diff == 0));

      // DQ (4/1/2014): Force name qualification where it was computed to be
      // required (see test2014_28.C). Even friends can need name
      // qualification.  However, this causes other test codes to fail.
      useNameQualification = true;
      useNameQualification =
          useNameQualification && (uses_operator_syntax == false);

      if (useNameQualification == true) {
        // DQ (8/6/2007): Now that we have a more sophisticated name
        // qualifiation mechanism using hidden declaration lists, we don't
        // have to force the qualification of function names. DQ (10/15/2006):
        // Force output of any qualified names for function calls.
        // info.set_forceQualifiedNames();

        // curprint ( "/* unparseFuncRef calling
        // info.set_forceQualifiedNames() */ ";

        // SgName nameQualifier = unp->u_name->generateNameQualifier(
        // declaration, info ); SgName nameQualifier =
        // unp->u_name->generateNameQualifier( declaration, tmp_info );

        // DQ (5/29/2011): Newest refactored support for name qualification.
        // printf ("In unparseFuncRef(): Looking for name qualification for
        // SgFunctionRefExp = %p \n",func_ref);
        SgName nameQualifier(
            exactNameQualification(unp, func_ref, info).qualifier);
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("In unparseFuncRef(): nameQualifier = %s \n",
               nameQualifier.str());
        printf("In unparseFuncRef(): exact contextual qualifier for "
               "SgFunctionRefExp = %p is %s \n",
               func_ref, nameQualifier.str());
        // curprint ( "\n /* unparseFuncRef using nameQualifier = " +
        // nameQualifier.str() + " */ \n";
#endif
        curprint(nameQualifier.str());
        // curprint (nameQualifier.str() + " ";
      } else {
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("In unparseFuncRef(): No name qualification permitted in this "
               "case! \n");
#endif
      }
    }

    // curprint ("\n /* unparseFuncRef func_name = " + func_name + " */ \n");
    // DQ (6/21/2011): Support for new name qualification (output of generated
    // function name).
    ASSERT_not_null(declaration);
    // printf ("Inside of Unparse_ExprStmt::unparseFuncRef(): declaration = %p
    // = %s \n",declaration,declaration->class_name().c_str()); If this is a
    // template then the name will include template arguments which require
    // name qualification and the name qualification will depend on where the
    // name is referenced in the code.  So we have generate the non-canonical
    // name with all possible qualifications and save it to be reused by the
    // unparser when it unparses the tempated function name.
    SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDecl =
        isSgTemplateInstantiationFunctionDecl(declaration);
    if (templateInstantiationFunctionDecl != NULL) {
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
      printf("In unparseFuncRef(): "
             "declaration->get_declarationModifier().isFriend() = %s \n",
             declaration->get_declarationModifier().isFriend() ? "true"
                                                               : "false");
      printf("In unparseFuncRef(): diff = %d \n", diff);
#endif
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
      printf("In unparseFuncRef(): templateInstantiationFunctionDecl = %p \n",
             templateInstantiationFunctionDecl);
#endif
      // SgTemplateFunctionDeclaration* templateFunctionDeclaration =
      // templateInstantiationFunctionDecl->get_templateDeclaration();
      // ASSERT_not_null(templateFunctionDeclaration);
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
      // printf ("In unparseFuncRef():
      // templateFunctionDeclaration->get_template_argument_list_is_explicit()
      // = %s
      // \n",templateFunctionDeclaration->get_template_argument_list_is_explicit()
      // ? "true" : "false");
      printf("In unparseFuncRef(): "
             "templateInstantiationFunctionDecl->get_template_argument_list_"
             "is_explicit() = %s \n",
             templateInstantiationFunctionDecl
                     ->get_template_argument_list_is_explicit()
                 ? "true"
                 : "false");
#endif
      if ((declaration->get_declarationModifier().isFriend() == false) &&
          (diff == 0)) {
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("Regenerate the name func_name = %s \n", func_name.c_str());
        printf("templateInstantiationFunctionDecl->get_templateName() = %s \n",
               templateInstantiationFunctionDecl->get_templateName().str());
#endif
        unparseTemplateFunctionName(templateInstantiationFunctionDecl, info);
      } else {
        // This case supports test2004_77.C
#if DEBUG_FUNCTION_REFERENCE_SUPPORT
        printf("In unparseFuncRef(): No name qualification permitted in this "
               "case! \n");
#endif
        curprint(func_name);
      }
    } else {
      curprint(func_name);
    }
  }

  // printDebugInfo("unparseFuncRef, Function Name: ", false);
  // printDebugInfo(func_name.c_str(), true);

#if DEBUG_FUNCTION_REFERENCE_SUPPORT
  printf("Leaving unparseFuncRefSupport() \n");
#endif
}

void Unparse_ExprStmt::unparseMFuncRef(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseMFuncRefSupport<SgMemberFunctionRefExp>(expr, info);
}

// DQ (7/6/2014): A different version of this is in the unparseCxx_expressions.C
// file.
bool partOfArrowOperatorChain(SgExpression *expr) {
#define DEBUG_ARROW_OPERATOR_CHAIN 0

  SgBinaryOp *binary_op = isSgBinaryOp(expr);
  // ASSERT_not_null(binary_op);

  bool result = false;

  // DQ (7/6/2014): We need this test to avoid more general cases where this
  // function can be called.
  if (binary_op != NULL) {
#if DEBUG_ARROW_OPERATOR_CHAIN
    printf("Inside of partOfArrowOperatorChain(): binary_op = %p = %s \n",
           binary_op, binary_op->class_name().c_str());
#endif

    // DQ (4/9/2013): Added support for unparsing "operator+(x,y)" in place of
    // "x+y".  This is required in places even though we have historically
    // defaulted to the generation of the operator syntax (e.g. "x+y"), see
    // test2013_100.C for an example of where this is required.
    SgNode *possibleParentFunctionCall = binary_op->get_parent();

    // DQ (4/9/2013): This fails for test2006_92.C.
    // ASSERT_not_null(possibleFunctionCall);
    //   bool parent_is_a_function_call                    = false;
    //   bool parent_function_call_uses_operator_syntax    = false;
    bool parent_function_is_overloaded_arrow_operator = false;
    //   bool parent_function_call_is_compiler_generated   = false;
    if (possibleParentFunctionCall != NULL) {
      SgFunctionCallExp *functionCallExp =
          isSgFunctionCallExp(possibleParentFunctionCall);
      if (functionCallExp != NULL) {
        //             parent_is_a_function_call                  = true;
        //             parent_function_call_uses_operator_syntax  =
        //             functionCallExp->get_uses_operator_syntax();
        //             parent_function_call_is_compiler_generated =
        //             functionCallExp->isCompilerGenerated();
        // DQ (7/5/2014): Add code to detect use of overloaded "operator->" as a
        // special case.
        SgExpression *rhs = binary_op->get_rhs_operand();
        // bool isRelevantOverloadedOperator = false;
        SgMemberFunctionRefExp *memberFunctionRefExp =
            isSgMemberFunctionRefExp(rhs);
        if (memberFunctionRefExp != NULL) {
          string functionName = memberFunctionRefExp->get_symbol()->get_name();
#if DEBUG_ARROW_OPERATOR_CHAIN
          printf("--- parent function is: functionName = %s \n",
                 functionName.c_str());
#endif
          if (functionName == "operator->") {
            parent_function_is_overloaded_arrow_operator = true;
          }
        }
        if (parent_function_is_overloaded_arrow_operator == true) {
          SgExpression *expression =
              isSgExpression(functionCallExp->get_parent());
          if (expression != NULL) {
            SgCastExp *castExp = isSgCastExp(expression);
            if (castExp != NULL) {
              // Skip over an SgCastExp IR nodes (see test2014_72.C).
              expression = isSgExpression(castExp->get_parent());
            }

            SgArrowExp *arrowExp = isSgArrowExp(expression);
            if (arrowExp != NULL) {
              result = true;
            } else {
              result = partOfArrowOperatorChain(expression);
            }
          } else {
            result = false;
          }
        } else {
          result = false;
        }
      }
    }
  }

#if DEBUG_ARROW_OPERATOR_CHAIN
  printf("Leaving partOfArrowOperatorChain(SgExpression* expr = %p = %s): "
         "result = %s \n",
         expr, expr->class_name().c_str(), result ? "true" : "false");
#endif

  return result;
}

template <class T>
void Unparse_ExprStmt::unparseMFuncRefSupport(SgExpression *expr,
                                              SgUnparse_Info &info) {
#define MFuncRefSupport_DEBUG 0

  T *mfunc_ref = dynamic_cast<T *>(expr);
  ASSERT_not_null(mfunc_ref);

#if MFuncRefSupport_DEBUG
  printf("In unparseMFuncRefSupport(): expr = %p = %s \n", expr,
         expr->class_name().c_str());
#endif
#if MFuncRefSupport_DEBUG
  curprint("\n /* Inside of unparseMFuncRef " +
           StringUtility::numberToString(expr) + " */ \n");
#endif

  SgMemberFunctionDeclaration *mfd = mfunc_ref->get_symbol()->get_declaration();
  ASSERT_not_null(mfd);

#if MFuncRefSupport_DEBUG
  printf("mfunc_ref->get_symbol()->get_name() = %s \n",
         mfunc_ref->get_symbol()->get_name().str());
  printf("mfunc_ref->get_symbol()->get_declaration()->get_name() = %s \n",
         mfunc_ref->get_symbol()->get_declaration()->get_name().str());
#endif

  // DQ (4/8/2013): Added support for unparsing "operator+(x,y)" in place of
  // "x+y".  This is required in places even though we have historically
  // defaulted to the generation of the operator syntax (e.g. "x+y"), see
  // test2013_100.C for an example of where this is required.
  ASSERT_not_null(mfunc_ref->get_parent());
  SgNode *possibleFunctionCall = mfunc_ref->get_parent();
  if (possibleFunctionCall != NULL &&
      isSgFunctionCallExp(possibleFunctionCall) == NULL) {
    possibleFunctionCall = possibleFunctionCall->get_parent();
  }

  if (possibleFunctionCall == NULL) {
    // DQ (3/5/2017): Converted to use message logging.
    printf("In unparseMFuncRefSupport(): possibleFunctionCall == NULL: "
           "mfunc_ref = %p = %s \n",
           mfunc_ref, mfunc_ref->class_name().c_str());
    SgNode *parent = mfunc_ref->get_parent();
    printf("  ---  parent = %p = %s \n", parent, parent->class_name().c_str());
    ROSE_ASSERT(parent->get_parent() == NULL);
  }

  // DQ (10/16/2016): Fix for test2016_84.C and test2016_85.C (simpler code).
  bool uses_operator_syntax = false;
  if (possibleFunctionCall != NULL) {
    SgFunctionCallExp *functionCallExp =
        isSgFunctionCallExp(possibleFunctionCall);
    if (functionCallExp != NULL) {
      uses_operator_syntax = functionCallExp->get_uses_operator_syntax();
    }
  }

  SgExpression *binary_op = isSgExpression(mfunc_ref->get_parent());
  // TV (11/15/2018): It can happen inside some STL include (originating from
  // <string>).
  bool isPartOfArrowOperatorChain =
      binary_op != NULL ? partOfArrowOperatorChain(binary_op) : false;

#if MFuncRefSupport_DEBUG
  printf("In unparseMFuncRefSupport(): isPartOfArrowOperatorChain              "
         "     = %s \n",
         isPartOfArrowOperatorChain ? "true" : "false");
  printf("In unparseMFuncRefSupport(): uses_operator_syntax  = %s \n",
         uses_operator_syntax ? "true" : "false");
#endif
#if MFuncRefSupport_DEBUG
  curprint(string("\n /* Inside of unparseMFuncRef: uses_operator_syntax  = ") +
           (uses_operator_syntax ? "true" : "false") + " */ \n");
#endif

  SgDeclarationStatement *decl = mfd->get_associatedClassDeclaration();
  SgClassDeclaration *xdecl = isSgClassDeclaration(decl);
  SgNonrealDecl *nrdecl = isSgNonrealDecl(decl);

#if MFuncRefSupport_DEBUG
  printf("In unparseMFuncRefSupport(): expr = %p (name = %s::%s) \n", expr,
         xdecl ? xdecl->get_name().str()
               : (nrdecl ? nrdecl->get_name().str() : ""),
         mfd->get_name().str());
#endif

  // qualified name is always outputed except when the p_need_qualifier is
  // set to 0 (when the naming class is identical to the selection class, and
  // and when we aren't suppressing the virtual function mechanism).

  // if (!get_is_virtual_call()) -- take off because this is not properly set

  // DQ (9/17/2004): Added assertion
  ASSERT_not_null(decl);
  if (decl->get_parent() == NULL) {
    // DQ (3/5/2017): Converted to use message logging.
    printf("Note: decl->get_parent() == NULL for decl = %p = %s (name = "
           "%s::%s) (OK for index expresion in array type) \n",
           decl, decl->class_name().c_str(),
           xdecl ? xdecl->get_name().str()
                 : (nrdecl ? nrdecl->get_name().str() : ""),
           mfd->get_name().str());
  }
  // DQ (5/30/2016): This need not have a parent if it is an expression in
  // index for an array type (see test2016_33.C).
  // ASSERT_not_null(decl->get_parent());

  bool print_colons = false;

#if MFuncRefSupport_DEBUG
  printf("mfunc_ref->get_need_qualifier() = %s \n",
         (mfunc_ref->get_need_qualifier() == true) ? "true" : "false");
#endif

  // DQ (11/7/2012): This is important for Elsa test code t0051.cc and now
  // also test2012_240.C (putting it back). DQ (3/28/2012): I think this is a
  // bug left over from the previous implementation of support for name
  // qualification. if (mfunc_ref->get_need_qualifier() == true)
  // SgFunctionCallExp* functionCall =
  // isSgFunctionCallExp(mfunc_ref->get_parent()); if (functionCall != NULL)
  if (mfunc_ref->get_need_qualifier() == true) {
    // check if this is a iostream operator function and the value of the
    // overload opt is false DQ (12/28/2005): Changed to check for more
    // general overloaded operators (e.g. operator[]) if
    // (!unp->opt.get_overload_opt() && isIOStreamOperator(mfunc_ref)); if
    // (unp->opt.get_overload_opt() == false &&
    // unp->u_sage->isOperator(mfunc_ref) == true)
    if (unp->opt.get_overload_opt() == false &&
        (uses_operator_syntax == true) &&
        unp->u_sage->isOperator(mfunc_ref) == true) {
      // ... nothing to do here
    } else {
      // printf ("In unparseMFuncRef(): Qualified names of member function
      // reference expressions are not handled yet! \n"); DQ (6/1/2011): Use
      // the newly generated qualified names.
      SgName nameQualifier(
          exactNameQualification(unp, mfunc_ref, info).qualifier);
      curprint(nameQualifier);
      print_colons = true;
    }
  } else {
    // See test2012_51.C for an example of this.

    // printf ("In unparseMFuncRefSupport(): mfunc_ref->get_parent() = %p = %s
    // \n",mfunc_ref->get_parent(),mfunc_ref->get_parent()->class_name().c_str());
    SgAddressOfOp *addressOperator = isSgAddressOfOp(mfunc_ref->get_parent());
    if (addressOperator != NULL) {
      // DQ (5/19/2012): This case also happens for test2005_112.C. This case
      // is now supported. When the address of a member function is take it
      // must use the qualified name.
      SgName nameQualifier(
          exactNameQualification(unp, mfunc_ref, info).qualifier);
      curprint(nameQualifier);
      // printf ("Output name qualification for SgMemberFunctionDeclaration:
      // nameQualifier = %s \n",nameQualifier.str());
      print_colons = true;
    }
  }

  // comments about the logic below can be found above in the unparseFuncRef
  // function.

  // char* func_name = mfunc_ref->get_symbol()->get_name();
  string func_name = exactFunctionBaseName(mfd);

  string full_function_name = func_name;

#if MFuncRefSupport_DEBUG
  // DQ (2/8/2014): This is a problem when we output comments in the func_name
  // and comments will not nest. curprint ( "\n /* Inside of unparseMFuncRef
  // (after name qualification) func_name = " + func_name + " */ \n");
#endif
#if MFuncRefSupport_DEBUG
  printf("func_name before processing to extract operator substring = %s \n",
         func_name.c_str());

  printf("unp->opt.get_overload_opt()                            = %s \n",
         (unp->opt.get_overload_opt() == true) ? "true" : "false");
  printf("strncmp(func_name, \"operator\", 8)                 = %d \n",
         strncmp(func_name.c_str(), "operator", 8));
  printf("print_colons                                      = %s \n",
         (print_colons == true) ? "true" : "false");
  printf("mfd->get_specialFunctionModifier().isConversion() = %s \n",
         (mfd->get_specialFunctionModifier().isConversion() == true) ? "true"
                                                                     : "false");
#endif

#if MFuncRefSupport_DEBUG
  printf("In unparseMFuncRefSupport(): exact function base name = %s \n",
         func_name.c_str());
  curprint("\n /* Inside of unparseMFuncRef (after name qualification and "
           "before output of function name) func_name = " +
           func_name + " */ \n");
#endif

  // DQ (7/6/2014): Added support for if the operator is compiler generated
  // (undid this change since overloaded operators using operator syntax will
  // always be marked as compiler generated). DQ (11/24/2004): unparse
  // conversion operators ("operator X&();") as "result.operator X&()" instead
  // of "(X&) result" (which appears as a cast instead of a function call.
  // check that this an operator overloading function and that colons were not
  // printed if (!unp->opt.get_overload_opt() && !strncmp(func_name,
  // "operator", 8) && !print_colons) if (!unp->opt.get_overload_opt() &&
  // func_name.size() >= 8 && func_name.substr(0, 8) == "operator" &&
  // !print_colons && !mfd->get_specialFunctionModifier().isConversion()) if
  // (!unp->opt.get_overload_opt() && (uses_operator_syntax == true) &&
  // func_name.size() >= 8 && func_name.substr(0, 8) == "operator" &&
  // !print_colons && !mfd->get_specialFunctionModifier().isConversion()) if
  // (!unp->opt.get_overload_opt() && (uses_operator_syntax == true &&
  // is_compiler_generated == true) && func_name.size() >= 8 &&
  // func_name.substr(0, 8) == "operator" &&  !print_colons &&
  // !mfd->get_specialFunctionModifier().isConversion())
  if (!unp->opt.get_overload_opt() && (uses_operator_syntax == true) &&
      func_name.size() >= 8 && func_name.substr(0, 8) == "operator" &&
      !print_colons && instantiatedConversionOperatorName(mfd).empty()) {
    func_name = func_name.substr(8);
  }
#if MFuncRefSupport_DEBUG
  printf("func_name after processing to extract operator substring = %s \n",
         func_name.c_str());
#endif

  if (func_name == "[]") {
    //
    // [DT] 3/30/2000 -- Don't unparse anything here.  The square brackets
    // will
    //      be handled from unparseFuncCall().
    //
    //      May want to handle overloaded operator() the same way.

    // This is a special case, while the input code may be either expressed as
    // "a[i]" or "a.operator[i]" (we can't tell which from the AST, I think).
    // often we want to unparse the code as "a[i]" but there is a case were
    // this is not possible
    // ("a->operator[](i)" is valid as is "(*a)[i]", but only if the
    // operator-> is not defined for the type of which "a" is a variable).  So
    // here we check the lhs of the parent of the curprintrent expression so
    // that we can detect this special case!

    // DQ (12/11/2004): We need to unparse the keyword "operator" in this
    // special cases (see test2004_159.C)
    SgExpression *parentExpression = isSgExpression(expr->get_parent());
    ASSERT_not_null(parentExpression);
    SgDotExp *dotExpression = isSgDotExp(parentExpression);
    if (dotExpression != NULL) {
      SgExpression *lhs = dotExpression->get_lhs_operand();
      ASSERT_not_null(lhs);
    }
  } else {
#if MFuncRefSupport_DEBUG
    printf("Case of unparsing a member function which is NOT short form of "
           "\"operator[]\" (i.e. \"[]\") funct_name = %s \n",
           func_name.c_str());
#endif
    // Make sure that the member function name does not include "()" (this
    // prevents "operator()()" from being output)
    if (func_name != "()") {
#if MFuncRefSupport_DEBUG
      printf("Case of unparsing a member function which is NOT "
             "\"operator()\" \n");
      curprint("/* Case of unparsing a member function which is NOT "
               "\"operator()\" */ \n");
#endif
      // DQ (12/11/2004): Catch special case of "a.operator->();" and avoid
      // unparsing it as "a->;" (illegal C++ code) Get the parent
      // SgFunctionCall so that we can check if it's parent was a SgDotExp
      // with a valid rhs_operand! if not then we have the case of
      // "a.operator->();"

      // It might be that this could be a "->" instead of a "."
      ASSERT_not_null(mfunc_ref);
      SgDotExp *dotExpression = isSgDotExp(mfunc_ref->get_parent());
      SgArrowExp *arrowExpression = isSgArrowExp(mfunc_ref->get_parent());

      // Note that not all references to a member function are a function
      // call.
      SgFunctionCallExp *functionCall = NULL;
      if (dotExpression != NULL) {
        functionCall = isSgFunctionCallExp(dotExpression->get_parent());
      }
      if (arrowExpression != NULL) {
        functionCall = isSgFunctionCallExp(arrowExpression->get_parent());
      }

#if MFuncRefSupport_DEBUG
      curprint(string("/* In unparseMFuncRefSupport(): (functionCall != "
                      "NULL) && (uses_operator_syntax == false) = ") +
               (((functionCall != NULL) && (uses_operator_syntax == false))
                    ? "true"
                    : "false") +
               " */ \n");
      curprint(
          string("/* In unparseMFuncRefSupport(): (functionCall != NULL) = ") +
          ((functionCall != NULL) ? "true" : "false") + " */ \n");
      curprint(
          string("/* In unparseMFuncRefSupport(): uses_operator_syntax   = ") +
          (uses_operator_syntax ? "true" : "false") + " */ \n");
#endif
#if MFuncRefSupport_DEBUG
      printf("In unparseMFuncRefSupport(): functionCall = %p "
             "uses_operator_syntax = %s \n",
             functionCall, uses_operator_syntax ? "true" : "false");
#endif
      if ((functionCall != NULL) && (uses_operator_syntax == false)) {
        if (unp->u_sage->isOverloadedArrowOperatorChain(functionCall) == true) {
          // DQ (Dec, 2004): special (rare) case of .operator->() or
          // ->operator->() decided to handle these cases because they are
          // amusing (C++ Trivia) :-).
          if (dotExpression != NULL) {
            curprint("operator->");
          } else {
            curprint("operator->");
          }
        } else {
          // DQ (2/9/2010): Fix for test2010_03.C
          // DQ (6/15/2013): The code for processing the function name when it
          // contains template arguments that requires name qualification.

          // DQ (5/25/2013): Added support to unparse the template arguments
          // separately from the member function name (which should NOT
          // include the template arguments when unparsing). Note the the
          // template arguments in the name are important for the generation
          // of mangled names for use in symbol tabls, but that we need to
          // output the member function name and it's template arguments
          // separately so that they name qulification can be computed and
          // saved in the name qualification name maps.

          // Note that this code below is a copy of that from the support for
          // unpasing the SgTemplateInstantiationFunctionDecl (in function
          // above).

          SgDeclarationStatement *declaration = mfd;
          ASSERT_not_null(declaration);

          // If this is a template then the name will include template
          // arguments which require name qualification and the name
          // qualification will depend on where the name is referenced in the
          // code.  So we have generate the non-canonical name with all
          // possible qualifications and save it to be reused by the unparser
          // when it unparses the tempated function name.
          SgTemplateInstantiationMemberFunctionDecl
              *templateInstantiationMemberFunctionDecl =
                  isSgTemplateInstantiationMemberFunctionDecl(declaration);
          if (templateInstantiationMemberFunctionDecl != NULL) {
            unparseTemplateMemberFunctionName(
                templateInstantiationMemberFunctionDecl, info);
          } else {
            // DQ (6/15/2013): I think this mod is required for test2010_03.C.
            curprint(func_name);
          }
        }
      } else {
        // If uses_operator_syntax == true, then we want to have the
        // unparseMFuncRefSupport() NOT output the operator name since it is
        // best done by the binary operator handling (e.g.
        // unparseBinaryExpr()).
        if (uses_operator_syntax == false) {
#if MFuncRefSupport_DEBUG
          printf("In unparseMFuncRefSupport(): function name IS output \n");
          curprint("/* In unparseMFuncRefSupport(): function name IS output "
                   "*/ \n");
#endif

          // DQ (5/25/2013): Added support to unparse the template arguments
          // separately from the member function name (which should NOT
          // include the template arguments when unparsing). Note the the
          // template arguments in the name are important for the generation
          // of mangled names for use in symbol tabls, but that we need to
          // output the member function name and it's template arguments
          // separately so that they name qulification can be computed and
          // saved in the name qualification name maps.

          // Note that this code below is a copy of that from the support for
          // unpasing the SgTemplateInstantiationFunctionDecl (in function
          // above).
          SgDeclarationStatement *declaration = mfd;

          // DQ (6/21/2011): Support for new name qualification (output of
          // generated function name).
          ASSERT_not_null(declaration);
          // printf ("Inside of Unparse_ExprStmt::unparseFuncRef():
          // declaration = %p = %s
          // \n",declaration,declaration->class_name().c_str()); If this is a
          // template then the name will include template arguments which
          // require name qualification and the name qualification will depend
          // on where the name is referenced in the code.  So we have generate
          // the non-canonical name with all possible qualifications and save
          // it to be reused by the unparser when it unparses the tempated
          // function name.
          SgTemplateInstantiationMemberFunctionDecl
              *templateInstantiationMemberFunctionDecl =
                  isSgTemplateInstantiationMemberFunctionDecl(declaration);
          if (templateInstantiationMemberFunctionDecl != NULL) {
            unparseTemplateMemberFunctionName(
                templateInstantiationMemberFunctionDecl, info);
          } else {
            curprint(func_name);
          }
        } else {
#if MFuncRefSupport_DEBUG
          printf("In unparseMFuncRefSupport(): function name is NOT output: "
                 "full_function_name = %s \n",
                 full_function_name.c_str());
          curprint("/* In unparseMFuncRefSupport(): function name is NOT "
                   "output */ \n");
#endif
#if MFuncRefSupport_DEBUG
          printf("In unparseMFuncRefSupport(): mfd->get_args().size() = "
                 "%" PRIuPTR " \n",
                 mfd->get_args().size());
#endif
          // DQ (11/17/2013): We need to distinguish between unary and binary
          // overloaded operators (for member functions a unary operator has
          // zero arguments, and a binary operator has a single argument).
          bool is_unary_operator = (mfd->get_args().size() == 0);
#if MFuncRefSupport_DEBUG
          printf("In unparseMFuncRefSupport(): is_unary_operator     = %s \n",
                 is_unary_operator ? "true" : "false");
          // printf ("In unparseMFuncRefSupport(): is_compiler_generated = %s
          // \n",is_compiler_generated ? "true" : "false");
#endif
          // DQ (7/6/2014): If this is compiler generated then supress the
          // output of the operator name.
          if (isPartOfArrowOperatorChain == false) {
            // DQ (7/5/2014): Adding operator-> as an additional special case.
            // These operators require special handling since they are prefix
            // operators when unparsed using operator syntax.
            if ((is_unary_operator == false) ||
                (is_unary_operator == true &&
                 full_function_name != "operator*" &&
                 full_function_name != "operator&")) {
#if MFuncRefSupport_DEBUG
              printf("In unparseMFuncRefSupport(): not overloaded reference "
                     "or dereference operator: function name IS output: "
                     "func_name = %s \n",
                     func_name.c_str());
              curprint("/* In unparseMFuncRefSupport(): not overloaded "
                       "reference or dereference operator: function name = " +
                       func_name + " IS output */ \n");
#endif
              curprint(func_name);
            } else {
#if MFuncRefSupport_DEBUG
              printf("info.isPrefixOperator() = %s \n",
                     info.isPrefixOperator() ? "true" : "false");
#endif
              if (info.isPrefixOperator() == true) {
                curprint(func_name);
              } else {
#if MFuncRefSupport_DEBUG
                printf("In unparseMFuncRefSupport(): function name is NOT "
                       "output for this operator: func_name = %s \n",
                       func_name.c_str());
                curprint("/* In unparseMFuncRefSupport(): function name is "
                         "NOT output for this operator:  func_name = " +
                         func_name + " */ \n");
#endif
              }
            }
          } else {
#if MFuncRefSupport_DEBUG
            printf("In unparseMFuncRefSupport(): case of "
                   "isPartOfArrowOperatorChain == true: function name is NOT "
                   "output for this operator: func_name = %s \n",
                   func_name.c_str());
            curprint("/* In unparseMFuncRefSupport(): case of "
                     "isPartOfArrowOperatorChain == true: function name is "
                     "NOT output for this operator:  func_name = " +
                     func_name + " */ \n");
#endif
          }
        }
      }
    } else {
#if MFuncRefSupport_DEBUG
      printf("Case of unparsing a member function which is \"operator()\" \n");
#endif
    }
  }

#if MFuncRefSupport_DEBUG
  printf("Leaving unparseMFuncRefSupport \n");
  curprint("\n/* leaving unparseMFuncRefSupport */ \n");
#endif
}

#define DEBUG_unparseStringVal 0

void Unparse_ExprStmt::unparseStringVal(SgExpression *expr, SgUnparse_Info &) {
#if DEBUG_unparseStringVal
  printf("Enter unparseStringVal():\n");
  printf("  expr = %p = %s\n", expr, expr->class_name().c_str());
#endif
  SgStringVal *str_val = isSgStringVal(expr);
  ASSERT_not_null(str_val);

#ifndef CXX_IS_ROSE_CODE_GENERATION
  curprintLiteral(str_val->get_cxx_literal_spelling());
#endif
}

void Unparse_ExprStmt::unparseUIntVal(SgExpression *expr, SgUnparse_Info &) {
  SgUnsignedIntVal *uint_val = isSgUnsignedIntVal(expr);
  ASSERT_not_null(uint_val);

  // curprint ( uint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this is
  // important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint ( "U";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (uint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(uint_val);
    curprint(tostring(uint_val->get_value()) + "U");
  } else {
    curprint(uint_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseLongIntVal(SgExpression *expr, SgUnparse_Info &) {
  SgLongIntVal *longint_val = isSgLongIntVal(expr);
  ASSERT_not_null(longint_val);

  // curprint ( longint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this is
  // important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint ( "L";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (longint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(longint_val);
    curprint(tostring(longint_val->get_value()) + "L");
  } else {
    curprint(longint_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseLongLongIntVal(SgExpression *expr,
                                             SgUnparse_Info &) {
  SgLongLongIntVal *longlongint_val = isSgLongLongIntVal(expr);
  ASSERT_not_null(longlongint_val);

  // curprint ( longlongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this is
  // important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint ( "LL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (longlongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(longlongint_val);
    curprint(tostring(longlongint_val->get_value()) + "LL");
  } else {
    curprint(longlongint_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseULongLongIntVal(SgExpression *expr,
                                              SgUnparse_Info &) {
  SgUnsignedLongLongIntVal *ulonglongint_val = isSgUnsignedLongLongIntVal(expr);
  ASSERT_not_null(ulonglongint_val);

  // curprint ( ulonglongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this is
  // important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint ( "ULL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (ulonglongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(ulonglongint_val);
    curprint(tostring(ulonglongint_val->get_value()) + "ULL");
  } else {
    curprint(ulonglongint_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseULongIntVal(SgExpression *expr,
                                          SgUnparse_Info &) {
  SgUnsignedLongVal *ulongint_val = isSgUnsignedLongVal(expr);
  ASSERT_not_null(ulongint_val);

  // curprint ( ulongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this is
  // important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint ( "UL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (ulongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(ulongint_val);
    curprint(tostring(ulongint_val->get_value()) + "UL");
  } else {
    curprint(ulongint_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloatVal(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgFloatVal *float_val = isSgFloatVal(expr);
  ASSERT_not_null(float_val);

  if (!float_val->get_valueString().empty()) {
    std::string spelling = float_val->get_valueString();
    if (info.get_user_defined_literal() &&
        (spelling.back() == 'f' || spelling.back() == 'F')) {
      spelling.pop_back();
    }
    curprint(spelling);
    return;
  }
  requireGeneratedCanonicalLiteralSpelling(float_val);

  // DQ (10/18/2005): Need to handle C code which cannot use C++ mechanism to
  // specify infinity, quiet NaN, and signaling NaN values.  Note that we can't
  // use the C++ interface since the input program, and thus the generated code,
  // might not have included the "limits" header file.
  float float_value = float_val->get_value();

  if (float_value == std::numeric_limits<float>::infinity()) {
    // printf ("Infinite value found as value in unparseFloatVal() \n");
    // curprint ( "std::numeric_limits<float>::infinity()";
    curprint("__builtin_huge_valf()");
  } else {
    // Test for NaN value (famous test of to check for equality) or check for
    // C++ definition of NaN. We detect C99 and C "__NAN__" and translate to
    // backend specific builtin function.
    if ((float_value != float_value) ||
        (float_value == std::numeric_limits<float>::quiet_NaN())) {
      // curprint ( "std::numeric_limits<float>::quiet_NaN()";
      curprint("__builtin_nanf (\"\")");
    } else {
      if (float_value == std::numeric_limits<float>::signaling_NaN()) {
        // curprint ( "std::numeric_limits<float>::signaling_NaN()";
        curprint("__builtin_nansf (\"\")");
      } else {
        curprint(canonicalFloatingLiteral(
            float_value, info.get_user_defined_literal() ? "" : "F"));
      }
    }
  }
}

void Unparse_ExprStmt::unparseBFloat16Val(SgExpression *expr,
                                          SgUnparse_Info &) {
  SgBFloat16Val *float_val = isSgBFloat16Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "bf16"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloat16Val(SgExpression *expr, SgUnparse_Info &) {
  SgFloat16Val *float_val = isSgFloat16Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "f16"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloat32Val(SgExpression *expr, SgUnparse_Info &) {
  SgFloat32Val *float_val = isSgFloat32Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "f32"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloat64Val(SgExpression *expr, SgUnparse_Info &) {
  SgFloat64Val *float_val = isSgFloat64Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "f64"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloat80Val(SgExpression *expr, SgUnparse_Info &) {
  SgFloat80Val *float_val = isSgFloat80Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString().empty()) {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "L"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseFloat128Val(SgExpression *expr,
                                          SgUnparse_Info &) {
  SgFloat128Val *float_val = isSgFloat128Val(expr);
  ASSERT_not_null(float_val);

  if (float_val->get_valueString().empty()) {
    requireGeneratedCanonicalLiteralSpelling(float_val);
    curprint(canonicalFloatingLiteral(float_val->get_value(), "Q"));
  } else {
    curprint(float_val->get_valueString());
  }
}

void Unparse_ExprStmt::unparseLongDoubleVal(SgExpression *expr,
                                            SgUnparse_Info &info) {
  SgLongDoubleVal *longdbl_val = isSgLongDoubleVal(expr);
  ASSERT_not_null(longdbl_val);

  if (!longdbl_val->get_valueString().empty()) {
    std::string spelling = longdbl_val->get_valueString();
    if (info.get_user_defined_literal() &&
        (spelling.back() == 'l' || spelling.back() == 'L')) {
      spelling.pop_back();
    }
    curprint(spelling);
    return;
  }
  requireGeneratedCanonicalLiteralSpelling(longdbl_val);

  // curprint ( longdbl_val->get_value();

  // DQ (10/18/2005): Need to handle C code which cannot use C++ mechanism to
  // specify infinity, quiet NaN, and signaling NaN values.
  long double longDouble_value = longdbl_val->get_value();
  if (longDouble_value == std::numeric_limits<long double>::infinity()) {
    // printf ("Infinite value found as value in unparseFloatVal() \n");
    // curprint ( "std::numeric_limits<long double>::infinity()";
    curprint("__builtin_huge_vall()");
  } else {
    // Test for NaN value (famous test of to check for equality) or check for
    // C++ definition of NaN. We detect C99 and C "__NAN__" and translate to
    // backend specific builtin function.
    if ((longDouble_value != longDouble_value) ||
        (longDouble_value == std::numeric_limits<long double>::quiet_NaN())) {
      // curprint ( "std::numeric_limits<long double>::quiet_NaN()";
      curprint("__builtin_nanl (\"\")");
    } else {
      if (longDouble_value ==
          std::numeric_limits<long double>::signaling_NaN()) {
        // curprint ( "std::numeric_limits<long double>::signaling_NaN()";
        curprint("__builtin_nansl (\"\")");
      } else {
        curprint(canonicalFloatingLiteral(
            longDouble_value, info.get_user_defined_literal() ? "" : "L"));
      }
    }
  }
}

void Unparse_ExprStmt::unparseComplexVal(SgExpression *expr,
                                         SgUnparse_Info &info) {
  SgComplexVal *complex_val = isSgComplexVal(expr);
  ASSERT_not_null(complex_val);

  if (complex_val->get_valueString() != "") { // Has string
    curprint(complex_val->get_valueString());
  } else if (complex_val->get_real_value() == NULL) { // Pure imaginary
    curprint("(");
    unparseExpression(complex_val->get_imaginary_value(), info);
    curprint(" * 1.0i)");
  } else { // Complex number
    curprint("(");
    unparseExpression(complex_val->get_real_value(), info);
    curprint(" + ");
    unparseExpression(complex_val->get_imaginary_value(), info);
    curprint(" * 1.0i)");
  }
}

void Unparse_ExprStmt::unparseTypeTraitBuiltinOperator(SgExpression *expr,
                                                       SgUnparse_Info &info) {
  SgTypeTraitBuiltinOperator *operatorExp = isSgTypeTraitBuiltinOperator(expr);
  ASSERT_not_null(operatorExp);

  const string functionNameString = operatorExp->get_name().getString();
  SgExpressionPtrList &list = operatorExp->get_args();
  if (functionNameString.empty() || list.empty()) {
    failBuiltinExpressionContract("has no exact name or argument list",
                                  operatorExp);
  }
  auto is_type_operand = [](SgExpression *argument) {
    SgTypeExpression *type_expression = isSgTypeExpression(argument);
    SgType *represented_type = type_expression != nullptr
                                   ? type_expression->get_represented_type()
                                   : nullptr;
    return type_expression != nullptr && represented_type != nullptr &&
           isSgTypeUnknown(represented_type) == nullptr &&
           isSgTypeDefault(represented_type) == nullptr;
  };
  auto is_value_operand = [&](SgExpression *argument) {
    return argument != nullptr && !is_type_operand(argument);
  };

  switch (operatorExp->get_builtin_operator_kind()) {
  case SgTypeTraitBuiltinOperator::e_type_trait_builtin:
    if (!std::all_of(list.begin(), list.end(), [&](SgExpression *argument) {
          return is_type_operand(argument) || is_value_operand(argument);
        })) {
      failBuiltinExpressionContract(
          "contains an argument without an exact source operand role",
          operatorExp);
    }
    break;
  case SgTypeTraitBuiltinOperator::e_convert_vector_builtin:
    if (functionNameString != "__builtin_convertvector" || list.size() != 2 ||
        !is_value_operand(list[0]) || !is_type_operand(list[1])) {
      failBuiltinExpressionContract(
          "does not have the exact convert-vector name and operand roles",
          operatorExp);
    }
    break;
  case SgTypeTraitBuiltinOperator::e_offsetof_builtin:
    if (functionNameString != "__builtin_offsetof" || list.size() != 2 ||
        !is_type_operand(list[0]) || !is_value_operand(list[1])) {
      failBuiltinExpressionContract(
          "does not have the exact offsetof name, type occurrence, and "
          "designator roles",
          operatorExp);
    }
    validateOffsetofDesignator(isSgExpression(list[1]), operatorExp,
                               operatorExp);
    break;
  default:
    failBuiltinExpressionContract("has an invalid typed builtin kind",
                                  operatorExp);
  }

  curprint(functionNameString);

  SgExpressionPtrList::iterator operand = list.begin();
  curprint("(");
  while (operand != list.end()) {
    // DQ (4/24/2013): Moved this to be ahead so that the unparseArg value would
    // be associated with the current argument.
    if (operand != list.begin()) {
      curprint(",");
    }

    SgExpression *expression = *operand;
    if (!is_type_operand(expression) && !is_value_operand(expression)) {
      failBuiltinExpressionContract(
          "contains an argument that is not exactly one owned type occurrence "
          "or value expression",
          operatorExp);
    }
    if (expression->get_parent() != operatorExp) {
      failBuiltinExpressionContract(
          "contains a non-exclusively-owned expression argument", operatorExp);
    }

    if (is_type_operand(*operand)) {
      // The SgTypeExpression is the typed occurrence identity for this
      // builtin argument, not a parenthesized value expression.  Invoke its
      // exact type emitter directly so generic expression precedence does not
      // invent an extra pair of parentheses around the source type-id.
      unparseTypeExpression(expression, info);
    } else {
      unparseExpression(expression, info);
    }
    operand++;
  }
  curprint(")");
}

//-----------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparseFuncCall
//
//  This function is called whenever we unparse a function call. It is divided
//  up into two parts. The first part unparses the function call and its
//  arguments using an "in-order" tree traversal method. This is done when we
//  have a binary operator overloading function and the operator overloading
//  option is turned off. The second part unparses the function call directly in
//  a list-like manner. This is done for non-operator function calls, or when
//  the operator overloading option is turned on.
//-----------------------------------------------------------------------------------
void Unparse_ExprStmt::unparseFuncCall(SgExpression *expr,
                                       SgUnparse_Info &info) {
#define DEBUG_FUNCTION_CALL 0

#if DEBUG_FUNCTION_CALL
  printf("In Unparse_ExprStmt::unparseFuncCall(): expr = %p "
         "unp->opt.get_overload_opt() = %s \n",
         expr, (unp->opt.get_overload_opt() == true) ? "true" : "false");
  curprint("\n/* In Unparse_ExprStmt::unparseFuncCall " +
           StringUtility::numberToString(expr) + " */ \n");
#endif

  SgFunctionCallExp *func_call = isSgFunctionCallExp(expr);
  ASSERT_not_null(func_call);
  switch (func_call->get_source_syntax()) {
  case SgFunctionCallExp::e_source_function_call:
    break;
  case SgFunctionCallExp::e_implicit_conversion:
    unparseExpression(GetImplicitConversionObject(func_call), info);
    return;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-call-source-syntax]: invalid "
            "value=%d\n",
            static_cast<int>(func_call->get_source_syntax()));
    ROSE_ABORT();
  }

  if (SgExprListExp *arguments = func_call->get_args()) {
    for (SgExpression *argument : arguments->get_expressions()) {
      if (argument == nullptr || argument->get_parent() != arguments ||
          argument->get_file_info() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-call-argument]: call=%p "
                "has a null, foreign, or source-less argument\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      if (argument->get_file_info()->isDefaultArgument()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-call-argument]: call=%p "
                "contains an implicit default argument instead of only "
                "source-explicit arguments\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
    }
  }

  const auto operator_surface = func_call->get_source_operator_surface();
  const auto operator_callee_form =
      func_call->get_source_operator_callee_form();
  const SgUnsignedCharList &operator_operand_roles =
      func_call->get_source_operator_operand_roles();
  SgExprListExp *literal_lexical_operands =
      func_call->get_source_user_defined_literal_operands();
  const SgUnsignedCharList &literal_suffix_roles =
      func_call->get_source_user_defined_literal_suffix_roles();
  if (operator_surface == SgFunctionCallExp::e_no_operator_surface) {
    if (func_call->get_uses_operator_syntax() ||
        operator_callee_form != SgFunctionCallExp::e_no_operator_callee_form ||
        !operator_operand_roles.empty() ||
        literal_lexical_operands != nullptr || !literal_suffix_roles.empty() ||
        !func_call->get_source_user_defined_literal_suffix()
             .getString()
             .empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-surface]: call=%p has "
              "operator metadata without an exact source surface\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
  } else {
    if (!func_call->get_uses_operator_syntax() ||
        func_call->get_function() == nullptr ||
        func_call->get_args() == nullptr ||
        operator_callee_form == SgFunctionCallExp::e_no_operator_callee_form) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[operator-source-surface]: typed "
                      "operator surface is incomplete\n");
      ROSE_ABORT();
    }

    SgExpressionPtrList &arguments = func_call->get_args()->get_expressions();
    if (operator_operand_roles.size() != arguments.size()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-operands]: call=%p "
              "owns %zu arguments but %zu exact operand roles\n",
              static_cast<void *>(func_call), arguments.size(),
              operator_operand_roles.size());
      ROSE_ABORT();
    }

    std::vector<SgExpression *> source_operands;
    std::vector<SgExpression *> semantic_operands;
    SgExpression *operator_reference = nullptr;
    if (operator_callee_form == SgFunctionCallExp::e_member_operator_callee) {
      SgExpression *source_object = nullptr;
      if (SgDotExp *dot = isSgDotExp(func_call->get_function())) {
        source_object = dot->get_lhs_operand();
        operator_reference = dot->get_rhs_operand();
      } else if (SgArrowExp *arrow = isSgArrowExp(func_call->get_function())) {
        source_object = arrow->get_lhs_operand();
        operator_reference = arrow->get_rhs_operand();
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-callee]: member "
                "surface call=%p has no exact object/reference owner\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      if (source_object == nullptr || operator_reference == nullptr ||
          source_object->get_parent() != func_call->get_function() ||
          operator_reference->get_parent() != func_call->get_function()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-callee]: member "
                "surface call=%p has malformed object/reference ownership\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      source_operands.push_back(source_object);
    } else if (operator_callee_form ==
               SgFunctionCallExp::e_nonmember_operator_callee) {
      if (isSgDotExp(func_call->get_function()) != nullptr ||
          isSgArrowExp(func_call->get_function()) != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-callee]: nonmember "
                "surface call=%p owns a member callee\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      operator_reference = func_call->get_function();
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p has "
              "invalid callee form=%d\n",
              static_cast<void *>(func_call),
              static_cast<int>(operator_callee_form));
      ROSE_ABORT();
    }
    ASSERT_not_null(operator_reference);
    SgNode *operator_reference_owner =
        operator_callee_form == SgFunctionCallExp::e_nonmember_operator_callee
            ? static_cast<SgNode *>(func_call)
            : operator_reference->get_parent();
    while (SgCastExp *cast = isSgCastExp(operator_reference)) {
      cast->validate_semantic_conversion();
      if (cast->get_cast_type() != SgCastExp::e_implicit_cast) {
        break;
      }
      SgExpression *operand = cast->get_operand();
      if (cast->get_parent() != operator_reference_owner ||
          operand == nullptr || operand->get_parent() != cast ||
          cast->get_type() == nullptr || operand->get_type() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-callee-conversion-owner]: "
                "call=%p implicit cast=%p has no exact typed owned operand\n",
                static_cast<void *>(func_call), static_cast<void *>(cast));
        ROSE_ABORT();
      }
      switch (cast->get_semantic_conversion_kind()) {
      case SgCastExp::e_semantic_conversion_FunctionToPointerDecay: {
        SgPointerType *target =
            isSgPointerType(cast->get_type()->stripTypedefsAndModifiers());
        if (target == nullptr ||
            (isSgFunctionType(operand->get_type()) == nullptr &&
             isSgMemberFunctionType(operand->get_type()) == nullptr) ||
            (isSgFunctionType(target->get_base_type()) == nullptr &&
             isSgMemberFunctionType(target->get_base_type()) == nullptr)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[operator-callee-conversion]: "
                  "function-to-pointer decay has incompatible exact types\n");
          ROSE_ABORT();
        }
        break;
      }
      case SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr: {
        SgType *source_type = operand->get_type()->stripTypedefsAndModifiers();
        SgType *result_type = cast->get_type()->stripTypedefsAndModifiers();
        SgPointerType *result_pointer = isSgPointerType(result_type);
        SgType *result_callable = result_pointer != nullptr
                                      ? result_pointer->get_base_type()
                                      : result_type;
        if (isSgFunctionType(source_type) == nullptr ||
            result_callable == nullptr ||
            isSgFunctionType(result_callable->stripTypedefsAndModifiers()) ==
                nullptr ||
            !SageInterface::isEquivalentType(source_type, result_callable)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[operator-callee-conversion]: builtin "
                  "callee conversion does not preserve one exact function or "
                  "function-pointer signature\n");
          ROSE_ABORT();
        }
        break;
      }
      case SgCastExp::e_semantic_conversion_LValueToRValue:
      case SgCastExp::e_semantic_conversion_NoOp:
        if (SageInterface::containsUnknownType(operand->get_type()) ||
            SageInterface::containsUnknownType(cast->get_type())) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[operator-callee-conversion]: "
                  "transparent conversion has no exact source/result type\n");
          ROSE_ABORT();
        }
        break;
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-callee-conversion]: call=%p "
                "has unsupported implicit semantic conversion=%d\n",
                static_cast<void *>(func_call),
                static_cast<int>(cast->get_semantic_conversion_kind()));
        ROSE_ABORT();
      }
      operator_reference_owner = cast;
      operator_reference = operand;
    }
    const bool recognized_operator_reference =
        isSgFunctionRefExp(operator_reference) != nullptr ||
        isSgTemplateFunctionRefExp(operator_reference) != nullptr ||
        isSgMemberFunctionRefExp(operator_reference) != nullptr ||
        isSgTemplateMemberFunctionRefExp(operator_reference) != nullptr ||
        isSgNonrealRefExp(operator_reference) != nullptr;
    if (!recognized_operator_reference) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p owns "
              "a non-function operator reference=%s\n",
              static_cast<void *>(func_call),
              operator_reference->class_name().c_str());
      ROSE_ABORT();
    }

    SgFunctionDeclaration *operator_declaration = nullptr;
    if (SgFunctionRefExp *function_reference =
            isSgFunctionRefExp(operator_reference);
        function_reference != nullptr &&
        (function_reference->get_symbol() == nullptr ||
         function_reference->get_symbol()->get_declaration() == nullptr)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p has "
              "no exact operator declaration\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
    operator_declaration = func_call->getAssociatedFunctionDeclaration();
    const bool literal_surface =
        operator_surface == SgFunctionCallExp::e_user_defined_literal_surface;
    if (operator_declaration == nullptr &&
        isSgNonrealRefExp(operator_reference) == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p has "
              "no exact operator declaration\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
    if (operator_declaration != nullptr &&
        (literal_surface ? !operator_declaration->get_specialFunctionModifier()
                                .isUldOperator()
                         : !operator_declaration->get_specialFunctionModifier()
                                .isOperator())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p "
              "surface=%d resolves to a declaration of the wrong kind\n",
              static_cast<void *>(func_call),
              static_cast<int>(operator_surface));
      ROSE_ABORT();
    }
    if (!literal_surface && !func_call->get_source_user_defined_literal_suffix()
                                 .getString()
                                 .empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-surface]: non-literal "
              "call=%p owns a literal suffix\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
    if (!literal_surface && (literal_lexical_operands != nullptr ||
                             !literal_suffix_roles.empty())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-surface]: non-literal "
              "call=%p owns literal lexical operands\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }

    auto role = operator_operand_roles.begin();
    for (SgExpression *argument : arguments) {
      if (argument == nullptr ||
          argument->get_parent() != func_call->get_args()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: call=%p "
                "has a null or foreign argument\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      switch (*role++) {
      case SgFunctionCallExp::e_source_operator_operand:
        source_operands.push_back(argument);
        break;
      case SgFunctionCallExp::e_semantic_operator_operand:
        semantic_operands.push_back(argument);
        break;
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: call=%p "
                "has invalid operand role\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
    }

    auto require_operand_shape = [&](size_t source_count, size_t semantic_count,
                                     const char *surface) {
      if (source_operands.size() != source_count ||
          semantic_operands.size() != semantic_count) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: %s "
                "call=%p requires source=%zu semantic=%zu, found "
                "source=%zu semantic=%zu\n",
                surface, static_cast<void *>(func_call), source_count,
                semantic_count, source_operands.size(),
                semantic_operands.size());
        ROSE_ABORT();
      }
    };
    auto is_prefix_surface = [&]() {
      switch (operator_surface) {
      case SgFunctionCallExp::e_prefix_plus:
      case SgFunctionCallExp::e_prefix_minus:
      case SgFunctionCallExp::e_dereference:
      case SgFunctionCallExp::e_address_of:
      case SgFunctionCallExp::e_logical_not:
      case SgFunctionCallExp::e_bitwise_not:
      case SgFunctionCallExp::e_prefix_increment:
      case SgFunctionCallExp::e_prefix_decrement:
      case SgFunctionCallExp::e_co_await:
        return true;
      default:
        return false;
      }
    };
    auto is_binary_surface = [&]() {
      return operator_surface >= SgFunctionCallExp::e_binary_plus &&
             operator_surface <= SgFunctionCallExp::e_binary_arrow_star;
    };
    auto operator_token = [&]() -> const char * {
      switch (operator_surface) {
      case SgFunctionCallExp::e_prefix_plus:
      case SgFunctionCallExp::e_binary_plus:
        return "+";
      case SgFunctionCallExp::e_prefix_minus:
      case SgFunctionCallExp::e_binary_minus:
        return "-";
      case SgFunctionCallExp::e_dereference:
      case SgFunctionCallExp::e_binary_multiply:
        return "*";
      case SgFunctionCallExp::e_address_of:
      case SgFunctionCallExp::e_binary_and:
        return "&";
      case SgFunctionCallExp::e_logical_not:
        return "!";
      case SgFunctionCallExp::e_bitwise_not:
        return "~";
      case SgFunctionCallExp::e_prefix_increment:
      case SgFunctionCallExp::e_postfix_increment:
        return "++";
      case SgFunctionCallExp::e_prefix_decrement:
      case SgFunctionCallExp::e_postfix_decrement:
        return "--";
      case SgFunctionCallExp::e_co_await:
        return "co_await ";
      case SgFunctionCallExp::e_binary_divide:
        return "/";
      case SgFunctionCallExp::e_binary_remainder:
        return "%";
      case SgFunctionCallExp::e_binary_xor:
        return "^";
      case SgFunctionCallExp::e_binary_or:
        return "|";
      case SgFunctionCallExp::e_binary_assign:
        return "=";
      case SgFunctionCallExp::e_binary_less:
        return "<";
      case SgFunctionCallExp::e_binary_greater:
        return ">";
      case SgFunctionCallExp::e_binary_plus_assign:
        return "+=";
      case SgFunctionCallExp::e_binary_minus_assign:
        return "-=";
      case SgFunctionCallExp::e_binary_multiply_assign:
        return "*=";
      case SgFunctionCallExp::e_binary_divide_assign:
        return "/=";
      case SgFunctionCallExp::e_binary_remainder_assign:
        return "%=";
      case SgFunctionCallExp::e_binary_xor_assign:
        return "^=";
      case SgFunctionCallExp::e_binary_and_assign:
        return "&=";
      case SgFunctionCallExp::e_binary_or_assign:
        return "|=";
      case SgFunctionCallExp::e_binary_left_shift:
        return "<<";
      case SgFunctionCallExp::e_binary_right_shift:
        return ">>";
      case SgFunctionCallExp::e_binary_left_shift_assign:
        return "<<=";
      case SgFunctionCallExp::e_binary_right_shift_assign:
        return ">>=";
      case SgFunctionCallExp::e_binary_equal:
        return "==";
      case SgFunctionCallExp::e_binary_not_equal:
        return "!=";
      case SgFunctionCallExp::e_binary_less_equal:
        return "<=";
      case SgFunctionCallExp::e_binary_greater_equal:
        return ">=";
      case SgFunctionCallExp::e_binary_spaceship:
        return "<=>";
      case SgFunctionCallExp::e_binary_logical_and:
        return "&&";
      case SgFunctionCallExp::e_binary_logical_or:
        return "||";
      case SgFunctionCallExp::e_binary_comma:
        return ",";
      case SgFunctionCallExp::e_binary_arrow_star:
        return "->*";
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-surface]: call=%p "
                "surface=%d has no operator token\n",
                static_cast<void *>(func_call),
                static_cast<int>(operator_surface));
        ROSE_ABORT();
      }
    };
    if (operator_declaration != nullptr && !literal_surface) {
      std::string expected_operator_name;
      switch (operator_surface) {
      case SgFunctionCallExp::e_call_operator_surface:
        expected_operator_name = "operator()";
        break;
      case SgFunctionCallExp::e_subscript_operator_surface:
        expected_operator_name = "operator[]";
        break;
      case SgFunctionCallExp::e_arrow_operator_surface:
        expected_operator_name = "operator->";
        break;
      case SgFunctionCallExp::e_co_await:
        expected_operator_name = "operator co_await";
        break;
      default:
        expected_operator_name = std::string("operator") + operator_token();
        break;
      }
      const std::string actual_operator_name =
          exactFunctionBaseName(operator_declaration);
      if (actual_operator_name != expected_operator_name) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-callee]: call=%p "
                "surface=%d requires declaration=%s, found=%s\n",
                static_cast<void *>(func_call),
                static_cast<int>(operator_surface),
                expected_operator_name.c_str(), actual_operator_name.c_str());
        ROSE_ABORT();
      }
    }
    auto unparse_source_operand = [&](SgExpression *operand,
                                      SgUnparse_Info &operand_info) {
      ASSERT_not_null(operand);
      unparseExpression(operand, operand_info);
    };

    SgUnparse_Info operand_info(info);
    operand_info.set_nested_expression();
    if (is_prefix_surface()) {
      require_operand_shape(1, 0, "prefix operator");
      curprint(operator_token());
      unparse_source_operand(source_operands.front(), operand_info);
    } else if (operator_surface == SgFunctionCallExp::e_postfix_increment ||
               operator_surface == SgFunctionCallExp::e_postfix_decrement) {
      require_operand_shape(1, 1, "postfix operator");
      if (operator_operand_roles.empty() ||
          operator_operand_roles.back() !=
              SgFunctionCallExp::e_semantic_operator_operand) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: postfix "
                "call=%p does not own one trailing semantic dummy\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      unparse_source_operand(source_operands.front(), operand_info);
      curprint(operator_token());
    } else if (is_binary_surface()) {
      require_operand_shape(2, 0, "binary operator");
      unparse_source_operand(source_operands[0], operand_info);
      curprint(" ");
      curprint(operator_token());
      curprint(" ");
      unparse_source_operand(source_operands[1], operand_info);
    } else if (operator_surface == SgFunctionCallExp::e_call_operator_surface) {
      if (source_operands.empty() || !semantic_operands.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: call "
                "operator call=%p has no object or owns semantic extras\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      unparse_source_operand(source_operands.front(), operand_info);
      curprint("(");
      for (size_t i = 1; i < source_operands.size(); ++i) {
        if (i != 1) {
          curprint(", ");
        }
        unparse_source_operand(source_operands[i], operand_info);
      }
      curprint(")");
    } else if (operator_surface ==
               SgFunctionCallExp::e_subscript_operator_surface) {
      if (source_operands.size() < 2 || !semantic_operands.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: subscript "
                "call=%p lacks an object/index or owns semantic extras\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      unparse_source_operand(source_operands.front(), operand_info);
      curprint("[");
      for (size_t i = 1; i < source_operands.size(); ++i) {
        if (i != 1) {
          curprint(", ");
        }
        unparse_source_operand(source_operands[i], operand_info);
      }
      curprint("]");
    } else if (operator_surface ==
               SgFunctionCallExp::e_arrow_operator_surface) {
      require_operand_shape(1, 0, "arrow operator");
      if (operator_callee_form != SgFunctionCallExp::e_member_operator_callee) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-callee]: arrow "
                "surface call=%p is not a member call\n",
                static_cast<void *>(func_call));
        ROSE_ABORT();
      }
      // Clang inserts this semantic call underneath the source MemberExpr.
      // The surrounding SgArrowExp owns the source `->`; this call owns only
      // the exact source object.
      unparse_source_operand(source_operands.front(), operand_info);
    } else if (operator_surface ==
               SgFunctionCallExp::e_user_defined_literal_surface) {
      if (!source_operands.empty() ||
          operator_callee_form !=
              SgFunctionCallExp::e_nonmember_operator_callee ||
          std::any_of(
              operator_operand_roles.begin(), operator_operand_roles.end(),
              [](unsigned char role) {
                return role != SgFunctionCallExp::e_semantic_operator_operand;
              }) ||
          literal_lexical_operands == nullptr ||
          literal_lexical_operands->get_parent() != func_call ||
          literal_lexical_operands->get_expressions().empty() ||
          literal_lexical_operands->get_expressions().size() !=
              literal_suffix_roles.size()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal "
                "surface has malformed lexical/semantic operands\n");
        ROSE_ABORT();
      }
      const std::string suffix =
          func_call->get_source_user_defined_literal_suffix().getString();
      if (suffix.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal "
                "surface has no exact source suffix\n");
        ROSE_ABORT();
      }
      bool emitted_suffix = false;
      for (size_t index = 0;
           index < literal_lexical_operands->get_expressions().size();
           ++index) {
        SgExpression *lexical =
            literal_lexical_operands->get_expressions()[index];
        if (lexical == nullptr ||
            lexical->get_parent() != literal_lexical_operands ||
            isSgValueExp(lexical) == nullptr ||
            isSgValueExp(lexical)->get_literal_spelling_form() !=
                SgValueExp::e_literal_source_spelled) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[user-defined-literal-call]: lexical "
                  "operand is not an exactly owned source literal\n");
          ROSE_ABORT();
        }
        const unsigned char suffix_role = literal_suffix_roles[index];
        if (suffix_role != SgFunctionCallExp::
                               e_user_defined_literal_token_without_suffix &&
            suffix_role !=
                SgFunctionCallExp::e_user_defined_literal_token_with_suffix) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[user-defined-literal-call]: lexical "
                  "operand has an invalid suffix role\n");
          ROSE_ABORT();
        }
        if (index != 0) {
          curprint(" ");
        }
        operand_info.set_user_defined_literal(true);
        unparse_source_operand(lexical, operand_info);
        if (suffix_role ==
            SgFunctionCallExp::e_user_defined_literal_token_with_suffix) {
          curprint(suffix);
          emitted_suffix = true;
        }
      }
      if (!emitted_suffix) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[user-defined-literal-call]: lexical "
                "surface owns no suffix occurrence\n");
        ROSE_ABORT();
      }
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-surface]: call=%p has "
              "invalid surface=%d\n",
              static_cast<void *>(func_call),
              static_cast<int>(operator_surface));
      ROSE_ABORT();
    }
    return;
  }
  SgUnparse_Info newinfo(info);
  bool needSquareBrackets = false;
  SgExpression *operator_ref =
      directOperatorReference(func_call->get_function());
  const bool call_uses_unary_operator =
      operator_ref != nullptr && unp->u_sage->isUnaryOperator(operator_ref);
  const bool call_uses_unary_postfix_operator =
      call_uses_unary_operator &&
      unp->u_sage->isUnaryPostfixOperator(operator_ref);

#if DEBUG_FUNCTION_CALL
  curprint("/* func_call->get_function()                   = " +
           func_call->get_function()->class_name() + " */\n");
  curprint(
      string("/* func_call->get_uses_operator_syntax()       = ") +
      ((func_call->get_uses_operator_syntax() == true) ? "true" : "false") +
      " */\n");
  curprint(string("/* unp->opt.get_overload_opt()                 = ") +
           ((unp->opt.get_overload_opt() == true) ? "true" : "false") +
           " */\n");
  // curprint("/* isBinaryOperator(func_call->get_function()) = " +
  // ((unp->u_sage->isBinaryOperator(func_call->get_function()) == true) ?
  // "true" : "false") + " */\n");
#endif

  // DQ (4/8/2013): Added support for unparsing "operator+(x,y)" in place of
  // "x+y".  This is required in places even though we have historically
  // defaulted to the generation of the operator syntax (e.g. "x+y"), see
  // test2013_100.C for an example of where this is required.
  bool uses_operator_syntax = func_call->get_uses_operator_syntax();
  const std::string user_defined_literal_suffix =
      func_call->get_source_user_defined_literal_suffix().getString();
  const bool is_user_defined_literal_call =
      !user_defined_literal_suffix.empty();
  SgFunctionDeclaration *operator_declaration = nullptr;
  if (uses_operator_syntax) {
    if (operator_ref == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-call-declaration]: "
              "operator-syntax call=%p has no exact operator reference\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
    operator_declaration = func_call->getAssociatedFunctionDeclaration();
    if (operator_declaration == nullptr ||
        (!operator_declaration->get_specialFunctionModifier().isOperator() &&
         !operator_declaration->get_specialFunctionModifier()
              .isUldOperator())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-call-declaration]: "
              "operator-syntax call=%p has no exact operator declaration\n",
              static_cast<void *>(func_call));
      ROSE_ABORT();
    }
  }
  if (is_user_defined_literal_call) {
    if (!uses_operator_syntax || operator_declaration == nullptr ||
        !operator_declaration->get_specialFunctionModifier().isUldOperator() ||
        func_call->get_args() == nullptr ||
        func_call->get_args()->get_expressions().empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal "
              "suffix=%s lacks exact operator syntax, declaration, or source "
              "operand\n",
              user_defined_literal_suffix.c_str());
      ROSE_ABORT();
    }
  } else if (uses_operator_syntax && operator_declaration != nullptr &&
             operator_declaration->get_specialFunctionModifier()
                 .isUldOperator()) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal "
                    "operator-syntax call has no typed source suffix\n");
    ROSE_ABORT();
  }

  bool is_binary_operator =
      uses_operator_syntax &&
      isBinaryOperatorName(exactFunctionBaseName(operator_declaration));
  if (is_binary_operator) {
    SgFunctionDeclaration *decl = operator_declaration;
    const string op_name = exactFunctionBaseName(decl);

    if (op_name == "operator()") {
      is_binary_operator = false;
    } else {
      const size_t arg_count =
          func_call->get_args()
              ? func_call->get_args()->get_expressions().size()
              : 0;

      bool is_member_operator = isMemberOperatorCall(func_call, decl);

      // Member binary operators have one explicit argument; non-members have
      // two.
      const size_t expected_args = is_member_operator ? 1 : 2;
      if (arg_count != expected_args) {
        is_binary_operator = false;
      }
    }
  }

#if DEBUG_FUNCTION_CALL
  printf("In Unparse_ExprStmt::unparseFuncCall(): (before test for conversion "
         "operator) uses_operator_syntax = %s \n",
         uses_operator_syntax == true ? "true" : "false");
  curprint(string("/* In unparseFuncCall(): (before test for conversion "
                  "operator) uses_operator_syntax     = ") +
           (uses_operator_syntax ? "true" : "false") + " */\n");
#endif

#if DEBUG_FUNCTION_CALL
  // DQ (4/8/2013): Test to make sure this is not presently required in our
  // regression tests.
  if (uses_operator_syntax == true) {
    printf("In Unparse_ExprStmt::unparseFuncCall(): Detected "
           "uses_operator_syntax == true \n");
    // ROSE_ASSERT(false);
  }
#endif

#if DEBUG_FUNCTION_CALL
  printf("func_call->get_function() = %p = %s \n", func_call->get_function(),
         func_call->get_function()->class_name().c_str());
#endif

#if DEBUG_FUNCTION_CALL
  printf("In Unparse_ExprStmt::unparseFuncCall(): (after test for conversion "
         "operator) uses_operator_syntax = %s \n",
         uses_operator_syntax == true ? "true" : "false");
  curprint(string("/* In unparseFuncCall(): (after test for conversion "
                  "operator) uses_operator_syntax     = ") +
           (uses_operator_syntax ? "true" : "false") + " */\n");
#endif

#if DEBUG_FUNCTION_CALL
  // DQ (11/16/2013): This need not be a SgFunctionRefExp.
  SgFunctionRefExp *func_ref = isSgFunctionRefExp(func_call->get_function());
  // ASSERT_not_null(func_ref);
  // ASSERT_not_null(func_ref->get_symbol());
  // printf ("Function name = %s \n",func_ref->get_symbol()->get_name().str());
  if (func_ref != NULL) {
    ASSERT_not_null(func_ref->get_symbol());
    printf("Function name = %s \n", func_ref->get_symbol()->get_name().str());
  } else {
    // If this is not a SgFunctionRefExp, then look for a member function
    // reference via a SgDotExp or SgArrowExp.
  }
#endif
#if DEBUG_FUNCTION_CALL
  printf("isBinaryOperator(func_call->get_function())       = %s \n",
         unp->u_sage->isBinaryOperator(func_call->get_function()) ? "true"
                                                                  : "false");
  printf("isSgDotExp(func_call->get_function())             = %s \n",
         isSgDotExp(func_call->get_function()) ? "true" : "false");
  printf("isSgArrowExp(func_call->get_function())           = %s \n",
         isSgArrowExp(func_call->get_function()) ? "true" : "false");

  printf("isUnaryOperatorPlus(func_call->get_function())    = %s \n",
         unp->u_sage->isUnaryOperatorPlus(func_call->get_function()) ? "true"
                                                                     : "false");
  printf("isUnaryOperatorMinus(func_call->get_function())   = %s \n",
         unp->u_sage->isUnaryOperatorMinus(func_call->get_function())
             ? "true"
             : "false");

  printf("isUnaryOperator(func_call->get_function())        = %s \n",
         unp->u_sage->isUnaryOperator(func_call->get_function()) ? "true"
                                                                 : "false");
  printf("isUnaryPostfixOperator(func_call->get_function()) = %s \n",
         unp->u_sage->isUnaryPostfixOperator(func_call->get_function())
             ? "true"
             : "false");
#endif

#if DEBUG_FUNCTION_CALL
  printf("In unparseFuncCall(): "
         "unp->u_sage->isBinaryOperator(func_call->get_function() = %p = %s ) "
         "= %s \n",
         func_call->get_function(),
         func_call->get_function()->class_name().c_str(),
         unp->u_sage->isBinaryOperator(func_call->get_function()) ? "true"
                                                                  : "false");
#endif

  // FIRST PART
  // check if this is an binary operator overloading function and if the
  // overloading option is off. If so, we traverse using "in-order" tree
  // traversal. However, do not enter this segment if we have a dot expression.
  // Dot expressions are handled by the second part. if
  // (!unp->opt.get_overload_opt() &&
  // unp->u_sage->isBinaryOperator(func_call->get_function()) &&
  // !(isSgDotExp(func_call->get_function())) &&
  // !(isSgArrowExp(func_call->get_function()))) if
  // (!unp->opt.get_overload_opt() && (uses_operator_syntax == false) &&
  // unp->u_sage->isBinaryOperator(func_call->get_function()) &&
  // !(isSgDotExp(func_call->get_function())) &&
  // !(isSgArrowExp(func_call->get_function())))
  if (!unp->opt.get_overload_opt() && (uses_operator_syntax == true) &&
      is_binary_operator && !(isSgDotExp(func_call->get_function())) &&
      !(isSgArrowExp(func_call->get_function()))) {
    unp->u_debug->printDebugInfo("in FIRST PART of unparseFuncCall", true);
#if DEBUG_FUNCTION_CALL
    printf("output 1st part (without syntax sugar) \n");
    curprint(" /* output 1st part (without syntax sugar) */ ");
#endif
    ASSERT_not_null(func_call->get_args());
    SgExpressionPtrList &list = func_call->get_args()->get_expressions();
#if DEBUG_FUNCTION_CALL
    printf("In unparseFuncCall(): argument list size = %ld \n", list.size());
#endif
    SgExpressionPtrList::iterator arg = list.begin();
    if (arg != list.end()) {
      newinfo.set_nested_expression();

      if (unp->u_sage->isBinaryBracketOperator(func_call->get_function()) ==
          true) {
        unparseExpression((*arg), newinfo);
        curprint("[");
        arg++;
        ROSE_ASSERT(arg != list.end());
        unparseExpression((*arg), newinfo);
        curprint("]");
        newinfo.unset_nested_expression();
        return;
      }

      // printf ("output function argument (left) \n");

      // unparse the lhs operand
      unp->u_debug->printDebugInfo("left arg: ", false);
      unparseExpression((*arg), newinfo);
      // unparse the operator

      // DQ (6/21/2011): Added support for name qualification.
      info.set_reference_node_for_qualification(func_call->get_function());
      ASSERT_not_null(info.get_reference_node_for_qualification());
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 1st part BEFORE: "
               "unparseExpression(func_call->get_function(), info); */ \n");
#endif
      unparseExpression(func_call->get_function(), info);
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 1st part AFTER: "
               "unparseExpression(func_call->get_function(), info); */ \n");
#endif
      info.set_reference_node_for_qualification(NULL);

      arg++;

      // unparse the rhs operand
      unp->u_debug->printDebugInfo("right arg: ", false);
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 1st part BEFORE: right arg: "
               "unparseExpression(*arg, info); */ \n");
#endif
      // DQ (5/6/2007): Added assert, though this was only a problem when
      // handling unary minus implemented as a non-member function
      ROSE_ASSERT(arg != list.end());
      unparseExpression((*arg), newinfo);
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 1st part AFTER: right arg: "
               "unparseExpression(*arg, info); */ \n");
#endif
      newinfo.unset_nested_expression();

      // printf ("DONE: output function argument (right) \n");
    }
#if DEBUG_FUNCTION_CALL
    curprint("\n/* Leaving processing first part in unparseFuncCall */ \n");
#endif
  } else {
    // SECOND PART
    // this means that we have an unary operator overloading function, a
    // non-operator overloading function, or that the overloading option was
    // turned on.
    unp->u_debug->printDebugInfo("in SECOND PART of unparseFuncCall", true);
    bool print_paren = true;

    // DQ (2/20/2005): By default always output the function arguments (only in
    // the case of the overloaded prefix/postfix increment/decrement operators
    // do we supress their output).
    bool printFunctionArguments = true;

    // if (unp->opt.get_overload_opt())
    if (unp->opt.get_overload_opt() || (uses_operator_syntax == false)) {
      info.set_nested_expression();
    }
#if DEBUG_FUNCTION_CALL
    printf("output 2nd part func_call->get_function() = %s \n",
           func_call->get_function()->class_name().c_str());
    curprint("/* output 2nd part  func_call->get_function() = " +
             func_call->get_function()->class_name() + " */ \n");
#endif

    {
      //
      // Unparse the function first.
      //
      SgUnparse_Info alt_info(info);
      // unparseExpression(func_call->get_function(), info);

      // DQ (6/13/2007): First set to NULL then to the correct value (this
      // allows us to have checking which detects the overwriting of pointer
      // values generally, but it is not relavant in this case).
      alt_info.set_current_function_call(NULL);
      alt_info.set_current_function_call(func_call);

      // DQ (6/21/2011): Added support for name qualification.
      alt_info.set_reference_node_for_qualification(func_call->get_function());
      ASSERT_not_null(alt_info.get_reference_node_for_qualification());
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 2nd part BEFORE: "
               "unparseExpression(func_call->get_function(), info); */ \n");
#endif
#if DEBUG_FUNCTION_CALL
      printf("uses_operator_syntax                                           = "
             "%s \n",
             uses_operator_syntax ? "true" : "false");
      printf("unp->u_sage->isUnaryOperator(func_call->get_function())        = "
             "%s \n",
             unp->u_sage->isUnaryOperator(func_call->get_function()) ? "true"
                                                                     : "false");
      printf("unp->u_sage->isUnaryPostfixOperator(func_call->get_function()) = "
             "%s \n",
             unp->u_sage->isUnaryPostfixOperator(func_call->get_function())
                 ? "true"
                 : "false");
#endif

      // DQ (2/2/2018): Handle the case of a non-postfix operator.
      // unparseExpression(func_call->get_function(), alt_info);
      if (!((uses_operator_syntax == true) &&
            (call_uses_unary_postfix_operator == true))) {
#if DEBUG_FUNCTION_CALL
        // printf ("func_call->get_function()->get_name() = %s
        // \n",func_call->get_function()->get_name().str());
        printf("uses_operator_syntax                                           "
               "= %s \n",
               uses_operator_syntax ? "true" : "false");
        printf("unp->u_sage->isUnaryOperator(func_call->get_function())        "
               "= %s \n",
               unp->u_sage->isUnaryOperator(func_call->get_function())
                   ? "true"
                   : "false");
        printf("unp->u_sage->isUnaryPostfixOperator(func_call->get_function()) "
               "= %s \n",
               unp->u_sage->isUnaryPostfixOperator(func_call->get_function())
                   ? "true"
                   : "false");
        printf("func_call->get_function()                                      "
               "= %p = %s \n",
               func_call->get_function(),
               func_call->get_function()->class_name().c_str());
        printf("###################### Calling "
               "unparseExpression(func_call->get_function(), alt_info); \n");
#endif
        unparseExpression(func_call->get_function(), alt_info);

#if DEBUG_FUNCTION_CALL
        printf("###################### DONE: Calling "
               "unparseExpression(func_call->get_function(), alt_info); \n");
#endif
      }
#if DEBUG_FUNCTION_CALL
      curprint("\n/* In unparseFuncCall(): 2nd part AFTER: "
               "unparseExpression(func_call->get_function(), info); */ \n");
#endif
      alt_info.set_reference_node_for_qualification(NULL);

#if DEBUG_FUNCTION_CALL
      curprint(" /* after output func_call->get_function() */ ");
#endif

      // if (unp->opt.get_overload_opt())
      if (unp->opt.get_overload_opt() || (uses_operator_syntax == false))
        info.unset_nested_expression();

      SgUnparse_Info newinfo(info);

      // now check if the overload option is off and that the function is dot
      // binary expression. If so, check if the rhs is an operator= overloading
      // function (and that the function isn't preceded by a class name). If the
      // operator= is preceded by a class name ("<class>::operator=") then do
      // not set print_paren to false. If so, set print_paren to false,
      // otherwise, set print_paren to true for all other functions.
      //
      // [DT] 4/6/2000 -- Need to check for operator==, also, as well
      //      any other potential overloaded operator that having
      //      this paren would cause a problem.  e.g. in the case
      //      of operator==, we would get something like (x==)(y)
      //      where the paren at ==) comes from unparseBinaryExpr()
      //      and the paren at (y comes from here.
      //
      //      NOTE:  I went ahead and created isBinaryEqualityOperator()
      //      and put the check here.  But there needs to be a more
      //      thorough fix that handles operator<, operator>=, etc...
      //
      //      4/10/2000 -- Created isBinaryInequalityOperator() and
      //      isBinaryArithmeticOperator().  Thinking about simply
      //      creating an isBinaryOverloadedOperator().
      //
      SgBinaryOp *binary_op = isSgBinaryOp(func_call->get_function());
#if DEBUG_FUNCTION_CALL
      curprint(string(" /* !unp->opt.get_overload_opt() && "
                      "(uses_operator_syntax == true) = ") +
               ((!unp->opt.get_overload_opt() && (uses_operator_syntax == true))
                    ? "true"
                    : "false") +
               " */ \n ");
      printf("In unparseFuncCall(): binary_op = %p \n", binary_op);
      printf(" --- func_call->get_function() = %p = %s \n",
             func_call->get_function(),
             func_call->get_function()->class_name().c_str());
#endif
      // if (!unp->opt.get_overload_opt())
      if (!unp->opt.get_overload_opt() && (uses_operator_syntax == true)) {
        // curprint ( "\n /* Unparse so as to suppress overloaded operator
        // function names (generate short syntax) */ \n"; DQ (2/19/2005):
        // Rewrote this case to be more general than just specific to a few
        // operators
        SgExpression *rhs = NULL;
        if (binary_op != NULL) {
          rhs = binary_op->get_rhs_operand();
          ASSERT_not_null(rhs);
        }
        // if ( binary_op != NULL &&
        // rhs->get_specialFunctionModifier().isOperator() &&
        // unp->u_sage->noQualifiedName(rhs) )

#if DEBUG_FUNCTION_CALL
        printf("binary_op = %p rhs = %p \n", binary_op, rhs);
        if (rhs != NULL) {
          printf("rhs       = %s \n", rhs->class_name().c_str());
          printf("binary_op = %s \n", binary_op->class_name().c_str());
        }
        printf("unp->u_sage->noQualifiedName(rhs) = %s \n",
               unp->u_sage->noQualifiedName(rhs) ? "true" : "false");
#endif

        // DQ (12/28/2005): I don't think this need be qualified to permit us to
        // use the "[]" syntax, see test2005_193.C if ( binary_op != NULL &&
        // unp->u_sage->noQualifiedName(rhs) )
        if (binary_op != NULL) {
          // printf ("Found a binary operator without qualification \n");
          // curprint ( "\n /* found a binary operator without qualification */
          // \n";
          SgFunctionRefExp *func_ref = isSgFunctionRefExp(rhs);
          SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(rhs);

          if ((func_ref != NULL) && isOverloadedOperatorReference(func_ref))
            print_paren = false;

          if ((mfunc_ref != NULL) && isOverloadedOperatorReference(mfunc_ref))
            print_paren = false;

          // DQ (2/20/2005) The operator()() is the parenthesis operator and for
          // this case we do want to output "(" and ")"
          if (unp->u_sage->isBinaryParenOperator(rhs) == true)
            print_paren = true;

          // DQ (2/20/2005): Merged code below with this case to simplify
          // operator handling! printf ("isBinaryBracketOperator(rhs) = %s
          // \n",isBinaryBracketOperator(rhs) ? "true" : "false");
          if (unp->u_sage->isBinaryBracketOperator(rhs) == true) {
            // DQ (2/20/2005): Just as for operator()(), operator[]() needs the
            // parens
            print_paren = true;

            // DQ (12/28/2005): This has to reproduce the same logic as in the
            // unparseMFuncRef() function curprint ( " /* Newly handled case in
            // unparser unparseFuncCall() */ ";
            needSquareBrackets = true;
            // Turn off parens in order to output [i] instead of [(i)].
            print_paren = false;
          }

          // DQ (2/20/2005): This operator is special in C++ in that it take an
          // integer parameter when called using the explicit operator function
          // form (e.g. "x.operator++()").  As decribed in C++:
          //      "x.operator++(0)"  --> x++ (the postfix increment operator)
          //      "x.operator++(1)"  --> ++x (the prefix increment operator)
          // an analigious syntax controls the use of the prefix and postfix
          // decrement operator.
          if (unp->u_sage->isUnaryIncrementOperator(rhs) ||
              unp->u_sage->isUnaryDecrementOperator(rhs)) {
            printFunctionArguments = false;
          } else {
            // DQ (2/12/2019): We may have to explicitly detect the literal
            // operators here!
          }
        } else {
          // DQ (2/12/2019): Added this branch for when binary_op == NULL.
          ROSE_ASSERT(binary_op == NULL);

          // ASSERT_not_null(rhs);
          // SgFunctionRefExp*       func_ref  = isSgFunctionRefExp(rhs);
          // SgMemberFunctionRefExp* mfunc_ref = isSgMemberFunctionRefExp(rhs);

          ASSERT_not_null(func_call->get_function());
          if (is_user_defined_literal_call) {
            print_paren = false;
            newinfo.set_user_defined_literal(true);
          }
        }
      }

      //
      // [DT] 3/30/2000 -- In the case of overloaded [] operators,
      //      set a flag indicating that square brackets should be
      //      wrapped around the argument below.  This will
      //      result in the desired syntax in the unparsed code
      //      as long as the unparseMFuncExpr() function knows better
      //      than to output any of ".operator[]".
      //
      //      Q: Need to check unp->opt.get_overload_opt()?
      //
      // MK: Yes! We only need square brackets if
      //     1. unp->opt.get_overload_opt() is false (= keyword "operator" not
      //     required in the output), and
      //     2. we do not have to specify a qualifier; i.e.,
      //     <classname>::<funcname> Otherwise, we print "operator[]" and need
      //     parenthesis "()" around the function argument.
      // DQ (12/10/2004): Skip this simplification if the lhs is a
      // SgPointerDerefExp (i.e. "x->operator[](i)" should not be simplified to
      // "x->[i]")
      if (needSquareBrackets) {
        curprint("[");
      }

      // now unparse the function's arguments
      // if (func_call->get_args() != NULL)
      //      printDebugInfo("unparsing arguments of function call", true);

#if DEBUG_FUNCTION_CALL
      curprint(
          string(
              "\n /* Before preint paren in unparseFuncCall: print_paren = ") +
          (print_paren ? "true" : "false") + " */ \n");
#endif
      if (print_paren) {
#if DEBUG_FUNCTION_CALL
        curprint("\n/* Unparse args in unparseFuncCall: opening */ \n");
#endif
        curprint("(");
        // printDebugInfo("( from FuncCall", true);
      }

      // DQ (2/20/2005): Added case of (printFunctionArguments == true) to
      // handle prefix/postfix increment/decrement overloaded operators (which
      // take an argument to control prefix/postfix, but which should never be
      // output unless we are trying to reproduce the operator function call
      // syntax e.g. "x.operator++(0)" or "x.operator++(1)").
      if ((printFunctionArguments == true) && (func_call->get_args() != NULL)) {
        const bool requiresSpecialArgumentSelection =
            is_user_defined_literal_call ||
            ((uses_operator_syntax == true) &&
             (call_uses_unary_postfix_operator == true));
        if (!requiresSpecialArgumentSelection) {
          unparseExpression(func_call->get_args(), newinfo);
        } else {
          SgExpressionPtrList &list = func_call->get_args()->get_expressions();
          SgExpressionPtrList::iterator arg = list.begin();
          while (arg != list.end()) {
            if (*arg == nullptr || (*arg)->get_file_info() == nullptr) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[function-call-argument]: call=%p "
                      "has a null argument or missing source provenance\n",
                      static_cast<void *>(func_call));
              ROSE_ABORT();
            }
            if ((*arg)->get_file_info()->isDefaultArgument()) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[function-call-argument]: call=%p "
                      "contains an implicit default argument instead of only "
                      "source-explicit arguments\n",
                      static_cast<void *>(func_call));
              ROSE_ABORT();
            }
#if DEBUG_FUNCTION_CALL
            printf("func_call->get_args() = %p = %s arg = %p = %s\n",
                   func_call->get_args(),
                   func_call->get_args()->class_name().c_str(), *arg,
                   (*arg)->class_name().c_str());
#endif
            if (arg != list.begin()) {
              curprint(", ");
            }

            unparseExpression((*arg), newinfo);
            if (is_user_defined_literal_call) {
              break;
            }

            arg++;

            // DQ (1/2/2018): Supress the trailing function argument in the case
            // of a postfix non-member function using operator syntax.
            if ((uses_operator_syntax == true) &&
                (call_uses_unary_postfix_operator == true)) {
#if DEBUG_FUNCTION_CALL
              printf("Suppress the trailing argument of the unary postfix "
                     "operator \n");
              curprint(
                  "\n/* Suppress the trailing argument of the unary postfix "
                  "operator in unparseFuncCall */ \n");
#endif
              // DQ (2/12/2019): Debugging C++11 literal operators.
              // ROSE_ASSERT(arg != list.end());
              // arg++;
              if (arg != list.end()) {
                arg++;
              } else {
              }
            }
          }
        }
      }

      if (!user_defined_literal_suffix.empty()) {
        curprint(user_defined_literal_suffix);
      }

      if (print_paren) {
#if DEBUG_FUNCTION_CALL
        curprint("\n/* Unparse args in unparseFuncCall: closing */ \n");
#endif
        curprint(")");
        // printDebugInfo(") from FuncCall", true);
      }

      if (needSquareBrackets) {
        curprint("]");
        // curprint(" /* needSquareBrackets == true */ ]");
      }
    }

    // DQ (2/2/2018): Handle the case of a postfix operator.
    if ((uses_operator_syntax == true) &&
        (call_uses_unary_postfix_operator == true)) {
      SgUnparse_Info alt_info(info);
      unparseExpression(func_call->get_function(), alt_info);
    }
#if DEBUG_FUNCTION_CALL
    curprint("\n/* Leaving processing second part in unparseFuncCall */ \n");
#endif
  }

#if DEBUG_FUNCTION_CALL
  printf("Leaving Unparse_ExprStmt::unparseFuncCall = %p \n", expr);
  curprint("\n/* Leaving Unparse_ExprStmt::unparseFuncCall " +
           StringUtility::numberToString(expr) + " */ \n");
#endif
}

void Unparse_ExprStmt::unparsePointStOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "->", info);
}

void Unparse_ExprStmt::unparseRecRef(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ".", info);
}
void Unparse_ExprStmt::unparseDotStarOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ".*", info);
}
void Unparse_ExprStmt::unparseArrowStarOp(SgExpression *expr,
                                          SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "->*", info);
}
void Unparse_ExprStmt::unparseEqOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "==", info);
}
void Unparse_ExprStmt::unparseLtOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "<", info);
}
void Unparse_ExprStmt::unparseGtOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ">", info);
}
void Unparse_ExprStmt::unparseNeOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "!=", info);
}
void Unparse_ExprStmt::unparseLeOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "<=", info);
}
void Unparse_ExprStmt::unparseGeOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ">=", info);
}
void Unparse_ExprStmt::unparseAddOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "+", info);
}
void Unparse_ExprStmt::unparseSubtOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "-", info);
}
void Unparse_ExprStmt::unparseMultOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "*", info);
}
void Unparse_ExprStmt::unparseDivOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "/", info);
}
void Unparse_ExprStmt::unparseIntDivOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "/", info);
}
void Unparse_ExprStmt::unparseModOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "%", info);
}
void Unparse_ExprStmt::unparseAndOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "&&", info);
}
void Unparse_ExprStmt::unparseOrOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "||", info);
}
void Unparse_ExprStmt::unparseBitXOrOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "^", info);
}
void Unparse_ExprStmt::unparseBitAndOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "&", info);
}
void Unparse_ExprStmt::unparseBitOrOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "|", info);
}
void Unparse_ExprStmt::unparseCommaOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  curprint("(");
  unparseBinaryOperator(expr, ",", info);
  curprint(")");
}
void Unparse_ExprStmt::unparseLShiftOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "<<", info);
}
void Unparse_ExprStmt::unparseRShiftOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ">>", info);
}
void Unparse_ExprStmt::unparseUnaryMinusOp(SgExpression *expr,
                                           SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "-", info);
}
void Unparse_ExprStmt::unparseUnaryAddOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "+", info);
}

// DQ (7/26/2020): Adding support for C++20 spaceship operator.
void Unparse_ExprStmt::unparseSpaceshipOp(SgExpression *expr,
                                          SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "<=>", info);
}

// DQ (7/26/2020): Adding support for C++20 await expression.
void Unparse_ExprStmt::unparseAwaitExpression(SgExpression *expr,
                                              SgUnparse_Info &info) {
  SgAwaitExpression *await_expr = isSgAwaitExpression(expr);
  ASSERT_not_null(await_expr);

  const char *keyword = nullptr;
  switch (await_expr->get_coroutine_keyword_kind()) {
  case SgAwaitExpression::e_coroutine_keyword_co_await:
    keyword = "co_await";
    break;
  case SgAwaitExpression::e_coroutine_keyword_co_yield:
    keyword = "co_yield";
    break;
  case SgAwaitExpression::e_coroutine_keyword_unspecified:
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[coroutine-keyword]: await expression has "
            "invalid typed source kind=%d\n",
            static_cast<int>(await_expr->get_coroutine_keyword_kind()));
    ROSE_ABORT();
  }
  SgExpression *value = await_expr->get_value();
  if (value == nullptr || value->get_parent() != await_expr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[coroutine-operand]: await expression has "
            "no exact owned operand\n");
    ROSE_ABORT();
  }

  curprint(keyword);
  curprint(" ");
  SgUnparse_Info ninfo(info);
  unparseExpression(value, ninfo);
}

// DQ (7/26/2020): Adding support for C++20 choose expression.
void Unparse_ExprStmt::unparseChooseExpression(SgExpression *expr,
                                               SgUnparse_Info &info) {
  SgChooseExpression *choose = isSgChooseExpression(expr);
  ASSERT_not_null(choose);
  SgExpression *condition = choose->get_condition();
  SgExpression *true_expression = choose->get_true_expression();
  SgExpression *false_expression = choose->get_false_expression();
  if (condition == nullptr || true_expression == nullptr ||
      false_expression == nullptr || condition->get_parent() != choose ||
      true_expression->get_parent() != choose ||
      false_expression->get_parent() != choose) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[choose-expression]: expression=%p does "
            "not own one exact condition and two result arms\n",
            static_cast<void *>(choose));
    ROSE_ABORT();
  }
  (void)choose->get_type();
  curprint("__builtin_choose_expr(");
  unparseExpression(condition, info);
  curprint(", ");
  unparseExpression(true_expression, info);
  curprint(", ");
  unparseExpression(false_expression, info);
  curprint(")");
}

// DQ (7/26/2020): Adding support for C++20 expression folding expression.
void Unparse_ExprStmt::unparseFoldExpression(SgExpression *expr,
                                             SgUnparse_Info &info) {
  SgFoldExpression *foldExpression = isSgFoldExpression(expr);
  ASSERT_not_null(foldExpression);

  SgExpression *operands = foldExpression->get_operands();
  ASSERT_not_null(operands);

  const std::string &operator_token = foldExpression->get_operator_token();
  ROSE_ASSERT(!operator_token.empty());

  if (SgExprListExp *operand_list = isSgExprListExp(operands)) {
    SgExpressionPtrList &expressions = operand_list->get_expressions();
    ROSE_ASSERT(expressions.size() == 2);
    ASSERT_not_null(expressions[0]);
    ASSERT_not_null(expressions[1]);

    curprint("(");
    curprint("(");
    unparseExpression(expressions[0], info);
    curprint(")");
    curprint(" ");
    curprint(operator_token.c_str());
    curprint(" ... ");
    curprint(operator_token.c_str());
    curprint(" ");
    curprint("(");
    unparseExpression(expressions[1], info);
    curprint(")");
    curprint(")");
    return;
  }

  curprint("(");
  if (foldExpression->get_is_left_associative()) {
    curprint("... ");
    curprint(operator_token.c_str());
    curprint(" ");
    unparseExpression(operands, info);
  } else {
    unparseExpression(operands, info);
    curprint(" ");
    curprint(operator_token.c_str());
    curprint(" ...");
  }
  curprint(")");
}

void Unparse_ExprStmt::unparseSizeOfOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgSizeOfOp *sizeof_op = isSgSizeOfOp(expr);
  ASSERT_not_null(sizeof_op);

  // DQ (10/19/2012): This is the explicitly set boolean value which indicates
  // that a class declaration is buried inside the current cast expression's
  // reference to a type (e.g. "(((union ABC { int __in; int __i; }) { .__in =
  // 42 }).__i);"). In this case we have to output the base type with its
  // definition.
  const bool outputTypeDefinition = expressionOwnsInlineTypeDefinition(
      sizeof_op, sizeof_op->get_operand_type(),
      sizeof_op->get_type_defining_declaration(), "SgSizeOfOp");

  if (sizeof_op->get_is_sizeof_pack()) {
    curprint("sizeof...(");
  } else {
    curprint("sizeof(");
  }

  SgExpression *sizeofExpression = sizeof_op->get_operand_expr();
  // if (sizeof_op->get_operand_expr() != NULL)
  if (sizeofExpression != NULL) {
    ASSERT_not_null(sizeofExpression);

    if (sizeof_op->get_is_sizeof_pack()) {
      if (SgTemplateParameterVal *pack_parameter_value =
              isSgTemplateParameterVal(sizeofExpression)) {
        std::string pack_name = pack_parameter_value->get_valueString();
        if (!pack_name.empty()) {
          curprint(pack_name);
          curprint(")");
          return;
        }
      }

      if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(sizeofExpression)) {
        if (nonreal_ref->get_symbol() != NULL) {
          curprint(nonreal_ref->get_symbol()->get_name().str());
          curprint(")");
          return;
        }
      }
    }

    // DQ (1/12/2019): Adding support for C++11 feature (see test2019_10.C).
    if (sizeof_op->get_is_objectless_nonstatic_data_member_reference() ==
        true) {
      // Output the name of the class (but don't conside this to be name
      // qualification). Need to find the member reference.
      SgArrowExp *arrowExp = isSgArrowExp(sizeofExpression);
      ASSERT_not_null(arrowExp);

      // SgExpression* lhs = arrowExp->get_lhs_operand();
      // ASSERT_not_null(lhs);
      // printf ("lhs = %p = %s \n",lhs,lhs->class_name().c_str());

      SgExpression *rhs = arrowExp->get_rhs_operand();
      ASSERT_not_null(rhs);
      SgVarRefExp *varRef = isSgVarRefExp(rhs);
      ASSERT_not_null(varRef);

      unparseExpression(varRef, info);
    } else {
      // DQ (1/12/2019): Previous code before supporting C++11 objectless
      // non-static data member references.
      unparseExpression(sizeofExpression, info);
    }
  } else {
    ASSERT_not_null(sizeof_op->get_operand_type());

    SgUnparse_Info info2(info);
    info2.unset_SkipBaseType();

    // DQ (11/3/2015): We might have to use the "struct" class elaboration if
    // this is a type. We have to turn this back on in the case where we are in
    // a for loop test (condition) where it would be turned off as a result of a
    // fix to make handling of the test expression more unifor between
    // token-based unparsing and the AST unparsing.
    info2.unset_SkipClassSpecifier();

    // DQ (3/15/2015): test2015_11.c demonstrates a case where I think this
    // should be not be set (un-named struct type).
    // info2.set_SkipClassDefinition();

    info2.unset_isTypeFirstPart();
    info2.unset_isTypeSecondPart();

    // DQ (6/2/2011): Added support for name qualification of types reference
    // via sizeof operator.
    info2.set_reference_node_for_qualification(sizeof_op);

    // DQ (10/19/2012): Modified to support output of the type's defining
    // declaration (see test2012_57.c).
    // unp->u_type->unparseType(sizeof_op->get_operand_type(), info2);

    SgUnparse_Info newinfo(info2);
    applyTypeReferenceInfoFromExpression(unp, sizeof_op, newinfo);

    if (outputTypeDefinition == true) {
      // DQ (10/11/2006): As part of new implementation of qualified names we
      // now default to the generation of all qualified names unless they are
      // skipped. newinfo.set_SkipQualifiedNames(); DQ (3/15/2015):
      // test2015_11.c demonstrates a case where I think this should be not be
      // set (un-named struct type). DQ (10/17/2012): Added new code not present
      // where this is handled for SgVariableDeclaration IR nodes.
      newinfo.unset_SkipDefinition();

      // DQ (5/23/2007): Commented these out since they are not applicable for
      // statement expressions (see test2007_51.C). DQ (10/5/2004): If this is a
      // defining declaration then make sure that we don't skip the definition
      ROSE_ASSERT(newinfo.SkipClassDefinition() == false);
      ROSE_ASSERT(newinfo.SkipEnumDefinition() == false);
      ROSE_ASSERT(newinfo.SkipDefinition() == false);
    } else {
      newinfo.set_SkipDefinition();
      ROSE_ASSERT(newinfo.SkipClassDefinition() == true);
      ROSE_ASSERT(newinfo.SkipEnumDefinition() == true);
    }

    // DQ (10/18/2012): Added to unset ";" usage in defining declaration.
    newinfo.unset_SkipSemiColon();

    // DQ (10/17/2012): We have to separate these out if we want to output the
    // defining declarations.
    newinfo.set_isTypeFirstPart();

    // DQ (1/6/2020): The type will be an argument to the sizeof operator (see
    // Cxx11_tests/test2020_14.C).
    newinfo.set_inArgList();
    unp->u_type->unparseType(sizeof_op->get_operand_type(), newinfo);
    newinfo.set_isTypeSecondPart();
    unp->u_type->unparseType(sizeof_op->get_operand_type(), newinfo);
  }

  curprint(")");
}

void Unparse_ExprStmt::unparseAlignOfOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  SgAlignOfOp *sizeof_op = isSgAlignOfOp(expr);
  ASSERT_not_null(sizeof_op);

  // DQ (10/19/2012): This is the explicitly set boolean value which indicates
  // that a class declaration is buried inside the current cast expression's
  // reference to a type (e.g. "(((union ABC { int __in; int __i; }) { .__in =
  // 42 }).__i);"). In this case we have to output the base type with its
  // definition.
  const bool outputTypeDefinition = expressionOwnsInlineTypeDefinition(
      sizeof_op, sizeof_op->get_operand_type(),
      sizeof_op->get_type_defining_declaration(), "SgAlignOfOp");

  // curprint ( "alignof(");
  curprint("__alignof__(");

  if (sizeof_op->get_operand_expr() != NULL) {
    ASSERT_not_null(sizeof_op->get_operand_expr());
    unparseExpression(sizeof_op->get_operand_expr(), info);
  } else {
    ASSERT_not_null(sizeof_op->get_operand_type());
    SgUnparse_Info info2(info);
    info2.unset_SkipBaseType();

    info2.set_SkipClassDefinition();
    // DQ (9/9/2016): Added call to set_SkipEnumDefinition().
    info2.set_SkipEnumDefinition();

    info2.unset_isTypeFirstPart();
    info2.unset_isTypeSecondPart();

    // DQ (6/2/2011): Added support for name qualification of types reference
    // via sizeof operator.
    info2.set_reference_node_for_qualification(sizeof_op);

    // DQ (10/19/2012): Modified to support output of the type's defining
    // declaration (see test2012_57.c).
    // unp->u_type->unparseType(sizeof_op->get_operand_type(), info2);

    SgUnparse_Info newinfo(info2);
    applyTypeReferenceInfoFromExpression(unp, sizeof_op, newinfo);

    if (outputTypeDefinition == true) {
      // DQ (10/11/2006): As part of new implementation of qualified names we
      // now default to the generation of all qualified names unless they are
      // skipped. newinfo.set_SkipQualifiedNames();

      // DQ (10/17/2012): Added new code not present where this is handled for
      // SgVariableDeclaration IR nodes.
      newinfo.unset_SkipDefinition();

      // DQ (5/23/2007): Commented these out since they are not applicable for
      // statement expressions (see test2007_51.C). DQ (10/5/2004): If this is a
      // defining declaration then make sure that we don't skip the definition
      ROSE_ASSERT(newinfo.SkipClassDefinition() == false);
      ROSE_ASSERT(newinfo.SkipEnumDefinition() == false);
      ROSE_ASSERT(newinfo.SkipDefinition() == false);
    } else {
      newinfo.set_SkipDefinition();
      ROSE_ASSERT(newinfo.SkipClassDefinition() == true);
      ROSE_ASSERT(newinfo.SkipEnumDefinition() == true);
    }

    // DQ (10/18/2012): Added to unset ";" usage in defining declaration.
    newinfo.unset_SkipSemiColon();
    // DQ (10/17/2012): We have to separate these out if we want to output the
    // defining declarations.
    newinfo.set_isTypeFirstPart();
    unp->u_type->unparseType(sizeof_op->get_operand_type(), newinfo);
    newinfo.set_isTypeSecondPart();
    unp->u_type->unparseType(sizeof_op->get_operand_type(), newinfo);
  }
  curprint(")");
}

void Unparse_ExprStmt::unparseNoexceptOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  SgNoexceptOp *noexcept_op = isSgNoexceptOp(expr);
  ASSERT_not_null(noexcept_op);

  curprint("noexcept(");

  ASSERT_not_null(noexcept_op->get_operand_expr());
  unparseExpression(noexcept_op->get_operand_expr(), info);

  curprint(")");
}

void Unparse_ExprStmt::unparseTypeIdOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgTypeIdOp *typeid_op = isSgTypeIdOp(expr);
  ASSERT_not_null(typeid_op);

  curprint("typeid(");
  if (typeid_op->get_operand_expr() != nullptr &&
      !isSgTypeExpression(typeid_op->get_operand_expr())) {
    ASSERT_not_null(typeid_op->get_operand_expr());
    unparseExpression(typeid_op->get_operand_expr(), info);
  } else {
    SgType *type;

    if (typeid_op->get_operand_type() != nullptr) {
      type = typeid_op->get_operand_type();
    } else {
      ASSERT_not_null(typeid_op->get_operand_expr());
      SgTypeExpression *type_expression =
          isSgTypeExpression(typeid_op->get_operand_expr());
      ASSERT_not_null(type_expression);
      type = type_expression->get_represented_type();
    }

    ASSERT_not_null(type);
    SgUnparse_Info info2(info);
    info2.unset_SkipBaseType();
    info2.set_SkipClassDefinition();

    // DQ (10/28/2015): This will be enforced uniformally with
    // SkipClassDefinition() in the unparseType() function below.
    info2.set_SkipEnumDefinition();

    // DQ (6/2/2011): Added support for name qualification of types reference
    // via sizeof operator.
    info2.set_reference_node_for_qualification(typeid_op);

    // DQ (10/28/2015): This will be enforced in the unparseType() function
    // (so detect it here where it is more clear how to fix it, above).
    ROSE_ASSERT(info2.SkipClassDefinition() == info2.SkipEnumDefinition());

    applyTypeReferenceInfoFromExpression(unp, typeid_op, info2);
    unp->u_type->unparseType(type, info2);
  }

  curprint(")");
}

void Unparse_ExprStmt::unparseNotOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "!", info);
}
void Unparse_ExprStmt::unparseDerefOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "*", info);
}
void Unparse_ExprStmt::unparseAddrOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "&", info);
}
void Unparse_ExprStmt::unparseMinusMinusOp(SgExpression *expr,
                                           SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "--", info);
}
void Unparse_ExprStmt::unparsePlusPlusOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "++", info);
}
void Unparse_ExprStmt::unparseAbstractOp(SgExpression *, SgUnparse_Info &) {}
void Unparse_ExprStmt::unparseBitCompOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "~", info);
}
void Unparse_ExprStmt::unparseRealPartOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "__real__ ", info);
}
void Unparse_ExprStmt::unparseImagPartOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "__imag__ ", info);
}
void Unparse_ExprStmt::unparseConjugateOp(SgExpression *expr,
                                          SgUnparse_Info &info) {
  unparseUnaryOperator(expr, "~", info);
}

void Unparse_ExprStmt::unparseExprCond(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgConditionalExp *expr_cond = isSgConditionalExp(expr);
  ASSERT_not_null(expr_cond);
  expr_cond->validate();

  // Parenthesization is decided once by requiresParentheses() from the exact
  // typed parent/operand relationship.  The former nested-expression and
  // lvalue checks emitted a second, context-free pair here, producing
  // constructs such as `to((condition ? lhs : rhs))` even though the OpenMP
  // clause already owns the required delimiter.
  info.set_nested_expression();

  unparseExpression(expr_cond->get_conditional_exp(), info);

  if (expr_cond->get_operator_kind() ==
      SgConditionalExp::e_conditional_operator_gnu_binary) {
    curprint(" ?: ");
    unparseExpression(expr_cond->get_false_exp(), info);
    info.unset_nested_expression();
    return;
  }
  if (expr_cond->get_operator_kind() !=
      SgConditionalExp::e_conditional_operator_standard) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[conditional-expression-kind]: "
            "conditional=%p has unsupported operator kind=%d\n",
            static_cast<void *>(expr_cond),
            static_cast<int>(expr_cond->get_operator_kind()));
    ROSE_ABORT();
  }

  // DQ (1/26/2009): Added spaces to make the formatting nicer (but it breaks
  // the diff tests in the loop processor, so fix this later). curprint (" ? ");
  curprint("?");

  // DQ (7/20/2024): This should be a non-null pointer.
  ROSE_ASSERT(expr_cond->get_true_exp() != NULL);

  unparseExpression(expr_cond->get_true_exp(), info);

  // Liao, 2/16/2009. We have to have space to avoid first?x:::std::string("")
  // Three colons in a row! DQ (1/26/2009): Added spaces to make the formatting
  // nicer (but it breaks the diff tests in the loop processor, so fix this
  // later).
  curprint(" : ");
  // curprint (":");

  // DQ (7/20/2024): This should be a non-null pointer.
  ROSE_ASSERT(expr_cond->get_false_exp() != NULL);

  unparseExpression(expr_cond->get_false_exp(), info);
  info.unset_nested_expression();
}

void Unparse_ExprStmt::unparseClassInitOp(SgExpression *, SgUnparse_Info &) {}

void Unparse_ExprStmt::unparseDyCastOp(SgExpression *, SgUnparse_Info &) {}

void Unparse_ExprStmt::unparseCastOp(SgExpression *expr, SgUnparse_Info &info) {
  SgCastExp *cast_op = isSgCastExp(expr);
  ASSERT_not_null(cast_op);
  cast_op->validate_semantic_conversion();
  if (cast_op->get_operand() == nullptr || cast_op->get_type() == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cast-expression-role]: cast kind=%d has "
            "no exact operand or target type\n",
            static_cast<int>(cast_op->cast_type()));
    ROSE_ABORT();
  }

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgUnparse_Info newinfo(info);
  newinfo.unset_PrintName();
  newinfo.unset_isTypeFirstPart();
  newinfo.unset_isTypeSecondPart();

  // DQ (5/30/2011): Added support for name qualification.
  newinfo.set_reference_node_for_qualification(cast_op);
  ASSERT_not_null(newinfo.get_reference_node_for_qualification());

  // DQ (10/8/2004): Never unparse the declaration from within a cast expression
  // (see testcode2001_28.C)!
  newinfo.set_SkipDefinition();

  newinfo.unset_SkipBaseType();

  // printf ("In unparseCastOp(): cast_op->cast_type() = %d
  // \n",cast_op->cast_type()); curprint ( "/* In unparseCastOp():
  // cast_op->cast_type() = " + cast_op->cast_type() + " */";

  // DQ (6/2/2011): I think this is all that is required.
  // SgName nameQualifier =
  // cast_op->get_qualified_name_prefix_for_referenced_type(); curprint ("/*
  // nameQualifier = " + nameQualifier + " */ \n");
  newinfo.set_reference_node_for_qualification(cast_op);

  // DQ (10/17/2012): This is the explicitly set boolean value which indicates
  // that a class declaration is buried inside the current cast expression's
  // reference to a type (e.g. "(((union ABC { int __in; int __i; }) { .__in =
  // 42 }).__i);"). In this case we have to output the base type with its
  // definition.
  SgType *emitted_type = cast_op->get_cast_type() == SgCastExp::e_implicit_cast
                             ? cast_op->get_type()
                             : cast_op->get_source_type();
  if (cast_op->get_cast_type() != SgCastExp::e_implicit_cast &&
      (emitted_type == nullptr || isSgTypeUnknown(emitted_type) != nullptr ||
       isSgTypeDefault(emitted_type) != nullptr ||
       SageInterface::containsUnknownType(emitted_type))) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cast-source-type]: explicit cast=%p "
            "surface=%d has no exact source-spelled type\n",
            static_cast<void *>(cast_op),
            static_cast<int>(cast_op->get_cast_type()));
    ROSE_ABORT();
  }
  const bool outputTypeDefinition = expressionOwnsInlineTypeDefinition(
      cast_op, emitted_type, cast_op->get_type_defining_declaration(),
      "SgCastExp");

  if (outputTypeDefinition == true) {
    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // newinfo.set_SkipQualifiedNames();

    // DQ (10/17/2012): Added new code not present where this is handled for
    // SgVariableDeclaration IR nodes.
    newinfo.unset_SkipDefinition();

    // DQ (5/23/2007): Commented these out since they are not applicable for
    // statement expressions (see test2007_51.C). DQ (10/5/2004): If this is a
    // defining declaration then make sure that we don't skip the definition
    ROSE_ASSERT(newinfo.SkipClassDefinition() == false);
    ROSE_ASSERT(newinfo.SkipEnumDefinition() == false);
    ROSE_ASSERT(newinfo.SkipDefinition() == false);
  } else {
    newinfo.set_SkipDefinition();
    ROSE_ASSERT(newinfo.SkipClassDefinition() == true);
    ROSE_ASSERT(newinfo.SkipEnumDefinition() == true);
  }

  applyTypeReferenceInfoFromExpression(unp, cast_op, newinfo);

  auto unparse_cast_type = [&]() {
    newinfo.unset_SkipSemiColon();
    newinfo.unset_SkipClassSpecifier();
    newinfo.set_reference_node_for_qualification(cast_op);
    newinfo.set_isTypeFirstPart();
    unp->u_type->unparseType(emitted_type, newinfo);
    newinfo.set_isTypeSecondPart();
    unp->u_type->unparseType(emitted_type, newinfo);
  };

  if (cast_op->get_cast_type() == SgCastExp::e_builtin_bit_cast) {
    curprint("__builtin_bit_cast(");
    unparse_cast_type();
    curprint(", ");
    info.set_reference_node_for_qualification(cast_op->get_operand());
    unparseExpression(cast_op->get_operand(), info);
    curprint(")");
    return;
  }

  if (cast_op->get_cast_type() == SgCastExp::e_functional_cast ||
      cast_op->get_cast_type() == SgCastExp::e_functional_list_cast) {
    unparse_cast_type();
    const bool braced =
        cast_op->get_cast_type() == SgCastExp::e_functional_list_cast;
    curprint(braced ? "{" : "(");
    SgExprListExp *arguments = nullptr;
    if (SgConstructorInitializer *constructor =
            isSgConstructorInitializer(cast_op->get_operand())) {
      arguments = constructor->get_args();
    } else if (SgAggregateInitializer *aggregate =
                   isSgAggregateInitializer(cast_op->get_operand())) {
      arguments = aggregate->get_initializers();
    }
    if (arguments != nullptr) {
      const SgExpressionPtrList &expressions = arguments->get_expressions();
      for (size_t i = 0; i < expressions.size(); ++i) {
        if (i != 0) {
          curprint(", ");
        }
        SgUnparse_Info argument_info(info);
        argument_info.set_reference_node_for_qualification(expressions[i]);
        unparseExpression(expressions[i], argument_info);
      }
    } else {
      if (isSgExprListExp(cast_op->get_operand()) != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[functional-cast-operand]: scalar "
                "functional cast owns an ambiguous raw expression list\n");
        ROSE_ABORT();
      }
      info.set_reference_node_for_qualification(cast_op->get_operand());
      unparseExpression(cast_op->get_operand(), info);
    }
    curprint(braced ? "}" : ")");
    return;
  }

  bool addParens = false;

  switch (cast_op->cast_type()) {
  case SgCastExp::e_unknown: {
    printf("SgCastExp::e_unknown found \n");
    ROSE_ABORT();
  }

  case SgCastExp::e_dynamic_cast: {
    // dynamic_cast <P *> (expr)
    curprint("dynamic_cast < ");
    unparse_cast_type();
    curprint(" > "); // paren are in operand_i
    addParens = true;
    break;
  }

  case SgCastExp::e_reinterpret_cast: {
    // reinterpret_cast <P *> (expr)
    curprint("reinterpret_cast < ");
    unparse_cast_type();
    curprint(" > ");
    addParens = true;
    break;
  }

  case SgCastExp::e_const_cast: {
    // const_cast <P *> (expr)
    curprint("const_cast < ");
    unparse_cast_type();
    curprint(" > ");
    addParens = true;
    break;
  }

  case SgCastExp::e_static_cast: {
    // static_cast <P *> (expr)
    curprint("static_cast < ");
    unparse_cast_type();
    curprint(" > ");
    addParens = true;
    break;
  }

  case SgCastExp::e_C_style_cast: {
    curprint("(");
    unparse_cast_type();
    curprint(")");
    break;
  }

  case SgCastExp::e_implicit_cast: {
    for (Sg_File_Info *file_info :
         {cast_op->get_file_info(), cast_op->get_startOfConstruct(),
          cast_op->get_endOfConstruct(), cast_op->get_operatorPosition()}) {
      if (file_info == nullptr || !file_info->isCompilerGenerated() ||
          !file_info->isOutputInCodeGeneration() ||
          !file_info->isImplicitCast()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[cast-expression-role]: implicit cast "
                "lacks exact synthesized implicit-conversion provenance\n");
        ROSE_ABORT();
      }
    }
    break;
  }

  case SgCastExp::e_builtin_bit_cast:
  case SgCastExp::e_functional_cast:
  case SgCastExp::e_functional_list_cast:
    ROSE_ABORT(); // Handled and returned above.

  default: {
    printf("Default reached in cast_op->cast_type() = %d \n",
           cast_op->cast_type());
    ROSE_ABORT();
  }
  }

  // DQ (6/15/2005): reinterpret_cast always needs parens
  if (addParens == true) {
    curprint(" (");
  }

  // DQ (6/21/2011): Added support for name qualification.
  info.set_reference_node_for_qualification(cast_op->get_operand());
  ASSERT_not_null(info.get_reference_node_for_qualification());

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  unparseExpression(cast_op->get_operand(), info);

  if (addParens == true) {
    curprint(")");
  }
}

void Unparse_ExprStmt::unparseArrayOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "[]", info);
}

void Unparse_ExprStmt::unparseDeleteOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgDeleteExp *delete_op = isSgDeleteExp(expr);
  ASSERT_not_null(delete_op);

  if (delete_op->get_need_global_specifier()) {
    curprint(":: ");
  }
  curprint("delete ");
  SgUnparse_Info newinfo(info);
  if (delete_op->get_is_array()) {
    curprint("[]");
  }
  unparseExpression(delete_op->get_variable(), newinfo);
}

void Unparse_ExprStmt::unparseThisNode(SgExpression *expr, SgUnparse_Info &) {
  SgThisExp *this_node = isSgThisExp(expr);

  ASSERT_not_null(this_node);

  // printf ("In Unparse_ExprStmt::unparseThisNode: unp->opt.get_this_opt() = %s
  // \n", (unp->opt.get_this_opt()) ? "true" : "false");

  if (unp->opt.get_this_opt()) // Checks options to determine whether to print
                               // "this"
  {
    curprint("this");
  }
}

void Unparse_ExprStmt::unparseScopeOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  SgScopeOp *scope_op = isSgScopeOp(expr);
  ASSERT_not_null(scope_op);

  if (scope_op->get_lhs_operand())
    unparseExpression(scope_op->get_lhs_operand(), info);
  curprint("::");
  unparseExpression(scope_op->get_rhs_operand(), info);
}

void Unparse_ExprStmt::unparseAssnOp(SgExpression *expr, SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "=", info);
}
void Unparse_ExprStmt::unparsePlusAssnOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "+=", info);
}
void Unparse_ExprStmt::unparseMinusAssnOp(SgExpression *expr,
                                          SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "-=", info);
}
void Unparse_ExprStmt::unparseAndAssnOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "&=", info);
}
void Unparse_ExprStmt::unparseIOrAssnOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "|=", info);
}
void Unparse_ExprStmt::unparseMultAssnOp(SgExpression *expr,
                                         SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "*=", info);
}
void Unparse_ExprStmt::unparseDivAssnOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "/=", info);
}
void Unparse_ExprStmt::unparseModAssnOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "%=", info);
}
void Unparse_ExprStmt::unparseXorAssnOp(SgExpression *expr,
                                        SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "^=", info);
}

void Unparse_ExprStmt::unparseLShiftAssnOp(SgExpression *expr,
                                           SgUnparse_Info &info) {
  unparseBinaryOperator(expr, "<<=", info);
}
void Unparse_ExprStmt::unparseRShiftAssnOp(SgExpression *expr,
                                           SgUnparse_Info &info) {
  unparseBinaryOperator(expr, ">>=", info);
}

void Unparse_ExprStmt::unparseForDeclOp(SgExpression *expr, SgUnparse_Info &) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[abstract-expression]: node=%p type=%s uses "
          "the legacy ForDecl expression placeholder\n",
          static_cast<void *>(expr),
          expr != nullptr ? expr->class_name().c_str() : "<null>");
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseTypeRef(SgExpression *expr,
                                      SgUnparse_Info &info) {
  SgRefExp *type_ref = isSgRefExp(expr);
  ASSERT_not_null(type_ref);

  SgUnparse_Info newinfo(info);
  newinfo.unset_PrintName();
  newinfo.unset_isTypeFirstPart();
  newinfo.unset_isTypeSecondPart();

  unp->u_type->unparseType(type_ref->get_type_name(), newinfo);
}

void Unparse_ExprStmt::unparseVConst(SgExpression *expr, SgUnparse_Info &) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[abstract-expression]: node=%p type=%s uses "
          "the legacy VConst expression placeholder\n",
          static_cast<void *>(expr),
          expr != nullptr ? expr->class_name().c_str() : "<null>");
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseExprInit(SgExpression *expr, SgUnparse_Info &) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[abstract-expression]: node=%p type=%s uses "
          "the abstract initializer node instead of a concrete initializer\n",
          static_cast<void *>(expr),
          expr != nullptr ? expr->class_name().c_str() : "<null>");
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseThrowOp(SgExpression *expr,
                                      SgUnparse_Info &info) {
  SgThrowOp *throw_op = isSgThrowOp(expr);
  ASSERT_not_null(throw_op);

  // printf ("In unparseThrowOp(%s) \n",expr->sage_class_name());
  // curprint ( "\n/* In unparseThrowOp(" + expr->sage_class_name() + ") */ \n";

  // DQ (9/19/2004): Added support for different types of throw expressions!
  switch (throw_op->get_throwKind()) {
  case SgThrowOp::unknown_throw: {
    printf("Error: case of SgThrowOp::unknown_throw in unparseThrowOp() \n");
    ROSE_ABORT();
  }

  case SgThrowOp::throw_expression: {
    curprint("throw ");
    ASSERT_not_null(throw_op->get_operand());
    unparseExpression(throw_op->get_operand(), info);
    break;
  }

  case SgThrowOp::rethrow: {
    curprint("throw");
    break;
  }

  default:
    printf("Error: default reached in unparseThrowOp() \n");
    ROSE_ABORT();
  }
}

void Unparse_ExprStmt::unparseVarArgStartOp(SgExpression *expr,
                                            SgUnparse_Info &info) {
  // printf ("Inside of Unparse_ExprStmt::unparseVarArgStartOp \n");

  SgVarArgStartOp *varArgStart = isSgVarArgStartOp(expr);
  ASSERT_not_null(varArgStart);
  SgExpression *lhsOperand = varArgStart->get_lhs_operand();
  SgExpression *rhsOperand = varArgStart->get_rhs_operand();

  ASSERT_not_null(lhsOperand);
  ASSERT_not_null(rhsOperand);

  // DQ (9/16/2013): This was a problem pointed out by Phil Miller, it only has
  // to be correct to make the resulting code link properly. curprint (
  // "va_start(");
  curprint("__builtin_va_start(");
  unparseExpression(lhsOperand, info);
  curprint(",");
  unparseExpression(rhsOperand, info);
  curprint(")");
}

void Unparse_ExprStmt::unparseVarArgStartOneOperandOp(SgExpression *expr,
                                                      SgUnparse_Info &info) {
  // printf ("Inside of Unparse_ExprStmt::unparseVarArgStartOneOperandOp \n");

  SgVarArgStartOneOperandOp *varArgStart = isSgVarArgStartOneOperandOp(expr);
  ASSERT_not_null(varArgStart);
  SgExpression *operand = varArgStart->get_operand_expr();
  ASSERT_not_null(operand);

  // DQ (9/16/2013): This was a problem pointed out by Phil Miller, it only has
  // to be correct to make the resulting code link properly. curprint (
  // "va_start(");
  curprint("__builtin_va_start(");
  unparseExpression(operand, info);
  curprint(")");
}

void Unparse_ExprStmt::unparseVarArgOp(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgVarArgOp *varArg = isSgVarArgOp(expr);
  ASSERT_not_null(varArg);

  SgExpression *operand = varArg->get_operand_expr();
  SgType *type = varArg->get_type();

  ASSERT_not_null(operand);
  ASSERT_not_null(type);

  // DQ (1/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  curprint("__builtin_va_arg(");
  unparseExpression(operand, info);
  curprint(",");
  SgUnparse_Info typeInfo(info);
  typeInfo.set_reference_node_for_qualification(varArg);
  applyTypeReferenceInfoFromExpression(unp, varArg, typeInfo);
  unp->u_type->unparseType(type, typeInfo);
  curprint(")");
}

void Unparse_ExprStmt::unparseVarArgEndOp(SgExpression *expr,
                                          SgUnparse_Info &info) {
  SgVarArgEndOp *varArgEnd = isSgVarArgEndOp(expr);
  ASSERT_not_null(varArgEnd);
  SgExpression *operand = varArgEnd->get_operand_expr();
  ASSERT_not_null(operand);

  // DQ (9/16/2013): This was a problem pointed out by Phil Miller, it only has
  // to be correct to make the resulting code link properly.
  // curprint("va_end(");
  curprint("__builtin_va_end(");
  unparseExpression(operand, info);
  curprint(")");
}

void Unparse_ExprStmt::unparseVarArgCopyOp(SgExpression *expr,
                                           SgUnparse_Info &info) {
  SgVarArgCopyOp *varArgCopy = isSgVarArgCopyOp(expr);

  SgExpression *lhsOperand = varArgCopy->get_lhs_operand();
  SgExpression *rhsOperand = varArgCopy->get_rhs_operand();

  ASSERT_not_null(lhsOperand);
  ASSERT_not_null(rhsOperand);

  // DQ (9/16/2013): This was a problem pointed out by Phil Miller, it only has
  // to be correct to make the resulting code link properly.
  // curprint("va_copy(");
  curprint("__builtin_va_copy(");
  unparseExpression(lhsOperand, info);
  curprint(",");
  unparseExpression(rhsOperand, info);
  curprint(")");
}

void Unparse_ExprStmt::unparsePseudoDtorRef(SgExpression *expr,
                                            SgUnparse_Info &info) {
  SgPseudoDestructorRefExp *pdre = isSgPseudoDestructorRefExp(expr);
  ASSERT_not_null(pdre);

  SgType *objt = pdre->get_object_type();
  SgNamedType *namedType = isSgNamedType(objt);
  bool append_call_parens = true;
  if (isSgFunctionCallExp(pdre->get_parent()) != NULL) {
    append_call_parens = false;
  } else if (SgBinaryOp *bin_op = isSgBinaryOp(pdre->get_parent())) {
    append_call_parens = (isSgFunctionCallExp(bin_op->get_parent()) == NULL);
  }
  if (namedType != NULL) {

    // DQ (1/18/2020): Adding support for name qualification (see
    // Cxx11_tests/test2020_56.C).
    SgName nameQualifier(exactNameQualification(unp, pdre, info).qualifier);
    if (nameQualifier.is_null() == false) {
      SgName nameOfType = namedType->get_name();
      SgName name = nameQualifier + nameOfType + "::";
      curprint(name.str());
    }
    curprint("~");
    curprint(namedType->get_name().str());

    // DQ (3/14/2012): Note that I had to add this for older frontends;
    // something in ROSE has likely changed.
    if (append_call_parens == true) {
      curprint("()");
    }
  } else {
    curprint("~");

    // DQ (3/14/2012): This is the case of of a primative type (e.g. "~int"),
    // which is allowed. PC: I do not think this case will ever occur in
    // practice.  If it does, the resulting code will be invalid.  It may,
    // however, appear in an implicit template instantiation.
    unp->u_type->unparseType(objt, info);
  }
}

void Unparse_ExprStmt::unparseCudaKernelCall(SgExpression *expr,
                                             SgUnparse_Info &info) {

  SgCudaKernelCallExp *kernel_call = isSgCudaKernelCallExp(expr);
  ASSERT_not_null(kernel_call);

  unparseExpression(kernel_call->get_function(), info);

  SgCudaKernelExecConfig *exec_config =
      isSgCudaKernelExecConfig(kernel_call->get_exec_config());
  ASSERT_not_null(exec_config);

  curprint("<<<");

  SgExpression *grid_exp = exec_config->get_grid();
  ASSERT_not_null(grid_exp);
  unparseExpression(grid_exp, info);
  curprint(",");

  SgExpression *blocks_exp = exec_config->get_blocks();
  ASSERT_not_null(blocks_exp);
  unparseExpression(blocks_exp, info);

  SgExpression *shared_exp = exec_config->get_shared();
  if (shared_exp != NULL) {
    curprint(",");
    unparseExpression(shared_exp, info);

    SgExpression *stream_exp = exec_config->get_stream();
    if (stream_exp != NULL) {
      curprint(",");
      unparseExpression(stream_exp, info);
    }
  }

  curprint(">>>");

  curprint("(");
  if (kernel_call->get_args() != NULL) {
    SgExpressionPtrList &list = kernel_call->get_args()->get_expressions();
    SgExpressionPtrList::iterator arg = list.begin();
    while (arg != list.end()) {
      unparseExpression((*arg), info);
      arg++;
      if (arg != list.end())
        curprint(",");
    }
  }
  curprint(")");
}
