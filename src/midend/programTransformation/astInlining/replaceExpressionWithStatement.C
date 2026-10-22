#include "sage3basic.h"

#include "astPostProcessing.h"

#include "sageBuilder.h"

#include "rose_config.h"

#include "inlinerSupport.h"

#include "replaceExpressionWithStatement.h"

using namespace SageInterface;
SgExpression *getRootOfExpression(SgExpression *n) {
  ROSE_ASSERT(n);

  SgNode *parent = n->get_parent();
  while (isSgExpression(parent) != NULL) {
    n = isSgExpression(parent);
    parent = n->get_parent();
  }

  return n;
}

SgStatement *getStatementOfExpression(SgNode *n) {
  SgExpression *expression = isSgExpression(n);
  ROSE_ASSERT(expression != NULL);
  SgExpression *rootExpression = getRootOfExpression(expression);
  ROSE_ASSERT(rootExpression->get_parent() != NULL);
  SgInitializedName *initializedName =
      isSgInitializedName(rootExpression->get_parent());

  SgNode *root = rootExpression;
  if (initializedName != NULL)
    root = initializedName;

  SgStatement *statement = isSgStatement(root->get_parent());
  ROSE_ASSERT(statement != NULL);
  return statement;
}

// Insert a new statement before or after a target statement.  If
// allowForInit is true, the new statement can be inserted into the
// initializer of a for statement.
void myStatementInsert(SgStatement *target, SgStatement *newstmt, bool before,
                       bool allowForInit) {
  ROSE_ASSERT(target != NULL);
  ROSE_ASSERT(newstmt != NULL);
  SgStatement *parent = isSgStatement(target->get_parent());
  if (isSgIfStmt(parent) && isSgIfStmt(parent)->get_conditional() == target) {
    target = parent;
    parent = isSgScopeStatement(target->get_parent());
  }

  if (auto *forInit = isSgForInitStatement(target->get_parent());
      forInit && !allowForInit) {
    SgForStatement *forLoop = isSgForStatement(forInit->get_parent());
    ROSE_ASSERT(forLoop);
    target = forLoop;
    parent = isSgScopeStatement(forLoop->get_parent());
  }

  if (auto *switchStmt = isSgSwitchStatement(target->get_parent());
      switchStmt && target == switchStmt->get_item_selector()) {
    target = switchStmt;
    parent = isSgScopeStatement(switchStmt->get_parent());
  }

  ROSE_ASSERT(parent);
  // The generic mutation boundary owns source publication, scope fixup, and
  // the exact container insertion.  Directly editing the sibling vector left
  // transformation nodes attached with the detached physical-file sentinel.
  SageInterface::insertStatement(target, newstmt, before,
                                 /*autoMovePreprocessingInfo=*/false);
}

// Replace the expression "from" with another expression "to", wherever it
// appears in the AST.  The expression "from" is not deleted, and may be
// reused elsewhere in the AST.
void replaceExpressionWithExpression(SgExpression *from, SgExpression *to) {
  SageInterface::replaceExpression(from, to, true);
}

// Convert something like "int a = foo();" into "int a; a = foo();"
SgAssignOp *convertInitializerIntoAssignment(SgAssignInitializer *init) {
  using namespace SageBuilder;
  ROSE_ASSERT(init);
  SgStatement *stmt = getStatementOfExpression(init);
  SgNode *statementOwner = stmt->get_parent();
  SgScopeStatement *parent = isSgScopeStatement(statementOwner);
  if (auto *forInit = isSgForInitStatement(statementOwner)) {
    SgForStatement *forLoop = isSgForStatement(forInit->get_parent());
    ROSE_ASSERT(forLoop);
    parent = isSgScopeStatement(forLoop->get_parent());
  }
  ROSE_ASSERT(parent);
  SgNode *initparent = init->get_parent();
  ROSE_ASSERT(initparent);

  SgInitializedName *initname = NULL;
  if (isSgInitializedName(initparent))
    initname = isSgInitializedName(initparent);
  else if (isSgVariableDefinition(initparent))
    initname = isSgVariableDefinition(initparent)->get_vardefn();
  else if (isSgVariableDeclaration(initparent)) {
    SgInitializedNamePtrList &vars =
        isSgVariableDeclaration(initparent)->get_variables();
    for (SgInitializedNamePtrList::iterator i = vars.begin(); i != vars.end();
         ++i) {
      if ((*i)->get_initializer() == init) {
        initname = *i;
        break;
      }
    }
  } else {
    fprintf(stderr,
            "REX_AST_INVARIANT[initializer-owner]: owner=%p/%s is "
            "not an initialized name or variable declaration\n",
            static_cast<void *>(initparent), initparent->class_name().c_str());
    ROSE_ABORT();
  }

  ROSE_ASSERT(initname);
  ROSE_ASSERT(initname->get_initializer() == init);
  SgSymbol *sym = initname->get_symbol_from_symbol_table();
  ROSE_ASSERT(isSgVariableSymbol(sym));
  SgVarRefExp *vr = buildVarRefExp(isSgVariableSymbol(sym));
  vr->set_lvalue(true);
  SgExpression *operand = init->release_operand();

  initname->set_initializer(NULL);
  delete init;
  SgExprStatement *assign_stmt = buildAssignStatement(vr, operand);

  myStatementInsert(stmt, assign_stmt, false);

  return isSgAssignOp(assign_stmt->get_expression());
}

void pushTestIntoBody(SgScopeStatement *loopStmt) {
  using namespace SageBuilder;
  ROSE_ASSERT(loopStmt != nullptr);
  SgStatement *cond = SageInterface::getLoopCondition(loopStmt);
  ROSE_ASSERT(isSgExprStatement(cond));
  SgExpression *root = isSgExprStatement(cond)->get_expression();
  if (root == nullptr || root->get_parent() != cond) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[loop-condition-root]: condition=%p root=%p "
            "parent=%p is not one exact expression-statement child\n",
            static_cast<void *>(cond), static_cast<void *>(root),
            root != nullptr ? static_cast<void *>(root->get_parent())
                            : nullptr);
    ROSE_ABORT();
  }
  SgType *condition_type = SageInterface::getBoolType(loopStmt);
  SgStatement *old_body = SageInterface::getLoopBody(loopStmt);
  SgBasicBlock *new_body = buildBasicBlock();
  SageInterface::setLoopBody(loopStmt, new_body);
  SageInterface::setLoopCondition(loopStmt,
                                  buildExprStatement(buildBoolValExp(true)));
  isSgExprStatement(cond)->set_expression(nullptr);
  root->set_parent(nullptr);
  SageInterface::deepDelete(cond);
  SgCastExp *cast = buildCastExp(root, condition_type);
  SgVariableDeclaration *new_decl = buildVariableDeclaration(
      SageInterface::generateUniqueVariableName(new_body, "rose_inline_test_"),
      condition_type, buildAssignInitializer(cast, condition_type), new_body);
  SgVariableSymbol *varsym = SageInterface::getFirstVarSym(new_decl);
  SageInterface::appendStatement(new_decl, new_body);
  SgIfStmt *loop_break = buildIfStmt(
      buildExprStatement(buildNotOp(buildVarRefExp(varsym), condition_type)),
      buildBasicBlock(buildBreakStmt()), buildBasicBlock());
  SageInterface::appendStatement(loop_break, new_body);
  if (isSgDoWhileStmt(loopStmt)) {
    SgFunctionDefinition *procedure =
        SageInterface::getEnclosingProcedure(loopStmt);
    ROSE_ASSERT(procedure);
    SgName label = SageInterface::generateUniqueVariableName(
        procedure, "rose_inline_test_label_");
    SgLabelStatement *ls =
        buildLabelStatement(label, buildBasicBlock(), procedure);
    SageInterface::changeContinuesToGotos(old_body, ls);
    // Note that these two insertions are backwards of the resulting statement
    // order
    SageInterface::prependStatement(ls, new_body);
    SageInterface::prependStatement(old_body, new_body);
  } else {
    SageInterface::appendStatement(old_body, new_body);
  }
  AstPostProcessing(loopStmt);
}
