/*
 * The goal of this transformation is to remove all short circuiting, to aid in
 * analysis work.
 */

#include "analysisUtils.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>
#include <iostream>
#include <set>

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

static void requireSplittableDeclarations(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (auto *owner = isSgIfStmt(declaration->get_parent());
        owner != nullptr && owner->get_conditional() == declaration) {
      continue;
    }
    const SgInitializedNamePtrList &variables = declaration->get_variables();
    if (variables.size() == 1 && variables.front() != nullptr &&
        isSgAssignInitializer(variables.front()->get_initializer()) !=
            nullptr) {
      continue;
    }

    bool needs_split = hasSC(declaration);
    if (!needs_split) {
      for (SgNode *expression :
           NodeQuery::querySubTree(declaration, V_SgExpression)) {
        if ((isSgAndOp(expression) != nullptr ||
             isSgOrOp(expression) != nullptr) &&
            isSgValueExp(expression->get_parent()) == nullptr) {
          needs_split = true;
          break;
        }
      }
    }
    if (needs_split) {
      fprintf(stderr,
              "REX_AST_INVARIANT[short-circuit-initializer]: declaration=%p "
              "requires an unsupported initialization split\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
  }
}

SgConditionalExp *createEquivalentConditional(SgBinaryOp *booleanOp) {
  const bool conjunction = isSgAndOp(booleanOp) != nullptr;
  ROSE_ASSERT(conjunction || isSgOrOp(booleanOp) != nullptr);
  auto booleanValue = [](bool value) {
    return buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, value));
  };
  // Preserve contextual conversion when either branch replaces the operator;
  // the operand's implicit cast alone is not emitted as a source conversion.
  SgExpression *right =
      buildSCGeneratedNode(SageBuilder::buildConditionalExp_nfi(
          SageInterface::copyExpression(booleanOp->get_rhs_operand()),
          booleanValue(true), booleanValue(false), booleanOp->get_type()));
  SgExpression *shortCircuitValue = booleanValue(!conjunction);
  return buildSCGeneratedNode(SageBuilder::buildConditionalExp_nfi(
      SageInterface::copyExpression(booleanOp->get_lhs_operand()),
      conjunction ? right : shortCircuitValue,
      conjunction ? shortCircuitValue : right, booleanOp->get_type()));
}

template <typename T, VariantT V_T, T *(isT)(SgNode *)>
inline void rewriteConditionals(SgNode *n) {
  Rose_STL_Container<SgNode *> booleanOps = NodeQuery::querySubTree(n, V_T);

  // Replacing a parent retires its subtree, so visit nested operators first.
  for (auto i = booleanOps.rbegin(); i != booleanOps.rend(); ++i) {
    T *booleanOp = isT(*i);
    ROSE_ASSERT(booleanOp != NULL);
    if (isSgValueExp(booleanOp->get_parent())) {
      continue;
    }

    SgConditionalExp *condExp = createEquivalentConditional(booleanOp);
    publishSCGeneratedSubtree(condExp, booleanOp);
    SageInterface::replaceExpression(booleanOp, condExp);
  }
}

// Split an assignment-initialized declaration into a declaration and an
// expression statement. The caller inserts the new statement.
SgExprStatement *splitVarDecl(SgVariableDeclaration *varDecl) {
  ROSE_ASSERT(varDecl->get_variables().size() == 1);
  SgInitializedName *in = varDecl->get_variables().front();
  ROSE_ASSERT(in != nullptr);
  SgInitializer *init = in->get_initializer();
  ROSE_ASSERT(init != nullptr);
  SgAssignInitializer *assignInit = isSgAssignInitializer(init);
  if (assignInit == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[short-circuit-initializer]: declaration=%p "
            "has no exact assignment initializer\n",
            static_cast<void *>(varDecl));
    ROSE_ABORT();
  }
  SgVariableSymbol *var =
      isSgVariableSymbol(in->get_symbol_from_symbol_table());
  if (var == nullptr || var->get_declaration() != in) {
    fprintf(stderr, "REX_AST_INVARIANT[short-circuit-variable-reference]: "
                    "initialized name has no exact typed variable symbol\n");
    ROSE_ABORT();
  }

  SgExpression *operand = assignInit->get_operand_i();
  ROSE_ASSERT(operand != nullptr && operand->get_parent() == assignInit);
  ROSE_ASSERT(init->get_parent() == in);
  SgVarRefExp *varRefForAssign =
      buildSCGeneratedNode(new SgVarRefExp(SgNULL_FILE, var));

  in->set_initializer(nullptr);
  SgExpression *released = assignInit->release_operand();
  ROSE_ASSERT(released == operand);
  assignInit->set_parent(nullptr);
  delete assignInit;

  return SageBuilder::buildAssignStatement(varRefForAssign, operand);
}

/*
 * Move condition in a loop that tests at the beginning of each iteration (for,
 * while) into the body.
 */
void moveConditionToBody(SgStatement *cond, SgBasicBlock *body) {
  ROSE_ASSERT(cond != nullptr);
  ROSE_ASSERT(body != nullptr);
  SgNode *const originalConditionalParent = cond->get_parent();
  SgWhileStmt *whileStmt = isSgWhileStmt(originalConditionalParent);
  SgForStatement *forStmt = isSgForStatement(originalConditionalParent);
  ROSE_ASSERT(whileStmt != nullptr || forStmt != nullptr);
  ROSE_ASSERT(isSgExprStatement(cond) != nullptr ||
              isSgVariableDeclaration(cond) != nullptr);

  SgBoolValExp *trueExp =
      buildSCGeneratedNode(new SgBoolValExp(SgNULL_FILE, 1));

  SgExprStatement *trueStmt =
      buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, trueExp));
  publishSCGeneratedSubtree(trueStmt, cond);
  if (whileStmt != nullptr) {
    whileStmt->set_condition(trueStmt);
  } else {
    forStmt->set_test(trueStmt);
  }
  trueStmt->set_parent(originalConditionalParent);
  cond->set_parent(nullptr);

  SgStatement *ifCondition = cond;
  if (SgVariableDeclaration *varDecl = isSgVariableDeclaration(cond)) {
    body->prepend_statement(varDecl);
    varDecl->set_parent(body);
    ifCondition = splitVarDecl(varDecl);
  }

  SgBasicBlock *trueBody = buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));

  SgBreakStmt *breakStmt = buildSCGeneratedNode(new SgBreakStmt(SgNULL_FILE));
  SgBasicBlock *falseBody =
      buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, breakStmt));
  breakStmt->set_parent(falseBody);

  SgIfStmt *ifStmt = buildSCGeneratedNode(
      new SgIfStmt(SgNULL_FILE, ifCondition, trueBody, falseBody));

  ifStmt->set_is_short_circuit_generated(true);
  publishSCGeneratedSubtree(ifStmt, cond);
  body->prepend_statement(ifStmt);
  ifStmt->set_parent(body);
}

void initialTransformation(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  requireSplittableDeclarations(project);
  rewriteConditionals<SgAndOp, V_SgAndOp, isSgAndOp>(project);
  rewriteConditionals<SgOrOp, V_SgOrOp, isSgOrOp>(project);

  std::set<SgFunctionDefinition *> functionsWithJumps;
  for (VariantT kind : {V_SgGotoStatement, V_SgAsmStmt})
    for (SgNode *node : NodeQuery::querySubTree(project, kind))
      if (auto *function = SageInterface::getEnclosingFunctionDefinition(node))
        functionsWithJumps.insert(function);

  Rose_STL_Container<SgNode *> ifStmts =
      NodeQuery::querySubTree(project, V_SgIfStmt);
  for (Rose_STL_Container<SgNode *>::iterator i = ifStmts.begin();
       i != ifStmts.end(); ++i) {
    SgIfStmt *ifStmt = isSgIfStmt(*i);
    ROSE_ASSERT(ifStmt != NULL);
    if (ifStmt->get_is_if_consteval_statement()) {
      ROSE_ASSERT(ifStmt->get_conditional() == nullptr);
      continue;
    }

    SgStatement *cond = ifStmt->get_conditional();
    if (hasSC(cond)) {
      auto *ifStmtParent = isSgBasicBlock(ifStmt->get_parent());
      if (ifStmtParent == nullptr) {
        ifStmtParent =
            publishSCGeneratedSubtree(SageBuilder::buildBasicBlock(), ifStmt);
        SageInterface::replaceStatement(ifStmt, ifStmtParent);
        ifStmtParent->append_statement(ifStmt);
      }
      if (isSgExprStatement(cond) &&
          ifStmt->get_if_statement_kind() == SgIfStmt::e_if_ordinary) {
        auto *function = SageInterface::getEnclosingFunctionDefinition(ifStmt);
        ROSE_ASSERT(function != nullptr);
        SgInitializer *initializer = nullptr;
        if (functionsWithJumps.count(function) == 0)
          initializer = SageBuilder::buildAssignInitializer(
              SageBuilder::buildBoolValExp(false),
              SageBuilder::buildBoolType());
        auto *flag = SageBuilder::buildVariableDeclaration(
            SageInterface::generateUniqueVariableName(ifStmtParent,
                                                      "rose_sc_bool_"),
            SageBuilder::buildBoolType(), initializer, ifStmtParent);
        ROSE_ASSERT(flag->get_variables().size() == 1);
        auto *name = flag->get_variables().front();
        auto *symbol = isSgVariableSymbol(name->get_symbol_from_symbol_table());
        ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == name);
        auto *replacement = SageBuilder::buildExprStatement(
            SageBuilder::buildVarRefExp(symbol));
        ifStmt->set_conditional(replacement);
        replacement->set_parent(ifStmt);
        cond->set_parent(nullptr);
        auto *evaluate = SageBuilder::buildIfStmt(
            cond,
            SageBuilder::buildBasicBlock(SageBuilder::buildAssignStatement(
                SageBuilder::buildVarRefExp(symbol),
                SageBuilder::buildBoolValExp(true))),
            SageBuilder::buildBasicBlock(SageBuilder::buildAssignStatement(
                SageBuilder::buildVarRefExp(symbol),
                SageBuilder::buildBoolValExp(false))));
        publishSCGeneratedSubtree(replacement, ifStmt);
        publishSCGeneratedSubtree(flag, ifStmt);
        publishSCGeneratedSubtree(evaluate, ifStmt);
        SageInterface::insertStatementBefore(ifStmt, flag, false);
        SageInterface::insertStatementBefore(ifStmt, evaluate, false);
      }
    }
  }

  Rose_STL_Container<SgNode *> whileStmts =
      NodeQuery::querySubTree(project, V_SgWhileStmt);
  for (Rose_STL_Container<SgNode *>::iterator i = whileStmts.begin();
       i != whileStmts.end(); ++i) {
    SgWhileStmt *whileStmt = isSgWhileStmt(*i);
    ROSE_ASSERT(whileStmt != NULL);

    SgStatement *cond = whileStmt->get_condition();
    if (hasSC(cond)) {
      moveConditionToBody(
          cond, SageInterface::ensureBasicBlockAsBodyOfWhile(whileStmt));
    }
  }

  Rose_STL_Container<SgNode *> dowhileStmts =
      NodeQuery::querySubTree(project, V_SgDoWhileStmt);
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

      publishSCGeneratedSubtree(trueStmt, dowhileStmt);
      dowhileStmt->set_condition(trueStmt);
      trueStmt->set_parent(dowhileStmt);
      cond->set_parent(nullptr);

      SgBasicBlock *trueBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));

      SgBreakStmt *breakStmt =
          buildSCGeneratedNode(new SgBreakStmt(SgNULL_FILE));
      SgBasicBlock *falseBody =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, breakStmt));
      breakStmt->set_parent(falseBody);

      SgIfStmt *ifStmt = buildSCGeneratedNode(
          new SgIfStmt(SgNULL_FILE, cond, trueBody, falseBody));
      SageInterface::addStepToLoopBody(dowhileStmt, ifStmt);
      publishSCGeneratedSubtree(dowhileStmt->get_body(), dowhileStmt);
    }
  }

  Rose_STL_Container<SgNode *> forStmts =
      NodeQuery::querySubTree(project, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::iterator i = forStmts.begin();
       i != forStmts.end(); ++i) {
    SgForStatement *forStmt = isSgForStatement(*i);
    ROSE_ASSERT(forStmt != NULL);

    SgForInitStatement *forInit = forStmt->get_for_init_stmt();
    if (hasSC(forInit)) {
      SgBasicBlock *forStmtParentBB = isSgBasicBlock(forStmt->get_parent());
      ROSE_ASSERT(forStmtParentBB != NULL);
      SgStatementPtrList &initStmts = forInit->get_init_stmt();
      ROSE_ASSERT(forInit->get_parent() == forStmt && !initStmts.empty());
      ROSE_ASSERT(std::count(forStmtParentBB->get_statements().begin(),
                             forStmtParentBB->get_statements().end(),
                             forStmt) == 1);
      for (SgStatement *initStmt : initStmts) {
        ROSE_ASSERT(initStmt != nullptr && initStmt->get_parent() == forInit);
        ROSE_ASSERT(std::count(initStmts.begin(), initStmts.end(), initStmt) ==
                    1);
      }

      SgBasicBlock *initBlock =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE));
      publishSCGeneratedSubtree(initBlock, forStmt);
      forStmtParentBB->replace_statement(forStmt, initBlock);

      SgStatementPtrList movedInitStmts;
      movedInitStmts.swap(initStmts);
      for (SgStatement *initStmt : movedInitStmts) {
        initStmt->set_parent(nullptr);
        initBlock->append_statement(initStmt);
      }
      initBlock->append_statement(forStmt);
      forInit->append_init_stmt(
          buildSCGeneratedNode(new SgNullStatement(SgNULL_FILE)));
      SageInterface::rebindVariableReferencesAfterMove(initBlock);
    }

    SgStatement *forTest = forStmt->get_test();
    if (hasSC(forTest)) {
      moveConditionToBody(forTest,
                          SageInterface::ensureBasicBlockAsBodyOfFor(forStmt));
    }

    SgExpression *forInc = forStmt->get_increment();
    if (hasSC(forInc)) {
      SageInterface::moveForStatementIncrementIntoBody(forStmt);
    }
  }

  Rose_STL_Container<SgNode *> varDeclStmts =
      NodeQuery::querySubTree(project, V_SgVariableDeclaration);
  for (Rose_STL_Container<SgNode *>::iterator i = varDeclStmts.begin();
       i != varDeclStmts.end(); ++i) {
    SgVariableDeclaration *varDecl = isSgVariableDeclaration(*i);
    ROSE_ASSERT(varDecl != NULL);

    if (auto *owner = isSgIfStmt(varDecl->get_parent());
        owner != nullptr && owner->get_conditional() == varDecl) {
      continue;
    }

    if (hasSC(varDecl)) {
      SgBasicBlock *varDeclParent = isSgBasicBlock(varDecl->get_parent());
      ROSE_ASSERT(varDeclParent);
      auto &statements = varDeclParent->get_statements();
      auto position = std::find(statements.begin(), statements.end(), varDecl);
      ROSE_ASSERT(position != statements.end());

      SgExprStatement *varAssign = splitVarDecl(varDecl);
      publishSCGeneratedSubtree(varAssign, varDecl);
      statements.insert(++position, varAssign);
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

bool reduceIfStmtsWithSCchild(SgProject *prj) {
  bool retVal = false;
  Rose_STL_Container<SgExpression *> appSCs = findApplicableSCs(prj);

  for (Rose_STL_Container<SgExpression *>::iterator i = appSCs.begin();
       i != appSCs.end(); ++i) {
    SgExpression *expr = *i;
    SgStatement *stmt = findStatementForExpression(expr);
    pair<SgBasicBlock *, SgNode *> bbPair = findBasicBlockForStmt(stmt);
    SgBasicBlock *basicBlock = bbPair.first;
    SgStatement *basicBlockChild = isSgStatement(bbPair.second);
    ROSE_ASSERT(basicBlockChild != NULL);

    retVal = true;

    SgStatement *fullStmt;

    switch (basicBlockChild->variantT()) {
    case V_SgExprStatement: {
      if (basicBlockChild->get_is_short_circuit_generated()) {
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

      SgExpression *dummyExpr =
          SageInterface::copyExpression(commaOpExp->get_lhs_operand());

      SgExprStatement *dummyExprStmt =
          buildSCGeneratedNode(new SgExprStatement(SgNULL_FILE, dummyExpr));

      dummyExprStmt->set_is_short_circuit_generated(true);

      SgExpression *rhsExpr =
          SageInterface::copyExpression(commaOpExp->get_rhs_operand());
      SageInterface::replaceExpression(commaOpExp, rhsExpr);

      SgBasicBlock *newBB =
          buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, dummyExprStmt));
      dummyExprStmt->set_parent(newBB);

      SgStatement *fullStmtParent = isSgStatement(fullStmt->get_parent());
      ROSE_ASSERT(fullStmtParent != nullptr);
      publishSCGeneratedSubtree(newBB, fullStmt);
      fullStmtParent->replace_statement(fullStmt, newBB);
      fullStmt->set_parent(nullptr);
      newBB->append_statement(fullStmt);

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

      auto copyBranch = [&](SgExpression *arm) -> SgBasicBlock * {
        SgCapturingCopy<SgTreeCopy> copy(vector<SgNode *>(1, condExp));
        SgStatement *statement = isSgStatement(fullStmt->copy(copy));
        ROSE_ASSERT(copy.get_copyList().size() == 1);
        SgExpression *conditional = isSgExpression(copy.get_copyList().at(0));
        ROSE_ASSERT(statement != nullptr && conditional != nullptr);
        SgBasicBlock *branch = isSgBasicBlock(statement);
        if (branch == nullptr) {
          branch =
              buildSCGeneratedNode(new SgBasicBlock(SgNULL_FILE, statement));
          statement->set_parent(branch);
        }
        ROSE_ASSERT(branch->get_parent() == nullptr);
        SageInterface::replaceExpression(conditional,
                                         SageInterface::copyExpression(arm));
        publishSCGeneratedSubtree(branch, fullStmt);
        return branch;
      };

      SgBasicBlock *trueBranch = copyBranch(condExp->get_true_exp());
      SgBasicBlock *falseBranch = copyBranch(condExp->get_false_exp());

      SgExpression *sourceCondition = condExp->get_conditional_exp();
      SgBoolValExp *constantCondition = isSgBoolValExp(sourceCondition);
      const bool hasConstantCondition = constantCondition != nullptr;
      const bool constantValue =
          hasConstantCondition && constantCondition->get_value() != 0;
      SgExpression *conditionExp =
          !hasConstantCondition ? SageInterface::copyExpression(sourceCondition)
                                : nullptr;
      SgStatement *replacementStmt;
      if (hasConstantCondition) {
        if (constantValue) {
          replacementStmt = trueBranch;
          SageInterface::deepDelete(falseBranch);
        } else {
          replacementStmt = falseBranch;
          SageInterface::deepDelete(trueBranch);
        }
      } else {
        SgExprStatement *conditionExpStmt = buildSCGeneratedNode(
            new SgExprStatement(SgNULL_FILE, conditionExp));

        SgIfStmt *ifStmt = buildSCGeneratedNode(new SgIfStmt(
            SgNULL_FILE, conditionExpStmt, trueBranch, falseBranch));
        ifStmt->set_is_short_circuit_generated(true);

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

      publishSCGeneratedSubtree(replacementStmt, fullStmt);
      fullStmtParent->replace_statement(fullStmt, replacementStmt);
      fullStmt->set_parent(nullptr);
      SageInterface::deepDelete(fullStmt);

      break;
    }

    default: {
      puts("whoops");
      ROSE_ABORT();
    }
    }
    // A replacement can retire the statement containing another candidate.
    break;
  }
  return retVal;
}

void shortCircuitingTransformation(SgProject *prj) {
  initialTransformation(prj);
  while (reduceIfStmtsWithSCchild(prj)) {
  }
}
