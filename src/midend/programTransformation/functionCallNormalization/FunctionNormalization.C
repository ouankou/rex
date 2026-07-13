// tps (01/14/2010) : Switching from rose.h to sage3.
#include "FunctionNormalization.h"

#include "ROSE_FALLTHROUGH.h"

#include "sage3basic.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

/*
 * Normalization of function calls - no statement will have more than one
 * function call This is to be done by inserting new variables, while preserving
 * the semantics of the code. Author: Radu Popovici Date: Sept-08-2005
 */

/*
Example:
Assume we have the functions:
  bool g ( int );
  int f1 ( bool );
  int &f2 ( bool );
and the code:
  for ( ; f1( g( 1 ) ) < 10; f2( g( 2 ) ) );

The code will become ( note the three different types of statements generated ):
  bool temp1 = g( 1 );        // --- type1
  int temp2 = f1( temp1 );    // --- type1
  bool temp3;                 // --- type2
  int *temp4;                 // --- type2
  for ( ; temp2 < 10; *temp4 )
  {
    temp3 = g( 2 );           // --- type3
    temp4 = &f2( temp3 );     // --- type3
    temp1 = g( 1 );           // --- type3
    temp2 = f1( temp1 );      // --- type3
  }

We generated 3 types of statements: initialized variable declaration,
non-initialized variable declaration and assignment. For each statement we
process we will generate a list structures, one structure for each function call
of the statement. The structure is called Declarations, and contains the three
types of statements for the desired function call Depending on the enclosing
scope we will insert the appropriate type(s) of statements to replace each
function call.

Note: in the case of functions returning references, we need to assign those to
pointers, in the case when we cannot initialize the substituting variable at the
point of its declaration (e.g. for the increment of for-loops).

The Algorithm:
1. get statement
2. generate list of (direct) function calls in the statement (not considering
statements associated with the current one)
3. for each function call, generate a structure containing the 3 statements, one
of each type (some may be null in some cases)
4. insert the newly created statements where appropriate
5. replace in the original expression the function calls with their
corresponding variables (at the highest level)
*/

void FunctionCallNormalization::visit(SgNode *astNode) {
  SgStatement *stm = isSgStatement(astNode);

  // visiting all statements which may contain function calls;
  // Note 1: we do not look at the body of loops, or sequences of statements,
  // but only at statements which may contain directly function calls; all other
  // statements will have their component parts visited in turn
  if (isSgEnumDeclaration(astNode) || isSgVariableDeclaration(astNode) ||
      isSgVariableDefinition(astNode) || isSgExprStatement(astNode) ||
      isSgForStatement(astNode) || isSgReturnStmt(astNode) ||
      isSgSwitchStatement(astNode)) {
    // Maintain the semantic recipe for each replacement.  Replacement
    // expressions are built fresh at every use site; no detached AST template
    // is shared or copied between structural owners.
    FunctionCallReplacementMap fct2Var;

    // list of Declaration structures, one structure per function call
    DeclarationPtrList declarations;
    bool variablesDefined = false;

    // list of function calls, in correnspondence with the inForTest list below
    list<SgNode *> functionCallExpList;
    list<bool> inForTest;

    SgForStatement *forStm = isSgForStatement(stm);
    SgSwitchStatement *swStm = isSgSwitchStatement(stm);
    SgScopeStatement *scope = stm->get_scope();
    ROSE_ASSERT(scope);
    list<SgNode *> temp1, temp2;

    // for-loops and Switch statements have conditions ( and increment )
    // expressed as expressions and not as standalone statements; this will
    // change in future Sage versions
    // TODO: when for-loops and switch statements have conditions expressed via
    // SgStatements these cases won't be treated separately; however, do-while
    // will have condition expressed via expression so that will be the only
    // exceptional case to be treated separately
    if (forStm != NULL) {
      // create a list of function calls in the condition and increment
      // expression the order is important, the condition is evaluated after the
      // increment expression temp1 = FEOQueryForNodes(
      // forStm->get_increment_expr_root(), V_SgFunctionCallExp ); temp2 =
      // FEOQueryForNodes( forStm->get_test_expr_root(), V_SgFunctionCallExp );
      temp1 = FEOQueryForNodes(forStm->get_increment(), V_SgFunctionCallExp);
      temp2 = FEOQueryForNodes(forStm->get_test(), V_SgFunctionCallExp);
      functionCallExpList = temp1;
      functionCallExpList.splice(functionCallExpList.end(), temp2);
    } else {
      if (swStm != NULL) {
        // create a list of function calls in the condition in the order of
        // function evaluation DQ (11/23/2005): Fixed SgSwitchStmt to have
        // SgStatement for conditional. list<SgNode*> temp1 = FEOQueryForNodes(
        // swStm->get_item_selector_root(), V_SgFunctionCallExp );
        list<SgNode *> temp1 =
            FEOQueryForNodes(swStm->get_item_selector(), V_SgFunctionCallExp);
        functionCallExpList = temp1;
      } else {
        // create a list of function calls in the statement in the order of
        // function evaluation
        functionCallExpList = FEOQueryForNodes(stm, V_SgFunctionCallExp);
      }
    }

    // all function calls get replaced: this is because they can occur in
    // expressions (e.g. for-loops) which makes it difficult to build control
    // flow graphs
    if (functionCallExpList.size() > 0) {
      cout << "--------------------------------------\nStatement ";
      cout << stm->unparseToString() << "\n";
      ;

      // traverse the list of function calls in the current statement, generate
      // a structure  Declaration for each call put these structures in a list
      // to be inserted in the code later
      for (list<SgNode *>::iterator i = functionCallExpList.begin();
           i != functionCallExpList.end(); i++) {
        // get function call exp
        SgFunctionCallExp *exp = isSgFunctionCallExp(*i);
        ROSE_ASSERT(exp);

        // get type of expression, generate unique variable name
        SgType *expType = exp->get_type();
        ROSE_ASSERT(expType);
        SgType *strippedExpType = expType->stripType();
        if (isSgTypeVoid(strippedExpType))
          continue;

        variablesDefined = true;

        // replace previous variable bindings in the AST
        SgExprListExp *paramsList = exp->get_args();
        SgExpression *function = exp->get_function();
        ROSE_ASSERT(paramsList && function);
        replaceFunctionCallsInExpression(paramsList, fct2Var);
        replaceFunctionCallsInExpression(function, fct2Var);

        Declaration *newDecl = new Declaration();
        SgStatement *nonInitVarDeclaration = NULL, *initVarDeclaration = NULL,
                    *assignStmt = NULL;
        SgVariableSymbol *varSymbol = NULL;
        SgInitializedName *initName = NULL;

        bool pointerTypeNeeded = false;
        bool useNonInitDeclaration = false;
        bool needsAssignment = false;

        // mark whether to replace inside or outside of ForStatement due to the
        // function call being inside the test or the increment for a for-loop
        // statement the 'inForTest' list is in 1:1  ordered correpondence with
        // the 'declarations' list
        if (forStm) {
          needsAssignment = true;

          SgStatement *test = isSgForStatement(astNode)->get_test();
          SgExpression *increment = isSgForStatement(astNode)->get_increment();
          ROSE_ASSERT(test && increment);
          SgNode *up = exp;
          while (up && up != test && up != increment)
            up = up->get_parent();
          ROSE_ASSERT(up);

          // function call is in the condition of the for-loop
          if (up == test)
            inForTest.push_back(true);
          // function call is in the increment expression
          else {
            inForTest.push_back(false);
            useNonInitDeclaration = true;

            // for increment expressions we need to be able to reassign the
            // return value of the function; if the ret value is a reference, we
            // need to generate a pointer of that type (to be able to reassign
            // it later)
            if (isSgReferenceType(expType))
              pointerTypeNeeded = true;
          }
        }

        // for do-while statements:  we need to generate declaration of type
        // pointer to be able to have non-assigned references when looping and
        // assign them at the end of the body of the loop
        if (isSgDoWhileStmt(stm->get_parent()) && isSgReferenceType(expType))
          pointerTypeNeeded = true;
        if (scope->variantT() == V_SgWhileStmt ||
            scope->variantT() == V_SgDoWhileStmt)
          needsAssignment = true;
        if (scope->variantT() == V_SgDoWhileStmt)
          useNonInitDeclaration = true;
        if (pointerTypeNeeded)
          useNonInitDeclaration = true;

        // Every generated declaration is semantically scoped where it will be
        // inserted.  Conditions owned by a control-flow statement introduce
        // their temporaries in that statement's enclosing scope.
        SgScopeStatement *declarationScope = scope;
        if (!forStm && (scope->variantT() == V_SgWhileStmt ||
                        scope->variantT() == V_SgDoWhileStmt ||
                        scope->variantT() == V_SgForStatement ||
                        scope->variantT() == V_SgIfStmt ||
                        scope->variantT() == V_SgSwitchStatement)) {
          if (SageInterface::isBodyStatement(scope)) {
            declarationScope =
                SageInterface::makeSingleStatementBodyToBlock(scope);
          } else {
            declarationScope =
                SageInterface::getEnclosingScope(scope->get_parent(), true);
          }
        }
        ROSE_ASSERT(declarationScope);
        SgName name = SageInterface::generateUniqueVariableName(
            declarationScope, "__tempVar__");

        // Duplicate function call expression only for nodes that are actually
        // inserted into the AST.
        SgFunctionCallExp *newExpInit = NULL, *newExpAssign = NULL;
        if (!useNonInitDeclaration) {
          SgTreeCopy initTreeCopy;
          newExpInit = isSgFunctionCallExp(exp->copy(initTreeCopy));
          ROSE_ASSERT(newExpInit);
        }
        if (needsAssignment) {
          SgTreeCopy assignTreeCopy;
          newExpAssign = isSgFunctionCallExp(exp->copy(assignTreeCopy));
          ROSE_ASSERT(newExpAssign);
        }

        SgType *declarationType = expType;
        SgInitializer *declarationInitializer = NULL;
        SgPointerType *pointerType = NULL;

        // we have a function call returning a reference and we can't initialize
        // the variable at the point of declaration; we need to define the
        // variable as a pointer
        if (pointerTypeNeeded) {
          ROSE_ASSERT(needsAssignment && newExpAssign);
          SgType *base = isSgReferenceType(expType)->get_base_type();
          ROSE_ASSERT(base);
          pointerType = SageBuilder::buildPointerType(base);
          ROSE_ASSERT(pointerType);
          declarationType = pointerType;
        } else {
          // Build only the declaration shape used by the enclosing control-flow
          // context.
          if (!useNonInitDeclaration) {
            ROSE_ASSERT(newExpInit);
            declarationInitializer =
                SageBuilder::buildAssignInitializer(newExpInit, expType);
          }
        }

        SgVariableDeclaration *variableDeclaration =
            SageBuilder::buildVariableDeclaration(name, declarationType,
                                                  declarationInitializer,
                                                  declarationScope);
        ROSE_ASSERT(variableDeclaration);
        initName = variableDeclaration->get_decl_item(name);
        ROSE_ASSERT(initName && initName->get_scope() == declarationScope);
        varSymbol = isSgVariableSymbol(
            declarationScope->find_symbol_from_declaration(initName));
        ROSE_ASSERT(varSymbol && varSymbol->get_declaration() == initName);

        if (useNonInitDeclaration)
          nonInitVarDeclaration = variableDeclaration;
        else
          initVarDeclaration = variableDeclaration;

        if (needsAssignment) {
          ROSE_ASSERT(newExpAssign);
          SgExpression *assignmentRhs = newExpAssign;
          if (pointerTypeNeeded) {
            assignmentRhs =
                SageBuilder::buildAddressOfOp(newExpAssign, pointerType);
          }
          SgVarRefExp *assignmentLhs = SageBuilder::buildVarRefExp(varSymbol);
          SgAssignOp *assignOp = SageBuilder::buildAssignOp(
              assignmentLhs, assignmentRhs, declarationType);
          assignStmt = SageBuilder::buildExprStatement(assignOp);
          ROSE_ASSERT(assignStmt);
        }

        const bool inserted =
            fct2Var
                .insert(std::make_pair(
                    exp, FunctionCallReplacement{varSymbol, expType,
                                                 pointerTypeNeeded}))
                .second;
        ROSE_ASSERT(inserted);

        // save the 'declaration' structure, with all 3 statements and the
        // variable name
        newDecl->nonInitVarDeclaration = nonInitVarDeclaration;
        newDecl->initVarDeclaration = initVarDeclaration;
        newDecl->assignment = assignStmt;
        newDecl->symbol = varSymbol;
        newDecl->name = name;
        ROSE_ASSERT(!initVarDeclaration ||
                    initVarDeclaration->get_parent() == NULL);
        ROSE_ASSERT(!nonInitVarDeclaration ||
                    nonInitVarDeclaration->get_parent() == NULL);
        ROSE_ASSERT(!assignStmt || assignStmt->get_parent() == NULL);
        declarations.push_back(newDecl);
      } // end for
    } // end if  fct calls in crt stmt > 1

    // insert function bindings to variables; each 'declaration' structure in
    // the list corresponds to one function call
    for (DeclarationPtrList::iterator i = declarations.begin();
         i != declarations.end(); i++) {
      Declaration *d = *i;
      ROSE_ASSERT(d);
      ROSE_ASSERT(d->initVarDeclaration || d->nonInitVarDeclaration);
      ROSE_ASSERT(d->symbol);

      // if the current statement is a for-loop, we insert Declarations before &
      // in the loop body, depending on the case
      if (forStm) {
        SgScopeStatement *parentScope = stm->get_scope();
        SgBasicBlock *body = SageInterface::ensureBasicBlockAsBodyOfFor(forStm);
        ROSE_ASSERT(!inForTest.empty() && body && parentScope);
        // SgStatementPtrList &list = body->get_statements();

        // if function call is in loop condition, we add initialized variable
        // before the loop and at its end hoist initialized variable
        // declarations outside the loop
        if (inForTest.front()) {
          ROSE_ASSERT(d->initVarDeclaration);
          SageInterface::insertStatementBefore(stm, d->initVarDeclaration);

          // set the scope of the initializedName
          SgInitializedName *initName =
              isSgVariableDeclaration(d->initVarDeclaration)
                  ->get_decl_item(d->name);
          ROSE_ASSERT(initName);
          ROSE_ASSERT(initName->get_scope() == parentScope);
          ROSE_ASSERT(parentScope->find_symbol_from_declaration(initName) ==
                      d->symbol);
        }
        // function call is in loop post increment so add noninitialized
        // variable decls above the loop
        else {
          SageInterface::insertStatementBefore(stm, d->nonInitVarDeclaration);

          // set the scope of the initializedName
          SgInitializedName *initName =
              isSgVariableDeclaration(d->nonInitVarDeclaration)
                  ->get_decl_item(d->name);
          ROSE_ASSERT(initName);
          ROSE_ASSERT(initName->get_scope() == parentScope);
          ROSE_ASSERT(parentScope->find_symbol_from_declaration(initName) ==
                      d->symbol);
        }

        // in a for-loop, always insert assignments at the end of the loop
        ROSE_ASSERT(d->assignment);
        SageInterface::appendStatement(d->assignment, body);

        // remove marker
        inForTest.pop_front();
      } else {
        // look at the type of the enclosing scope
        switch (scope->variantT()) {

          // while stmts have to repeat the function calls at the end of the
          // loop; note there is no "break" statement, since we want to also add
          // initialized declarations before the while-loop
        case V_SgWhileStmt: {
          // assignments need to be inserted at the end of each while loop
          SgBasicBlock *body = SageInterface::ensureBasicBlockAsBodyOfWhile(
              isSgWhileStmt(scope));
          ROSE_ASSERT(body && d->assignment);
          SageInterface::appendStatement(d->assignment, body);
        }
          ROSE_FALLTHROUGH;

          // SgForInitStatement has scope SgForStatement, move declarations
          // before the for loop; same thing if the enclosing scope is an If, or
          // Switch statement
        case V_SgForStatement:
        case V_SgIfStmt:
        case V_SgSwitchStatement: {
          ROSE_ASSERT(d->initVarDeclaration);
          // adding bindings (initialized variable declarations only, not
          // assignments) outside the statement, in the parent scope
          ROSE_ASSERT(scope->get_parent());
          SageInterface::insertStatementBefore(scope, d->initVarDeclaration);

          // setting the scope of the initializedName
          SgInitializedName *initName =
              isSgVariableDeclaration(d->initVarDeclaration)
                  ->get_decl_item(d->name);
          ROSE_ASSERT(initName);
          SgScopeStatement *outerScope =
              SageInterface::getEnclosingScope(scope->get_parent(), true);
          ROSE_ASSERT(initName->get_scope() == outerScope);
          ROSE_ASSERT(outerScope->find_symbol_from_declaration(initName) ==
                      d->symbol);
        } break;

          // do-while needs noninitialized declarations before the loop, with
          // assignments inside the loop
        case V_SgDoWhileStmt: {
          ROSE_ASSERT(d->nonInitVarDeclaration && d->assignment);
          // adding noninitialized variable declarations before the body of the
          // loop
          ROSE_ASSERT(scope->get_parent());
          SageInterface::insertStatementBefore(scope, d->nonInitVarDeclaration);

          // initialized name scope setting
          SgInitializedName *initName =
              isSgVariableDeclaration(d->nonInitVarDeclaration)
                  ->get_decl_item(d->name);
          ROSE_ASSERT(initName);
          SgScopeStatement *outerScope =
              SageInterface::getEnclosingScope(scope->get_parent(), true);
          ROSE_ASSERT(initName->get_scope() == outerScope);
          ROSE_ASSERT(outerScope->find_symbol_from_declaration(initName) ==
                      d->symbol);

          // adding assignemts at the end of the do-while loop
          SgBasicBlock *body = SageInterface::ensureBasicBlockAsBodyOfDoWhile(
              isSgDoWhileStmt(scope));
          ROSE_ASSERT(body);
          SageInterface::appendStatement(d->assignment, body);
        } break;

          // for all other scopes, add bindings ( initialized declarations )
          // before the statement, in the same scope
        default:
          ROSE_ASSERT(d->initVarDeclaration);
          SageInterface::insertStatementBefore(stm, d->initVarDeclaration);

          // initialized name scope setting
          SgInitializedName *initName =
              isSgVariableDeclaration(d->initVarDeclaration)
                  ->get_decl_item(d->name);
          ROSE_ASSERT(initName);
          ROSE_ASSERT(initName->get_scope() == scope);
          ROSE_ASSERT(scope->find_symbol_from_declaration(initName) ==
                      d->symbol);
        }
      }
    }

    // once we have inserted all variable declarations, we need to replace
    // top-level calls in the original statement
    if (variablesDefined) {
      cout << "\tReplacing in the expression " << stm->unparseToString()
           << "\n";

      // for ForStatements, replace expressions in condition and increment
      // expressions, not in the body, since those get replace later
      if (forStm) {
        replaceFunctionCallsInExpression(forStm->get_increment(), fct2Var);
        replaceFunctionCallsInExpression(forStm->get_test(), fct2Var);
      } else if (swStm) {
        // DQ (11/23/2005): Fixed SgSwitch to permit use of declaration for
        // conditional replaceFunctionCallsInExpression(
        // swStm->get_item_selector_root(), fct2Var );
        replaceFunctionCallsInExpression(swStm->get_item_selector(), fct2Var);
      } else
        replaceFunctionCallsInExpression(stm, fct2Var);
    }

    for (Declaration *declaration : declarations) {
      delete declaration;
    }
  } // end if isSgStatement block
}

/*
Given a node in the AST, it replaces all function calls with the corresponding
expression from the fct2Var mapping. Since we traverse in order of function
evaluation, all nodes that are lower in the subtree are supposed to have been
mapped, since they have already appeared in the list. Also, we only replace the
shallowest level of function calls in the surrent subtree, since we assume that
replaceFunctionCallsInExpression  has already been called for each of the
subtrees rooted at the shallowest function calls lower than the current node:

E.g.: f(g(h(i))) + 5
has the AST:
     +
    / \
  fc1  5
  / \
 f  fc2
    / \
   g  fc3
      / \
     h   i
where fc? represents an SgFunctionCallExpression. Order of function evaluation
is h(i), g(_), f(_).

Calling 'replaceFunctionCallsInExpression' on '+' will generate
                     temp_var + 5
but calling the same function on 'fc2' will generate
                     f(temp_var) + 5
*/
void FunctionCallNormalization::replaceFunctionCallsInExpression(
    SgNode *root, const FunctionCallReplacementMap &fct2Var) {
  if (!root)
    return;

  if (NodeQuery::querySubTree(root, V_SgFunctionCallExp).size() > 0) {
    list<SgNode *> toVisit;
    toVisit.push_back(root);

    while (!toVisit.empty()) {
      SgNode *crt = toVisit.front();

      // will visit every node above an function call exp, but not below
      // also, we will replace function call expressions found
      if (!isSgFunctionCallExp(crt)) {
        vector<SgNode *> succ = (crt)->get_traversalSuccessorContainer();
        for (vector<SgNode *>::iterator succIt = succ.begin();
             succIt != succ.end(); succIt++)
          if (isSgNode(*succIt))
            toVisit.push_back(*succIt);
      } else {
        SgFunctionCallExp *call = isSgFunctionCallExp(crt);
        ROSE_ASSERT(call);

        FunctionCallReplacementMap::const_iterator mapIter = fct2Var.find(call);
        if (mapIter == fct2Var.end())
          cout << "NOT FOUND " << call->unparseToString() << "\t" << call
               << "\n";

        // a mapping for function call exps in the subtree must already exist (
        // postorder traversal )
        ROSE_ASSERT(mapIter != fct2Var.end());
        const FunctionCallReplacement &replacement = mapIter->second;
        ROSE_ASSERT(replacement.symbol && replacement.expressionType);

        // Construct one fresh, fully source-positioned replacement for this
        // exact structural use site.
        SgExpression *newVar = SageBuilder::buildVarRefExp(replacement.symbol);
        if (replacement.dereference) {
          newVar = SageBuilder::buildPointerDerefExp(
              newVar, replacement.expressionType);
        }
        ROSE_ASSERT(newVar && newVar->get_parent() == NULL);

        ROSE_ASSERT(call->get_parent());

        // Replace through SageInterface so non-expression parents (e.g.
        // SgExprStatement) are handled correctly.
        SageInterface::replaceExpression(call, newVar, true);
      }
      toVisit.pop_front();
    }
  }
}

/*
Query on a the AST using the VariantT and BFS traversal
*/
list<SgNode *> FunctionCallNormalization::BFSQueryForNodes(SgNode *root,
                                                           VariantT type) {
  list<SgNode *> toVisit, retList;
  toVisit.push_back(root);

  while (!toVisit.empty()) {
    SgNode *crt = toVisit.front();
    if (crt->variantT() == type)
      retList.push_back(crt);

    vector<SgNode *> succ = (crt)->get_traversalSuccessorContainer();
    for (vector<SgNode *>::iterator succIt = succ.begin(); succIt != succ.end();
         succIt++)
      if (isSgNode(*succIt))
        toVisit.push_back(*succIt);

    toVisit.pop_front();
  }
  return retList;
}

/*
Query on a list of nodes using the VariantT
*/
list<SgNode *> FunctionCallNormalization::FEOQueryForNodes(SgNode *root,
                                                           VariantT type) {
  list<SgNode *> toVisit = createTraversalList(root);
  list<SgNode *> retList;

  for (list<SgNode *>::iterator succIt = toVisit.begin();
       succIt != toVisit.end(); succIt++)
    if (isSgNode(*succIt) && isSgNode(*succIt)->variantT() == type) {
      retList.push_back(*succIt);
      cout << "Function " << isSgNode(*succIt)->unparseToString() << "\t"
           << *succIt << "\n";
    }

  return retList;
}

/*
Creates a list of nodes in Function evaluation order (FEO) (first eval function
expression, then args; for other nodes, it's BFS) For the previous example, if
f, g, and h were expressions (pointers) evaluating to functions, the order is:
f, g, h, h(i), g(_), f(_).
Note: This is not always equivalent to postorder (because of the BFS on nodes
other than function calls).
*/
list<SgNode *> FunctionCallNormalization::createTraversalList(SgNode *root) {
  list<SgNode *> retList;

  if (isSgFunctionCallExp(root)) {
    list<SgNode *> temp1 =
        createTraversalList(isSgFunctionCallExp(root)->get_function());
    list<SgNode *> temp2 =
        createTraversalList(isSgFunctionCallExp(root)->get_args());
    retList = temp1;
    retList.splice(retList.end(), temp2);
    retList.push_back(root);
  } else if (isSgNode(root)) {
    vector<SgNode *> succ = root->get_traversalSuccessorContainer();
    for (vector<SgNode *>::iterator succIt = succ.begin(); succIt != succ.end();
         succIt++)
      if (isSgNode(*succIt)) {
        list<SgNode *> temp1 = createTraversalList(*succIt);
        retList.splice(retList.end(), temp1);
      }
    retList.push_back(root);
  }
  return retList;
}
