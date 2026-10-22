#include "analysisUtils.h"

#include "sage3basic.h"

using namespace std;

stmt_pos findPosition(SgNode *n) {
  SgBasicBlock *bb = isSgBasicBlock(n->get_parent());
  ROSE_ASSERT(bb);
  SgStatementPtrList &stmts = bb->get_statements();
  for (SgStatementPtrList::size_type index = 0; index < stmts.size(); ++index) {
    if (stmts[index] == n) {
      return stmt_pos(&stmts, index, bb);
    }
  }
  ROSE_ABORT();
}

stmt_pos_list findFollowingPositions(SgInitializedName *in) {
  if (SgFunctionParameterList *pl =
          isSgFunctionParameterList(in->get_parent())) {
    stmt_pos_list poss;
    SgFunctionDeclaration *fnDecl = isSgFunctionDeclaration(pl->get_parent());
    ROSE_ASSERT(fnDecl);

    SgFunctionDeclaration *fnDefDecl =
        isSgFunctionDeclaration(fnDecl->get_definingDeclaration());
    if (fnDefDecl) {
      SgFunctionDefinition *fnDef = fnDefDecl->get_definition();
      SgBasicBlock *body = fnDef->get_body();

      SgStatementPtrList &bodyStmts = body->get_statements();
      poss.push_back(stmt_pos(&bodyStmts, 0, body));
    }
    return poss;
  } else {
    SgStatement *parent = isSgStatement(in->get_parent());
    ROSE_ASSERT(parent != NULL);
    return findFollowingPositions(parent);
  }
}

stmt_pos_list findFollowingPositions(SgStatement *stmt) {
  stmt_pos_list poss;
  SgNode *parent = stmt->get_parent();
  if (isSgBasicBlock(parent)) {
    stmt_pos pos = findPosition(stmt);
    ++pos.index;
    poss.push_back(pos);
  } else if (SgIfStmt *is = isSgIfStmt(parent)) {
    {
      SgStatementPtrList &ts = SageInterface::ensureBasicBlockAsTrueBodyOfIf(is)
                                   ->get_statements(),
                         &fs =
                             SageInterface::ensureBasicBlockAsFalseBodyOfIf(is)
                                 ->get_statements();
      poss.push_back(stmt_pos(&ts, 0, is->get_true_body()));
      if (is->get_false_body()) {
        poss.push_back(stmt_pos(&fs, 0, is->get_false_body()));
      }
    }
  } else if (SgWhileStmt *ws = isSgWhileStmt(parent)) {
    SgStatementPtrList &stmts =
        SageInterface::ensureBasicBlockAsBodyOfWhile(ws)->get_statements();
    poss.push_back(stmt_pos(&stmts, 0, ws->get_body()));
    stmt_pos outerPos = findPosition(ws);
    ++outerPos.index;
    poss.push_back(outerPos);
  } else if (SgForStatement *fs = isSgForStatement(parent)) {
    // we cannot check the inits here, as it would lead to a reference being
    // made to a place which is passed multiple times for a one-time
    // initializetion
    if (fs->get_test() == stmt) {
      SgStatementPtrList &stmts =
          SageInterface::ensureBasicBlockAsBodyOfFor(fs)->get_statements();
      poss.push_back(stmt_pos(&stmts, 0, fs->get_loop_body()));
      stmt_pos outerPos = findPosition(fs);
      ++outerPos.index;
      poss.push_back(outerPos);
    }
  }
  return poss;
}

SgStatement *findStatementForExpression(SgExpression *expr) {
  return findNearestParentOfType<SgStatement, isSgStatement>(expr).first;
}

pair<SgBasicBlock *, SgNode *> findBasicBlockForStmt(SgStatement *stmt) {
  return findNearestParentOfType<SgBasicBlock, isSgBasicBlock>(stmt);
}
