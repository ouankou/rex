#include "rose.h"

#include <limits>
#include <memory>
#include <string>

template <class Literal>
static void checkMutation(const std::string &spelling, int64_t value = 2) {
  auto literal = std::make_unique<Literal>(1, spelling);
  ROSE_ASSERT(literal->get_value() == 1);
  ROSE_ASSERT(literal->get_valueString() == spelling);
  literal->set_value(value);
  ROSE_ASSERT(literal->get_value() == value);
  ROSE_ASSERT(literal->get_valueString().empty());
  ROSE_ASSERT(literal->get_literal_spelling_form() ==
              SgValueExp::e_literal_canonical_generated);
  ROSE_ASSERT(literal->get_isModified());
}

int main() {
  auto existing = std::make_unique<SgIntVal>(1, "1");
  std::unique_ptr<SgIntVal> outerNode;
  std::unique_ptr<SgIntVal> innerNode;
  {
    SgNode::SourceStateConstruction outer;
    outerNode = std::make_unique<SgIntVal>(1, "1");
    {
      SgNode::SourceStateConstruction inner;
      innerNode = std::make_unique<SgIntVal>(1, "1");
      existing->set_value(2);
      outerNode->set_value(2);
      innerNode->set_value(2);
      ROSE_ASSERT(existing->get_isModified());
      ROSE_ASSERT(!outerNode->get_isModified());
      ROSE_ASSERT(!innerNode->get_isModified());
    }
    innerNode->set_value(3);
    ROSE_ASSERT(!innerNode->get_isModified());
  }
  outerNode->set_value(4);
  innerNode->set_value(4);
  ROSE_ASSERT(outerNode->get_isModified());
  ROSE_ASSERT(innerNode->get_isModified());
  {
    const auto mode = SageBuilder::getSourcePositionClassificationMode();
    SageBuilder::setSourcePositionClassificationMode(
        SageBuilder::e_sourcePositionTransformation);
    std::unique_ptr<SgIntVal> edited(SageBuilder::buildIntVal(1));
    edited->set_value(2);
    ROSE_ASSERT(edited->get_isModified());
    const auto sequence = edited->get_lastMutationSequence();
    AstPostProcessing(edited.get());
    ROSE_ASSERT(edited->get_isModified());
    ROSE_ASSERT(edited->get_lastMutationSequence() == sequence);
    auto *file = SageBuilder::buildGeneratedSourceFile(
        "rex_literal_typed_mutation_output.cpp");
    const auto originalAbi = file->get_target_abi();
    SgSourceFile::target_abi_info abi;
    abi.available = true;
    abi.scalars[{V_SgTypeInt, 0}] = {32, 32, 0, 0, false};
    abi.scalars[{V_SgTypeDouble, 0}] = {64, 64, 53, 1024, false};
    file->set_target_abi(abi);
    ROSE_ASSERT(SageInterface::requireTargetScalarSizeBits(
                    SageBuilder::buildIntType(), file) == 32);
    ROSE_ASSERT(SageInterface::requireTargetScalarSizeBits(
                    SageBuilder::buildDoubleType(), file) == 64);
    bool isUnsigned = true;
    unsigned width = 0;
    SageInterface::requireTargetIntegerDomain(SageBuilder::buildIntType(), file,
                                              isUnsigned, width);
    ROSE_ASSERT(!isUnsigned && width == 32);
    unsigned precision = 0;
    int maxExponent = 0;
    SageInterface::requireTargetFloatingDomain(SageBuilder::buildDoubleType(),
                                               file, precision, maxExponent);
    ROSE_ASSERT(precision == 53 && maxExponent == 1024);
    file->set_target_abi(originalAbi);
    auto *scope = file->get_globalScope();
    auto *initializer = SageBuilder::buildAssignInitializer(
        edited.release(), SageBuilder::buildIntType());
    auto *declaration = SageBuilder::buildVariableDeclaration(
        "rex_edited_initializer", SageBuilder::buildIntType(), initializer,
        scope);
    auto *value = isSgIntVal(initializer->get_operand_i());
    ROSE_ASSERT(value != nullptr && value->get_isModified());
    ROSE_ASSERT(value->get_lastMutationSequence() == sequence);
    SageInterface::appendStatement(declaration, scope);
    for (bool declarationBody : {false, true}) {
      SgWhileStmt *loop = nullptr;
      SgInitializedName *local = nullptr;
      {
        SgNode::SourceStateConstruction construction;
        auto *function = SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            declarationBody ? "rex_decl_body_edit" : "rex_body_edit",
            SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), scope);
        loop = SageBuilder::buildWhileStmt(SageBuilder::buildBoolValExp(true),
                                           SageBuilder::buildNullStatement());
        SageInterface::appendStatement(loop,
                                       function->get_definition()->get_body());
        if (declarationBody) {
          auto *oldBody = loop->get_body();
          auto *body = SageBuilder::buildVariableDeclaration(
              "rex_loop_local", SageBuilder::buildIntType(), nullptr, loop);
          local = SageInterface::getFirstInitializedName(body);
          loop->set_body(body);
          body->set_parent(loop);
          oldBody->set_parent(nullptr);
          SageInterface::deleteAST(oldBody);
        }
      }
      auto *body = loop->get_body();
      ROSE_ASSERT(!loop->get_isModified());
      SageInterface::changeAllBodiesToBlocks(loop, false);
      auto *wrapper = isSgBasicBlock(loop->get_body());
      ROSE_ASSERT(wrapper != nullptr && wrapper->get_statements().size() == 1);
      ROSE_ASSERT(wrapper->get_statements().front() == body);
      ROSE_ASSERT(body->get_parent() == wrapper);
      ROSE_ASSERT(loop->get_isModified());
      auto *comment =
          SageInterface::attachComment(wrapper, "REX normalization comment",
                                       PreprocessingInfo::C_StyleComment);
      if (local != nullptr) {
        ROSE_ASSERT(local->get_scope() == wrapper);
        ROSE_ASSERT(wrapper->lookup_variable_symbol(local->get_name()) !=
                    nullptr);
      }
      SageInterface::cleanupNontransformedBasicBlockNode();
      ROSE_ASSERT(loop->get_body() == body && body->get_parent() == loop);
      ROSE_ASSERT(!SgNode::isLiveNode(wrapper));
      ROSE_ASSERT(loop->get_isModified());
      auto *comments = body->getAttachedPreprocessingInfo();
      ROSE_ASSERT(comments != nullptr && comments->size() == 1);
      ROSE_ASSERT(comments->front() == comment);
      if (local != nullptr) {
        ROSE_ASSERT(local->get_scope() == loop);
        auto *symbol = loop->lookup_variable_symbol(local->get_name());
        ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == local);
      }
      SageInterface::cleanupNontransformedBasicBlockNode();
      SageInterface::changeAllBodiesToBlocks(loop, false);
    }
    SageInterface::deleteAST(SageInterface::getProject(file));
    SageInterface::cleanupNontransformedBasicBlockNode();
    SageBuilder::setSourcePositionClassificationMode(mode);
  }
  checkMutation<SgCharVal>("'\\1'");
  checkMutation<SgWcharVal>("L'\\1'", std::numeric_limits<uint32_t>::max());
  checkMutation<SgSignedCharVal>("'\\1'");
  checkMutation<SgUnsignedCharVal>("'\\1'");
  checkMutation<SgShortVal>("01");
  checkMutation<SgUnsignedShortVal>("01");
  checkMutation<SgIntVal>("0x1");
  checkMutation<SgUnsignedIntVal>("0x1U");
  checkMutation<SgLongIntVal>("01L");
  checkMutation<SgUnsignedLongVal>("01UL");
  checkMutation<SgLongLongIntVal>("01LL");
  checkMutation<SgUnsignedLongLongIntVal>("01ULL");
  checkMutation<SgFloatVal>("1.0f");
  checkMutation<SgDoubleVal>("1.0");
  checkMutation<SgLongDoubleVal>("1.0L");
  checkMutation<SgFloat80Val>("1.0L");
  checkMutation<SgFloat128Val>("1.0Q");
  checkMutation<SgBFloat16Val>("1.0bf16");
  checkMutation<SgFloat16Val>("1.0f16");
  checkMutation<SgFloat32Val>("1.0f32");
  checkMutation<SgFloat64Val>("1.0f64");
  checkMutation<SgChar16Val>("u'\\1'");
  checkMutation<SgChar32Val>("U'\\1'");
  checkMutation<SgWcharVal>("L'\\1'");
}
