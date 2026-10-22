#include "RoseAst.h"
#include "rose.h"

#include <set>
#include <string>
#include <vector>

namespace {

std::string literalSpelling(SgValueExp *value) {
  if (SgIntVal *literal = isSgIntVal(value)) {
    return literal->get_valueString();
  }
  if (SgCharVal *literal = isSgCharVal(value)) {
    const int64_t actual = literal->get_value();
    if (literal->get_valueString() == "'AB'")
      ROSE_ASSERT(actual == 16706 && isSgTypeInt(literal->get_type()));
    if (literal->get_valueString() == "u8'\\xC3'")
      ROSE_ASSERT(actual == 195 && isSgTypeChar8(literal->get_type()));
    return literal->get_valueString();
  }
  if (SgWcharVal *literal = isSgWcharVal(value)) {
    ROSE_ASSERT(literal->get_value() == 0x03a9);
    return literal->get_valueString();
  }
  if (SgChar16Val *literal = isSgChar16Val(value)) {
    ROSE_ASSERT(literal->get_value() == 0x03a9);
    return literal->get_valueString();
  }
  if (SgChar32Val *literal = isSgChar32Val(value)) {
    ROSE_ASSERT(literal->get_value() == 0x1f642);
    return literal->get_valueString();
  }
  if (SgFloat16Val *literal = isSgFloat16Val(value)) {
    ROSE_ASSERT(literal->get_value() == 1.25f);
    return literal->get_valueString();
  }
  if (SgFloatVal *literal = isSgFloatVal(value)) {
    return literal->get_valueString();
  }
  if (SgLongDoubleVal *literal = isSgLongDoubleVal(value)) {
    return literal->get_valueString();
  }
  if (SgUnsignedLongLongIntVal *literal = isSgUnsignedLongLongIntVal(value)) {
    return literal->get_valueString();
  }
  return "";
}

void requireExactSemanticSubtree(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  for (SgNode *node : RoseAst(root)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr) {
      continue;
    }
    std::vector<Sg_File_Info *> positions{located->get_file_info(),
                                          located->get_startOfConstruct(),
                                          located->get_endOfConstruct()};
    if (SgExpression *expression = isSgExpression(located)) {
      positions.push_back(expression->get_operatorPosition());
    }
    for (Sg_File_Info *position : positions) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == located);
      ROSE_ASSERT(position->isCompilerGenerated());
      ROSE_ASSERT(position->isFrontendSpecific());
      ROSE_ASSERT(!position->isTransformation());
      ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(position->isOutputInCodeGeneration());
      ROSE_ASSERT(position->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(position->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    }
    if (SgValueExp *value = isSgValueExp(located)) {
      ROSE_ASSERT(value->get_literal_spelling_form() ==
                  SgValueExp::e_literal_canonical_generated);
      if (auto *character = isSgCharVal(value))
        ROSE_ASSERT(character->get_valueString().empty());
      if (auto *integer = isSgUnsignedLongLongIntVal(value))
        ROSE_ASSERT(integer->get_valueString().empty());
      if (auto *floating = isSgLongDoubleVal(value))
        ROSE_ASSERT(floating->get_valueString().empty());
    }
  }
}

class LiteralSpellingVerifier : public AstSimpleProcessing {
public:
  std::set<std::string> found;
  std::set<std::string> found_user_defined_literals;

  void visit(SgNode *node) override {
    if (SgFunctionCallExp *call = isSgFunctionCallExp(node)) {
      verifyUserDefinedLiteral(call);
    }

    SgValueExp *value = isSgValueExp(node);
    if (value == nullptr) {
      return;
    }

    const Sg_File_Info *source = value->get_startOfConstruct();
    if (source == nullptr ||
        source->get_filenameString().find(
            "rex_frontend_literal_spelling.cpp") == std::string::npos) {
      return;
    }

    const std::string spelling = literalSpelling(value);
    static const std::set<std::string> expected{
        "0x2a",           "052",     "1'000",      "'\\x41'",
        "0x1.4p+0F",      "1.25L",   "L'\\u03a9'", "u'\\u03a9'",
        "U'\\U0001f642'", "1.25f16", "'AB'",       "u8'\\xC3'"};
    if (expected.count(spelling) == 0) {
      return;
    }

    ROSE_ASSERT(!source->isTransformation());
    ROSE_ASSERT(!source->isCompilerGenerated());
    auto *file = SageInterface::getEnclosingSourceFile(value);
    ROSE_ASSERT(file != nullptr && file->get_target_abi().available);
    const auto &scalars = file->get_target_abi().scalars;
    const auto layout = scalars.find({value->get_type()->variantT(), 0});
    ROSE_ASSERT(layout != scalars.end());
    ROSE_ASSERT(layout->second.size_bits > 0 &&
                layout->second.alignment_bits > 0);
    switch (value->get_type()->variantT()) {
    case V_SgTypeChar8:
    case V_SgTypeChar16:
    case V_SgTypeChar32:
      ROSE_ASSERT(layout->second.is_unsigned);
      break;
    case V_SgTypeInt:
    case V_SgTypeFloat:
    case V_SgTypeLongDouble:
    case V_SgTypeFloat16:
      ROSE_ASSERT(!layout->second.is_unsigned);
      break;
    default:
      break;
    }
    ROSE_ASSERT(value->unparseToString() == spelling);
    found.insert(spelling);
  }

private:
  void verifyUserDefinedLiteral(SgFunctionCallExp *call) {
    ROSE_ASSERT(call != nullptr);
    const Sg_File_Info *source = call->get_startOfConstruct();
    if (source == nullptr ||
        source->get_filenameString().find(
            "rex_frontend_literal_spelling.cpp") == std::string::npos) {
      return;
    }

    static const std::set<std::string> expected{
        "0X2A_rex_integer",      "0x1.ABp+2_rex_floating",
        "'\\x41'_rex_character", "u8R\"rex(left right)rex\"_rex_text",
        "1'2'3_rex_raw",         "0b1010_rex_template"};
    const std::string spelling = call->unparseToString();
    if (expected.count(spelling) == 0) {
      return;
    }

    SgExprListExp *arguments = call->get_args();
    ROSE_ASSERT(arguments != nullptr);
    if (spelling == "0b1010_rex_template") {
      ROSE_ASSERT(arguments->get_expressions().empty());
    } else {
      ROSE_ASSERT(!arguments->get_expressions().empty());
    }
    SgCxxLiteralTokenList *lexical =
        call->get_source_user_defined_literal_operands();
    ROSE_ASSERT(lexical != nullptr);
    ROSE_ASSERT(lexical->get_parent() == call);
    ROSE_ASSERT(!lexical->get_tokens().empty());
    SgCxxLiteralToken *source_operand = lexical->get_tokens().front();
    ROSE_ASSERT(source_operand->get_spelling() == spelling);
    ROSE_ASSERT(source_operand->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(!source_operand->get_startOfConstruct()->isCompilerGenerated());
    requireExactSemanticSubtree(call->get_function());
    requireExactSemanticSubtree(arguments);
    if (spelling == "u8R\"rex(left right)rex\"_rex_text") {
      ROSE_ASSERT(arguments->get_expressions().size() == 2);
      SgUnsignedLongVal *length =
          isSgUnsignedLongVal(arguments->get_expressions()[1]);
      ROSE_ASSERT(length != nullptr);
      ROSE_ASSERT(length->get_value() == 10);
      ROSE_ASSERT(isSgTypeUnsignedLong(length->get_type()));
      ROSE_ASSERT(length->get_valueString().empty());
      ROSE_ASSERT(length->get_startOfConstruct() != nullptr);
      ROSE_ASSERT(length->get_startOfConstruct()->isCompilerGenerated());
    }
    ROSE_ASSERT(call->get_uses_operator_syntax());
    found_user_defined_literals.insert(spelling);
  }
};

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  LiteralSpellingVerifier verifier;
  verifier.traverse(project, preorder);
  const std::set<std::string> expected{
      "0x2a",           "052",     "1'000",      "'\\x41'",
      "0x1.4p+0F",      "1.25L",   "L'\\u03a9'", "u'\\u03a9'",
      "U'\\U0001f642'", "1.25f16", "'AB'",       "u8'\\xC3'"};
  ROSE_ASSERT(verifier.found == expected);
  const std::set<std::string> expectedUserDefinedLiterals{
      "0X2A_rex_integer",      "0x1.ABp+2_rex_floating",
      "'\\x41'_rex_character", "u8R\"rex(left right)rex\"_rex_text",
      "1'2'3_rex_raw",         "0b1010_rex_template"};
  ROSE_ASSERT(verifier.found_user_defined_literals ==
              expectedUserDefinedLiterals);

  return backend(project);
}
