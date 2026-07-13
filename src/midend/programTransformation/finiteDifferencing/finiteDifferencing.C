
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "AstConsistencyTests.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include "unparser.h"

#include "expressionTreeEqual.h"

#include "inlinerSupport.h"

#include "patternRewrite.h"

#include "pre.h"

#include "replaceExpressionWithStatement.h"

#include <iomanip>

#include <iostream>

#include <memory>

#include <vector>

#include "finiteDifferencing.h"

#undef FD_DEBUG

// DQ (8/1/2005): test use of new static function to create
// Sg_File_Info object that are marked as transformations
#undef SgNULL_FILE
#define SgNULL_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

using namespace std;

void FixSgTree(SgNode *);

namespace {
SgExpression *
finiteDifferencingOutermostImplicitConversion(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  SgExpression *result = expression;
  while (SgCastExp *cast = isSgCastExp(result->get_parent())) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    if (cast->get_operand() != result || result->get_parent() != cast) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[implicit-conversion]: "
              "expression=%p cast=%p has no exact transparent owner edge\n",
              static_cast<void *>(expression), static_cast<void *>(cast));
      ROSE_ABORT();
    }
    result = cast;
  }
  return result;
}
} // namespace

class FdFindCopiesVisitor : public AstSimpleProcessing {
  SgExpression *target;
  vector<SgExpression *> &copies;

public:
  FdFindCopiesVisitor(SgExpression *target, vector<SgExpression *> &copies)
      : target(target), copies(copies) {
#ifdef FD_DEBUG
    cout << "Looking for copies of " << target->unparseToString() << endl;
#endif
  }

  virtual void visit(SgNode *n) {
#ifdef FD_DEBUG
    cout << "FdFindCopiesVisitor visiting" << n->sage_class_name() << endl;
#endif
    if (isSgExpression(n) && expressionTreeEqual(isSgExpression(n), target)) {
#ifdef FD_DEBUG
      cout << "Found copy " << n->unparseToString() << endl;
#endif
      SgExpression *copy = isSgExpression(n);
      if (isSgVarRefExp(target)) {
        copy = finiteDifferencingOutermostImplicitConversion(copy);
      }
      if (std::find(copies.begin(), copies.end(), copy) == copies.end()) {
        copies.push_back(copy);
      }
    }
  }
};

SgExpression *copyFiniteDifferencingValueOccurrence(SgExpression *source,
                                                    SgNode *root) {
  vector<SgExpression *> occurrences;
  FdFindCopiesVisitor(source, occurrences).traverse(root, preorder);
  if (occurrences.empty()) {
    fprintf(stderr,
            "REX_FINITE_DIFFERENCING_INVARIANT[value-occurrence]: source=%p/%s "
            "has no exact typed occurrence in root=%p/%s\n",
            static_cast<void *>(source), source->class_name().c_str(),
            static_cast<void *>(root), root->class_name().c_str());
    ROSE_ABORT();
  }
  SgExpression *copy =
      isSgExpression(SageInterface::deepCopyNode(occurrences.front()));
  if (copy == nullptr || copy->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_FINITE_DIFFERENCING_INVARIANT[value-occurrence]: copied "
            "typed value is not one detached expression\n");
    ROSE_ABORT();
  }
  return copy;
}

void replaceCopiesOfExpression(SgExpression *src, SgExpression *tgt,
                               SgNode *root) {
#ifdef FD_DEBUG
  cout << "replaceCopiesOfExpression: src = " << src->unparseToString()
       << ", tgt = " << tgt->unparseToString()
       << ", root = " << root->unparseToString() << endl;
#endif
  vector<SgExpression *> copies_of_src;
#ifdef FD_DEBUG
  cout << "---" << endl;
#endif
  FdFindCopiesVisitor(src, copies_of_src).traverse(root, preorder);
#ifdef FD_DEBUG
  cout << copies_of_src.size() << " copy(ies) found." << endl;
#endif
  for (unsigned int i = 0; i < copies_of_src.size(); ++i) {
    SgTreeCopy tc;
    SgExpression *copy = isSgExpression(tgt->copy(tc));
    SgExpression *replaced = copies_of_src[i];
    SgExpression *parent = isSgExpression(replaced->get_parent());
    assert(parent);
    parent->replace_expression(replaced, copy);
    copy->set_parent(parent);
    const SgNodePtrList ownedChildren =
        parent->get_traversalSuccessorContainer();
    if (std::count(ownedChildren.begin(), ownedChildren.end(), replaced) != 0 ||
        std::count(ownedChildren.begin(), ownedChildren.end(), copy) != 1) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[expression-replacement]: "
              "parent=%p/%s did not replace source=%p with copy=%p exactly "
              "once\n",
              static_cast<void *>(parent), parent->class_name().c_str(),
              static_cast<void *>(replaced), static_cast<void *>(copy));
      ROSE_ABORT();
    }
    replaced->set_parent(nullptr);
  }
#ifdef FD_DEBUG
  cout << "result is " << root->unparseToString() << endl;
#endif
}

class FdFindModifyingStatementsVisitor : public AstSimpleProcessing {
  const vector<SgVariableSymbol *> &syms;
  vector<SgExpression *> &mods;
  vector<SgAssignInitializer *> initializersToSplit;

public:
  FdFindModifyingStatementsVisitor(const vector<SgVariableSymbol *> &syms,
                                   vector<SgExpression *> &mods)
      : syms(syms), mods(mods) {}

  void go(SgNode *root) {
    // Not thread safe or reentrant
    initializersToSplit.clear();
    traverse(root, postorder);
    for (unsigned int i = 0; i < initializersToSplit.size(); ++i) {
      SgAssignOp *assignment =
          convertInitializerIntoAssignment(initializersToSplit[i]);
      assert(assignment->variantT() == V_SgAssignOp);
      mods.push_back(assignment);
    }
  }

  virtual void visit(SgNode *n) {
    switch (n->variantT()) {
    case V_SgAssignOp:
    case V_SgPlusAssignOp:
    case V_SgMinusAssignOp:
    case V_SgAndAssignOp:
    case V_SgIorAssignOp:
    case V_SgMultAssignOp:
    case V_SgDivAssignOp:
    case V_SgModAssignOp:
    case V_SgXorAssignOp:
    case V_SgLshiftAssignOp:
    case V_SgRshiftAssignOp:
      if (legacy::anyOfListPotentiallyModifiedIn(syms, n))
        mods.push_back(isSgExpression(n));
      break;

    case V_SgPlusPlusOp:
    case V_SgMinusMinusOp:
      if (legacy::anyOfListPotentiallyModifiedIn(syms, n))
        mods.push_back(isSgExpression(n));
      break;

    case V_SgAssignInitializer: {
      SgAssignInitializer *init = isSgAssignInitializer(n);
      assert(init);
      for (unsigned int i = 0; i < syms.size(); ++i) {
        SgInitializedName *initname = syms[i]->get_declaration();
        if (init->get_parent() == initname ||
            init->get_parent()->get_parent() == initname) {
          initializersToSplit.push_back(init);
          break;
        }
      }
    } break;

    default:
      break;
    }
  }
};

SgExpression *doFdVariableUpdate(
    RewriteRule *rules, SgExpression *cache,
    SgExpression *old_val /* Cannot be referenced in output tree w/o copying */,
    SgExpression *new_val) {
#ifdef FD_DEBUG
  cout << "Trying to convert from " << old_val->unparseToString() << " to "
       << new_val->unparseToString() << ", using cache "
       << cache->unparseToString() << endl;
#endif
  SgTreeCopy tc;
  SgExpression *old_valCopy = isSgExpression(old_val->copy(tc));
  ROSE_ASSERT(old_valCopy);
  SgCommaOpExp *innerComma =
      new SgCommaOpExp(SgNULL_FILE, old_valCopy, new_val, new_val->get_type());
  innerComma->set_endOfConstruct(SgNULL_FILE);
  old_valCopy->set_parent(innerComma);
  new_val->set_parent(innerComma);
  SgExpression *expr =
      new SgCommaOpExp(SgNULL_FILE, cache, innerComma, innerComma->get_type());
  expr->set_endOfConstruct(SgNULL_FILE);
  cache->set_parent(expr);
  innerComma->set_parent(expr);
  // This is done so rewrite's expression replacement code will never find a
  // NULL parent for the expression being replaced
  SgExprStatement *dummyExprStatement = new SgExprStatement(SgNULL_FILE, expr);
  dummyExprStatement->set_endOfConstruct(SgNULL_FILE);
  expr->set_parent(dummyExprStatement);
  SgNode *exprCopyForRewrite = expr;
  rewrite(rules, exprCopyForRewrite); // This might modify exprCopyForRewrite
  ROSE_ASSERT(isSgExpression(exprCopyForRewrite));
  expr = isSgExpression(exprCopyForRewrite);
  expr->set_parent(NULL);
  dummyExprStatement->set_expression(NULL);
  delete dummyExprStatement;
  SgExpression *expr2 = expr;
  ROSE_ASSERT(expr2);
  if ( // The rewrite rules may have changed the form of expr to something other
       // than a comma pair
      isSgCommaOpExp(expr2) &&
      isSgCommaOpExp(isSgCommaOpExp(expr2)->get_rhs_operand())) {
    SgExpression *cache2 = isSgCommaOpExp(expr2)->get_lhs_operand();
    SgCommaOpExp *rhs =
        isSgCommaOpExp(isSgCommaOpExp(expr2)->get_rhs_operand());
    // SgExpression* old_val2 = rhs->get_lhs_operand();
    SgExpression *new_val2 = rhs->get_rhs_operand();
    // return new SgAssignOp(SgNULL_FILE, cache2, new_val2);
    cache2->set_lvalue(true);
    SgAssignOp *assignmentOperator =
        new SgAssignOp(SgNULL_FILE, cache2, new_val2, cache2->get_type());
    assignmentOperator->set_endOfConstruct(SgNULL_FILE);
    cache2->set_parent(assignmentOperator);
    new_val2->set_parent(assignmentOperator);

#ifdef FD_DEBUG
    printf("In doFdVariableUpdate(): assignmentOperator = %p \n",
           assignmentOperator);
#endif

    return assignmentOperator;
  } else {
    return expr2;
  }
}

// Do finite differencing on one expression within one context.  The expression
// must be defined and valid within the entire body of root.  The rewrite rules
// are used to simplify expressions.  When a variable var is updated from
// old_value to new_value, an expression of the form (var, (old_value,
// new_value)) is created and rewritten.  The rewrite rules may either produce
// an arbitrary expression (which will be used as-is) or one of the form (var,
// (something, value)) (which will be changed to (var = value)).
void doFiniteDifferencingOne(SgExpression *e, SgBasicBlock *root,
                             RewriteRule *rules) {
  SgStatementPtrList &root_stmts = root->get_statements();
  SgStatementPtrList::iterator i;
  for (i = root_stmts.begin(); i != root_stmts.end(); ++i) {
    if (legacy::expressionComputedIn(e, *i))
      break;
  }
  if (i == root_stmts.end())
    return; // Expression is not used within root, so quit
  vector<SgVariableSymbol *> used_symbols =
      SageInterface::getSymbolsUsedInExpression(e);
  SgName cachename = "cache_fd__";
  cachename << ++SageInterface::gensym_counter;
  SgVariableDeclaration *cachedecl = SageBuilder::buildVariableDeclaration(
      cachename, e->get_type(),
      nullptr /* the exact initializer is published below */, root);
  SgInitializedName *cachevar = cachedecl->get_variables().back();
  ROSE_ASSERT(cachevar);
  cachevar->set_scope(root);
  SageInterface::insertStatementBefore(*i, cachedecl, false);
  SgVariableSymbol *sym = root->lookup_variable_symbol(cachename);
  if (sym == NULL || sym->get_declaration() != cachevar) {
    fprintf(stderr,
            "REX_AST_INVARIANT[finite-differencing-cache-owner]: generated "
            "cache declaration has no exact published symbol\n");
    ROSE_ABORT();
  }
  SgVarRefExp *vr = new SgVarRefExp(SgNULL_FILE, sym);
  vr->set_endOfConstruct(SgNULL_FILE);
  replaceCopiesOfExpression(e, vr, root);

  vector<SgExpression *> modifications_to_used_symbols;
  FdFindModifyingStatementsVisitor(used_symbols, modifications_to_used_symbols)
      .go(root);

  PreprocessingInfo *cacheComment = new PreprocessingInfo(
      PreprocessingInfo::CplusplusStyleComment,
      (string("// Finite differencing: ") + cachename.str() +
       " is an exact generated expression cache")
          .c_str(),
      "Compiler-Generated in Finite Differencing", 0, 0, 0,
      PreprocessingInfo::before);
  ROSE_ASSERT(cacheComment->get_file_info() != NULL);
  cacheComment->get_file_info()->set_physical_file_id(
      Sg_File_Info::NULL_FILE_ID);
  SageInterface::publishGeneratedPreprocessingInfo(cacheComment, cachedecl);
  cachedecl->addToAttachedPreprocessingInfo(cacheComment);

  if (modifications_to_used_symbols.size() == 0) {
    SgInitializer *cacheinit =
        new SgAssignInitializer(SgNULL_FILE, e, cachevar->get_type());
    cacheinit->set_endOfConstruct(SgNULL_FILE);
    e->set_parent(cacheinit);
    cachevar->set_initializer(cacheinit);
    cacheinit->set_parent(cachevar);
  } else {
    for (unsigned int i = 0; i < modifications_to_used_symbols.size(); ++i) {
      SgExpression *modstmt = modifications_to_used_symbols[i];
#ifdef FD_DEBUG
      cout << "Updating cache after " << modstmt->unparseToString() << endl;
#endif
      SgExpression *updateCache = 0;
      SgVarRefExp *varref = new SgVarRefExp(SgNULL_FILE, sym);
      varref->set_endOfConstruct(SgNULL_FILE);
      SgTreeCopy tc;
      SgExpression *eCopy = isSgExpression(e->copy(tc));
      switch (modstmt->variantT()) {
      case V_SgAssignOp: {
        SgAssignOp *assignment = isSgAssignOp(modstmt);
        assert(assignment);
        SgExpression *lhs = assignment->get_lhs_operand();
        SgExpression *rhs = assignment->get_rhs_operand();
        replaceCopiesOfExpression(lhs, rhs, eCopy);
      } break;

      case V_SgPlusAssignOp:
      case V_SgMinusAssignOp:
      case V_SgAndAssignOp:
      case V_SgIorAssignOp:
      case V_SgMultAssignOp:
      case V_SgDivAssignOp:
      case V_SgModAssignOp:
      case V_SgXorAssignOp:
      case V_SgLshiftAssignOp:
      case V_SgRshiftAssignOp: {
        SgBinaryOp *assignment = isSgBinaryOp(modstmt);
        assert(assignment);
        SgExpression *lhs = assignment->get_lhs_operand();
        SgExpression *rhs = assignment->get_rhs_operand();
        SgExpression *lhsCopy =
            copyFiniteDifferencingValueOccurrence(lhs, eCopy);
        SgExpression *rhsCopy =
            isSgExpression(SageInterface::deepCopyNode(rhs));
        ROSE_ASSERT(lhsCopy != nullptr);
        ROSE_ASSERT(rhsCopy != nullptr);
        ROSE_ASSERT(lhsCopy->get_parent() == nullptr);
        ROSE_ASSERT(rhsCopy->get_parent() == nullptr);
        SgExpression *newval = 0;
        switch (modstmt->variantT()) {
#define DO_OP(op, nonassignment)                                               \
  case V_##op: {                                                               \
    newval = new nonassignment(SgNULL_FILE, lhsCopy, rhsCopy,                  \
                               assignment->get_type());                        \
    newval->set_endOfConstruct(SgNULL_FILE);                                   \
    lhsCopy->set_parent(newval);                                               \
    rhsCopy->set_parent(newval);                                               \
  } break

          DO_OP(SgPlusAssignOp, SgAddOp);
          DO_OP(SgMinusAssignOp, SgSubtractOp);
          DO_OP(SgAndAssignOp, SgBitAndOp);
          DO_OP(SgIorAssignOp, SgBitOrOp);
          DO_OP(SgMultAssignOp, SgMultiplyOp);
          DO_OP(SgDivAssignOp, SgDivideOp);
          DO_OP(SgModAssignOp, SgModOp);
          DO_OP(SgXorAssignOp, SgBitXorOp);
          DO_OP(SgLshiftAssignOp, SgLshiftOp);
          DO_OP(SgRshiftAssignOp, SgRshiftOp);
#undef DO_OP

        default:
          break;
        }
        assert(newval);
        replaceCopiesOfExpression(lhs, newval, eCopy);
      } break;

      case V_SgPlusPlusOp: {
        SgExpression *lhs = isSgPlusPlusOp(modstmt)->get_operand();
        SgExpression *lhsCopy =
            copyFiniteDifferencingValueOccurrence(lhs, eCopy);
        ROSE_ASSERT(lhsCopy != nullptr);
        ROSE_ASSERT(lhsCopy->get_parent() == nullptr);
        SgIntVal *one = new SgIntVal(SgNULL_FILE, 1);
        one->set_endOfConstruct(SgNULL_FILE);
        one->set_literal_spelling_form(
            SgValueExp::e_literal_canonical_generated);
        SgAddOp *add =
            new SgAddOp(SgNULL_FILE, lhsCopy, one, modstmt->get_type());
        add->set_endOfConstruct(SgNULL_FILE);
        lhsCopy->set_parent(add);
        one->set_parent(add);
        replaceCopiesOfExpression(lhs, add, eCopy);
      } break;

      case V_SgMinusMinusOp: {
        SgExpression *lhs = isSgMinusMinusOp(modstmt)->get_operand();
        SgExpression *lhsCopy =
            copyFiniteDifferencingValueOccurrence(lhs, eCopy);
        ROSE_ASSERT(lhsCopy != nullptr);
        ROSE_ASSERT(lhsCopy->get_parent() == nullptr);
        SgIntVal *one = new SgIntVal(SgNULL_FILE, 1);
        one->set_endOfConstruct(SgNULL_FILE);
        one->set_literal_spelling_form(
            SgValueExp::e_literal_canonical_generated);
        SgSubtractOp *sub =
            new SgSubtractOp(SgNULL_FILE, lhsCopy, one, modstmt->get_type());
        sub->set_endOfConstruct(SgNULL_FILE);
        lhsCopy->set_parent(sub);
        one->set_parent(sub);
        replaceCopiesOfExpression(lhs, sub, eCopy);
      } break;

      default:
        cerr << modstmt->sage_class_name() << endl;
        ROSE_ABORT();
        break;
      }

#ifdef FD_DEBUG
      cout << "e is " << e->unparseToString() << endl;
      cout << "eCopy is " << eCopy->unparseToString() << endl;
#endif
      updateCache = doFdVariableUpdate(rules, varref, e, eCopy);
#ifdef FD_DEBUG
      cout << "updateCache is " << updateCache->unparseToString() << endl;
#endif
      if (updateCache) {
        ROSE_ASSERT(modstmt != NULL);
        SgNode *ifp = modstmt->get_parent();
        SgCommaOpExp *comma = new SgCommaOpExp(SgNULL_FILE, updateCache,
                                               modstmt, modstmt->get_type());
        comma->set_endOfConstruct(SgNULL_FILE);
        modstmt->set_parent(comma);
        updateCache->set_parent(comma);

        if (ifp == NULL) {
          printf("modstmt->get_parent() == NULL modstmt = %p = %s \n", modstmt,
                 modstmt->class_name().c_str());
          modstmt->get_startOfConstruct()->display(
              "modstmt->get_parent() == NULL: debug");
        }
        ROSE_ASSERT(ifp != NULL);
#ifdef FD_DEBUG
        cout << "New expression is " << comma->unparseToString() << endl;
        cout << "IFP is " << ifp->sage_class_name() << ": "
             << ifp->unparseToString() << endl;
#endif
        if (isSgExpression(ifp)) {
          isSgExpression(ifp)->replace_expression(modstmt, comma);
          comma->set_parent(ifp);
        } else {
          // DQ (12/16/2006): Need to handle cases that are not SgExpression
          // (now that SgExpressionRoot is not used!) cerr <<
          // ifp->sage_class_name() << endl; assert (!"Bad parent type for
          // inserting comma expression");
          SgStatement *statement = isSgStatement(ifp);
          if (statement != NULL) {
#ifdef FD_DEBUG
            printf("Before statement->replace_expression(): statement = %p = "
                   "%s modstmt = %p = %s \n",
                   statement, statement->class_name().c_str(), modstmt,
                   modstmt->class_name().c_str());
            SgExprStatement *expresionStatement = isSgExprStatement(statement);
            if (expresionStatement != NULL) {
              SgExpression *expression = expresionStatement->get_expression();
              printf("expressionStatement expression = %p = %s \n", expression,
                     expression->class_name().c_str());
            }
#endif
            statement->replace_expression(modstmt, comma);
            comma->set_parent(statement);
          } else {
            ROSE_ASSERT(ifp != NULL);
            printf("Error: parent is neither a SgExpression nor a SgStatement "
                   "ifp = %p = %s \n",
                   ifp, ifp->class_name().c_str());
            ROSE_ABORT();
          }
        }

#ifdef FD_DEBUG
        cout << "IFP is now " << ifp->unparseToString() << endl;
#endif
      }
    }
  }
}

// Propagate definitions of a variable to its uses.
// Assumptions: var is only assigned at the top level of body
//              nothing var depends on is assigned within body
// Very simple algorithm designed to only handle simplest cases
void simpleUndoFiniteDifferencingOne(SgBasicBlock *body, SgExpression *var) {
  SgExpression *value = 0;
  SgStatementPtrList &stmts = body->get_statements();
  vector<SgStatement *> stmts_to_remove;

  for (SgStatementPtrList::iterator i = stmts.begin(); i != stmts.end(); ++i) {
    // cout << "Next statement: value = " << (value ? value->unparseToString() :
    // "(null)") << endl; cout << (*i)->unparseToString() << endl;
    if (isSgExprStatement(*i) &&
        isSgAssignOp(isSgExprStatement(*i)->get_expression())) {
      SgAssignOp *assignment =
          isSgAssignOp(isSgExprStatement(*i)->get_expression());
      // cout << "In assignment statement " << assignment->unparseToString() <<
      // endl;
      if (value)
        replaceCopiesOfExpression(var, value, assignment->get_rhs_operand());
      if (isSgVarRefExp(assignment->get_lhs_operand()) && isSgVarRefExp(var)) {
        SgVarRefExp *vr = isSgVarRefExp(assignment->get_lhs_operand());
        if (vr->get_symbol()->get_declaration() ==
            isSgVarRefExp(var)->get_symbol()->get_declaration()) {
          value = assignment->get_rhs_operand();
          stmts_to_remove.push_back(*i);
        }
      }
    } else {
      if (value)
        replaceCopiesOfExpression(var, value, *i);
    }
  }

  for (vector<SgStatement *>::iterator i = stmts_to_remove.begin();
       i != stmts_to_remove.end(); ++i) {
    stmts.erase(std::find(stmts.begin(), stmts.end(), *i));
  }

  if (value) {
    // DQ (12/17/2006): Separate out the construction of the SgAssignOp from the
    // SgExprStatement to support debugging and testing. stmts.push_back(new
    // SgExprStatement(SgNULL_FILE, new SgAssignOp(SgNULL_FILE, var, value)));
    var->set_lvalue(true);
    SgAssignOp *assignmentOperator =
        new SgAssignOp(SgNULL_FILE, var, value, var->get_type());
    assignmentOperator->set_endOfConstruct(SgNULL_FILE);
    var->set_parent(assignmentOperator);
    value->set_parent(assignmentOperator);

    printf("In simpleUndoFiniteDifferencingOne(): assignmentOperator = %p \n",
           assignmentOperator);

    // DQ: Note that the parent of the SgExprStatement will be set in AST
    // post-processing (or it should be).
    SgExprStatement *es = new SgExprStatement(SgNULL_FILE, assignmentOperator);
    es->set_endOfConstruct(SgNULL_FILE);
    assignmentOperator->set_parent(es);
    stmts.push_back(es);
    es->set_parent(body);
  }
}

class FindForStatementsVisitor : public AstSimpleProcessing {
  vector<SgForStatement *> &stmts;

public:
  FindForStatementsVisitor(vector<SgForStatement *> &stmts) : stmts(stmts) {}

  virtual void visit(SgNode *n) {
    if (isSgForStatement(n))
      stmts.push_back(isSgForStatement(n));
  }
};

// Move variables declared in a for statement to just outside that statement.
void moveForDeclaredVariables(SgNode *root) {
  vector<SgForStatement *> for_statements;
  FindForStatementsVisitor(for_statements).traverse(root, preorder);

  for (unsigned int i = 0; i < for_statements.size(); ++i) {
    SgForStatement *stmt = for_statements[i];
#ifdef FD_DEBUG
    cout << "moveForDeclaredVariables: " << stmt->unparseToString() << endl;
#endif
    SgForInitStatement *init = stmt->get_for_init_stmt();
    if (!init)
      continue;
    SgStatementPtrList &inits = init->get_init_stmt();
    vector<SgVariableDeclaration *> decls;
    for (SgStatementPtrList::iterator j = inits.begin(); j != inits.end();
         ++j) {
      SgStatement *one_init = *j;
      if (isSgVariableDeclaration(one_init)) {
        decls.push_back(isSgVariableDeclaration(one_init));
      }
    }
    if (decls.empty())
      continue;
    SgStatement *parent = isSgStatement(stmt->get_parent());
    assert(parent);
    SgBasicBlock *bb = SageBuilder::buildBasicBlock_nfi();
    SageInterface::setSourcePositionForTransformation(bb);
    SgStatementPtrList ls;
    for (unsigned int j = 0; j < decls.size(); ++j) {
      for (SgInitializedNamePtrList::iterator k =
               decls[j]->get_variables().begin();
           k != decls[j]->get_variables().end(); ++k) {
#ifdef FD_DEBUG
        cout << "Working on variable " << (*k)->get_name().getString() << endl;
#endif
        SgVariableSymbol *sym = new SgVariableSymbol(*k);
        bb->insert_symbol((*k)->get_name(), sym);
        (*k)->set_scope(bb);
        SgAssignInitializer *kinit = 0;
        if (isSgAssignInitializer((*k)->get_initializer())) {
          kinit = isSgAssignInitializer((*k)->get_initializer());
          (*k)->set_initializer(0);
        }

        if (kinit) {
          SgVarRefExp *vr = new SgVarRefExp(SgNULL_FILE, sym);
          vr->set_endOfConstruct(SgNULL_FILE);
          SageInterface::setSourcePositionForTransformation(vr);
          vr->set_lvalue(true);
          SgAssignOp *assignment = new SgAssignOp(
              SgNULL_FILE, vr, kinit->get_operand(), vr->get_type());
          assignment->set_endOfConstruct(SgNULL_FILE);
          SageInterface::setSourcePositionForTransformation(assignment);
          vr->set_parent(assignment);
          kinit->get_operand()->set_parent(assignment);
          SgExprStatement *expr = new SgExprStatement(SgNULL_FILE, assignment);
          expr->set_endOfConstruct(SgNULL_FILE);
          SageInterface::setSourcePositionForTransformation(expr);
          assignment->set_parent(expr);
          ls.push_back(expr);
          expr->set_parent(init);
        }
      }
    }
    inits = ls;
    // Replace the original loop while its physical parent relation is still
    // intact, then move the declarations and loop into the new transformation
    // block.  Reparenting the loop before replacement left a transiently
    // malformed tree that legacy fixup happened to repair only in some cases.
    parent->replace_statement(stmt, bb);
    if (bb->get_parent() != parent) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[loop-wrapper]: parent=%p/%s "
              "loop=%p replacement=%p was not installed exactly once\n",
              static_cast<void *>(parent), parent->class_name().c_str(),
              static_cast<void *>(stmt), static_cast<void *>(bb));
      ROSE_ABORT();
    }
    for (SgVariableDeclaration *declaration : decls) {
      bb->append_statement(declaration);
    }
    bb->append_statement(stmt);
    // The original loop retains the exact source-file identity for the
    // replaced surface.  Publish that identity onto the generated wrapper and
    // declarations as one explicit construction transaction; transformation
    // file information alone is deliberately not an output-owner fallback.
    SageInterface::publishGeneratedSubtreeOutputOwner(bb, stmt);
    if (stmt->get_parent() != bb) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[loop-wrapper]: loop=%p has "
              "no exact generated block owner\n",
              static_cast<void *>(stmt));
      ROSE_ABORT();
    }
  }
}

class FdFindFunctionsVisitor : public AstSimpleProcessing {
public:
  vector<SgFunctionDefinition *> functions;

  virtual void visit(SgNode *n) {
    if (isSgFunctionDefinition(n)) {
      functions.push_back(isSgFunctionDefinition(n));
    }
  }
};

class FdFindInitnamesVisitor : public AstSimpleProcessing {
public:
  vector<SgInitializedName *> initnames;

  virtual void visit(SgNode *n) {
#ifdef FD_DEBUG
    cout << "FdFindInitnamesVisitor " << n->class_name() << " " << n << endl;
#endif
    if (isSgInitializedName(n)) {
#ifdef FD_DEBUG
      cout << "Init name " << isSgInitializedName(n)->get_name().getString()
           << endl;
#endif
      initnames.push_back(isSgInitializedName(n));
    }
#ifdef FD_DEBUG
    if (isSgVarRefExp(n)) {
      cout << "Var ref "
           << isSgVarRefExp(n)->get_symbol()->get_name().getString() << endl;
    }
#endif
  }
};

class FdFindMultiplicationsVisitor : public AstSimpleProcessing {
public:
  vector<SgMultiplyOp *> exprs;

  virtual void visit(SgNode *n) {
    if (isSgMultiplyOp(n)) {
      exprs.push_back(isSgMultiplyOp(n));
    }
  }
};

namespace {
SgExpression *finiteDifferencingSemanticOperand(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  while (SgCastExp *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    SgExpression *operand = cast->get_operand();
    if (operand == nullptr || operand->get_parent() != cast) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[implicit-conversion]: "
              "cast=%p has no exact operand ownership\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }
    expression = operand;
  }
  return expression;
}

SgNode *finiteDifferencingSemanticParent(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  SgNode *current = expression;
  SgNode *parent = current->get_parent();
  while (SgCastExp *cast = isSgCastExp(parent)) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    if (cast->get_operand() != current || current->get_parent() != cast) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[implicit-conversion]: "
              "expression=%p cast=%p has no exact transparent parent edge\n",
              static_cast<void *>(expression), static_cast<void *>(cast));
      ROSE_ABORT();
    }
    current = cast;
    parent = cast->get_parent();
  }
  return parent;
}
} // namespace

class IsModifiedBadlyVisitor : public AstSimpleProcessing {
  SgInitializedName *initname;
  bool &safe;

public:
  IsModifiedBadlyVisitor(SgInitializedName *initname, bool &safe)
      : initname(initname), safe(safe) {}

  virtual void visit(SgNode *n) {
    SgVarRefExp *vr = isSgVarRefExp(n);
    if (vr && vr->get_symbol()->get_declaration() == initname) {
      SgNode *semanticParent = finiteDifferencingSemanticParent(vr);
      ROSE_ASSERT(semanticParent != nullptr);
      switch (semanticParent->variantT()) {
      case V_SgReturnStmt:
      case V_SgExprStatement:
      case V_SgIfStmt:
      case V_SgWhileStmt:
      case V_SgDoWhileStmt:
      case V_SgSwitchStatement:
      case V_SgCaseOptionStmt:
      case V_SgForStatement:
      case V_SgForInitStatement:
#ifdef FD_DEBUG
        cout << "Statement: Variable " << initname->get_name().getString()
             << " is safe" << endl;
#endif
        // Safe
        break;

      case V_SgPlusPlusOp:
      case V_SgMinusMinusOp:
#ifdef FD_DEBUG
        cout << "Inc/dec: Variable " << initname->get_name().getString()
             << " is safe" << endl;
#endif
        // Safe
        break;

      case V_SgAddOp:
      case V_SgSubtractOp:
      case V_SgMinusOp:
      case V_SgUnaryAddOp:
      case V_SgNotOp:
      case V_SgPointerDerefExp:
      case V_SgBitComplementOp:
      case V_SgThrowOp:
      case V_SgEqualityOp:
      case V_SgLessThanOp:
      case V_SgLessOrEqualOp:
      case V_SgGreaterThanOp:
      case V_SgGreaterOrEqualOp:
      case V_SgNotEqualOp:
      case V_SgMultiplyOp:
      case V_SgDivideOp:
      case V_SgIntegerDivideOp:
      case V_SgModOp:
      case V_SgAndOp:
      case V_SgOrOp:
      case V_SgBitAndOp:
      case V_SgBitOrOp:
      case V_SgBitXorOp:
      case V_SgCommaOpExp:
      case V_SgLshiftOp:
      case V_SgRshiftOp:
      case V_SgAssignInitializer:
#ifdef FD_DEBUG
        cout << "Non-mutating: Variable " << initname->get_name().getString()
             << " is safe" << endl;
#endif
        // Safe
        break;

      case V_SgExprListExp:
        if (isSgFunctionCallExp(semanticParent->get_parent())) {
          if (isPotentiallyModified(vr, semanticParent->get_parent())) {
#ifdef FD_DEBUG
            cout << "Function call: Variable "
                 << initname->get_name().getString() << " is unsafe" << endl;
#endif
            safe = false;
          }
        } else if (isSgConstructorInitializer(semanticParent->get_parent())) {
#ifdef FD_DEBUG
          cout << "Constructor: Variable " << initname->get_name().getString()
               << " is unsafe" << endl;
#endif
          safe = false;
          // FIXME: constructors
        } else {
          cerr << semanticParent->get_parent()->sage_class_name() << endl;
          assert(!"Unknown SgExprListExp case");
        }
        break;

      case V_SgAssignOp:
      case V_SgPlusAssignOp:
      case V_SgMinusAssignOp: {
        SgBinaryOp *binop = isSgBinaryOp(semanticParent);
        ROSE_ASSERT(binop != nullptr);
        SgExpression *rhs =
            finiteDifferencingSemanticOperand(binop->get_rhs_operand());
        bool lhs_good =
            finiteDifferencingSemanticOperand(binop->get_lhs_operand()) == vr;
#ifdef FD_DEBUG
        cout << "Assign case for " << initname->get_name().getString() << endl;
        cout << "lhs_good = " << (lhs_good ? "true" : "false") << endl;
#endif
        SgAddOp *rhs_a = isSgAddOp(rhs);
        SgExpression *rhs_a_lhs =
            rhs_a ? finiteDifferencingSemanticOperand(rhs_a->get_lhs_operand())
                  : nullptr;
        SgExpression *rhs_a_rhs =
            rhs_a ? finiteDifferencingSemanticOperand(rhs_a->get_rhs_operand())
                  : nullptr;
        if (lhs_good) {
          if (isSgValueExp(rhs)) {
            // Safe
          } else if (isSgVarRefExp(rhs)) {
            // Safe
          } else if (isSgAssignOp(binop) && rhs_a &&
                     ((isSgVarRefExp(rhs_a_lhs) &&
                       isSgVarRefExp(rhs_a_lhs)
                               ->get_symbol()
                               ->get_declaration() == initname) ||
                      (isSgVarRefExp(rhs_a_rhs) &&
                       isSgVarRefExp(rhs_a_rhs)
                               ->get_symbol()
                               ->get_declaration() == initname))) {
            // Safe
          } else {
#ifdef FD_DEBUG
            cout << "Assign: Variable " << initname->get_name().str()
                 << " is unsafe because of " << binop->unparseToString() << ": "
                 << binop->get_rhs_operand()->sage_class_name() << endl;
#endif
            safe = false;
          }
        } else {
          // Safe: RHS of assignment
        }
      } break;

      default: {
#ifdef FD_DEBUG
        cout << "Default: Variable " << initname->get_name().str()
             << " is unsafe because of " << semanticParent->unparseToString()
             << ": " << semanticParent->sage_class_name() << endl;
#endif
        safe = false;
      } break;
      }
    }
  }
};

// Do a simple form of finite differencing on all functions contained within
// root (which should be a project, file, or function definition).
void simpleIndexFiniteDifferencing(SgNode *root) {
  moveForDeclaredVariables(root);
  SgNode *proj = root;
  while (proj && !isSgProject(proj))
    proj = proj->get_parent();
  ROSE_ASSERT(proj);
  AstTests::runAllTests(isSgProject(proj));
  std::unique_ptr<RewriteRule> algebraicRules(getAlgebraicRules());
  rewrite(algebraicRules.get(), root); // This might modify root
  FdFindFunctionsVisitor ffv;
  ffv.traverse(root, preorder);
  for (unsigned int x = 0; x < ffv.functions.size(); ++x) {
#ifdef FD_DEBUG
    cout << "Working on function "
         << ffv.functions[x]->get_declaration()->get_name().str() << endl;
#endif
    SgBasicBlock *body = ffv.functions[x]->get_body();
    FdFindInitnamesVisitor fiv;
    fiv.traverse(ffv.functions[x]->get_declaration(), preorder);
    vector<SgInitializedName *> initnames = fiv.initnames;
    set<SgInitializedName *> safe_vars;
    for (vector<SgInitializedName *>::iterator i = initnames.begin();
         i != initnames.end(); ++i) {
      SgInitializedName *initname = *i;
#ifdef FD_DEBUG
      cout << "Found variable " << initname->get_name().str() << endl;
#endif
      bool safe = true;
      IsModifiedBadlyVisitor(initname, safe).traverse(body, preorder);
      if (safe)
        safe_vars.insert(initname);
#ifdef FD_DEBUG
      cout << "Variable " << initname->get_name().str() << " is "
           << (safe ? "" : "not ") << "safe" << endl;
#endif
    }
    FdFindMultiplicationsVisitor fmv;
    fmv.traverse(body, postorder);
    vector<SgMultiplyOp *> mult_exprs;
    for (unsigned int i = 0; i < fmv.exprs.size(); ++i) {
      bool alreadyProcessed = false;
      for (unsigned int j = 0; j < i; ++j) {
        if (expressionTreeEqual(fmv.exprs[i], fmv.exprs[j])) {
          alreadyProcessed = true;
          break;
        }
      }
      if (alreadyProcessed)
        continue;

#ifdef FD_DEBUG
      cout << "Testing expression " << fmv.exprs[i]->unparseToString()
           << " for possible FD" << endl;
#endif

      SgExpression *expr1 =
          finiteDifferencingSemanticOperand(fmv.exprs[i]->get_lhs_operand());
      SgExpression *expr2 =
          finiteDifferencingSemanticOperand(fmv.exprs[i]->get_rhs_operand());
      bool isConst1 = isSgValueExp(expr1);
      bool isSafeVar1 =
          isSgVarRefExp(expr1) &&
          safe_vars.find(
              isSgVarRefExp(expr1)->get_symbol()->get_declaration()) !=
              safe_vars.end();
      bool isGood1 = isConst1 || isSafeVar1;
      bool isConst2 = isSgValueExp(expr2);
      bool isSafeVar2 =
          isSgVarRefExp(expr2) &&
          safe_vars.find(
              isSgVarRefExp(expr2)->get_symbol()->get_declaration()) !=
              safe_vars.end();
      bool isGood2 = isConst2 || isSafeVar2;
#ifdef FD_DEBUG
      cout << boolalpha << "isGood1 = " << isGood1 << ", isGood2 = " << isGood2
           << endl;
#endif
      if (isGood1 && isGood2) {
#ifdef FD_DEBUG
        cout << "Expression is good to run FD on" << endl;
#endif
        mult_exprs.push_back(fmv.exprs[i]);
      } else {
#ifdef FD_DEBUG
        cout << "Expression is not good to run FD on" << endl;
#endif
      }
    }
    std::unique_ptr<RewriteRule> finiteDifferencingRules(
        getFiniteDifferencingRules());
    for (int i = mult_exprs.size() - 1; i >= 0; --i)
      doFiniteDifferencingOne(mult_exprs[i], body,
                              finiteDifferencingRules.get());

    SgNode *bodyCopyForRewrite = body;
    std::unique_ptr<RewriteRule> bodyAlgebraicRules(getAlgebraicRules());
    rewrite(bodyAlgebraicRules.get(),
            bodyCopyForRewrite); // This might update bodyCopyForRewrite
    ROSE_ASSERT(isSgBasicBlock(bodyCopyForRewrite));
    body = isSgBasicBlock(bodyCopyForRewrite);
  }
}
