#include "rose.h"

#include <string>

namespace {

constexpr const char *kFilename = "rex_unparser_source_provenance_contract.cpp";

template <class Expression>
Expression *markTransformation(Expression *expression) {
  ROSE_ASSERT(expression != nullptr);
  SageInterface::setSourcePositionForTransformation(expression);
  return expression;
}

template <class Expression>
Expression *markSource(Expression *expression, int line) {
  ROSE_ASSERT(expression != nullptr);
  expression->set_file_info(new Sg_File_Info(kFilename, line, 1));
  expression->set_startOfConstruct(new Sg_File_Info(kFilename, line, 1));
  expression->set_endOfConstruct(new Sg_File_Info(kFilename, line, 2));
  expression->get_file_info()->set_parent(expression);
  expression->get_startOfConstruct()->set_parent(expression);
  expression->get_endOfConstruct()->set_parent(expression);
  return expression;
}

SgIntVal *buildTypedSourceOwner() {
  SgIntVal *owner = markTransformation(SageBuilder::buildIntVal_nfi(7, "7"));
  SgOmpSourceExpression *source =
      markTransformation(new SgOmpSourceExpression("REX_TYPED_SOURCE(7)"));
  owner->set_originalExpressionTree(source);
  source->set_parent(owner);
  return owner;
}

void unparseWithCxxContext(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  struct EmissionContext {
    SgSourceFile *source_file = nullptr;
    SgBasicBlock *body = nullptr;
  };
  static EmissionContext context = [] {
    EmissionContext result;
    SgSourceFile *file = SageBuilder::buildGeneratedSourceFile(kFilename);
    ROSE_ASSERT(file != nullptr);
    ROSE_ASSERT(file->get_globalScope() != nullptr);
    ROSE_ASSERT(file->get_project() != nullptr);
    file->set_Cxx_only(true);
    file->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *global = file->get_globalScope();
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_source_provenance_emission_scope"),
            SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), global);
    ROSE_ASSERT(function != nullptr);
    SgFunctionDeclaration *prototype =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(prototype != nullptr && prototype != function);
    ROSE_ASSERT(function->get_definition() != nullptr);
    result.source_file = file;
    result.body = function->get_definition()->get_body();
    ROSE_ASSERT(result.body != nullptr);
    return result;
  }();

  SgExprStatement *statement = SageBuilder::buildExprStatement(expression);
  ROSE_ASSERT(statement != nullptr);
  SageInterface::appendStatement(statement, context.body);
  ROSE_ASSERT(expression->get_parent() == statement);

  SgUnparse_Info info;
  info.set_current_source_file(context.source_file);
  info.set_current_scope(context.body);
  info.set_language(SgFile::e_Cxx_language);
  info.set_template_argument_qualification_context(statement);
  (void)expression->unparseToString(&info);
}

int exercisePositiveContracts() {
  SgIntVal *owner = buildTypedSourceOwner();
  if (owner->unparseToString() != "REX_TYPED_SOURCE(7)") {
    return 10;
  }

  SgIntVal *copy = isSgIntVal(SageInterface::copyExpression(owner));
  if (copy == nullptr || copy == owner ||
      copy->get_originalExpressionTree() == nullptr ||
      copy->get_originalExpressionTree() ==
          owner->get_originalExpressionTree() ||
      copy->get_originalExpressionTree()->get_parent() != copy ||
      copy->unparseToString() != "REX_TYPED_SOURCE(7)") {
    return 11;
  }

  SgIntVal *annotated =
      markTransformation(SageBuilder::buildIntVal_nfi(9, "9"));
  unparseWithCxxContext(annotated);
  PreprocessingInfo *before = SageInterface::attachComment(
      annotated, "rex-before", PreprocessingInfo::C_StyleComment,
      PreprocessingInfo::before);
  PreprocessingInfo *after = SageInterface::attachComment(
      annotated, "rex-after", PreprocessingInfo::C_StyleComment,
      PreprocessingInfo::after);
  if (before == nullptr || after == nullptr ||
      before->getRelativePosition() != PreprocessingInfo::before ||
      after->getRelativePosition() != PreprocessingInfo::after) {
    return 12;
  }
  const std::string annotated_text = annotated->unparseToCompleteString();
  const std::size_t before_position = annotated_text.find("rex-before");
  const std::size_t value_position = annotated_text.find('9');
  const std::size_t after_position = annotated_text.find("rex-after");
  if (before_position == std::string::npos ||
      value_position == std::string::npos ||
      after_position == std::string::npos ||
      before_position >= value_position || value_position >= after_position) {
    return 13;
  }

  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    return exercisePositiveContracts();
  }
  if (argc != 2) {
    return 2;
  }

  const std::string mode = argv[1];
  if (mode == "original-missing-owner") {
    SgIntVal *owner = markTransformation(SageBuilder::buildIntVal_nfi(7, "7"));
    SgOmpSourceExpression *source =
        markTransformation(new SgOmpSourceExpression("REX_SOURCE"));
    owner->set_originalExpressionTree(source);
    unparseWithCxxContext(owner);
    return 0;
  }
  if (mode == "original-chain") {
    SgIntVal *owner = markTransformation(SageBuilder::buildIntVal_nfi(7, "7"));
    SgIntVal *source = markTransformation(SageBuilder::buildIntVal_nfi(8, "8"));
    SgOmpSourceExpression *nested =
        markTransformation(new SgOmpSourceExpression("REX_NESTED"));
    source->set_originalExpressionTree(nested);
    nested->set_parent(source);
    owner->set_originalExpressionTree(source);
    source->set_parent(owner);
    unparseWithCxxContext(owner);
    return 0;
  }
  if (mode == "original-semantic-lowering") {
    SgIntVal *lhs = markTransformation(SageBuilder::buildIntVal_nfi(1, "1"));
    SgIntVal *rhs = markTransformation(SageBuilder::buildIntVal_nfi(2, "2"));
    SgAddOp *owner = markTransformation(
        SageBuilder::buildAddOp(lhs, rhs, SageBuilder::buildIntType()));
    SgFunctionCallExp *semantic_call = markTransformation(new SgFunctionCallExp(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExprListExp *>(nullptr), static_cast<SgType *>(nullptr)));
    owner->set_originalExpressionTree(semantic_call);
    semantic_call->set_parent(owner);
    unparseWithCxxContext(owner);
    return 0;
  }
  if (mode == "original-cast-role") {
    SgIntVal *operand =
        markTransformation(SageBuilder::buildIntVal_nfi(1, "1"));
    SgCastExp *owner = markTransformation(SageBuilder::buildCastExp(
        operand, SageBuilder::buildIntType(), SgCastExp::e_C_style_cast));
    SgIntVal *source = markTransformation(SageBuilder::buildIntVal_nfi(1, "1"));
    owner->set_originalExpressionTree(source);
    source->set_parent(owner);
    unparseWithCxxContext(owner);
    return 0;
  }
  if (mode == "original-source-range") {
    SgIntVal *owner = markSource(SageBuilder::buildIntVal_nfi(7, "7"), 1);
    SgIntVal *source = markSource(SageBuilder::buildIntVal_nfi(8, "8"), 2);
    owner->set_originalExpressionTree(source);
    source->set_parent(owner);
    unparseWithCxxContext(owner);
    return 0;
  }

  return 3;
}
