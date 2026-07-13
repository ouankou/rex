#include "rose.h"

#include "nameQualificationSupport.h"
#include "unparseCxx.h"
#include "unparser.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>

namespace {

constexpr const char *kGeneratedFilename =
    "rex_unparser_template_and_tag_ownership_contract.cpp";

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

void initializeInfo(SgUnparse_Info &info, SgSourceFile *source_file,
                    SgStatement *emission_statement, SgScopeStatement *scope) {
  ROSE_ASSERT(source_file != nullptr);
  info.set_current_source_file(source_file);
  info.set_current_scope(scope);
  info.set_language(SgFile::e_Cxx_language);
  info.set_template_argument_qualification_context(emission_statement);
}

std::string renderTemplateInstantiation(
    SgDeclarationStatement *declaration, SgStatement *emission_statement,
    NameQualificationContext &qualifications, bool class_name_only = false) {
  ROSE_ASSERT(declaration != nullptr);
  SgSourceFile *source_file =
      SageInterface::getEnclosingSourceFile(declaration);
  ROSE_ASSERT(source_file != nullptr);

  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, source_file->getFileName(), options, nullptr,
                    nullptr, nullptr, &qualifications);
  unparser.currentFile = source_file;

  SgUnparse_Info info;
  initializeInfo(info, source_file, emission_statement,
                 declaration->get_scope());
  if (class_name_only) {
    info.set_outputClassTemplateName();
  }

  if (SgTemplateInstantiationDecl *class_declaration =
          isSgTemplateInstantiationDecl(declaration)) {
    unparser.u_exprStmt->unparseTemplateInstantiationDeclStmt(class_declaration,
                                                              info);
  } else if (SgTemplateInstantiationFunctionDecl *function_declaration =
                 isSgTemplateInstantiationFunctionDecl(declaration)) {
    unparser.u_exprStmt->unparseTemplateInstantiationFunctionDeclStmt(
        function_declaration, info);
  } else if (SgTemplateInstantiationMemberFunctionDecl *member_declaration =
                 isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    unparser.u_exprStmt->unparseTemplateInstantiationMemberFunctionDeclStmt(
        member_declaration, info);
  } else {
    ROSE_ABORT();
  }
  return output.str();
}

bool templateInstantiationHasTemplateDeclaration(
    SgDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  if (SgTemplateInstantiationDecl *class_declaration =
          isSgTemplateInstantiationDecl(declaration)) {
    return class_declaration->get_templateDeclaration() != nullptr;
  }
  if (SgTemplateInstantiationFunctionDecl *function_declaration =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    return function_declaration->get_templateDeclaration() != nullptr;
  }
  if (SgTemplateInstantiationMemberFunctionDecl *member_declaration =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    return member_declaration->get_templateDeclaration() != nullptr;
  }
  ROSE_ABORT();
}

std::string templateInstantiationName(SgDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  if (SgTemplateInstantiationDecl *class_declaration =
          isSgTemplateInstantiationDecl(declaration)) {
    return class_declaration->get_name().str();
  }
  if (SgTemplateInstantiationFunctionDecl *function_declaration =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    return function_declaration->get_name().str();
  }
  if (SgTemplateInstantiationMemberFunctionDecl *member_declaration =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    return member_declaration->get_name().str();
  }
  ROSE_ABORT();
}

bool templateInstantiationHasDefinition(SgDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  if (SgTemplateInstantiationDecl *class_declaration =
          isSgTemplateInstantiationDecl(declaration)) {
    return class_declaration->get_definition() != nullptr;
  }
  if (SgTemplateInstantiationFunctionDecl *function_declaration =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    return function_declaration->get_definition() != nullptr;
  }
  if (SgTemplateInstantiationMemberFunctionDecl *member_declaration =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    return member_declaration->get_definition() != nullptr;
  }
  ROSE_ABORT();
}

bool checkSelectedTemplateInstantiation(
    SgDeclarationStatement *declaration, SgStatement *emission_statement,
    NameQualificationContext &qualifications) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(emission_statement != nullptr);
  if (declaration->get_parent() != emission_statement ||
      templateInstantiationHasDefinition(declaration) ||
      !templateInstantiationHasTemplateDeclaration(declaration)) {
    return false;
  }

  const std::string name = templateInstantiationName(declaration);
  const std::string spelling = renderTemplateInstantiation(
      declaration, emission_statement, qualifications);
  return !name.empty() && !spelling.empty() && contains(spelling, name);
}

int checkTemplateInstantiationContracts(
    SgProject *project, NameQualificationContext &qualifications) {
  bool found_class = false;
  bool found_function = false;
  bool found_member = false;
  Rose_STL_Container<SgNode *> directives = NodeQuery::querySubTree(
      project, V_SgTemplateInstantiationDirectiveStatement);
  for (SgNode *node : directives) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    ROSE_ASSERT(directive != nullptr);
    SgDeclarationStatement *declaration = directive->get_declaration();
    const bool supported_instantiation =
        isSgTemplateInstantiationDecl(declaration) != nullptr ||
        isSgTemplateInstantiationFunctionDecl(declaration) != nullptr ||
        isSgTemplateInstantiationMemberFunctionDecl(declaration) != nullptr;
    if (declaration == nullptr || declaration->get_parent() != directive ||
        !supported_instantiation ||
        templateInstantiationHasDefinition(declaration)) {
      continue;
    }

    if (!found_class) {
      if (SgTemplateInstantiationDecl *class_declaration =
              isSgTemplateInstantiationDecl(declaration)) {
        if (contains(class_declaration->get_templateName().str(), "Holder")) {
          if (!checkSelectedTemplateInstantiation(class_declaration, directive,
                                                  qualifications)) {
            return 10;
          }
          const std::string expected_name =
              class_declaration->get_qualified_name().str();
          const std::string name_only = renderTemplateInstantiation(
              class_declaration, directive, qualifications, true);
          if (name_only != expected_name || contains(name_only, "class ") ||
              contains(name_only, "struct ") || contains(name_only, "{") ||
              contains(name_only, ";")) {
            return 11;
          }
          found_class = true;
          continue;
        }
      }
    }

    if (!found_function) {
      if (SgTemplateInstantiationFunctionDecl *function_declaration =
              isSgTemplateInstantiationFunctionDecl(declaration)) {
        if (contains(function_declaration->get_name().str(), "twice")) {
          if (!checkSelectedTemplateInstantiation(function_declaration,
                                                  directive, qualifications)) {
            return 12;
          }
          found_function = true;
          continue;
        }
      }
    }

    if (!found_member) {
      if (SgTemplateInstantiationMemberFunctionDecl *member_declaration =
              isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
        if (contains(member_declaration->get_name().str(),
                     "approximate_gradient")) {
          if (!checkSelectedTemplateInstantiation(member_declaration, directive,
                                                  qualifications)) {
            return 13;
          }
          found_member = true;
        }
      }
    }
  }

  return found_class && found_function && found_member ? 0 : 14;
}

void recordDeclarationRole(SgClassDeclaration *declaration,
                           bool &saw_nonautonomous_declaration) {
  if (declaration == nullptr) {
    return;
  }
  saw_nonautonomous_declaration = saw_nonautonomous_declaration ||
                                  !declaration->get_isAutonomousDeclaration();
}

int checkElaboratedTagContract(SgProject *project,
                               NameQualificationContext &qualifications) {
  Rose_STL_Container<SgNode *> initialized_names =
      NodeQuery::querySubTree(project, V_SgInitializedName);
  for (SgNode *node : initialized_names) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr || initialized_name->get_name() != "bp") {
      continue;
    }
    if (!initialized_name->get_source_type_qualification_present() ||
        initialized_name->get_source_type_global_qualification() ||
        !initialized_name->get_source_type_qualification_tokens().empty()) {
      return 20;
    }

    SgVariableDeclaration *variable_declaration =
        isSgVariableDeclaration(initialized_name->get_parent());
    SgClassType *class_type =
        isSgClassType(initialized_name->get_type()->findBaseType());
    if (variable_declaration == nullptr || class_type == nullptr) {
      return 21;
    }
    SgClassDeclaration *type_declaration =
        isSgClassDeclaration(class_type->get_declaration());
    if (type_declaration == nullptr) {
      return 22;
    }

    bool saw_nonautonomous_declaration = false;
    recordDeclarationRole(type_declaration, saw_nonautonomous_declaration);
    recordDeclarationRole(
        isSgClassDeclaration(
            type_declaration->get_firstNondefiningDeclaration()),
        saw_nonautonomous_declaration);
    recordDeclarationRole(
        isSgClassDeclaration(type_declaration->get_definingDeclaration()),
        saw_nonautonomous_declaration);
    if (!saw_nonautonomous_declaration) {
      return 23;
    }

    SgSourceFile *source_file =
        SageInterface::getEnclosingSourceFile(variable_declaration);
    ROSE_ASSERT(source_file != nullptr);
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, source_file->getFileName(), options, nullptr,
                      nullptr, nullptr, &qualifications);
    unparser.currentFile = source_file;
    SgUnparse_Info info;
    initializeInfo(info, source_file, variable_declaration,
                   variable_declaration->get_scope());
    unparser.u_exprStmt->unparseVarDeclStmt(variable_declaration, info);
    const std::string spelling = output.str();
    if (!contains(spelling, "struct B") || contains(spelling, "::B") ||
        contains(spelling, "N::B")) {
      return 24;
    }
    return 0;
  }
  return 25;
}

void setPhysicalPosition(SgLocatedNode *node, const std::string &filename,
                         bool output) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *file_info = new Sg_File_Info(filename, 1, 1);
  Sg_File_Info *start = new Sg_File_Info(filename, 1, 1);
  Sg_File_Info *end = new Sg_File_Info(filename, 1, 1);
  if (output) {
    file_info->setOutputInCodeGeneration();
    start->setOutputInCodeGeneration();
    end->setOutputInCodeGeneration();
  } else {
    file_info->unsetOutputInCodeGeneration();
    start->unsetOutputInCodeGeneration();
    end->unsetOutputInCodeGeneration();
  }
  file_info->set_parent(node);
  start->set_parent(node);
  end->set_parent(node);
  node->set_file_info(file_info);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
}

void setSemanticFrontendPosition(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *previous_start = node->get_startOfConstruct();
  Sg_File_Info *previous_end = node->get_endOfConstruct();
  auto make_position = []() {
    Sg_File_Info *position =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    ROSE_ASSERT(position != nullptr);
    position->setCompilerGenerated();
    position->setFrontendSpecific();
    position->unsetTransformation();
    position->unsetSourcePositionUnavailableInFrontend();
    position->setOutputInCodeGeneration();
    position->set_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    position->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    return position;
  };
  Sg_File_Info *start = make_position();
  Sg_File_Info *end = make_position();
  start->set_parent(node);
  end->set_parent(node);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  if (previous_start != nullptr) {
    delete previous_start;
  }
  if (previous_end != nullptr && previous_end != previous_start) {
    delete previous_end;
  }
}

struct ForFixture {
  SgSourceFile *source_file = nullptr;
  SgForStatement *statement = nullptr;
  SgForInitStatement *initializer = nullptr;
};

ForFixture buildForFixture(bool expression_initializer) {
  ForFixture fixture;
  fixture.source_file =
      SageBuilder::buildGeneratedSourceFile(kGeneratedFilename);
  ROSE_ASSERT(fixture.source_file != nullptr);
  fixture.source_file->set_Cxx_only(true);
  fixture.source_file->set_outputLanguage(SgFile::e_Cxx_language);
  SgGlobal *global = fixture.source_file->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgFunctionDeclaration *function =
      SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          SgName("rex_for_initializer_contract"), SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global);
  ROSE_ASSERT(function != nullptr);
  SgFunctionDeclaration *prototype =
      isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
  ROSE_ASSERT(prototype != nullptr);
  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);
  const std::string physical_filename =
      fixture.source_file->get_sourceFileNameWithPath();

  SgStatement *initializer_statement = nullptr;
  if (expression_initializer) {
    SgIntVal *literal = SageBuilder::buildIntVal_nfi(7, "7");
    setPhysicalPosition(literal, physical_filename, true);
    initializer_statement = SageBuilder::buildExprStatement_nfi(literal);
    literal->set_parent(initializer_statement);
    setPhysicalPosition(isSgLocatedNode(initializer_statement),
                        physical_filename, true);
  } else {
    initializer_statement = SageBuilder::buildNullStatement_nfi();
    setSemanticFrontendPosition(isSgLocatedNode(initializer_statement));
  }
  fixture.initializer =
      SageBuilder::buildForInitStatement(initializer_statement);
  if (expression_initializer) {
    setPhysicalPosition(fixture.initializer, physical_filename, true);
  } else {
    setSemanticFrontendPosition(fixture.initializer);
  }

  SgNullStatement *test = SageBuilder::buildNullStatement();
  SgNullExpression *increment = SageBuilder::buildNullExpression(
      SgNullExpression::e_null_expression_syntactic_absence);
  SgNullStatement *loop_body = SageBuilder::buildNullStatement();
  fixture.statement = SageBuilder::buildForStatement(fixture.initializer, test,
                                                     increment, loop_body);
  SageInterface::appendStatement(fixture.statement, body);
  return fixture;
}

std::string renderForStatement(ForFixture &fixture) {
  ROSE_ASSERT(fixture.source_file != nullptr);
  ROSE_ASSERT(fixture.statement != nullptr);
  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, kGeneratedFilename, options);
  unparser.currentFile = fixture.source_file;
  SgUnparse_Info info;
  initializeInfo(info, fixture.source_file, fixture.statement,
                 fixture.statement->get_scope());
  unparser.u_exprStmt->unparseForStmt(fixture.statement, info);
  return output.str();
}

bool hasExactForHeaderSeparators(const std::string &output) {
  const size_t begin = output.find('(');
  const size_t end = output.find(')', begin);
  if (begin == std::string::npos || end == std::string::npos) {
    return false;
  }
  return std::count(output.begin() + static_cast<std::ptrdiff_t>(begin),
                    output.begin() + static_cast<std::ptrdiff_t>(end),
                    ';') == 2;
}

int checkForInitializerContracts() {
  ForFixture expression_fixture = buildForFixture(true);
  const std::string expression_output = renderForStatement(expression_fixture);
  if (!contains(expression_output, "7;") ||
      !hasExactForHeaderSeparators(expression_output)) {
    std::cerr << "REX_TEST_FAILURE[for-expression-initializer]: output='"
              << expression_output << "'\n";
    return 30;
  }

  ForFixture absent_fixture = buildForFixture(false);
  const std::string absent_output = renderForStatement(absent_fixture);
  if (contains(absent_output, "7") ||
      !hasExactForHeaderSeparators(absent_output)) {
    std::cerr << "REX_TEST_FAILURE[for-absent-initializer]: output='"
              << absent_output << "'\n";
    return 31;
  }
  return 0;
}

SgTemplateInstantiationDecl *
buildTemplateInstantiation(const std::string &template_name,
                           const std::string &template_id, bool with_argument) {
  SgTemplateArgumentPtrList arguments;
  SgTemplateArgumentPtrList semantic_arguments;
  if (with_argument) {
    SgTemplateArgument *argument =
        SageBuilder::buildTemplateArgument(SageBuilder::buildIntType());
    argument->set_explicitlySpecified(true);
    arguments.push_back(argument);
    SgTemplateArgument *semantic_argument =
        SageBuilder::buildTemplateArgument(SageBuilder::buildIntType());
    semantic_argument->set_explicitlySpecified(false);
    semantic_arguments.push_back(semantic_argument);
  }
  SgTemplateInstantiationDecl *declaration = new SgTemplateInstantiationDecl(
      SgName(template_id), SgClassDeclaration::e_class, nullptr, nullptr,
      nullptr, arguments, semantic_arguments);
  SageInterface::setSourcePositionForTransformation(declaration);
  declaration->set_templateName(SgName(template_name));
  declaration->set_firstNondefiningDeclaration(declaration);
  declaration->set_definingDeclaration(nullptr);
  return declaration;
}

int runDeathMode(const std::string &mode) {
  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, kGeneratedFilename, options);
  SgUnparse_Info info;
  info.set_language(SgFile::e_Cxx_language);

  if (mode == "empty-template-name") {
    unparser.u_exprStmt->unparseTemplateName(
        buildTemplateInstantiation("", "rex_bad<int>", true), info);
    return 0;
  }
  if (mode == "empty-template-id") {
    unparser.u_exprStmt->unparseTemplateName(
        buildTemplateInstantiation("rex_bad", "", true), info);
    return 0;
  }
  if (mode == "malformed-template-id") {
    unparser.u_exprStmt->unparseTemplateName(
        buildTemplateInstantiation("rex_bad", "rex_bad", true), info);
    return 0;
  }
  if (mode == "mismatched-template-payload") {
    unparser.u_exprStmt->unparseTemplateName(
        buildTemplateInstantiation("rex_bad", "rex_bad<>", true), info);
    return 0;
  }
  if (mode == "for-init-foreign-owner") {
    ForFixture fixture = buildForFixture(true);
    fixture.initializer->set_parent(nullptr);
    (void)renderForStatement(fixture);
    return 0;
  }
  if (mode == "for-init-empty-list") {
    ForFixture fixture = buildForFixture(true);
    fixture.initializer->get_init_stmt().clear();
    (void)renderForStatement(fixture);
    return 0;
  }
  return 3;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string mode = argv[1];
    if (mode == "empty-template-name" || mode == "empty-template-id" ||
        mode == "malformed-template-id" ||
        mode == "mismatched-template-payload" ||
        mode == "for-init-foreign-owner" || mode == "for-init-empty-list") {
      return runDeathMode(mode);
    }
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  NameQualificationContext qualifications;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    if (source_file == nullptr) {
      continue;
    }
    SgUnorderedNodeSet referenced_names;
    generateNameQualificationSupport(source_file, referenced_names,
                                     qualifications);
  }

  int status = checkTemplateInstantiationContracts(project, qualifications);
  if (status != 0) {
    return status;
  }
  status = checkElaboratedTagContract(project, qualifications);
  if (status != 0) {
    return status;
  }
  return checkForInitializerContracts();
}
