#include "RoseAst.h"
#include "rose.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::string literalSpelling(SgValueExp *value) {
  if (SgIntVal *literal = isSgIntVal(value)) {
    return literal->get_valueString();
  }
  if (SgCharVal *literal = isSgCharVal(value)) {
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

std::string userDefinedLiteralOperandSpelling(SgExpression *expression) {
  if (SgStringVal *literal = isSgStringVal(expression)) {
    return literal->get_cxx_literal_spelling();
  }
  return literalSpelling(isSgValueExp(expression));
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
        "0x2a", "052", "1'000", "'\\x41'", "0x1.4p+0F", "1.25L"};
    if (expected.count(spelling) == 0) {
      return;
    }

    ROSE_ASSERT(!source->isTransformation());
    ROSE_ASSERT(!source->isCompilerGenerated());
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

    static const std::map<std::string, std::string> expected{
        {"0X2A_rex_integer", "0X2A"},
        {"0x1.ABp+2_rex_floating", "0x1.ABp+2"},
        {"'\\x41'_rex_character", "'\\x41'"},
        {"u8R\"rex(left right)rex\"_rex_text", "u8R\"rex(left right)rex\""},
        {"1'2'3_rex_raw", "1'2'3"},
        {"0b1010_rex_template", "0b1010"}};
    const std::string spelling = call->unparseToString();
    auto target = expected.find(spelling);
    if (target == expected.end()) {
      return;
    }

    SgExprListExp *arguments = call->get_args();
    ROSE_ASSERT(arguments != nullptr);
    if (spelling == "0b1010_rex_template") {
      ROSE_ASSERT(arguments->get_expressions().empty());
    } else {
      ROSE_ASSERT(!arguments->get_expressions().empty());
    }
    SgExprListExp *lexical = call->get_source_user_defined_literal_operands();
    ROSE_ASSERT(lexical != nullptr);
    ROSE_ASSERT(lexical->get_parent() == call);
    ROSE_ASSERT(!lexical->get_expressions().empty());
    SgExpression *source_operand = lexical->get_expressions().front();
    ROSE_ASSERT(userDefinedLiteralOperandSpelling(source_operand) ==
                target->second);
    ROSE_ASSERT(source_operand->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(!source_operand->get_startOfConstruct()->isCompilerGenerated());
    requireExactSemanticSubtree(call->get_function());
    requireExactSemanticSubtree(arguments);
    if (spelling == "u8R\"rex(left right)rex\"_rex_text") {
      ROSE_ASSERT(arguments->get_expressions().size() == 2);
      SgUnsignedLongVal *length =
          isSgUnsignedLongVal(arguments->get_expressions()[1]);
      ROSE_ASSERT(length != nullptr);
      ROSE_ASSERT(length->get_valueString() == "10UL");
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
  const std::set<std::string> expected{"0x2a",    "052",       "1'000",
                                       "'\\x41'", "0x1.4p+0F", "1.25L"};
  ROSE_ASSERT(verifier.found == expected);
  const std::set<std::string> expectedUserDefinedLiterals{
      "0X2A_rex_integer",      "0x1.ABp+2_rex_floating",
      "'\\x41'_rex_character", "u8R\"rex(left right)rex\"_rex_text",
      "1'2'3_rex_raw",         "0b1010_rex_template"};
  ROSE_ASSERT(verifier.found_user_defined_literals ==
              expectedUserDefinedLiterals);

  return backend(project);
}
