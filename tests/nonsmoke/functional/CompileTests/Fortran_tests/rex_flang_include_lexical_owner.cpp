#include "rose.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

SgFortranIncludeLine *findInclude(SgProject *project,
                                  const std::string &filename) {
  SgFortranIncludeLine *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFortranIncludeLine)) {
    SgFortranIncludeLine *include = isSgFortranIncludeLine(node);
    ROSE_ASSERT(include != nullptr);
    if (include->get_filename() == filename) {
      ROSE_ASSERT(result == nullptr);
      result = include;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

std::string normalizePath(const std::filesystem::path &path) {
  const std::string result =
      std::filesystem::weakly_canonical(path).lexically_normal().string();
  ROSE_ASSERT(!result.empty());
  return result;
}

std::string sourcePath(const SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  const std::string startPath = normalizePath(start->get_filenameString());
  const std::string endPath = normalizePath(end->get_filenameString());
  ROSE_ASSERT(startPath == endPath);
  return startPath;
}

template <typename Node>
std::vector<Node *> nodesFromFile(SgProject *project, VariantT variant,
                                  const std::string &path) {
  std::vector<Node *> result;
  for (SgNode *node : NodeQuery::querySubTree(project, variant)) {
    Node *candidate = dynamic_cast<Node *>(node);
    ROSE_ASSERT(candidate != nullptr);
    if (sourcePath(candidate) == path) {
      result.push_back(candidate);
    }
  }
  return result;
}

std::size_t statementIndex(SgScopeStatement *scope, SgStatement *statement) {
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(statement != nullptr);
  const SgStatementPtrList statements = scope->generateStatementList();
  const auto position =
      std::find(statements.begin(), statements.end(), statement);
  ROSE_ASSERT(position != statements.end());
  return static_cast<std::size_t>(position - statements.begin());
}

void verifyIncludeOwners(SgProject *project, const std::string &inputPath) {
  ROSE_ASSERT(project != nullptr);
  const Rose_STL_Container<SgNode *> includes =
      NodeQuery::querySubTree(project, V_SgFortranIncludeLine);
  ROSE_ASSERT(includes.size() == 3);

  SgFortranIncludeLine *programInclude =
      findInclude(project, "rex_flang_include_program.inc");
  SgFortranIncludeLine *ifInclude =
      findInclude(project, "rex_flang_include_if.inc");
  SgFortranIncludeLine *doInclude =
      findInclude(project, "rex_flang_include_do.inc");

  SgBasicBlock *programBody = isSgBasicBlock(programInclude->get_parent());
  SgBasicBlock *ifBody = isSgBasicBlock(ifInclude->get_parent());
  SgBasicBlock *doBody = isSgBasicBlock(doInclude->get_parent());
  ROSE_ASSERT(programBody != nullptr);
  ROSE_ASSERT(ifBody != nullptr);
  ROSE_ASSERT(doBody != nullptr);
  ROSE_ASSERT(programBody != ifBody);
  ROSE_ASSERT(programBody != doBody);
  ROSE_ASSERT(ifBody != doBody);

  SgFunctionDefinition *programDefinition =
      isSgFunctionDefinition(programBody->get_parent());
  ROSE_ASSERT(programDefinition != nullptr);
  ROSE_ASSERT(programDefinition->get_body() == programBody);

  SgIfStmt *ifStatement = isSgIfStmt(ifBody->get_parent());
  ROSE_ASSERT(ifStatement != nullptr);
  ROSE_ASSERT(ifStatement->get_true_body() == ifBody);

  SgFortranDo *doStatement = isSgFortranDo(doBody->get_parent());
  ROSE_ASSERT(doStatement != nullptr);
  ROSE_ASSERT(doStatement->get_body() == doBody);

  const std::filesystem::path includeDirectory =
      std::filesystem::path(inputPath).parent_path();
  const std::string programPath =
      normalizePath(includeDirectory / "rex_flang_include_program.inc");
  const std::string ifPath =
      normalizePath(includeDirectory / "rex_flang_include_if.inc");
  const std::string doPath =
      normalizePath(includeDirectory / "rex_flang_include_do.inc");

  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(sourceFile != nullptr);
  const SgStringList &ownedIncludes =
      sourceFile->get_frontendIncludeOwnershipPathList();
  std::set<std::string> normalizedOwnedIncludes;
  for (const std::string &path : ownedIncludes) {
    ROSE_ASSERT(normalizedOwnedIncludes.insert(normalizePath(path)).second);
  }
  ROSE_ASSERT(normalizedOwnedIncludes ==
              std::set<std::string>({programPath, ifPath, doPath}));
  const SgStringList &externalInputs =
      sourceFile->get_frontendExternalOwnershipPathList();
  for (const std::string &path : externalInputs) {
    const std::string normalized = normalizePath(path);
    ROSE_ASSERT(normalized != programPath);
    ROSE_ASSERT(normalized != ifPath);
    ROSE_ASSERT(normalized != doPath);
  }

  const std::vector<SgExprStatement *> programStatements =
      nodesFromFile<SgExprStatement>(project, V_SgExprStatement, programPath);
  const std::vector<SgIfStmt *> ifStatements =
      nodesFromFile<SgIfStmt>(project, V_SgIfStmt, ifPath);
  const std::vector<SgExprStatement *> ifExpressions =
      nodesFromFile<SgExprStatement>(project, V_SgExprStatement, ifPath);
  const std::vector<SgExprStatement *> doStatements =
      nodesFromFile<SgExprStatement>(project, V_SgExprStatement, doPath);
  ROSE_ASSERT(programStatements.size() == 1);
  ROSE_ASSERT(ifStatements.size() == 1);
  ROSE_ASSERT(ifExpressions.size() == 2);
  ROSE_ASSERT(doStatements.size() == 1);

  SgIfStmt *includedIf = ifStatements.front();
  SgBasicBlock *includedIfBody = isSgBasicBlock(includedIf->get_true_body());
  ROSE_ASSERT(includedIfBody != nullptr);
  SgExprStatement *includedCondition =
      isSgExprStatement(includedIf->get_conditional());
  ROSE_ASSERT(includedCondition != nullptr);
  const auto includedAssignment =
      std::find_if(ifExpressions.begin(), ifExpressions.end(),
                   [includedCondition](SgExprStatement *expression) {
                     return expression != includedCondition;
                   });
  ROSE_ASSERT(includedAssignment != ifExpressions.end());
  ROSE_ASSERT(std::count(ifExpressions.begin(), ifExpressions.end(),
                         includedCondition) == 1);
  ROSE_ASSERT(programStatements.front()->get_parent() == programBody);
  ROSE_ASSERT(includedIf->get_parent() == ifBody);
  ROSE_ASSERT(includedCondition->get_parent() == includedIf);
  ROSE_ASSERT((*includedAssignment)->get_parent() == includedIfBody);
  ROSE_ASSERT(doStatements.front()->get_parent() == doBody);

  ROSE_ASSERT(statementIndex(programBody, programInclude) + 1 ==
              statementIndex(programBody, programStatements.front()));
  ROSE_ASSERT(statementIndex(ifBody, ifInclude) + 1 ==
              statementIndex(ifBody, includedIf));
  ROSE_ASSERT(statementIndex(doBody, doInclude) + 1 ==
              statementIndex(doBody, doStatements.front()));
}

} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc > 1);
  const std::string inputPath = argv[argc - 1];
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyIncludeOwners(project, inputPath);
  AstTests::runAllTests(project);
  return 0;
}
