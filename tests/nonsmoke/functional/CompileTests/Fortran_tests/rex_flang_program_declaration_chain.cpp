#include "rose.h"

#include <algorithm>
#include <string>

namespace {

void verifyGeneratedAnchor(const SgLocatedNode *node,
                           const SgLocatedNode *sourceAnchor) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(sourceAnchor != nullptr);
  const Sg_File_Info *nodeStart = node->get_startOfConstruct();
  const Sg_File_Info *nodeEnd = node->get_endOfConstruct();
  const Sg_File_Info *sourceStart = sourceAnchor->get_startOfConstruct();
  const Sg_File_Info *sourceEnd = sourceAnchor->get_endOfConstruct();
  ROSE_ASSERT(node->get_file_info() != nullptr);
  ROSE_ASSERT(nodeStart != nullptr);
  ROSE_ASSERT(nodeEnd != nullptr);
  ROSE_ASSERT(sourceStart != nullptr);
  ROSE_ASSERT(sourceEnd != nullptr);
  ROSE_ASSERT(node->get_file_info()->get_parent() == node);
  ROSE_ASSERT(nodeStart->get_parent() == node);
  ROSE_ASSERT(nodeEnd->get_parent() == node);
  ROSE_ASSERT(node->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(nodeStart->isCompilerGenerated());
  ROSE_ASSERT(nodeEnd->isCompilerGenerated());
  ROSE_ASSERT(nodeStart->get_raw_filename() == sourceStart->get_raw_filename());
  ROSE_ASSERT(nodeStart->get_raw_line() == sourceStart->get_raw_line());
  ROSE_ASSERT(nodeStart->get_raw_col() == sourceStart->get_raw_col());
  ROSE_ASSERT(nodeEnd->get_raw_filename() == sourceEnd->get_raw_filename());
  ROSE_ASSERT(nodeEnd->get_raw_line() == sourceEnd->get_raw_line());
  ROSE_ASSERT(nodeEnd->get_raw_col() == sourceEnd->get_raw_col());
}

void verifyProgramFamily(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);

  const Rose_STL_Container<SgNode *> programs =
      NodeQuery::querySubTree(file, V_SgProgramHeaderStatement);
  ROSE_ASSERT(programs.size() == 2);

  SgProgramHeaderStatement *definition = nullptr;
  for (SgNode *node : programs) {
    SgProgramHeaderStatement *candidate = isSgProgramHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_definition() != nullptr) {
      ROSE_ASSERT(definition == nullptr);
      definition = candidate;
    }
  }
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  ROSE_ASSERT(definition->get_definition()->get_declaration() == definition);

  SgProgramHeaderStatement *canonical =
      isSgProgramHeaderStatement(definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical != definition);
  ROSE_ASSERT(std::find(programs.begin(), programs.end(), canonical) !=
              programs.end());
  ROSE_ASSERT(canonical->get_definition() == nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == definition);
  ROSE_ASSERT(canonical->get_type() == definition->get_type());
  ROSE_ASSERT(canonical->get_program_statement_kind() ==
              definition->get_program_statement_kind());
  ROSE_ASSERT(canonical->get_name() == definition->get_name());
  ROSE_ASSERT(canonical->get_named_in_end_statement() ==
              definition->get_named_in_end_statement());
  ROSE_ASSERT(canonical->get_end_statement_name() ==
              definition->get_end_statement_name());

  SgFunctionParameterList *definitionParameters =
      definition->get_parameterList();
  SgFunctionParameterList *canonicalParameters = canonical->get_parameterList();
  ROSE_ASSERT(definitionParameters != nullptr);
  ROSE_ASSERT(canonicalParameters != nullptr);
  ROSE_ASSERT(definitionParameters != canonicalParameters);
  ROSE_ASSERT(definitionParameters->get_parent() == definition);
  ROSE_ASSERT(canonicalParameters->get_parent() == canonical);
  ROSE_ASSERT(definitionParameters->get_args().empty());
  ROSE_ASSERT(canonicalParameters->get_args().empty());
  ROSE_ASSERT(definition->get_file_info() != nullptr);
  ROSE_ASSERT(!definition->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(definition->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(definition->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(!definition->get_startOfConstruct()->isCompilerGenerated());
  ROSE_ASSERT(!definition->get_endOfConstruct()->isCompilerGenerated());
  ROSE_ASSERT(definition->get_startOfConstruct()->get_parent() == definition);
  ROSE_ASSERT(definition->get_endOfConstruct()->get_parent() == definition);
  ROSE_ASSERT(definition->get_endOfConstruct()->get_line() >
                  definition->get_startOfConstruct()->get_line() ||
              (definition->get_endOfConstruct()->get_line() ==
                   definition->get_startOfConstruct()->get_line() &&
               definition->get_endOfConstruct()->get_col() >
                   definition->get_startOfConstruct()->get_col()));
  verifyGeneratedAnchor(canonical, definition);
  verifyGeneratedAnchor(canonicalParameters, definitionParameters);
  ROSE_ASSERT(canonical->get_file_info()->isOutputInCodeGeneration());
  ROSE_ASSERT(canonical->get_startOfConstruct()->isOutputInCodeGeneration());
  ROSE_ASSERT(canonical->get_endOfConstruct()->isOutputInCodeGeneration());

  SgGlobal *global = isSgGlobal(definition->get_scope());
  ROSE_ASSERT(global != nullptr);
  ROSE_ASSERT(definition->get_parent() == global);
  ROSE_ASSERT(canonical->get_scope() == global);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(), definition) == 1);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(), canonical) == 0);

  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == global);
  ROSE_ASSERT(global->get_auxiliary_declarations() == auxiliary);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(), canonical) == 1);

  const SgName symbolKey =
      SageInterface::getFortranProgramUnitSymbolTableKey(definition);
  ROSE_ASSERT(symbolKey ==
              SageInterface::getFortranProgramUnitSymbolTableKey(canonical));
  ROSE_ASSERT(global->get_symbol_table()->get_name(definition) == symbolKey);
  ROSE_ASSERT(global->get_symbol_table()->get_name(canonical) == symbolKey);
  SgFunctionSymbol *symbol =
      isSgFunctionSymbol(canonical->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == canonical);
  ROSE_ASSERT(symbol->get_symbol_basis() == canonical);
  ROSE_ASSERT(global->get_symbol_table()->find_function(symbolKey) == symbol);
  ROSE_ASSERT(global->find_symbol_from_declaration(canonical) == symbol);

  switch (definition->get_program_statement_kind()) {
  case SgProgramHeaderStatement::e_explicit_program_statement:
    ROSE_ASSERT(!definition->get_name().is_null());
    ROSE_ASSERT(symbolKey == definition->get_name());
    ROSE_ASSERT(
        !SageInterface::isFortranProgramUnitWithoutSourceName(definition));
    ROSE_ASSERT(
        !SageInterface::isFortranProgramUnitWithoutSourceName(canonical));
    break;
  case SgProgramHeaderStatement::e_implicit_program_statement:
    ROSE_ASSERT(definition->get_name().is_null());
    ROSE_ASSERT(definition->get_mangled_name().is_null());
    ROSE_ASSERT(
        symbolKey.getString().find("__rex_internal_implicit_program_") == 0);
    ROSE_ASSERT(
        SageInterface::isFortranProgramUnitWithoutSourceName(definition));
    ROSE_ASSERT(
        SageInterface::isFortranProgramUnitWithoutSourceName(canonical));
    break;
  default:
    ROSE_ABORT();
  }
}

void rebuildGlobalSymbolTable(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  SgGlobal *global = file->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgSymbolTable *table = global->get_symbol_table();
  ROSE_ASSERT(table != nullptr);
  ROSE_ASSERT(table->get_table() != nullptr);
  table->get_table()->clear();
  table->set_symbolSet(SgNodeSet());
  table->clear_functionSymbolExactIndex();
  ROSE_ASSERT(table->size() == 0);
  SageInterface::rebuildSymbolTable(global);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyProgramFamily(project);
  AstTests::runAllTests(project);

  SgProject *copy = SageInterface::deepCopy(project);
  ROSE_ASSERT(copy != nullptr);
  ROSE_ASSERT(copy != project);
  verifyProgramFamily(copy);
  AstTests::runAllTests(copy);
  SageInterface::deepDelete(copy);

  rebuildGlobalSymbolTable(project);
  verifyProgramFamily(project);
  return 0;
}
