#include "BuildExprVisitor.h"
#include "RoseAst.h"
#include "rose.h"
#include "sage-build.h"

#include <flang/Evaluate/expression.h>
#include <flang/Evaluate/tools.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

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
  SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
  ROSE_ASSERT(arguments != nullptr);
  arguments->append_argument(argument);
  SgFunctionType *function = new SgFunctionType(result, false, arguments);
  ROSE_ASSERT(function != nullptr);
  function->set_fortran_source_syntax(sourceSyntax);
  ROSE_ASSERT(function->get_argument_list() == arguments &&
              arguments->get_parent() == function);
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
  if (argc < 2 ||
      (argc != 2 &&
       !(argc == 3 &&
         (std::string(argv[1]) == "valid-real-literals" ||
          std::string(argv[1]) == "valid-complex-integer-parts" ||
          std::string(argv[1]) == "valid-native-unsigned-literals" ||
          std::string(argv[1]) == "valid-native-boz-literals" ||
          std::string(argv[1]) == "valid-signed-integer-minimum" ||
          std::string(argv[1]) == "valid-wide-complex-integer-parts")))) {
    std::cerr << "usage: " << argv[0] << " MODE [SOURCE]\n";
    return 2;
  }
  const std::string mode = argv[1];
  if (mode == "valid-signed-integer-minimum") {
    SgProject *project = frontend(std::vector<std::string>{argv[0], argv[2]});
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    const std::array<int, 5> kinds{1, 2, 4, 8, 16};
    const std::array<std::int64_t, 4> narrowValues{INT8_MIN, INT16_MIN,
                                                   INT32_MIN, INT64_MIN};
    std::array<bool, 5> seen{};
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
      const auto *name = isSgInitializedName(node);
      for (std::size_t index = 0; index < kinds.size(); ++index) {
        if (name->get_name().getString() !=
            "minimum" + std::to_string(kinds[index]))
          continue;
        ROSE_ASSERT(!seen[index]);
        const auto *initializer =
            isSgAssignInitializer(name->get_initializer());
        ROSE_ASSERT(initializer != nullptr);
        const auto *value = isSgValueExp(initializer->get_operand());
        ROSE_ASSERT(value != nullptr && value->get_parent() == initializer);
        if (index == 4) {
          const auto *wide = isSgInteger128Val(value);
          ROSE_ASSERT(wide != nullptr);
          ROSE_ASSERT(wide->get_value() == Rose::Integer128Value::fromBits(
                                               {0, UINT64_C(1) << 63}, false));
        } else if (const auto *small = isSgIntVal(value)) {
          ROSE_ASSERT(small->get_value() == narrowValues[index]);
        } else {
          const auto *large = isSgLongLongIntVal(value);
          ROSE_ASSERT(large != nullptr &&
                      large->get_value() == narrowValues[index]);
        }
        const auto *type = isSgTypeInt(value->get_literal_type());
        ROSE_ASSERT(type != nullptr);
        const auto *kind = isSgIntVal(type->get_type_kind());
        ROSE_ASSERT(kind != nullptr && kind->get_value() == kinds[index]);
        seen[index] = true;
      }
    }
    ROSE_ASSERT(std::all_of(seen.begin(), seen.end(),
                            [](bool value) { return value; }));
    SageInterface::deleteAST(project);
    return 0;
  }
  if (mode == "valid-native-boz-literals") {
    SgProject *project = frontend(std::vector<std::string>{argv[0], argv[2]});
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    const std::array<Rose::Integer128Value::Words, 5> expected{
        {{UINT64_MAX, 0},
         {UINT64_C(0x0123456789abcdef), UINT64_C(0xfedcba9876543210)},
         {45, 0},
         {493, 0},
         {0, 0}}};
    std::array<int, 5> occurrences{};
    const auto values = NodeQuery::querySubTree(project, V_SgFortranBOZVal);
    ROSE_ASSERT(values.size() == 10);
    for (SgNode *node : values) {
      const auto *value = isSgFortranBOZVal(node);
      ROSE_ASSERT(value != nullptr && value->get_parent() != nullptr);
      ROSE_ASSERT(value->get_bits().isUnsigned());
      ROSE_ASSERT(value->get_type() == SgTypeFortranBOZ::createType());
      const auto words = value->get_bits().words();
      const auto found = std::find(expected.begin(), expected.end(), words);
      ROSE_ASSERT(found != expected.end());
      ++occurrences[found - expected.begin()];
    }
    ROSE_ASSERT(std::all_of(occurrences.begin(), occurrences.end(),
                            [](int count) { return count == 2; }));
    const auto dataValues =
        NodeQuery::querySubTree(project, V_SgDataStatementValue);
    ROSE_ASSERT(dataValues.size() == 5);
    for (SgNode *node : dataValues) {
      const auto *data = isSgDataStatementValue(node);
      const auto *literal = isSgFortranBOZVal(data->get_constant_expression());
      ROSE_ASSERT(literal != nullptr && literal->get_parent() == data);
    }
    SageInterface::deleteAST(project);
    return 0;
  }
  if (mode == "valid-native-unsigned-literals") {
    SgProject *project =
        frontend(std::vector<std::string>{argv[0], "-funsigned", argv[2]});
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    const std::array<std::string, 6> names{"u1", "u2",      "u4",
                                           "u8", "wrapped", "negated"};
    const std::array<std::uint64_t, 6> values{
        UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX, 0, 1};
    const std::array<int, 6> kinds{1, 2, 4, 8, 1, 8};
    std::array<bool, 6> seen{};
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
      auto *name = isSgInitializedName(node);
      const auto found =
          std::find(names.begin(), names.end(), name->get_name().getString());
      if (found == names.end())
        continue;
      const auto index = found - names.begin();
      ROSE_ASSERT(!seen[index]);
      auto *initializer = isSgAssignInitializer(name->get_initializer());
      ROSE_ASSERT(initializer != nullptr);
      auto *operand = initializer->get_operand_i();
      ROSE_ASSERT(operand != nullptr && operand->get_parent() == initializer);
      if (name->get_name() == "negated") {
        auto *minus = isSgMinusOp(operand);
        ROSE_ASSERT(minus != nullptr);
        operand = minus->get_operand();
        ROSE_ASSERT(operand != nullptr && operand->get_parent() == minus);
        ROSE_ASSERT(minus->get_type() == operand->get_type());
      }
      auto *value = isSgValueExp(operand);
      ROSE_ASSERT(value != nullptr);
      if (auto *narrow = isSgUnsignedIntVal(value)) {
        ROSE_ASSERT(narrow->get_value() == values[index]);
      } else {
        auto *wide = isSgUnsignedLongLongIntVal(value);
        ROSE_ASSERT(wide != nullptr && wide->get_value() == values[index]);
      }
      auto *type = isSgTypeUnsignedInt(value->get_literal_type());
      ROSE_ASSERT(type != nullptr);
      auto *kind = isSgIntVal(type->get_type_kind());
      ROSE_ASSERT(kind != nullptr && kind->get_value() == kinds[index]);
      seen[index] = true;
    }
    ROSE_ASSERT(std::all_of(seen.begin(), seen.end(),
                            [](bool value) { return value; }));
    SageInterface::deleteAST(project);
    return 0;
  }
  if (mode == "valid-wide-complex-integer-parts") {
    std::vector<std::string> arguments{argv[0], argv[2]};
    SgProject *project = frontend(arguments);
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    const auto complexes = NodeQuery::querySubTree(project, V_SgComplexVal);
    ROSE_ASSERT(complexes.size() == 1);
    auto *complex = isSgComplexVal(complexes.front());
    const std::array<Rose::Integer128Value::Words, 2> expected{
        Rose::Integer128Value::Words{UINT64_MAX, UINT64_C(0x7fffffffffffffff)},
        Rose::Integer128Value::Words{1, UINT64_C(0x8000000000000000)}};
    const std::array<SgExpression *, 2> parts{complex->get_real_value(),
                                              complex->get_imaginary_value()};
    for (std::size_t i = 0; i < parts.size(); ++i) {
      auto *integer = isSgInteger128Val(parts[i]);
      ROSE_ASSERT(integer != nullptr && integer->get_parent() == complex);
      auto *type = isSgTypeInt(integer->get_literal_type());
      ROSE_ASSERT(type != nullptr);
      auto *kind = isSgIntVal(type->get_type_kind());
      ROSE_ASSERT(kind != nullptr && kind->get_value() == 16);
      ROSE_ASSERT(integer->get_value() ==
                  Rose::Integer128Value::fromBits(expected[i], false));
    }
    std::size_t initializers = 0;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
      auto *name = isSgInitializedName(node);
      if (name->get_name() != "positive" && name->get_name() != "negative") {
        continue;
      }
      auto *initializer = isSgAssignInitializer(name->get_initializer());
      ROSE_ASSERT(initializer != nullptr && initializer->get_parent() == name);
      SgExpression *operand = initializer->get_operand();
      ROSE_ASSERT(operand != nullptr && operand->get_parent() == initializer);
      if (name->get_name() == "negative") {
        auto *minus = isSgMinusOp(operand);
        ROSE_ASSERT(minus != nullptr);
        operand = minus->get_operand();
        ROSE_ASSERT(operand != nullptr && operand->get_parent() == minus);
      }
      auto *integer = isSgInteger128Val(operand);
      ROSE_ASSERT(integer != nullptr);
      ROSE_ASSERT(integer->get_value() ==
                  Rose::Integer128Value::fromBits(expected[0], false));
      auto *type = isSgTypeInt(integer->get_literal_type());
      ROSE_ASSERT(type != nullptr);
      auto *kind = isSgIntVal(type->get_type_kind());
      ROSE_ASSERT(kind != nullptr && kind->get_value() == 16);
      ++initializers;
    }
    ROSE_ASSERT(initializers == 2);
    const auto dataValues =
        NodeQuery::querySubTree(project, V_SgDataStatementValue);
    ROSE_ASSERT(dataValues.size() == 2);
    for (SgNode *node : dataValues) {
      auto *value = isSgDataStatementValue(node);
      auto *repeat = isSgIntVal(value->get_repeat_expression());
      ROSE_ASSERT(repeat != nullptr && repeat->get_parent() == value);
      ROSE_ASSERT(repeat->get_value() == 2);
      auto *type = isSgTypeInt(repeat->get_literal_type());
      ROSE_ASSERT(type != nullptr);
      auto *kind = isSgIntVal(type->get_type_kind());
      ROSE_ASSERT(kind != nullptr && kind->get_value() == 8);
    }
    SageInterface::deleteAST(project);
    return 0;
  }
  if (mode == "valid-complex-integer-parts") {
    std::vector<std::string> arguments{argv[0], argv[2]};
    SgProject *project = frontend(arguments);
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    std::size_t integers = 0;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgComplexVal)) {
      auto *complex = isSgComplexVal(node);
      for (auto *part :
           {complex->get_real_value(), complex->get_imaginary_value()}) {
        if (auto *integer = isSgIntVal(part)) {
          auto *type = isSgTypeInt(integer->get_literal_type());
          ROSE_ASSERT(type != nullptr);
          auto *kind = isSgIntVal(type->get_type_kind());
          ROSE_ASSERT(kind != nullptr && kind->get_value() == 8);
          ROSE_ASSERT(integer->get_value() == 1 || integer->get_value() == 2);
          ROSE_ASSERT(integer->get_parent() == complex);
          ++integers;
        }
      }
    }
    ROSE_ASSERT(integers == 3);
    SageInterface::deleteAST(project);
    return 0;
  }
  if (mode == "valid-real-literals") {
    std::vector<std::string> arguments{argv[0], argv[2]};
    SgProject *project = frontend(arguments);
    ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
    const std::array<std::string, 8> names{"half",          "bfloat", "small",
                                           "dexp",          "named",  "wide",
                                           "negative_zero", "mixed"};
    const std::array<int, 9> kinds{2, 3, 4, 8, 8, 10, 8, 4, 8};
    const std::array<double, 9> values{0.5, 0.5, 1.25, 125.0, 0.125,
                                       2.5, 0.0, 1.0,  2.0};
    std::array<bool, 9> seen{};
    auto check = [&](SgExpression *expression, std::size_t index) {
      auto *literal = isSgValueExp(expression);
      ROSE_ASSERT(literal != nullptr && !seen[index]);
      switch (kinds[index]) {
      case 2: {
        auto *value = isSgFloat16Val(literal);
        ROSE_ASSERT(value != nullptr && value->get_value() == values[index]);
        break;
      }
      case 3: {
        auto *value = isSgBFloat16Val(literal);
        ROSE_ASSERT(value != nullptr && value->get_value() == values[index]);
        break;
      }
      case 4: {
        auto *value = isSgFloatVal(literal);
        ROSE_ASSERT(value != nullptr && value->get_value() == values[index]);
        break;
      }
      case 8: {
        auto *value = isSgDoubleVal(literal);
        ROSE_ASSERT(value != nullptr && value->get_value() == values[index]);
        break;
      }
      case 10: {
        auto *value = isSgFloat80Val(literal);
        ROSE_ASSERT(value != nullptr);
        const auto expected =
            Rose::FloatingValue(values[index])
                .converted(Rose::FloatingValue::Format::x87_binary80);
        ROSE_ASSERT(value->get_value() == expected);
        break;
      }
      default:
        ROSE_ABORT();
      }
      auto *type = isSgTypeFloat(literal->get_literal_type());
      auto *selector =
          type != nullptr ? isSgIntVal(type->get_type_kind()) : nullptr;
      ROSE_ASSERT(selector != nullptr && selector->get_value() == kinds[index]);
      seen[index] = true;
    };
    for (SgNode *node : RoseAst(project)) {
      auto *name = isSgInitializedName(node);
      if (name == nullptr)
        continue;
      const auto entry =
          std::find(names.begin(), names.end(), name->get_name().getString());
      if (entry == names.end())
        continue;
      const std::size_t index = entry - names.begin();
      auto *initializer = isSgAssignInitializer(name->get_initializer());
      ROSE_ASSERT(initializer != nullptr && initializer->get_parent() == name);
      auto *expression = initializer->get_operand();
      ROSE_ASSERT(expression != nullptr &&
                  expression->get_parent() == initializer);
      if (index == 6) {
        auto *negative = isSgMinusOp(expression);
        ROSE_ASSERT(negative != nullptr);
        auto *zero = isSgDoubleVal(negative->get_operand());
        ROSE_ASSERT(zero != nullptr && zero->get_parent() == negative);
        ROSE_ASSERT(!std::signbit(zero->get_value()) &&
                    std::signbit(-zero->get_value()));
        ROSE_ASSERT(SageInterface::isEquivalentType(negative->get_type(),
                                                    zero->get_type()));
        check(zero, index);
      } else if (index == 7) {
        auto *sum = isSgAddOp(expression);
        ROSE_ASSERT(sum != nullptr);
        ASSERT_not_null(sum->get_lhs_operand());
        ASSERT_not_null(sum->get_rhs_operand());
        ROSE_ASSERT(sum->get_lhs_operand()->get_parent() == sum &&
                    sum->get_rhs_operand()->get_parent() == sum);
        check(sum->get_lhs_operand(), 7);
        check(sum->get_rhs_operand(), 8);
      } else {
        check(expression, index);
      }
    }
    for (bool found : seen)
      ROSE_ASSERT(found);
    SageInterface::deleteAST(project);
    return 0;
  }
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
    std::string kindSpelling;
    Rose::builder::BuildImpl(kind, kindSpelling);
    ROSE_ASSERT(kindSpelling == std::to_string(maximum));

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
    auto checkUnsigned = [](auto kind, std::uint64_t expected,
                            std::uint64_t spelled) {
      constexpr int k = decltype(kind)::value;
      using Type =
          Fortran::evaluate::Type<Fortran::common::TypeCategory::Unsigned, k>;
      Fortran::parser::TypedExpr semantic(
          new Fortran::evaluate::GenericExprWrapper(
              Fortran::evaluate::AsGenericExpr(
                  Fortran::evaluate::Constant<Type>(
                      typename Type::Scalar(expected)))),
          Fortran::evaluate::GenericExprWrapper::Deleter);
      const std::string digits = std::to_string(spelled);
      Fortran::parser::UnsignedLiteralConstant literal(
          sourceBlock(digits), Fortran::parser::KindParam(std::uint64_t{k}));
      Rose::builder::BuildExprVisitor visitor(semantic);
      visitor.Walk(literal);
      SgExpression *expression = nullptr;
      visitor.get(expression);
      auto *value = isSgValueExp(expression);
      ROSE_ASSERT(value != nullptr);
      if (auto *narrow = isSgUnsignedIntVal(value)) {
        ROSE_ASSERT(narrow->get_value() == expected);
        ROSE_ASSERT(narrow->get_valueString() ==
                    digits + "u_" + std::to_string(k));
      } else {
        auto *wide = isSgUnsignedLongLongIntVal(value);
        ROSE_ASSERT(wide != nullptr && wide->get_value() == expected);
        ROSE_ASSERT(wide->get_valueString() ==
                    digits + "u_" + std::to_string(k));
      }
      auto *type = isSgTypeUnsignedInt(value->get_literal_type());
      ROSE_ASSERT(type != nullptr);
      auto *selector = isSgIntVal(type->get_type_kind());
      ROSE_ASSERT(selector != nullptr && selector->get_value() == k);
      SageInterface::deleteAST(value);
    };
    checkUnsigned(std::integral_constant<int, 1>{}, UINT8_MAX, UINT8_MAX);
    checkUnsigned(std::integral_constant<int, 2>{}, UINT16_MAX, UINT16_MAX);
    checkUnsigned(std::integral_constant<int, 4>{}, UINT32_MAX, UINT32_MAX);
    checkUnsigned(std::integral_constant<int, 8>{}, UINT64_MAX, UINT64_MAX);
    checkUnsigned(std::integral_constant<int, 1>{}, 0, 256);
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
  } else if (mode == "literal-category-before-value") {
    const std::string text = "12x";
    Fortran::parser::IntLiteralConstant literal(
        sourceBlock(text), std::optional<Fortran::parser::KindParam>{});
    Rose::builder::BuildExprVisitor visitor{SageBuilder::buildBoolType()};
    visitor.Walk(literal);
  } else if (mode == "kind-param-range") {
    std::optional<Fortran::parser::KindParam> kind{
        Fortran::parser::KindParam{std::numeric_limits<std::uint64_t>::max()}};
    std::string spelling;
    Rose::builder::BuildImpl(kind, spelling);
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
