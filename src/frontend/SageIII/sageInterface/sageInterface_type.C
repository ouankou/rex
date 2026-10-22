#include "sage3basic.h"

#include "integerWidth.h"
#include "sageInterface.h"

#include <iostream>
#include <array>

#include <memory>

#include <cstdint>
#include <limits>
#include <optional>

#include <unordered_set>
#include <vector>

using namespace std;


// originally from src/midend/astInlining/typeTraits.C
// src/midend/astUtil/astInterface/AstInterface.C
namespace SageInterface {

namespace {
SgType *resolveNonrealType(SgNonrealType *type) {
  ROSE_ASSERT(type != nullptr);
  SgNonrealDecl *source = isSgNonrealDecl(type->get_declaration());
  ROSE_ASSERT(source != nullptr);
  SgDeclarationStatement *target = source->get_templateDeclaration();
  if (target == nullptr || isSgTemplateClassDeclaration(target) ||
      isSgTemplateTypedefDeclaration(target)) {
    return nullptr;
  }
  SgType *resolved = nullptr;
  if (SgClassDeclaration *decl = isSgClassDeclaration(target)) {
    resolved = decl->get_type();
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(target)) {
    resolved = decl->get_type();
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(target)) {
    resolved = decl->get_type();
  } else {
    return nullptr;
  }
  ROSE_ASSERT(resolved != nullptr && resolved != type);
  return resolved;
}

std::array<const SgDeclarationStatementPtrList *, 2>
classMemberDeclarationLists(SgClassDefinition *definition) {
  ROSE_ASSERT(definition != nullptr);
  auto *auxiliary = definition->get_auxiliary_declarations();
  if (auxiliary != nullptr) {
    ROSE_ASSERT(auxiliary->get_parent() == definition);
    auxiliary->validate_semantic_non_output_role();
  }
  return {&definition->get_members(),
          auxiliary != nullptr ? &auxiliary->get_declarations() : nullptr};
}

} // namespace

namespace {
enum class FortranIntrinsicCategory {
  none,
  integer,
  unsigned_integer,
  real,
  complex,
  logical,
  character,
};

const SgType *stripFortranSelectorModifiers(const SgType *type) {
  while (const SgModifierType *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return type;
}

FortranIntrinsicCategory fortranIntrinsicCategory(const SgType *type) {
  type = stripFortranSelectorModifiers(type);
  if (isSgTypeInt(type) != nullptr || isSgTypeSignedInt(type) != nullptr) {
    return FortranIntrinsicCategory::integer;
  }
  if (isSgTypeUnsignedInt(type) != nullptr) {
    return FortranIntrinsicCategory::unsigned_integer;
  }
  if (isSgTypeFloat(type) != nullptr || isSgTypeDouble(type) != nullptr) {
    return FortranIntrinsicCategory::real;
  }
  if (isSgTypeComplex(type) != nullptr) {
    return FortranIntrinsicCategory::complex;
  }
  if (isSgTypeBool(type) != nullptr) {
    return FortranIntrinsicCategory::logical;
  }
  if (isSgTypeChar(type) != nullptr || isSgTypeString(type) != nullptr) {
    return FortranIntrinsicCategory::character;
  }
  return FortranIntrinsicCategory::none;
}

template <typename Unsigned>
std::optional<std::int64_t> signedIntegerValue(Unsigned value) {
  if (!Detail::unsignedValueFitsIn<std::int64_t>(value)) {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(value);
}

std::optional<std::int64_t>
exactFortranIntegerLiteral(const SgExpression *expression) {
  if (const SgIntVal *value = isSgIntVal(expression)) {
    return static_cast<std::int64_t>(value->get_value());
  }
  if (const SgLongIntVal *value = isSgLongIntVal(expression)) {
    return static_cast<std::int64_t>(value->get_value());
  }
  if (const SgLongLongIntVal *value = isSgLongLongIntVal(expression)) {
    return static_cast<std::int64_t>(value->get_value());
  }
  if (const SgShortVal *value = isSgShortVal(expression)) {
    return static_cast<std::int64_t>(value->get_value());
  }
  if (const SgUnsignedIntVal *value = isSgUnsignedIntVal(expression)) {
    return signedIntegerValue(value->get_value());
  }
  if (const SgUnsignedLongVal *value = isSgUnsignedLongVal(expression)) {
    return signedIntegerValue(value->get_value());
  }
  if (const SgUnsignedLongLongIntVal *value =
          isSgUnsignedLongLongIntVal(expression)) {
    return signedIntegerValue(value->get_value());
  }
  if (const SgUnsignedShortVal *value = isSgUnsignedShortVal(expression)) {
    return signedIntegerValue(value->get_value());
  }
  return std::nullopt;
}

struct FortranProcedureLocalVariableIdentity {
  const SgFunctionDeclaration *canonical = nullptr;
  SgName name;
  SgType *type = nullptr;
};

const SgFunctionDeclaration *
exactEnclosingFortranProcedure(const SgScopeStatement *scope) {
  const SgFunctionDeclaration *procedure = nullptr;
  if (const SgFunctionParameterScope *parameters =
          isSgFunctionParameterScope(scope)) {
    procedure = isSgFunctionDeclaration(parameters->get_parent());
  } else if (const SgBasicBlock *body = isSgBasicBlock(scope)) {
    const SgFunctionDefinition *definition =
        isSgFunctionDefinition(body->get_parent());
    procedure = definition != nullptr ? definition->get_declaration() : nullptr;
    if (procedure == nullptr && definition != nullptr) {
      SgScopeStatement *owner =
          definition->get_construction_physical_output_owner();
      const SgName &name = definition->get_fortran_construction_name();
      SgFunctionSymbol *symbol = owner != nullptr && !name.is_null()
                                     ? owner->lookup_function_symbol(name)
                                     : nullptr;
      const SgProcedureHeaderStatement *canonical =
          symbol != nullptr
              ? isSgProcedureHeaderStatement(symbol->get_declaration())
              : nullptr;
      if (canonical == nullptr || canonical->get_scope() != owner ||
          canonical->get_firstNondefiningDeclaration() != canonical ||
          canonical->get_definition() != nullptr ||
          canonical->get_fortran_procedure_source_form() !=
              SgProcedureHeaderStatement::
                  e_fortran_procedure_source_form_semantic_only) {
        return nullptr;
      }
      procedure = canonical;
    }
  }
  if (isSgProcedureHeaderStatement(procedure) == nullptr) {
    return nullptr;
  }
  const SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(procedure->get_firstNondefiningDeclaration());
  return canonical != nullptr &&
                 canonical->get_firstNondefiningDeclaration() == canonical
             ? canonical
             : nullptr;
}

bool sameFortranIdentifier(const SgName &left, const SgName &right) {
  std::string leftName = left.getString();
  std::string rightName = right.getString();
  std::transform(leftName.begin(), leftName.end(), leftName.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  std::transform(rightName.begin(), rightName.end(), rightName.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return leftName == rightName;
}

std::optional<FortranProcedureLocalVariableIdentity>
exactFortranProcedureLocalVariableIdentity(const SgVariableSymbol *symbol) {
  const SgInitializedName *declaration =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  const SgScopeStatement *scope =
      declaration != nullptr ? declaration->get_scope() : nullptr;
  const SgFunctionDeclaration *canonical =
      exactEnclosingFortranProcedure(scope);
  const SgVariableDeclaration *owner =
      declaration != nullptr
          ? isSgVariableDeclaration(declaration->get_parent())
          : nullptr;
  if (declaration == nullptr || scope == nullptr || canonical == nullptr ||
      symbol->get_scope() != scope || declaration->get_type() == nullptr ||
      owner == nullptr ||
      std::find(owner->get_variables().begin(), owner->get_variables().end(),
                declaration) == owner->get_variables().end()) {
    return std::nullopt;
  }
  return FortranProcedureLocalVariableIdentity{
      canonical, declaration->get_name(), declaration->get_type()};
}

bool exactFortranProcedureLocalVariableIdentity(const SgVariableSymbol *left,
                                                const SgVariableSymbol *right) {
  const std::optional<FortranProcedureLocalVariableIdentity> leftIdentity =
      exactFortranProcedureLocalVariableIdentity(left);
  const std::optional<FortranProcedureLocalVariableIdentity> rightIdentity =
      exactFortranProcedureLocalVariableIdentity(right);
  return leftIdentity && rightIdentity &&
         leftIdentity->canonical == rightIdentity->canonical &&
         leftIdentity->type == rightIdentity->type &&
         sameFortranIdentifier(leftIdentity->name, rightIdentity->name);
}

bool exactFortranSemanticExpressionTypeIdentity(const SgType *left,
                                                const SgType *right);

bool exactFortranProcedureLocalFunctionIdentity(const SgFunctionSymbol *left,
                                                const SgFunctionSymbol *right) {
  auto identity = [](const SgFunctionSymbol *symbol) {
    const SgFunctionDeclaration *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    const SgScopeStatement *scope =
        symbol != nullptr ? symbol->get_scope() : nullptr;
    const SgProcedureHeaderStatement *fortranDeclaration =
        isSgProcedureHeaderStatement(declaration);
    const SgFunctionDeclaration *context =
        exactEnclosingFortranProcedure(scope);
    const SgFunctionDeclaration *canonical =
        declaration != nullptr
            ? isSgFunctionDeclaration(
                  declaration->get_firstNondefiningDeclaration())
            : nullptr;
    const auto sourceForm =
        fortranDeclaration != nullptr
            ? fortranDeclaration->get_fortran_procedure_source_form()
            : SgProcedureHeaderStatement::
                  e_fortran_procedure_source_form_unknown;
    const SgAuxiliaryDeclarationList *auxiliary =
        declaration != nullptr
            ? isSgAuxiliaryDeclarationList(declaration->get_parent())
            : nullptr;
    const SgInterfaceBody *interfaceBody =
        declaration != nullptr ? isSgInterfaceBody(declaration->get_parent())
                               : nullptr;
    const SgInterfaceStatement *interfaceStatement =
        interfaceBody != nullptr
            ? isSgInterfaceStatement(interfaceBody->get_parent())
            : nullptr;
    const bool exactSemanticOnlyOwner =
        sourceForm == SgProcedureHeaderStatement::
                          e_fortran_procedure_source_form_semantic_only &&
        auxiliary != nullptr && auxiliary->get_parent() == scope &&
        scope != nullptr && scope->get_auxiliary_declarations() == auxiliary;
    const bool exactInterfaceOwner =
        sourceForm == SgProcedureHeaderStatement::
                          e_fortran_procedure_source_form_header &&
        interfaceBody != nullptr && interfaceStatement != nullptr &&
        interfaceBody->get_functionDeclaration() == declaration &&
        interfaceStatement->get_parent() == scope;
    if (declaration == nullptr || scope == nullptr ||
        fortranDeclaration == nullptr || context == nullptr ||
        declaration->get_scope() != scope ||
        declaration->get_type() == nullptr || canonical != declaration ||
        symbol->get_scope() != scope ||
        (!exactSemanticOnlyOwner && !exactInterfaceOwner)) {
      return std::tuple<
          const SgFunctionDeclaration *, SgName, SgFunctionType *,
          SgProcedureHeaderStatement::fortran_procedure_source_form_enum>{
          nullptr, SgName(), nullptr,
          SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown};
    }
    return std::tuple<
        const SgFunctionDeclaration *, SgName, SgFunctionType *,
        SgProcedureHeaderStatement::fortran_procedure_source_form_enum>{
        context, declaration->get_name(), declaration->get_type(), sourceForm};
  };

  const auto [leftContext, leftName, leftType, leftForm] = identity(left);
  const auto [rightContext, rightName, rightType, rightForm] = identity(right);
  return leftContext != nullptr && leftContext == rightContext &&
         leftType != nullptr &&
         exactFortranSemanticExpressionTypeIdentity(leftType, rightType) &&
         (leftForm == SgProcedureHeaderStatement::
                          e_fortran_procedure_source_form_semantic_only ||
          rightForm == SgProcedureHeaderStatement::
                           e_fortran_procedure_source_form_semantic_only) &&
         sameFortranIdentifier(leftName, rightName);
}

bool exactFortranSemanticExpressionTypeIdentity(const SgType *left,
                                                const SgType *right) {
  if (left == nullptr || right == nullptr) {
    return false;
  }
  if (left == right) {
    return true;
  }

  const SgFunctionType *leftFunction = isSgFunctionType(left);
  const SgFunctionType *rightFunction = isSgFunctionType(right);
  if (leftFunction != nullptr || rightFunction != nullptr) {
    if (leftFunction == nullptr || rightFunction == nullptr ||
        leftFunction->get_fortran_source_syntax() ||
        rightFunction->get_fortran_source_syntax() ||
        leftFunction->get_has_ellipses() != rightFunction->get_has_ellipses() ||
        leftFunction->get_has_prototype() !=
            rightFunction->get_has_prototype() ||
        leftFunction->get_argument_list() == nullptr ||
        rightFunction->get_argument_list() == nullptr ||
        leftFunction->get_argument_list()->get_parent() != leftFunction ||
        rightFunction->get_argument_list()->get_parent() != rightFunction ||
        leftFunction->get_arguments().size() !=
            rightFunction->get_arguments().size() ||
        !exactFortranSemanticExpressionTypeIdentity(
            leftFunction->get_return_type(),
            rightFunction->get_return_type())) {
      return false;
    }
    for (std::size_t index = 0; index < leftFunction->get_arguments().size();
         ++index) {
      if (!exactFortranSemanticExpressionTypeIdentity(
              leftFunction->get_arguments()[index],
              rightFunction->get_arguments()[index])) {
        return false;
      }
    }
    return true;
  }

  // Ranked expression results deliberately own fresh shape-only array types:
  // their dimension lists are AST children and therefore cannot be shared by
  // a global canonical type.  Compare only that exact semantic schema here;
  // declaration arrays, source-syntax arrays, explicit bounds, and coarrays
  // remain pointer-distinct and do not match through this path.
  const SgArrayType *leftArray = isSgArrayType(left);
  const SgArrayType *rightArray = isSgArrayType(right);
  if (leftArray == nullptr || rightArray == nullptr ||
      leftArray->get_fortran_source_syntax() ||
      rightArray->get_fortran_source_syntax() || leftArray->get_isCoArray() ||
      rightArray->get_isCoArray() || leftArray->get_rank() <= 0 ||
      leftArray->get_rank() != rightArray->get_rank() ||
      leftArray->get_is_variable_length_array() !=
          rightArray->get_is_variable_length_array() ||
      leftArray->get_index() != nullptr || rightArray->get_index() != nullptr) {
    return false;
  }

  const SgExprListExp *leftDimensions = leftArray->get_dim_info();
  const SgExprListExp *rightDimensions = rightArray->get_dim_info();
  if (leftDimensions == nullptr || rightDimensions == nullptr ||
      leftDimensions->get_parent() != leftArray ||
      rightDimensions->get_parent() != rightArray ||
      leftDimensions->get_expressions().size() !=
          static_cast<std::size_t>(leftArray->get_rank()) ||
      rightDimensions->get_expressions().size() !=
          static_cast<std::size_t>(rightArray->get_rank())) {
    return false;
  }
  for (const SgExprListExp *dimensions : {leftDimensions, rightDimensions}) {
    for (const SgExpression *dimension : dimensions->get_expressions()) {
      if (isSgColonShapeExp(dimension) == nullptr ||
          dimension->get_parent() != dimensions ||
          dimension->get_fortran_integer_constant_value_is_available() ||
          dimension->get_fortran_integer_constant_value() != 0) {
        return false;
      }
    }
  }
  return exactFortranSemanticExpressionTypeIdentity(
      leftArray->get_base_type(), rightArray->get_base_type());
}

bool exactFortranDynamicLengthExpression(const SgExpression *source,
                                         const SgExpression *semantic) {
  if (source == nullptr || semantic == nullptr ||
      source->variantT() != semantic->variantT() ||
      source->get_fortran_integer_constant_value_is_available() !=
          semantic->get_fortran_integer_constant_value_is_available() ||
      source->get_fortran_integer_constant_value() !=
          semantic->get_fortran_integer_constant_value()) {
    return false;
  }
  const bool sourceHasSemanticType = source->has_semantic_value_type();
  const bool semanticHasSemanticType = semantic->has_semantic_value_type();
  if (sourceHasSemanticType != semanticHasSemanticType) {
    return false;
  }
  if (sourceHasSemanticType) {
    SgType *sourceType = source->get_type();
    SgType *semanticType = semantic->get_type();
    if (sourceType == nullptr || semanticType == nullptr ||
        !exactFortranSemanticExpressionTypeIdentity(sourceType, semanticType)) {
      return false;
    }
  }

  const std::optional<std::int64_t> sourceLiteral =
      exactFortranIntegerLiteral(source);
  const std::optional<std::int64_t> semanticLiteral =
      exactFortranIntegerLiteral(semantic);
  if (sourceLiteral || semanticLiteral) {
    return sourceLiteral && semanticLiteral &&
           *sourceLiteral == *semanticLiteral;
  }
  if (const SgStringVal *sourceString = isSgStringVal(source)) {
    const SgStringVal *semanticString = isSgStringVal(semantic);
    return semanticString != nullptr &&
           sourceString->get_value() == semanticString->get_value() &&
           sourceString->get_literal_encoding() ==
               semanticString->get_literal_encoding() &&
           sourceString->get_cxx_unevaluated() ==
               semanticString->get_cxx_unevaluated() &&
           sourceString->get_stringDelimiter() ==
               semanticString->get_stringDelimiter() &&
           sourceString->get_isRawString() ==
               semanticString->get_isRawString() &&
           sourceString->get_raw_string_delimiter() ==
               semanticString->get_raw_string_delimiter() &&
           sourceString->get_raw_string_payload() ==
               semanticString->get_raw_string_payload();
  }
  if (const SgVarRefExp *sourceReference = isSgVarRefExp(source)) {
    const SgVarRefExp *semanticReference = isSgVarRefExp(semantic);
    return semanticReference != nullptr &&
           sourceReference->get_symbol() != nullptr &&
           (sourceReference->get_symbol() == semanticReference->get_symbol() ||
            exactFortranProcedureLocalVariableIdentity(
                sourceReference->get_symbol(),
                semanticReference->get_symbol()));
  }
  if (const SgFunctionRefExp *sourceReference = isSgFunctionRefExp(source)) {
    const SgFunctionRefExp *semanticReference = isSgFunctionRefExp(semantic);
    return semanticReference != nullptr &&
           sourceReference->get_symbol() != nullptr &&
           (sourceReference->get_symbol() == semanticReference->get_symbol() ||
            exactFortranProcedureLocalFunctionIdentity(
                sourceReference->get_symbol(),
                semanticReference->get_symbol()));
  }
  if (const SgMemberFunctionRefExp *sourceReference =
          isSgMemberFunctionRefExp(source)) {
    const SgMemberFunctionRefExp *semanticReference =
        isSgMemberFunctionRefExp(semantic);
    return semanticReference != nullptr &&
           sourceReference->get_symbol() != nullptr &&
           sourceReference->get_symbol() == semanticReference->get_symbol();
  }
  if (const SgActualArgumentExpression *sourceArgument =
          isSgActualArgumentExpression(source)) {
    const SgActualArgumentExpression *semanticArgument =
        isSgActualArgumentExpression(semantic);
    if (semanticArgument == nullptr ||
        sourceArgument->get_argument_name() !=
            semanticArgument->get_argument_name()) {
      return false;
    }
  }

  const std::vector<SgNode *> sourceChildren =
      const_cast<SgExpression *>(source)->get_traversalSuccessorContainer();
  const std::vector<SgNode *> semanticChildren =
      const_cast<SgExpression *>(semantic)->get_traversalSuccessorContainer();
  if (sourceChildren.empty() || semanticChildren.empty()) {
    const SgExprListExp *sourceList = isSgExprListExp(source);
    const SgExprListExp *semanticList = isSgExprListExp(semantic);
    return sourceChildren.empty() && semanticChildren.empty() &&
           sourceList != nullptr && semanticList != nullptr &&
           sourceList->get_expressions().empty() &&
           semanticList->get_expressions().empty() &&
           sourceList->get_parent() != nullptr &&
           semanticList->get_parent() != nullptr;
  }
  if (sourceChildren.size() != semanticChildren.size()) {
    return false;
  }
  for (std::size_t index = 0; index < sourceChildren.size(); ++index) {
    const SgExpression *sourceChild = isSgExpression(sourceChildren[index]);
    const SgExpression *semanticChild = isSgExpression(semanticChildren[index]);
    if ((sourceChild == nullptr) != (semanticChild == nullptr) ||
        (sourceChild != nullptr &&
         !exactFortranDynamicLengthExpression(sourceChild, semanticChild))) {
      return false;
    }
  }
  return true;
}

bool sourceSelectorMetadataIsExact(const SgExpression *expression) {
  if (expression == nullptr ||
      !expression->get_fortran_integer_constant_value_is_available()) {
    return false;
  }
  const std::int64_t folded = expression->get_fortran_integer_constant_value();
  if (isSgValueExp(expression) != nullptr) {
    const std::optional<std::int64_t> literal =
        exactFortranIntegerLiteral(expression);
    return literal && *literal == folded;
  }
  return true;
}

bool selectorHasNoFoldedMetadata(const SgExpression *expression) {
  return expression != nullptr &&
         !expression->get_fortran_integer_constant_value_is_available() &&
         expression->get_fortran_integer_constant_value() == 0;
}

bool typeHasNoFixedFortranKindMetadata(const SgType *type) {
  return type != nullptr &&
         !type->get_fortran_fixed_kind_value_is_available() &&
         type->get_fortran_fixed_kind_value() == 0;
}

bool isFixedDoubleSourceType(const SgType *type) {
  type = stripFortranSelectorModifiers(type);
  if (isSgTypeDouble(type) != nullptr) {
    return true;
  }
  const SgTypeComplex *complex = isSgTypeComplex(type);
  return complex != nullptr && isSgTypeDouble(stripFortranSelectorModifiers(
                                   complex->get_base_type())) != nullptr;
}

bool fortranKindMatches(const SgType *source, const SgType *semantic,
                        FortranIntrinsicCategory category) {
  source = stripFortranSelectorModifiers(source);
  semantic = stripFortranSelectorModifiers(semantic);
  if (source == nullptr || semantic == nullptr ||
      semantic->get_hasTypeKindStar() ||
      !typeHasNoFixedFortranKindMetadata(semantic)) {
    return false;
  }

  SgExpression *semanticKind = semantic->get_type_kind();
  if (semanticKind == nullptr) {
    if (const SgTypeComplex *complex = isSgTypeComplex(semantic)) {
      const SgType *component =
          stripFortranSelectorModifiers(complex->get_base_type());
      semanticKind =
          component != nullptr ? component->get_type_kind() : nullptr;
    }
  }
  SgExpression *sourceKind = source->get_type_kind();
  if (semanticKind == nullptr) {
    return sourceKind == nullptr && !source->get_hasTypeKindStar() &&
           typeHasNoFixedFortranKindMetadata(source);
  }
  const std::optional<std::int64_t> semanticValue =
      exactFortranIntegerLiteral(semanticKind);
  if (!semanticValue || *semanticValue <= 0 ||
      !selectorHasNoFoldedMetadata(semanticKind)) {
    return false;
  }

  const bool fixedDouble = isFixedDoubleSourceType(source);
  if (fixedDouble) {
    return sourceKind == nullptr && !source->get_hasTypeKindStar() &&
           source->get_fortran_fixed_kind_value_is_available() &&
           source->get_fortran_fixed_kind_value() > 0 &&
           source->get_fortran_fixed_kind_value() == *semanticValue;
  }
  if (!typeHasNoFixedFortranKindMetadata(source)) {
    return false;
  }
  if (sourceKind == nullptr) {
    return !source->get_hasTypeKindStar();
  }
  if (!sourceSelectorMetadataIsExact(sourceKind)) {
    return false;
  }

  std::int64_t resolvedSourceValue =
      sourceKind->get_fortran_integer_constant_value();
  if (source->get_hasTypeKindStar()) {
    if (resolvedSourceValue <= 0) {
      return false;
    }
    // Legacy COMPLEX*n spells the total complex storage size, while Flang's
    // semantic KIND is the size of one real component.
    if (category == FortranIntrinsicCategory::complex) {
      if ((resolvedSourceValue % 2) != 0) {
        return false;
      }
      resolvedSourceValue /= 2;
    }
  } else if (resolvedSourceValue <= 0) {
    return false;
  }
  return resolvedSourceValue == *semanticValue;
}

bool fortranCharacterLengthMatches(const SgType *source, const SgType *semantic,
                                   bool allowDynamicExpressionResult) {
  source = stripFortranSelectorModifiers(source);
  semantic = stripFortranSelectorModifiers(semantic);
  const SgTypeString *semanticString = isSgTypeString(semantic);
  if (semanticString == nullptr ||
      semanticString->get_fortran_dynamic_length_pending()) {
    return false;
  }
  if (semanticString->get_fortran_dynamic_result_length()) {
    const SgTypeString *sourceString = isSgTypeString(source);
    SgExpression *sourceLength = sourceString != nullptr
                                     ? sourceString->get_lengthExpression()
                                     : nullptr;
    return allowDynamicExpressionResult &&
           !semanticString->get_fortran_source_syntax() &&
           semanticString->get_lengthExpression() == nullptr &&
           sourceString != nullptr &&
           sourceString->get_fortran_source_syntax() &&
           !sourceString->get_fortran_dynamic_length_pending() &&
           !sourceString->get_fortran_dynamic_result_length() &&
           sourceLength != nullptr &&
           sourceLength->get_parent() == sourceString &&
           isSgValueExp(sourceLength) == nullptr &&
           isSgAsteriskShapeExp(sourceLength) == nullptr &&
           isSgColonShapeExp(sourceLength) == nullptr &&
           !sourceLength->get_fortran_integer_constant_value_is_available() &&
           sourceLength->get_fortran_integer_constant_value() == 0;
  }
  SgExpression *semanticLength = semanticString->get_lengthExpression();
  if (semanticLength == nullptr) {
    return false;
  }

  SgExpression *sourceLength = nullptr;
  if (const SgTypeString *sourceString = isSgTypeString(source)) {
    sourceLength = sourceString->get_lengthExpression();
  } else if (isSgTypeChar(source) == nullptr) {
    return false;
  }

  if (sourceLength == nullptr) {
    const std::optional<std::int64_t> semanticValue =
        exactFortranIntegerLiteral(semanticLength);
    return semanticValue && *semanticValue == 1 &&
           selectorHasNoFoldedMetadata(semanticLength);
  }
  if (isSgAsteriskShapeExp(sourceLength) != nullptr ||
      isSgColonShapeExp(sourceLength) != nullptr) {
    return selectorHasNoFoldedMetadata(sourceLength) &&
           selectorHasNoFoldedMetadata(semanticLength) &&
           sourceLength->variantT() == semanticLength->variantT();
  }
  const std::optional<std::int64_t> semanticValue =
      exactFortranIntegerLiteral(semanticLength);
  if (sourceLength->get_fortran_integer_constant_value_is_available()) {
    return sourceSelectorMetadataIsExact(sourceLength) && semanticValue &&
           *semanticValue >= 0 && selectorHasNoFoldedMetadata(semanticLength) &&
           sourceLength->get_fortran_integer_constant_value() == *semanticValue;
  }
  return sourceLength->get_fortran_integer_constant_value() == 0 &&
         !semanticValue && selectorHasNoFoldedMetadata(semanticLength) &&
         exactFortranDynamicLengthExpression(sourceLength, semanticLength);
}

bool fortranSourceTypeMatchesSemanticTypeImpl(
    const SgType *source, const SgType *semantic,
    bool allowDynamicExpressionResult) {
  if (source == nullptr || semantic == nullptr) {
    return false;
  }

  const SgModifierType *sourceModifier = isSgModifierType(source);
  const SgModifierType *semanticModifier = isSgModifierType(semantic);
  if (sourceModifier != nullptr || semanticModifier != nullptr) {
    // PARAMETER is a declaration attribute in Fortran, not part of the
    // declaration-type-spec source surface.  Its semantic object type carries
    // one exact const wrapper while the independently owned source type starts
    // at the written type-spec.  No other unmatched modifier is admissible.
    if (sourceModifier == nullptr && semanticModifier != nullptr &&
        !semanticModifier->get_fortran_source_syntax()) {
      SgTypeModifier exactParameterModifier;
      exactParameterModifier.get_constVolatileModifier().setConst();
      if (semanticModifier->get_typeModifier() == exactParameterModifier) {
        return fortranSourceTypeMatchesSemanticTypeImpl(
            source, semanticModifier->get_base_type(),
            allowDynamicExpressionResult);
      }
    }
    return sourceModifier != nullptr && semanticModifier != nullptr &&
           sourceModifier->get_fortran_source_syntax() &&
           !semanticModifier->get_fortran_source_syntax() &&
           sourceModifier->get_typeModifier() ==
               semanticModifier->get_typeModifier() &&
           fortranSourceTypeMatchesSemanticTypeImpl(
               sourceModifier->get_base_type(),
               semanticModifier->get_base_type(), allowDynamicExpressionResult);
  }

  const SgPointerType *sourcePointer = isSgPointerType(source);
  const SgPointerType *semanticPointer = isSgPointerType(semantic);
  if (sourcePointer != nullptr || semanticPointer != nullptr) {
    return sourcePointer != nullptr && semanticPointer != nullptr &&
           sourcePointer->get_fortran_source_syntax() &&
           !semanticPointer->get_fortran_source_syntax() &&
           fortranSourceTypeMatchesSemanticTypeImpl(
               sourcePointer->get_base_type(), semanticPointer->get_base_type(),
               allowDynamicExpressionResult);
  }

  const SgArrayType *sourceArray = isSgArrayType(source);
  const SgArrayType *semanticArray = isSgArrayType(semantic);
  if (sourceArray != nullptr || semanticArray != nullptr) {
    if (sourceArray == nullptr || semanticArray == nullptr ||
        !sourceArray->get_fortran_source_syntax() ||
        semanticArray->get_fortran_source_syntax() ||
        sourceArray->get_isCoArray() != semanticArray->get_isCoArray() ||
        sourceArray->get_rank() != semanticArray->get_rank() ||
        sourceArray->get_is_variable_length_array() !=
            semanticArray->get_is_variable_length_array()) {
      return false;
    }
    const SgExprListExp *sourceDimensions = sourceArray->get_dim_info();
    const SgExprListExp *semanticDimensions = semanticArray->get_dim_info();
    if (sourceDimensions == nullptr || semanticDimensions == nullptr ||
        sourceDimensions->get_parent() != sourceArray ||
        semanticDimensions->get_parent() != semanticArray ||
        sourceDimensions->get_expressions().size() != sourceArray->get_rank() ||
        semanticDimensions->get_expressions().size() !=
            semanticArray->get_rank()) {
      return false;
    }
    return fortranSourceTypeMatchesSemanticTypeImpl(
        sourceArray->get_base_type(), semanticArray->get_base_type(),
        allowDynamicExpressionResult);
  }

  const bool sourceAssumed = isSgTypeFortranAssumed(source) != nullptr;
  const bool semanticAssumed = isSgTypeFortranAssumed(semantic) != nullptr;
  const bool sourceUnlimited =
      isSgTypeFortranUnlimitedPolymorphic(source) != nullptr;
  const bool semanticUnlimited =
      isSgTypeFortranUnlimitedPolymorphic(semantic) != nullptr;
  if (sourceAssumed || semanticAssumed || sourceUnlimited ||
      semanticUnlimited) {
    return source->get_fortran_source_syntax() &&
           !semantic->get_fortran_source_syntax() &&
           sourceAssumed == semanticAssumed &&
           sourceUnlimited == semanticUnlimited;
  }

  const FortranIntrinsicCategory sourceCategory =
      fortranIntrinsicCategory(source);
  const FortranIntrinsicCategory semanticCategory =
      fortranIntrinsicCategory(semantic);
  if (sourceCategory != FortranIntrinsicCategory::none ||
      semanticCategory != FortranIntrinsicCategory::none) {
    if (sourceCategory == FortranIntrinsicCategory::none ||
        sourceCategory != semanticCategory ||
        !source->get_fortran_source_syntax() ||
        semantic->get_fortran_source_syntax() ||
        !fortranKindMatches(source, semantic, sourceCategory)) {
      return false;
    }
    return sourceCategory != FortranIntrinsicCategory::character ||
           fortranCharacterLengthMatches(source, semantic,
                                         allowDynamicExpressionResult);
  }

  // A Fortran PROCEDURE declaration stores its exact source interface on the
  // initialized name.  Its function type is therefore the canonical semantic
  // signature on both sides of the source/semantic type surface; only wrappers
  // such as POINTER are distinct source-syntax nodes.
  if (isSgFunctionType(source) != nullptr ||
      isSgFunctionType(semantic) != nullptr) {
    return source == semantic && isSgFunctionType(source) != nullptr &&
           isSgFunctionType(semantic) != nullptr;
  }

  // Derived-type syntax and semantic types deliberately share one canonical
  // declaration/type identity.  This is the only source/semantic category in
  // which pointer identity is the typed contract; no structural-equivalence
  // fallback is permitted for an unclassified type.
  return isSgClassType(source) != nullptr &&
         isSgClassType(semantic) != nullptr && source == semantic;
}
} // namespace

bool fortranSourceTypeMatchesSemanticType(const SgType *source,
                                          const SgType *semantic) {
  return fortranSourceTypeMatchesSemanticTypeImpl(source, semantic, false);
}

bool fortranSourceTypeMatchesSemanticExpressionType(const SgType *source,
                                                    const SgType *semantic) {
  return fortranSourceTypeMatchesSemanticTypeImpl(source, semantic, true);
}

bool fortranSourceFunctionResultMatchesSemanticResult(
    const SgFunctionType *source, const SgFunctionType *semantic) {
  if (source == nullptr || semantic == nullptr ||
      !source->get_fortran_source_syntax() ||
      semantic->get_fortran_source_syntax() ||
      !fortranSourceTypeMatchesSemanticType(source->get_return_type(),
                                            semantic->get_return_type()) ||
      source->get_arguments().size() != semantic->get_arguments().size()) {
    return false;
  }
  for (std::size_t index = 0; index < source->get_arguments().size(); ++index) {
    const SgType *sourceArgument = source->get_arguments()[index];
    const SgType *semanticArgument = semantic->get_arguments()[index];
    if (sourceArgument == nullptr || semanticArgument == nullptr ||
        sourceArgument != semanticArgument) {
      return false;
    }
  }
  return true;
}

std::string getTypeName(SgType *type) {
  std::string typeName;

  switch (type->variantT()) {
  case V_SgTypeComplex:
    typeName = "complex";
    break;
  case V_SgTypeImaginary:
    typeName = "imaginary";
    break;
  case V_SgTypeBool:
    typeName = "bool";
    break;
  case V_SgTypeChar:
    typeName = "char";
    break;
  case V_SgTypeVoid:
    typeName = "void";
    break;
  case V_SgTypeInt:
    typeName = "int";
    break;
  case V_SgTypeDouble:
    typeName = "double";
    break;
  case V_SgTypeFloat:
    typeName = "float";
    break;
  case V_SgTypeLong:
    typeName = "long";
    break;
  case V_SgTypeLongDouble:
    typeName = "long double";
    break;
  case V_SgTypeEllipse:
    typeName = "ellipse";
    break;
  case V_SgTypeGlobalVoid:
    typeName = "void";
    break;
  case V_SgTypeLongLong:
    typeName = "long long";
    break;
  case V_SgTypeShort:
    typeName = "short";
    break;
  case V_SgTypeSignedChar:
    typeName = "signed char";
    break;
  case V_SgTypeSignedInt:
    typeName = "signed int";
    break;
  case V_SgTypeSignedLong:
    typeName = "signed long";
    break;
  case V_SgTypeSignedShort:
    typeName = "signed short";
    break;
  case V_SgTypeString:
    typeName = "string";
    break;
  case V_SgTypeUnknown:
    typeName = "unknown";
    break;
  case V_SgTypeDependent:
    typeName = "dependent";
    break;
  case V_SgTypeUnsignedChar:
    typeName = "unsigned char";
    break;
  case V_SgTypeUnsignedInt:
    typeName = "unsigned int";
    break;
  case V_SgTypeUnsignedLong:
    typeName = "unsigned long";
    break;
  case V_SgTypeUnsignedShort:
    typeName = "unsigned short";
    break;
  case V_SgTypeUnsignedLongLong:
    typeName = "unsigned long long";
    break;
  case V_SgReferenceType: {
    ROSE_ASSERT(isSgReferenceType(type)->get_base_type() != NULL);
    typeName = getTypeName(isSgReferenceType(type)->get_base_type()) + "&";
    break;
  }
  case V_SgPointerType: {
    ROSE_ASSERT(isSgPointerType(type)->get_base_type() != NULL);
    typeName = getTypeName(isSgPointerType(type)->get_base_type()) + "*";
    break;
  }
  case V_SgModifierType: {
    ROSE_ASSERT(isSgModifierType(type)->get_base_type() != NULL);
    SgModifierType *modifier = isSgModifierType(type);
    typeName = modifier->unparseToString();
    break;
  }
  case V_SgEnumType:
  case V_SgNamedType: {
    SgNamedType *sageNamedType = isSgNamedType(type);
    ROSE_ASSERT(sageNamedType != NULL);
    typeName = sageNamedType->get_name().str();
    break;
  }
  case V_SgClassType: {
    SgClassType *sageClassType = isSgClassType(type);
    ROSE_ASSERT(sageClassType != NULL);
    typeName = sageClassType->get_name().str();
    break;
  }
  case V_SgTypedefType: {
    SgTypedefType *sageTypedefType = isSgTypedefType(type);
    ROSE_ASSERT(sageTypedefType != NULL);
    typeName = sageTypedefType->get_name().str();
    break;
  }
  case V_SgPointerMemberType: {
    SgPointerMemberType *pointerMemberType = isSgPointerMemberType(type);
    ROSE_ASSERT(pointerMemberType != NULL);
    SgClassType *classType = isSgClassType(
        pointerMemberType->get_class_type()->stripTypedefsAndModifiers());
    ROSE_ASSERT(classType != NULL);
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(classType->get_declaration());
    ROSE_ASSERT(classDeclaration != NULL);
    typeName = classDeclaration->get_name().str();
    break;
  }
  case V_SgArrayType: {
    ROSE_ASSERT(isSgArrayType(type)->get_base_type() != NULL);
    typeName = getTypeName(isSgArrayType(type)->get_base_type());
    break;
  }
  case V_SgFunctionType: {
    SgFunctionType *functionType = isSgFunctionType(type);
    ROSE_ASSERT(functionType != NULL);
    typeName = functionType->get_mangled().str();
    break;
  }
  case V_SgMemberFunctionType: {
    SgMemberFunctionType *memberFunctionType = isSgMemberFunctionType(type);
    ROSE_ASSERT(memberFunctionType != NULL);
    SgClassType *classType = isSgClassType(
        memberFunctionType->get_class_type()->stripTypedefsAndModifiers());
    ROSE_ASSERT(classType != NULL);
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(classType->get_declaration());
    ROSE_ASSERT(classDeclaration != NULL);
    typeName = classDeclaration->get_name().str();
    break;
  }
  case V_SgTypeWchar:
    typeName = "wchar";
    break;
  case V_SgTypeChar8:
    typeName = "char8_t";
    break;
  case V_SgTypeChar16:
    typeName = "char16_t";
    break;
  case V_SgTypeChar32:
    typeName = "char32_t";
    break;
  case V_SgTypeDefault:
    typeName = "default";
    break;
  case V_SgTypeTargetBuiltin: {
    SgTypeTargetBuiltin *targetBuiltin = isSgTypeTargetBuiltin(type);
    ROSE_ASSERT(targetBuiltin != nullptr);
    ROSE_ASSERT(!targetBuiltin->get_spelling().is_null());
    typeName = targetBuiltin->get_spelling().getString();
    break;
  }
  case V_SgTypeFortranAssumed:
    typeName = "type(*)";
    break;
  case V_SgTypeFortranBOZ:
    typeName = "BOZ";
    break;
  case V_SgTypeFortranUnlimitedPolymorphic:
    typeName = "class(*)";
    break;
  default: {
    printf("default reached in switch within SageInterface::getTypeName "
           "type->sage_class_name() = %s variant = %d \n",
           type->sage_class_name(), type->variant());
    ROSE_ABORT();
    break;
  }
  }

  return typeName;
}

namespace {
struct ActiveTypeCheck {
  const SgType *type;
  const ActiveTypeCheck *previous;

  ActiveTypeCheck(const SgType *type, const ActiveTypeCheck *previous)
      : type(type), previous(previous) {
    for (auto *ancestor = previous; ancestor != nullptr;
         ancestor = ancestor->previous) {
      if (ancestor->type != type)
        continue;
      fprintf(stderr,
              "REX_AST_INVARIANT[unknown-type-cycle]: type=%p has a cyclic "
              "dependency\n",
              static_cast<const void *>(type));
      ROSE_ABORT();
    }
  }
};

bool containsUnknownTypeImpl(const SgType *type,
                             const ActiveTypeCheck *active) {
  if (type == NULL)
    return true;

  if (isSgTypeUnknown(type) || isSgTypeDefault(type))
    return true;

  ActiveTypeCheck check(type, active);
  auto contains = [&](const SgType *component) {
    return containsUnknownTypeImpl(component, &check);
  };

  if (auto *mod = isSgModifierType(type))
    return contains(mod->get_base_type());
  if (auto *memPtr = isSgPointerMemberType(type))
    return contains(memPtr->get_base_type()) ||
           contains(memPtr->get_class_type());
  if (auto *ptr = isSgPointerType(type))
    return contains(ptr->get_base_type());
  if (auto *ref = isSgReferenceType(type))
    return contains(ref->get_base_type());
  if (auto *rref = isSgRvalueReferenceType(type))
    return contains(rref->get_base_type());
  if (auto *arr = isSgArrayType(type))
    return contains(arr->get_base_type());
  if (auto *complex = isSgTypeComplex(type))
    return contains(complex->get_base_type());
  if (auto *imaginary = isSgTypeImaginary(type))
    return contains(imaginary->get_base_type());
  if (auto *td = isSgTypedefType(type))
    return contains(td->get_base_type());
  if (auto *transform = isSgUnaryTransformType(type))
    return contains(transform->get_operand_type());
  if (auto *typeOf = isSgTypeOfType(type))
    return contains(typeOf->get_base_type());
  if (auto *tmpl = isSgTemplateType(type)) {
    for (const SgTemplateArgument *arg : tmpl->get_tpl_args()) {
      if (arg == nullptr ||
          arg->get_argumentType() != SgTemplateArgument::type_argument ||
          contains(arg->get_type()))
        return true;
    }
  }
  if (auto *func = isSgFunctionType(type)) {
    if (auto *member = isSgMemberFunctionType(func);
        member != nullptr && member->get_class_type() != nullptr &&
        contains(member->get_class_type()))
      return true;
    if (contains(func->get_return_type()))
      return true;
    const SgFunctionParameterTypeList *params = func->get_argument_list();
    if (params == NULL)
      return true;
    const SgTypePtrList &args = params->get_arguments();
    for (const SgType *arg : args) {
      if (contains(arg))
        return true;
    }
    return false;
  }
  if (auto *declType = isSgDeclType(type))
    return contains(declType->get_base_type());

  return false;
}
} // namespace

bool containsUnknownType(const SgType *type) {
  return containsUnknownTypeImpl(type, nullptr);
}

// by Jeremiah
// Return bool for C++ code, and int for C code
SgType *getBoolType(SgNode *n) {
  bool isC =
      SageInterface::getEnclosingSourceFile(n, false)->get_outputLanguage() ==
      SgFile::e_C_language;
  if (isC) {
    return SgTypeInt::createType();
  } else {
    return SgTypeBool::createType();
  }
}

//! Check if a type is an integral type, only allowing signed/unsigned short,
//! int, long, long long.
////!
////! There is another similar function named SgType::isIntegerType(), which
/// allows additional types char, wchar, and bool.
bool isStrictIntegerType(SgType *t) {
  ROSE_ASSERT(t != NULL);

  while (t->variant() == T_TYPEDEF) {
    t = ((SgTypedefType *)t)->get_base_type();
    ROSE_ASSERT(t != NULL);
  }

  switch (t->variant()) {
  case T_SHORT:
  case T_SIGNED_SHORT:
  case T_UNSIGNED_SHORT:
  case T_INT:
  case T_SIGNED_INT:
  case T_UNSIGNED_INT:
  case T_LONG:
  case T_SIGNED_LONG:
  case T_UNSIGNED_LONG:
  case T_LONG_LONG:
  case T_SIGNED_LONG_LONG:
  case T_UNSIGNED_LONG_LONG:
    return true;
  default:
    return false;
  }
}

bool isScalarType(SgType *t) {
  ROSE_ASSERT(t);
  t = t->stripTypedefsAndModifiers();
  if (isSgFloatingPointType(t) || isSgIntegralType(t))
    return true;
  switch (t->variantT()) {
  case V_SgTypeVoid:
  case V_SgTypeString:
  case V_SgTypeComplex:
  case V_SgTypeImaginary:
    return true;
  default:
    return false;
  }
}

bool isStructType(SgType *t) {
  bool result = false;
  ROSE_ASSERT(t != NULL);
  SgClassType *class_type = isSgClassType(t);
  if (class_type) {
    SgDeclarationStatement *decl = class_type->get_declaration();
    result = isStructDeclaration(decl);
  }
  return result;
}

bool isUnionType(SgType *t) {
  bool result = false;
  ROSE_ASSERT(t != NULL);
  SgClassType *class_type = isSgClassType(t);
  if (class_type) {
    SgDeclarationStatement *decl = class_type->get_declaration();
    result = isUnionDeclaration(decl);
  }
  return result;
}

SgType *getBaseFromType(SgType *t) {

  SgType *deref = t->dereference();
  if (deref && deref != t)
    return getBaseFromType(deref);
  return t;
}

// Is this a const type?
bool isConstType(SgType *t) {
  if (isSgTypedefType(t))
    return isConstType(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    SgTypeModifier &modifier = isSgModifierType(t)->get_typeModifier();
    SgConstVolatileModifier &cv = modifier.get_constVolatileModifier();
    if (cv.isConst())
      return true;
    else
      return isConstType(isSgModifierType(t)->get_base_type());
  }
  return false;
}

// Remove a top-level const from the type, if present
SgType *removeConst(SgType *t) {
  if (isSgTypedefType(t))
    return removeConst(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    SgModifierType *newT = deepCopy(isSgModifierType(t));
    ROSE_ASSERT(newT);
    SgTypeModifier &modifier = newT->get_typeModifier();
    SgConstVolatileModifier &cv = modifier.get_constVolatileModifier();
    cv.unsetConst();
    return newT;
  }
  return t;
}

// Is this a volatile type?
bool isVolatileType(SgType *t) {
  if (isSgTypedefType(t))
    return isVolatileType(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    SgTypeModifier &modifier = isSgModifierType(t)->get_typeModifier();
    SgConstVolatileModifier &cv = modifier.get_constVolatileModifier();
    if (cv.isVolatile())
      return true;
    else
      return isVolatileType(isSgModifierType(t)->get_base_type());
  }
  return false;
}

// Is this a restrict type?
bool isRestrictType(SgType *t) {
  if (isSgTypedefType(t))
    return isRestrictType(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    SgTypeModifier &modifier = isSgModifierType(t)->get_typeModifier();
    if (modifier.isRestrict())
      return true;
    else
      return isRestrictType(isSgModifierType(t)->get_base_type());
  }
  return false;
}

// Is this type a non-constant reference type?
bool isNonconstReference(SgType *t) {
  if (isSgReferenceType(t))
    return !isConstType(isSgReferenceType(t)->get_base_type());
  if (isSgTypedefType(t))
    return isNonconstReference(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    SgTypeModifier &modifier = isSgModifierType(t)->get_typeModifier();
    SgConstVolatileModifier &cv = modifier.get_constVolatileModifier();
    if (cv.isConst())
      return false;
    else
      return isNonconstReference(isSgModifierType(t)->get_base_type());
  }
  return false;
}

// Is this type a constant or non-constant reference type?
bool isReferenceType(SgType *t) {
  if (isSgReferenceType(t))
    return true;
  if (isSgTypedefType(t))
    return isReferenceType(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    return isReferenceType(isSgModifierType(t)->get_base_type());
  }
  return false;
}

// Is this type a pointer type?
bool isPointerType(SgType *t) {
  if (isSgPointerType(t))
    return true;
  if (isSgTypedefType(t))
    return isPointerType(isSgTypedefType(t)->get_base_type());
  if (isSgModifierType(t)) {
    return isPointerType(isSgModifierType(t)->get_base_type());
  }
  return false;
}

bool isPointerToNonConstType(SgType *type) {
  if (SgTypedefType *typeDef = isSgTypedefType(type))
    return isPointerToNonConstType(typeDef->get_base_type());
  else if (SgPointerType *pointerType = isSgPointerType(type))
    return !SageInterface::isConstType(pointerType->get_base_type());
  else if (SgModifierType *modifierType = isSgModifierType(type))
    return isPointerToNonConstType(modifierType->get_base_type());
  else
    return false;
}

//! Check if an expression is an array access (SgPntrArrRefExp) . If so, return
//! its name and subscripts if requested.
bool isArrayReference(SgExpression *ref, SgExpression **arrayName /*=NULL*/,
                      vector<SgExpression *> **subscripts /*=NULL*/) {
  SgExpression *arrayRef = NULL;
  if (ref->variantT() == V_SgPntrArrRefExp) {
    if (subscripts != 0 || arrayName != 0) {
      SgExpression *n = ref;
      while (true) {
        SgPntrArrRefExp *arr = isSgPntrArrRefExp(n);
        if (arr == 0)
          break;
        n = arr->get_lhs_operand();
        // store left hand for possible reference exp to array variable
        if (arrayName != 0)
          arrayRef = n;
        // right hand stores subscripts
        if (subscripts != 0) // must insert to be the first here !! The last
                             // visited rhs will be the first dimension!!
          (*subscripts)->insert((*subscripts)->begin(), arr->get_rhs_operand());
      } // end while
      if (arrayName != NULL) {
        *arrayName = arrayRef;
      }
    }
    return true;
  }
  return false;
}

//! Calculate the number of elements of an array type
size_t getArrayElementCount(SgArrayType *t) {
  ROSE_ASSERT(t);
  SgExpression *indexExp = t->get_index();
  ROSE_ASSERT(indexExp != nullptr && isSgNullExpression(indexExp) == nullptr);
  SgUnsignedLongVal *valExp = isSgUnsignedLongVal(indexExp);
  SgIntVal *valExpInt = isSgIntVal(indexExp);
  SgUnsignedLongLongIntVal *valExpLongLong =
      isSgUnsignedLongLongIntVal(indexExp);
  ROSE_ASSERT(valExp || valExpInt || valExpLongLong);
  size_t result;
  if (valExpInt != nullptr) {
    ROSE_ASSERT(valExpInt->get_value() >= 0);
    result = static_cast<size_t>(valExpInt->get_value());
  } else if (valExp != nullptr) {
    ROSE_ASSERT(Detail::unsignedValueFitsIn<size_t>(valExp->get_value()));
    result = static_cast<size_t>(valExp->get_value());
  } else {
    ROSE_ASSERT(
        Detail::unsignedValueFitsIn<size_t>(valExpLongLong->get_value()));
    result = static_cast<size_t>(valExpLongLong->get_value());
  }

  SgArrayType *arraybase = isSgArrayType(t->get_base_type());
  if (arraybase) {
    const size_t innerCount = getArrayElementCount(arraybase);
    ROSE_ASSERT(innerCount == 0 ||
                result <= std::numeric_limits<size_t>::max() / innerCount);
    result *= innerCount;
  }

  return result;
}

SgType *getArrayElementType(SgType *t) {
  ROSE_ASSERT(t);
  SgType *current_type =
      t->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
                   SgType::STRIP_REFERENCE_TYPE);
  while (auto *array_type = isSgArrayType(current_type))
    current_type = array_type->get_base_type()->stripTypedefsAndModifiers();
  if (current_type->variantT() == V_SgTypeString)
    return SgTypeChar::createType();
  return current_type;
}

SgType *getElementType(SgType *t) {
  ROSE_ASSERT(t);
  t = t->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
                   SgType::STRIP_REFERENCE_TYPE);
  VariantT vt = t->variantT();
  if (vt == V_SgPointerType)
    return static_cast<SgPointerType *>(t)->get_base_type();
  else if (vt == V_SgArrayType)
    return static_cast<SgArrayType *>(t)->get_base_type();
  else if (vt == V_SgTypeString)
    return SgTypeChar::createType();
  else
    return NULL;
}

//! Get the number of dimensions of an array type
int getDimensionCount(SgType *mytype) {
  ROSE_ASSERT(mytype != NULL);
  int dimension = 0;
  SgType *current_type = mytype->stripType(SgType::STRIP_MODIFIER_TYPE |
                                           SgType::STRIP_TYPEDEF_TYPE |
                                           SgType::STRIP_REFERENCE_TYPE);
  while (auto *array_type = isSgArrayType(current_type)) {
    dimension++;
    current_type = array_type->get_base_type()->stripTypedefsAndModifiers();
  }
  if (current_type->variantT() == V_SgTypeString)
    dimension++;
  return dimension;
}
bool isDefaultConstructible(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  switch (type->variantT()) {
  case V_SgArrayType:
    return isDefaultConstructible(isSgArrayType(type)->get_base_type());

  case V_SgModifierType:
    return isDefaultConstructible(isSgModifierType(type)->get_base_type());

  case V_SgClassType: {
    SgClassDeclaration *decl =
        isSgClassDeclaration(isSgClassType(type)->get_declaration());
    ROSE_ASSERT(decl != nullptr);
    decl = isSgClassDeclaration(decl->get_definingDeclaration());
    ROSE_ASSERT(decl != nullptr);
    SgClassDefinition *defn = decl->get_definition();
    ROSE_ASSERT(defn != nullptr);
    auto *source = getEnclosingSourceFile(defn);
    if (source != nullptr &&
        source->get_inputLanguage() == SgFile::e_C_language)
      return true;
    bool hasAnyConstructor = false;
    for (const auto *declarations : classMemberDeclarationLists(defn)) {
      if (declarations == nullptr)
        continue;
      for (auto *declaration : *declarations) {
        auto *fundecl = isSgMemberFunctionDeclaration(declaration);
        if (fundecl == nullptr ||
            !fundecl->get_specialFunctionModifier().isConstructor())
          continue;
        hasAnyConstructor = true;
        const auto &properties = fundecl->get_functionModifier();
        if (properties.isDefaultConstructor() && !properties.isDeleted() &&
            !properties.isIneligibleSpecialMember() &&
            fundecl->get_declarationModifier().get_accessModifier().isPublic())
          return true;
      }
    }
    ROSE_ASSERT(hasAnyConstructor);
    return false;
  }

  case V_SgTypedefType:
    return isDefaultConstructible(isSgTypedefType(type)->get_base_type());

  case V_SgNonrealType: {
    if (SgType *resolved = resolveNonrealType(isSgNonrealType(type))) {
      return isDefaultConstructible(resolved);
    }
    return false;
  }

  case V_SgTemplateType:
    return false;

  case V_SgReferenceType:
  case V_SgRvalueReferenceType:
    return false;

  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
  case V_SgTypeTargetBuiltin:

  case V_SgTypeComplex:
  case V_SgTypeImaginary:
  case V_SgEnumType:
  case V_SgPointerType:
  case V_SgPointerMemberType:
  case V_SgTypeBool:
  case V_SgTypeChar:
  case V_SgTypeDouble:
  case V_SgTypeFloat:
  case V_SgTypeInt:
  case V_SgTypeLong:
  case V_SgTypeLongDouble:
  case V_SgTypeLongLong:
  case V_SgTypeNullptr:
  case V_SgTypeShort:
  case V_SgTypeSignedChar:
  case V_SgTypeSignedInt:
  case V_SgTypeSignedLong:
  case V_SgTypeSignedShort:
  case V_SgTypeString:
  case V_SgTypeUnsignedChar:
  case V_SgTypeUnsignedInt:
  case V_SgTypeUnsignedLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeUnsignedShort:
  case V_SgTypeWchar:

  case V_SgTypeChar8:
  case V_SgTypeChar16:
  case V_SgTypeChar32:

    return true;

  case V_SgTypeVoid:
  case V_SgTypeGlobalVoid:
  case V_SgTypeFortranBOZ:
    return false;

  default:
    std::cerr << "REX_AST_INVARIANT[default-constructible-type]: unexpected "
              << type->class_name() << std::endl;
    ROSE_ABORT();
  }
}

bool isCopyConstructible(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  switch (type->variantT()) {
  case V_SgArrayType:
    return false;

  case V_SgModifierType:
    return isCopyConstructible(isSgModifierType(type)->get_base_type());

  case V_SgFunctionType:
  case V_SgMemberFunctionType:
  case V_SgTypeEllipse:
  case V_SgDeclType:
    return false;

  case V_SgClassType: {
    SgClassDeclaration *decl =
        isSgClassDeclaration(isSgClassType(type)->get_declaration());
    ROSE_ASSERT(decl != nullptr);
    SgClassDeclaration *definingDecl =
        isSgClassDeclaration(decl->get_definingDeclaration());
    ROSE_ASSERT(definingDecl != nullptr);
    SgClassDefinition *defn = definingDecl->get_definition();
    ROSE_ASSERT(defn != nullptr);
    auto *source = getEnclosingSourceFile(defn);
    if (source != nullptr &&
        source->get_inputLanguage() == SgFile::e_C_language)
      return true;
    bool hasPublicCopyConstructor = false, hasAnyCopyConstructor = false;
    for (const auto *declarations : classMemberDeclarationLists(defn)) {
      if (declarations == nullptr)
        continue;
      for (auto *declaration : *declarations) {
        const auto *fundecl = isSgFunctionDeclaration(declaration);
        if (fundecl == nullptr)
          continue;
        if (fundecl->get_functionModifier().getCopyMoveKind() ==
            SgFunctionModifier::e_copy_constructor) {
          hasAnyCopyConstructor = true;
          if (fundecl->get_declarationModifier()
                  .get_accessModifier()
                  .isPublic() &&
              !fundecl->get_functionModifier().isDeleted() &&
              !fundecl->get_functionModifier().isIneligibleSpecialMember())
            hasPublicCopyConstructor = true;
        }
        if (fundecl->get_functionModifier().isPureVirtual()) {
          return false;
        }
      }
    }

    ROSE_ASSERT(hasAnyCopyConstructor);
    return hasPublicCopyConstructor;
  }

  case V_SgTypedefType:
    return isCopyConstructible(isSgTypedefType(type)->get_base_type());

  case V_SgNonrealType: {
    if (SgType *resolved = resolveNonrealType(isSgNonrealType(type))) {
      return isCopyConstructible(resolved);
    }
    return false;
  }

  case V_SgTemplateType:
    return false;

  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
  case V_SgTypeTargetBuiltin:

  case V_SgReferenceType:
  case V_SgRvalueReferenceType:
  case V_SgTypeComplex:
  case V_SgTypeImaginary:
  case V_SgEnumType:
  case V_SgPointerType:
  case V_SgPointerMemberType:
  case V_SgTypeBool:
  case V_SgTypeChar:
  case V_SgTypeDouble:
  case V_SgTypeFloat:
  case V_SgTypeInt:
  case V_SgTypeLong:
  case V_SgTypeLongDouble:
  case V_SgTypeLongLong:
  case V_SgTypeNullptr:
  case V_SgTypeShort:
  case V_SgTypeSignedChar:
  case V_SgTypeSignedInt:
  case V_SgTypeSignedLong:
  case V_SgTypeSignedShort:
  case V_SgTypeString:
  case V_SgTypeUnsignedChar:
  case V_SgTypeUnsignedInt:
  case V_SgTypeUnsignedLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeUnsignedShort:
  case V_SgTypeWchar:

  case V_SgTypeChar8:
  case V_SgTypeChar16:
  case V_SgTypeChar32:

    return true;

  case V_SgTypeVoid:
  case V_SgTypeGlobalVoid:
  case V_SgTypeFortranBOZ:
    return false;

  default:
    std::cerr << "REX_AST_INVARIANT[copy-constructible-type]: unexpected "
              << type->class_name() << std::endl;
    ROSE_ABORT();
  }
}

bool isAssignable(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  switch (type->variantT()) {
  case V_SgArrayType:
    return false;

  case V_SgModifierType:
    if (isSgModifierType(type)
            ->get_typeModifier()
            .get_constVolatileModifier()
            .isConst())
      return false;
    return isAssignable(isSgModifierType(type)->get_base_type());

  case V_SgFunctionType:
  case V_SgMemberFunctionType:
  case V_SgReferenceType:
  case V_SgRvalueReferenceType:
  case V_SgTypeEllipse:
  case V_SgDeclType:
    return false;

  case V_SgClassType: {
    SgClassDeclaration *decl =
        isSgClassDeclaration(isSgClassType(type)->get_declaration());
    ROSE_ASSERT(decl != nullptr);
    SgClassDeclaration *definingDecl =
        isSgClassDeclaration(decl->get_definingDeclaration());
    ROSE_ASSERT(definingDecl != nullptr);
    SgClassDefinition *defn = definingDecl->get_definition();
    ROSE_ASSERT(defn != nullptr);
    auto *source = getEnclosingSourceFile(defn);
    if (source != nullptr &&
        source->get_inputLanguage() == SgFile::e_C_language) {
      for (auto *declaration : defn->get_members()) {
        auto *member = isSgVariableDeclaration(declaration);
        if (member == nullptr)
          continue;
        for (auto *name : member->get_variables()) {
          auto *member_type = name->get_type();
          while (true) {
            if (isConstType(member_type))
              return false;
            member_type = member_type->stripTypedefsAndModifiers();
            auto *array = isSgArrayType(member_type);
            if (array == nullptr)
              break;
            member_type = array->get_base_type();
          }
          if (!isAssignable(member_type))
            return false;
        }
      }
      return true;
    }
    bool hasPublicAssignOperator = false, hasAnyAssignOperator = false;
    for (const auto *declarations : classMemberDeclarationLists(defn)) {
      if (declarations == nullptr)
        continue;
      for (auto *declaration : *declarations) {
        const auto *fundecl = isSgFunctionDeclaration(declaration);
        if (fundecl == nullptr ||
            fundecl->get_functionModifier().getCopyMoveKind() !=
                SgFunctionModifier::e_copy_assignment)
          continue;
        hasAnyAssignOperator = true;
        if (fundecl->get_declarationModifier()
                .get_accessModifier()
                .isPublic() &&
            !fundecl->get_functionModifier().isDeleted() &&
            !fundecl->get_functionModifier().isIneligibleSpecialMember())
          hasPublicAssignOperator = true;
      }
    }
    ROSE_ASSERT(hasAnyAssignOperator);
    return hasPublicAssignOperator;
  }

  case V_SgTypedefType:
    return isAssignable(isSgTypedefType(type)->get_base_type());

  case V_SgNonrealType: {
    if (SgType *resolved = resolveNonrealType(isSgNonrealType(type))) {
      return isAssignable(resolved);
    }
    return false;
  }

  case V_SgTemplateType:
    return false;

  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
  case V_SgTypeTargetBuiltin:

  case V_SgTypeComplex:
  case V_SgTypeImaginary:
  case V_SgEnumType:
  case V_SgPointerType:
  case V_SgPointerMemberType:
  case V_SgTypeBool:
  case V_SgTypeChar:
  case V_SgTypeDouble:
  case V_SgTypeFloat:
  case V_SgTypeInt:
  case V_SgTypeLong:
  case V_SgTypeLongDouble:
  case V_SgTypeLongLong:
  case V_SgTypeNullptr:
  case V_SgTypeShort:
  case V_SgTypeSignedChar:
  case V_SgTypeSignedInt:
  case V_SgTypeSignedLong:
  case V_SgTypeSignedShort:
  case V_SgTypeString:
  case V_SgTypeUnsignedChar:
  case V_SgTypeUnsignedInt:
  case V_SgTypeUnsignedLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeUnsignedShort:
  case V_SgTypeWchar:

  case V_SgTypeChar8:
  case V_SgTypeChar16:
  case V_SgTypeChar32:

    return true;

  case V_SgTypeVoid:
  case V_SgTypeGlobalVoid:
  case V_SgTypeFortranBOZ:
    return false;

  default:
    std::cerr << "REX_AST_INVARIANT[assignable-type]: unexpected "
              << type->class_name() << std::endl;
    ROSE_ABORT();
  }
}

SgType *getFirstVarType(SgVariableDeclaration *decl) {
  ROSE_ASSERT(decl);
  SgVariableSymbol *sym = getFirstVarSym(decl);
  ROSE_ASSERT(sym);
  return sym->get_type();
}

bool isPureVirtualClass(SgType *type,
                        const ClassHierarchyWrapper &classHierarchy) {
  SgClassType *classType = isSgClassType(type);
  if (classType == NULL)
    return false;

  SgClassDeclaration *classDeclaration =
      isSgClassDeclaration(classType->get_declaration());
  ROSE_ASSERT(classDeclaration != NULL);

  classDeclaration =
      isSgClassDeclaration(classDeclaration->get_definingDeclaration());
  if (classDeclaration == NULL) {
    // There's no defining declaration. We really can't tell
    return false;
  }
  SgClassDefinition *classDefinition = classDeclaration->get_definition();

  // Find all superclasses
  const ClassHierarchyWrapper::ClassDefSet &ancestors =
      classHierarchy.getAncestorClasses(classDefinition);
  set<SgClassDefinition *> inclusiveAncestors(ancestors.begin(),
                                              ancestors.end());
  inclusiveAncestors.insert(classDefinition);

  // Find all virtual and concrete functions
  set<SgMemberFunctionDeclaration *> concreteFunctions, pureVirtualFunctions;
  for (SgClassDefinition *c : inclusiveAncestors) {
    for (SgDeclarationStatement *memberDeclaration : c->get_members()) {
      if (SgMemberFunctionDeclaration *memberFunction =
              isSgMemberFunctionDeclaration(memberDeclaration)) {
        if (memberFunction->get_functionModifier().isPureVirtual()) {
          pureVirtualFunctions.insert(memberFunction);
        } else {
          concreteFunctions.insert(memberFunction);
        }
      }
    }
  }

  // Check if each virtual function is implemented somewhere
  for (SgMemberFunctionDeclaration *virtualFunction : pureVirtualFunctions) {
    bool foundConcrete = false;

    for (SgMemberFunctionDeclaration *concreteFunction : concreteFunctions) {
      if (concreteFunction->get_name() != virtualFunction->get_name())
        continue;

      if (concreteFunction->get_args().size() !=
          virtualFunction->get_args().size())
        continue;

      bool argsMatch = true;
      for (size_t i = 0; i < concreteFunction->get_args().size(); i++) {
        if (concreteFunction->get_args()[i]->get_type() !=
            virtualFunction->get_args()[i]->get_type()) {
          argsMatch = false;
          break;
        }
      }

      if (!argsMatch)
        continue;

      foundConcrete = true;
      break;
    }

    // If there's a pure virtual function with no corresponding concrete
    // function, the type is pure virtual
    if (!foundConcrete) {
      return true;
    }
  }

  return false;
}

namespace {
SgType *typeForTrait(const SgType *input, unsigned char strip) {
  ROSE_ASSERT(input != nullptr);
  SgType *type = input->stripType(strip);
  std::unordered_set<SgType *> visited;
  while (isSgNonrealType(type)) {
    ROSE_ASSERT(visited.insert(type).second);
    SgType *resolved = resolveNonrealType(isSgNonrealType(type));
    ROSE_ASSERT(resolved != nullptr && resolved != type);
    type = resolved->stripType(strip);
  }
  ROSE_ASSERT(type != nullptr && !isSgTypeUnknown(type) &&
              !isSgTypeDefault(type) && !isSgTypeDependent(type) &&
              !isSgTemplateType(type));
  return type;
}

SgClassDefinition *completeClassDefinitionForType(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  SgClassType *sgClassType = isSgClassType(type->stripTypedefsAndModifiers());
  ROSE_ASSERT(sgClassType != nullptr);

  SgClassDeclaration *decl =
      isSgClassDeclaration(sgClassType->get_declaration());
  ROSE_ASSERT(decl && "Requires a complete class type");
  SgClassDeclaration *definingDeclaration =
      isSgClassDeclaration(decl->get_definingDeclaration());
  ROSE_ASSERT(definingDeclaration && "Requires a complete class type");
  SgClassDefinition *classDef = definingDeclaration->get_definition();
  ROSE_ASSERT(classDef && "Requires a complete class type");
  return classDef;
}

struct TypeTraitClassHierarchyCache {
  SgProject *project = nullptr;
  uint64_t modificationSequence = 0;
  std::unique_ptr<ClassHierarchyWrapper> hierarchy;
};

ClassHierarchyWrapper &cachedTypeTraitClassHierarchy(SgClassDefinition *owner) {
  SgProject *project = getProject(owner);
  ROSE_ASSERT(project != nullptr);
  static thread_local TypeTraitClassHierarchyCache cache;
  const uint64_t sequence = SgNode::get_globalAstModificationSequence();
  if (!cache.hierarchy || cache.project != project ||
      cache.modificationSequence != sequence) {
    cache.hierarchy = std::make_unique<ClassHierarchyWrapper>(project);
    cache.project = project;
    cache.modificationSequence = sequence;
  }

  return *cache.hierarchy;
}

bool specialMemberCannotThrow(SgMemberFunctionDeclaration *member) {
  const auto &properties = member->get_functionModifier();
  if (properties.isGnuAttributeNoThrow())
    return true;
  auto *type = member->get_declared_exception_signature();
  if (type == nullptr) {
    ROSE_ASSERT(member->get_type() != nullptr &&
                member->get_type()->get_exception_specification_kind() ==
                    SgFunctionType::e_exception_none);
    return false;
  }
  switch (type->get_exception_specification_kind()) {
  case SgFunctionType::e_exception_dynamic_none:
  case SgFunctionType::e_exception_nothrow:
  case SgFunctionType::e_exception_noexcept:
  case SgFunctionType::e_exception_noexcept_true:
    return true;
  case SgFunctionType::e_exception_none:
  case SgFunctionType::e_exception_dynamic:
  case SgFunctionType::e_exception_ms_any:
  case SgFunctionType::e_exception_noexcept_false:
    return false;
  default:
    ROSE_ABORT();
  }
}

bool classSpecialMemberCannotThrow(
    SgType *type, SgFunctionModifier::copy_move_kind_enum kind) {
  auto *definition = completeClassDefinitionForType(type);
  bool found = false, eligible = false;
  for (const auto *declarations : classMemberDeclarationLists(definition)) {
    if (declarations == nullptr)
      continue;
    for (auto *declaration : *declarations) {
      auto *member = isSgMemberFunctionDeclaration(declaration);
      if (member == nullptr)
        continue;
      const auto &properties = member->get_functionModifier();
      const bool matches =
          kind == SgFunctionModifier::e_copy_move_none
              ? member->get_specialFunctionModifier().isConstructor() &&
                    properties.isDefaultConstructor() &&
                    member->get_args().empty()
              : properties.getCopyMoveKind() == kind;
      if (!matches)
        continue;
      found = true;
      if (properties.isIneligibleSpecialMember())
        continue;
      eligible = true;
      if (!specialMemberCannotThrow(member))
        return false;
    }
  }
  ROSE_ASSERT(found || kind == SgFunctionModifier::e_copy_move_none);
  return eligible;
}
} // namespace

vector<SgMemberFunctionDeclaration *>
GetAllMemberFunctionsInClassHierarchy(SgType *type) {
  auto *classDef = completeClassDefinitionForType(type);
  ClassHierarchyWrapper &classHierarchy =
      cachedTypeTraitClassHierarchy(classDef);
  vector<SgMemberFunctionDeclaration *> allMemberFunctions;
  const ClassHierarchyWrapper::ClassDefSet &ancestors =
      classHierarchy.getAncestorClasses(classDef);
  set<SgClassDefinition *> inclusiveAncestors(ancestors.begin(),
                                              ancestors.end());
  inclusiveAncestors.insert(classDef);

  for (SgClassDefinition *c : inclusiveAncestors) {
    for (SgDeclarationStatement *memberDeclaration : c->get_members()) {
      if (SgMemberFunctionDeclaration *memberFunction =
              isSgMemberFunctionDeclaration(memberDeclaration)) {
        allMemberFunctions.push_back(memberFunction);
      }
    }
  }

  return allMemberFunctions;
}

vector<SgVariableDeclaration *>
GetAllVariableDeclarationsInAncestors(SgType *type) {
  auto *classDef = completeClassDefinitionForType(type);
  ClassHierarchyWrapper &classHierarchy =
      cachedTypeTraitClassHierarchy(classDef);
  vector<SgVariableDeclaration *> allVariableDeclarations;
  const ClassHierarchyWrapper::ClassDefSet &ancestors =
      classHierarchy.getAncestorClasses(classDef);
  set<SgClassDefinition *> exclusiveAncestors(ancestors.begin(),
                                              ancestors.end());

  for (SgClassDefinition *c : exclusiveAncestors) {
    for (SgDeclarationStatement *memberDeclaration : c->get_members()) {
      if (SgVariableDeclaration *sgVariableDeclaration =
              isSgVariableDeclaration(memberDeclaration)) {
        allVariableDeclarations.push_back(sgVariableDeclaration);
      }
    }
  }

  return allVariableDeclarations;
}

bool HasNoThrowAssign(const SgType *const inputType) {
  // Strip typedef and array
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);

  if (isConstType(type))
    return false;

  if (isReferenceType(type))
    return false;

  if (HasTrivialAssign(type))
    return true;

  type = type->stripTypedefsAndModifiers();
  return isSgClassType(type) != nullptr &&
         classSpecialMemberCannotThrow(type,
                                       SgFunctionModifier::e_copy_assignment);
}

bool HasNoThrowCopy(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);

  if (HasTrivialCopy(type))
    return true;

  return isSgClassType(type) != nullptr &&
         classSpecialMemberCannotThrow(type,
                                       SgFunctionModifier::e_copy_constructor);
}

bool HasNoThrowConstructor(const SgType *const inputType) {
  // Strip typedef and array
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);

  if (HasTrivialConstructor(type))
    return true;

  type = type->stripTypedefsAndModifiers();
  return isSgClassType(type) != nullptr &&
         classSpecialMemberCannotThrow(type,
                                       SgFunctionModifier::e_copy_move_none);
}

bool HasTrivialAssign(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);
  if (isConstType(type) || isReferenceType(type) ||
      isSgRvalueReferenceType(type))
    return false;
  type = type->stripTypedefsAndModifiers();
  if (isSgTypeVoid(type))
    return false;
  if (isScalarType(type) || isSgPointerType(type) ||
      isSgPointerMemberType(type) || isSgEnumType(type) ||
      isSgTypeNullptr(type))
    return true;
  if (isSgClassType(type) == nullptr)
    return false;
  auto *definition = completeClassDefinitionForType(type);
  auto *source = getEnclosingSourceFile(definition);
  if (source != nullptr && source->get_inputLanguage() == SgFile::e_C_language)
    return true;
  bool foundCopyAssignment = false, foundEligibleAssignment = false;
  for (const auto *declarations : classMemberDeclarationLists(definition)) {
    if (declarations == nullptr)
      continue;
    for (auto *declaration : *declarations) {
      auto *member = isSgMemberFunctionDeclaration(declaration);
      if (member == nullptr ||
          member->get_functionModifier().getCopyMoveKind() !=
              SgFunctionModifier::e_copy_assignment)
        continue;
      foundCopyAssignment = true;
      if (member->get_functionModifier().isIneligibleSpecialMember())
        continue;
      foundEligibleAssignment = true;
      if (!member->get_functionModifier().isTrivialSpecialMember())
        return false;
    }
  }
  ROSE_ASSERT(foundCopyAssignment);
  return foundEligibleAssignment;
}

bool HasTrivialCopy(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);
  if (isSgTypeVoid(type))
    return false;
  if (isScalarType(type) || isSgPointerType(type) ||
      isSgPointerMemberType(type) || isSgEnumType(type) ||
      isSgTypeNullptr(type) || isSgReferenceType(type) ||
      isSgRvalueReferenceType(type))
    return true;
  if (isSgClassType(type) == nullptr)
    return false;
  auto *definition = completeClassDefinitionForType(type);
  auto *source = getEnclosingSourceFile(definition);
  if (source != nullptr && source->get_inputLanguage() == SgFile::e_C_language)
    return true;
  bool foundCopyConstructor = false, foundEligibleConstructor = false;
  for (const auto *declarations : classMemberDeclarationLists(definition)) {
    if (declarations == nullptr)
      continue;
    for (auto *declaration : *declarations) {
      auto *member = isSgMemberFunctionDeclaration(declaration);
      if (member == nullptr ||
          member->get_functionModifier().getCopyMoveKind() !=
              SgFunctionModifier::e_copy_constructor)
        continue;
      foundCopyConstructor = true;
      if (member->get_functionModifier().isIneligibleSpecialMember())
        continue;
      foundEligibleConstructor = true;
      if (!member->get_functionModifier().isTrivialSpecialMember())
        return false;
    }
  }
  ROSE_ASSERT(foundCopyConstructor);
  return foundEligibleConstructor;
}

bool HasTrivialConstructor(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);
  if (isSgTypeVoid(type) || isSgReferenceType(type) ||
      isSgRvalueReferenceType(type))
    return false;
  if (isScalarType(type) || isSgPointerType(type) ||
      isSgPointerMemberType(type) || isSgEnumType(type) ||
      isSgTypeNullptr(type))
    return true;
  if (isSgClassType(type) == nullptr)
    return false;
  auto *definition = completeClassDefinitionForType(type);
  auto *source = getEnclosingSourceFile(definition);
  if (source != nullptr && source->get_inputLanguage() == SgFile::e_C_language)
    return true;
  bool hasAnyConstructor = false;
  bool foundDefaultConstructor = false;
  for (const auto *declarations : classMemberDeclarationLists(definition)) {
    if (declarations == nullptr)
      continue;
    for (auto *declaration : *declarations) {
      auto *member = isSgMemberFunctionDeclaration(declaration);
      if (member == nullptr ||
          !member->get_specialFunctionModifier().isConstructor())
        continue;
      hasAnyConstructor = true;
      const auto &properties = member->get_functionModifier();
      if (properties.isDefaultConstructor() &&
          !properties.isIneligibleSpecialMember()) {
        foundDefaultConstructor = true;
        if (!properties.isTrivialSpecialMember())
          return false;
      }
    }
  }
  ROSE_ASSERT(hasAnyConstructor);
  return foundDefaultConstructor;
}

bool HasTrivialDestructor(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);
  if (isSgTypeVoid(type))
    return false;
  if (isScalarType(type) || isPointerType(type) ||
      isSgPointerMemberType(type) || isSgEnumType(type) ||
      isSgTypeNullptr(type) || isSgReferenceType(type) ||
      isSgRvalueReferenceType(type))
    return true;
  if (isSgClassType(type) == nullptr)
    return false;
  auto *definition = completeClassDefinitionForType(type);
  auto *source = getEnclosingSourceFile(definition);
  if (source != nullptr && source->get_inputLanguage() == SgFile::e_C_language)
    return true;
  auto *destructor = getDefaultDestructor(definition->get_declaration());
  ROSE_ASSERT(destructor != nullptr);
  const auto properties =
      destructor->get_functionModifier().getDestructorProperties();
  ROSE_ASSERT(properties == SgFunctionModifier::e_destructor_selected_trivial ||
              properties ==
                  SgFunctionModifier::e_destructor_selected_nontrivial);
  return properties == SgFunctionModifier::e_destructor_selected_trivial;
}

bool HasVirtualDestructor(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);

  if (!IsClass(type)) {
    return false;
  }

  vector<SgMemberFunctionDeclaration *> allMemberFunctions =
      GetAllMemberFunctionsInClassHierarchy(type);
  for (SgMemberFunctionDeclaration *memberFunction : allMemberFunctions) {
    if (memberFunction->get_functionModifier().isVirtual() &&
        memberFunction->get_specialFunctionModifier().isDestructor())
      return true;
  }

  // No virtual destructor(s) found
  return false;
}

bool IsAbstract(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);
  if (!IsClass(type)) {
    return false;
  }
  return SageInterface::isPureVirtualClass(
      type,
      cachedTypeTraitClassHierarchy(completeClassDefinitionForType(type)));
}

bool IsBaseOf(const SgType *const inputBaseType,
              const SgType *const inputDerivedType) {
  // Strip typedef and array
  SgType *baseType = typeForTrait(
      inputBaseType, SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
  SgType *derivedType =
      typeForTrait(inputDerivedType,
                   SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);

  if (!IsClass(baseType) || !IsClass(derivedType)) {
    return false;
  }

  SgClassType *baseClassType = isSgClassType(baseType);
  SgClassDefinition *classDef = completeClassDefinitionForType(derivedType);

  // Find all superclasses
  const ClassHierarchyWrapper::ClassDefSet &ancestors =
      cachedTypeTraitClassHierarchy(classDef).getAncestorClasses(classDef);
  set<SgClassDefinition *> inclusiveAncestors(ancestors.begin(),
                                              ancestors.end());
  inclusiveAncestors.insert(classDef);

  // Match types
  for (SgClassDefinition *c : inclusiveAncestors) {
    if (c->get_declaration()->get_type()->stripType(
            SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_ARRAY_TYPE |
            SgType::STRIP_MODIFIER_TYPE) == baseClassType)
      return true;
  }
  return false;
}

bool IsClass(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);
  return isSgClassType(type) && (!IsUnion(type));
}

bool IsEmpty(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);

  if (!IsClass(type))
    return false;

  SgClassDefinition *classDefinition = completeClassDefinitionForType(type);
  SgDeclarationStatementPtrList &members = classDefinition->get_members();
  for (SgDeclarationStatementPtrList::iterator it = members.begin();
       it != members.end(); ++it) {
    SgDeclarationStatement *declarationStatement = *it;
    ROSE_ASSERT(declarationStatement != NULL);

    if (SgMemberFunctionDeclaration *memberFunctionDeclaration =
            isSgMemberFunctionDeclaration(declarationStatement)) {
      // Member function can't be virtual
      if (memberFunctionDeclaration->get_functionModifier().isVirtual())
        return false;
    } else if (SgVariableDeclaration *sgVariableDeclaration =
                   isSgVariableDeclaration(declarationStatement)) {
      if (SageInterface::isStatic(sgVariableDeclaration))
        continue;
      for (SgInitializedName *name : sgVariableDeclaration->get_variables()) {
        ROSE_ASSERT(name != nullptr);
        SgVariableDefinition *definition = name->get_definition();
        ROSE_ASSERT(definition != nullptr);
        if (definition->get_bitfield() != nullptr) {
          if (!definition->get_is_zero_width_bitfield())
            return false;
        } else if (!definition->get_has_no_unique_address() ||
                   !IsEmpty(definition->get_type())) {
          return false;
        }
      }
    }
  }

  // No virtual base classes
  SgBaseClassPtrList &baseClasses = classDefinition->get_inheritances();
  for (SgBaseClassPtrList::iterator it = baseClasses.begin();
       it != baseClasses.end(); ++it) {
    // Base class can't be virtual

    ROSE_ASSERT((*it)->get_baseClassModifier() != NULL);
    if ((*it)->get_baseClassModifier()->isVirtual())
      return false;

    // Base classes must be empty as well
    SgClassDeclaration *baseClass = (*it)->get_base_class();
    ROSE_ASSERT(baseClass != NULL);
    if (!IsEmpty(baseClass->get_type()))
      return false;
  }

  return true;
}

bool IsEnum(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);
  return isSgEnumType(type);
}

bool IsLiteralType(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);

  // Scalars, Pointers and Reference are literal
  if (isScalarType(type) || isReferenceType(type) || isPointerType(type)) {
    return true;
  }

  ROSE_ABORT();
}

bool IsPod(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);
  if (IsTrivial(type) && IsStandardLayout(type))
    return true;
  return false;
}

bool IsPolymorphic(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_TYPEDEF_TYPE);

  if (!IsClass(type)) {
    return false;
  }

  vector<SgMemberFunctionDeclaration *> allMemberFunctions =
      GetAllMemberFunctionsInClassHierarchy(type);
  for (SgMemberFunctionDeclaration *memberFunction : allMemberFunctions) {
    if (memberFunction->get_functionModifier().isVirtual())
      return true;
  }
  return false;
}

bool IsStandardLayout(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);

  if (isSgTypeVoid(type))
    return false;

  // isPointerType are scalar
  if (isScalarType(type) || isPointerType(type))
    return true;

  if (isSgClassType(type) == nullptr)
    return false;
  SgClassDefinition *classDef = completeClassDefinitionForType(type);

  // If we have a virtual function, then it not OK
  for (SgDeclarationStatement *memberDeclaration : classDef->get_members()) {
    if (SgMemberFunctionDeclaration *memberFunction =
            isSgMemberFunctionDeclaration(memberDeclaration)) {
      if (memberFunction->get_functionModifier().isVirtual())
        return false;
    }
  }

  // No virtual base classes
  SgBaseClassPtrList &baseClasses = classDef->get_inheritances();
  for (SgBaseClassPtrList::iterator it = baseClasses.begin();
       it != baseClasses.end(); ++it) {
    // Base class can't be virtual
    ROSE_ASSERT((*it)->get_baseClassModifier() != NULL);
    if ((*it)->get_baseClassModifier()->isVirtual())
      return false;
  }

  bool haveNonStaticData = false;
  SgType *firstNonStaticDataMember = NULL;

  // same access control (private, protected, public) for all its non-static
  // data members
  bool first = true;
  SgAccessModifier::access_modifier_enum prevModifier =
      SgAccessModifier::e_unknown;
  for (SgDeclarationStatement *memberDeclaration : classDef->get_members()) {
    // has no non-static data members with brace- or equal- initializers.
    if (SgVariableDeclaration *memberVariable =
            isSgVariableDeclaration(memberDeclaration)) {
      if (memberVariable->get_declarationModifier()
              .get_storageModifier()
              .isStatic())
        continue;

      SgAccessModifier::access_modifier_enum mod =
          memberDeclaration->get_declarationModifier()
              .get_accessModifier()
              .get_modifier();
      if (!first && mod != prevModifier) {
        return false;
      }

      if (first) {
        first = false;
        firstNonStaticDataMember = NULL;
        for (SgInitializedName *name : memberVariable->get_variables()) {
          firstNonStaticDataMember = name->get_type();
          break;
        }
      }
      prevModifier = mod;
      haveNonStaticData = true;
    }
  }

  // If the most derived type has non-static data members, then none of the base
  // classes can have non-static members.
  if (haveNonStaticData) {
    vector<SgVariableDeclaration *> allVariableDeclarations =
        GetAllVariableDeclarationsInAncestors(type);
    for (SgVariableDeclaration *memberVariable : allVariableDeclarations) {
      if (memberVariable->get_declarationModifier()
              .get_storageModifier()
              .isStatic())
        continue;
      // ok, we have atleast one non-static data member in the ancestry
      return false;
    }
  } else {
    // no non-static data members in the most derived class and at most one base
    // class with non-static data members
    // Find all superclasses
    const ClassHierarchyWrapper::ClassDefSet &ancestors =
        cachedTypeTraitClassHierarchy(classDef).getAncestorClasses(classDef);
    set<SgClassDefinition *> exclusiveAncestors(ancestors.begin(),
                                                ancestors.end());

    // There can be atmost 1 base class with non-static data members
    int baseClassesWithNonStaticData = 0;
    for (SgClassDefinition *c : exclusiveAncestors) {
      for (SgDeclarationStatement *baseMemberDeclaration : c->get_members()) {
        if (SgVariableDeclaration *baseMemberVariable =
                isSgVariableDeclaration(baseMemberDeclaration)) {
          if (!baseMemberVariable->get_declarationModifier()
                   .get_storageModifier()
                   .isStatic()) {
            baseClassesWithNonStaticData++;
            if (baseClassesWithNonStaticData > 1)
              return false;
            break;
          }
        }
      }
    }
  }

  // All base classes have to be standard layout
  for (SgBaseClass *baseClass : classDef->get_inheritances()) {
    if (!IsStandardLayout(baseClass->get_base_class()->get_type()))
      return false;
  }

  // There should be no base classes of the same type as its first non-static
  // data member
  if (firstNonStaticDataMember) {
    const ClassHierarchyWrapper::ClassDefSet &ancestors =
        cachedTypeTraitClassHierarchy(classDef).getAncestorClasses(classDef);
    set<SgClassDefinition *> exclusiveAncestors(ancestors.begin(),
                                                ancestors.end());

    for (SgClassDefinition *c : exclusiveAncestors) {
      if (c->get_declaration()->get_type()->stripType(
              SgType::STRIP_TYPEDEF_TYPE) ==
          firstNonStaticDataMember->stripType(SgType::STRIP_TYPEDEF_TYPE))
        return false;
    }
  }

  return true;
}

bool IsTrivial(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_ARRAY_TYPE);

  if (isSgTypeVoid(type))
    return false;

  if (isScalarType(type) || isSgPointerType(type) ||
      isSgPointerMemberType(type) || isSgEnumType(type) ||
      isSgTypeNullptr(type))
    return true;

  if (isSgClassType(type) == nullptr)
    return false;
  auto *definition = completeClassDefinitionForType(type);
  auto *source = getEnclosingSourceFile(definition);
  if (source != nullptr && source->get_inputLanguage() == SgFile::e_C_language)
    return true;
  if (!HasTrivialConstructor(type) || !HasTrivialDestructor(type))
    return false;
  bool eligibleCopyMove = false;
  for (const auto *declarations : classMemberDeclarationLists(definition)) {
    if (declarations == nullptr)
      continue;
    for (auto *declaration : *declarations) {
      auto *member = isSgMemberFunctionDeclaration(declaration);
      if (member == nullptr)
        continue;
      const auto &properties = member->get_functionModifier();
      if (properties.getCopyMoveKind() == SgFunctionModifier::e_copy_move_none)
        continue;
      if (properties.isIneligibleSpecialMember())
        continue;
      eligibleCopyMove = true;
      if (!properties.isTrivialSpecialMember())
        return false;
    }
  }
  return eligibleCopyMove;
}

bool IsUnion(const SgType *const inputType) {
  SgType *type = typeForTrait(inputType, SgType::STRIP_TYPEDEF_TYPE);
  return isUnionType(type);
}

} // namespace SageInterface
