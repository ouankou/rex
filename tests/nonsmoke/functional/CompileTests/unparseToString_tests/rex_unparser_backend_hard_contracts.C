#include "rose.h"

#include "nameQualificationSupport.h"
#include "unparseCxx.h"
#include "unparseFortran.h"
#include "unparseLanguageIndependentConstructs.h"
#include "unparser.h"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace {

void attachSourceFileToProject(SgSourceFile &file) {
  ROSE_ASSERT(file.get_parent() == nullptr);
  SgProject *project = new SgProject();
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->get_fileList_ptr()->get_parent() == project);
  ROSE_ASSERT(project->get_fileList().empty());
  project->get_fileList().push_back(&file);
  file.set_parent(project->get_fileList_ptr());
  ROSE_ASSERT(project->get_fileList().size() == 1);
  ROSE_ASSERT(project->get_fileList().front() == &file);
  ROSE_ASSERT(file.get_project() == project);
}

void initializeSourceFile(SgSourceFile &file, const std::string &filename,
                          bool is_fortran) {
  attachSourceFileToProject(file);
  file.set_file_info(new Sg_File_Info(filename, 1, 1));
  file.set_sourceFileNameWithPath(filename);
  file.set_sourceFileNameWithoutPath(filename);
  file.set_Cxx_only(!is_fortran);
  file.set_Fortran_only(is_fortran);
  if (is_fortran) {
    file.set_inputFormat(SgFile::e_free_form_output_format);
    file.set_outputFormat(SgFile::e_free_form_output_format);
    file.set_backendCompileFormat(SgFile::e_free_form_output_format);
  }
}

void initializeExactSourceFile(SgSourceFile &file, const std::string &filename,
                               bool is_fortran) {
  initializeSourceFile(file, filename, is_fortran);
}

void setExactSourceRange(SgLocatedNode *node, const std::string &filename,
                         int start_line, int end_line) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *start = new Sg_File_Info(filename, start_line, 1);
  Sg_File_Info *end = new Sg_File_Info(filename, end_line, 1);
  start->setOutputInCodeGeneration();
  end->setOutputInCodeGeneration();
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  start->set_parent(node);
  end->set_parent(node);
}

SgGlobal *attachExactGlobalScope(SgSourceFile &file,
                                 const std::string &filename) {
  ROSE_ASSERT(file.get_globalScope() == nullptr);
  SgGlobal *global = new SgGlobal();
  global->set_file_info(new Sg_File_Info(filename, 1, 1));
  setExactSourceRange(global, filename, 1, 1);
  global->set_parent(&file);
  file.set_globalScope(global);
  ROSE_ASSERT(file.get_globalScope() == global);
  ROSE_ASSERT(global->get_parent() == &file);
  return global;
}

SgBasicBlock *buildExactFunctionBody(SgGlobal *global, const SgName &name) {
  ROSE_ASSERT(global != nullptr);
  SgFunctionDeclaration *function =
      SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(), name,
          SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global);
  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(function->get_definition() != nullptr);
  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(body, true) != nullptr);
  return body;
}

template <typename Expression>
Expression *
markSourceExpression(Expression *expression,
                     const std::string &filename = "source-expression.cpp") {
  ROSE_ASSERT(expression != nullptr);
  ROSE_ASSERT(expression->get_startOfConstruct() == nullptr);
  ROSE_ASSERT(expression->get_endOfConstruct() == nullptr);
  ROSE_ASSERT(expression->get_operatorPosition() == nullptr);
  expression->set_startOfConstruct(new Sg_File_Info(filename, 1, 1));
  expression->set_endOfConstruct(new Sg_File_Info(filename, 1, 1));
  expression->set_file_info(new Sg_File_Info(filename, 1, 1));
  expression->get_startOfConstruct()->set_parent(expression);
  expression->get_endOfConstruct()->set_parent(expression);
  expression->get_operatorPosition()->set_parent(expression);
  return expression;
}

template <typename Literal>
Literal *markSourceLiteral(Literal *literal,
                           const std::string &filename = "literal.cpp") {
  markSourceExpression(literal, filename);
  if (SgValueExp *value = isSgValueExp(literal)) {
    value->set_literal_spelling_form(SgValueExp::e_literal_source_spelled);
  }
  return literal;
}

template <typename Literal>
Literal *markCanonicalGeneratedLiteral(Literal *literal) {
  ROSE_ASSERT(literal != nullptr);
  auto make_file_info = []() {
    Sg_File_Info *file_info =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    ROSE_ASSERT(file_info != nullptr);
    file_info->setCompilerGenerated();
    file_info->setOutputInCodeGeneration();
    return file_info;
  };
  literal->set_startOfConstruct(make_file_info());
  literal->set_endOfConstruct(make_file_info());
  literal->set_file_info(make_file_info());
  literal->get_startOfConstruct()->set_parent(literal);
  literal->get_endOfConstruct()->set_parent(literal);
  literal->get_file_info()->set_parent(literal);
  if (SgValueExp *value = isSgValueExp(literal)) {
    value->set_literal_spelling_form(SgValueExp::e_literal_canonical_generated);
  }
  return literal;
}

PreprocessingInfo *buildInsideComment(const std::string &filename,
                                      const std::string &text) {
  return new PreprocessingInfo(PreprocessingInfo::C_StyleComment, text,
                               filename, 1, 1, 1, PreprocessingInfo::inside);
}

template <typename Value>
std::string expectedCanonicalFloating(Value value, const std::string &suffix) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::defaultfloat
         << std::setprecision(std::numeric_limits<Value>::max_digits10)
         << value;
  ROSE_ASSERT(!stream.fail());
  std::string spelling = stream.str();
  if (spelling.find_first_of(".eE") == std::string::npos) {
    spelling += ".0";
  }
  return spelling + suffix;
}

size_t countOccurrences(const std::string &text, const std::string &needle) {
  size_t count = 0;
  size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

class ContractStatementDelegate final : public UnparseDelegate {
public:
  ContractStatementDelegate(SgStatement *target,
                            StatementCoreEmission target_result)
      : target_(target), target_result_(target_result) {}

  StatementCoreEmission unparse_statement(SgStatement *statement,
                                          SgUnparse_Info &,
                                          UnparseFormat &output) override {
    ++call_count_;
    if (statement != target_) {
      return StatementCoreEmission::declined;
    }
    if (target_result_ == StatementCoreEmission::emitted) {
      output << std::string("rex_delegated();");
    }
    return target_result_;
  }

  size_t callCount() const { return call_count_; }

private:
  SgStatement *target_ = nullptr;
  StatementCoreEmission target_result_ = StatementCoreEmission::declined;
  size_t call_count_ = 0;
};

SgTemplateParameterPtrList buildTemplateParameters() {
  SgTemplateParameterPtrList parameters;
  parameters.push_back(SageBuilder::buildTemplateParameter(
      SgTemplateParameter::type_parameter,
      SageBuilder::buildTemplateType(SgName("T")),
      SgTemplateParameter::keyword_typename));
  return parameters;
}

SgTemplateInstantiationDecl *buildTemplateInstantiationTypeDeclaration(
    const SgName &template_name, const SgName &template_id,
    const SgTemplateArgumentPtrList &arguments, SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  SgTemplateInstantiationDecl *declaration = new SgTemplateInstantiationDecl(
      template_id, SgClassDeclaration::e_class, nullptr, nullptr, nullptr,
      arguments, SgTemplateArgumentPtrList());
  declaration->set_templateName(template_name);
  declaration->set_scope(scope);
  declaration->set_firstNondefiningDeclaration(declaration);
  declaration->set_definingDeclaration(nullptr);
  for (SgTemplateArgument *argument : declaration->get_templateArguments()) {
    ROSE_ASSERT(argument != nullptr);
    argument->set_parent(declaration);
  }
  SageBuilder::attachAuxiliaryDeclaration(scope, declaration);
  return declaration;
}

void unparseTemplateParameters(Unparser &unparser, SgUnparse_Info &info) {
  SgTemplateParameterPtrList parameters = buildTemplateParameters();
  unparser.u_exprStmt->unparseTemplateParameterList(parameters, info, true,
                                                    nullptr);
}

SgExprListExp *
buildFortranShape(int extent,
                  const std::string &filename = "dimension-source.f90") {
  SgExprListExp *shape =
      markSourceExpression(SageBuilder::buildExprListExp_nfi(), filename);
  SgIntVal *bound = markSourceLiteral(
      SageBuilder::buildIntVal_nfi(extent, std::to_string(extent)), filename);
  shape->append_expression(bound);
  bound->set_parent(shape);
  return shape;
}

SgExprListExp *buildFortranRankOnlyShape() {
  SgExprListExp *shape = SageBuilder::buildExprListExp_nfi();
  SgColonShapeExp *dimension = SageBuilder::buildColonShapeExp_nfi();
  shape->append_expression(dimension);
  dimension->set_parent(shape);
  return shape;
}

SgTypeInt *buildSemanticFortranInteger(int kind = 4) {
  SgExpression *selector = SageBuilder::buildIntVal_nfi(std::to_string(kind));
  ROSE_ASSERT(selector != nullptr);
  return SageBuilder::buildIntType(selector);
}

SgTypeInt *buildSourceFortranInteger() {
  SgTypeInt *type = new SgTypeInt();
  ROSE_ASSERT(type != nullptr);
  type->set_fortran_source_syntax(true);
  return type;
}

struct FortranDimensionFixture {
  SgScopeStatement *scope = nullptr;
  SgVariableDeclaration *declaration = nullptr;
  SgInitializedName *initialized_name = nullptr;
  SgAttributeSpecificationStatement *dimension = nullptr;
};

struct CxxFunctionCallFixture {
  SgSourceFile *source_file = nullptr;
  SgGlobal *global_scope = nullptr;
  SgFunctionDeclaration *declaration = nullptr;
  SgExpression *callee = nullptr;
  SgExprListExp *arguments = nullptr;
  SgExprListExp *literal_lexical_operands = nullptr;
  SgFunctionCallExp *call = nullptr;
  SgVariableDeclaration *use_site = nullptr;

  CxxFunctionCallFixture(bool has_declaration, bool is_literal_operator,
                         bool has_operand, bool operand_is_default_argument,
                         const std::string &filename) {
    source_file = new SgSourceFile();
    initializeExactSourceFile(*source_file, filename, false);
    global_scope = attachExactGlobalScope(*source_file, filename);

    if (has_declaration) {
      const SgName function_name(is_literal_operator
                                     ? "operator\"\"_declaration_suffix"
                                     : "rex_default_argument_target");
      declaration = SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          function_name, SageBuilder::buildIntType(),
          SageBuilder::buildFunctionParameterList(), global_scope);
      if (is_literal_operator) {
        declaration->get_specialFunctionModifier().setUldOperator();
      }
      callee = SageBuilder::buildFunctionRefExp(declaration);
    } else {
      callee = new SgFunctionRefExp(new SgFunctionSymbol(nullptr), nullptr);
    }

    arguments = SageBuilder::buildExprListExp_nfi();
    if (has_operand) {
      if (is_literal_operator) {
        SgIntVal *semantic_operand = SageBuilder::buildIntVal_nfi(42, "42");
        arguments->append_expression(semantic_operand);
        semantic_operand->set_parent(arguments);

        literal_lexical_operands = SageBuilder::buildExprListExp_nfi();
        SgIntVal *lexical_operand = markSourceLiteral(
            SageBuilder::buildIntVal_nfi(42, "0x2a"), filename);
        literal_lexical_operands->append_expression(lexical_operand);
        lexical_operand->set_parent(literal_lexical_operands);
      } else {
        SgIntVal *operand = markSourceLiteral(
            SageBuilder::buildIntVal_nfi(42, "0x2a"), filename);
        arguments->append_expression(operand);
        operand->set_parent(arguments);
      }
    }
    call =
        new SgFunctionCallExp(callee, arguments, SageBuilder::buildIntType());
    call->set_source_syntax(SgFunctionCallExp::e_source_function_call);
    callee->set_parent(call);
    arguments->set_parent(call);
    if (literal_lexical_operands != nullptr) {
      call->set_source_user_defined_literal_operands(literal_lexical_operands);
      literal_lexical_operands->set_parent(call);
      call->set_source_user_defined_literal_suffix_roles(SgUnsignedCharList{
          SgFunctionCallExp::e_user_defined_literal_token_with_suffix});
    }
    SgAssignInitializer *initializer =
        SageBuilder::buildAssignInitializer_nfi(call, call->get_type());
    use_site = SageBuilder::buildVariableDeclaration(
        "rex_function_call_use_site", call->get_type(), initializer,
        global_scope);
    SageInterface::appendStatement(use_site, global_scope);
    if (operand_is_default_argument) {
      for (SgExpression *argument : arguments->get_expressions()) {
        for (Sg_File_Info *position :
             {argument->get_file_info(), argument->get_startOfConstruct(),
              argument->get_endOfConstruct(),
              argument->get_operatorPosition()}) {
          ROSE_ASSERT(position != nullptr);
          position->setDefaultArgument();
        }
      }
    }
    if (is_literal_operator) {
      for (SgExpression *semantic_operand : arguments->get_expressions()) {
        markCanonicalGeneratedLiteral(semantic_operand);
      }
    }
  }

  void unparse(Unparser &unparser) {
    unparser.currentFile = source_file;
    SgUnparse_Info info;
    info.set_current_source_file(source_file);
    info.set_language(SgFile::e_Cxx_language);
    info.set_template_argument_qualification_context(use_site);
    if (declaration != nullptr) {
      unparser.get_name_qualification_context().recordName(
          callee, use_site, {"", 0, false, false});
    }
    unparser.u_exprStmt->unparseFuncCall(call, info);
  }
};

struct CxxMemberFunctionFixture {
  SgSourceFile *source_file = nullptr;
  SgClassDefinition *class_definition = nullptr;
  SgMemberFunctionDeclaration *declaration = nullptr;

  explicit CxxMemberFunctionFixture(const std::string &filename) {
    source_file = SageBuilder::buildGeneratedSourceFile(filename);
    ROSE_ASSERT(source_file != nullptr);
    source_file->set_Cxx_only(true);
    source_file->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *global = source_file->get_globalScope();
    ROSE_ASSERT(global != nullptr);
    SgClassDeclaration *class_declaration = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexMemberContract"), global);
    ROSE_ASSERT(class_declaration != nullptr);
    class_definition = class_declaration->get_definition();
    ROSE_ASSERT(class_definition != nullptr);
    declaration = SageBuilder::buildNondefiningMemberFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("member"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), class_definition);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(isSgMemberFunctionType(declaration->get_type()) != nullptr);
  }

  void initialize(Unparser &unparser, SgUnparse_Info &info) const {
    unparser.currentFile = source_file;
    info.set_current_source_file(source_file);
    info.set_current_scope(class_definition);
    info.set_language(SgFile::e_Cxx_language);
    info.set_template_argument_qualification_context(declaration);
  }
};

FortranDimensionFixture
buildFortranDimensionFixture(SgScopeStatement *exact_scope,
                             bool link_source_owner) {
  FortranDimensionFixture fixture;
  ROSE_ASSERT(exact_scope != nullptr);
  fixture.scope = exact_scope;
  SgArrayType *array_type = SageBuilder::buildArrayType(
      buildSemanticFortranInteger(), buildFortranRankOnlyShape());
  fixture.declaration = SageBuilder::buildVariableDeclaration(
      "rex_dimension_value", array_type, nullptr, fixture.scope);
  SageInterface::appendStatement(fixture.declaration, fixture.scope);
  fixture.initialized_name = fixture.declaration->get_variables().front();
  fixture.initialized_name->set_fortran_source_type(
      buildSourceFortranInteger());
  SgVariableSymbol *symbol =
      fixture.scope->lookup_variable_symbol("rex_dimension_value");
  ROSE_ASSERT(symbol != nullptr);

  fixture.dimension = SageBuilder::buildAttributeSpecificationStatement(
      SgAttributeSpecificationStatement::e_dimensionStatement);
  SageInterface::appendStatement(fixture.dimension, fixture.scope);
  SgExprListExp *parameters = fixture.dimension->get_parameter_list();
  ROSE_ASSERT(parameters != nullptr);
  SgVarRefExp *reference = SageBuilder::buildVarRefExp(symbol);
  SgExprListExp *shape = buildFortranShape(10);
  SgPntrArrRefExp *array_reference = SageBuilder::buildPntrArrRefExp(
      reference, shape, array_type->get_base_type());
  parameters->append_expression(array_reference);
  array_reference->set_parent(parameters);
  if (link_source_owner) {
    fixture.initialized_name->set_fortran_separate_shape_declaration(
        fixture.dimension);
  }
  return fixture;
}

} // namespace

int main(int argc, char **argv) {
  const std::string mode = argc == 2 ? argv[1] : std::string();
  if (argc > 2) {
    return 2;
  }

  Unparser_Opt options;
  std::ostringstream output;
  Unparser unparser(&output, "rex_unparser_backend_hard_contracts.cpp",
                    options);

  if (mode == "fortran-text-detached") {
    unparser.emitFortranText("integer :: value");
    return 0;
  }

  if (mode == "current-source-file-missing" ||
      mode == "current-source-file-mismatch") {
    SgSourceFile *requested =
        SageBuilder::buildGeneratedSourceFile("rex_current_source_file.cpp");
    ROSE_ASSERT(requested != nullptr);
    SgUnparse_Info info;
    if (mode == "current-source-file-mismatch") {
      SgSourceFile *inherited =
          SageBuilder::buildGeneratedSourceFile("rex_other_source_file.cpp");
      ROSE_ASSERT(inherited != nullptr);
      info.set_current_source_file(inherited);
    }
    unparser.unparseFile(requested, info);
    return 0;
  }

  if (mode == "member-function-restrict-type-only" ||
      mode == "member-function-restrict-declaration-only") {
    CxxMemberFunctionFixture fixture("rex_member_restrict_contract.cpp");
    SgMemberFunctionType *type =
        isSgMemberFunctionType(fixture.declaration->get_type());
    ROSE_ASSERT(type != nullptr);
    if (mode == "member-function-restrict-type-only") {
      type->setRestrictFunc();
    } else {
      fixture.declaration->get_declarationModifier()
          .get_typeModifier()
          .setRestrict();
    }
    SgUnparse_Info info;
    fixture.initialize(unparser, info);
    unparser.u_exprStmt->unparseMemberFunctionParametersAndQualifiers(
        fixture.declaration, info);
    return 0;
  }

  if (mode == "constructor-initializer-borrowed-from-definition") {
    CxxMemberFunctionFixture fixture("rex_ctor_initializer_contract.cpp");
    SgMemberFunctionDeclaration *definition =
        SageBuilder::buildDefiningMemberFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            fixture.declaration->get_name(), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), fixture.class_definition,
            false, 0, fixture.declaration, nullptr);
    ROSE_ASSERT(definition != nullptr);
    SgInitializedName *initializer = SageBuilder::buildInitializedName(
        SgName("base"), SageBuilder::buildIntType(),
        SageBuilder::buildAssignInitializer_nfi(
            SageBuilder::buildIntVal_nfi(1, "1"), SageBuilder::buildIntType()));
    ROSE_ASSERT(initializer != nullptr);
    definition->append_ctor_initializer(initializer);
    initializer->set_parent(definition->get_CtorInitializerList());
    fixture.declaration->set_definingDeclaration(definition);
    fixture.declaration->unsetForward();

    SgUnparse_Info info;
    fixture.initialize(unparser, info);
    info.set_SkipFunctionDefinition();
    NameQualificationContext &qualifications =
        unparser.get_name_qualification_context();
    qualifications.recordName(fixture.declaration, fixture.declaration,
                              {"", 0, false, false});
    qualifications.recordType(fixture.declaration, fixture.declaration,
                              {"", 0, false, false});
    unparser.u_exprStmt->unparseMFuncDeclStmt(fixture.declaration, info);
    return 0;
  }

  if (mode == "append-statement-nondeclaration-to-declaration-scope") {
    SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
        "append-statement-nondeclaration-to-declaration-scope.cpp");
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    SageInterface::appendStatement(SageBuilder::buildNullStatement(),
                                   source->get_globalScope());
    return 0;
  }

  if (mode == "append-variable-declaration-to-conflicting-scope") {
    SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
        "append-variable-declaration-to-conflicting-scope.cpp");
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    SgGlobal *global = source->get_globalScope();
    SgNamespaceDeclarationStatement *container =
        SageBuilder::buildNamespaceDeclaration("rex_scope_conflict_owner",
                                               global);
    ROSE_ASSERT(container != nullptr && container->get_definition() != nullptr);
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "rex_scope_conflict", SageBuilder::buildIntType(), nullptr, global);
    SageInterface::appendStatement(declaration, container->get_definition());
    return 0;
  }

  if (mode == "function-call-literal-callee") {
    SgIntVal *callee = SageBuilder::buildIntVal_nfi(1, "1");
    SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
    SgFunctionCallExp *call =
        new SgFunctionCallExp(callee, arguments, SageBuilder::buildIntType());
    callee->set_parent(call);
    arguments->set_parent(call);
    (void)call->getAssociatedFunctionSymbol();
    return 0;
  }

  if (mode == "lambda-missing-closure-type") {
    SgLambdaExp *lambda = new SgLambdaExp(
        static_cast<SgLambdaCaptureList *>(nullptr), nullptr, nullptr);
    (void)lambda->get_type();
    return 0;
  }

  if (mode == "this-missing-result-type") {
    (void)(new SgThisExp(nullptr, nullptr, 0, nullptr))->get_type();
    return 0;
  }
  if (mode == "this-builder-missing-result-type") {
    (void)SageBuilder::buildThisExp_nfi(new SgClassSymbol(nullptr), nullptr);
    return 0;
  }
  if (mode == "this-symbol-type-mismatch") {
    SgSourceFile *source =
        SageBuilder::buildGeneratedSourceFile("this-symbol-type-mismatch.cpp");
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    SgClassDeclaration *declaration = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexThisOwner"), source->get_globalScope());
    ROSE_ASSERT(declaration != nullptr);
    SgClassDeclaration *first =
        isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
    SgClassSymbol *symbol = isSgClassSymbol(
        first != nullptr ? first->get_symbol_from_symbol_table() : nullptr);
    ROSE_ASSERT(symbol != nullptr);
    (void)(new SgThisExp(
               symbol, nullptr, 0,
               SageBuilder::buildPointerType(SageBuilder::buildIntType())))
        ->get_type();
    return 0;
  }
  if (mode == "sizeof-missing-result-type" ||
      mode == "alignof-missing-result-type") {
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgExpression *operation =
        mode == "sizeof-missing-result-type"
            ? static_cast<SgExpression *>(
                  new SgSizeOfOp(operand, nullptr, nullptr))
            : static_cast<SgExpression *>(
                  new SgAlignOfOp(operand, nullptr, nullptr));
    operand->set_parent(operation);
    (void)operation->get_type();
    return 0;
  }
  if (mode == "sizeof-builder-missing-result-type") {
    (void)SageBuilder::buildSizeOfOp_nfi(SageBuilder::buildIntVal_nfi(1, "1"),
                                         nullptr);
    return 0;
  }
  if (mode == "alignof-builder-missing-result-type") {
    (void)SageBuilder::buildAlignOfOp_nfi(SageBuilder::buildIntVal_nfi(1, "1"),
                                          nullptr);
    return 0;
  }
  if (mode == "sizeof-explicit-invalid-result-type" ||
      mode == "alignof-explicit-invalid-result-type") {
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgExpression *operation =
        mode == "sizeof-explicit-invalid-result-type"
            ? static_cast<SgExpression *>(new SgSizeOfOp(
                  operand, nullptr, SageBuilder::buildUnsignedLongType()))
            : static_cast<SgExpression *>(new SgAlignOfOp(
                  operand, nullptr, SageBuilder::buildUnsignedLongType()));
    operand->set_parent(operation);
    operation->set_explicitly_stored_type(SageBuilder::buildIntType());
    return 0;
  }
  if (mode == "sizeof-foreign-target-result-type" ||
      mode == "alignof-foreign-target-result-type") {
    SgSourceFile *source = new SgSourceFile();
    initializeExactSourceFile(*source, "foreign-target-result-type.cpp", false);
    SgGlobal *global =
        attachExactGlobalScope(*source, "foreign-target-result-type.cpp");
    source->set_target_size_type(SageBuilder::buildUnsignedLongType());
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgExpression *operation =
        mode == "sizeof-foreign-target-result-type"
            ? static_cast<SgExpression *>(new SgSizeOfOp(
                  operand, nullptr, SageBuilder::buildUnsignedIntType()))
            : static_cast<SgExpression *>(new SgAlignOfOp(
                  operand, nullptr, SageBuilder::buildUnsignedIntType()));
    operand->set_parent(operation);
    operation->set_parent(global);
    (void)operation->get_type();
    return 0;
  }
  if (mode == "sizeof-replace-foreign-old" ||
      mode == "alignof-replace-foreign-old") {
    SgIntVal *owned_operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgExpression *operation =
        mode == "sizeof-replace-foreign-old"
            ? static_cast<SgExpression *>(new SgSizeOfOp(
                  owned_operand, nullptr, SageBuilder::buildUnsignedLongType()))
            : static_cast<SgExpression *>(
                  new SgAlignOfOp(owned_operand, nullptr,
                                  SageBuilder::buildUnsignedLongType()));
    owned_operand->set_parent(operation);
    SgIntVal *foreign_old = SageBuilder::buildIntVal_nfi(2, "2");
    SgIntVal *replacement = SageBuilder::buildIntVal_nfi(3, "3");
    if (SgSizeOfOp *size_of = isSgSizeOfOp(operation)) {
      (void)size_of->replace_expression(foreign_old, replacement);
    } else {
      (void)isSgAlignOfOp(operation)->replace_expression(foreign_old,
                                                         replacement);
    }
    return 0;
  }
  if (mode == "sizeof-replace-owned-new" ||
      mode == "alignof-replace-owned-new") {
    SgIntVal *owned_operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgExpression *operation =
        mode == "sizeof-replace-owned-new"
            ? static_cast<SgExpression *>(new SgSizeOfOp(
                  owned_operand, nullptr, SageBuilder::buildUnsignedLongType()))
            : static_cast<SgExpression *>(
                  new SgAlignOfOp(owned_operand, nullptr,
                                  SageBuilder::buildUnsignedLongType()));
    owned_operand->set_parent(operation);
    SgIntVal *already_owned_replacement = SageBuilder::buildIntVal_nfi(2, "2");
    already_owned_replacement->set_parent(operation);
    if (SgSizeOfOp *size_of = isSgSizeOfOp(operation)) {
      (void)size_of->replace_expression(owned_operand,
                                        already_owned_replacement);
    } else {
      (void)isSgAlignOfOp(operation)->replace_expression(
          owned_operand, already_owned_replacement);
    }
    return 0;
  }
  if (mode == "target-size-detached-context") {
    (void)SageInterface::requireTargetSizeType(
        SageBuilder::buildIntVal_nfi(1, "1"));
    return 0;
  }

  if (mode == "function-call-missing-result-type" ||
      mode == "function-call-callable-result-type" ||
      mode == "function-call-builder-callable-result-type") {
    SgIntVal *callee = SageBuilder::buildIntVal_nfi(1, "1");
    SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
    SgFunctionType *function_type = SageBuilder::buildFunctionType(
        SageBuilder::buildIntType(),
        SageBuilder::buildFunctionParameterTypeList());
    if (mode == "function-call-builder-callable-result-type") {
      (void)SageBuilder::buildFunctionCallExp_nfi(callee, function_type,
                                                  arguments);
      return 0;
    }
    SgType *stored_type =
        mode == "function-call-missing-result-type" ? nullptr : function_type;
    SgFunctionCallExp *call =
        new SgFunctionCallExp(callee, arguments, stored_type);
    callee->set_parent(call);
    arguments->set_parent(call);
    (void)call->get_type();
    return 0;
  }

  if (mode == "implied-do-type-query") {
    SgImpliedDo *implied_do =
        new SgImpliedDo(static_cast<SgExpression *>(nullptr), nullptr, nullptr,
                        nullptr, nullptr);
    (void)implied_do->get_type();
    return 0;
  }

  if (mode == "conditional-missing-result-type") {
    SgConditionalExp *conditional =
        new SgConditionalExp(SageBuilder::buildBoolValExp_nfi(true),
                             SageBuilder::buildIntVal_nfi(1, "1"),
                             SageBuilder::buildIntVal_nfi(0, "0"), nullptr);
    conditional->set_operator_kind(
        SgConditionalExp::e_conditional_operator_standard);
    (void)conditional->get_type();
    return 0;
  }

  if (mode == "typed-builtin-missing-result-type") {
    SgTypeTraitBuiltinOperator *builtin = new SgTypeTraitBuiltinOperator(
        "__builtin_offsetof", SgTypeTraitBuiltinOperator::e_offsetof_builtin,
        nullptr);
    (void)builtin->get_type();
    return 0;
  }

  if (mode == "binary-missing-result-type") {
    (void)(new SgAddOp(SageBuilder::buildIntVal_nfi(1, "1"),
                       SageBuilder::buildIntVal_nfi(2, "2"), nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "unary-missing-result-type") {
    (void)(new SgMinusOp(SageBuilder::buildIntVal_nfi(1, "1"), nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "binary-builder-missing-result-type") {
    (void)SageBuilder::buildAddOp(SageBuilder::buildIntVal_nfi(1, "1"),
                                  SageBuilder::buildIntVal_nfi(2, "2"),
                                  nullptr);
    return 0;
  }
  if (mode == "unary-builder-missing-result-type") {
    (void)SageBuilder::buildMinusOp(SageBuilder::buildIntVal_nfi(1, "1"),
                                    nullptr);
    return 0;
  }
  if (mode == "throw-nonvoid-result-type") {
    (void)(new SgThrowOp(SageBuilder::buildIntVal_nfi(1, "1"),
                         SageBuilder::buildIntType(),
                         SgThrowOp::throw_expression))
        ->get_type();
    return 0;
  }
  if (mode == "await-missing-result-type") {
    (void)(new SgAwaitExpression(SageBuilder::buildIntVal_nfi(1, "1"), nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "fold-default-result-type") {
    (void)(new SgFoldExpression(SageBuilder::buildIntVal_nfi(1, "1"), "+", true,
                                SgTypeDefault::createType()))
        ->get_type();
    return 0;
  }
  if (mode == "pack-pattern-foreign-owner") {
    SgIntVal *pattern = SageBuilder::buildIntVal_nfi(1, "1");
    (void)SageBuilder::buildExprStatement_nfi(pattern);
    (void)SageBuilder::buildPackExpansionExpr_nfi(pattern,
                                                  SageBuilder::buildIntType());
    return 0;
  }
  if (mode == "statement-expression-unknown-result-type") {
    (void)(new SgStatementExpression(SageBuilder::buildBasicBlock_nfi(),
                                     SageBuilder::buildUnknownType()))
        ->get_type();
    return 0;
  }
  if (mode == "caf-coexpression-missing-result-type") {
    (void)(new SgCAFCoExpression(nullptr, SageBuilder::buildExprListExp_nfi(),
                                 SageBuilder::buildIntVal_nfi(1, "1"), nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "typeid-missing-result-type") {
    (void)(new SgTypeIdOp(SageBuilder::buildIntVal_nfi(1, "1"), nullptr,
                          nullptr))
        ->get_type();
    return 0;
  }

  if (mode == "null-expression-type-query") {
    (void)SageBuilder::buildNullExpression_nfi(
        SgNullExpression::e_null_expression_syntactic_absence)
        ->get_type();
    return 0;
  }
  if (mode == "null-expression-invalid-role") {
    (void)SageBuilder::buildNullExpression_nfi(
        static_cast<SgNullExpression::null_expression_role_enum>(999));
    return 0;
  }
  if (mode == "expr-list-type-query") {
    (void)SageBuilder::buildExprListExp_nfi()->get_type();
    return 0;
  }
  if (mode == "cuda-exec-config-type-query") {
    (void)SageBuilder::buildCudaKernelExecConfig_nfi(
        SageBuilder::buildIntVal_nfi(1, "1"),
        SageBuilder::buildIntVal_nfi(1, "1"), nullptr, nullptr)
        ->get_type();
    return 0;
  }
  if (mode == "range-type-query" || mode == "range-fourth-element") {
    SgRangeExp *range =
        SageBuilder::buildRangeExp(SageBuilder::buildIntVal_nfi(1, "1"),
                                   SageBuilder::buildIntVal_nfi(2, "2"),
                                   SageBuilder::buildIntVal_nfi(3, "3"));
    if (mode == "range-fourth-element") {
      (void)range->append(SageBuilder::buildIntVal_nfi(4, "4"));
    } else {
      (void)range->get_type();
    }
    return 0;
  }
  if (mode == "subscript-type-query") {
    (void)SageBuilder::buildSubscriptExpression_nfi(
        nullptr, SageBuilder::buildIntVal_nfi(8, "8"), nullptr)
        ->get_type();
    return 0;
  }
  if (mode == "designator-type-query") {
    SgIntVal *index = SageBuilder::buildIntVal_nfi(0, "0");
    SgDesignator *designator =
        new SgDesignator(SgDesignator::e_designator_array, index, nullptr);
    index->set_parent(designator);
    (void)designator->get_type();
    return 0;
  }
  if (mode == "asm-operand-type-query") {
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(0, "0");
    SgAsmOp *asm_operand =
        new SgAsmOp(SgAsmOp::e_any, SgAsmOp::e_input, operand);
    operand->set_parent(asm_operand);
    (void)asm_operand->get_type();
    return 0;
  }
  if (mode == "simple-requirement-type-query") {
    (void)SageBuilder::buildSimpleRequirement_nfi(
        SageBuilder::buildBoolValExp_nfi(true))
        ->get_type();
    return 0;
  }
  if (mode == "type-requirement-type-query") {
    (void)SageBuilder::buildTypeRequirement_nfi(SageBuilder::buildIntType())
        ->get_type();
    return 0;
  }
  if (mode == "compound-requirement-type-query") {
    (void)SageBuilder::buildCompoundRequirement_nfi(
        SageBuilder::buildIntVal_nfi(1, "1"), false, nullptr)
        ->get_type();
    return 0;
  }
  if (mode == "nested-requirement-type-query") {
    (void)SageBuilder::buildNestedRequirement_nfi(
        SageBuilder::buildBoolValExp_nfi(true))
        ->get_type();
    return 0;
  }
  if (mode == "type-expression-type-query") {
    (void)SageBuilder::buildTypeExpression(SageBuilder::buildIntType())
        ->get_type();
    return 0;
  }
  if (mode == "type-expression-missing-represented-type") {
    (void)SageBuilder::buildTypeExpression(nullptr);
    return 0;
  }
  if (mode == "source-location-builtin-missing-result-type") {
    (void)(new SgSourceLocationBuiltinExp(SgSourceLocationBuiltinExp::e_file,
                                          nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "pseudo-destructor-missing-result-type") {
    SgPseudoDestructorRefExp *reference =
        new SgPseudoDestructorRefExp(SageBuilder::buildIntType(), nullptr);
    (void)reference->get_type();
    return 0;
  }
  if (mode == "vararg-missing-result-type") {
    (void)(new SgVarArgOp(SageBuilder::buildIntVal_nfi(0, "0"), nullptr))
        ->get_type();
    return 0;
  }
  if (mode == "function-parameter-reference-missing-type") {
    (void)(new SgFunctionParameterRefExp(nullptr, 0, 0, nullptr))->get_type();
    return 0;
  }
  if (mode == "function-parameter-reference-placeholder-type") {
    (void)(new SgFunctionParameterRefExp(nullptr, 0, 0,
                                         SgTypeDefault::createType()))
        ->get_type();
    return 0;
  }

  if (mode == "asterisk-shape-type-query") {
    (void)(new SgAsteriskShapeExp())->get_type();
    return 0;
  }
  if (mode == "colon-shape-type-query") {
    (void)(new SgColonShapeExp())->get_type();
    return 0;
  }
  if (mode == "assumed-rank-type-query") {
    (void)(new SgAssumedRankExp())->get_type();
    return 0;
  }
  if (mode == "coarray-image-selector-type-query") {
    SgCAFImageSelectorExp *selector = new SgCAFImageSelectorExp(
        static_cast<SgExprListExp *>(nullptr), nullptr, nullptr, nullptr);
    (void)selector->get_type();
    return 0;
  }

  if (mode == "formatter-negative-newline-count") {
    unparser.cur.insert_newline(-1);
    return 0;
  }
  if (mode == "formatter-negative-indent") {
    unparser.cur.insert_newline(1, -1);
    return 0;
  }
  if (mode == "formatter-zero-linewrap") {
    unparser.cur.set_linewrap(0);
    return 0;
  }
  if (mode == "formatter-negative-linewrap") {
    unparser.cur.set_linewrap(-1);
    return 0;
  }
  if (mode == "line-directive-missing-source-location") {
    Unparser_Opt lineOptions;
    lineOptions.set_linefile_opt(true);
    std::ostringstream lineOutput;
    Unparser lineUnparser(&lineOutput, "line-directive.cpp", lineOptions);
    lineUnparser.u_exprStmt->unparseLineDirectives(new SgNullStatement());
    return 0;
  }
  if (mode == "line-directive-filename-quote" ||
      mode == "line-directive-filename-backslash" ||
      mode == "line-directive-filename-line-feed" ||
      mode == "line-directive-filename-carriage-return" ||
      mode == "line-directive-filename-control-byte") {
    std::string filename;
    if (mode == "line-directive-filename-quote") {
      filename = "line-\"bad.cpp";
    } else if (mode == "line-directive-filename-backslash") {
      filename = "line-\\bad.cpp";
    } else if (mode == "line-directive-filename-line-feed") {
      filename = "line-\nbad.cpp";
    } else if (mode == "line-directive-filename-carriage-return") {
      filename = "line-\rbad.cpp";
    } else {
      filename = "line-" + std::string(1, '\x01') + "bad.cpp";
    }
    Unparser_Opt lineOptions;
    lineOptions.set_linefile_opt(true);
    std::ostringstream lineOutput;
    Unparser lineUnparser(&lineOutput, "line-directive.cpp", lineOptions);
    SgNullStatement *statement = new SgNullStatement();
    setExactSourceRange(statement, filename, 1, 1);
    lineUnparser.u_exprStmt->unparseLineDirectives(statement);
    return 0;
  }
  if (mode == "token-sequence-position-invalid") {
    (void)unparser.u_exprStmt->token_sequence_position_name(
        static_cast<UnparseLanguageIndependentConstructs::
                        token_sequence_position_enum_type>(999));
    return 0;
  }
  if (mode == "obsolete-include-directive-statement") {
    SgIncludeDirectiveStatement *directive = new SgIncludeDirectiveStatement();
    directive->set_directiveString("#include <obsolete.h>");
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseIncludeDirectiveStatement(directive, info);
    return 0;
  }
  if (mode == "name-qualification-null-initialized-name" ||
      mode == "name-qualification-unpublished-declaration" ||
      mode == "name-qualification-canonical-namespace-missing-function") {
    NameQualificationTraversal::NameQualificationMapType names;
    NameQualificationTraversal::NameQualificationMapType types;
    NameQualificationTraversal::NameQualificationMapOfMapsType type_maps;
    NameQualificationTraversal::NameQualificationSetType referenced;
    NameQualificationContext qualifications;
    NameQualificationTraversal traversal(names, types, type_maps, referenced,
                                         qualifications);
    if (mode == "name-qualification-null-initialized-name") {
      traversal.setNameQualificationOnName(nullptr, nullptr, 0, false);
    } else if (mode == "name-qualification-unpublished-declaration") {
      SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
          SgName("rex_unpublished"), nullptr, nullptr);
      traversal.validateDeclarationPublishedBeforeQualification(declaration);
    } else {
      const SgName name("rex_missing_canonical_function");
      SgNamespaceDefinitionStatement *lexical_namespace =
          SageBuilder::buildNamespaceDefinition();
      SgNamespaceDefinitionStatement *canonical_namespace =
          SageBuilder::buildNamespaceDefinition();
      lexical_namespace->set_global_definition(canonical_namespace);
      canonical_namespace->set_global_definition(canonical_namespace);

      SgTemplateInstantiationFunctionDecl *declaration =
          new SgTemplateInstantiationFunctionDecl(
              name, static_cast<SgFunctionType *>(nullptr),
              static_cast<SgFunctionDefinition *>(nullptr));
      declaration->set_scope(lexical_namespace);
      lexical_namespace->insert_symbol(name, new SgFunctionSymbol(declaration));

      SgNullStatement *use_site = new SgNullStatement();
      (void)traversal.nameQualificationDepth(declaration, lexical_namespace,
                                             use_site);
    }
    return 0;
  }
  if (mode == "name-qualification-using-null-scope-child" ||
      mode == "name-qualification-using-null-target" ||
      mode == "name-qualification-using-target-missing-definition" ||
      mode == "name-qualification-using-missing-comparison-order" ||
      mode == "name-qualification-using-typed-mirror-mismatch" ||
      mode == "name-qualification-using-mirror-without-typed-order" ||
      mode == "name-qualification-using-typed-order-without-mirror" ||
      mode == "name-qualification-position-typed-mirror-mismatch" ||
      mode == "name-qualification-using-unsupported-scope-kind" ||
      mode == "name-qualification-using-scope-cycle") {
    SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
        "name-qualification-using-order.cpp");
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    SgGlobal *global = source->get_globalScope();
    SgClassDeclaration *target = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexUsingOrderTarget"), global);
    ROSE_ASSERT(target != nullptr);

    SgNamespaceDeclarationStatement *namespaceDeclaration = nullptr;
    if (mode == "name-qualification-using-target-missing-definition") {
      namespaceDeclaration = new SgNamespaceDeclarationStatement(
          SgName("rex_using_order_broken"), nullptr, false);
    } else if (mode != "name-qualification-using-null-target" &&
               mode != "name-qualification-using-null-scope-child") {
      namespaceDeclaration =
          SageBuilder::buildNamespaceDeclaration("rex_using_order", global);
    }

    SgUsingDirectiveStatement *directive = nullptr;
    if (mode != "name-qualification-using-null-scope-child") {
      directive =
          SageBuilder::buildUsingDirectiveStatement(namespaceDeclaration);
      SageInterface::appendStatement(directive, global);
    } else {
      global->get_declarations().push_back(nullptr);
    }

    if (mode == "name-qualification-using-typed-mirror-mismatch" ||
        mode == "name-qualification-using-mirror-without-typed-order" ||
        mode == "name-qualification-using-typed-order-without-mirror") {
      ROSE_ASSERT(directive != nullptr);
      setExactSourceRange(directive, "name-qualification-using-order.cpp", 2,
                          2);
      if (mode != "name-qualification-using-typed-order-without-mirror") {
        directive->get_startOfConstruct()->set_source_sequence_number(12);
      }
      if (mode != "name-qualification-using-mirror-without-typed-order") {
        directive->initialize_translation_unit_source_order(11);
      }
    }

    SgStatement *position = nullptr;
    if (mode == "name-qualification-using-null-scope-child") {
      position = target;
    } else if (mode == "name-qualification-position-typed-mirror-mismatch") {
      SgUsingDirectiveStatement *positionDirective =
          SageBuilder::buildUsingDirectiveStatement(namespaceDeclaration);
      setExactSourceRange(positionDirective,
                          "name-qualification-using-order.cpp", 3, 3);
      positionDirective->get_startOfConstruct()->set_source_sequence_number(12);
      positionDirective->initialize_translation_unit_source_order(11);
      SageInterface::appendStatement(positionDirective, global);
      position = positionDirective;
    } else if (mode == "name-qualification-using-missing-comparison-order") {
      position = SageBuilder::buildClassDeclaration(
          SageBuilder::declaration_ownership::semanticAuxiliary(),
          SgName("RexUsingOrderSemanticPosition"), global);
    } else {
      position = SageBuilder::buildVariableDeclaration(
          "rex_using_order_position", SageBuilder::buildIntType(), nullptr,
          global);
      setExactSourceRange(position, "name-qualification-using-order.cpp", 3, 3);
      SageInterface::appendStatement(position, global);
    }

    SgScopeStatement *currentScope = global;
    if (mode == "name-qualification-using-null-scope-child") {
      SgBasicBlock *nestedScope = SageBuilder::buildBasicBlock();
      nestedScope->set_parent(global);
      nestedScope->set_scope(global);
      currentScope = nestedScope;
    } else if (mode == "name-qualification-using-unsupported-scope-kind") {
      currentScope = new SgScopeStatement();
      currentScope->set_parent(global);
      currentScope->set_scope(global);
      SgClassDeclaration *canonicalTarget =
          isSgClassDeclaration(target->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonicalTarget != nullptr);
      currentScope->insert_symbol(SgName("RexUsingOrderTarget"),
                                  new SgClassSymbol(canonicalTarget));
    } else if (mode == "name-qualification-using-scope-cycle") {
      SgBasicBlock *firstScope = SageBuilder::buildBasicBlock();
      SgBasicBlock *secondScope = SageBuilder::buildBasicBlock();
      firstScope->set_parent(secondScope);
      secondScope->set_parent(firstScope);
      SgClassDeclaration *canonicalTarget =
          isSgClassDeclaration(target->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonicalTarget != nullptr);
      firstScope->insert_symbol(SgName("RexUsingOrderTarget"),
                                new SgClassSymbol(canonicalTarget));
      currentScope = firstScope;
    }

    NameQualificationTraversal::NameQualificationMapType names;
    NameQualificationTraversal::NameQualificationMapType types;
    NameQualificationTraversal::NameQualificationMapOfMapsType typeMaps;
    NameQualificationTraversal::NameQualificationSetType referenced;
    NameQualificationContext qualifications;
    NameQualificationTraversal traversal(names, types, typeMaps, referenced,
                                         qualifications);
    (void)traversal.nameQualificationDepth(target, currentScope, position);
    return 0;
  }
  if (mode == "preprocessing-missing-file-info" ||
      mode == "preprocessing-invalid-source-line" ||
      mode == "preprocessing-retrograde-source-anchor") {
    const std::string filename = "preprocessing-source-contract.cpp";
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, filename, false);
    unparser.currentFile = &source_file;
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);

    if (mode == "preprocessing-retrograde-source-anchor") {
      SgIntVal *value = SageBuilder::buildIntVal_nfi(1, "1");
      setExactSourceRange(value, filename, 2, 2);
      SgExprStatement *statement = SageBuilder::buildExprStatement_nfi(value);
      setExactSourceRange(statement, filename, 10, 10);
      value->set_parent(statement);
      PreprocessingInfo *comment = new PreprocessingInfo(
          PreprocessingInfo::C_StyleComment, "/* misplaced */", filename, 5, 1,
          1, PreprocessingInfo::before);
      value->addToAttachedPreprocessingInfo(comment, PreprocessingInfo::before);
      unparser.u_exprStmt->unparseAttachedPreprocessingInfo(
          value, info, PreprocessingInfo::before);
      return 0;
    }

    SgNullStatement *statement = new SgNullStatement();
    setExactSourceRange(statement, filename, 2, 2);
    const int line = mode == "preprocessing-invalid-source-line" ? 0 : 1;
    PreprocessingInfo *comment =
        new PreprocessingInfo(PreprocessingInfo::C_StyleComment, "/* source */",
                              filename, line, 1, 1, PreprocessingInfo::before);
    statement->addToAttachedPreprocessingInfo(comment,
                                              PreprocessingInfo::before);
    if (mode == "preprocessing-missing-file-info") {
      comment->set_file_info(nullptr);
    }
    unparser.u_exprStmt->unparseAttachedPreprocessingInfo(
        statement, info, PreprocessingInfo::before);
    return 0;
  }
  if (mode == "fortran-string-detached") {
    unparser.emitFortranCharacterLiteral("value", '\'');
    return 0;
  }
  if (mode == "fortran-text-cxx" || mode == "fortran-string-cxx") {
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeSourceFile(cxx_file, "wrong-language.cpp", false);
    unparser.currentFile = &cxx_file;
    if (mode == "fortran-text-cxx") {
      unparser.emitFortranText("integer :: value");
    } else {
      unparser.emitFortranCharacterLiteral("value", '\'');
    }
    return 0;
  }

  if (mode == "fortran-call-target-missing-type") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeSourceFile(fortran_file, "missing-call-target.f90", true);
    unparser.currentFile = &fortran_file;

    SgExpression *target = new SgExpression();
    SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi();
    SgFunctionCallExp *call =
        new SgFunctionCallExp(target, arguments, SageBuilder::buildVoidType());
    target->set_parent(call);
    arguments->set_parent(call);
    SgExprStatement *statement = SageBuilder::buildExprStatement_nfi(call);
    call->set_parent(statement);

    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    FortranCodeGeneration_locatedNode fortran(&unparser,
                                              "missing-call-target.f90");
    fortran.unparseFuncCall(call, info);
    return 0;
  }

  if (mode == "fortran-statement-missing-physical-owner" ||
      mode == "fortran-statement-ambiguous-physical-owner") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeExactSourceFile(fortran_file, "exact-owner.f90", true);
    SgGlobal *global = attachExactGlobalScope(fortran_file, "exact-owner.f90");
    unparser.currentFile = &fortran_file;

    const SgName function_name("rex_exact_owner_scope");
    SgType *return_type = SageBuilder::buildVoidType();
    SgFunctionDeclaration *canonical =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            function_name, return_type,
            SageBuilder::buildFunctionParameterList_nfi(), global);
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            function_name, return_type,
            SageBuilder::buildFunctionParameterList(), global, false, canonical,
            nullptr, false);
    SgFunctionDefinition *definition = function->get_definition();
    SgBasicBlock *body =
        definition != nullptr ? definition->get_body() : nullptr;
    ROSE_ASSERT(body != nullptr && body->get_parent() == definition);

    SgNullStatement *statement = new SgNullStatement();
    setExactSourceRange(statement, "exact-owner.f90", 2, 2);
    SageInterface::appendStatement(statement, body);
    Sg_File_Info *statement_start =
        mode == "fortran-statement-missing-physical-owner"
            ? Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode()
            : new Sg_File_Info("exact-owner.f90", 2, 1);
    Sg_File_Info *statement_end = new Sg_File_Info(*statement_start);
    if (mode == "fortran-statement-ambiguous-physical-owner") {
      statement_start->setShared();
      statement_end->setShared();
    }
    statement->set_file_info(statement_start);
    statement->set_endOfConstruct(statement_end);
    statement_end->set_parent(statement);

    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    info.set_language(SgFile::e_Fortran_language);
    FortranCodeGeneration_locatedNode fortran(&unparser, "exact-owner.f90");
    (void)fortran.statementFromFile(statement, "exact-owner.f90", info);
    return 0;
  }

  if (mode == "fortran-implied-do-missing-object-list" ||
      mode == "fortran-implied-do-empty-object-list" ||
      mode == "fortran-implied-do-foreign-object-list-owner") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeSourceFile(fortran_file, "implied-do.f90", true);
    unparser.currentFile = &fortran_file;

    SgIntVal *control = SageBuilder::buildIntVal_nfi(1, "i");
    SgIntVal *initial = SageBuilder::buildIntVal_nfi(1, "1");
    SgAssignOp *initialization =
        new SgAssignOp(control, initial, SageBuilder::buildIntType());
    control->set_parent(initialization);
    initial->set_parent(initialization);
    SgIntVal *upper = SageBuilder::buildIntVal_nfi(4, "4");
    SgIntVal *step = SageBuilder::buildIntVal_nfi(1, "1");
    SgExprListExp *objects = mode == "fortran-implied-do-missing-object-list"
                                 ? nullptr
                                 : SageBuilder::buildExprListExp_nfi();
    if (objects != nullptr && mode != "fortran-implied-do-empty-object-list") {
      SgIntVal *object = SageBuilder::buildIntVal_nfi(7, "7");
      objects->append_expression(object);
      object->set_parent(objects);
    }

    SgImpliedDo *implied =
        new SgImpliedDo(initialization, upper, step, objects, nullptr);
    initialization->set_parent(implied);
    upper->set_parent(implied);
    step->set_parent(implied);
    if (objects != nullptr) {
      objects->set_parent(mode == "fortran-implied-do-foreign-object-list-owner"
                              ? static_cast<SgNode *>(new SgNullStatement())
                              : static_cast<SgNode *>(implied));
    }

    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    info.set_language(SgFile::e_Fortran_language);
    FortranCodeGeneration_locatedNode fortran(&unparser, "implied-do.f90");
    fortran.unparseImpliedDo(implied, info);
    return 0;
  }

  if (mode == "template-parameter-missing-keyword") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeSourceFile(source_file, "template-parameter.cpp", false);
    unparser.currentFile = &source_file;
    SgTemplateParameterPtrList parameters = buildTemplateParameters();
    parameters.front()->set_templateParameterKeyword(
        SgTemplateParameter::keyword_unspecified);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseTemplateParameterList(parameters, info, true,
                                                      nullptr);
    return 0;
  }

  if (mode == "template-argument-explicit-after-deduced" ||
      mode == "template-argument-source-type-nonexplicit") {
    SgTemplateArgumentPtrList arguments;
    SgTemplateArgument *first =
        SageBuilder::buildTemplateArgument(SageBuilder::buildIntType());
    first->set_explicitlySpecified(false);
    arguments.push_back(first);
    if (mode == "template-argument-explicit-after-deduced") {
      SgTemplateArgument *second =
          SageBuilder::buildTemplateArgument(SageBuilder::buildFloatType());
      second->set_explicitlySpecified(true);
      arguments.push_back(second);
    } else {
      first->set_sourceSpelledType(SageBuilder::buildFloatType());
    }
    SgUnparse_Info info;
    info.set_SkipQualifiedNames();
    unparser.u_exprStmt->unparseTemplateArgumentList(
        arguments, info, TemplateArgumentEmission::explicit_source_prefix);
    return 0;
  }

  if (mode == "declaration-missing-semantic-scope") {
    SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
        SgName("rex_missing_scope"), nullptr, nullptr);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseFuncDeclStmt(declaration, info);
    return 0;
  }

  if (mode == "delegate-invalid-core-emission") {
    const std::string filename = "delegate-invalid.cpp";
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, filename, false);
    SgGlobal *global = attachExactGlobalScope(source_file, filename);
    SgBasicBlock *scope =
        buildExactFunctionBody(global, SgName("rex_invalid_delegate_scope"));
    SgNullStatement *statement = new SgNullStatement();
    setExactSourceRange(statement, filename, 1, 1);
    SageInterface::appendStatement(statement, scope);
    ContractStatementDelegate delegate(
        statement, static_cast<UnparseDelegate::StatementCoreEmission>(999));
    unparser.delegate = &delegate;
    unparser.currentFile = &source_file;
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_current_scope(scope);
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseStatement(statement, info);
    return 0;
  }

  if (mode == "concept-missing-constraint") {
    SgNonrealDecl *concept = new SgNonrealDecl(SgName("rex_concept"));
    concept->set_is_concept(true);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseNonrealDecl(concept, info);
    return 0;
  }

  if (mode == "fortran-print-missing-format") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeSourceFile(fortran_file, "missing-print-format.f90", true);
    unparser.currentFile = &fortran_file;
    SgPrintStatement *print_statement = new SgPrintStatement();
    SgExprListExp *items = SageBuilder::buildExprListExp_nfi();
    print_statement->set_io_stmt_list(items);
    items->set_parent(print_statement);
    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    FortranCodeGeneration_locatedNode fortran(&unparser,
                                              "missing-print-format.f90");
    fortran.unparsePrintStatement(print_statement, info);
    return 0;
  }

  if (mode == "fortran-dimension-missing-source-owner") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeExactSourceFile(fortran_file, "dimension-source.f90", true);
    SgGlobal *global =
        attachExactGlobalScope(fortran_file, "dimension-source.f90");
    unparser.currentFile = &fortran_file;
    FortranDimensionFixture fixture =
        buildFortranDimensionFixture(global, false);
    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    info.set_language(SgFile::e_Fortran_language);
    FortranCodeGeneration_locatedNode fortran(&unparser,
                                              "dimension-source.f90");
    fortran.unparseAttributeSpecificationStatement(fixture.dimension, info);
    return 0;
  }

  if (mode == "requires-missing-requirements") {
    SgRequiresExpr *requires_expr =
        SageBuilder::buildRequiresExpr_nfi(nullptr, nullptr);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(requires_expr, info);
    return 0;
  }

  if (mode == "template-identity-only") {
    SgTemplateDeclaration *template_identity =
        new SgTemplateDeclaration(SgName("rex_identity"));
    template_identity->set_template_kind(
        SgTemplateDeclaration::e_template_class);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseTemplateDeclStmt(template_identity, info);
    return 0;
  }

  if (mode == "required-name-qualification-missing-record") {
    SgNonrealDecl *reference =
        new SgNonrealDecl(SgName("rex_missing_qualification"));
    SgNullStatement *use_site = new SgNullStatement();
    (void)unparser.u_name->lookup_type_qualification(reference, use_site);
    return 0;
  }

  if (mode == "name-qualification-unsupported-symbol-kind") {
    SgSymbolTable &symbol_table = *new SgSymbolTable();
    SgUnorderedNodeSet visible_alias_causes;
    (void)symbol_table.find_for_name_qualification(
        SgName("rex_bad_symbol_kind"), V_SgNode, nullptr, nullptr, nullptr,
        visible_alias_causes, false);
    return 0;
  }

  if (mode == "copy-fixup-unsupported-symbol-kind") {
    SgSymbolTable &symbol_table = *new SgSymbolTable();
    (void)symbol_table.find_for_ast_copy_fixup(
        SgName("rex_bad_copy_symbol_kind"), V_SgNode, nullptr, nullptr,
        nullptr);
    return 0;
  }

  if (mode == "copy-fixup-catch-scope") {
    SgSourceFile &target_file = *new SgSourceFile();
    initializeExactSourceFile(target_file, "copy-target.cpp", false);
    SgGlobal *target_global =
        attachExactGlobalScope(target_file, "copy-target.cpp");
    SgCatchOptionStmt *copy = SageBuilder::buildCatchOptionStmt();
    SgCatchOptionStmt *original = SageBuilder::buildCatchOptionStmt();
    SageBuilder::fixupCopyOfNodeFromSeparateFileInNewTargetAst(
        target_global, true, copy, original);
    return 0;
  }

  if (mode == "variable-partial-token") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "variable.cpp", false);
    SgGlobal *global = attachExactGlobalScope(source_file, "variable.cpp");
    unparser.currentFile = &source_file;
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "value", SageBuilder::buildIntType(), nullptr, global);
    SageInterface::appendStatement(declaration, global);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_unparsedPartiallyUsingTokenStream();
    unparser.u_exprStmt->unparseVarDeclStmt(declaration, info);
    return 0;
  }

  if (mode == "function-body-partial-token-missing-source" ||
      mode == "function-body-partial-token-disabled-source") {
    SgSourceFile *source_file = new SgSourceFile();
    initializeExactSourceFile(*source_file, "function-body.cpp", false);
    SgGlobal *global =
        attachExactGlobalScope(*source_file, "function-body.cpp");
    unparser.currentFile = source_file;
    SgUnparse_Info info;
    if (mode == "function-body-partial-token-disabled-source") {
      source_file->set_unparse_tokens(false);
      info.set_current_source_file(source_file);
    }
    info.set_unparsedPartiallyUsingTokenStream();

    const SgName function_name("rex_partial_token_body");
    SgType *return_type = SageBuilder::buildVoidType();
    SgFunctionDeclaration *canonical =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            function_name, return_type,
            SageBuilder::buildFunctionParameterList_nfi(), global);
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            function_name, return_type,
            SageBuilder::buildFunctionParameterList(), global, false, canonical,
            nullptr, false);
    SgFunctionDefinition *definition = function->get_definition();
    ROSE_ASSERT(definition != nullptr);
    SgBasicBlock *body = definition->get_body();
    ROSE_ASSERT(body != nullptr);
    ROSE_ASSERT(definition->get_body() == body);
    ROSE_ASSERT(body->get_parent() == definition);
    unparser.u_exprStmt->unparseBasicBlockStmt(body, info);
    return 0;
  }

  if (mode == "namespace-missing-fragments") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeSourceFile(source_file, "namespace.cpp", false);
    unparser.currentFile = &source_file;
    SgNamespaceDefinitionStatement *definition =
        SageBuilder::buildNamespaceDefinition();
    SgNamespaceDeclarationStatement *declaration =
        new SgNamespaceDeclarationStatement("missing", definition, false);
    definition->set_parent(declaration);
    definition->set_namespaceDeclaration(declaration);
    SageInterface::setOneSourcePositionForTransformation(declaration);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    unparser.u_exprStmt->unparseNamespaceDeclarationStatement(declaration,
                                                              info);
    return 0;
  }
  if (mode == "namespace-source-fragments-string-without-owner") {
    const std::string filename = "namespace-source.cpp";
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, filename, false);
    unparser.currentFile = &source_file;

    SgNamespaceDefinitionStatement *definition =
        SageBuilder::buildNamespaceDefinition();
    SgNamespaceDeclarationStatement *declaration =
        new SgNamespaceDeclarationStatement("source", definition, false);
    definition->set_parent(declaration);
    definition->set_namespaceDeclaration(declaration);

    const auto set_exact_range = [&](SgLocatedNode *node, int start_line,
                                     int start_column, int end_line,
                                     int end_column) {
      node->set_file_info(new Sg_File_Info(filename, start_line, start_column));
      node->set_startOfConstruct(
          new Sg_File_Info(filename, start_line, start_column));
      node->set_endOfConstruct(
          new Sg_File_Info(filename, end_line, end_column));
    };
    set_exact_range(declaration, 1, 1, 3, 1);
    set_exact_range(definition, 1, 18, 3, 1);

    SgNamespaceSourceFragment *opening = new SgNamespaceSourceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_opening,
        SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
    SgNamespaceSourceFragment *closing = new SgNamespaceSourceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_closing,
        SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
    set_exact_range(opening, 1, 1, 1, 18);
    set_exact_range(closing, 3, 1, 3, 1);
    opening->get_startOfConstruct()->set_source_sequence_number(1);
    opening->get_endOfConstruct()->set_source_sequence_number(2);
    closing->get_startOfConstruct()->set_source_sequence_number(3);
    closing->get_endOfConstruct()->set_source_sequence_number(3);
    declaration->initialize_translation_unit_source_order(1);
    declaration->attach_source_fragments(nullptr, opening, closing);

    SgUnparse_Info info;
    info.set_usedInUparseToStringFunction();
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseNamespaceDeclarationStatement(declaration,
                                                              info);
    return 0;
  }

  if (mode == "source-location-kind" ||
      mode == "source-location-missing-type") {
    const auto kind =
        mode == "source-location-kind"
            ? static_cast<SgSourceLocationBuiltinExp::
                              source_location_builtin_kind_enum>(999)
            : SgSourceLocationBuiltinExp::e_line;
    SgType *type = mode == "source-location-missing-type"
                       ? nullptr
                       : SageBuilder::buildUnsignedIntType();
    SgSourceLocationBuiltinExp *builtin =
        new SgSourceLocationBuiltinExp(kind, type);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(builtin, info);
    return 0;
  }

  if (mode == "source-int-missing-spelling") {
    markSourceLiteral(SageBuilder::buildIntVal_nfi(42))->unparseToString();
    return 0;
  }
  if (mode == "source-char-missing-spelling") {
    markSourceLiteral(SageBuilder::buildCharVal_nfi('A', ""))
        ->unparseToString();
    return 0;
  }
  if (mode == "source-float-missing-spelling") {
    markSourceLiteral(SageBuilder::buildFloatVal_nfi(1.25F))->unparseToString();
    return 0;
  }
  if (mode == "source-long-double-missing-spelling") {
    markSourceLiteral(SageBuilder::buildLongDoubleVal_nfi(1.25L, ""))
        ->unparseToString();
    return 0;
  }
  if (mode == "float80-missing-spelling") {
    markSourceLiteral(SageBuilder::buildFloat80Val_nfi(1.25L, ""))
        ->unparseToString();
    return 0;
  }
  if (mode == "float128-missing-spelling") {
    markSourceLiteral(SageBuilder::buildFloat128Val_nfi(1.25L, ""))
        ->unparseToString();
    return 0;
  }
  if (mode == "source-unnamed-enum-missing-spelling") {
    SgSourceFile *source =
        SageBuilder::buildGeneratedSourceFile("source-unnamed-enum.cpp");
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    SgGlobal *scope = source->get_globalScope();
    SgEnumDeclaration *declaration = SageBuilder::buildEnumDeclaration(
        SageBuilder::declaration_ownership::semanticAuxiliary(),
        SgName("rex_literal_enum"), false, scope);
    SgEnumVal *value = markSourceLiteral(
        SageBuilder::buildEnumVal_nfi(7, declaration, SgName()));
    SgUnparse_Info info;
    info.set_inEnumDecl();
    unparser.u_exprStmt->unparseEnumVal(value, info);
    return 0;
  }
  if (mode == "void-value") {
    SageBuilder::buildVoidVal()->unparseToString();
    return 0;
  }
  if (mode == "anonymous-class-type-name") {
    SgSourceFile *source_file =
        SageBuilder::buildGeneratedSourceFile("anonymous-type.cpp");
    ROSE_ASSERT(source_file != nullptr);
    source_file->set_Cxx_only(true);
    source_file->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *scope = source_file->get_globalScope();
    ROSE_ASSERT(scope != nullptr);
    SgClassDeclaration *declaration =
        SageBuilder::buildAnonymousStructDeclaration(
            SageBuilder::declaration_ownership::sourceLexical(),
            SgName("__anonymous_rex_type_identity"), scope);
    ROSE_ASSERT(declaration != nullptr);
    SgClassDeclaration *first_declaration =
        isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first_declaration != nullptr);
    first_declaration->set_definingDeclaration(declaration);
    ROSE_ASSERT(declaration->get_isUnNamed());
    SgUnparse_Info info;
    info.set_current_source_file(source_file);
    info.set_language(SgFile::e_Cxx_language);
    info.set_forceQualifiedNames();
    declaration->get_type()->unparseToString(&info);
    return 0;
  }
  if (mode == "context-free-named-type") {
    SgBasicBlock *scope = SageBuilder::buildBasicBlock();
    SgTypedefDeclaration *declaration = new SgTypedefDeclaration(
        SgName("rex_contextual_type"), SageBuilder::buildIntType(), nullptr,
        nullptr, nullptr);
    declaration->set_scope(scope);
    declaration->set_parent(scope);
    SgTypedefType *type = new SgTypedefType(declaration, nullptr);
    declaration->set_type(type);
    type->unparseToString();
    return 0;
  }
  if (mode == "context-free-declarator-type") {
    SageBuilder::buildPointerType(SageBuilder::buildIntType())
        ->unparseToString();
    return 0;
  }
  if (mode == "detached-bool-type") {
    SageBuilder::buildBoolType()->unparseToString();
    return 0;
  }
  if (mode == "constrained-auto-missing-source-spelling" ||
      mode == "unconstrained-auto-with-source-spelling") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeSourceFile(source_file, "constrained-auto.cpp", false);
    unparser.currentFile = &source_file;
    SgAutoType *auto_type = SageBuilder::buildAutoType();
    auto_type->set_is_constrained(mode ==
                                  "constrained-auto-missing-source-spelling");
    if (mode == "unconstrained-auto-with-source-spelling") {
      auto_type->set_source_constraint_spelling("rex_concept");
    }
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_type->unparseType(auto_type, info);
    return 0;
  }

  if (mode == "cxx-unknown-type-second" || mode == "cxx-default-type-skipped" ||
      mode == "cxx-imaginary-type-second") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeSourceFile(source_file, "bad-type.cpp", false);
    unparser.currentFile = &source_file;
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    SgType *type = nullptr;
    if (mode == "cxx-unknown-type-second") {
      type = SgTypeUnknown::createType();
      info.set_isTypeSecondPart();
    } else if (mode == "cxx-default-type-skipped") {
      type = SgTypeDefault::createType();
      info.set_isWithType();
      info.set_SkipBaseType();
    } else {
      type = SgTypeImaginary::createType(SageBuilder::buildFloatType());
      info.set_isTypeSecondPart();
    }
    unparser.u_type->unparseType(type, info);
    return 0;
  }

  if (mode == "typedef-unknown-source-form") {
    SgTypedefDeclaration *declaration = new SgTypedefDeclaration(
        SgName("rex_bad_alias"), SageBuilder::buildIntType(), nullptr, nullptr,
        nullptr);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseTypeDefStmt(declaration, info);
    return 0;
  }

  if (mode == "typedef-null-modifier-base" ||
      mode == "typedef-cyclic-modifier-base") {
    SgModifierType *modifier = new SgModifierType(SageBuilder::buildIntType());
    if (mode == "typedef-cyclic-modifier-base") {
      modifier->set_base_type(modifier);
    } else {
      modifier->set_base_type(nullptr);
    }
    SgTypedefDeclaration *declaration = new SgTypedefDeclaration(
        SgName("rex_bad_alias"), modifier, nullptr, nullptr, nullptr);
    declaration->set_typedef_type(SgTypedefDeclaration::e_typedef);
    SgUnparse_Info info;
    info.set_SkipBaseType();
    unparser.u_exprStmt->unparseTypeDefStmt(declaration, info);
    return 0;
  }

  if (mode == "constructor-missing-arguments" ||
      mode == "constructor-missing-type") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeSourceFile(source_file, "constructor.cpp", false);
    unparser.currentFile = &source_file;
    SgNullStatement *use_site = SageBuilder::buildNullStatement();
    SgExprListExp *arguments = mode == "constructor-missing-arguments"
                                   ? nullptr
                                   : SageBuilder::buildExprListExp_nfi();
    SgType *type = mode == "constructor-missing-type"
                       ? nullptr
                       : SageBuilder::buildIntType();
    SgConstructorInitializer *initializer = new SgConstructorInitializer(
        nullptr, arguments, type, true, false, false, true);
    if (mode == "constructor-missing-arguments") {
      initializer->set_args(nullptr);
    }
    if (arguments != nullptr) {
      arguments->set_parent(initializer);
    }
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_template_argument_qualification_context(use_site);
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "compound-literal-missing-owner" ||
      mode == "compound-literal-visible-owner") {
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "compound-literal.c", false);
    source_file.set_C_only(true);
    source_file.set_Cxx_only(false);
    SgGlobal *global =
        attachExactGlobalScope(source_file, "compound-literal.c");
    unparser.currentFile = &source_file;
    SgExprListExp *initializers = SageBuilder::buildExprListExp_nfi();
    SgAggregateInitializer *initializer =
        SageBuilder::buildAggregateInitializer_nfi(
            initializers, SageBuilder::buildIntType(),
            SgAggregateInitializer::
                e_aggregate_initializer_source_compound_literal);
    if (mode == "compound-literal-visible-owner") {
      SgVariableDeclaration *declaration =
          SageBuilder::buildVariableDeclaration(
              "value", SageBuilder::buildIntType(), initializer, global);
      SageInterface::appendStatement(declaration, global);
    }
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "assignment-initializer-missing-operand") {
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(1, "1");
    SgAssignInitializer *initializer = SageBuilder::buildAssignInitializer_nfi(
        operand, SageBuilder::buildIntType());
    initializer->set_operand_i(nullptr);
    operand->set_parent(nullptr);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "braced-initializer-missing-list" ||
      mode == "braced-initializer-null-element") {
    SgExprListExp *initializers = SageBuilder::buildExprListExp_nfi();
    if (mode == "braced-initializer-null-element") {
      initializers->get_expressions().push_back(nullptr);
    }
    SgBracedInitializer *initializer =
        new SgBracedInitializer(initializers, SageBuilder::buildIntType());
    if (initializers != nullptr) {
      initializers->set_parent(initializer);
    }
    if (mode == "braced-initializer-missing-list") {
      initializer->set_initializers(nullptr);
      initializers->set_parent(nullptr);
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "aggregate-initializer-missing-list" ||
      mode == "aggregate-initializer-null-element" ||
      mode == "aggregate-initializer-unclassified-source") {
    SgExprListExp *initializers = nullptr;
    if (mode != "aggregate-initializer-missing-list") {
      initializers = SageBuilder::buildExprListExp_nfi();
    }
    if (mode == "aggregate-initializer-null-element") {
      initializers->get_expressions().push_back(nullptr);
    }
    SgAggregateInitializer *initializer = new SgAggregateInitializer(
        initializers, SageBuilder::buildIntType(),
        SgAggregateInitializer::e_aggregate_initializer_source_braced);
    if (initializers != nullptr) {
      initializers->set_parent(initializer);
    }
    if (mode == "aggregate-initializer-unclassified-source") {
      initializer->set_source_form(
          SgAggregateInitializer::e_aggregate_initializer_source_unclassified);
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "designated-initializer-nondesignator") {
    SgExprListExp *designators = SageBuilder::buildExprListExp_nfi();
    SgIntVal *raw_designator = SageBuilder::buildIntVal(0);
    raw_designator->set_parent(designators);
    designators->append_expression(raw_designator);
    SgAssignInitializer *member = SageBuilder::buildAssignInitializer_nfi(
        SageBuilder::buildIntVal(1), SageBuilder::buildIntType());
    SgDesignatedInitializer *initializer =
        new SgDesignatedInitializer(designators, member);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(initializer, info);
    return 0;
  }

  if (mode == "fortran-aggregate-missing-list") {
    SgSourceFile &fortran_file = *new SgSourceFile();
    initializeSourceFile(fortran_file, "missing-array-constructor.f90", true);
    unparser.currentFile = &fortran_file;
    SgAggregateInitializer *initializer = new SgAggregateInitializer(
        static_cast<SgExprListExp *>(nullptr), SageBuilder::buildIntType(),
        SgAggregateInitializer::e_aggregate_initializer_source_fortran);
    SgUnparse_Info info;
    info.set_current_source_file(&fortran_file);
    FortranCodeGeneration_locatedNode fortran(&unparser,
                                              "missing-array-constructor.f90");
    fortran.unparseAggrInit(initializer, info);
    return 0;
  }

  if (mode == "static-assert-message-owner") {
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeSourceFile(cxx_file, "malformed-static-assert.cpp", false);
    unparser.currentFile = &cxx_file;
    SgBoolValExp *condition = SageBuilder::buildBoolValExp(true);
    SgStringVal *message = SageBuilder::buildStringVal("message");
    SgStaticAssertionDeclaration *declaration =
        new SgStaticAssertionDeclaration(condition, message);
    condition->set_parent(declaration);
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    unparser.u_exprStmt->unparseStaticAssertionDeclaration(declaration, info);
    return 0;
  }

  if (mode == "cuda-launch-bounds-owner") {
    SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
        SgName("rex_kernel"), static_cast<SgFunctionType *>(nullptr), nullptr);
    declaration->set_cuda_launch_bounds_expression(
        SageBuilder::buildIntVal_nfi(128, "128"));
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseCudaLaunchBounds(declaration, info);
    return 0;
  }

  if (mode == "declare-mapper-missing-user-identifier" ||
      mode == "declare-mapper-unspecified-identifier-kind") {
    SgOmpDeclareMapperStatement *mapper = new SgOmpDeclareMapperStatement();
    mapper->set_identifier(
        mode == "declare-mapper-missing-user-identifier"
            ? SgOmpClause::e_omp_declare_mapper_identifier_user
            : SgOmpClause::e_omp_declare_mapper_identifier_unspecified);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseOmpBeginDirectiveClauses(mapper, info);
    return 0;
  }

  if (mode == "openmp-device-kind" || mode == "openmp-vendor") {
    const bool invalid_device_kind = mode == "openmp-device-kind";
    SgOmpWhenClause *clause =
        new SgOmpWhenClause(static_cast<SgStatement *>(nullptr));
    SgOmpContextSelectorSet *selector_set = new SgOmpContextSelectorSet(
        invalid_device_kind
            ? SgOmpClause::e_omp_context_selector_set_device
            : SgOmpClause::e_omp_context_selector_set_implementation);
    SgOmpContextSelector *selector = new SgOmpContextSelector(
        invalid_device_kind ? SgOmpClause::e_omp_context_trait_kind
                            : SgOmpClause::e_omp_context_trait_vendor);
    SgOmpContextSelectorProperty *property = new SgOmpContextSelectorProperty();
    if (invalid_device_kind) {
      property->set_context_kind(
          static_cast<SgOmpClause::omp_when_context_kind_enum>(999));
    } else {
      property->set_context_vendor(
          static_cast<SgOmpClause::omp_when_context_vendor_enum>(999));
    }
    selector->get_properties().push_back(property);
    property->set_parent(selector);
    selector_set->get_selectors().push_back(selector);
    selector->set_parent(selector_set);
    clause->get_context_selector_sets().push_back(selector_set);
    selector_set->set_parent(clause);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseOmpWhenClause(clause, info);
    return 0;
  }

  if (mode == "openmp-requires-kind" ||
      mode == "openmp-requires-mismatched-payload") {
    SgOmpWhenClause *clause =
        new SgOmpWhenClause(static_cast<SgStatement *>(nullptr));
    SgOmpContextSelectorSet *selector_set = new SgOmpContextSelectorSet(
        SgOmpClause::e_omp_context_selector_set_implementation);
    SgOmpContextSelector *selector =
        new SgOmpContextSelector(SgOmpClause::e_omp_context_trait_requires);
    SgOmpContextSelectorProperty *property = new SgOmpContextSelectorProperty();
    property->set_requires_kind(
        mode == "openmp-requires-kind"
            ? static_cast<SgOmpClause::omp_requires_property_kind_enum>(999)
            : SgOmpClause::e_omp_requires_property_dynamic_allocators);
    if (mode == "openmp-requires-mismatched-payload") {
      property->set_requires_atomic_default_mem_order(
          SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire);
    }
    selector->get_properties().push_back(property);
    property->set_parent(selector);
    selector_set->get_selectors().push_back(selector);
    selector->set_parent(selector_set);
    clause->get_context_selector_sets().push_back(selector_set);
    selector_set->set_parent(clause);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseOmpWhenClause(clause, info);
    return 0;
  }

  if (mode == "openmp-adjust-invalid-modifier" ||
      mode == "openmp-adjust-missing-arguments" ||
      mode == "openmp-append-missing-operations" ||
      mode == "openmp-append-missing-modifier-list" ||
      mode == "openmp-append-missing-interop-type" ||
      mode == "openmp-append-invalid-depinfo" ||
      mode == "openmp-append-invalid-prefer-type-payload" ||
      mode == "openmp-init-missing-interop-type" ||
      mode == "openmp-init-invalid-depinfo" ||
      mode == "openmp-init-syntax-operand" ||
      mode == "openmp-append-invalid-list-replacement" ||
      mode == "openmp-init-duplicate-modifier-replacement" ||
      mode == "openmp-append-self-replacement") {
    auto build_modifier = [](SgOmpClause::omp_init_modifier_kind_enum kind,
                             SgExpression *expression = nullptr) {
      SgOmpInitModifier *modifier = new SgOmpInitModifier(kind, expression);
      if (expression != nullptr) {
        expression->set_parent(modifier);
      }
      return modifier;
    };
    auto append_modifier = [](SgOmpInitModifierList *list,
                              SgOmpInitModifier *modifier) {
      list->get_modifiers().push_back(modifier);
      modifier->set_parent(list);
    };
    auto build_target_list = [&]() {
      SgOmpInitModifierList *list = new SgOmpInitModifierList();
      append_modifier(list,
                      build_modifier(SgOmpClause::e_omp_init_modifier_target));
      return list;
    };
    auto append_operation = [](SgOmpAppendArgsClause *clause,
                               SgOmpInitModifierList *list) {
      SgOmpAppendArgsOperation *operation = new SgOmpAppendArgsOperation(list);
      if (list != nullptr) {
        list->set_parent(operation);
      }
      clause->get_interop_operations().push_back(operation);
      operation->set_parent(clause);
      return operation;
    };

    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    if (mode == "openmp-adjust-invalid-modifier" ||
        mode == "openmp-adjust-missing-arguments") {
      SgExprListExp *arguments = mode == "openmp-adjust-missing-arguments"
                                     ? nullptr
                                     : SageBuilder::buildExprListExp_nfi();
      if (arguments != nullptr) {
        SgOmpNameExpression *argument = new SgOmpNameExpression("rex_argument");
        arguments->append_expression(argument);
        argument->set_parent(arguments);
      }
      SgOmpAdjustArgsClause *clause = new SgOmpAdjustArgsClause(
          arguments, mode == "openmp-adjust-invalid-modifier"
                         ? SgOmpClause::e_omp_adjust_args_modifier_unknown
                         : SgOmpClause::e_omp_adjust_args_modifier_nothing);
      if (arguments != nullptr) {
        arguments->set_parent(clause);
      }
      unparser.u_exprStmt->unparseOmpAdjustArgsClause(clause, info);
      return 0;
    }

    if (mode == "openmp-append-missing-operations") {
      unparser.u_exprStmt->unparseOmpAppendArgsClause(
          new SgOmpAppendArgsClause(), info);
      return 0;
    }

    SgOmpAppendArgsClause *append_clause = new SgOmpAppendArgsClause();
    SgOmpInitModifierList *append_list = nullptr;
    if (mode != "openmp-append-missing-modifier-list") {
      append_list = mode == "openmp-append-missing-interop-type"
                        ? new SgOmpInitModifierList()
                        : build_target_list();
    }
    if (mode == "openmp-append-invalid-depinfo") {
      append_modifier(
          append_list,
          build_modifier(SgOmpClause::e_omp_init_modifier_depinfo_in,
                         SageBuilder::buildIntVal_nfi(1, "1")));
    }
    if (mode == "openmp-append-invalid-prefer-type-payload") {
      append_modifier(
          append_list,
          build_modifier(SgOmpClause::e_omp_init_modifier_prefer_type,
                         SageBuilder::buildIntVal_nfi(1, "1")));
    }
    SgOmpAppendArgsOperation *operation =
        append_operation(append_clause, append_list);
    if (mode == "openmp-append-invalid-list-replacement") {
      operation->replace_expression(append_list, new SgOmpInitModifierList());
      return 0;
    }
    if (mode == "openmp-append-self-replacement") {
      return operation->replace_expression(append_list, append_list) == 1 ? 0
                                                                          : 3;
    }
    if (mode == "openmp-init-duplicate-modifier-replacement") {
      SgOmpInitModifier *prefer =
          build_modifier(SgOmpClause::e_omp_init_modifier_prefer_type,
                         new SgOmpSourceExpression("{fr(\"cuda\")}"));
      append_modifier(append_list, prefer);
      operation->get_modifier_list()->replace_expression(
          prefer, build_modifier(SgOmpClause::e_omp_init_modifier_target));
      return 0;
    }
    if (mode == "openmp-init-missing-interop-type" ||
        mode == "openmp-init-invalid-depinfo" ||
        mode == "openmp-init-syntax-operand") {
      SgOmpInteropStatement *statement = new SgOmpInteropStatement();
      SgOmpInitModifierList *init_list = new SgOmpInitModifierList();
      if (mode == "openmp-init-invalid-depinfo" ||
          mode == "openmp-init-syntax-operand") {
        append_modifier(
            init_list, build_modifier(SgOmpClause::e_omp_init_modifier_target));
      }
      if (mode == "openmp-init-invalid-depinfo") {
        append_modifier(
            init_list,
            build_modifier(SgOmpClause::e_omp_init_modifier_depinfo_in,
                           SageBuilder::buildIntVal_nfi(1, "1")));
      }
      SgExpression *operand =
          mode == "openmp-init-syntax-operand"
              ? static_cast<SgExpression *>(
                    new SgOmpNameExpression("rex_interop_object"))
              : static_cast<SgExpression *>(
                    SageBuilder::buildIntVal_nfi(7, "7"));
      SgOmpInitClause *init = new SgOmpInitClause(init_list, operand);
      init_list->set_parent(init);
      operand->set_parent(init);
      statement->get_clause_list()->append_clause(init);
      unparser.u_exprStmt->unparseOmpClause(init, info);
      return 0;
    }
    unparser.u_exprStmt->unparseOmpAppendArgsClause(append_clause, info);
    return 0;
  }

  if (mode == "openmp-variables-missing-list" ||
      mode == "openmp-variables-null-item" ||
      mode == "openmp-variables-comma-item") {
    SgExprListExp *variables = mode == "openmp-variables-missing-list"
                                   ? nullptr
                                   : SageBuilder::buildExprListExp_nfi();
    SgOmpPrivateClause *clause = new SgOmpPrivateClause(variables);
    if (variables != nullptr) {
      variables->set_parent(clause);
      if (mode == "openmp-variables-null-item") {
        variables->get_expressions().push_back(nullptr);
      } else {
        SgOmpNameExpression *lhs = new SgOmpNameExpression("rex_omp_left");
        SgOmpNameExpression *rhs = new SgOmpNameExpression("rex_omp_right");
        SgCommaOpExp *comma =
            SageBuilder::buildCommaOpExp(lhs, rhs, SageBuilder::buildIntType());
        variables->append_expression(comma);
        comma->set_parent(variables);
      }
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseOmpVariablesClause(clause, info);
    return 0;
  }

  if (mode == "openmp-source-override-missing-list" ||
      mode == "openmp-source-list-missing-discriminator") {
    SgExprListExp *variables = SageBuilder::buildExprListExp_nfi();
    variables->append_expression(new SgOmpNameExpression("rex_semantic"));
    SgOmpPrivateClause *clause = new SgOmpPrivateClause(variables);
    variables->set_parent(clause);
    if (mode == "openmp-source-override-missing-list") {
      clause->set_has_source_variables_override(true);
    } else {
      SgExprListExp *source_variables = SageBuilder::buildExprListExp_nfi();
      source_variables->append_expression(
          new SgOmpSourceExpression("REX_SOURCE"));
      clause->set_source_variables(source_variables);
      source_variables->set_parent(clause);
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseOmpVariablesClause(clause, info);
    return 0;
  }

  if (mode == "openmp-map-unwrapped-item" ||
      mode == "openmp-map-invalid-policy-payload") {
    SgSourceFile *source_file = nullptr;
    if (mode == "openmp-map-invalid-policy-payload") {
      source_file = new SgSourceFile();
      initializeExactSourceFile(*source_file, "openmp-map-item.cpp", false);
      unparser.currentFile = source_file;
    }
    SgExprListExp *variables = SageBuilder::buildExprListExp_nfi();
    SgOmpMapClause *clause =
        new SgOmpMapClause(variables, SgOmpClause::e_omp_map_to);
    variables->set_parent(clause);
    if (mode == "openmp-map-unwrapped-item") {
      variables->append_expression(new SgOmpNameExpression("rex_data"));
    } else {
      SgOmpNameExpression *locator = markSourceLiteral(
          new SgOmpNameExpression("rex_data[0:4]"), "openmp-map-item.cpp");
      SgOmpMapItem *item = new SgOmpMapItem(locator);
      locator->set_parent(item);
      SgIntVal *argument = SageBuilder::buildIntVal_nfi(2, "2");
      SgOmpMapDistDataPolicy *policy = new SgOmpMapDistDataPolicy(
          SgOmpClause::e_omp_map_dist_data_duplicate, argument);
      argument->set_parent(policy);
      item->get_policies().push_back(policy);
      policy->set_parent(item);
      variables->append_expression(item);
    }
    SgUnparse_Info info;
    if (source_file != nullptr) {
      info.set_current_source_file(source_file);
      info.set_language(SgFile::e_Cxx_language);
    }
    unparser.u_exprStmt->unparseOmpVariablesClause(clause, info);
    return 0;
  }

  if (mode == "switch-missing-selector" || mode == "switch-foreign-selector" ||
      mode == "switch-missing-body" || mode == "switch-foreign-body" ||
      mode == "switch-aliased-roles") {
    SgExprStatement *selector =
        SageBuilder::buildExprStatement(SageBuilder::buildIntVal(1));
    SgBasicBlock *body = SageBuilder::buildBasicBlock();
    SgSwitchStatement *switch_statement =
        SageBuilder::buildSwitchStatement(selector, body);
    if (mode == "switch-missing-selector") {
      switch_statement->set_item_selector(nullptr);
    } else if (mode == "switch-foreign-selector") {
      selector->set_parent(SageBuilder::buildBasicBlock());
    } else if (mode == "switch-missing-body") {
      switch_statement->set_body(nullptr);
    } else if (mode == "switch-aliased-roles") {
      switch_statement->set_body(selector);
      selector->set_parent(switch_statement);
    } else {
      body->set_parent(SageBuilder::buildBasicBlock());
    }
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseSwitchStmt(switch_statement, info);
    return 0;
  }

  if (mode == "case-missing-key" || mode == "case-foreign-key" ||
      mode == "case-missing-body" || mode == "case-foreign-body" ||
      mode == "case-foreign-range-end" || mode == "case-aliased-range-end") {
    SgIntVal *key = SageBuilder::buildIntVal(1);
    SgNullStatement *body = SageBuilder::buildNullStatement();
    SgCaseOptionStmt *case_statement =
        SageBuilder::buildCaseOptionStmt(key, body);
    if (mode == "case-missing-key") {
      case_statement->set_key(nullptr);
    } else if (mode == "case-foreign-key") {
      key->set_parent(SageBuilder::buildBasicBlock());
    } else if (mode == "case-missing-body") {
      case_statement->set_body(nullptr);
    } else if (mode == "case-foreign-body") {
      body->set_parent(SageBuilder::buildBasicBlock());
    } else if (mode == "case-aliased-range-end") {
      case_statement->set_key_range_end(key);
      key->set_parent(case_statement);
    } else {
      SgIntVal *range_end = SageBuilder::buildIntVal(3);
      case_statement->set_key_range_end(range_end);
      range_end->set_parent(SageBuilder::buildBasicBlock());
    }
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseCaseStmt(case_statement, info);
    return 0;
  }

  if (mode == "default-missing-body" || mode == "default-foreign-body") {
    SgNullStatement *body = SageBuilder::buildNullStatement();
    SgDefaultOptionStmt *default_statement =
        SageBuilder::buildDefaultOptionStmt(body);
    if (mode == "default-missing-body") {
      default_statement->set_body(nullptr);
    } else {
      body->set_parent(SageBuilder::buildBasicBlock());
    }
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseDefaultStmt(default_statement, info);
    return 0;
  }

  if (mode == "coroutine-missing-keyword" ||
      mode == "coroutine-invalid-keyword" ||
      mode == "coroutine-missing-operand") {
    SgAwaitExpression *await_expression = nullptr;
    if (mode == "coroutine-missing-operand") {
      await_expression =
          new SgAwaitExpression(nullptr, SageBuilder::buildIntType());
    } else {
      SgIntVal *operand = SageBuilder::buildIntVal_nfi(7, "7");
      await_expression = SageBuilder::buildAwaitExpression_nfi(
          operand, SageBuilder::buildIntType());
    }
    await_expression->set_coroutine_keyword_kind(
        mode == "coroutine-missing-keyword"
            ? SgAwaitExpression::e_coroutine_keyword_unspecified
        : mode == "coroutine-invalid-keyword"
            ? static_cast<SgAwaitExpression::coroutine_keyword_kind_enum>(999)
            : SgAwaitExpression::e_coroutine_keyword_co_await);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseLanguageSpecificExpression(await_expression,
                                                           info);
    return 0;
  }

  if (mode == "coroutine-invalid-return-kind") {
    SgReturnStmt *return_statement = SageBuilder::buildReturnStmt();
    return_statement->set_return_keyword_kind(
        static_cast<SgReturnStmt::return_keyword_kind_enum>(999));
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseReturnStmt(return_statement, info);
    return 0;
  }

  if (mode == "gnu-visibility-unknown" || mode == "gnu-visibility-error" ||
      mode == "gnu-type-visibility-unknown" ||
      mode == "gnu-type-visibility-error") {
    SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
        SgName("rex_visibility"), static_cast<SgFunctionType *>(nullptr),
        nullptr);
    const bool type_visibility = mode == "gnu-type-visibility-unknown" ||
                                 mode == "gnu-type-visibility-error";
    const auto invalid_visibility =
        mode == "gnu-visibility-unknown" ||
                mode == "gnu-type-visibility-unknown"
            ? SgDeclarationModifier::e_unknown_visibility
            : SgDeclarationModifier::e_error_visibility;
    if (type_visibility) {
      declaration->get_declarationModifier().set_gnu_type_visibility(
          invalid_visibility);
    } else {
      declaration->get_declarationModifier().set_gnu_attribute_visibility(
          invalid_visibility);
    }
    SgUnparse_Info info;
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_sage->printAttributes(declaration, info);
    return 0;
  }

  if (mode == "openacc-default") {
    SgAccDefaultClause *clause = new SgAccDefaultClause(999);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseAccClause(clause, info);
    return 0;
  }
  if (mode == "openacc-reduction") {
    SgExprListExp *variables = SageBuilder::buildExprListExp();
    SgAccReductionClause *clause = new SgAccReductionClause(variables, 999);
    variables->set_parent(clause);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseAccClause(clause, info);
    return 0;
  }

  if (mode == "cxx-udl-missing-declaration" ||
      mode == "cxx-udl-missing-operator-syntax" ||
      mode == "cxx-udl-missing-suffix" || mode == "cxx-udl-missing-operand" ||
      mode == "cxx-call-default-argument") {
    const bool missing_declaration = mode == "cxx-udl-missing-declaration";
    const bool default_argument = mode == "cxx-call-default-argument";
    const bool missing_operand = mode == "cxx-udl-missing-operand";
    CxxFunctionCallFixture fixture(!missing_declaration, !default_argument,
                                   !missing_operand, default_argument,
                                   "function-call-contract.cpp");
    fixture.call->set_uses_operator_syntax(
        !default_argument && mode != "cxx-udl-missing-operator-syntax");
    if (!default_argument) {
      fixture.call->set_source_operator_surface(
          SgFunctionCallExp::e_user_defined_literal_surface);
      fixture.call->set_source_operator_callee_form(
          SgFunctionCallExp::e_nonmember_operator_callee);
      if (!fixture.arguments->get_expressions().empty()) {
        fixture.call->set_source_operator_operand_roles(
            SgUnsignedCharList{SgFunctionCallExp::e_semantic_operator_operand});
      }
    }
    if (!default_argument && mode != "cxx-udl-missing-suffix") {
      fixture.call->set_source_user_defined_literal_suffix(
          SgName("_typed_suffix"));
    }
    fixture.unparse(unparser);
    return 0;
  }

  if (mode == "cxx-source-pragma-missing-payload" ||
      mode == "cxx-pragma-missing-kind" ||
      mode == "cxx-generated-pragma-source-payload") {
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration("ident payload", nullptr);
    if (mode == "cxx-source-pragma-missing-payload") {
      pragma->set_cxx_pragma_payload_kind(
          SgPragmaDeclaration::e_cxx_pragma_source_spelled);
    } else if (mode == "cxx-pragma-missing-kind") {
      pragma->set_cxx_pragma_payload_kind(
          SgPragmaDeclaration::e_cxx_pragma_payload_none);
    } else {
      pragma->set_cxx_source_text("ident source");
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparsePragmaDeclStmt(pragma, info);
    return 0;
  }

  if (mode == "class-type-declaration-inside-preprocessing") {
    const std::string filename = "embedded-preprocessing-owner.cpp";
    SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(filename);
    ROSE_ASSERT(sourceFile != nullptr);
    sourceFile->set_Cxx_only(true);
    sourceFile->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *scope = sourceFile->get_globalScope();
    ROSE_ASSERT(scope != nullptr);
    SgClassDeclaration *declaration = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexEmbeddedPreprocessingOwner"), scope);
    ROSE_ASSERT(declaration != nullptr);
    declaration->set_isAutonomousDeclaration(false);
    declaration->set_isUnNamed(true);
    declaration->addToAttachedPreprocessingInfo(
        buildInsideComment(filename, "/*misowned embedded inside syntax*/\n"),
        PreprocessingInfo::after);

    unparser.currentFile = sourceFile;
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_language(SgFile::e_Cxx_language);
    info.set_isTypeFirstPart();
    info.set_declstatement_ptr(declaration);
    unparser.u_type->unparseType(declaration->get_type(), info);
    return 0;
  }

  if (mode == "class-declaration-inside-preprocessing" ||
      mode == "namespace-declaration-inside-preprocessing") {
    SgDeclarationStatement *declaration = nullptr;
    if (mode == "class-declaration-inside-preprocessing") {
      declaration =
          new SgClassDeclaration(SgName("RexPreprocessingOwner"),
                                 SgClassDeclaration::e_class, nullptr, nullptr);
    } else {
      declaration = new SgNamespaceDeclarationStatement(
          SgName("rex_preprocessing_owner"), nullptr, false);
    }
    declaration->addToAttachedPreprocessingInfo(
        buildInsideComment("preprocessing-owner.cpp",
                           "/*misowned inside syntax*/\n"),
        PreprocessingInfo::after);
    SgUnparse_Info info;
    if (SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration)) {
      unparser.u_exprStmt->unparseClassDeclStmt(classDeclaration, info);
    } else {
      unparser.u_exprStmt->unparseNamespaceDeclarationStatement(declaration,
                                                                info);
    }
    return 0;
  }

  if (!mode.empty()) {
    return 3;
  }

  {
    const std::string filename = "delegate-envelope.cpp";
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, filename, false);
    SgGlobal *global = attachExactGlobalScope(source_file, filename);
    SgBasicBlock *scope =
        buildExactFunctionBody(global, SgName("rex_delegate_scope"));
    SgNullStatement *delegated_statement = new SgNullStatement();
    SgNullStatement *ordinary_statement = new SgNullStatement();
    setExactSourceRange(delegated_statement, filename, 2, 2);
    setExactSourceRange(ordinary_statement, filename, 4, 4);
    SageInterface::appendStatement(delegated_statement, scope);
    SageInterface::appendStatement(ordinary_statement, scope);
    delegated_statement->addToAttachedPreprocessingInfo(
        new PreprocessingInfo(PreprocessingInfo::C_StyleComment,
                              "/* delegate before */\n", filename, 1, 1, 1,
                              PreprocessingInfo::before),
        PreprocessingInfo::before);
    delegated_statement->addToAttachedPreprocessingInfo(
        new PreprocessingInfo(PreprocessingInfo::C_StyleComment,
                              "/* delegate after */\n", filename, 3, 1, 1,
                              PreprocessingInfo::after),
        PreprocessingInfo::after);

    ContractStatementDelegate delegate(
        delegated_statement, UnparseDelegate::StatementCoreEmission::emitted);
    std::ostringstream delegate_output;
    Unparser delegate_unparser(&delegate_output, filename, options, nullptr,
                               &delegate);
    delegate_unparser.currentFile = &source_file;
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_current_scope(scope);
    info.set_language(SgFile::e_Cxx_language);
    delegate_unparser.u_exprStmt->unparseStatement(delegated_statement, info);
    delegate_unparser.u_exprStmt->unparseStatement(ordinary_statement, info);

    const std::string emitted = delegate_output.str();
    const size_t before = emitted.find("/* delegate before */");
    const size_t core = emitted.find("rex_delegated();");
    const size_t after = emitted.find("/* delegate after */");
    if (delegate.callCount() != 2 || before == std::string::npos ||
        core == std::string::npos || after == std::string::npos ||
        !(before < core && core < after) ||
        countOccurrences(emitted, "/* delegate before */") != 1 ||
        countOccurrences(emitted, "rex_delegated();") != 1 ||
        countOccurrences(emitted, "/* delegate after */") != 1 ||
        emitted.find(';', after) == std::string::npos) {
      std::cerr << "delegate did not preserve the statement envelope: "
                << emitted << std::endl;
      return 28;
    }
  }

  {
    Unparser_Opt lineOptions;
    lineOptions.set_linefile_opt(true);
    std::ostringstream lineOutput;
    Unparser lineUnparser(&lineOutput, "line-directive.cpp", lineOptions);

    SgNullStatement *first = new SgNullStatement();
    SgNullStatement *duplicate = new SgNullStatement();
    SgBasicBlock *excluded = new SgBasicBlock();
    SgNullStatement *afterExcluded = new SgNullStatement();
    SgNullStatement *differentLine = new SgNullStatement();
    SgNullStatement *differentFile = new SgNullStatement();
    setExactSourceRange(first, "line-a.cpp", 10, 10);
    setExactSourceRange(duplicate, "line-a.cpp", 10, 10);
    setExactSourceRange(excluded, "line-b.cpp", 20, 20);
    setExactSourceRange(afterExcluded, "line-a.cpp", 10, 10);
    setExactSourceRange(differentLine, "line-a.cpp", 11, 11);
    setExactSourceRange(differentFile, "line-b.cpp", 10, 10);
    lineUnparser.u_exprStmt->unparseLineDirectives(first);
    lineUnparser.u_exprStmt->unparseLineDirectives(duplicate);
    lineUnparser.u_exprStmt->unparseLineDirectives(excluded);
    lineUnparser.u_exprStmt->unparseLineDirectives(afterExcluded);
    lineUnparser.u_exprStmt->unparseLineDirectives(differentLine);
    lineUnparser.u_exprStmt->unparseLineDirectives(differentFile);
    if (countOccurrences(lineOutput.str(), "#line 10 \"line-a.cpp\"") != 1 ||
        countOccurrences(lineOutput.str(), "#line 11 \"line-a.cpp\"") != 1 ||
        countOccurrences(lineOutput.str(), "#line 10 \"line-b.cpp\"") != 1) {
      std::cerr << "line-directive cache did not preserve exact emitted state"
                << std::endl;
      return 4;
    }

    std::ostringstream isolatedOutput;
    Unparser isolatedUnparser(&isolatedOutput, "line-directive.cpp",
                              lineOptions);
    isolatedUnparser.u_exprStmt->unparseLineDirectives(first);
    if (countOccurrences(isolatedOutput.str(), "#line 10 \"line-a.cpp\"") !=
        1) {
      std::cerr << "line-directive cache leaked across unparser sessions"
                << std::endl;
      return 4;
    }
  }

  {
    auto qualificationDepthWithUsingOrder = [](bool usingBeforePosition,
                                               bool nestedPosition) {
      SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
          "name-qualification-using-order-positive.cpp");
      ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
      SgGlobal *global = source->get_globalScope();
      SgClassDeclaration *target = SageBuilder::buildClassDeclaration(
          SageBuilder::declaration_ownership::sourceLexical(),
          SgName("RexUsingOrderTarget"), global);
      SgNamespaceDeclarationStatement *importedNamespace =
          SageBuilder::buildNamespaceDeclaration("rex_using_order", global);
      ROSE_ASSERT(importedNamespace != nullptr &&
                  importedNamespace->get_definition() != nullptr);
      ROSE_ASSERT(SageBuilder::buildClassDeclaration(
                      SageBuilder::declaration_ownership::sourceLexical(),
                      SgName("RexUsingOrderTarget"),
                      importedNamespace->get_definition()) != nullptr);

      SgUsingDirectiveStatement *directive =
          SageBuilder::buildUsingDirectiveStatement(importedNamespace);
      SgVariableDeclaration *position = nullptr;
      SgScopeStatement *currentScope = global;
      if (nestedPosition) {
        ROSE_ASSERT(usingBeforePosition);
        SageInterface::appendStatement(directive, global);
        SgNamespaceDeclarationStatement *container =
            SageBuilder::buildNamespaceDeclaration("rex_using_container",
                                                   global);
        ROSE_ASSERT(container != nullptr &&
                    container->get_definition() != nullptr);
        currentScope = container->get_definition();
        position = SageBuilder::buildVariableDeclaration(
            "rex_using_order_position", SageBuilder::buildIntType(), nullptr,
            currentScope);
        SageInterface::appendStatement(position, currentScope);
      } else if (usingBeforePosition) {
        position = SageBuilder::buildVariableDeclaration(
            "rex_using_order_position", SageBuilder::buildIntType(), nullptr,
            global);
        SageInterface::appendStatement(directive, global);
        SageInterface::appendStatement(position, global);
      } else {
        position = SageBuilder::buildVariableDeclaration(
            "rex_using_order_position", SageBuilder::buildIntType(), nullptr,
            global);
        SageInterface::appendStatement(position, global);
        SageInterface::appendStatement(directive, global);
      }
      ROSE_ASSERT(position != nullptr);

      NameQualificationTraversal::NameQualificationMapType names;
      NameQualificationTraversal::NameQualificationMapType types;
      NameQualificationTraversal::NameQualificationMapOfMapsType typeMaps;
      NameQualificationTraversal::NameQualificationSetType referenced;
      NameQualificationContext qualifications;
      NameQualificationTraversal traversal(names, types, typeMaps, referenced,
                                           qualifications);
      return traversal.nameQualificationDepth(target, currentScope, position);
    };

    if (qualificationDepthWithUsingOrder(true, false) <= 0 ||
        qualificationDepthWithUsingOrder(false, false) != 0 ||
        qualificationDepthWithUsingOrder(true, true) <= 0) {
      std::cerr << "using-directive qualification order was not preserved"
                << std::endl;
      return 4;
    }

    auto qualificationDepthWithTypedSourceOrder =
        [](unsigned int directiveOrder, unsigned int positionOrder) {
          SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
              "name-qualification-using-typed-order-positive.cpp");
          ROSE_ASSERT(source != nullptr &&
                      source->get_globalScope() != nullptr);
          SgGlobal *global = source->get_globalScope();
          SgClassDeclaration *target = SageBuilder::buildClassDeclaration(
              SageBuilder::declaration_ownership::sourceLexical(),
              SgName("RexUsingTypedOrderTarget"), global);
          SgNamespaceDeclarationStatement *importedNamespace =
              SageBuilder::buildNamespaceDeclaration("rex_using_typed_order",
                                                     global);
          ROSE_ASSERT(target != nullptr && importedNamespace != nullptr &&
                      importedNamespace->get_definition() != nullptr);
          ROSE_ASSERT(SageBuilder::buildClassDeclaration(
                          SageBuilder::declaration_ownership::sourceLexical(),
                          SgName("RexUsingTypedOrderTarget"),
                          importedNamespace->get_definition()) != nullptr);

          // A reopened namespace has one semantic declarative region but
          // distinct lexical source fragments.  A use in one fragment has no
          // direct-child index in the other fragment, so this is the valid
          // source shape that requires translation-unit source order instead
          // of a fabricated same-list position.
          SgNamespaceDeclarationStatement *firstFragment =
              SageBuilder::buildNamespaceDeclaration(
                  "rex_using_typed_position_container", global);
          SgNamespaceDeclarationStatement *secondFragment =
              SageBuilder::buildNamespaceDeclaration(
                  "rex_using_typed_position_container", global);
          ROSE_ASSERT(firstFragment != nullptr &&
                      firstFragment->get_definition() != nullptr &&
                      secondFragment != nullptr &&
                      secondFragment->get_definition() != nullptr);
          SgScopeStatement *earlierScope = firstFragment->get_definition();
          SgScopeStatement *laterScope = secondFragment->get_definition();
          ROSE_ASSERT(
              SgScopeStatement::isEquivalentScope(earlierScope, laterScope));
          SgScopeStatement *directiveScope =
              directiveOrder < positionOrder ? earlierScope : laterScope;
          SgScopeStatement *positionScope =
              directiveOrder < positionOrder ? laterScope : earlierScope;

          SgUsingDirectiveStatement *directive =
              SageBuilder::buildUsingDirectiveStatement(importedNamespace);
          setExactSourceRange(
              directive, "name-qualification-using-typed-order-positive.cpp", 2,
              2);
          directive->get_startOfConstruct()->set_source_sequence_number(
              directiveOrder);
          directive->initialize_translation_unit_source_order(directiveOrder);
          SageInterface::appendStatement(directive, directiveScope);

          SgVariableDeclaration *position =
              SageBuilder::buildVariableDeclaration("rex_using_typed_position",
                                                    SageBuilder::buildIntType(),
                                                    nullptr, positionScope);
          setExactSourceRange(
              position, "name-qualification-using-typed-order-positive.cpp", 3,
              3);
          position->get_startOfConstruct()->set_source_sequence_number(
              positionOrder);
          position->initialize_translation_unit_source_order(positionOrder);
          SageInterface::appendStatement(position, positionScope);

          NameQualificationTraversal::NameQualificationMapType names;
          NameQualificationTraversal::NameQualificationMapType types;
          NameQualificationTraversal::NameQualificationMapOfMapsType typeMaps;
          NameQualificationTraversal::NameQualificationSetType referenced;
          NameQualificationContext qualifications;
          NameQualificationTraversal traversal(names, types, typeMaps,
                                               referenced, qualifications);
          return traversal.nameQualificationDepth(target, directiveScope,
                                                  position);
        };

    if (qualificationDepthWithTypedSourceOrder(10, 20) <= 0 ||
        qualificationDepthWithTypedSourceOrder(20, 10) != 0) {
      std::cerr << "typed using-directive source order was not preserved"
                << std::endl;
      return 4;
    }
  }

  {
    std::ostringstream namespace_output;
    Unparser namespace_unparser(&namespace_output, "namespace-generated.cpp",
                                options);
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "namespace-generated.cpp", false);
    SgGlobal *global =
        attachExactGlobalScope(source_file, "namespace-generated.cpp");
    namespace_unparser.currentFile = &source_file;
    SgNamespaceDeclarationStatement *declaration =
        SageBuilder::buildNamespaceDeclaration("generated", global);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);
    namespace_unparser.u_exprStmt->unparseNamespaceDeclarationStatement(
        declaration, info);
    const std::string text = namespace_output.str();
    const size_t name = text.find("namespace generated");
    const size_t opening = text.find('{', name);
    const size_t closing = text.find('}', opening);
    if (name == std::string::npos || opening == std::string::npos ||
        closing == std::string::npos || opening >= closing) {
      std::cerr << "unexpected generated namespace spelling: " << text
                << std::endl;
      return 4;
    }
  }

  {
    SgFunctionDeclaration &declaration = *new SgFunctionDeclaration(
        SgName("rex_default_visibility"),
        static_cast<SgFunctionType *>(nullptr), nullptr);
    if (declaration.get_declarationModifier().get_gnu_attribute_visibility() !=
        SgDeclarationModifier::e_unspecified_visibility) {
      return 4;
    }

    SgDeclarationModifier *original = new SgDeclarationModifier();
    original->set_gnu_attribute_visibility(
        SgDeclarationModifier::e_hidden_visibility);
    original->set_gnu_type_visibility(
        SgDeclarationModifier::e_internal_visibility);
    SgDeclarationModifier *copied = new SgDeclarationModifier(*original);
    if (copied->get_gnu_attribute_visibility() !=
            SgDeclarationModifier::e_hidden_visibility ||
        copied->get_gnu_type_visibility() !=
            SgDeclarationModifier::e_internal_visibility ||
        *copied == declaration.get_declarationModifier()) {
      return 4;
    }
    SgDeclarationModifier *assigned = new SgDeclarationModifier();
    *assigned = *original;
    if (assigned->get_gnu_attribute_visibility() !=
            SgDeclarationModifier::e_hidden_visibility ||
        assigned->get_gnu_type_visibility() !=
            SgDeclarationModifier::e_internal_visibility) {
      return 4;
    }
    assigned->reset();
    if (assigned->get_gnu_attribute_visibility() !=
            SgDeclarationModifier::e_unspecified_visibility ||
        assigned->get_gnu_type_visibility() !=
            SgDeclarationModifier::e_unspecified_visibility) {
      return 4;
    }
  }

  if (SageBuilder::buildIntVal(42)->unparseToString() != "42" ||
      SageBuilder::buildCharVal('A')->unparseToString() != "'A'" ||
      SageBuilder::buildFloatVal(1.25F)->unparseToString() !=
          expectedCanonicalFloating(1.25F, "F") ||
      SageBuilder::buildDoubleVal(1.25)->unparseToString() !=
          expectedCanonicalFloating(1.25, "") ||
      SageBuilder::buildLongDoubleVal(1.25L)->unparseToString() !=
          expectedCanonicalFloating(1.25L, "L") ||
      SageBuilder::buildFloat80Val(1.25L)->unparseToString() !=
          expectedCanonicalFloating(1.25L, "L") ||
      SageBuilder::buildFloat128Val(1.25L)->unparseToString() !=
          expectedCanonicalFloating(1.25L, "Q")) {
    return 4;
  }

  const std::string source_literal_file = "source-literal-spelling.cpp";
  if (markSourceLiteral(SageBuilder::buildIntVal_nfi(42, "0x2a"),
                        source_literal_file)
              ->unparseToString() != "0x2a" ||
      markSourceLiteral(SageBuilder::buildIntVal_nfi(42, "052"),
                        source_literal_file)
              ->unparseToString() != "052" ||
      markSourceLiteral(SageBuilder::buildIntVal_nfi(1000, "1'000"),
                        source_literal_file)
              ->unparseToString() != "1'000" ||
      markSourceLiteral(SageBuilder::buildUnsignedShortVal_nfi(42, "052u"),
                        source_literal_file)
              ->unparseToString() != "052u" ||
      markSourceLiteral(SageBuilder::buildCharVal_nfi('A', "'\\x41'"),
                        source_literal_file)
              ->unparseToString() != "'\\x41'" ||
      markSourceLiteral(SageBuilder::buildFloatVal_nfi(1.25F, "0x1.4p+0F"),
                        source_literal_file)
              ->unparseToString() != "0x1.4p+0F" ||
      markSourceLiteral(SageBuilder::buildLongDoubleVal_nfi(1.25L, "1.25L"),
                        source_literal_file)
              ->unparseToString() != "1.25L" ||
      markSourceLiteral(SageBuilder::buildFloat80Val_nfi(1.25L, "0xap-3L"),
                        source_literal_file)
              ->unparseToString() != "0xap-3L" ||
      markSourceLiteral(SageBuilder::buildFloat128Val_nfi(1.25L, "0xap-3Q"),
                        source_literal_file)
              ->unparseToString() != "0xap-3Q") {
    return 4;
  }

  {
    std::ostringstream enum_output;
    Unparser enum_unparser(&enum_output, "literal-enum.cpp", options);
    SgSourceFile *enum_file =
        SageBuilder::buildGeneratedSourceFile("literal-enum.cpp");
    ROSE_ASSERT(enum_file != nullptr &&
                enum_file->get_globalScope() != nullptr);
    SgGlobal *scope = enum_file->get_globalScope();
    SgEnumDeclaration *declaration = SageBuilder::buildEnumDeclaration(
        SageBuilder::declaration_ownership::semanticAuxiliary(),
        SgName("rex_literal_enum"), false, scope);

    SgEnumVal *generated = SageBuilder::buildEnumVal(7, declaration, SgName());
    enum_unparser.get_name_qualification_context().record(
        generated, declaration, {"", 0, false, false});
    SgUnparse_Info declaration_info;
    declaration_info.set_inEnumDecl();
    declaration_info.set_template_argument_qualification_context(declaration);
    enum_unparser.u_exprStmt->unparseEnumVal(generated, declaration_info);

    SgEnumVal *named = SageBuilder::buildEnumVal_nfi(
        8, declaration, SgName("__anonymous_literal"));
    enum_unparser.get_name_qualification_context().record(
        named, declaration, {"", 0, false, false});
    SgUnparse_Info reference_info;
    reference_info.set_template_argument_qualification_context(declaration);
    enum_unparser.u_exprStmt->unparseEnumVal(named, reference_info);
    if (enum_output.str() != "7__anonymous_literal") {
      return 4;
    }
  }

  {
    std::ostringstream nullptr_output;
    Unparser nullptr_unparser(&nullptr_output, "nullptr.cpp", options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeSourceFile(cxx_file, "nullptr.cpp", false);
    nullptr_unparser.currentFile = &cxx_file;
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    info.set_language(SgFile::e_Cxx_language);
    nullptr_unparser.u_type->unparseType(SageBuilder::buildNullptrType(), info);
    if (nullptr_output.str() != "decltype(nullptr)") {
      return 5;
    }
  }

  {
    std::ostringstream vector_output;
    Unparser vector_unparser(&vector_output, "qualified-vector.cpp", options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeExactSourceFile(cxx_file, "qualified-vector.cpp", false);
    SgGlobal *global = attachExactGlobalScope(cxx_file, "qualified-vector.cpp");
    vector_unparser.currentFile = &cxx_file;
    SgTypedefDeclaration *lane_declaration =
        SageBuilder::buildTypedefDeclaration(
            SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
            SgTypedefDeclaration::e_typedef, SgName("Lane"),
            SageBuilder::buildUnsignedIntType(), global);
    SgTypeModifier vector_modifier;
    vector_modifier.setVectorType();
    vector_modifier.set_vector_size(4);
    SgModifierType *vector_type = SageBuilder::buildModifierType(
        lane_declaration->get_type(), vector_modifier);
    SgInitializedName *reference =
        SageBuilder::buildInitializedName(SgName("rex_vector"), vector_type);
    SgVariableDeclaration *use_site = SageBuilder::buildVariableDeclaration(
        "rex_vector_use_site", SageBuilder::buildIntType(), nullptr, global);
    SageInterface::appendStatement(use_site, global);

    vector_unparser.get_name_qualification_context().recordType(
        reference, use_site, {"rex_type_context::", 1, false, false});
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    info.set_language(SgFile::e_Cxx_language);
    info.set_template_argument_qualification_context(use_site);
    info.set_reference_node_for_qualification(reference);
    info.set_name_qualification_length(1);
    info.set_SkipClassDefinition();
    info.set_SkipEnumDefinition();
    info.set_SkipClassSpecifier();
    vector_unparser.u_type->unparseType(vector_type, info);

    const std::string vector_text = vector_output.str();
    if (countOccurrences(vector_text, "rex_type_context::Lane") != 2 ||
        vector_text.find("__vector_size__(sizeof(") == std::string::npos) {
      std::cerr << "unexpected qualified vector spelling: " << vector_text
                << std::endl;
      return 6;
    }
  }

  {
    std::ostringstream auto_output;
    Unparser auto_unparser(&auto_output, "constrained-auto.cpp", options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeSourceFile(cxx_file, "constrained-auto.cpp", false);
    auto_unparser.currentFile = &cxx_file;
    SgAutoType *auto_type = SageBuilder::buildAutoType();
    auto_type->set_is_constrained(true);
    auto_type->set_source_constraint_spelling("rex_concept<int>");
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    info.set_language(SgFile::e_Cxx_language);
    auto_unparser.u_type->unparseType(auto_type, info);
    auto_unparser.cur << std::string("rex_value");
    if (auto_output.str() != "rex_concept<int> auto rex_value") {
      std::cerr << "unexpected constrained auto spelling: " << auto_output.str()
                << std::endl;
      return 6;
    }
  }

  SgSourceFile &fortran_file = *new SgSourceFile();
  initializeSourceFile(fortran_file, "raw.f90", true);
  unparser.currentFile = &fortran_file;
  unparser.cur.disable_linewrap();
  unparser.emitFortranText("integer :: value");
  unparser.emitFortranCharacterLiteral("isn't", '\'');
  unparser.emitFortranCharacterLiteral("say \"yes\"", '"');
  if (output.str() != "integer :: value'isn''t'\"say \"\"yes\"\"\"") {
    return 5;
  }

  {
    std::ostringstream template_output;
    Unparser template_unparser(&template_output, "standalone.cpp", options);
    SgUnparse_Info info;
    unparseTemplateParameters(template_unparser, info);
    if (template_output.str() != "<typename T>") {
      std::cerr << "unexpected standalone template-parameter spelling: "
                << template_output.str() << std::endl;
      return 6;
    }
  }

  {
    std::ostringstream pragma_output;
    Unparser pragma_unparser(&pragma_output, "pragma.cpp", options);
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration("omp parallel", nullptr);
    SgUnparse_Info info;
    pragma_unparser.u_exprStmt->unparsePragmaDeclStmt(pragma, info);
    if (pragma_output.str() != "#pragma omp parallel\n") {
      return 7;
    }
  }

  {
    std::ostringstream pragma_output;
    Unparser pragma_unparser(&pragma_output, "source-pragma.cpp", options);
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration("ident payload", nullptr);
    pragma->set_cxx_pragma_payload_kind(
        SgPragmaDeclaration::e_cxx_pragma_source_spelled);
    pragma->set_cxx_source_text("ident \"payload\"");
    SgUnparse_Info info;
    pragma_unparser.u_exprStmt->unparsePragmaDeclStmt(pragma, info);
    if (pragma_output.str() != "#pragma ident \"payload\"\n") {
      return 22;
    }
  }

  {
    const std::string filename = "empty-enum-preprocessing.cpp";
    SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(filename);
    ROSE_ASSERT(sourceFile != nullptr);
    sourceFile->set_Cxx_only(true);
    sourceFile->set_outputLanguage(SgFile::e_Cxx_language);
    SgGlobal *scope = sourceFile->get_globalScope();
    ROSE_ASSERT(scope != nullptr);
    Sg_File_Info *sourceFileInfo = sourceFile->get_file_info();
    ROSE_ASSERT(sourceFileInfo != nullptr);
    const std::string sourceIdentity = sourceFileInfo->get_filenameString();
    ROSE_ASSERT(!sourceIdentity.empty());
    SgEnumDeclaration *declaration = SageBuilder::buildEnumDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexEmptyEnum"), false, scope);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration == declaration->get_definingDeclaration());
    declaration->set_file_info(new Sg_File_Info(sourceIdentity, 1, 1));
    setExactSourceRange(declaration, sourceIdentity, 1, 2);
    declaration->addToAttachedPreprocessingInfo(
        buildInsideComment(sourceIdentity, "/*rex_inside_empty_enum*/\n"),
        PreprocessingInfo::after);

    std::ostringstream enumOutput;
    Unparser enumUnparser(&enumOutput, filename, options);
    enumUnparser.currentFile = sourceFile;
    enumUnparser.get_name_qualification_context().recordName(
        declaration, declaration, {"", 0, false, false});
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_language(SgFile::e_Cxx_language);
    enumUnparser.u_exprStmt->unparseEnumDeclStmt(declaration, info);

    const std::string output = enumOutput.str();
    const size_t openingBrace = output.find('{');
    const size_t payload = output.find("/*rex_inside_empty_enum*/");
    const size_t closingBrace = output.find('}', openingBrace);
    if (openingBrace == std::string::npos || payload == std::string::npos ||
        closingBrace == std::string::npos || openingBrace >= payload ||
        payload >= closingBrace) {
      std::cerr << "unexpected empty-enum preprocessing placement: " << output
                << std::endl;
      return 25;
    }
  }

  {
    std::ostringstream literal_output;
    Unparser literal_unparser(&literal_output, "typed-literal.cpp", options);
    CxxFunctionCallFixture fixture(true, true, true, false,
                                   "typed-literal.cpp");
    fixture.call->set_uses_operator_syntax(true);
    fixture.call->set_source_operator_surface(
        SgFunctionCallExp::e_user_defined_literal_surface);
    fixture.call->set_source_operator_callee_form(
        SgFunctionCallExp::e_nonmember_operator_callee);
    fixture.call->set_source_operator_operand_roles(
        SgUnsignedCharList{SgFunctionCallExp::e_semantic_operator_operand});
    fixture.call->set_source_user_defined_literal_suffix(
        SgName("_typed_suffix"));
    fixture.unparse(literal_unparser);
    if (literal_output.str() != "0x2a_typed_suffix") {
      return 23;
    }
  }

  {
    std::ostringstream initializer_output;
    Unparser initializer_unparser(&initializer_output, "initializer.cpp",
                                  options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeSourceFile(cxx_file, "initializer.cpp", false);
    initializer_unparser.currentFile = &cxx_file;
    SgExprListExp *initializers = markSourceExpression(
        SageBuilder::buildExprListExp_nfi(), "initializer.cpp");
    initializers->append_expression(markSourceLiteral(
        SageBuilder::buildIntVal_nfi(7, "7"), "initializer.cpp"));
    SgAggregateInitializer *initializer = markSourceExpression(
        SageBuilder::buildAggregateInitializer_nfi(
            initializers, SageBuilder::buildIntType(),
            SgAggregateInitializer::e_aggregate_initializer_source_braced),
        "initializer.cpp");
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    initializer_unparser.u_exprStmt->unparseLanguageSpecificExpression(
        initializer, info);
    if (initializer_output.str() != "{7}") {
      return 8;
    }
  }

  {
    std::ostringstream launch_bounds_output;
    Unparser launch_bounds_unparser(&launch_bounds_output,
                                    "typed-launch-bounds.cpp", options);
    SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
        SgName("rex_kernel"), static_cast<SgFunctionType *>(nullptr), nullptr);
    SgIntVal *expression = markSourceLiteral(
        SageBuilder::buildIntVal_nfi(128, "128"), "typed-launch-bounds.cpp");
    declaration->set_cuda_launch_bounds_expression(expression);
    expression->set_parent(declaration);
    SgUnparse_Info info;
    launch_bounds_unparser.u_exprStmt->unparseCudaLaunchBounds(declaration,
                                                               info);
    if (launch_bounds_output.str() != "__launch_bounds__(128)") {
      return 9;
    }
  }

  {
    std::ostringstream map_output;
    Unparser map_unparser(&map_output, "ordered-map-items.cpp", options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeExactSourceFile(cxx_file, "ordered-map-items.cpp", false);
    SgGlobal *scope = attachExactGlobalScope(cxx_file, "ordered-map-items.cpp");
    map_unparser.currentFile = &cxx_file;

    SgVariableDeclaration *data_declaration =
        SageBuilder::buildVariableDeclaration(
            "rex_data",
            SageBuilder::buildPointerType(SageBuilder::buildIntType()), nullptr,
            scope);
    SageInterface::appendStatement(data_declaration, scope);
    SgVariableSymbol *symbol = scope->lookup_variable_symbol("rex_data");
    ROSE_ASSERT(symbol != nullptr);

    auto build_map_item = [&](int lower, int length,
                              SgOmpClause::omp_map_dist_data_enum policy_kind,
                              int policy_argument) {
      SgVarRefExp *reference = SageBuilder::buildVarRefExp(symbol);
      SgSubscriptExpression *subscript = markSourceExpression(
          SageBuilder::buildSubscriptExpression_nfi(
              markSourceLiteral(
                  SageBuilder::buildIntVal_nfi(lower, std::to_string(lower)),
                  "ordered-map-items.cpp"),
              markSourceLiteral(
                  SageBuilder::buildIntVal_nfi(length, std::to_string(length)),
                  "ordered-map-items.cpp"),
              nullptr),
          "ordered-map-items.cpp");
      SgPntrArrRefExp *section = SageBuilder::buildPntrArrRefExp(
          reference, subscript, SageBuilder::buildIntType());
      SgOmpMapItem *item = new SgOmpMapItem(section);
      section->set_parent(item);
      SgIntVal *argument = markSourceLiteral(
          SageBuilder::buildIntVal_nfi(policy_argument,
                                       std::to_string(policy_argument)),
          "ordered-map-items.cpp");
      SgOmpMapDistDataPolicy *policy =
          new SgOmpMapDistDataPolicy(policy_kind, argument);
      argument->set_parent(policy);
      item->get_policies().push_back(policy);
      policy->set_parent(item);
      return item;
    };

    SgExprListExp *variables = SageBuilder::buildExprListExp_nfi();
    SgOmpMapItem *first =
        build_map_item(0, 4, SgOmpClause::e_omp_map_dist_data_block, 2);
    SgOmpMapItem *second =
        build_map_item(4, 4, SgOmpClause::e_omp_map_dist_data_cyclic, 3);
    variables->append_expression(first);
    variables->append_expression(second);
    SgOmpMapClause *map_clause =
        new SgOmpMapClause(variables, SgOmpClause::e_omp_map_to);
    variables->set_parent(map_clause);

    SgNullStatement *target_body = SageBuilder::buildNullStatement();
    SgOmpTargetStatement *target = new SgOmpTargetStatement(target_body);
    ROSE_ASSERT(target->get_body() == target_body);
    target_body->set_parent(target);
    ROSE_ASSERT(target_body->get_parent() == target);
    OmpSupport::addOmpClause(target, map_clause);
    for (SgNode *node : NodeQuery::querySubTree(map_clause, V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(node);
      ROSE_ASSERT(reference != nullptr);
      map_unparser.get_name_qualification_context().recordName(
          reference, target, {"", 0, false, false});
    }

    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    info.set_language(SgFile::e_Cxx_language);
    map_unparser.u_exprStmt->unparseOmpVariablesClause(map_clause, info);
    const std::string expected =
        " map(to : rex_data[0:4] dist_data(BLOCK(2)),rex_data[4:4] "
        "dist_data(CYCLIC(3)))";
    if (map_output.str() != expected || first->get_parent() != variables ||
        second->get_parent() != variables ||
        first->get_expression()->get_parent() != first ||
        second->get_expression()->get_parent() != second ||
        first->get_policies().front()->get_parent() != first ||
        second->get_policies().front()->get_parent() != second) {
      std::cerr << "unexpected ordered map-item spelling: " << map_output.str()
                << std::endl;
      return 9;
    }
  }

  {
    std::ostringstream if_output;
    Unparser if_unparser(&if_output, "dangling-else.cpp", options);
    SgSourceFile &cxx_file = *new SgSourceFile();
    initializeExactSourceFile(cxx_file, "dangling-else.cpp", false);
    SgGlobal *if_scope = attachExactGlobalScope(cxx_file, "dangling-else.cpp");
    if_unparser.currentFile = &cxx_file;

    SgIfStmt *inner =
        SageBuilder::buildIfStmt(SageBuilder::buildIntVal(1),
                                 SageBuilder::buildNullStatement(), nullptr);
    SgIfStmt *outer = SageBuilder::buildIfStmt(
        SageBuilder::buildIntVal(1), inner, SageBuilder::buildNullStatement());
    SageInterface::publishGeneratedSubtreeOutputOwner(outer, if_scope);
    SgUnparse_Info info;
    info.set_current_source_file(&cxx_file);
    if_unparser.u_exprStmt->unparseIfStmt(outer, info);

    const std::string text = if_output.str();
    if (text.find("else {}") != std::string::npos ||
        text.find('{') == std::string::npos ||
        text.find('}') == std::string::npos ||
        text.find("else") == std::string::npos) {
      return 10;
    }
  }
  {
    std::ostringstream wrapped_output;
    Unparser wrapped_unparser(&wrapped_output, "wrapped.cpp", options);
    wrapped_unparser.cur.set_linewrap(8);
    wrapped_unparser.cur << std::string("left ");
    wrapped_unparser.cur << std::string("right");
    wrapped_unparser.cur.insert_newline();
    if (wrapped_output.str() != "left\n    right\n") {
      return 11;
    }
  }
  {
    std::ostringstream disabled_wrap_output;
    Unparser disabled_wrap_unparser(&disabled_wrap_output, "disabled-wrap.cpp",
                                    options);
    disabled_wrap_unparser.cur.disable_linewrap();
    disabled_wrap_unparser.cur << std::string("left ");
    disabled_wrap_unparser.cur << std::string("right");
    disabled_wrap_unparser.cur.insert_newline();
    if (disabled_wrap_output.str() != "left right\n") {
      return 12;
    }
  }
  {
    std::ostringstream indent_output;
    Unparser indent_unparser(&indent_output, "indent.cpp", options);
    indent_unparser.cur << std::string("zero");
    indent_unparser.cur.insert_newline(1, 0);
    indent_unparser.cur << std::string("three");
    indent_unparser.cur.insert_newline(1, 3);
    indent_unparser.cur << std::string("default");
    indent_unparser.cur.insert_newline();
    if (indent_output.str() != "zero\nthree\n   default\n") {
      return 13;
    }
  }
  {
    std::ostringstream raw_boundary_output;
    Unparser raw_boundary_unparser(&raw_boundary_output, "raw-boundary.cpp",
                                   options);
    raw_boundary_unparser.cur << std::string("left ");
    raw_boundary_unparser.cur.emit_raw_text("\nright");
    if (raw_boundary_output.str() != "left\nright") {
      return 14;
    }
  }
  {
    SgNonrealDecl *shared_reference =
        new SgNonrealDecl(SgName("rex_shared_qualification"));
    SgNullStatement *first_use = new SgNullStatement();
    SgNullStatement *second_use = new SgNullStatement();
    NameQualificationContext &context =
        unparser.get_name_qualification_context();
    context.recordType(shared_reference, first_use,
                       {"first::", 1, false, false});
    context.recordType(shared_reference, second_use,
                       {"second::", 1, false, false});
    context.recordName(shared_reference, first_use,
                       {"name_first::", 1, false, false});
    context.recordName(shared_reference, second_use,
                       {"name_second::", 1, false, false});
    if (!context.containsName(shared_reference, first_use) ||
        !context.containsName(shared_reference, second_use) ||
        !context.containsType(shared_reference, first_use) ||
        !context.containsType(shared_reference, second_use) ||
        context.lookupName(shared_reference, first_use).qualifier !=
            "name_first::" ||
        context.lookupName(shared_reference, second_use).qualifier !=
            "name_second::" ||
        context.lookupType(shared_reference, first_use).qualifier !=
            "first::" ||
        context.lookupType(shared_reference, second_use).qualifier !=
            "second::" ||
        unparser.u_name->lookup_type_qualification(shared_reference, first_use)
                .qualifier != "first::" ||
        unparser.u_name->lookup_type_qualification(shared_reference, second_use)
                .qualifier != "second::" ||
        unparser.u_name->lookup_name_qualification(shared_reference, first_use)
                .qualifier != "name_first::" ||
        unparser.u_name->lookup_name_qualification(shared_reference, second_use)
                .qualifier != "name_second::") {
      return 21;
    }
  }
  {
    SgBasicBlock *declaration_scope = SageBuilder::buildBasicBlock();
    SgBasicBlock *alias_scope = SageBuilder::buildBasicBlock();
    SgInitializedName *initialized_name = SageBuilder::buildInitializedName(
        SgName("rex_session_alias"), SageBuilder::buildIntType());
    SgVariableSymbol *target = new SgVariableSymbol(initialized_name);
    declaration_scope->get_symbol_table()->insert(SgName("rex_session_alias"),
                                                  target);

    SgNullStatement *causal_node = SageBuilder::buildNullStatement();
    SgAliasSymbol *alias = new SgAliasSymbol(target);
    alias->get_causal_nodes().push_back(causal_node);
    alias_scope->get_symbol_table()->insert(SgName("rex_session_alias"), alias);

    SgUnorderedNodeSet before_cause;
    SgUnorderedNodeSet after_cause;
    after_cause.insert(causal_node);
    if (alias_scope->lookup_symbol_for_name_qualification(
            SgName("rex_session_alias"), V_SgVariableSymbol, nullptr, nullptr,
            nullptr, before_cause) != nullptr ||
        alias_scope->lookup_symbol_for_name_qualification(
            SgName("rex_session_alias"), V_SgVariableSymbol, nullptr, nullptr,
            nullptr, after_cause) != target ||
        alias_scope->lookup_symbol_for_name_qualification(
            SgName("rex_session_alias"), V_SgVariableSymbol, nullptr, nullptr,
            nullptr, before_cause) != nullptr ||
        alias_scope->lookup_variable_symbol(SgName("rex_session_alias")) !=
            target) {
      return 14;
    }
  }
  {
    SgSourceFile *copy_source =
        SageBuilder::buildGeneratedSourceFile("rex-copy-lookup.cpp");
    ROSE_ASSERT(copy_source != nullptr);
    SgGlobal *owner = copy_source->get_globalScope();
    ROSE_ASSERT(owner != nullptr);
    SgClassDeclaration *base = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexCopyBase"), owner);
    SgClassDeclaration *derived = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        SgName("RexCopyDerived"), owner);
    SgClassDefinition *base_definition = base->get_definition();
    SgClassDefinition *derived_definition = derived->get_definition();
    if (base_definition == nullptr || derived_definition == nullptr) {
      return 15;
    }

    SgInitializedName *member = SageBuilder::buildInitializedName(
        SgName("rex_base_member"), SageBuilder::buildIntType());
    member->set_scope(base_definition);
    SgVariableSymbol *member_symbol = new SgVariableSymbol(member);
    base_definition->insert_symbol(member->get_name(), member_symbol);
    SageBuilder::buildBaseClass(
        isSgClassDeclaration(base->get_firstNondefiningDeclaration()),
        base->get_type(), derived_definition, false, true);

    if (derived_definition->lookup_variable_symbol(member->get_name()) !=
            nullptr ||
        derived_definition->lookup_symbol_for_ast_copy_fixup(
            member->get_name(), V_SgVariableSymbol, nullptr, nullptr,
            nullptr) != member_symbol ||
        derived_definition->lookup_variable_symbol(member->get_name()) !=
            nullptr) {
      return 15;
    }
  }
  {
    std::ostringstream declaration_output;
    Unparser declaration_unparser(&declaration_output, "dimension-source.f90",
                                  options);
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "dimension-source.f90", true);
    SgGlobal *global =
        attachExactGlobalScope(source_file, "dimension-source.f90");
    declaration_unparser.currentFile = &source_file;
    FortranDimensionFixture fixture =
        buildFortranDimensionFixture(global, true);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Fortran_language);
    FortranCodeGeneration_locatedNode declaration_fortran(
        &declaration_unparser, "dimension-source.f90");
    declaration_fortran.unparseVarDecl(fixture.declaration,
                                       fixture.initialized_name, info);
    const std::string declaration_text = declaration_output.str();
    if (declaration_text != "INTEGER :: rex_dimension_value") {
      return 16;
    }

    std::ostringstream dimension_output;
    Unparser dimension_unparser(&dimension_output, "dimension-source.f90",
                                options);
    dimension_unparser.currentFile = &source_file;
    FortranCodeGeneration_locatedNode dimension_fortran(&dimension_unparser,
                                                        "dimension-source.f90");
    dimension_fortran.unparseAttributeSpecificationStatement(fixture.dimension,
                                                             info);
    if (dimension_output.str() != "dimension :: rex_dimension_value(10)\n") {
      return 17;
    }
  }
  {
    std::ostringstream clause_output;
    Unparser clause_unparser(&clause_output, "openmp.cpp", options);
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "openmp.cpp", false);
    clause_unparser.currentFile = &source_file;
    SgExprListExp *variables = SageBuilder::buildExprListExp_nfi();
    SgOmpPrivateClause *clause = new SgOmpPrivateClause(variables);
    variables->set_parent(clause);
    SgOmpNameExpression *item = new SgOmpNameExpression("rex_omp_item");
    markSourceLiteral(item, "openmp.cpp");
    variables->append_expression(item);
    item->set_parent(variables);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);
    clause_unparser.u_exprStmt->unparseOmpVariablesClause(clause, info);
    if (clause_output.str() != " private(rex_omp_item)") {
      return 18;
    }
  }
  {
    std::ostringstream coroutine_output;
    Unparser coroutine_unparser(&coroutine_output, "coroutine.cpp", options);
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "coroutine.cpp", false);
    coroutine_unparser.currentFile = &source_file;
    SgIntVal *operand = SageBuilder::buildIntVal_nfi(7, "7");
    markSourceLiteral(operand, "coroutine.cpp");
    SgAwaitExpression *await_expression = SageBuilder::buildAwaitExpression_nfi(
        operand, SageBuilder::buildIntType());
    markSourceLiteral(await_expression, "coroutine.cpp");
    await_expression->set_coroutine_keyword_kind(
        SgAwaitExpression::e_coroutine_keyword_co_yield);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_language(SgFile::e_Cxx_language);
    coroutine_unparser.u_exprStmt->unparseLanguageSpecificExpression(
        await_expression, info);
    if (coroutine_output.str() != "co_yield 7") {
      return 19;
    }

    SgReturnStmt *return_statement =
        SageBuilder::buildReturnStmt(SageBuilder::buildIntVal(9));
    return_statement->set_return_keyword_kind(
        SgReturnStmt::e_return_keyword_co_return);
    coroutine_unparser.u_exprStmt->unparseReturnStmt(return_statement, info);
    if (coroutine_output.str() != "co_yield 7co_return 9;") {
      return 19;
    }
  }
  {
    std::ostringstream restrict_output;
    Unparser restrict_unparser(&restrict_output, "restrict.cpp", options);
    CxxMemberFunctionFixture fixture("restrict.cpp");
    SgMemberFunctionType *type =
        isSgMemberFunctionType(fixture.declaration->get_type());
    ROSE_ASSERT(type != nullptr);
    type->setRestrictFunc();
    fixture.declaration->get_declarationModifier()
        .get_typeModifier()
        .setRestrict();
    SgUnparse_Info info;
    fixture.initialize(restrict_unparser, info);
    restrict_unparser.u_exprStmt->unparseMemberFunctionParametersAndQualifiers(
        fixture.declaration, info);
    if (countOccurrences(restrict_output.str(), "__restrict__") != 1) {
      return 26;
    }
  }
  {
    std::ostringstream alias_output;
    Unparser alias_unparser(&alias_output, "alias.cpp", options);
    SgSourceFile &source_file = *new SgSourceFile();
    initializeExactSourceFile(source_file, "alias.cpp", false);
    SgGlobal *global = attachExactGlobalScope(source_file, "alias.cpp");
    alias_unparser.currentFile = &source_file;
    SgName template_name("rex_alias");
    SgTemplateTypedefDeclaration *template_declaration =
        SageBuilder::buildTemplateTypedefDeclaration_nfi(
            SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
            SgTypedefDeclaration::e_using, template_name,
            SageBuilder::buildIntType(), global);
    SgTemplateParameter *template_parameter = buildTemplateParameters().front();
    template_declaration->get_templateParameters().push_back(
        template_parameter);
    template_parameter->set_parent(template_declaration);

    std::ostringstream declaration_output;
    Unparser declaration_unparser(&declaration_output, "alias.cpp", options);
    declaration_unparser.currentFile = &source_file;
    SgUnparse_Info declaration_info;
    declaration_info.set_current_source_file(&source_file);
    declaration_info.set_current_scope(global);
    declaration_info.set_language(SgFile::e_Cxx_language);
    declaration_info.set_template_argument_qualification_context(
        template_declaration);
    declaration_unparser.u_exprStmt->unparseTemplateTypedefDeclaration(
        template_declaration, declaration_info);
    const std::string declaration_text = declaration_output.str();
    if (declaration_text != "template <typename T>\nusing rex_alias = int ;" ||
        declaration_text.find("unparse template") != std::string::npos) {
      std::cerr << "unexpected alias-template spelling: " << declaration_text
                << std::endl;
      return 27;
    }
    SgNonrealType *argument_type = SageBuilder::buildSemanticNonrealType(
        SgName("rex_value"), global, nullptr, nullptr);
    SgNonrealDecl *argument_declaration =
        isSgNonrealDecl(argument_type->get_declaration());
    ROSE_ASSERT(argument_declaration != nullptr);
    SgNonrealSymbol *argument_symbol =
        isSgNonrealSymbol(argument_declaration->get_symbol_from_symbol_table());
    ROSE_ASSERT(argument_symbol != nullptr);
    SgNonrealRefExp *argument_expression = markSourceExpression(
        SageBuilder::buildNonrealRefExp_nfi(argument_symbol), "alias.cpp");
    SgTemplateArgument *argument =
        SageBuilder::buildTemplateArgument(argument_expression);
    argument->set_explicitlySpecified(true);
    SgTemplateArgumentPtrList arguments{argument};
    SgName semantic_name("rex_stale_alias<wrong>");
    SgTemplateInstantiationTypedefDeclaration *instantiation =
        SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
            SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
            SgTypedefDeclaration::e_using, template_name,
            SageBuilder::buildIntType(), global, template_declaration,
            arguments, semantic_name);

    SgVariableDeclaration *use_site = SageBuilder::buildVariableDeclaration(
        "rex_alias_use_site", SageBuilder::buildIntType(), nullptr, global);
    SageInterface::appendStatement(use_site, global);
    SgUnparse_Info info;
    info.set_current_source_file(&source_file);
    info.set_template_argument_qualification_context(use_site);
    NameQualificationContext &qualifications =
        alias_unparser.get_name_qualification_context();
    qualifications.record(argument, use_site, {"", 0, false, false});
    qualifications.recordName(argument_expression, use_site,
                              {"", 0, false, false});
    alias_unparser.u_type->unparseTemplateTypedefName(instantiation, info);
    if (alias_output.str() != "rex_alias<rex_value>") {
      return 20;
    }
  }
  {
    SgSourceFile &argument_owner_file = *new SgSourceFile();
    initializeExactSourceFile(argument_owner_file,
                              "template-argument-owner.cpp", false);
    argument_owner_file.set_Cxx03_only();
    SgGlobal *argument_owner_scope = attachExactGlobalScope(
        argument_owner_file, "template-argument-owner.cpp");

    SgTemplateArgument *inner_argument =
        SageBuilder::buildTemplateArgument(SageBuilder::buildIntType());
    inner_argument->set_explicitlySpecified(true);
    SgTemplateInstantiationDecl *inner =
        buildTemplateInstantiationTypeDeclaration(
            SgName("rex_inner"), SgName("rex_inner<int>"),
            SgTemplateArgumentPtrList{inner_argument}, argument_owner_scope);
    SgClassType *inner_type = SgClassType::createType(inner);
    ROSE_ASSERT(inner_type != nullptr);
    SgTemplateArgument *outer_argument =
        SageBuilder::buildTemplateArgument(inner_type);
    outer_argument->set_explicitlySpecified(true);
    SgTemplateInstantiationDecl *outer =
        buildTemplateInstantiationTypeDeclaration(
            SgName("rex_outer"), SgName("rex_outer<rex_inner<int>>"),
            SgTemplateArgumentPtrList{outer_argument}, argument_owner_scope);

    std::ostringstream compact_output;
    Unparser compact_unparser(&compact_output, "template-output-cxx20.cpp",
                              options);
    SgSourceFile &compact_file = *new SgSourceFile();
    initializeExactSourceFile(compact_file, "template-output-cxx20.cpp", false);
    compact_unparser.currentFile = &compact_file;
    SgUnparse_Info compact_info;
    compact_info.set_current_source_file(&compact_file);
    compact_info.set_language(SgFile::e_Cxx_language);
    compact_info.set_SkipQualifiedNames();
    compact_unparser.u_exprStmt->unparseTemplateArgumentList(
        outer->get_templateArguments(), compact_info,
        TemplateArgumentEmission::explicit_source_prefix);
    if (compact_output.str() != "<rex_inner<int>>") {
      std::cerr << "unexpected C++20 template closer spelling: "
                << compact_output.str() << std::endl;
      return 28;
    }

    std::ostringstream separated_output;
    Unparser separated_unparser(&separated_output, "template-output-cxx03.cpp",
                                options);
    SgSourceFile &separated_file = *new SgSourceFile();
    initializeExactSourceFile(separated_file, "template-output-cxx03.cpp",
                              false);
    separated_file.set_Cxx03_only();
    separated_unparser.currentFile = &separated_file;
    SgUnparse_Info separated_info;
    separated_info.set_current_source_file(&separated_file);
    separated_info.set_language(SgFile::e_Cxx_language);
    separated_info.set_SkipQualifiedNames();
    separated_unparser.u_exprStmt->unparseTemplateArgumentList(
        outer->get_templateArguments(), separated_info,
        TemplateArgumentEmission::explicit_source_prefix);
    if (separated_output.str() != "<rex_inner<int> >") {
      std::cerr << "unexpected C++03 template closer spelling: "
                << separated_output.str() << std::endl;
      return 29;
    }
  }
  return 0;
}
