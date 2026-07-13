#include "rose.h"

#include "unparseCxx.h"
#include "unparser.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace {

Sg_File_Info *semanticFileInfo(bool implicit_cast) {
  Sg_File_Info *info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  info->setOutputInCodeGeneration();
  if (implicit_cast) {
    info->setImplicitCast();
  }
  return info;
}

void markSemanticNode(SgLocatedNode *node, bool implicit_cast = false) {
  ROSE_ASSERT(node != nullptr);
  node->set_file_info(semanticFileInfo(implicit_cast));
  node->set_startOfConstruct(semanticFileInfo(implicit_cast));
  node->set_endOfConstruct(semanticFileInfo(implicit_cast));
  if (SgExpression *expression = isSgExpression(node)) {
    expression->set_operatorPosition(semanticFileInfo(implicit_cast));
  }
}

std::string compact(std::string value) {
  value.erase(
      std::remove_if(value.begin(), value.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      value.end());
  return value;
}

struct RenderContext {
  SgSourceFile *source_file;
  SgGlobal *global;
  SgBasicBlock *scope;
  SgNullStatement *use_site;

  RenderContext()
      : source_file(SageBuilder::buildGeneratedSourceFile(
            "rex_unparser_cxx_expression_role_contract.cpp")),
        global(source_file != nullptr ? source_file->get_globalScope()
                                      : nullptr),
        scope(nullptr), use_site(nullptr) {
    ROSE_ASSERT(source_file != nullptr);
    ROSE_ASSERT(global != nullptr);
    source_file->set_Cxx_only(true);
    source_file->set_outputLanguage(SgFile::e_Cxx_language);
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_expression_role_context"), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList_nfi(), global);
    ROSE_ASSERT(function != nullptr);
    ROSE_ASSERT(function->get_definition() != nullptr);
    scope = function->get_definition()->get_body();
    ROSE_ASSERT(scope != nullptr);
    advanceUseSite();
  }

  void advanceUseSite() {
    use_site = SageBuilder::buildNullStatement();
    SageInterface::appendStatement(use_site, scope);
  }
};

RenderContext &renderContext() {
  static RenderContext context;
  return context;
}

std::string render(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  RenderContext &context = renderContext();
  Unparser_Opt options;
  std::ostringstream output;
  Unparser unparser(&output, "rex_unparser_cxx_expression_role_contract.cpp",
                    options);
  unparser.currentFile = context.source_file;
  NameQualificationContext &qualification =
      unparser.get_name_qualification_context();
  qualification.recordType(expression, context.use_site, {"", 0, false, false});
  for (SgNode *node : NodeQuery::querySubTree(expression, V_SgVarRefExp)) {
    qualification.recordName(node, context.use_site, {"", 0, false, false});
  }
  if (SgTypeTraitBuiltinOperator *builtin =
          isSgTypeTraitBuiltinOperator(expression)) {
    for (SgNode *argument : builtin->get_args()) {
      if (SgExpression *argument_expression = isSgExpression(argument)) {
        qualification.recordName(argument_expression, context.use_site,
                                 {"", 0, false, false});
        if (isSgTypeExpression(argument_expression) != nullptr) {
          qualification.recordType(argument_expression, context.use_site,
                                   {"", 0, false, false});
        }
      }
    }
  }
  for (SgNode *node :
       NodeQuery::querySubTree(expression, V_SgMemberFunctionRefExp)) {
    qualification.recordName(node, context.use_site, {"", 0, false, false});
  }
  SgUnparse_Info info;
  info.set_language(SgFile::e_Cxx_language);
  info.set_current_source_file(context.source_file);
  info.set_current_scope(context.scope);
  info.set_template_argument_qualification_context(context.use_site);
  unparser.u_exprStmt->unparseExpression(expression, info);
  return compact(output.str());
}

SgTypeTraitBuiltinOperator *buildOffsetof(std::string *expected = nullptr) {
  static unsigned class_index = 0;
  RenderContext &context = renderContext();
  const std::string class_name = "RexOffsetof" + std::to_string(++class_index);
  SgClassDeclaration *class_declaration = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), class_name,
      context.scope);
  ROSE_ASSERT(class_declaration != nullptr);
  SgClassDefinition *class_definition = class_declaration->get_definition();
  ROSE_ASSERT(class_definition != nullptr);
  SgVariableDeclaration *field_declaration =
      SageBuilder::buildVariableDeclaration(
          "member", SageBuilder::buildIntType(), nullptr, class_definition);
  SageInterface::appendStatement(field_declaration, class_definition);
  SgVariableSymbol *symbol = class_definition->lookup_variable_symbol("member");
  ROSE_ASSERT(symbol != nullptr);
  SgVarRefExp *reference = SageBuilder::buildVarRefExp(symbol);
  markSemanticNode(reference);
  SgTypeExpression *type_operand =
      SageBuilder::buildTypeExpression(class_declaration->get_type());
  markSemanticNode(type_operand);
  SgExpressionPtrList arguments{type_operand, reference};
  SgTypeTraitBuiltinOperator *builtin =
      SageBuilder::buildTypeTraitBuiltinOperator(
          "__builtin_offsetof", SgTypeTraitBuiltinOperator::e_offsetof_builtin,
          SageBuilder::buildUnsignedLongType(), arguments);
  markSemanticNode(builtin);
  context.advanceUseSite();
  if (expected != nullptr) {
    *expected = "__builtin_offsetof(" + class_name + ",member)";
  }
  return builtin;
}

SgFunctionCallExp *buildImplicitConversion(std::string *expected = nullptr) {
  static unsigned class_index = 0;
  RenderContext &context = renderContext();
  SgBasicBlock *scope = context.scope;
  const std::string class_name =
      "RexConversion" + std::to_string(++class_index);
  SgClassDeclaration *class_declaration = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), class_name, scope);
  ROSE_ASSERT(class_declaration != nullptr);
  ROSE_ASSERT(class_declaration->get_definition() != nullptr);
  SgMemberFunctionDeclaration *declaration =
      SageBuilder::buildNondefiningMemberFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          "operator int", SageBuilder::buildIntType(),
          SageBuilder::buildFunctionParameterList_nfi(),
          class_declaration->get_definition());
  declaration->get_specialFunctionModifier().setConversion();
  SgMemberFunctionSymbol *symbol =
      isSgMemberFunctionSymbol(declaration->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr);
  SgMemberFunctionRefExp *reference =
      SageBuilder::buildMemberFunctionRefExp(symbol, false, false);
  const std::string object_name =
      "rex_conversion_object_" + std::to_string(class_index);
  SgVariableDeclaration *object_declaration =
      SageBuilder::buildVariableDeclaration(
          object_name, class_declaration->get_type(), nullptr, scope);
  SageInterface::appendStatement(object_declaration, scope);
  SgVariableSymbol *object_symbol = scope->lookup_variable_symbol(object_name);
  ROSE_ASSERT(object_symbol != nullptr);
  SgVarRefExp *object = SageBuilder::buildVarRefExp(object_symbol);
  markSemanticNode(object);
  SgDotExp *callee =
      SageBuilder::buildDotExp(object, reference, reference->get_type());
  SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
  SgFunctionCallExp *call =
      new SgFunctionCallExp(callee, arguments, SageBuilder::buildIntType());
  callee->set_parent(call);
  arguments->set_parent(call);
  call->set_source_syntax(SgFunctionCallExp::e_implicit_conversion);
  markSemanticNode(call);
  markSemanticNode(callee);
  markSemanticNode(reference);
  markSemanticNode(arguments);
  context.advanceUseSite();
  if (expected != nullptr) {
    *expected = object_name;
  }
  return call;
}

SgCastExp *buildCast(SgCastExp::cast_type_enum kind, bool implicit_role) {
  SgIntVal *operand = SageBuilder::buildIntVal_nfi(7, "7");
  markSemanticNode(operand);
  SgCastExp *cast =
      SageBuilder::buildCastExp_nfi(operand, SageBuilder::buildIntType(), kind,
                                    SgCastExp::e_semantic_conversion_NoOp,
                                    SgCastExp::e_value_category_prvalue, {});
  if (kind != SgCastExp::e_implicit_cast) {
    cast->set_source_type(SageBuilder::buildIntType());
  }
  markSemanticNode(cast, implicit_role);
  return cast;
}

} // namespace

int main(int argc, char **argv) {
  const std::string mode = argc == 2 ? argv[1] : std::string();
  if (argc > 2) {
    return 2;
  }

  if (mode == "offsetof-roles") {
    SgTypeTraitBuiltinOperator *builtin = new SgTypeTraitBuiltinOperator(
        "__builtin_offsetof", SgTypeTraitBuiltinOperator::e_offsetof_builtin,
        SageBuilder::buildUnsignedLongType());
    builtin->get_args().push_back(
        SageBuilder::buildTypeExpression(SageBuilder::buildIntType()));
    markSemanticNode(builtin);
    (void)render(builtin);
    return 0;
  }
  if (mode == "offsetof-owner") {
    SgTypeTraitBuiltinOperator *builtin = buildOffsetof();
    isSgExpression(builtin->get_args()[1])->set_parent(nullptr);
    (void)render(builtin);
    return 0;
  }
  if (mode == "implicit-conversion-structure") {
    SgIntVal *callee = SageBuilder::buildIntVal_nfi(1, "1");
    markSemanticNode(callee);
    SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
    SgFunctionCallExp *call =
        new SgFunctionCallExp(callee, arguments, SageBuilder::buildIntType());
    callee->set_parent(call);
    arguments->set_parent(call);
    call->set_source_syntax(SgFunctionCallExp::e_implicit_conversion);
    markSemanticNode(call);
    markSemanticNode(arguments);
    (void)render(call);
    return 0;
  }
  if (mode == "implicit-conversion-provenance") {
    SgFunctionCallExp *call = buildImplicitConversion();
    call->get_args()->set_file_info(new Sg_File_Info("source.cpp", 1, 1));
    (void)render(call);
    return 0;
  }
  if (mode == "implicit-cast-provenance") {
    SgCastExp *cast = buildCast(SgCastExp::e_implicit_cast, false);
    (void)render(cast);
    return 0;
  }
  if (mode == "implicit-cast-kind") {
    SgCastExp *cast = buildCast(SgCastExp::e_implicit_cast, true);
    cast->set_semantic_conversion_kind(
        SgCastExp::e_semantic_conversion_unclassified);
    (void)render(cast);
    return 0;
  }
  if (mode == "implicit-cast-builder-kind") {
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(7, "7");
    markSemanticNode(operand);
    (void)SageBuilder::buildCastExp_nfi(
        operand, SageBuilder::buildIntType(), SgCastExp::e_implicit_cast,
        SgCastExp::e_semantic_conversion_unclassified,
        SgCastExp::e_value_category_prvalue, {});
    return 0;
  }
  if (mode == "cast-value-category") {
    SgCastExp *cast = buildCast(SgCastExp::e_C_style_cast, false);
    cast->set_value_category(SgCastExp::e_value_category_unclassified);
    (void)render(cast);
    return 0;
  }
  if (mode == "cast-base-path") {
    SgCastExp *cast = buildCast(SgCastExp::e_C_style_cast, false);
    cast->set_conversion_base_path({SageBuilder::buildIntType()});
    (void)render(cast);
    return 0;
  }
  if (mode == "cast-surface-semantic-mismatch") {
    SgCastExp *cast = buildCast(SgCastExp::e_C_style_cast, false);
    cast->set_cast_type(SgCastExp::e_dynamic_cast);
    cast->set_semantic_conversion_kind(
        SgCastExp::e_semantic_conversion_IntegralCast);
    (void)render(cast);
    return 0;
  }
  if (mode == "builtin-bit-cast-semantic-mismatch") {
    SgCastExp *cast = buildCast(SgCastExp::e_C_style_cast, false);
    cast->set_cast_type(SgCastExp::e_builtin_bit_cast);
    (void)render(cast);
    return 0;
  }
  if (mode == "functional-cast-semantic-mismatch") {
    SgCastExp *cast = buildCast(SgCastExp::e_C_style_cast, false);
    cast->set_cast_type(SgCastExp::e_functional_cast);
    cast->set_semantic_conversion_kind(
        SgCastExp::e_semantic_conversion_Dynamic);
    (void)render(cast);
    return 0;
  }
  if (mode == "invalid-call-source-syntax") {
    SgFunctionCallExp *call = buildImplicitConversion();
    call->set_source_syntax(
        static_cast<SgFunctionCallExp::source_syntax_enum>(999));
    (void)render(call);
    return 0;
  }
  if (!mode.empty()) {
    return 2;
  }

  std::string expected;
  if (render(buildOffsetof(&expected)) != expected) {
    return 1;
  }
  if (render(buildImplicitConversion(&expected)) != expected) {
    return 1;
  }
  if (render(buildCast(SgCastExp::e_C_style_cast, false)) != "(int)7") {
    return 1;
  }
  if (render(buildCast(SgCastExp::e_implicit_cast, true)) != "7") {
    return 1;
  }
  return 0;
}
