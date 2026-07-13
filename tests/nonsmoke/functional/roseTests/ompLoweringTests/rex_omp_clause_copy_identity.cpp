#include "omp_lowering.h"

#include "rose.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

SgFunctionDeclaration *findFunction(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  SgFunctionDeclaration *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr || declaration->get_definition() == nullptr ||
        declaration->get_name().getString() != "rex_omp_clause_copy_identity") {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgVariableSymbol *requireClauseSymbol(SgOmpExecStatement *directive) {
  ROSE_ASSERT(directive != nullptr);
  SgOmpClauseBodyStatement *body = isSgOmpClauseBodyStatement(directive);
  ROSE_ASSERT(body != nullptr);
  SgVariableSymbol *result = nullptr;
  for (SgOmpClause *clause : body->get_clauses()) {
    SgOmpFirstprivateClause *firstprivate = isSgOmpFirstprivateClause(clause);
    if (firstprivate == nullptr)
      continue;
    SgExprListExp *variables = firstprivate->get_variables();
    ROSE_ASSERT(variables != nullptr &&
                variables->get_expressions().size() == 1);
    SgVarRefExp *reference =
        isSgVarRefExp(variables->get_expressions().front());
    ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
    ROSE_ASSERT(result == nullptr);
    result = isSgVariableSymbol(reference->get_symbol());
    ROSE_ASSERT(result != nullptr && result->get_declaration() != nullptr);
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

bool isWithin(const SgNode *node, const SgNode *root) {
  std::set<const SgNode *> visited;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    ROSE_ASSERT(visited.insert(current).second);
    if (current == root)
      return true;
  }
  return false;
}

void retireDetachedFunctionCopy(SgFunctionDeclaration *function,
                                SgFunctionDeclaration *sourceFunction,
                                SgGlobal *semanticScope,
                                SgCopyHelp::copiedNodeMapType &identityMap) {
  ROSE_ASSERT(function != nullptr && sourceFunction != nullptr &&
              semanticScope != nullptr);
  SgFunctionDeclaration *copiedCanonical =
      isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *sourceCanonical = isSgFunctionDeclaration(
      sourceFunction->get_firstNondefiningDeclaration());
  SgAuxiliaryDeclarationList *copiedAuxiliary =
      copiedCanonical != nullptr
          ? isSgAuxiliaryDeclarationList(copiedCanonical->get_parent())
          : nullptr;
  const auto canonicalIdentity = identityMap.find(sourceCanonical);
  SgFunctionSymbol *sourceSymbol =
      sourceCanonical != nullptr
          ? isSgFunctionSymbol(
                semanticScope->find_symbol_from_declaration(sourceCanonical))
          : nullptr;
  SgFunctionSymbol *copiedSymbol =
      copiedCanonical != nullptr
          ? isSgFunctionSymbol(
                semanticScope->find_symbol_from_declaration(copiedCanonical))
          : nullptr;
  ROSE_ASSERT(
      sourceCanonical != nullptr && sourceCanonical != sourceFunction &&
      copiedCanonical != nullptr && copiedCanonical != function &&
      copiedCanonical != sourceCanonical && copiedAuxiliary != nullptr &&
      copiedAuxiliary->get_parent() == nullptr &&
      copiedAuxiliary->get_declarations().size() == 1 &&
      copiedAuxiliary->get_declarations().front() == copiedCanonical &&
      copiedCanonical->get_parent() == copiedAuxiliary &&
      copiedCanonical->get_scope() == semanticScope &&
      copiedCanonical->get_firstNondefiningDeclaration() == copiedCanonical &&
      copiedCanonical->get_definingDeclaration() == function &&
      function->get_parent() == nullptr &&
      function->get_scope() == semanticScope &&
      function->get_definingDeclaration() == function &&
      canonicalIdentity != identityMap.end() &&
      canonicalIdentity->second == copiedCanonical && copiedSymbol == nullptr &&
      sourceSymbol != nullptr &&
      sourceSymbol->get_symbol_basis() == sourceCanonical &&
      sourceCanonical->get_definingDeclaration() == sourceFunction);

  // The copied defining declaration and its copied canonical prototype are one
  // detached semantic family.  Retire the auxiliary-owned prototype first,
  // then the detached definition.  Neither declaration was published in the
  // source symbol table, and the source family must remain untouched.
  identityMap.clear();
  copiedAuxiliary->get_declarations().clear();
  copiedCanonical->set_parent(nullptr);
  copiedCanonical->set_definingDeclaration(nullptr);
  function->set_firstNondefiningDeclaration(function);
  delete copiedAuxiliary;
  SageInterface::deleteAST(copiedCanonical,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copiedCanonical));
  SageInterface::deleteAST(function,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(function));
  ROSE_ASSERT(sourceCanonical->get_definingDeclaration() == sourceFunction &&
              sourceSymbol->get_symbol_basis() == sourceCanonical);
}

std::vector<SgFunctionDeclaration *> findOutlinedFunctions(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  std::vector<SgFunctionDeclaration *> result;
  std::set<SgFunctionDeclaration *> seen;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr || declaration->get_definition() == nullptr ||
        declaration->get_name().getString().rfind("OUT__", 0) != 0) {
      continue;
    }
    ROSE_ASSERT(seen.insert(declaration).second);
    result.push_back(declaration);
  }
  return result;
}

void requireExactOutlinedVariableOwnership(
    const std::vector<SgFunctionDeclaration *> &functions) {
  std::set<SgVariableSymbol *> symbolsAcrossFunctions;
  size_t relocatedGeneratedCastCount = 0;
  for (SgFunctionDeclaration *function : functions) {
    ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
    SgSourceFile *outputFile = SageInterface::getEnclosingSourceFile(function);
    ROSE_ASSERT(outputFile != nullptr &&
                outputFile->get_file_info() != nullptr);
    const int outputPhysicalId =
        outputFile->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(outputPhysicalId >= 0);

    for (SgCastExp *cast :
         SageInterface::querySubTree<SgCastExp>(function, V_SgCastExp)) {
      ROSE_ASSERT(cast != nullptr && cast->get_file_info() != nullptr);
      if (!cast->get_file_info()->isTransformation() &&
          !cast->get_file_info()->isCompilerGenerated()) {
        continue;
      }
      ++relocatedGeneratedCastCount;
      for (Sg_File_Info *position :
           {cast->get_file_info(), cast->get_startOfConstruct(),
            cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
        ROSE_ASSERT(position != nullptr && position->get_parent() == cast &&
                    !position->isShared() &&
                    position->get_physical_file_id() == outputPhysicalId);
      }
    }

    std::set<SgVariableSymbol *> localSymbols;
    for (SgNode *node :
         NodeQuery::querySubTree(function->get_definition(), V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(node);
      SgVariableSymbol *symbol =
          reference != nullptr ? isSgVariableSymbol(reference->get_symbol())
                               : nullptr;
      SgInitializedName *name =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      ROSE_ASSERT(reference != nullptr && symbol != nullptr &&
                  name != nullptr && name->get_type() != nullptr);
      SgScopeStatement *scope = name->get_scope();
      ROSE_ASSERT(scope != nullptr);
      const bool nonlocal =
          isSgGlobal(scope) != nullptr ||
          isSgNamespaceDefinitionStatement(scope) != nullptr ||
          isSgClassDefinition(scope) != nullptr;
      if (nonlocal)
        continue;
      ROSE_ASSERT(isWithin(name, function));
      localSymbols.insert(symbol);
    }
    ROSE_ASSERT(!localSymbols.empty());
    for (SgVariableSymbol *symbol : localSymbols)
      ROSE_ASSERT(symbolsAcrossFunctions.insert(symbol).second);
  }
  ROSE_ASSERT(relocatedGeneratedCastCount > 0);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  if (mode != "--accept-shadowed-repeated" &&
      mode != "--reject-missing-directive-map") {
    return 2;
  }

  std::vector<std::string> frontendArguments{argv[0],
                                             "-rose:openmp:ast_only",
                                             "-rose:skipfinalCompileStep",
                                             "-w",
                                             "-rose:verbose",
                                             "0",
                                             "-c",
                                             argv[2]};
  SgProject *project = frontend(frontendArguments);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(sourceFile != nullptr);
  const size_t initialFileCount = project->get_fileList().size();
  SgFunctionDeclaration *originalFunction = findFunction(project);

  SgCopyHelp::copiedNodeMapType identityMap;
  SgFunctionDeclaration *copiedFunction =
      isSgFunctionDeclaration(SageInterface::deepCopyNodeWithIdentityMap(
          originalFunction, identityMap));
  ROSE_ASSERT(copiedFunction != nullptr && copiedFunction != originalFunction);
  ROSE_ASSERT(copiedFunction->get_parent() == nullptr);

  const Rose_STL_Container<SgNode *> directiveNodes =
      NodeQuery::querySubTree(originalFunction, V_SgOmpParallelStatement);
  ROSE_ASSERT(directiveNodes.size() == 3);
  std::map<SgInitializedName *, size_t> originalVariableOccurrences;
  std::map<SgInitializedName *, size_t> copiedVariableOccurrences;
  std::set<SgOmpExecStatement *> copiedDirectives;

  if (mode == "--reject-missing-directive-map") {
    SgOmpExecStatement *directive =
        isSgOmpExecStatement(directiveNodes.front());
    ROSE_ASSERT(directive != nullptr);
    SgVariableSymbol *symbol = requireClauseSymbol(directive);
    SgInitializedName *name = symbol->get_declaration();
    ROSE_ASSERT(identityMap.erase(directive) == 1);
    (void)OmpSupport::requireExactClauseVariableCopyIdentity(
        identityMap, directive, name, symbol);
    return 1;
  }

  for (SgNode *node : directiveNodes) {
    SgOmpExecStatement *directive = isSgOmpExecStatement(node);
    ROSE_ASSERT(directive != nullptr);
    SgVariableSymbol *symbol = requireClauseSymbol(directive);
    SgInitializedName *name = symbol->get_declaration();
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == name);

    OmpSupport::ClauseVariableCopyIdentity copied =
        OmpSupport::requireExactClauseVariableCopyIdentity(
            identityMap, directive, name, symbol);
    ROSE_ASSERT(copied.directive != directive &&
                copied.clauseVariable != name &&
                copied.backingSymbol != symbol);
    ROSE_ASSERT(copied.backingSymbol->get_declaration() ==
                copied.clauseVariable);
    ROSE_ASSERT(requireClauseSymbol(copied.directive)->get_declaration() ==
                copied.clauseVariable);
    ROSE_ASSERT(isWithin(copied.directive, copiedFunction));
    ROSE_ASSERT(copiedDirectives.insert(copied.directive).second);
    ++originalVariableOccurrences[name];
    ++copiedVariableOccurrences[copied.clauseVariable];
  }

  ROSE_ASSERT(copiedDirectives.size() == 3);
  ROSE_ASSERT(originalVariableOccurrences.size() == 2);
  ROSE_ASSERT(copiedVariableOccurrences.size() == 2);
  std::multiset<size_t> originalCounts;
  std::multiset<size_t> copiedCounts;
  for (const auto &entry : originalVariableOccurrences) {
    ROSE_ASSERT(entry.first->get_name().getString() == "shared");
    originalCounts.insert(entry.second);
  }
  for (const auto &entry : copiedVariableOccurrences) {
    ROSE_ASSERT(entry.first->get_name().getString() == "shared");
    copiedCounts.insert(entry.second);
  }
  ROSE_ASSERT(originalCounts == std::multiset<size_t>({1, 2}));
  ROSE_ASSERT(copiedCounts == originalCounts);

  retireDetachedFunctionCopy(copiedFunction, originalFunction,
                             sourceFile->get_globalScope(), identityMap);
  OmpSupport::lower_omp(sourceFile);
  ROSE_ASSERT(project->get_fileList().size() > initialFileCount);
  ROSE_ASSERT(
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement).empty());
  const std::vector<SgFunctionDeclaration *> outlinedFunctions =
      findOutlinedFunctions(project);
  ROSE_ASSERT(outlinedFunctions.size() == 3);
  requireExactOutlinedVariableOwnership(outlinedFunctions);
  AstTests::runAllTests(project);
  SageInterface::tearDownAst(project);
  return 0;
}
