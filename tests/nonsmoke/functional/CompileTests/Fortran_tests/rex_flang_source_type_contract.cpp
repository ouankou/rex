#include "rose.h"
#include "sage-build.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace {

Fortran::parser::CharBlock sourceBlock(const std::string &text);

SgTypeInt *buildSourceInteger(std::int64_t kind, bool includeSelector) {
  SgTypeInt *type = new SgTypeInt();
  ROSE_ASSERT(type != nullptr);
  type->set_fortran_source_syntax(true);
  if (includeSelector) {
    SgIntVal *selector = SageBuilder::buildIntVal_nfi(std::to_string(kind));
    ROSE_ASSERT(selector != nullptr);
    selector->set_fortran_integer_constant_value(kind);
    selector->set_fortran_integer_constant_value_is_available(true);
    type->set_type_kind(selector);
    selector->set_parent(type);
  }
  return type;
}

SgTypeInt *buildSemanticInteger(std::int64_t kind) {
  SgIntVal *selector = SageBuilder::buildIntVal_nfi(std::to_string(kind));
  ROSE_ASSERT(selector != nullptr);
  SgTypeInt *type = SageBuilder::buildIntType(selector);
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(!type->get_fortran_source_syntax());
  return type;
}

void requireObjectContract(SgType *source, SgType *semantic) {
  Rose::builder::detail::RequireFlangSourceTypeContract(
      source, semantic,
      Rose::builder::detail::FlangSourceTypeContract::explicit_object,
      "selector_probe");
}

SgArrayType *buildArrayWrapper(SgType *base, int rank, bool sourceSyntax,
                               bool coarray = false) {
  ROSE_ASSERT(base != nullptr && rank > 0);
  SgArrayType *array = new SgArrayType(base, nullptr);
  ROSE_ASSERT(array != nullptr);
  SgExprListExp *dimensions = SageBuilder::buildExprListExp_nfi();
  ROSE_ASSERT(dimensions != nullptr);
  for (int index = 0; index < rank; ++index) {
    SageInterface::appendExpression(dimensions,
                                    SageBuilder::buildColonShapeExp_nfi());
  }
  array->set_dim_info(dimensions);
  dimensions->set_parent(array);
  array->set_rank(rank);
  array->set_isCoArray(coarray);
  array->set_fortran_source_syntax(sourceSyntax);
  return array;
}

SgType *buildRecursiveWrapperType(bool sourceSyntax, int rank = 2) {
  SgType *base = sourceSyntax
                     ? static_cast<SgType *>(buildSourceInteger(4, true))
                     : static_cast<SgType *>(buildSemanticInteger(4));
  SgArrayType *array =
      buildArrayWrapper(base, rank, sourceSyntax, /*coarray=*/true);
  SgPointerType *pointer = new SgPointerType(array);
  ROSE_ASSERT(pointer != nullptr);
  pointer->set_fortran_source_syntax(sourceSyntax);
  SgModifierType *modifier = new SgModifierType(pointer);
  ROSE_ASSERT(modifier != nullptr);
  modifier->set_fortran_source_syntax(sourceSyntax);
  return modifier;
}

SgTypeString *buildDynamicCharacterContract(SgType *resultType,
                                            bool sourceSyntax) {
  ROSE_ASSERT(resultType != nullptr);
  SgAddOp *length = SageBuilder::buildBinaryExpression_nfi<SgAddOp>(
      SageBuilder::buildIntVal_nfi("1"), SageBuilder::buildIntVal_nfi("2"),
      resultType);
  ROSE_ASSERT(length != nullptr);
  SgTypeString *type = new SgTypeString(length);
  ROSE_ASSERT(type != nullptr);
  length->set_parent(type);
  SgIntVal *kind = SageBuilder::buildIntVal_nfi("4");
  ROSE_ASSERT(kind != nullptr);
  if (sourceSyntax) {
    kind->set_fortran_integer_constant_value(4);
    kind->set_fortran_integer_constant_value_is_available(true);
  }
  type->set_type_kind(kind);
  kind->set_parent(type);
  type->set_fortran_source_syntax(sourceSyntax);
  return type;
}

void setExactExpressionPositions(SgExpression *expression, int line,
                                 int column) {
  ROSE_ASSERT(expression != nullptr);
  Sg_File_Info *fileInfo =
      new Sg_File_Info("rex_flang_source_type_contract.f90", line, column);
  Sg_File_Info *start =
      new Sg_File_Info("rex_flang_source_type_contract.f90", line, column);
  Sg_File_Info *end =
      new Sg_File_Info("rex_flang_source_type_contract.f90", line, column);
  Sg_File_Info *operatorPosition =
      new Sg_File_Info("rex_flang_source_type_contract.f90", line, column);
  for (Sg_File_Info *position : {fileInfo, start, end, operatorPosition}) {
    ROSE_ASSERT(position != nullptr);
    position->setOutputInCodeGeneration();
    position->set_parent(expression);
  }
  expression->set_file_info(fileInfo);
  expression->set_startOfConstruct(start);
  expression->set_endOfConstruct(end);
  expression->set_operatorPosition(operatorPosition);
}

void setComponentLengthPositions(SgExpression *length, SgNode *positionOwner,
                                 bool complete) {
  ROSE_ASSERT(length != nullptr && positionOwner != nullptr);
  Sg_File_Info *start =
      new Sg_File_Info("rex_flang_source_type_contract.f90", 1, 20);
  ROSE_ASSERT(start != nullptr);
  start->setOutputInCodeGeneration();
  start->set_parent(positionOwner);
  length->set_startOfConstruct(start);
  if (!complete) {
    return;
  }
  Sg_File_Info *end =
      new Sg_File_Info("rex_flang_source_type_contract.f90", 1, 20);
  Sg_File_Info *operatorPosition =
      new Sg_File_Info("rex_flang_source_type_contract.f90", 1, 20);
  ROSE_ASSERT(end != nullptr && operatorPosition != nullptr);
  for (Sg_File_Info *position : {end, operatorPosition}) {
    position->setOutputInCodeGeneration();
    position->set_parent(positionOwner);
  }
  length->set_endOfConstruct(end);
  length->set_operatorPosition(operatorPosition);
}

void publishComponentLengthContract(const std::string &statement,
                                    const std::string &nameSpelling,
                                    SgExpression *length) {
  const std::size_t nameOffset = statement.find(nameSpelling);
  ROSE_ASSERT(nameOffset != std::string::npos);
  Fortran::parser::Name name{
      Fortran::parser::CharBlock(statement.data() + nameOffset,
                                 nameSpelling.size()),
      nullptr};
  Fortran::parser::CharLength sourceLength(std::uint64_t{7});
  Rose::builder::detail::PublishExactFortranEntityCharacterLength(
      length, sourceLength, name, sourceBlock(statement));
}

SgFunctionType *buildFunctionTypeWithArgument(SgType *result, SgType *argument,
                                              bool sourceSyntax) {
  ROSE_ASSERT(result != nullptr && argument != nullptr);
  SgFunctionType *function = new SgFunctionType(result, false);
  ROSE_ASSERT(function != nullptr);
  function->set_fortran_source_syntax(sourceSyntax);
  SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
  ROSE_ASSERT(arguments != nullptr);
  arguments->append_argument(argument);
  SgFunctionParameterTypeList *empty = function->get_argument_list();
  ROSE_ASSERT(empty != nullptr && empty->get_parent() == function);
  function->set_argument_list(arguments);
  arguments->set_parent(function);
  empty->set_parent(nullptr);
  SageInterface::deleteAST(
      empty, SageInterface::DeleteAstMode::kSkipExternalReferences);
  return function;
}

Fortran::parser::CharBlock sourceBlock(const std::string &text) {
  return Fortran::parser::CharBlock(text.data(), text.size());
}

template <typename Builder>
void requirePrimitiveSemanticInterning(Builder build, const char *name) {
  SgIntVal *firstSelector = SageBuilder::buildIntVal_nfi("4");
  ROSE_ASSERT(firstSelector != nullptr);
  SgType *canonical = build(firstSelector);
  ROSE_ASSERT(canonical != nullptr);
  SgExpression *canonicalSelector = canonical->get_type_kind();
  ROSE_ASSERT(canonicalSelector != nullptr);
  ROSE_ASSERT(canonicalSelector->get_parent() == canonical);
  ROSE_ASSERT(!canonical->get_fortran_source_syntax());

  SgIntVal *duplicateSelector = SageBuilder::buildIntVal_nfi("4");
  ROSE_ASSERT(duplicateSelector != nullptr);
  SgType *repeated = build(duplicateSelector);
  if (repeated != canonical || repeated->get_type_kind() != canonicalSelector ||
      SgNode::isLiveNode(duplicateSelector)) {
    std::cerr << "semantic " << name
              << " KIND interning retained an exploratory selector or changed "
                 "canonical identity\n";
    ROSE_ABORT();
  }
}

void requireSemanticStringInterning() {
  SgIntVal *firstLength = SageBuilder::buildIntVal_nfi("3");
  SgIntVal *firstKind = SageBuilder::buildIntVal_nfi("4");
  ROSE_ASSERT(firstLength != nullptr && firstKind != nullptr);
  SgTypeString *canonical = SgTypeString::createType(firstLength, firstKind);
  ROSE_ASSERT(canonical != nullptr);
  SgExpression *canonicalLength = canonical->get_lengthExpression();
  SgExpression *canonicalKind = canonical->get_type_kind();
  ROSE_ASSERT(canonicalLength != nullptr && canonicalKind != nullptr);
  ROSE_ASSERT(canonicalLength->get_parent() == canonical);
  ROSE_ASSERT(canonicalKind->get_parent() == canonical);
  ROSE_ASSERT(!canonical->get_fortran_source_syntax());

  SgIntVal *duplicateLength = SageBuilder::buildIntVal_nfi("3");
  SgIntVal *duplicateKind = SageBuilder::buildIntVal_nfi("4");
  ROSE_ASSERT(duplicateLength != nullptr && duplicateKind != nullptr);
  SgTypeString *repeated =
      SgTypeString::createType(duplicateLength, duplicateKind);
  SgTypeTable *table = SgNode::get_globalTypeTable();
  ROSE_ASSERT(table != nullptr);
  if (repeated != canonical ||
      repeated->get_lengthExpression() != canonicalLength ||
      repeated->get_type_kind() != canonicalKind ||
      table->lookup_type(canonical->get_mangled()) != canonical ||
      SgNode::isLiveNode(duplicateLength) ||
      SgNode::isLiveNode(duplicateKind)) {
    std::cerr << "semantic CHARACTER interning retained exploratory selectors "
                 "or changed canonical identity\n";
    ROSE_ABORT();
  }

  auto buildDynamicLength = []() -> SgExpression * {
    SgAddOp *length = SageBuilder::buildBinaryExpression_nfi<SgAddOp>(
        SageBuilder::buildIntVal_nfi("1"), SageBuilder::buildIntVal_nfi("2"),
        SageBuilder::buildIntType());
    ROSE_ASSERT(length != nullptr && length->get_parent() == nullptr);
    return length;
  };
  SgExpression *firstDynamicLength = buildDynamicLength();
  SgTypeString *dynamicCanonical =
      SgTypeString::createType(firstDynamicLength, nullptr);
  ROSE_ASSERT(dynamicCanonical != nullptr &&
              dynamicCanonical->get_lengthExpression() == firstDynamicLength);
  SgExpression *duplicateDynamicLength = buildDynamicLength();
  SgTypeString *dynamicRepeated =
      SgTypeString::createType(duplicateDynamicLength, nullptr);
  if (dynamicRepeated != dynamicCanonical ||
      dynamicRepeated->get_lengthExpression() != firstDynamicLength ||
      SgNode::isLiveNode(duplicateDynamicLength)) {
    std::cerr << "semantic CHARACTER interning did not canonicalize "
                 "structurally equivalent dynamic length expressions\n";
    ROSE_ABORT();
  }

  SgAsteriskShapeExp *firstAssumed = new SgAsteriskShapeExp();
  SgAsteriskShapeExp *duplicateAssumed = new SgAsteriskShapeExp();
  SgColonShapeExp *deferred = SageBuilder::buildColonShapeExp_nfi();
  ROSE_ASSERT(firstAssumed != nullptr && duplicateAssumed != nullptr &&
              deferred != nullptr);
  SgTypeString *assumedCanonical =
      SgTypeString::createType(firstAssumed, nullptr);
  SgTypeString *assumedRepeated =
      SgTypeString::createType(duplicateAssumed, nullptr);
  SgTypeString *deferredCanonical = SgTypeString::createType(deferred, nullptr);
  if (assumedRepeated != assumedCanonical ||
      assumedCanonical == deferredCanonical ||
      assumedCanonical->get_mangled() == deferredCanonical->get_mangled() ||
      SgNode::isLiveNode(duplicateAssumed)) {
    std::cerr << "semantic CHARACTER interning did not preserve distinct "
                 "assumed and deferred length identities\n";
    ROSE_ABORT();
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }
  const std::string mode = argv[1];
  SgTypeInt *semantic = buildSemanticInteger(4);

  if (mode == "valid-explicit") {
    SgTypeInt *source = buildSourceInteger(4, true);
    requireObjectContract(source, semantic);
    SgTypeTable *table = SgNode::get_globalTypeTable();
    ROSE_ASSERT(table != nullptr);
    ROSE_ASSERT(source != semantic);
    ROSE_ASSERT(table->lookup_type(source->get_mangled()) != source);

    setExactExpressionPositions(source->get_type_kind(), 1, 1);
    SgTreeCopy copyHelp;
    SgExpression *selectorCopy =
        isSgExpression(source->get_type_kind()->copy(copyHelp));
    ROSE_ASSERT(selectorCopy != nullptr);
    ROSE_ASSERT(
        selectorCopy->get_fortran_integer_constant_value_is_available());
    ROSE_ASSERT(selectorCopy->get_fortran_integer_constant_value() == 4);
    return 0;
  }
  if (mode == "valid-omitted") {
    SgTypeInt *source = buildSourceInteger(0, false);
    requireObjectContract(source, semantic);
    return source != semantic && source->get_type_kind() == nullptr ? 0 : 1;
  }
  if (mode == "valid-semantic-interning") {
    requirePrimitiveSemanticInterning(
        [](SgExpression *kind) { return SageBuilder::buildBoolType(kind); },
        "LOGICAL");
    requirePrimitiveSemanticInterning(
        [](SgExpression *kind) { return SageBuilder::buildIntType(kind); },
        "INTEGER");
    requirePrimitiveSemanticInterning(
        [](SgExpression *kind) {
          return SageBuilder::buildUnsignedIntType(kind);
        },
        "UNSIGNED");
    requirePrimitiveSemanticInterning(
        [](SgExpression *kind) { return SageBuilder::buildFloatType(kind); },
        "REAL");
    requireSemanticStringInterning();
    return 0;
  }
  if (mode == "valid-recursive-wrapper") {
    SgType *source = buildRecursiveWrapperType(true);
    SgType *semanticWrapper = buildRecursiveWrapperType(false);
    return SageInterface::fortranSourceTypeMatchesSemanticType(source,
                                                               semanticWrapper)
               ? 0
               : 1;
  }
  if (mode == "valid-parameter-attribute") {
    SgType *source = buildSourceInteger(4, true);
    SgType *parameterSemantic = SageBuilder::buildConstType(semantic);
    ROSE_ASSERT(isSgModifierType(parameterSemantic) != nullptr);
    return SageInterface::fortranSourceTypeMatchesSemanticType(
               source, parameterSemantic)
               ? 0
               : 1;
  }
  if (mode == "valid-function-argument-identity") {
    SgType *argument = SageBuilder::buildIntType();
    SgFunctionType *sourceFunction = buildFunctionTypeWithArgument(
        buildSourceInteger(4, true), argument, true);
    SgFunctionType *semanticFunction =
        buildFunctionTypeWithArgument(buildSemanticInteger(4), argument, false);
    return SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
               sourceFunction, semanticFunction)
               ? 0
               : 1;
  }
  if (mode == "valid-dynamic-length-result-identity") {
    SgType *resultType = SageBuilder::buildIntType();
    return SageInterface::fortranSourceTypeMatchesSemanticType(
               buildDynamicCharacterContract(resultType, true),
               buildDynamicCharacterContract(resultType, false))
               ? 0
               : 1;
  }
  if (mode == "valid-signed-selector-boundary") {
    const std::uint64_t maximum =
        static_cast<std::uint64_t>(std::numeric_limits<long long>::max());
    std::optional<Fortran::parser::KindParam> kind(
        Fortran::parser::KindParam(std::uint64_t{maximum}));
    std::uint64_t kindValue = 0;
    std::string kindSpelling;
    Rose::builder::BuildImpl(kind, kindValue, kindSpelling);
    ROSE_ASSERT(kindValue == maximum &&
                kindSpelling == std::to_string(maximum));

    SgExpression *starExpression = nullptr;
    Rose::builder::BuildImpl(
        Fortran::parser::KindSelector::StarSize(std::uint64_t{maximum}),
        starExpression);
    SgExpression *lengthExpression = nullptr;
    Rose::builder::BuildImpl(
        Fortran::parser::CharLength(std::uint64_t{maximum}), lengthExpression);
    return isSgLongLongIntVal(starExpression) != nullptr &&
                   isSgLongLongIntVal(lengthExpression) != nullptr
               ? 0
               : 1;
  }
  if (mode == "valid-literals") {
    const std::string signedText = "42";
    Fortran::parser::IntLiteralConstant signedLiteral(
        sourceBlock(signedText), std::optional<Fortran::parser::KindParam>{});
    SgExpression *signedExpression = nullptr;
    Rose::builder::BuildImpl(signedLiteral, signedExpression);
    SgIntVal *signedValue = isSgIntVal(signedExpression);
    ROSE_ASSERT(signedValue != nullptr && signedValue->get_value() == 42);

    const std::string unsignedText = "18446744073709551615";
    Fortran::parser::UnsignedLiteralConstant unsignedLiteral(
        sourceBlock(unsignedText), std::optional<Fortran::parser::KindParam>{});
    SgExpression *unsignedExpression = nullptr;
    Rose::builder::BuildImpl(unsignedLiteral, unsignedExpression);
    SgUnsignedLongLongIntVal *unsignedValue =
        isSgUnsignedLongLongIntVal(unsignedExpression);
    ROSE_ASSERT(unsignedValue != nullptr);
    ROSE_ASSERT(unsignedValue->get_value() ==
                std::numeric_limits<unsigned long long>::max());

    const std::string statement = "( + 1.0, 2.0)";
    const std::size_t realOffset = statement.find("1.0");
    ROSE_ASSERT(realOffset != std::string::npos);
    const Fortran::parser::CharBlock sign =
        Rose::builder::detail::RequireExactFortranSignedRealSignSource(
            Fortran::parser::CharBlock(statement.data() + realOffset, 3),
            sourceBlock(statement), Fortran::parser::Sign::Positive);
    return sign.size() == 1 && sign.front() == '+' ? 0 : 1;
  }
  if (mode == "mismatched-kind") {
    requireObjectContract(buildSourceInteger(8, true), semantic);
  } else if (mode == "missing-source-identity") {
    SgTypeInt *source = buildSourceInteger(4, true);
    source->set_fortran_source_syntax(false);
    requireObjectContract(source, semantic);
  } else if (mode == "mismatched-wrapper") {
    requireObjectContract(buildRecursiveWrapperType(true, 2),
                          buildRecursiveWrapperType(false, 1));
  } else if (mode == "distinct-function-argument") {
    SgFunctionType *sourceFunction = buildFunctionTypeWithArgument(
        buildSourceInteger(4, true), new SgTypeInt(), true);
    SgFunctionType *semanticFunction = buildFunctionTypeWithArgument(
        buildSemanticInteger(4), new SgTypeInt(), false);
    return !SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
               sourceFunction, semanticFunction)
               ? 0
               : 1;
  } else if (mode == "invalid-integer-literal") {
    const std::string text = "12x";
    Fortran::parser::IntLiteralConstant literal(
        sourceBlock(text), std::optional<Fortran::parser::KindParam>{});
    SgExpression *expression = nullptr;
    Rose::builder::BuildImpl(literal, expression);
  } else if (mode == "integer-literal-range") {
    const std::string text = "9223372036854775808";
    Fortran::parser::IntLiteralConstant literal(
        sourceBlock(text), std::optional<Fortran::parser::KindParam>{});
    SgExpression *expression = nullptr;
    Rose::builder::BuildImpl(literal, expression);
  } else if (mode == "unsigned-literal-sign") {
    const std::string text = "-1";
    Fortran::parser::UnsignedLiteralConstant literal(
        sourceBlock(text), std::optional<Fortran::parser::KindParam>{});
    SgExpression *expression = nullptr;
    Rose::builder::BuildImpl(literal, expression);
  } else if (mode == "kind-param-range") {
    std::optional<Fortran::parser::KindParam> kind{
        Fortran::parser::KindParam{std::numeric_limits<std::uint64_t>::max()}};
    std::uint64_t value = 0;
    std::string spelling;
    Rose::builder::BuildImpl(kind, value, spelling);
  } else if (mode == "kind-star-range") {
    SgExpression *expression = nullptr;
    Rose::builder::BuildImpl(Fortran::parser::KindSelector::StarSize(
                                 std::numeric_limits<std::uint64_t>::max()),
                             expression);
  } else if (mode == "character-length-range") {
    SgExpression *expression = nullptr;
    Rose::builder::BuildImpl(
        Fortran::parser::CharLength(std::numeric_limits<std::uint64_t>::max()),
        expression);
  } else if (mode == "distinct-dynamic-length-result-type") {
    requireObjectContract(
        buildDynamicCharacterContract(new SgTypeInt(), true),
        buildDynamicCharacterContract(new SgTypeInt(), false));
  } else if (mode == "component-length-missing-containment") {
    const std::string statement = "character :: field*7";
    const std::string foreignName = "field";
    Fortran::parser::Name name{sourceBlock(foreignName), nullptr};
    Fortran::parser::CharLength sourceLength(std::uint64_t{7});
    SgExpression *length = SageBuilder::buildLongLongIntVal_nfi(7, "7");
    Rose::builder::detail::PublishExactFortranEntityCharacterLength(
        length, sourceLength, name, sourceBlock(statement));
  } else if (mode == "component-length-partial-position") {
    SgExpression *length = SageBuilder::buildLongLongIntVal_nfi(7, "7");
    setComponentLengthPositions(length, length, false);
    publishComponentLengthContract("character :: field*7", "field", length);
  } else if (mode == "component-length-foreign-position-owner") {
    SgExpression *length = SageBuilder::buildLongLongIntVal_nfi(7, "7");
    SgExpression *foreignOwner = SageBuilder::buildIntVal_nfi("0");
    setComponentLengthPositions(length, foreignOwner, true);
    publishComponentLengthContract("character :: field*7", "field", length);
  } else if (mode == "component-length-preclassified") {
    SgExpression *length = SageBuilder::buildLongLongIntVal_nfi(7, "7");
    setComponentLengthPositions(length, length, true);
    publishComponentLengthContract("character :: field*7", "field", length);
  } else if (mode == "missing-signed-real-sign") {
    const std::string statement = "1.0, 2.0";
    Rose::builder::detail::RequireExactFortranSignedRealSignSource(
        Fortran::parser::CharBlock(statement.data(), 3), sourceBlock(statement),
        Fortran::parser::Sign::Positive);
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "source-type contract unexpectedly returned\n";
  return 1;
}
