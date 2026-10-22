#include "sage3basic.h"

#include "sageBuilder.h"

#undef SgNULL_FILE
#define SgNULL_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

#include "inlinerSupport.h"

using namespace std;

class FlattenBlocksVisitor : public AstSimpleProcessing {
public:
  void visit(SgNode *node) override {
    auto *target = isSgBasicBlock(node);
    if (target == nullptr)
      return;
    const SgStatementPtrList original = target->get_statements();
    for (SgStatement *statement : original) {
      auto *nested = isSgBasicBlock(statement);
      if (nested == nullptr)
        continue;
      ROSE_ASSERT(nested->get_parent() == target);
      // Semantic auxiliary declarations retain this lexical scope's identity.
      // Such a scope cannot be removed by a lexical-only flattening operation.
      if (auto *auxiliary = nested->get_auxiliary_declarations()) {
        auxiliary->validate_semantic_non_output_role();
        continue;
      }
      const auto &children = nested->get_statements();
      if (std::any_of(children.begin(), children.end(), [](SgStatement *child) {
            return isSgDeclarationStatement(child) != nullptr;
          }))
        continue;
      auto &statements = target->get_statements();
      auto position = std::find(statements.begin(), statements.end(), nested);
      ROSE_ASSERT(position != statements.end());
      const size_t index = position - statements.begin();
      const size_t oldSize = statements.size();
      const size_t movedSize = nested->get_statements().size();
      SageInterface::moveStatementsBetweenBlocks(nested, target);
      ROSE_ASSERT(nested->get_statements().empty());
      ROSE_ASSERT(statements.size() == oldSize + movedSize);
      // The transfer appends in source order. Put those already-owned children
      // at the removed block's position without changing their ownership again.
      std::rotate(statements.begin() + index + 1, statements.begin() + oldSize,
                  statements.end());
      SageInterface::removeStatement(nested);
      SageInterface::deleteAST(nested,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
  }
};

void flattenBlocks(SgNode *n) { FlattenBlocksVisitor().traverse(n, postorder); }

// Internal routine to check whether a given variable declaration is a
// member variable.
bool isMemberVariable(const SgNode &in) {
  SgNode *p = in.get_parent();
  if (isSgClassDefinition(p))
    return true;
  if (isSgGlobal(p))
    return false;
  return isMemberVariable(*p);
}

// Visitor to rename non-member variables.  Used internally.
class RenameVariablesVisitor : public AstSimpleProcessing {
  int counter;

public:
  RenameVariablesVisitor() : counter(0) {}

  virtual void visit(SgNode *n) {
    if (isSgFunctionDefinition(n)) {
      renameLabels(isSgFunctionDefinition(n), isSgFunctionDefinition(n));
    } else if (isSgInitializedName(n)) {
      SgInitializedName *n2 = isSgInitializedName(n);

      ASSERT_not_null(n2->get_file_info());

      if (isMemberVariable(*n2))
        return;

      if (isSgVariableDeclaration(n2->get_parent())) {
        SgVariableDeclaration *decl = isSgVariableDeclaration(n2->get_parent());
        if (isSgGlobal(decl->get_parent()))
          return;
        if (isSgNamespaceDefinitionStatement(decl->get_parent()))
          return;
      }

      if (isSgCtorInitializerList(n2->get_parent()))
        return;
      if (n2->get_name().getString() == "")
        return;
      SgName name(n2->get_name());
      SgSymbolTable *symtab = n2->get_scope()->get_symbol_table();
      auto *symbol = isSgVariableSymbol(symtab->find(n2));
      ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == n2);
      ROSE_ASSERT(symbol->get_parent() == symtab);
      name << "__" << counter++;
      // Reindex the same symbol so every existing reference keeps its exact
      // identity through the rename.
      symtab->remove(symbol);
      n2->set_name(name);
      symtab->insert(name, symbol);
      ROSE_ASSERT(symtab->find(n2) == symbol);
      ASSERT_not_null(n2->get_parent());
      ASSERT_not_null(n2->get_file_info());
    }
  }
};

// Rename all variables and labels in a block of code, and ensure that all
// variable and label references point to the correct declarations.
void renameVariables(SgNode *n) {
  std::vector<SgNode *> defs =
      NodeQuery::querySubTree(n, V_SgFunctionDefinition);
  for (size_t i = 0; i < defs.size(); ++i) {
    ASSERT_not_null(defs[i]->get_parent());
    RenameVariablesVisitor().traverse(defs[i]->get_parent(), preorder);
  }
}

// Visitor to rename all labels (and gotos to them) in a (possibly inlined)
// function definition
class RenameLabelsVisitor : public AstSimpleProcessing {
  static int labelRenameCounter;
  SgScopeStatement *newScope;
  SgSymbolTable *symtab;

public:
  RenameLabelsVisitor(SgScopeStatement *scope)
      : newScope(scope), symtab(scope->get_symbol_table()) {
    ROSE_ASSERT(symtab);
  }

  virtual void visit(SgNode *n) {
    SgLabelStatement *l = isSgLabelStatement(n);
    if (l) {
      SgName name = l->get_label();
      // The label is in some inner scope, and needs to be moved to the
      // enclosing function definition
      SgSymbolTable *st = l->get_scope()->get_symbol_table();
      ROSE_ASSERT(st);
      SgLabelSymbol *old_symbol = st->find_label(name);
      ROSE_ASSERT(old_symbol);
      if (old_symbol->get_parent() != st || !st->exists(old_symbol)) {
        fprintf(stderr,
                "REX_INLINER_INVARIANT[label-symbol-owner]: label=%p "
                "symbol=%p parent=%p expected-table=%p\n",
                static_cast<void *>(l), static_cast<void *>(old_symbol),
                static_cast<void *>(old_symbol->get_parent()),
                static_cast<void *>(st));
        ROSE_ABORT();
      }
      st->remove(old_symbol);
      name << "__" << ++labelRenameCounter;
      l->set_label(name);
      l->set_scope(newScope);
      if (symtab->find_label(name) != NULL) {
        fprintf(stderr,
                "REX_INLINER_INVARIANT[label-symbol-collision]: label=%p "
                "name='%s' target-table=%p\n",
                static_cast<void *>(l), name.str(),
                static_cast<void *>(symtab));
        ROSE_ABORT();
      }
      symtab->insert(name, old_symbol);
      if (old_symbol->get_parent() != symtab || !symtab->exists(old_symbol)) {
        fprintf(stderr,
                "REX_INLINER_INVARIANT[label-symbol-publication]: label=%p "
                "symbol=%p parent=%p expected-table=%p\n",
                static_cast<void *>(l), static_cast<void *>(old_symbol),
                static_cast<void *>(old_symbol->get_parent()),
                static_cast<void *>(symtab));
        ROSE_ABORT();
      }
    }
  }
};

int RenameLabelsVisitor::labelRenameCounter = 0;

// Rename all labels in a (possibly inlined) function definition.  Gotos to
// them will be automatically updated as part of unparsing.
void renameLabels(SgNode *fd,
                  SgFunctionDefinition *enclosingFunctionDefinition) {
  RenameLabelsVisitor(enclosingFunctionDefinition).traverse(fd, preorder);
}

// Remove all statements whose sole content is an integer constant.  Dummy
// statements like this are introduced by the inliner to ensure that a
// label is not the last statement in a block, but not all of them are
// necessary after unused labels are removed.
class RemoveNullStatementsVisitor : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n) {
    if (isSgBasicBlock(n)) {
      SgBasicBlock *bb = isSgBasicBlock(n);
      SgStatementPtrList &stmts = bb->get_statements();
      bool changes = true;
      while (changes) {
        changes = false;
        for (SgStatementPtrList::iterator i = stmts.begin(); i != stmts.end();
             ++i) {
          if (isSgExprStatement(*i)) {
            SgExpression *expr = isSgExprStatement(*i)->get_expression();
            if (isSgIntVal(expr) || isSgNullExpression(expr)) {
              SgStatementPtrList::iterator inext = i, iprev = i;
              ++inext;
              if (iprev != stmts.begin())
                --iprev;
              if ((inext != stmts.end() &&
                   !isSgDeclarationStatement(
                       *inext)) || // A label cannot precede a declaration
                  !isSgLabelStatement(*iprev)) {
                // Checking to be sure that this statement isn't ensuring
                // that a label isn't the last statement in a block (which
                // would be illegal)
                SageInterface::myRemoveStatement(*i);
                changes = true;
                break; // To avoid iterator invalidation
              }
            }
          }
        }
      }
    }
  }
};

// Count the number of references to a given variable (visitor).
class CountVariableReferencesVisitor : public AstSimpleProcessing {
  int &result;
  SgInitializedName *var;

public:
  CountVariableReferencesVisitor(int &result, SgInitializedName *var)
      : result(result), var(var) {}

  virtual void visit(SgNode *n) {
    if (isSgVarRefExp(n)) {
      assert(isSgVarRefExp(n)->get_symbol());
      if (isSgVarRefExp(n)->get_symbol()->get_declaration() == var)
        ++result;
    } else if (n == var) {
      ++result;
    }
  }
};

// Count references to a given variable
int countVariableReferences(SgNode *root, SgInitializedName *var) {
  int result = 0;
  CountVariableReferencesVisitor(result, var).traverse(root, preorder);
  return result;
}

// Does some code refer to a given variable?
bool containsVariableReference(SgNode *root, SgInitializedName *var) {
  return countVariableReferences(root, var) != 0;
}

// Does a given declaration declare a given variable?
bool isDeclarationOf(SgVariableDeclaration *decl, SgInitializedName *var) {
  SgInitializedNamePtrList &vars = decl->get_variables();
  for (SgInitializedNamePtrList::iterator i = vars.begin(); i != vars.end();
       ++i)
    if (*i == var)
      return true;
  return false;
}

// Within sc, is the variable toCheck modified between the declaration of
// lifetime and its last use?  This is used to determine whether, whenever
// toCheck and lifetime are equal, one can be used as a substitute for the
// other.
bool isPotentiallyModifiedDuringLifeOf(SgBasicBlock *sc,
                                       SgInitializedName *toCheck,
                                       SgInitializedName *lifetime) {
  auto *symbol = isSgVariableSymbol(toCheck->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == toCheck);
  SgStatementPtrList &stmts = sc->get_statements();
  bool inLiveRange = false;
  for (SgStatementPtrList::reverse_iterator i = stmts.rbegin();
       i != stmts.rend(); ++i) {
    if (containsVariableReference(*i, lifetime))
      inLiveRange = true;
    if (inLiveRange && isPotentiallyModified(symbol, *i))
      return true;

    if (isSgVariableDeclaration(*i) &&
        isDeclarationOf(isSgVariableDeclaration(*i), lifetime))
      return false; // This must be last
  }
  return false;
}

static bool canPropagateLambdaCaptureValue(const SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  for (const SgNode *child = expression; child->get_parent() != nullptr;
       child = child->get_parent()) {
    const auto *capture = isSgLambdaCapture(child->get_parent());
    if (capture == nullptr)
      continue;
    ROSE_ASSERT(capture->get_capture_variable() != nullptr &&
                capture->get_capture_variable()->get_parent() == capture);
    ROSE_ASSERT(capture->get_capture_initializer() != nullptr &&
                capture->get_capture_initializer()->get_parent() == capture);
    ROSE_ASSERT(capture->get_closure_variable() == nullptr ||
                capture->get_closure_variable()->get_parent() == capture);
    const unsigned matchingEdges =
        static_cast<unsigned>(capture->get_capture_variable() == child) +
        static_cast<unsigned>(capture->get_capture_initializer() == child) +
        static_cast<unsigned>(capture->get_closure_variable() == child);
    if (matchingEdges != 1) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[lambda-capture-expression-owner]: "
              "capture=%p variable=%p matches %u owned expression edges\n",
              static_cast<const void *>(capture),
              static_cast<const void *>(expression), matchingEdges);
      ROSE_ABORT();
    }
    return capture->get_is_init_capture() &&
           capture->get_capture_initializer() == child;
  }
  return true;
}

static void replaceLambdaCaptureInitializer(SgLambdaCapture *capture,
                                            SgVarRefExp *variable,
                                            SgExpression *replacement) {
  ROSE_ASSERT(capture != nullptr && variable != nullptr &&
              variable->get_parent() == capture);
  ROSE_ASSERT(replacement != nullptr && replacement->get_parent() == nullptr);
  if (capture->get_capture_initializer() != variable ||
      !capture->get_is_init_capture()) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[lambda-capture-expression-role]: "
            "capture=%p variable=%p attempted value replacement through a "
            "capture identity edge\n",
            static_cast<void *>(capture), static_cast<void *>(variable));
    ROSE_ABORT();
  }
  replacement->set_parent(capture);
  capture->set_capture_initializer(replacement);
  variable->set_parent(nullptr);
  if (replacement->get_parent() != capture ||
      capture->get_capture_initializer() != replacement) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[lambda-capture-expression-owner]: "
            "capture=%p did not adopt replacement=%p\n",
            static_cast<void *>(capture), static_cast<void *>(replacement));
    ROSE_ABORT();
  }
  SageInterface::deleteAST(variable,
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

// Find all variables which are set as references to other variables and
// replace uses of the reference variable with what it is a reference to.
class FindReferenceVariablesVisitor : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n) {
    if (isSgVarRefExp(n)) {
      SgVarRefExp *copy_vr = isSgVarRefExp(n);
      if (!canPropagateLambdaCaptureValue(copy_vr))
        return;
      assert(copy_vr->get_symbol());
      SgInitializedName *copy = copy_vr->get_symbol()->get_declaration();
      assert(copy);
      if (!SageInterface::isReferenceType(copy->get_type()))
        return; // Fail if non-reference

      SgInitializer *copyinit = copy->get_initializer();
      if (isSgAssignInitializer(copyinit)) {
        SgAssignInitializer *init = isSgAssignInitializer(copyinit);
        SgExpression *orig_expr = init->get_operand();
        bool shouldReplace = false;
        if (isSgVarRefExp(orig_expr)) {
          SgVarRefExp *orig_vr = isSgVarRefExp(orig_expr);
          SgInitializedName *orig = orig_vr->get_symbol()->get_declaration();
          assert(orig);
#ifndef NDEBUG
          SgNode *origscope = orig->get_parent()->get_parent();
          assert(origscope);
#endif
          shouldReplace = true;
        }
        if (shouldReplace) {
          assert(orig_expr);
          SgExpression *orig_copy =
              isSgExpression(SageInterface::deepCopy(orig_expr));
          ASSERT_not_null(orig_copy);
          orig_copy->set_lvalue(copy_vr->get_lvalue());
          if (auto *capture = isSgLambdaCapture(copy_vr->get_parent())) {
            replaceLambdaCaptureInitializer(capture, copy_vr, orig_copy);
          } else {
            ROSE_ASSERT(isSgExpression(copy_vr->get_parent()) != nullptr);
            SageInterface::replaceExpression(copy_vr, orig_copy);
          }
        }
      }
    }
  }
};

// Find all (variable, value) and (variable, variable) pairs to which a
// simple form of copy and constant propagation can be applied.
class FindCopiesVisitor : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n) {
    if (isSgVarRefExp(n)) {
      SgVarRefExp *copy_vr = isSgVarRefExp(n);
      if (!canPropagateLambdaCaptureValue(copy_vr))
        return;
      assert(copy_vr->get_symbol());
      SgInitializedName *copy = copy_vr->get_symbol()->get_declaration();
      assert(copy);
      if (SageInterface::isVolatileType(copy->get_type()))
        return;
      SgInitializer *copyinit = copy->get_initializer();
      SgScopeStatement *copyscope =
          SageInterface::getScope(copy->get_parent()->get_parent());
      if (isSgAssignInitializer(copyinit)) {
        SgAssignInitializer *init = isSgAssignInitializer(copyinit);
        SgExpression *orig_expr = init->get_operand();
        if (!isPotentiallyModified(copy_vr, copyscope) &&
            !isSgGlobal(copyscope) &&
            !isSgNamespaceDefinitionStatement(copyscope)) {
          bool shouldReplace = false;
          if (isSgVarRefExp(orig_expr)) {
            SgVarRefExp *orig_vr = isSgVarRefExp(orig_expr);
            SgInitializedName *orig = orig_vr->get_symbol()->get_declaration();
            assert(orig);
            if (SageInterface::isVolatileType(orig->get_type()))
              return;
            SgNode *origscope = orig->get_parent()->get_parent();
            assert(origscope);
            if (!hasAddressTaken(orig_vr, origscope) &&
                isSgBasicBlock(copyscope) &&
                !isPotentiallyModifiedDuringLifeOf(isSgBasicBlock(copyscope),
                                                   orig, copy) &&
                !isSgGlobal(origscope) &&
                !isSgNamespaceDefinitionStatement(origscope)) {
              shouldReplace = true;
            }
          } else if (isSgValueExp(orig_expr)) {
            shouldReplace = true;
          }

          SgLambdaCapture *lambdaCapture =
              isSgLambdaCapture(copy_vr->get_parent());

          if (shouldReplace) {
            assert(orig_expr);
            SgExpression *orig_copy =
                isSgExpression(SageInterface::deepCopy(orig_expr));
            ASSERT_not_null(orig_copy);
            orig_copy->set_lvalue(copy_vr->get_lvalue());

            ASSERT_not_null(copy_vr);
            ASSERT_not_null(copy_vr->get_parent());
            SgNode *owner = copy_vr->get_parent();
            if (isSgStatement(owner) != nullptr ||
                isSgExpression(owner) != nullptr) {
              SageInterface::replaceExpression(copy_vr, orig_copy);
            } else if (lambdaCapture != nullptr) {
              replaceLambdaCaptureInitializer(lambdaCapture, copy_vr,
                                              orig_copy);
            } else {
              fprintf(stderr,
                      "REX_INLINE_INVARIANT[copy-propagation-expression-"
                      "owner]: variable=%p has unsupported owner=%p/%s\n",
                      static_cast<void *>(copy_vr),
                      static_cast<void *>(copy_vr->get_parent()),
                      copy_vr->get_parent()->class_name().c_str());
              ROSE_ABORT();
            }
          }
        }
      }
    }
  }
};

// Find all used variable declarations.
class FindUsedDeclarationsVisitor : public AstSimpleProcessing {
public:
  set<SgInitializedName *> used_decls;

  virtual void visit(SgNode *n) {
    if (isSgVarRefExp(n)) {
      assert(isSgVarRefExp(n)->get_symbol());
      used_decls.insert(isSgVarRefExp(n)->get_symbol()->get_declaration());
    }
  }
};

set<SgInitializedName *> findVariablesUsedInRegion(SgNode *e) {
  FindUsedDeclarationsVisitor vis;
  vis.traverse(e, preorder);
  return vis.used_decls;
}

// Remove unused variable declarations in which the initializer is null, a
// variable, or a constant.
class RemoveUnusedDeclarationsVisitor : public AstSimpleProcessing {
  const set<SgInitializedName *> &used_decls;
  const set<SgFunctionDeclaration *> &safe_functions;

public:
  RemoveUnusedDeclarationsVisitor(const set<SgInitializedName *> &u,
                                  const set<SgFunctionDeclaration *> &s)
      : used_decls(u), safe_functions(s) {}

  bool isSimpleInitializer(SgExpression *e) {
    if (SageInterface::isVolatileType(e->get_type()))
      return false;
    if (isSgVarRefExp(e))
      return true;
    if (isSgValueExp(e))
      return true;
    if (isSgUnaryOp(e) && isSimpleInitializer(isSgUnaryOp(e)->get_operand())) {
      return isSgBitComplementOp(e) || isSgMinusOp(e) || isSgNotOp(e) ||
             isSgUnaryAddOp(e) || isSgCastExp(e) || isSgPointerDerefExp(e);
    }
    if (isSgBinaryOp(e) &&
        isSimpleInitializer(isSgBinaryOp(e)->get_lhs_operand()) &&
        isSimpleInitializer(isSgBinaryOp(e)->get_rhs_operand())) {
      return isSgAddOp(e) || isSgAndOp(e) || isSgBitAndOp(e) ||
             isSgBitOrOp(e) || isSgBitXorOp(e) || isSgCommaOpExp(e) ||
             isSgDivideOp(e) || isSgEqualityOp(e) || isSgGreaterOrEqualOp(e) ||
             isSgGreaterThanOp(e) || isSgLessOrEqualOp(e) ||
             isSgLessThanOp(e) || isSgLshiftOp(e) || isSgModOp(e) ||
             isSgMultiplyOp(e) || isSgNotEqualOp(e) || isSgOrOp(e) ||
             isSgRshiftOp(e) || isSgSubtractOp(e) || isSgPntrArrRefExp(e);
    }
    if (isSgConditionalExp(e)) {
      SgConditionalExp *c = isSgConditionalExp(e);
      c->validate();
      return isSimpleInitializer(c->get_conditional_exp()) &&
             (c->get_operator_kind() ==
                  SgConditionalExp::e_conditional_operator_gnu_binary ||
              isSimpleInitializer(c->get_true_exp())) &&
             isSimpleInitializer(c->get_false_exp());
    }
    if (isSgFunctionCallExp(e)) {
      SgFunctionRefExp *fr =
          isSgFunctionRefExp(isSgFunctionCallExp(e)->get_function());
      if (!fr)
        return false;
      SgFunctionDeclaration *decl = fr->get_symbol()->get_declaration();
      if (safe_functions.find(decl) == safe_functions.end()) {
        return false;
      }
      const SgExpressionPtrList &args =
          isSgFunctionCallExp(e)->get_args()->get_expressions();
      for (size_t i = 0; i < args.size(); ++i) {
        if (!isSimpleInitializer(args[i]))
          return false;
      }
      return true;
    }
    return false;
  }

  virtual void visit(SgNode *n) {
    if (isSgBasicBlock(n)) {
      SgBasicBlock *bb = isSgBasicBlock(n);
      for (size_t i = 0; i < bb->get_statements().size();) {
        auto *declaration = isSgVariableDeclaration(bb->get_statements()[i]);
        bool removed = false;
        if (declaration != nullptr) {
          for (SgInitializedName *variable : declaration->get_variables()) {
            if (used_decls.count(variable) != 0 ||
                SageInterface::isVolatileType(variable->get_type()))
              continue;
            SgInitializer *initializer = variable->get_initializer();
            bool shouldErase = initializer == nullptr;
            if (auto *assignment = isSgAssignInitializer(initializer))
              shouldErase = isSimpleInitializer(assignment->get_operand());
            if (shouldErase) {
              auto *symbol = isSgVariableSymbol(
                  variable->search_for_symbol_from_symbol_table());
              ROSE_ASSERT(symbol != nullptr);
              auto *table = isSgSymbolTable(symbol->get_parent());
              ROSE_ASSERT(table != nullptr && table->exists(symbol));
              const bool owns_symbol = symbol->get_declaration() == variable;
              removeVariableDeclaration(variable);
              if (owns_symbol)
                table->remove(symbol);
              SageInterface::deleteAST(
                  {declaration, owns_symbol ? symbol : nullptr},
                  SageInterface::DeleteAstMode::kRequireIsolated);
              removed = true;
              break;
            }
          }
        }
        if (!removed)
          ++i;
      }
    }
  }
};

// In code with declarations such as "int foo = bar", where foo and bar are
// not modified, replace "foo" with "bar" and remove the declaration
void simpleCopyAndConstantPropagation(SgNode *top) {
  FindReferenceVariablesVisitor().traverse(top, postorder);
  FindCopiesVisitor().traverse(top, postorder);
  FindUsedDeclarationsVisitor vis;
  vis.traverse(top, preorder);
  RemoveUnusedDeclarationsVisitor(vis.used_decls,
                                  set<SgFunctionDeclaration *>())
      .traverse(top, postorder);
}

// Remove unused variables in a scope
void removeUnusedVariables(SgNode *top,
                           const set<SgFunctionDeclaration *> &safeFunctions) {
  set<SgInitializedName *> usedDecls;
  vector<SgNode *> varRefs = NodeQuery::querySubTree(top, V_SgVarRefExp);
  for (size_t i = 0; i < varRefs.size(); ++i) {
    usedDecls.insert(
        isSgVarRefExp(varRefs[i])->get_symbol()->get_declaration());
  }
  RemoveUnusedDeclarationsVisitor(usedDecls, safeFunctions)
      .traverse(top, postorder);
}

// Remove the declaration of a given variable.
void removeVariableDeclaration(SgInitializedName *initname) {
  SgVariableDeclaration *parent =
      isSgVariableDeclaration(initname->get_parent());
  ROSE_ASSERT(parent != nullptr);
  const SgInitializedNamePtrList &vars = parent->get_variables();
  ASSERT_require(vars.size() == 1);
  ROSE_ASSERT(vars.front() == initname);
  SageInterface::removeStatement(parent);
}

// Move variable declarations down until just before they are used, and
// combine uninitialized variables with their first assignments if
// possible.
class MoveDeclarationsToFirstUseVisitor : public AstSimpleProcessing {
public:
  virtual void visit(SgNode *n) {
    if (isSgBasicBlock(n)) {
      SgBasicBlock *bb = isSgBasicBlock(n);
      SgStatementPtrList &stmts = bb->get_statements();
      size_t initi;
      for (size_t decli = 0; decli < stmts.size(); ++decli) {
        if (isSgVariableDeclaration(stmts[decli])) {
          SgVariableDeclaration *decl = isSgVariableDeclaration(stmts[decli]);
          SgInitializedNamePtrList &vars = decl->get_variables();
          for (size_t vari = 0; vari != vars.size(); ++vari) {
            SgInitializedName *in = vars[vari];
            if (in->get_initializer() == 0) {
              bool used = false;
              for (initi = decli + 1; initi < stmts.size();
                   used |= containsVariableReference(stmts[initi], in),
                  ++initi) {
                SgExprStatement *initExprStmt = isSgExprStatement(stmts[initi]);
                if (initExprStmt) {
                  SgExpression *top = initExprStmt->get_expression();
                  if (isSgAssignOp(top)) {
                    SgVarRefExp *vr =
                        isSgVarRefExp(isSgAssignOp(top)->get_lhs_operand());
                    ASSERT_not_null(isSgAssignOp(top));
                    SgExpression *newinit =
                        isSgAssignOp(top)->get_rhs_operand();
                    if (!used && vr &&
                        vr->get_symbol()->get_declaration() == in) {
                      ASSERT_not_null(newinit);
                      ASSERT_not_null(newinit->get_type());

                      SgExpression *initializer_operand =
                          isSgExpression(SageInterface::deepCopy(newinit));
                      if (initializer_operand == nullptr ||
                          initializer_operand->get_parent() != nullptr) {
                        fprintf(stderr,
                                "REX_AST_INVARIANT[inliner-initializer-move]: "
                                "copied operand is missing or already owned\n");
                        ROSE_ABORT();
                      }
                      SgAssignInitializer *i = new SgAssignInitializer(
                          SgNULL_FILE, initializer_operand, in->get_type());
                      i->set_endOfConstruct(SgNULL_FILE);

                      vars[vari]->set_initializer(i);
                      stmts[initi] = decl;
                      ASSERT_not_null(i->get_parent());

                      ASSERT_not_null(i->get_file_info());

                      // Assumes only one var per declaration FIXME
                      ASSERT_require(vars.size() == 1);
                      stmts.erase(stmts.begin() + decli);
                      initExprStmt->set_parent(nullptr);
                      SageInterface::deleteAST(
                          initExprStmt,
                          SageInterface::DeleteAstMode::kRequireIsolated);
                      --decli; // To counteract ++decli in loop header
                      break;   // To get out of initi loop
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
};

// Find all initialized names in some code.
class FindInitializedNames : public AstSimpleProcessing {
public:
  SgInitializedNamePtrList ls;

  virtual void visit(SgNode *n) {
    if (isSgInitializedName(n)) {
      ls.insert(ls.begin(), isSgInitializedName(n));
    }
  }
};

SgInitializedNamePtrList findInitializedNamesInScope(SgScopeStatement *scope) {
  FindInitializedNames fin;
  fin.traverse(scope, preorder);
  return fin.ls;
}

// Combined procedure for cleaning up code after inlining.  Does not do
// variable renaming or block flattening, however.
void cleanupInlinedCode(SgNode *top) {
  ASSERT_not_null(isSgProject(top));

  simpleCopyAndConstantPropagation(top);
  SageInterface::removeJumpsToNextStatement(top);
  SageInterface::removeUnusedLabels(top);

  RemoveNullStatementsVisitor().traverse(top, postorder);

  MoveDeclarationsToFirstUseVisitor().traverse(top, postorder);

  simpleCopyAndConstantPropagation(top);
  RemoveNullStatementsVisitor().traverse(top, postorder);
}

void removeNullStatements(SgNode *top) {
  RemoveNullStatementsVisitor().traverse(top, postorder);
}
