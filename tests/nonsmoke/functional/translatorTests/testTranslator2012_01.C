// This program demonstrates an exact two-translation-unit transformation: it
// rebuilds a free-function definition in the second file immediately before
// main, then removes the source definition through the statement-mutation API.
// Reusing the source declaration nodes would require an atomic cross-TU
// declaration-family relocation transaction; changing parent, scope, symbol,
// and source-position fields independently is malformed.

#include "rose.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace std;

void verifyPerTranslationUnitCommandOwnership(SgProject *project) {
  ROSE_ASSERT(project != NULL);
  ROSE_ASSERT(project->numberOfFiles() > 1);

  std::set<std::string> ownedSources;
  for (SgFile *file : project->get_fileList()) {
    ROSE_ASSERT(file != NULL);
    const std::vector<std::string> &command =
        file->get_originalCommandLineArgumentList();
    ROSE_ASSERT(!command.empty());
    const Rose_STL_Container<std::string> sourceInputs =
        CommandlineProcessing::generateSourceFilenames(
            command, project->get_binary_only());
    ROSE_ASSERT(sourceInputs.size() == 1);

    const std::string absoluteSource =
        Rose::StringUtility::getAbsolutePathFromRelativePath(
            sourceInputs.front(), true);
    ROSE_ASSERT(absoluteSource == file->get_sourceFileNameWithPath());
    ROSE_ASSERT(ownedSources.insert(absoluteSource).second);
  }
  ROSE_ASSERT(ownedSources.size() == project->get_fileList().size());
}

SgFunctionDeclaration *findExactDefiningFunction(SgGlobal *global,
                                                 const std::string &name) {
  ROSE_ASSERT(global != NULL);
  SgFunctionDeclaration *result = NULL;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    if (function == NULL || function->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(function->get_definition() != NULL);
    ROSE_ASSERT(function->get_definingDeclaration() == function);
    ROSE_ASSERT(result == NULL);
    result = function;
  }
  ROSE_ASSERT(result != NULL);
  return result;
}

SgFunctionDeclaration *rebuildExactFreeFunctionDefinition(
    SgFunctionDeclaration *sourceDefinition, SgGlobal *sourceGlobal,
    SgGlobal *targetGlobal, SgFunctionDeclaration *targetAnchor) {
  ROSE_ASSERT(sourceDefinition != NULL);
  ROSE_ASSERT(sourceGlobal != NULL);
  ROSE_ASSERT(targetGlobal != NULL);
  ROSE_ASSERT(sourceGlobal != targetGlobal);
  ROSE_ASSERT(targetAnchor != NULL);
  ROSE_ASSERT(isSgMemberFunctionDeclaration(sourceDefinition) == NULL);
  ROSE_ASSERT(sourceDefinition->get_definition() != NULL);
  ROSE_ASSERT(sourceDefinition->get_definingDeclaration() == sourceDefinition);
  ROSE_ASSERT(sourceDefinition->get_parent() == sourceGlobal);
  ROSE_ASSERT(sourceDefinition->get_scope() == sourceGlobal);
  ROSE_ASSERT(targetAnchor->get_parent() == targetGlobal);
  ROSE_ASSERT(targetAnchor->get_scope() == targetGlobal);

  SgFunctionDeclaration *first = isSgFunctionDeclaration(
      sourceDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != NULL);
  ROSE_ASSERT(first != sourceDefinition);
  ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(first->get_definingDeclaration() == sourceDefinition);
  ROSE_ASSERT(first->get_scope() == sourceGlobal);
  SgAuxiliaryDeclarationList *sourceAuxiliary =
      isSgAuxiliaryDeclarationList(first->get_parent());
  ROSE_ASSERT(sourceAuxiliary != NULL);
  ROSE_ASSERT(sourceAuxiliary == sourceGlobal->get_auxiliary_declarations());
  ROSE_ASSERT(sourceAuxiliary->get_parent() == sourceGlobal);
  ROSE_ASSERT(std::count(sourceAuxiliary->get_declarations().begin(),
                         sourceAuxiliary->get_declarations().end(),
                         first) == 1);

  SgFunctionSymbol *sourceSymbol =
      isSgFunctionSymbol(sourceGlobal->find_symbol_from_declaration(first));
  ROSE_ASSERT(sourceSymbol != NULL);
  ROSE_ASSERT(sourceSymbol->get_declaration() == first);
  ROSE_ASSERT(sourceSymbol->get_symbol_basis() == first);
  ROSE_ASSERT(sourceSymbol->get_parent() == sourceGlobal->get_symbol_table());
  ROSE_ASSERT(sourceGlobal->get_symbol_table()->find_function(
                  sourceDefinition->get_name(), sourceDefinition->get_type(),
                  NULL) == sourceSymbol);
  ROSE_ASSERT(targetGlobal->get_symbol_table()->find_function(
                  sourceDefinition->get_name(), sourceDefinition->get_type(),
                  NULL) == NULL);

  SgFunctionDeclaration *targetFirst =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::semanticAuxiliary(),
          sourceDefinition, targetGlobal);
  SgFunctionParameterList *targetParameters =
      SageInterface::deepCopy(sourceDefinition->get_parameterList());
  ROSE_ASSERT(targetParameters != NULL);
  ROSE_ASSERT(targetParameters->get_parent() == NULL);
  SgFunctionDeclaration *targetDefinition =
      SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexicalBefore(
              targetGlobal, targetAnchor),
          sourceDefinition->get_name(),
          sourceDefinition->get_type()->get_return_type(), targetParameters,
          targetGlobal, false, targetFirst, NULL, false);
  ROSE_ASSERT(targetDefinition != NULL);
  ROSE_ASSERT(targetDefinition->get_parent() == targetGlobal);
  ROSE_ASSERT(targetDefinition->get_scope() == targetGlobal);
  ROSE_ASSERT(targetDefinition->get_firstNondefiningDeclaration() ==
              targetFirst);
  ROSE_ASSERT(targetFirst->get_definingDeclaration() == targetDefinition);

  SgBasicBlock *sourceBody = sourceDefinition->get_definition()->get_body();
  SgBasicBlock *targetBody = targetDefinition->get_definition()->get_body();
  ROSE_ASSERT(sourceBody != NULL);
  ROSE_ASSERT(targetBody != NULL);
  ROSE_ASSERT(targetBody->get_statements().empty());
  for (SgStatement *sourceStatement : sourceBody->get_statements()) {
    SgStatement *targetStatement = SageInterface::deepCopy(sourceStatement);
    ROSE_ASSERT(targetStatement != NULL);
    ROSE_ASSERT(targetStatement->get_parent() == NULL);
    SageInterface::setSourcePositionForTransformation(targetStatement);
    SageInterface::appendStatement(targetStatement, targetBody);
  }
  ROSE_ASSERT(targetBody->get_statements().size() ==
              sourceBody->get_statements().size());

  const SgDeclarationStatementPtrList &targetDeclarations =
      targetGlobal->get_declarations();
  const auto definitionPosition = std::find(
      targetDeclarations.begin(), targetDeclarations.end(), targetDefinition);
  const auto anchorPosition = std::find(targetDeclarations.begin(),
                                        targetDeclarations.end(), targetAnchor);
  ROSE_ASSERT(definitionPosition != targetDeclarations.end());
  ROSE_ASSERT(anchorPosition != targetDeclarations.end());
  ROSE_ASSERT(definitionPosition < anchorPosition);
  SgFunctionSymbol *targetSymbol = isSgFunctionSymbol(
      targetGlobal->find_symbol_from_declaration(targetFirst));
  ROSE_ASSERT(targetSymbol != NULL);
  ROSE_ASSERT(targetSymbol->get_declaration() == targetFirst);
  ROSE_ASSERT(targetSymbol->get_parent() == targetGlobal->get_symbol_table());

  // Removal severs the source definition from its canonical semantic
  // declaration before detaching it.  The source symbol remains valid through
  // that canonical auxiliary declaration; no raw symbol or scope move occurs.
  SageInterface::removeStatement(sourceDefinition, false);
  ROSE_ASSERT(sourceDefinition->get_parent() == NULL);
  ROSE_ASSERT(sourceDefinition->get_firstNondefiningDeclaration() ==
              sourceDefinition);
  ROSE_ASSERT(sourceDefinition->get_definingDeclaration() == sourceDefinition);
  ROSE_ASSERT(first->get_definingDeclaration() == NULL);
  ROSE_ASSERT(first->get_parent() == sourceAuxiliary);
  ROSE_ASSERT(sourceGlobal->find_symbol_from_declaration(first) ==
              sourceSymbol);
  ROSE_ASSERT(sourceGlobal->get_declarations().empty());
  SageInterface::deleteAST(sourceDefinition,
                           SageInterface::DeleteAstMode::kRequireIsolated);

  return targetDefinition;
}

int main(int argc, char *argv[]) {
  // Build the abstract syntax tree
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  verifyPerTranslationUnitCommandOwnership(project);
  ROSE_ASSERT(project->numberOfFiles() == 2);

  SgSourceFile *sourceFile = isSgSourceFile(&project->get_file(0));
  SgSourceFile *targetFile = isSgSourceFile(&project->get_file(1));
  ROSE_ASSERT(sourceFile != NULL);
  ROSE_ASSERT(targetFile != NULL);
  SgGlobal *sourceGlobal = sourceFile->get_globalScope();
  SgGlobal *targetGlobal = targetFile->get_globalScope();
  ROSE_ASSERT(sourceGlobal != NULL);
  ROSE_ASSERT(targetGlobal != NULL);

  SgFunctionDeclaration *foobar =
      findExactDefiningFunction(sourceGlobal, "foobar");
  SgFunctionDeclaration *mainFunction =
      findExactDefiningFunction(targetGlobal, "main");
  ROSE_ASSERT(sourceGlobal->get_declarations().size() == 1);
  ROSE_ASSERT(targetGlobal->get_declarations().size() == 1);
  SgFunctionDeclaration *rebuiltFoobar = rebuildExactFreeFunctionDefinition(
      foobar, sourceGlobal, targetGlobal, mainFunction);
  ROSE_ASSERT(rebuiltFoobar != NULL);

  // The ownership-complete transformation must pass every AST invariant.
  AstTests::runAllTests(project);

  // Output an optional graph of the AST (just the tree, when active)
  generateDOT(*project);

  // Output an optional graph of the AST (the whole graph, of bounded
  // complexity, when active).
  const int MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH = 10000;
  generateAstGraph(project, MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH,
                   "");

  return backend(project); // only backend error code is reported
}
