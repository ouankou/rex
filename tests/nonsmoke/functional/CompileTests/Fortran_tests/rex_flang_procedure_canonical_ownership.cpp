#include "rose.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace {

void verifyHeaderComments(SgLocatedNode *node, const char *prefix,
                          const char *header) {
  ROSE_ASSERT(node != nullptr);
  const auto *comments = node->getAttachedPreprocessingInfo();
  ROSE_ASSERT(comments != nullptr && comments->size() == 2);
  ROSE_ASSERT(comments->front()->getString() == prefix);
  ROSE_ASSERT(comments->front()->getRelativePosition() ==
              PreprocessingInfo::before);
  ROSE_ASSERT(comments->back()->getString() == header);
  ROSE_ASSERT(comments->back()->getRelativePosition() ==
              PreprocessingInfo::after_syntax);
  ROSE_ASSERT(comments->back()->getOutputPlacement() ==
              PreprocessingInfo::attached_output_trailing_line);
}

SgProgramHeaderStatement *findProgramDefinition(SgProject *project) {
  SgProgramHeaderStatement *program = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProgramHeaderStatement)) {
    SgProgramHeaderStatement *candidate = isSgProgramHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_definition() != nullptr) {
      ROSE_ASSERT(program == nullptr);
      program = candidate;
    }
  }
  ROSE_ASSERT(program != nullptr);
  return program;
}

SgProcedureHeaderStatement *findProcedure(SgNode *root, const std::string &name,
                                          bool defining) {
  SgProcedureHeaderStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_name().getString() == name &&
        (candidate->get_definition() != nullptr) == defining) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgFunctionSymbol *verifyProcedureSymbol(SgProcedureHeaderStatement *canonical,
                                        SgScopeStatement *scope) {
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(canonical->get_scope() == scope);
  SgFunctionSymbol *symbol =
      scope->lookup_function_symbol(canonical->get_name());
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == canonical);
  ROSE_ASSERT(symbol->get_symbol_basis() == canonical);
  ROSE_ASSERT(canonical->get_symbol_from_symbol_table() == symbol);
  ROSE_ASSERT(scope->find_symbol_from_declaration(canonical) == symbol);
  return symbol;
}

void verifyOwnedSourceAnchor(SgLocatedNode *node, bool compilerGenerated) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(node->get_file_info() == start);
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(start != end);
  ROSE_ASSERT(start->get_parent() == node);
  ROSE_ASSERT(end->get_parent() == node);
  ROSE_ASSERT(start->get_physical_file_id() >= 0);
  ROSE_ASSERT(end->get_physical_file_id() == start->get_physical_file_id());
  ROSE_ASSERT(!start->get_raw_filename().empty());
  ROSE_ASSERT(end->get_raw_filename() == start->get_raw_filename());
  ROSE_ASSERT(start->get_physical_filename() == start->get_raw_filename());
  ROSE_ASSERT(end->get_physical_filename() == start->get_raw_filename());
  ROSE_ASSERT(!start->isSourcePositionUnavailableInFrontend());
  ROSE_ASSERT(!end->isSourcePositionUnavailableInFrontend());
  ROSE_ASSERT(start->isCompilerGenerated() == compilerGenerated);
  ROSE_ASSERT(end->isCompilerGenerated() == compilerGenerated);
  if (compilerGenerated) {
    ROSE_ASSERT(start->get_filenameString() == "compilerGenerated");
    ROSE_ASSERT(end->get_filenameString() == "compilerGenerated");
  }
}

void verifyProcedureParameterSourceClassification(
    SgProcedureHeaderStatement *procedure, bool compilerGenerated) {
  ROSE_ASSERT(procedure != nullptr);
  SgFunctionParameterList *parameters = procedure->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_parent() == procedure);
  verifyOwnedSourceAnchor(parameters, compilerGenerated);
  for (SgInitializedName *argument : parameters->get_args()) {
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == parameters);
    ROSE_ASSERT(argument->get_declptr() == procedure);
    verifyOwnedSourceAnchor(argument, compilerGenerated);
  }
}

void verifyCompletedParameterScope(SgProcedureHeaderStatement *procedure,
                                   SgScopeStatement *semanticScope) {
  ROSE_ASSERT(procedure != nullptr);
  ROSE_ASSERT(semanticScope != nullptr);
  SgFunctionParameterScope *parameterScope =
      procedure->get_functionParameterScope();
  ROSE_ASSERT(parameterScope != nullptr);
  ROSE_ASSERT(parameterScope->get_parent() == procedure);
  ROSE_ASSERT(parameterScope->get_scope() == semanticScope);
  ROSE_ASSERT(parameterScope->get_construction_physical_output_owner() ==
              nullptr);
  ROSE_ASSERT(parameterScope->get_construction_semantic_scope() == nullptr);

  Sg_File_Info *ownerInfo = semanticScope->get_file_info();
  ROSE_ASSERT(ownerInfo != nullptr);
  ROSE_ASSERT(ownerInfo->get_physical_file_id() >= 0);
  const int descendantPhysicalFileId = ownerInfo->get_physical_file_id();
  for (Sg_File_Info *info :
       {parameterScope->get_file_info(), parameterScope->get_startOfConstruct(),
        parameterScope->get_endOfConstruct()}) {
    ROSE_ASSERT(info != nullptr);
    ROSE_ASSERT(info->get_parent() == parameterScope);
    ROSE_ASSERT(info->isCompilerGenerated());
    ROSE_ASSERT(info->isFrontendSpecific());
    ROSE_ASSERT(!info->isTransformation());
    ROSE_ASSERT(!info->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(info->isOutputInCodeGeneration());
    ROSE_ASSERT(info->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(info->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(!info->isShared());
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        parameterScope, info));
  }

  for (SgDeclarationStatement *declaration :
       parameterScope->get_declarations()) {
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() == parameterScope);
    ROSE_ASSERT(declaration->get_scope() == parameterScope);
    ROSE_ASSERT(declaration->get_file_info() != nullptr);
    ROSE_ASSERT(declaration->get_file_info()->get_physical_file_id() ==
                descendantPhysicalFileId);
  }
}

void verifyCanonicalSourceRange(SgProcedureHeaderStatement *canonical,
                                SgProcedureHeaderStatement *definition) {
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(definition != nullptr);
  Sg_File_Info *canonicalStart = canonical->get_startOfConstruct();
  Sg_File_Info *canonicalEnd = canonical->get_endOfConstruct();
  Sg_File_Info *definitionStart = definition->get_startOfConstruct();
  Sg_File_Info *definitionEnd = definition->get_endOfConstruct();
  ROSE_ASSERT(canonicalStart != nullptr);
  ROSE_ASSERT(canonicalEnd != nullptr);
  ROSE_ASSERT(definitionStart != nullptr);
  ROSE_ASSERT(definitionEnd != nullptr);
  ROSE_ASSERT(canonicalStart->get_raw_filename() ==
              definitionStart->get_raw_filename());
  ROSE_ASSERT(canonicalEnd->get_raw_filename() ==
              definitionEnd->get_raw_filename());
  ROSE_ASSERT(canonicalStart->get_physical_file_id() ==
              definitionStart->get_physical_file_id());
  ROSE_ASSERT(canonicalEnd->get_physical_file_id() ==
              definitionEnd->get_physical_file_id());
  ROSE_ASSERT(canonicalStart->get_raw_line() ==
              definitionStart->get_raw_line());
  ROSE_ASSERT(canonicalStart->get_raw_col() == definitionStart->get_raw_col());
  ROSE_ASSERT(canonicalEnd->get_raw_line() == definitionEnd->get_raw_line());
  ROSE_ASSERT(canonicalEnd->get_raw_col() == definitionEnd->get_raw_col());
}

void verifyAuxiliaryCanonical(SgProcedureHeaderStatement *canonical,
                              SgProcedureHeaderStatement *definition,
                              SgScopeStatement *scope) {
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_definition() == nullptr);
  ROSE_ASSERT(canonical->get_fortran_procedure_source_form() ==
              SgProcedureHeaderStatement::
                  e_fortran_procedure_source_form_semantic_only);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == definition);
  if (definition != nullptr) {
    ROSE_ASSERT(definition != canonical);
    ROSE_ASSERT(definition->get_firstNondefiningDeclaration() == canonical);
    ROSE_ASSERT(definition->get_definingDeclaration() == definition);
    ROSE_ASSERT(definition->get_scope() == scope);
    ROSE_ASSERT(
        definition->get_fortran_procedure_source_form() ==
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header);
  }

  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == scope);
  ROSE_ASSERT(scope->get_auxiliary_declarations() == auxiliary);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(), canonical) == 1);
  for (Sg_File_Info *info :
       {canonical->get_file_info(), canonical->get_startOfConstruct(),
        canonical->get_endOfConstruct()}) {
    ROSE_ASSERT(info != nullptr);
    ROSE_ASSERT(info->get_parent() == canonical);
    ROSE_ASSERT(info->isCompilerGenerated());
    ROSE_ASSERT(info->isOutputInCodeGeneration());
  }
  SgFunctionParameterList *parameters = canonical->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_parent() == canonical);
  verifyProcedureParameterSourceClassification(canonical,
                                               /*compilerGenerated=*/true);
  if (definition != nullptr) {
    SgFunctionParameterList *definitionParameters =
        definition->get_parameterList();
    ROSE_ASSERT(definitionParameters != nullptr);
    ROSE_ASSERT(definitionParameters != parameters);
    ROSE_ASSERT(definitionParameters->get_parent() == definition);
    ROSE_ASSERT(definitionParameters->get_args().size() ==
                parameters->get_args().size());
    for (std::size_t index = 0; index < parameters->get_args().size();
         ++index) {
      SgInitializedName *canonicalArgument = parameters->get_args()[index];
      SgInitializedName *definitionArgument =
          definitionParameters->get_args()[index];
      ROSE_ASSERT(canonicalArgument != nullptr);
      ROSE_ASSERT(definitionArgument != nullptr);
      ROSE_ASSERT(canonicalArgument != definitionArgument);
      ROSE_ASSERT(canonicalArgument->get_parent() == parameters);
      ROSE_ASSERT(definitionArgument->get_parent() == definitionParameters);
      ROSE_ASSERT(canonicalArgument->get_name() ==
                  definitionArgument->get_name());
      ROSE_ASSERT(canonicalArgument->get_type() ==
                  definitionArgument->get_type());
    }
    verifyProcedureParameterSourceClassification(definition,
                                                 /*compilerGenerated=*/false);
    verifyCanonicalSourceRange(canonical, definition);
  }
  verifyProcedureSymbol(canonical, scope);
}

struct ProcedureOwners {
  SgBasicBlock *host = nullptr;
  SgProcedureHeaderStatement *internalCanonical = nullptr;
  SgProcedureHeaderStatement *internalDefinition = nullptr;
  SgProcedureHeaderStatement *implicitCanonical = nullptr;
  SgProcedureHeaderStatement *interfaceSubroutine = nullptr;
  SgProcedureHeaderStatement *interfaceFunction = nullptr;
  SgProcedureHeaderStatement *statementFunctionCanonical = nullptr;
  SgProcedureHeaderStatement *statementFunction = nullptr;
};

ProcedureOwners verifyOwners(SgProject *project) {
  ProcedureOwners owners;
  SgProgramHeaderStatement *program = findProgramDefinition(project);
  owners.host = program->get_definition()->get_body();
  ROSE_ASSERT(owners.host != nullptr);

  owners.internalDefinition =
      findProcedure(project, "rex_internal_worker", true);
  owners.internalCanonical = isSgProcedureHeaderStatement(
      owners.internalDefinition->get_firstNondefiningDeclaration());
  verifyAuxiliaryCanonical(owners.internalCanonical, owners.internalDefinition,
                           owners.host);
  verifyCompletedParameterScope(owners.internalCanonical, owners.host);

  owners.implicitCanonical =
      findProcedure(project, "rex_implicit_external", false);
  verifyAuxiliaryCanonical(owners.implicitCanonical, nullptr, owners.host);

  const Rose_STL_Container<SgNode *> interfaces =
      NodeQuery::querySubTree(project, V_SgInterfaceStatement);
  ROSE_ASSERT(interfaces.size() == 1);
  SgInterfaceStatement *interfaceStatement =
      isSgInterfaceStatement(interfaces.front());
  ROSE_ASSERT(interfaceStatement != nullptr);
  ROSE_ASSERT(interfaceStatement->get_parent() == owners.host);
  ROSE_ASSERT(interfaceStatement->get_interface_body_list().size() == 2);
  for (SgInterfaceBody *body : interfaceStatement->get_interface_body_list()) {
    ROSE_ASSERT(body != nullptr);
    ROSE_ASSERT(body->get_parent() == interfaceStatement);
    ROSE_ASSERT(!body->get_use_function_name());
    SgProcedureHeaderStatement *procedure =
        isSgProcedureHeaderStatement(body->get_functionDeclaration());
    ROSE_ASSERT(procedure != nullptr);
    ROSE_ASSERT(procedure->get_parent() == body);
    ROSE_ASSERT(procedure->get_scope() == owners.host);
    ROSE_ASSERT(procedure->get_definition() == nullptr);
    ROSE_ASSERT(procedure->get_firstNondefiningDeclaration() == procedure);
    ROSE_ASSERT(procedure->get_definingDeclaration() == nullptr);
    ROSE_ASSERT(procedure->get_file_info() != nullptr);
    ROSE_ASSERT(!procedure->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(
        procedure->get_fortran_procedure_source_form() ==
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header);
    verifyProcedureParameterSourceClassification(procedure,
                                                 /*compilerGenerated=*/false);
    verifyProcedureSymbol(procedure, owners.host);
    if (procedure->get_name() == "rex_interface_subroutine") {
      ROSE_ASSERT(owners.interfaceSubroutine == nullptr);
      owners.interfaceSubroutine = procedure;
    } else if (procedure->get_name() == "rex_interface_function") {
      ROSE_ASSERT(owners.interfaceFunction == nullptr);
      owners.interfaceFunction = procedure;
    } else {
      ROSE_ABORT();
    }
  }
  ROSE_ASSERT(owners.interfaceSubroutine != nullptr);
  ROSE_ASSERT(owners.interfaceFunction != nullptr);
  const std::vector<SgDeclarationStatement *> expectedOrder{
      program, owners.interfaceSubroutine, owners.interfaceFunction,
      owners.internalDefinition};
  std::vector<SgDeclarationStatement *> reversedOrder = expectedOrder;
  std::reverse(reversedOrder.begin(), reversedOrder.end());
  ROSE_ASSERT(SageInterface::sortSgNodeListBasedOnAppearanceOrderInSource(
                  reversedOrder) == expectedOrder);
  verifyCompletedParameterScope(owners.interfaceSubroutine, owners.host);
  verifyCompletedParameterScope(owners.interfaceFunction, owners.host);

  const Rose_STL_Container<SgNode *> statementFunctions =
      NodeQuery::querySubTree(project, V_SgStatementFunctionStatement);
  ROSE_ASSERT(statementFunctions.size() == 1);
  SgStatementFunctionStatement *statementFunction =
      isSgStatementFunctionStatement(statementFunctions.front());
  ROSE_ASSERT(statementFunction != nullptr);
  ROSE_ASSERT(statementFunction->get_parent() == owners.host);
  owners.statementFunction =
      isSgProcedureHeaderStatement(statementFunction->get_function());
  ROSE_ASSERT(owners.statementFunction != nullptr);
  ROSE_ASSERT(owners.statementFunction->get_parent() == statementFunction);
  ROSE_ASSERT(owners.statementFunction->get_scope() == owners.host);
  owners.statementFunctionCanonical = isSgProcedureHeaderStatement(
      owners.statementFunction->get_firstNondefiningDeclaration());
  ROSE_ASSERT(owners.statementFunctionCanonical != nullptr);
  ROSE_ASSERT(owners.statementFunctionCanonical != owners.statementFunction);
  ROSE_ASSERT(owners.statementFunctionCanonical->get_parent() == owners.host);
  ROSE_ASSERT(owners.statementFunctionCanonical->get_scope() == owners.host);
  ROSE_ASSERT(
      owners.host->statementExistsInScope(owners.statementFunctionCanonical));
  ROSE_ASSERT(owners.statementFunctionCanonical->get_type() ==
              owners.statementFunction->get_type());
  ROSE_ASSERT(owners.statementFunctionCanonical->get_definingDeclaration() ==
              nullptr);
  ROSE_ASSERT(
      owners.statementFunctionCanonical->get_fortran_procedure_source_form() ==
      SgProcedureHeaderStatement::
          e_fortran_procedure_source_form_type_declaration);
  ROSE_ASSERT(owners.host->lookup_variable_symbol("rex_statement_function") ==
              nullptr);
  ROSE_ASSERT(owners.statementFunction->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(
      owners.statementFunction->get_fortran_procedure_source_form() ==
      SgProcedureHeaderStatement::e_fortran_procedure_source_form_header);
  verifyProcedureParameterSourceClassification(owners.statementFunction,
                                               /*compilerGenerated=*/false);
  verifyProcedureSymbol(owners.statementFunctionCanonical, owners.host);
  verifyCompletedParameterScope(owners.statementFunction, owners.host);
  verifyHeaderComments(owners.internalDefinition, "! rex_procedure_prefix",
                       "! rex_procedure_header");
  ROSE_ASSERT(owners.internalDefinition->getAttachedPreprocessingInfo()
                  ->back()
                  ->getLineNumber() ==
              owners.internalDefinition->get_startOfConstruct()->get_line() +
                  1);
  for (SgProcedureHeaderStatement *procedure :
       {owners.interfaceSubroutine, owners.interfaceFunction}) {
    const auto *header = procedure->getAttachedPreprocessingInfo();
    verifyHeaderComments(procedure, "! rex_interface_procedure_prefix",
                         "! rex_interface_header");
    ROSE_ASSERT(header->back()->getLineNumber() ==
                procedure->get_startOfConstruct()->get_line() + 1);
  }
  auto *interfaceBody =
      isSgInterfaceBody(owners.interfaceSubroutine->get_parent());
  ROSE_ASSERT(interfaceBody != nullptr);
  auto *interfaceBlock = isSgInterfaceStatement(interfaceBody->get_parent());
  ROSE_ASSERT(interfaceBlock != nullptr);
  verifyHeaderComments(interfaceBlock, "! rex_interface_block_prefix",
                       "! rex_interface_block_header");
  bool sawModule = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgModuleStatement)) {
    auto *module = isSgModuleStatement(node);
    ROSE_ASSERT(module != nullptr);
    if (module->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(!sawModule);
    ROSE_ASSERT(module->get_name() == "rex_procedure_comment_module");
    verifyHeaderComments(module, "! rex_module_prefix", "! rex_module_header");
    bool sawType = false;
    for (SgDeclarationStatement *member :
         module->get_definition()->get_members()) {
      auto *derived = isSgDerivedTypeStatement(member);
      if (derived == nullptr) {
        continue;
      }
      ROSE_ASSERT(!sawType && derived->get_name() == "rex_header_type");
      verifyHeaderComments(derived, "! rex_type_prefix", "! rex_type_header");
      ROSE_ASSERT(
          derived->getAttachedPreprocessingInfo()->back()->getLineNumber() ==
          derived->get_startOfConstruct()->get_line() + 1);
      sawType = true;
    }
    ROSE_ASSERT(sawType);
    sawModule = true;
  }
  ROSE_ASSERT(sawModule);
  return owners;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyOwners(project);
  AstTests::runAllTests(project);
  ROSE_ASSERT(backend(project) == 0);
  auto *file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(file != nullptr);
  std::ifstream output(file->get_unparse_output_filename());
  ROSE_ASSERT(output.is_open());
  int headerComments = 0;
  int interfacePrefixes = 0;
  std::string line;
  while (std::getline(output, line)) {
    if (line.find("! rex_procedure_header") != std::string::npos) {
      ROSE_ASSERT(line.find("SUBROUTINE rex_internal_worker") !=
                  std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_program_header") != std::string::npos) {
      ROSE_ASSERT(line.find("PROGRAM rex_procedure_canonical_ownership") !=
                  std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_interface_header") != std::string::npos) {
      ROSE_ASSERT(line.find("SUBROUTINE rex_interface_subroutine") !=
                      std::string::npos ||
                  line.find("FUNCTION rex_interface_function") !=
                      std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_module_header") != std::string::npos) {
      ROSE_ASSERT(line.find("MODULE rex_procedure_comment_module") !=
                  std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_type_header") != std::string::npos) {
      ROSE_ASSERT(line.find("TYPE ") != std::string::npos);
      ROSE_ASSERT(line.find(":: rex_header_type") != std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_interface_block_header") != std::string::npos) {
      ROSE_ASSERT(line.find("INTERFACE ") != std::string::npos);
      ROSE_ASSERT(line.find("END") == std::string::npos);
      ++headerComments;
    }
    if (line.find("! rex_interface_procedure_prefix") != std::string::npos) {
      ++interfacePrefixes;
    }
  }
  ROSE_ASSERT(headerComments == 7);
  ROSE_ASSERT(interfacePrefixes == 2);
  SgProject *copy = SageInterface::deepCopy(project);
  ROSE_ASSERT(copy != nullptr && copy != project);
  verifyOwners(copy);
  AstTests::runAllTests(copy);
  SageInterface::deepDelete(copy);
  verifyOwners(project);
  AstTests::runAllTests(project);
  return 0;
}
