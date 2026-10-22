#include "sage3basic.h"

#include "sageBuilder.h"

#include "analysisUtils.h"

#include "destructorCallAnnotator.h"

#include <unordered_map>
#include <vector>

using namespace std;

typedef Rose_STL_Container<SgNode *> in_list;
typedef Rose_STL_Container<SgNode *> temp_list;

namespace {
class FunctionBodyTraversal : public AstSimpleProcessing {
public:
  FunctionBodyTraversal() { set_useDefaultIndexBasedTraversal(false); }

protected:
  void setNodeSuccessors(SgNode *node,
                         SuccessorsContainer &successors) override {
    if (!isSgFunctionDeclaration(node) && !isSgClassDeclaration(node))
      AstSimpleProcessing::setNodeSuccessors(node, successors);
  }
};

void addStmtVarsIfAny(in_list &objs, SgStatement *stmt) {
  if (SgVariableDeclaration *ds = isSgVariableDeclaration(stmt)) {
    const auto &storage = ds->get_declarationModifier().get_storageModifier();
    if (storage.isStatic() || storage.isExtern() ||
        storage.get_thread_local_storage() || ds->get_is_thread_local())
      return;
    Rose_STL_Container<SgInitializedName *> &vars = ds->get_variables();
    for (Rose_STL_Container<SgInitializedName *>::iterator i = vars.begin();
         i != vars.end(); ++i) {
      if (SgConstructorInitializer *ci =
              isSgConstructorInitializer(((*i)->get_initializer()))) {
        if (ci->get_class_decl() != nullptr) {
          objs.insert(objs.begin(), ci);
        }
      }
    }
  }
}

class FindTemporaries : public FunctionBodyTraversal {
private:
  bool isGood(SgNode *n) {
    if (isSgAssignOp(n->get_parent()) ||
        isSgAssignInitializer(n->get_parent())) {
      return false;
    }
    if (SgConstructorInitializer *ci = isSgConstructorInitializer(n)) {
      return ci->get_class_decl() != nullptr;
    }
    return true;
  }

public:
  temp_list &temps;

  FindTemporaries(temp_list &_temps) : temps(_temps) {}

  void visit(SgNode *node) {
    if (!isGood(node)) {
      return;
    }
    if (SgConstructorInitializer *ci = isSgConstructorInitializer(node)) {
      SgNode *parent = ci->get_parent();
      if (!isSgInitializedName(parent) && !isSgNewExp(parent)) {
        SgClassDeclaration *cd = isSgClassDeclaration(ci->get_class_decl());
        ROSE_ASSERT(cd);

        temps.insert(temps.begin(), cd);
      }
    } else if (SgFunctionCallExp *fc = isSgFunctionCallExp(node)) {
      SgType *retType = fc->get_type();
      while (SgTypedefType *retTypedef = isSgTypedefType(retType)) {
        retType = retTypedef->get_base_type();
      }
      if (SgClassType *retClsType = isSgClassType(retType)) {
        SgClassDeclaration *retClsDecl =
            isSgClassDeclaration(retClsType->get_declaration());
        ROSE_ASSERT(retClsDecl);
        temps.push_back(retClsDecl);
      }
    }
  }
};

in_list blockObjectsAllocated(SgStatement *block, SgStatement *stop) {
  in_list objs;
  ROSE_ASSERT(block);
  switch (block->variantT()) {
  case V_SgBasicBlock: {
    SgBasicBlock *b = isSgBasicBlock(block);
    ROSE_ASSERT(b);

    SgNode *parent = block->get_parent();
    if (SgForStatement *fs = isSgForStatement(parent)) {
      addStmtVarsIfAny(objs, fs->get_test());
    } else if (SgWhileStmt *ws = isSgWhileStmt(parent)) {
      addStmtVarsIfAny(objs, ws->get_condition());
    } else if (SgIfStmt *is = isSgIfStmt(parent)) {
      addStmtVarsIfAny(objs, is->get_conditional());

      if (is->get_is_short_circuit_generated()) {
        FindTemporaries ft(objs);
        ft.traverse(is->get_conditional(), preorder);

        if (b->get_statements().size() > 1) {
          // the first statement is the left conjunct of a ,
          // expression... grab temporaries from this as well
          ft.traverse(b->get_statements().front(), preorder);
        }
      }
    }

    SgStatementPtrList &stmts = b->get_statements();
    for (SgStatementPtrList::iterator i = stmts.begin(); i != stmts.end();
         ++i) {
      if (*i == stop)
        break;
      addStmtVarsIfAny(objs, *i);
    }
  } break;
  case V_SgForStatement: {
    SgForStatement *fs = isSgForStatement(block);
    SgStatementPtrList &stmts = fs->get_init_stmt();
    for (SgStatementPtrList::iterator i = stmts.begin(); i != stmts.end();
         ++i) {
      if (*i == stop) {
        break;
      }
      addStmtVarsIfAny(objs, *i);
    }
  } break;
  default:
    break;
  }
  return objs;
}

template <class StopCond>
in_list objectsAllocated(SgStatement *statement, StopCond stopCond) {

  in_list objs;
  SgStatement *parent;
  do {
    parent = isSgStatement(statement->get_parent());
    ROSE_ASSERT(parent);
    in_list blockObjs = blockObjectsAllocated(parent, statement);
    objs.insert(objs.end(), blockObjs.begin(), blockObjs.end());
    statement = parent;
  } while (!stopCond(parent));
  return objs;
}

/*
 * The list of objects which are allocated and are normally disposed of by the
 * inner block. This is to ensure that in cases such as
 * while (objtype foo = expr) { ... }
 * foo is properly disposed of should it evaluate to 0
 */
in_list auxObjectsAllocated(SgStatement *stmt) {
  in_list objs;
  if (SgForStatement *fs = isSgForStatement(stmt)) {
    addStmtVarsIfAny(objs, fs->get_test());
  } else if (SgWhileStmt *ws = isSgWhileStmt(stmt)) {
    addStmtVarsIfAny(objs, ws->get_condition());
  }
  return objs;
}

struct IsFunctionDef {
  bool operator()(SgNode *n) { return isSgFunctionDefinition(n) != nullptr; }
};

struct IsBreakPoint {
  bool operator()(SgNode *n) {
    return isSgForStatement(n) || isSgWhileStmt(n) || isSgDoWhileStmt(n) ||
           isSgSwitchStatement(n) || isSgFunctionDefinition(n);
  }
};

struct ParentIsBreakPoint {
  bool operator()(SgNode *n) { return IsBreakPoint()(n->get_parent()); }
};

struct DestructionAnnotations {
  std::unordered_map<SgNode *, in_list> objects;
  std::unordered_map<SgNode *, in_list> auxiliaryObjects;
  std::unordered_map<SgNode *, temp_list> temporaries;
};

class Annotator : public FunctionBodyTraversal {
  DestructionAnnotations &annotations;

public:
  explicit Annotator(DestructionAnnotations &annotations)
      : annotations(annotations) {}

  void visit(SgNode *node) {
    if (SgScopeStatement *ss = isSgScopeStatement(node)) {
      annotations.objects[ss] = blockObjectsAllocated(ss, NULL);
      annotations.auxiliaryObjects[ss] = auxObjectsAllocated(ss);
    } else if (SgBreakStmt *bs = isSgBreakStmt(node)) {
      annotations.objects[node] = objectsAllocated(bs, IsBreakPoint());
    } else if (SgContinueStmt *cs = isSgContinueStmt(node)) {
      annotations.objects[node] = objectsAllocated(cs, ParentIsBreakPoint());
    } else if (SgReturnStmt *rs = isSgReturnStmt(node)) {
      annotations.objects[node] = objectsAllocated(rs, IsFunctionDef());
    } else if (SgGotoStatement *gs = isSgGotoStatement(node)) {
      in_list gotoList = objectsAllocated(gs, IsFunctionDef());
      in_list labelList = objectsAllocated(gs->get_label(), IsFunctionDef());
      in_list::iterator gi = gotoList.end(), li = labelList.end();
      while (li != labelList.begin()) {
        if (gi == gotoList.begin() || *--gi != *--li) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[goto-destruction-scope]: label objects "
                  "are not a suffix of goto objects\n");
          ROSE_ABORT();
        }
      }

      in_list &gotoDiff = annotations.objects[node];
      gotoDiff.insert(gotoDiff.begin(), gotoList.begin(), gi);
    } else if (node->variantT() == V_SgInitializedName) {
      FindTemporaries ft(annotations.temporaries[node]);
      ft.traverse(node, preorder);
    } else {
      return;
    }
  }
};

SgMemberFunctionRefExp *buildDtorRefExpr(SgClassDeclaration *decl) {
  ROSE_ASSERT(decl != nullptr);
  SgClassDeclaration *defDecl =
      isSgClassDeclaration(decl->get_definingDeclaration());
  ROSE_ASSERT(defDecl != nullptr);
  SgClassDefinition *def = defDecl->get_definition();
  ROSE_ASSERT(def != nullptr);

  SgMemberFunctionDeclaration *destructor =
      SageInterface::getDefaultDestructor(defDecl);
  SgMemberFunctionSymbol *dtor =
      destructor != nullptr
          ? isSgMemberFunctionSymbol(destructor->get_symbol_from_symbol_table())
          : nullptr;
  SgMemberFunctionDeclaration *symbolDeclaration =
      dtor != nullptr ? dtor->get_declaration() : nullptr;
  const bool sameDeclaration =
      symbolDeclaration == destructor ||
      (destructor != nullptr && symbolDeclaration != nullptr &&
       destructor->get_firstNondefiningDeclaration() != nullptr &&
       symbolDeclaration->get_firstNondefiningDeclaration() ==
           destructor->get_firstNondefiningDeclaration());
  if (dtor == nullptr || symbolDeclaration == nullptr ||
      dtor->get_parent() != def->get_symbol_table() || !sameDeclaration ||
      symbolDeclaration->get_type() == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[destructor-reference]: class=%p has no exact "
            "typed destructor declaration and symbol\n",
            static_cast<void *>(decl));
    ROSE_ABORT();
  }
  return SageBuilder::buildMemberFunctionRefExp(dtor, false, false);
}

SgDotExp *buildDtorDotExpr(SgInitializedName *in) {
  SgVariableSymbol *var =
      isSgVariableSymbol(in->get_symbol_from_symbol_table());
  if (var == nullptr || var->get_declaration() != in) {
    fprintf(stderr,
            "REX_AST_INVARIANT[destructor-variable-reference]: initialized "
            "name has no exact typed variable symbol\n");
    ROSE_ABORT();
  }
  SgVarRefExp *vref = SageBuilder::buildVarRefExp(var);
  SgMemberFunctionRefExp *dtorRef = buildDtorRefExpr(
      isSgConstructorInitializer(in->get_initializer())->get_class_decl());
  return SageBuilder::buildDotExp(vref, dtorRef, dtorRef->get_type());
}

SgExpression *buildDestructorExpr(SgNode *n) {
  SgExpression *ref;
  if (SgInitializedName *in = isSgInitializedName(n->get_parent())) {
    ref = buildDtorDotExpr(in);
  } else if (SgConstructorInitializer *ci = isSgConstructorInitializer(n)) {
    ref = buildDtorRefExpr(ci->get_class_decl());
  } else if (SgClassDeclaration *cd = isSgClassDeclaration(n)) {
    ref = buildDtorRefExpr(cd);
  } else {
    ROSE_ABORT();
  }

  SgExprListExp *dtorParams = SageBuilder::buildExprListExp();
  auto *call = SageBuilder::buildFunctionCallExp(ref, SgTypeVoid::createType(),
                                                 dtorParams);
  call->set_source_syntax(SgFunctionCallExp::e_implicit_destruction);
  ROSE_ASSERT(call->get_source_syntax() ==
              SgFunctionCallExp::e_implicit_destruction);
  return call;
}

SgStatement *buildDestructorStmt(SgNode *n, SgLocatedNode *outputOwner) {
  SgExpression *dtorExpr = buildDestructorExpr(n);
  SgExprStatement *dtorExprStmt = SageBuilder::buildExprStatement(dtorExpr);
  SageInterface::publishGeneratedSubtreeOutputOwner(dtorExprStmt, outputOwner);
  return dtorExprStmt;
}

void destroyTemporariesInFollowingPositions(
    SgInitializedName *name, DestructionAnnotations &annotations) {
  temp_list &temps = annotations.temporaries[name];
  stmt_pos_list poss = findFollowingPositions(name);
  if (poss.empty() && !temps.empty()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[temporary-destructor-placement]: no exact "
            "statement position follows a live temporary\n");
    ROSE_ABORT();
  }
  for (stmt_pos &pos : poss) {
    ROSE_ASSERT(pos.statements != nullptr && pos.owner != nullptr &&
                pos.index <= pos.statements->size());
    for (SgNode *temp : temps) {
      SgStatement *dtorStmt = buildDestructorStmt(temp, pos.owner);
      pos.statements->insert(pos.statements->begin() + pos.index, dtorStmt);
      ++pos.index;
      dtorStmt->set_parent(pos.owner);
    }
  }
}

class Transformer : public FunctionBodyTraversal {
  DestructionAnnotations &annotations;

public:
  explicit Transformer(DestructionAnnotations &annotations)
      : annotations(annotations) {}

  void visit(SgNode *node) {
    if (SgBasicBlock *bb = isSgBasicBlock(node)) {
      in_list &ctors = annotations.objects[bb];
      for (in_list::iterator i = ctors.begin(); i != ctors.end(); ++i) {
        SgStatement *dtorStmt = buildDestructorStmt(*i, bb);
        bb->append_statement(dtorStmt);
      }
    } else if (node->variantT() == V_SgForStatement ||
               node->variantT() == V_SgWhileStmt) {
      SgStatement *stmt = isSgStatement(node);
      ROSE_ASSERT(stmt);

      stmt_pos pos = findPosition(stmt);
      ++pos.index;
      ROSE_ASSERT(pos.statements != nullptr && pos.owner != nullptr &&
                  pos.index <= pos.statements->size());

      in_list &auxCtors = annotations.auxiliaryObjects[stmt];
      for (in_list::iterator i = auxCtors.begin(); i != auxCtors.end(); ++i) {
        SgStatement *dtorStmt = buildDestructorStmt(*i, pos.owner);
        pos.statements->insert(pos.statements->begin() + pos.index, dtorStmt);
        ++pos.index;
        dtorStmt->set_parent(pos.owner);
      }

      in_list &ctors = annotations.objects[stmt];
      for (in_list::iterator i = ctors.begin(); i != ctors.end(); ++i) {
        SgStatement *dtorStmt = buildDestructorStmt(*i, pos.owner);
        pos.statements->insert(pos.statements->begin() + pos.index, dtorStmt);
        ++pos.index;
        dtorStmt->set_parent(pos.owner);
      }
    } else if (node->variantT() == V_SgBreakStmt ||
               node->variantT() == V_SgContinueStmt ||
               node->variantT() == V_SgGotoStatement) {
      SgStatement *stmt = isSgStatement(node);
      in_list &ctors = annotations.objects[stmt];

      if (ctors.empty()) {
        return;
      }

      SgBasicBlock *newBlock = SageBuilder::buildBasicBlock();
      for (in_list::iterator i = ctors.begin(); i != ctors.end(); ++i) {
        newBlock->append_statement(buildDestructorStmt(*i, stmt));
      }
      SageInterface::replaceStatement(stmt, newBlock);
      newBlock->append_statement(stmt);
    } else if (SgReturnStmt *retStmt = isSgReturnStmt(node)) {
      in_list &ctors = annotations.objects[retStmt];
      if (ctors.empty()) {
        return;
      }

      SgFunctionDeclaration *function =
          SageInterface::getEnclosingFunctionDeclaration(retStmt);
      SgFunctionType *functionType =
          function != nullptr ? function->get_type() : nullptr;
      SgType *returnType =
          functionType != nullptr ? functionType->get_return_type() : nullptr;
      SgExpression *retExp = retStmt->get_expression();
      SgType *valueType = returnType != nullptr
                              ? returnType->stripTypedefsAndModifiers()
                              : nullptr;
      SgNullExpression *absentValue = isSgNullExpression(retExp);
      const bool hasValue = retExp != nullptr && absentValue == nullptr;
      SgType *expressionType = hasValue ? retExp->get_type() : nullptr;
      SgType *expressionCore = expressionType != nullptr
                                   ? expressionType->stripTypedefsAndModifiers()
                                   : nullptr;
      const bool voidReturn = isSgTypeVoid(valueType) != nullptr;
      const bool supportedValue =
          valueType != nullptr &&
          (isSgIntegralType(valueType) || isSgFloatingPointType(valueType) ||
           isSgEnumType(valueType) || isSgPointerType(valueType) ||
           isSgPointerMemberType(valueType) || isSgReferenceType(valueType) ||
           isSgTypeComplex(valueType) || isSgTypeImaginary(valueType));
      if (returnType == nullptr ||
          SageInterface::containsUnknownType(returnType) ||
          (retExp != nullptr && retExp->get_parent() != retStmt) ||
          (absentValue != nullptr &&
           absentValue->get_role() !=
               SgNullExpression::e_null_expression_syntactic_absence) ||
          (hasValue && (expressionType == nullptr ||
                        SageInterface::containsUnknownType(expressionType) ||
                        isSgTypeDefault(expressionType) != nullptr)) ||
          (!voidReturn && (!hasValue || !supportedValue ||
                           isSgRvalueReferenceType(valueType) != nullptr ||
                           isSgTypeVoid(expressionCore) != nullptr ||
                           (isSgReferenceType(valueType) != nullptr &&
                            !retExp->get_lvalue()))) ||
          (voidReturn && hasValue && isSgTypeVoid(expressionCore) == nullptr)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[destructor-return-value]: return requires "
                "an exact typed scalar or lvalue-reference result; class "
                "values and rvalue references need value-category-preserving "
                "lowering\n");
        ROSE_ABORT();
      }

      std::vector<SgStatement *> destructorStmts;
      destructorStmts.reserve(ctors.size());
      for (SgNode *ctor : ctors) {
        destructorStmts.push_back(buildDestructorStmt(ctor, retStmt));
      }

      SgBasicBlock *returnBlock = SageBuilder::buildBasicBlock();
      SageInterface::replaceStatement(retStmt, returnBlock);
      returnBlock->append_statement(retStmt);

      if (hasValue && !voidReturn) {
        SgExpression *valueCopy = SageInterface::copyExpression(retExp);
        SgAssignInitializer *initializer =
            SageBuilder::buildAssignInitializer(valueCopy, returnType);
        SgName valueName = SageInterface::generateUniqueVariableName(
            returnBlock, "rose_return_value_");
        SgVariableDeclaration *valueDecl =
            SageBuilder::buildUnpublishedVariableDeclaration(
                valueName, returnType, initializer, returnBlock);
        ROSE_ASSERT(valueDecl != nullptr &&
                    valueDecl->get_variables().size() == 1);
        SageInterface::insertStatementBefore(retStmt, valueDecl, false);
        SgInitializedName *initialized = valueDecl->get_variables().front();
        SgVariableSymbol *valueSymbol = isSgVariableSymbol(
            returnBlock->find_symbol_from_declaration(initialized));
        ROSE_ASSERT(valueSymbol != nullptr &&
                    valueSymbol->get_declaration() == initialized);
        SageInterface::replaceExpression(
            retExp, SageBuilder::buildVarRefExp(valueSymbol));
      } else if (hasValue) {
        SgExprStatement *evaluate = SageBuilder::buildExprStatement(
            SageInterface::copyExpression(retExp));
        SageInterface::insertStatementBefore(retStmt, evaluate, false);
        SageInterface::replaceExpression(
            retExp, SageBuilder::buildNullExpression(
                        SgNullExpression::e_null_expression_syntactic_absence));
      }

      for (SgStatement *destructorStmt : destructorStmts) {
        SageInterface::insertStatementBefore(retStmt, destructorStmt, false);
      }
    } else if (SgInitializedName *iname = isSgInitializedName(node)) {
      destroyTemporariesInFollowingPositions(iname, annotations);
    }
  }
};

void moveForInitializers(SgForStatement *forStmt) {
  SgForInitStatement *forInit = forStmt->get_for_init_stmt();
  ROSE_ASSERT(forInit != nullptr && forInit->get_parent() == forStmt);
  SgStatementPtrList &initStmts = forInit->get_init_stmt();
  ROSE_ASSERT(!initStmts.empty());
  if (initStmts.size() == 1 && isSgNullStatement(initStmts.front())) {
    return;
  }

  stmt_pos forPos = findPosition(forStmt);
  ROSE_ASSERT(std::count(forPos.statements->begin(), forPos.statements->end(),
                         forStmt) == 1);
  for (SgStatement *initStmt : initStmts) {
    ROSE_ASSERT(initStmt != nullptr && initStmt->get_parent() == forInit);
    ROSE_ASSERT(std::count(initStmts.begin(), initStmts.end(), initStmt) == 1);
    if (auto *group = isSgDeclarationGroupStatement(initStmt)) {
      group->validate();
      ROSE_ASSERT(group->get_scope() == forStmt);
    }
  }

  SgBasicBlock *newBlock = SageBuilder::buildBasicBlock();
  SageInterface::replaceStatement(forStmt, newBlock);

  SgStatementPtrList movedInitStmts;
  movedInitStmts.swap(initStmts);
  for (SgStatement *initStmt : movedInitStmts) {
    initStmt->set_parent(nullptr);
    if (auto *group = isSgDeclarationGroupStatement(initStmt))
      group->set_scope(newBlock);
    newBlock->append_statement(initStmt);
  }
  newBlock->append_statement(forStmt);
  SageInterface::appendStatement(SageBuilder::buildNullStatement(), forInit);
  SageInterface::rebindVariableReferencesAfterMove(newBlock);
}

} // namespace

void destructorCallAnnotator(SgProject *prj) {
  Rose_STL_Container<SgNode *> forStmts =
      NodeQuery::querySubTree(prj, V_SgForStatement);

  for (Rose_STL_Container<SgNode *>::iterator i = forStmts.begin();
       i != forStmts.end(); ++i) {
    SgForStatement *forStmt = isSgForStatement(*i);
    ROSE_ASSERT(forStmt);
    moveForInitializers(forStmt);
  }

  Rose_STL_Container<SgNode *> defs =
      NodeQuery::querySubTree(prj, V_SgFunctionDefinition);

  for (Rose_STL_Container<SgNode *>::iterator i = defs.begin(); i != defs.end();
       ++i) {
    DestructionAnnotations annotations;
    Annotator a(annotations);
    a.traverse(isSgFunctionDefinition(*i)->get_body(), preorder);

    Transformer t(annotations);
    t.traverse(isSgFunctionDefinition(*i)->get_body(), postorder);
  }
}
