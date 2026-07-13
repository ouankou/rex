/*
 * The goal of this transformation is to remove all short circuiting, to aid in
 * analysis work.
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "analysisUtils.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <iostream>

#if ROSE_WITH_LIBHARU
#include "AstPDFGeneration.h"
#endif

#include "shortCircuitingTransformation.h"

using namespace std;

#define SgNULL_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

template <class Node> Node *buildSCGeneratedNode(Node *node) {
  ROSE_ASSERT(node != NULL);
  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  ROSE_ASSERT(locatedNode != NULL);
  SageInterface::ensureLocatedNodeFileInfoForTransformation(locatedNode);
  ROSE_ASSERT(locatedNode->get_startOfConstruct() != NULL);
  ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);
  ROSE_ASSERT(locatedNode->get_startOfConstruct()->get_parent() == locatedNode);
  ROSE_ASSERT(locatedNode->get_endOfConstruct()->get_parent() == locatedNode);
  ROSE_ASSERT(locatedNode->isTransformation());
  if (SgExpression *expression = isSgExpression(locatedNode)) {
    ROSE_ASSERT(expression->get_operatorPosition() != NULL);
    ROSE_ASSERT(expression->get_operatorPosition()->get_parent() == expression);
    ROSE_ASSERT(expression->get_operatorPosition()->isTransformation());
  }
  return node;
}

template <class Node>
Node *publishSCGeneratedSubtree(Node *node, SgLocatedNode *exactOwner) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(exactOwner != nullptr);
  SageInterface::publishGeneratedSubtreeOutputOwner(node, exactOwner);
  return node;
}

void MarkSCGenerated(SgNode *n) {
  n->setAttribute("SCGenerated", new AstAttribute);
}

bool IsSCGenerated(const SgNode *n) {
  return n->attributeExists("SCGenerated");
}

template <class Base> class SCPreservingCopy : public Base {
public:
  SCPreservingCopy(const Base &base = Base()) : Base(base) {}

  SgNode *copyAst(const SgNode *orig) {
    SgNode *copy = Base::copyAst(orig);
    if (IsSCGenerated(orig)) {
      MarkSCGenerated(copy);
    }
    return copy;
  }
};

/*
 * See implementation notes for details on the three different types of sc.
 */
inline bool isSCType1(SgNode *n) {
  return isSgConditionalExp(n) && !isSgValueExp(n->get_parent());
}

inline bool isSCType2(SgNode *n) { return isSgCommaOpExp(n); }

bool isSC(SgNode *n) {
  if (isSCType1(n)) {
    return !isSCType1(n->get_parent());
  } else if (isSCType2(n)) {
    Rose_STL_Container<SgNode *> exprs =
        NodeQuery::querySubTree(n, V_SgExpression);

    for (Rose_STL_Container<SgNode *>::iterator i = exprs.begin();
         i != exprs.end(); i++) {
      if (*i != n && isSCType1(*i)) {
        return true;
      }
    }
    return false;
  } else {
    return false;
  }
}

bool hasSC(SgNode *er) {
  Rose_STL_Container<SgNode *> exprs =
      NodeQuery::querySubTree(er, V_SgExpression);

  for (Rose_STL_Container<SgNode *>::iterator i = exprs.begin();
       i != exprs.end(); i++) {
    if (isSC(*i)) {
      return true;
    }
  }

  return false;
}

template <typename T>
SgConditionalExp *createEquivalentConditional(T *booleanOp);

template <>
inline SgConditionalExp *createEquivalentConditional(SgAndOp *andOp) {
  SgBoolValExp *zero = buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 0));
  return buildSCGeneratedNode(SageBuilder::buildConditionalExp_nfi(
      andOp->get_lhs_operand(), andOp->get_rhs_operand(), zero,
      andOp->get_type()));
}

template <> inline SgConditionalExp *createEquivalentConditional(SgOrOp *orOp) {
  SgBoolValExp *one = buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 1));
  return buildSCGeneratedNode(SageBuilder::buildConditionalExp_nfi(
      orOp->get_lhs_operand(), one, orOp->get_rhs_operand(), orOp->get_type()));
}

template <typename T, VariantT V_T, T *(isT)(SgNode *)>
inline void rewriteConditionals(SgNode *n) {
  Rose_STL_Container<SgNode *> booleanOps = NodeQuery::querySubTree(n, V_T);

  for (Rose_STL_Container<SgNode *>::iterator i = booleanOps.begin();
       i != booleanOps.end(); i++) {
    T *booleanOp = isT(*i);
    ROSE_ASSERT(booleanOp != NULL);
    if (isSgValueExp(booleanOp->get_parent())) {
      continue;
    }

    SgExpression *lhs = booleanOp->get_lhs_operand();
    SgExpression *rhs = booleanOp->get_rhs_operand();

    SgConditionalExp *condExp = createEquivalentConditional(booleanOp);

    SgExpression *booleanOpParent = isSgExpression(booleanOp->get_parent());
    // DQ (12/16/2006): Modified to reflect expressions attached directly to
    // statements (SgExpressionRoot IR nodes are no longer generated).
    condExp->set_parent(booleanOp->get_parent());
    lhs->set_parent(condExp);
    rhs->set_parent(condExp);
    publishSCGeneratedSubtree(condExp, booleanOp);

    // DQ (12/16/2006): Need to handle case where parent is not an expression!
    if (booleanOpParent != NULL)
      booleanOpParent->replace_expression(booleanOp, condExp);
    else {
      SgStatement *booleanOpParent = isSgStatement(booleanOp->get_parent());
      if (booleanOpParent != NULL) {
        booleanOpParent->replace_expression(booleanOp, condExp);
      } else {
        printf("Error: booleanOp->get_parent() = %p is unknown \n",
               booleanOp->get_parent());
        ROSE_ABORT();
      }
    }

    booleanOp->set_lhs_operand(
        0); // so they won't be deleted with the booleanOp
    booleanOp->set_rhs_operand(0);
    delete booleanOp;
  }
}

static SgStatement *replaceContinues(SgStatement *stmt,
                                     SgLabelStatement *dest) {
  if (isSgContinueStmt(stmt)) {
    SgGotoStatement *gotoStmt =
        buildSCGeneratedNode(new SgGotoStatement(SgNULL_FILE, dest));
    publishSCGeneratedSubtree(gotoStmt, stmt);
    delete stmt;
    return gotoStmt;
  } else {
    SageInterface::changeContinuesToGotos(stmt, dest);
    return stmt;
  }
}

// Split a variable declaration into two statements: the variable declaration
// itself and an expression statement setting the variable to the value
// specified in its initializer.  Sets the initializer to null and does not
// insert the new expression statement itself, as it may be used in a different
// way (e.g. condition of if statement). Caveat: for constructor initializers
// this is not the correct behavior (as 0-argument constructor called first) but
// the best we can do if we want the initializer in an expression statement.
SgExprStatement *splitVarDecl(SgVariableDeclaration *varDecl) {
  SgInitializedName *in = varDecl->get_variables().front();

  SgInitializer *init = in->get_initializer();

  SgExpression *operand = NULL;
  switch (init->variantT()) {
  case V_SgAssignInitializer: {
    SgAssignInitializer *assignInit = isSgAssignInitializer(init);
    ROSE_ASSERT(assignInit);

    operand = assignInit->get_operand_i();

    break;
  }

  case V_SgConstructorInitializer: {
    SgConstructorInitializer *ctorInit = isSgConstructorInitializer(init);
    ROSE_ASSERT(ctorInit != NULL);

    operand = ctorInit;
    break;
  }

  default: {
    cout << "splitVarDecl: unknown init type: " << init->class_name() << endl;
    ROSE_ABORT();
  }
  }

  in->set_initializer(NULL);

  SgVariableSymbol *var = in->get_scope()->lookup_var_symbol(in->get_name());

  SgVarRefExp *varRefForAssign =
      buildSCGeneratedNode(new SgVarRefExp(SgNULL_FILE, var));
  SgAssignOp *assignOp = buildSCGeneratedNode(new SgAssignOp(
      SgNULL_FILE, varRefForAssign, operand, varRefForAssign->get_type()));
  varRefForAssign->set_parent(assignOp);
  operand->set_parent(assignOp);

  SgExprStatement *condExprStmt =
      buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, assignOp));
  assignOp->set_parent(condExprStmt);

  return condExprStmt;
}

/*
 * Move condition in a loop that tests at the beginning of each iteration (for,
 * while) into the body.  Returns replacement condition
 */
SgStatement *moveConditionToBody(SgStatement *cond, SgBasicBlock *body) {
  ROSE_ASSERT(cond != nullptr);
  ROSE_ASSERT(body != nullptr);
  SgNode *const originalConditionalParent = cond->get_parent();
  SgStatement *newCond;

  SgBoolValExp *trueExp =
      buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 1));

  SgExprStatement *trueStmt =
      buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, trueExp));
  trueExp->set_parent(trueStmt);

  newCond = trueStmt;
  publishSCGeneratedSubtree(trueStmt, cond);
  trueStmt->set_parent(originalConditionalParent);

  SgBasicBlock *trueBody = buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));

  SgBreakStmt *breakStmt = buildSCGeneratedNode(new SgBreakStmt(SgNULL_FILE));
  SgBasicBlock *falseBody =
      buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, breakStmt));
  breakStmt->set_parent(falseBody);

  SgIfStmt *ifStmt = buildSCGeneratedNode(
      new SgIfStmt(SgNULL_FILE, NULL, trueBody, falseBody));
  trueBody->set_parent(ifStmt);
  falseBody->set_parent(ifStmt);

  switch (cond->variantT()) {
  case V_SgExprStatement: {
    ifStmt->set_conditional(cond);
    cond->set_parent(ifStmt);

    break;
  }

  case V_SgVariableDeclaration: {
    body->prepend_statement(cond);
    cond->set_parent(body);

    SgVariableDeclaration *varDecl = isSgVariableDeclaration(cond);
    SgExprStatement *varAssign = splitVarDecl(varDecl);
    ifStmt->set_conditional(varAssign);
    varAssign->set_parent(ifStmt);

    break;
  }

  default: {
    ROSE_ABORT();
  }
  }

  publishSCGeneratedSubtree(ifStmt, cond);
  body->prepend_statement(ifStmt);
  ifStmt->set_parent(body);
  MarkSCGenerated(ifStmt);
  return newCond;
}

void initialTransformation(SgNode *n) {
  rewriteConditionals<SgAndOp, V_SgAndOp, isSgAndOp>(n);
  rewriteConditionals<SgOrOp, V_SgOrOp, isSgOrOp>(n);

  Rose_STL_Container<SgNode *> ifStmts = NodeQuery::querySubTree(n, V_SgIfStmt);
  for (Rose_STL_Container<SgNode *>::iterator i = ifStmts.begin();
       i != ifStmts.end(); ++i) {
    SgIfStmt *ifStmt = isSgIfStmt(*i);
    ROSE_ASSERT(ifStmt != NULL);

    SgBasicBlock *ifStmtParent = isSgBasicBlock(ifStmt->get_parent());
    ROSE_ASSERT(ifStmtParent != NULL);

    SgStatementPtrList::iterator ifStmtI =
        findIterator(ifStmtParent->get_statements(), ifStmt);
    ROSE_ASSERT(ifStmtI != ifStmtParent->get_statements().end());
    unsigned int ifStmtPos = ifStmtI - ifStmtParent->get_statements().begin();

    SgStatement *cond = ifStmt->get_conditional();
    if (hasSC(cond)) {
      cout << "hasSC(cond)" << endl;
      SgExprStatement *condExprStmt = NULL;
      switch (cond->variantT()) {
      case V_SgExprStatement: {
        condExprStmt = isSgExprStatement(cond);
        ROSE_ASSERT(condExprStmt != NULL);

        break;
      }

      case V_SgVariableDeclaration: {
        SgVariableDeclaration *varDecl = isSgVariableDeclaration(cond);
        ROSE_ASSERT(varDecl != NULL);

        condExprStmt = splitVarDecl(varDecl);

        ifStmtParent->get_statements().insert(
            ifStmtParent->get_statements().begin() + ifStmtPos, varDecl);
        varDecl->set_parent(ifStmtParent);

        break;
      }

      default: {
        cout << "initialTransformation (if): unknown cond type "
             << cond->class_name() << endl;
        ROSE_ABORT();
      }
      }

      static int counter = 0;

      stringstream varNameSS;
      varNameSS << "rose_sc_bool_" << counter++;
      SgName varName = varNameSS.str();

      SgBoolValExp *falseExp =
          buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 0));

      SgAssignInitializer *falseAssign =
          buildSCGeneratedNode(new SgAssignInitializer(
              SgNULL_FILE, falseExp, SgTypeBool::createType()));
      falseExp->set_parent(falseAssign);

      SgVariableDeclaration *varDecl = SageBuilder::buildVariableDeclaration(
          varName, SgTypeBool::createType(), falseAssign, ifStmtParent);
      ROSE_ASSERT(varDecl != NULL);
      ROSE_ASSERT(varDecl->get_variables().size() == 1);
      SgInitializedName *in = varDecl->get_variables().front();
      ROSE_ASSERT(in != NULL);
      ROSE_ASSERT(in->get_parent() == varDecl);
      ROSE_ASSERT(in->get_scope() == ifStmtParent);
      ROSE_ASSERT(in->get_declptr() != NULL);
      SgVariableSymbol *varSym =
          isSgVariableSymbol(in->get_symbol_from_symbol_table());
      ROSE_ASSERT(varSym != NULL);

      publishSCGeneratedSubtree(varDecl, ifStmt);
      ifStmtParent->get_statements().insert(
          ifStmtParent->get_statements().begin() + ifStmtPos, varDecl);
      varDecl->set_parent(ifStmtParent);

      SgVarRefExp *varRef =
          buildSCGeneratedNode(new SgVarRefExp(SgNULL_FILE, varSym));
      SgBoolValExp *trueExp =
          buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 1));

      SgAssignOp *varAssignOp = buildSCGeneratedNode(
          new SgAssignOp(SgNULL_FILE, varRef, trueExp, varRef->get_type()));
      varRef->set_parent(varAssignOp);
      trueExp->set_parent(varAssignOp);

      SgExprStatement *varAssignStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, varAssignOp));
      varAssignOp->set_parent(varAssignStmt);

      SgBasicBlock *trueBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, varAssignStmt));
      varAssignStmt->set_parent(trueBody);
      SgBasicBlock *falseBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));

      SgIfStmt *newIfStmt = buildSCGeneratedNode(
          new SgIfStmt(SgNULL_FILE, condExprStmt, trueBody, falseBody));
      condExprStmt->set_parent(newIfStmt);
      trueBody->set_parent(newIfStmt);
      falseBody->set_parent(newIfStmt);

      varRef = buildSCGeneratedNode(new SgVarRefExp(SgNULL_FILE, varSym));

      SgExprStatement *exprStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, varRef));
      varRef->set_parent(exprStmt);

      publishSCGeneratedSubtree(exprStmt, ifStmt);
      ifStmt->set_conditional(exprStmt);
      exprStmt->set_parent(ifStmt);

      publishSCGeneratedSubtree(newIfStmt, ifStmt);
      ifStmtParent->get_statements().insert(
          ifStmtParent->get_statements().begin() + ifStmtPos, newIfStmt);
      newIfStmt->set_parent(ifStmtParent);
    }
  }

  Rose_STL_Container<SgNode *> whileStmts =
      NodeQuery::querySubTree(n, V_SgWhileStmt);
  for (Rose_STL_Container<SgNode *>::iterator i = whileStmts.begin();
       i != whileStmts.end(); ++i) {
    SgWhileStmt *whileStmt = isSgWhileStmt(*i);
    ROSE_ASSERT(whileStmt != NULL);

    SgStatement *cond = whileStmt->get_condition();
    if (hasSC(cond)) {
      whileStmt->set_condition(moveConditionToBody(
          whileStmt->get_condition(),
          SageInterface::ensureBasicBlockAsBodyOfWhile(whileStmt)));
    }
  }

  Rose_STL_Container<SgNode *> dowhileStmts =
      NodeQuery::querySubTree(n, V_SgDoWhileStmt);
  for (Rose_STL_Container<SgNode *>::iterator i = dowhileStmts.begin();
       i != dowhileStmts.end(); ++i) {
    SgDoWhileStmt *dowhileStmt = isSgDoWhileStmt(*i);
    ROSE_ASSERT(dowhileStmt != NULL);

    SgStatement *cond = dowhileStmt->get_condition();
    if (hasSC(cond)) {
      SgBoolValExp *trueExp =
          buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 1));
      SgExprStatement *trueStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, trueExp));
      trueExp->set_parent(trueStmt);

      publishSCGeneratedSubtree(trueStmt, dowhileStmt);
      dowhileStmt->set_condition(trueStmt);
      trueStmt->set_parent(dowhileStmt);

      static int counter = 0;

      stringstream labelNameSS;
      labelNameSS << "rose_sc_label_" << counter++;
      SgName labelName = labelNameSS.str();

      SgNullStatement *labeledNullStatement =
          buildSCGeneratedNode(new SgNullStatement(SgNULL_FILE));
      SgLabelStatement *labelStmt = buildSCGeneratedNode(
          new SgLabelStatement(SgNULL_FILE, labelName, labeledNullStatement));
      labeledNullStatement->set_parent(labelStmt);
      publishSCGeneratedSubtree(labelStmt, dowhileStmt);
      SageInterface::ensureBasicBlockAsBodyOfDoWhile(dowhileStmt)
          ->append_statement(labelStmt);
      labelStmt->set_parent(dowhileStmt->get_body());

      dowhileStmt->set_body(
          replaceContinues(dowhileStmt->get_body(), labelStmt));
      dowhileStmt->get_body()->set_parent(dowhileStmt);

      SgBasicBlock *trueBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));

      SgBreakStmt *breakStmt =
          buildSCGeneratedNode(new SgBreakStmt(SgNULL_FILE));
      SgBasicBlock *falseBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, breakStmt));
      breakStmt->set_parent(falseBody);

      SgIfStmt *ifStmt = buildSCGeneratedNode(
          new SgIfStmt(SgNULL_FILE, NULL, trueBody, falseBody));
      trueBody->set_parent(ifStmt);
      falseBody->set_parent(ifStmt);
      ifStmt->set_conditional(cond);
      cond->set_parent(ifStmt);
      publishSCGeneratedSubtree(ifStmt, dowhileStmt);
      SageInterface::ensureBasicBlockAsBodyOfDoWhile(dowhileStmt)
          ->append_statement(ifStmt);
      ifStmt->set_parent(dowhileStmt->get_body());
    }
  }

  Rose_STL_Container<SgNode *> forStmts =
      NodeQuery::querySubTree(n, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::iterator i = forStmts.begin();
       i != forStmts.end(); ++i) {
    SgForStatement *forStmt = isSgForStatement(*i);
    ROSE_ASSERT(forStmt != NULL);

    SgForInitStatement *forInit = forStmt->get_for_init_stmt();
    if (hasSC(forInit)) {
      cerr << "hasSC(forInit))" << endl;
      // If any of the init statements have an sc, we need to
      // transform all of them anyway, to enforce correct
      // execution order
      SgNode *forStmtParent = forStmt->get_parent();
      SgBasicBlock *forStmtParentBB = isSgBasicBlock(forStmtParent);
      ROSE_ASSERT(forStmtParentBB != NULL);

      SgStatementPtrList &forParentStmts = forStmtParentBB->get_statements();
      SgStatementPtrList::iterator forParentStmtsIter =
          findIterator(forParentStmts, forStmt);

      SgStatementPtrList &initStmts = forInit->get_init_stmt();
      for (SgStatementPtrList::iterator i = initStmts.begin();
           i != initStmts.end(); ++i) {
        SgStatement *initStmt = *i;
        forStmtParentBB->get_statements().insert(forParentStmtsIter, *i);
        initStmt->set_parent(forStmtParentBB);
      }

      initStmts.clear();
    }

    SgStatement *forTest = forStmt->get_test();
    if (hasSC(forTest)) {
      forStmt->set_test(moveConditionToBody(
          forTest, SageInterface::ensureBasicBlockAsBodyOfFor(forStmt)));
    }

    // DQ (11/7/2006): modified to reflect removal of SgExpressionRoot IR node
    // SgExpressionRoot *forInc = forStmt->get_increment_expr_root();
    SgExpression *forInc = forStmt->get_increment();
    if (hasSC(forInc)) {
      SgExprStatement *forIncStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, forInc));
      forInc->set_parent(forIncStmt);

      SgBasicBlock *forBody =
          SageInterface::ensureBasicBlockAsBodyOfFor(forStmt);
      publishSCGeneratedSubtree(forIncStmt, forStmt);
      forBody->append_statement(forIncStmt);
      forIncStmt->set_parent(forBody);

      SgNullExpression *blankIncExpr =
          buildSCGeneratedNode(new SgNullExpression(SgNULL_FILE));
      blankIncExpr->set_role(
          SgNullExpression::e_null_expression_syntactic_absence);
      publishSCGeneratedSubtree(blankIncExpr, forStmt);

      // DQ (11/7/2006): modified to reflect removal of SgExpressionRoot IR node
      // SgExpressionRoot *blankIncRoot = new SgExpressionRoot(SgNULL_FILE,
      // blankIncExpr); blankIncExpr->set_parent(blankIncRoot);

      // forStmt->set_increment_expr_root(blankIncRoot);
      // blankIncRoot->set_parent(forStmt);
      forStmt->set_increment(blankIncExpr);
      blankIncExpr->set_parent(forStmt);
    }
  }

  Rose_STL_Container<SgNode *> varDeclStmts =
      NodeQuery::querySubTree(n, V_SgVariableDeclaration);
  for (Rose_STL_Container<SgNode *>::iterator i = varDeclStmts.begin();
       i != varDeclStmts.end(); ++i) {
    SgVariableDeclaration *varDecl = isSgVariableDeclaration(*i);
    ROSE_ASSERT(varDecl != NULL);

    cout << "in varDecl loop" << varDecl << endl;
    if (hasSC(varDecl)) {
      cout << "doing varDecl" << endl;
      SgBasicBlock *varDeclParent = isSgBasicBlock(varDecl->get_parent());
      ROSE_ASSERT(varDeclParent);

      SgExprStatement *varAssign = splitVarDecl(varDecl);
      publishSCGeneratedSubtree(varAssign, varDecl);
      varDeclParent->get_statements().insert(
          ++findIterator(varDeclParent->get_statements(), varDecl), varAssign);
      varAssign->set_parent(varDeclParent);
    }
  }
}

// Find applicable SCs more efficiently, by only searching the relevant parts of
// the tree.

static void doFindApplicableSCs(SgNode *node,
                                Rose_STL_Container<SgExpression *> &appSCs) {
  vector<SgNode *> subnodes = node->get_traversalSuccessorContainer();
  for (vector<SgNode *>::iterator i = subnodes.begin(); i != subnodes.end();
       ++i) {
    if (*i == NULL) {
      continue;
    }
    SgExpression *expr = isSgExpression(*i);
    if (expr != NULL && isSC(expr)) {
      appSCs.push_back(expr);
    } else {
      doFindApplicableSCs(*i, appSCs);
    }
  }
}

Rose_STL_Container<SgExpression *> findApplicableSCs(SgNode *node) {
  SgExpressionPtrList scList;
  doFindApplicableSCs(node, scList);
  return scList;
}

// Perform an optimization where if (constant) { ... } else { ... } is replaced
// with { ... } This function takes what might be the root expression of an if
// statement, and only does the optimization if this is the case.  The function
// is designed this way since it is passed the expression that is the result of
// reducing a conditional expression (which may appear anywhere).
void ifConstOptimization(SgExpression *expr) {
  if (SgBoolValExp *boolExpr = isSgBoolValExp(expr)) {
    if (SgExpression *boolExprRoot =
            boolExpr) // JJW removed use of SgExpressionRoot here
    {
      if (SgExprStatement *boolExprStmt =
              isSgExprStatement(boolExprRoot->get_parent())) {
        if (SgIfStmt *constIfStmt = isSgIfStmt(boolExprStmt->get_parent())) {
          SgBasicBlock *constIfStmtParent =
              isSgBasicBlock(constIfStmt->get_parent());
          ROSE_ASSERT(constIfStmtParent != NULL);

          SgStatement *replacementStmt = constIfStmt,
                      *replacementStmtParent = constIfStmtParent;

          cout << "replacementStmt is now " << replacementStmt << " "
               << replacementStmt->class_name() << endl;
          cout << "replacementStmtParent is now " << replacementStmtParent
               << " " << replacementStmtParent->class_name() << endl;
          while (
              isSgBasicBlock(replacementStmtParent) &&
              isSgBasicBlock(replacementStmtParent)->get_statements().size() ==
                  1 &&
              isSgStatement(replacementStmtParent->get_parent())) {
            cout << "replacementStmt was " << replacementStmt << " "
                 << replacementStmt->class_name() << endl;
            replacementStmt = replacementStmtParent;
            cout << "replacementStmt is now " << replacementStmt << " "
                 << replacementStmt->class_name() << endl;
            cout << "replacementStmtParent was " << replacementStmtParent << " "
                 << replacementStmtParent->class_name() << endl;
            replacementStmtParent =
                isSgStatement(replacementStmtParent->get_parent());
            cout << "replacementStmtParent is now " << replacementStmtParent
                 << " " << replacementStmtParent->class_name() << endl;
          }

          if (boolExpr->get_value() != 0) {
            // true branch
            publishSCGeneratedSubtree(constIfStmt->get_true_body(),
                                      constIfStmt);
            replacementStmtParent->replace_statement(
                replacementStmt, constIfStmt->get_true_body());
            delete constIfStmt->get_false_body();
          } else {
            // false branch
            SgStatement *falseBranch = constIfStmt->get_false_body();
            if (!falseBranch)
              falseBranch = SageBuilder::buildBasicBlock();
            publishSCGeneratedSubtree(falseBranch, constIfStmt);
            replacementStmtParent->replace_statement(replacementStmt,
                                                     falseBranch);
            delete constIfStmt->get_true_body();
          }
        }
      }
    }
  }
}

bool reduceIfStmtsWithSCchild(SgProject *prj) {
  bool retVal = false;
  Rose_STL_Container<SgExpression *> appSCs = findApplicableSCs(prj);
  static int callCount = 0;
  callCount++;

  for (Rose_STL_Container<SgExpression *>::iterator i = appSCs.begin();
       i != appSCs.end(); ++i) {
    SgExpression *expr = *i;
    cout << "reduceIfStmtsWithSCchild: doing " << expr << " iter " << callCount
         << endl;
    SgStatement *stmt = findStatementForExpression(expr);
    pair<SgBasicBlock *, SgNode *> bbPair = findBasicBlockForStmt(stmt);
    SgBasicBlock *basicBlock = bbPair.first;
    SgStatement *basicBlockChild = isSgStatement(bbPair.second);
    ROSE_ASSERT(basicBlockChild != NULL);

    retVal = true;

    SgStatement *fullStmt;

    switch (basicBlockChild->variantT()) {
    case V_SgExprStatement: {
      if (IsSCGenerated(basicBlockChild)) {
        fullStmt = basicBlock;
      } else {
        fullStmt = basicBlockChild;
      }
      break;
    }

    case V_SgIfStmt:
    case V_SgReturnStmt: {
      fullStmt = basicBlockChild;
      break;
    }

    default: {

#if ROSE_WITH_LIBHARU
      AstPDFGeneration().generate("error", prj);
#else
      cout << "Warning: libharu support is not enabled" << endl;
#endif
      cerr << "Error: reduceIfStmtsWithSCchild: basicBlockChild has unknown "
              "type "
           << basicBlockChild->class_name() << basicBlockChild << endl;
      ROSE_ABORT();
    }
    }

    switch (expr->variantT()) {
    case V_SgCommaOpExp: {
      SgCommaOpExp *commaOpExp = isSgCommaOpExp(expr);
      ROSE_ASSERT(commaOpExp != NULL);

      SgExpression *dummyExpr = commaOpExp->get_lhs_operand();

      SgExprStatement *dummyExprStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, dummyExpr));
      dummyExpr->set_parent(dummyExprStmt);

      MarkSCGenerated(dummyExprStmt);

      SgExpression *commaOpExpParent = isSgExpression(commaOpExp->get_parent());
      ROSE_ASSERT(commaOpExpParent != NULL);

      SgExpression *rhsExpr = commaOpExp->get_rhs_operand();
      commaOpExpParent->replace_expression(commaOpExp, rhsExpr);
      rhsExpr->set_parent(commaOpExpParent);

      commaOpExp->set_lhs_operand(NULL);
      commaOpExp->set_rhs_operand(NULL);
      delete commaOpExp;

      SgBasicBlock *newBB =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, dummyExprStmt));
      dummyExprStmt->set_parent(newBB);

      publishSCGeneratedSubtree(newBB, fullStmt);
      newBB->set_parent(fullStmt->get_parent());
      isSgStatement(fullStmt->get_parent())->replace_statement(fullStmt, newBB);
      newBB->append_statement(fullStmt);
      fullStmt->set_parent(newBB);

      break;
    }

    case V_SgConditionalExp: {
      SgConditionalExp *condExp = isSgConditionalExp(expr);
      ROSE_ASSERT(condExp != NULL);
      condExp->validate();
      if (condExp->get_operator_kind() !=
          SgConditionalExp::e_conditional_operator_standard) {
        fprintf(stderr,
                "REX_AST_INVARIANT[short-circuit-gnu-conditional]: the "
                "short-circuiting transformation requires an explicit "
                "evaluation-once lowering for GNU binary conditionals\n");
        ROSE_ABORT();
      }

      SgStatement *fullStmtParent = isSgStatement(fullStmt->get_parent());
      ROSE_ASSERT(fullStmtParent != NULL);

      // Deduce a suitable block for attaching to the if statement
      SgBasicBlock *fullStmtBlock;
      if (SgBasicBlock *fullStmtBB = isSgBasicBlock(fullStmt)) {
        fullStmtBlock = fullStmtBB;
      } else {
        fullStmtBlock =
            buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, fullStmt));
        // fullStmt->set_parent(fullStmtBlock);
      }

      // Build the true branch of the if statement into fullStmtBlockCopy
      // where the conditional is replaced with its true clause
      SgExpression *trueExp = condExp->get_true_exp();

      SCPreservingCopy<SgCapturingCopy<SgTreeCopy>> ctc(
          vector<SgNode *>(1, condExp));
      SgNode *fullStmtBlockCopyNode = fullStmtBlock->copy(ctc);
      SgBasicBlock *fullStmtBlockCopy = isSgBasicBlock(fullStmtBlockCopyNode);
      ROSE_ASSERT(fullStmtBlockCopy != NULL);

      SgNode *condCopyNode = ctc.get_copyList()[0];
      SgExpression *condCopy = isSgExpression(condCopyNode);
      ROSE_ASSERT(condCopy != NULL);

      SgNode *condCopyParentNode = condCopy->get_parent();
      SgExpression *condCopyParent = isSgExpression(condCopyParentNode);

      // DQ (12/16/2006): Need to handle separate case of where the parent is a
      // SgExpression or a SgStatement (might could be a SgInitializedName,
      // which is not handled yet). ROSE_ASSERT(condCopyParent != NULL);
      // condCopyParent->replace_expression(condCopy, trueExp);
      // delete condCopy;
      // trueExp->set_parent(condCopyParent);
      if (condCopyParent != NULL) {
        condCopyParent->replace_expression(condCopy, trueExp);
        delete condCopy;
        trueExp->set_parent(condCopyParent);
      } else {
        SgStatement *condCopyParent = isSgStatement(condCopyParentNode);
        if (condCopyParent != NULL) {
          condCopyParent->replace_expression(condCopy, trueExp);
          delete condCopy;
          trueExp->set_parent(condCopyParent);
        } else {
          printf("What is this condCopyParentNode = %p \n", condCopyParentNode);
          ROSE_ABORT();
        }
      }

#ifdef SCDBG
      static int optCounter = 0;

      optCounter++;
      cout << "optCounter = " << optCounter << endl;

      stringstream beforeNameSS;
      beforeNameSS << "beforeIfConst" << optCounter;
#if ROSE_WITH_LIBHARU
      AstPDFGeneration().generate(beforeNameSS.str(), prj);
#else
      cout << "Warning: libharu support is not enabled" << endl;
#endif
#endif

      ifConstOptimization(trueExp);
#ifdef SCDBG
      stringstream afterNameSS;
      afterNameSS << "afterIfConst" << optCounter;
#if ROSE_WITH_LIBHARU
      AstPDFGeneration().generate(afterNameSS.str(), prj);
#else
      cout << "Warning: libharu support is not enabled" << endl;
#endif
#endif

      // Build the false branch of the if statement into fullStmtBlock
      // (reusing the current tree)
      // where the conditional is replaced with its false clause
      SgExpression *falseExp = condExp->get_false_exp();

      SgNode *condParent = condExp->get_parent();
      SgExpression *condParentExp = isSgExpression(condParent);

      // DQ (12/16/2006): Need to handle separate case of where the parent is a
      // SgExpression or a SgStatement (might could be a SgInitializedName,
      // which is not handled yet). ROSE_ASSERT(condParentExp != NULL);
      // condParentExp->replace_expression(condExp, falseExp);

      if (condParentExp != NULL) {
        condParentExp->replace_expression(condExp, falseExp);
      } else {
        SgStatement *condParentStmt = isSgStatement(condParent);
        if (condParentStmt != NULL) {
          condParentStmt->replace_expression(condExp, falseExp);
        } else {
          printf("What is this condParent = %p \n", condParent);
          ROSE_ABORT();
        }
      }

#ifdef SCDBG
      optCounter++;
      cout << "optCounter = " << optCounter << endl;

      stringstream beforeNameSS2;
      beforeNameSS2 << "beforeIfConst" << optCounter;
#if ROSE_WITH_LIBHARU
      AstPDFGeneration().generate(beforeNameSS2.str(), prj);
#else
      cout << "Warning: libharu support is not enabled" << endl;
#endif
#endif
      ifConstOptimization(falseExp);
#ifdef SCDBG
      stringstream afterNameSS2;
      afterNameSS2 << "afterIfConst" << optCounter;
#if ROSE_WITH_LIBHARU
      AstPDFGeneration().generate(afterNameSS2.str(), prj);
#else
      cout << "Warning: libharu support is not enabled" << endl;
#endif
#endif

      // done creating branches, now create if statement

      SgExpression *conditionExp = condExp->get_conditional_exp();

      SgStatement *replacementStmt;
      // optimisation in case the condition is true
      if (SgBoolValExp *conditionBoolVal = isSgBoolValExp(conditionExp)) {
        if (conditionBoolVal->get_value() != 0) {
          // true branch
          replacementStmt = fullStmtBlockCopy;
          delete fullStmtBlock;
        } else {
          // false branch
          replacementStmt = fullStmtBlock;
          delete fullStmtBlockCopy;
        }
      } else {
        SgExprStatement *conditionExpStmt = buildSCGeneratedNode(
            new SgExprStatement(SgNULL_FILE, conditionExp));
        conditionExp->set_parent(conditionExpStmt);

        SgIfStmt *ifStmt = buildSCGeneratedNode(new SgIfStmt(
            SgNULL_FILE, conditionExpStmt, fullStmtBlockCopy, fullStmtBlock));
        conditionExpStmt->set_parent(ifStmt);
        fullStmtBlockCopy->set_parent(ifStmt);
        fullStmtBlock->set_parent(ifStmt);
        MarkSCGenerated(ifStmt);

        // if the parent statement is an if statement, then it must have an
        // SgBasicBlock which contains the if statement just created as a child
        if (isSgIfStmt(fullStmtParent)) {
          replacementStmt =
              buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, ifStmt));
          ifStmt->set_parent(replacementStmt);
        } else {
          replacementStmt = ifStmt;
        }
      }

      // XXX : this is a kludge
      SgNode *fullStmtParentTmp = fullStmt->get_parent();
      fullStmt->set_parent(fullStmtParent);
      publishSCGeneratedSubtree(replacementStmt, fullStmt);
      fullStmtParent->replace_statement(fullStmt, replacementStmt);
      fullStmt->set_parent(fullStmtParentTmp);

      replacementStmt->set_parent(fullStmtParent);

      break;
    }

    default: {
      puts("whoops");
      ROSE_ABORT();
    }
    }
  }
  return retVal;
}

void shortCircuitingTransformation(SgProject *prj) {
  initialTransformation(prj);

#ifdef SCDBG
  int pass = 0;
  do {
    stringstream ss;
    ss << "scdbg_p" << pass++;
#if ROSE_WITH_LIBHARU
    AstPDFGeneration().generate(ss.str(), prj);
#else
    cout << "Warning: libharu support is not enabled" << endl;
#endif
    AstTests::runAllTests(prj);
    ss << ".C";
    ofstream f(ss.str().c_str());
    f << prj->unparseToCompleteString();
  } while (reduceIfStmtsWithSCchild(prj));
#endif
}

#ifdef SCDBG
int main(int argc, char **argv) {
  SgProject *prj = frontend(argc, argv);
  shortCircuitingTransformation(prj);

  AstTests::runAllTests(prj);

  // return backend(prj);
  prj->unparse();
}
#endif
