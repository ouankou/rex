/*
test code for loopTiling
by Liao, 6/25/2009
*/
#include "RoseAst.h"
#include "rose.h"

#include <iostream>
#include <string>
#include <vector>

#include "commandline_processing.h"

using namespace std;

namespace {

SgExpression *exactSemanticHeaderExpression(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expression)) {
    SgExpression *expanded = macro->get_expanded_expression_checked();
    ROSE_ASSERT(isSgMacroExpansionExp(expanded) == nullptr);
    return expanded;
  }
  return expression;
}

struct LoopIdentitySnapshot {
  std::vector<SgNode *> projectNodes;
  SgForStatement *target = nullptr;
  SgNode *targetParent = nullptr;
  SgForInitStatement *initialization = nullptr;
  SgStatement *testStatement = nullptr;
  SgExpression *testSurface = nullptr;
  SgExpression *testSemantic = nullptr;
  SgExpression *incrementSurface = nullptr;
  SgExpression *incrementSemantic = nullptr;
  SgStatement *body = nullptr;
};

SgForStatement *exactNestedLoop(SgForStatement *outer, int depth) {
  ROSE_ASSERT(outer != nullptr && depth > 0);
  SgForStatement *current = outer;
  for (int level = 1; level < depth; ++level) {
    SgStatement *body = current->get_loop_body();
    ROSE_ASSERT(body != nullptr && body->get_parent() == current);
    SgForStatement *nested = isSgForStatement(body);
    if (SgBasicBlock *block = isSgBasicBlock(body)) {
      ROSE_ASSERT(block->get_statements().size() == 1);
      nested = isSgForStatement(block->get_statements().front());
    }
    ROSE_ASSERT(nested != nullptr);
    current = nested;
  }
  return current;
}

LoopIdentitySnapshot snapshotLoopIdentity(SgProject *project,
                                          SgForStatement *loopNest, int depth) {
  ROSE_ASSERT(project != nullptr && loopNest != nullptr && depth > 0);
  SgForStatement *target = exactNestedLoop(loopNest, depth);

  LoopIdentitySnapshot snapshot;
  RoseAst ast(project);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    snapshot.projectNodes.push_back(*current);
  }
  snapshot.target = target;
  snapshot.targetParent = target->get_parent();
  snapshot.initialization = target->get_for_init_stmt();
  snapshot.testStatement = target->get_test();
  snapshot.testSurface = target->get_test_expr();
  snapshot.testSemantic = exactSemanticHeaderExpression(snapshot.testSurface);
  snapshot.incrementSurface = target->get_increment();
  snapshot.incrementSemantic =
      exactSemanticHeaderExpression(snapshot.incrementSurface);
  snapshot.body = target->get_loop_body();
  return snapshot;
}

void requireExactIdentity(const LoopIdentitySnapshot &before,
                          const LoopIdentitySnapshot &after) {
  ROSE_ASSERT(after.projectNodes == before.projectNodes);
  ROSE_ASSERT(after.target == before.target);
  ROSE_ASSERT(after.targetParent == before.targetParent);
  ROSE_ASSERT(after.initialization == before.initialization);
  ROSE_ASSERT(after.testStatement == before.testStatement);
  ROSE_ASSERT(after.testSurface == before.testSurface);
  ROSE_ASSERT(after.testSemantic == before.testSemantic);
  ROSE_ASSERT(after.incrementSurface == before.incrementSurface);
  ROSE_ASSERT(after.incrementSemantic == before.incrementSemantic);
  ROSE_ASSERT(after.body == before.body);
}

} // namespace

int main(int argc, char *argv[])

{
  int line;
  int tilesize = 1, depth = 1;
  // command line processing
  //--------------------------------------------------
  vector<std::string> argvList(argv, argv + argc);
  if (!CommandlineProcessing::isOptionWithParameter(
          argvList, "-rose:loopTiling:", "line", line, true) ||
      !CommandlineProcessing::isOptionWithParameter(
          argvList, "-rose:loopTiling:", "depth", depth, true) ||
      !CommandlineProcessing::isOptionWithParameter(
          argvList, "-rose:loopTiling:", "tilesize", tilesize, true)) {
    cout << "Usage: loopTiling inputFile.c -rose:loopTiling:line <line_number> "
            "-rose:loopTiling:depth D -rose:loopTiling:tilesize N"
         << endl;
    return 0;
  }

  // Retrieve corresponding SgForStatement from line number
  //--------------------------------------------------
  SgProject *project = frontend(argvList);
  SgForStatement *forLoop = NULL;
  ROSE_ASSERT(project != NULL);
  SgFilePtrList &filelist = project->get_fileList();
  SgFilePtrList::iterator iter = filelist.begin();
  for (; iter != filelist.end(); iter++) {
    SgSourceFile *sFile = isSgSourceFile(*iter);
    SgStatement *stmt = SageInterface::getFirstStatementAtLine(sFile, line);
    forLoop = isSgForStatement(stmt);
    if (forLoop != NULL) {
      cout << "Find a loop from line:" << line << endl;
      break;
    } else {
      cout << "Cannot find a matching target from line:" << line << endl;
      return 0;
    }
  }

  // Tile it
  //--------------------------------------------------
  LoopIdentitySnapshot identityBefore;
  if (tilesize == 1) {
    identityBefore = snapshotLoopIdentity(project, forLoop, depth);
  }
  SageInterface::loopTiling(forLoop, depth, tilesize);
  if (tilesize == 1) {
    requireExactIdentity(identityBefore,
                         snapshotLoopIdentity(project, forLoop, depth));
  }

  //  AstPostProcessing(project);
  // run all tests
  AstTests::runAllTests(project);

  // Generate source code from AST and call the vendor's compiler
  return backend(project);
}
