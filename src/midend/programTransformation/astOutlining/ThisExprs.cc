/**
 *  \file ThisExprs.cc
 *  \brief Preprocessor phase to convert 'this' expressions
 *  to-be-outlined into references to a local variable.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <iostream>

#include <list>

#include <string>

#include "ASTtools.hh"

#include "Preprocess.hh"

#include "PreprocessingInfo.hh"

#include "StmtRewrite.hh"

#include "This.hh"

#include "VarSym.hh"

// =====================================================================

using namespace std;
using namespace Outliner;
using namespace SageInterface;

// =====================================================================

/*!
 *  Checks that a set of 'this' expressions has the same class symbol,
 *  and returns that symbol.
 */
static SgClassSymbol *getClassSymAndVerify(const ASTtools::ThisExprSet_t &E) {
  SgClassSymbol *sym = 0;
  if (!E.empty()) {
    for (ASTtools::ThisExprSet_t::const_iterator i = E.begin(); i != E.end();
         ++i) {
      const SgThisExp *t = *i;
      ROSE_ASSERT(t);
      if (!sym)
        sym = t->get_class_symbol();
      else if (sym != t->get_class_symbol()) {
        cerr << "*** 'this' expressions use different symbols! ***" << endl;
        return 0; // Signal error
      }
    }
  }
  return sym;
}
//!  class Hello *this__ptr__ = this;
// creation and insertion if it does not yet exist
// multiple outlining targets within the same member function can share the same
// declaration
static SgVariableDeclaration *createThisShadowDecl(
    const string &name,
    SgClassSymbol *sym, /* the class symbol for this pointer*/
    SgFunctionDefinition *func_def /*The enclosing class member function*/)
//                      SgScopeStatement* scope)
{
#ifdef __linux__
  if (enable_debug)
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
#endif
  SgVariableDeclaration *decl = NULL;
  ROSE_ASSERT(sym && func_def);

  // Analyze function definition.
  const SgMemberFunctionDeclaration *func_decl =
      isSgMemberFunctionDeclaration(func_def->get_declaration());
  ROSE_ASSERT(func_decl);
  SgBasicBlock *func_body = func_def->get_body();
  ROSE_ASSERT(func_body);

  // Build name for shadow variable.
  SgName var_name(name);

  SgVariableSymbol *exist_symbol = func_body->lookup_variable_symbol(var_name);
  if (exist_symbol) {
    // decl =
    // isSgVariableDeclaration(exist_symbol->get_declaration()->get_definition());
    decl = isSgVariableDeclaration(
        exist_symbol->get_declaration()->get_declaration());
    ROSE_ASSERT(decl);
    ROSE_ASSERT(decl->get_scope() == isSgScopeStatement(func_body));
  } else {
    // Build variable's type. class A*  or const class A *
    SgType *class_type = sym->get_type();
    ROSE_ASSERT(class_type);
    SgType *var_type = 0;
    if (ASTtools::isConstMemFunc(func_decl)) {
      SgModifierType *mod_type = SageBuilder::buildConstType(class_type);
      var_type = SgPointerType::createType(mod_type);
    } else
      var_type = SgPointerType::createType(class_type);
    ROSE_ASSERT(var_type);

    // Build initial value: this pointer
    SgThisExp *this_expr = SageBuilder::buildThisExp(sym);
    ROSE_ASSERT(this_expr);
    SgAssignInitializer *init = SageBuilder::buildAssignInitializer(this_expr);

    // Build final declaration.
    decl = SageBuilder::buildVariableDeclaration(var_name, var_type, init,
                                                 func_body);
    // SageBuilder::buildVariableDeclaration (var_name, var_type, init, scope);
    ROSE_ASSERT(
        decl->get_variableDeclarationContainsBaseTypeDefiningDeclaration() ==
        false);
    SageInterface::prependStatement(decl, func_body);
  }
  ROSE_ASSERT(decl);
  // Add some comments to mark it
  SageBuilder::buildComment(decl, "//A declaration for this pointer");

  // We insert it to the enclosing member function definition
  if (enable_debug) {
    cout << "prepending a statement declaring this__ptr into a function body:"
         << func_body << endl;
    cout << "The function body's file info is:" << endl;
    func_body->get_file_info()->display();
    func_body->unparseToString();
  }

  // Liao (1/i28/2020): When used in conjunction with header file unparsing we
  // need to set the physical file id on entirety of the subtree being inserted.
  SgSourceFile *sfile = getEnclosingSourceFile(func_body);
  if (sfile->get_unparseHeaderFiles()) {
    int physical_file_id =
        func_body->get_startOfConstruct()->get_physical_file_id();
    string physical_filename_from_id =
        Sg_File_Info::getFilenameFromID(physical_file_id);
    if (enable_debug) {
      printf("scope for function call transformation: "
             "physical_filename_from_id = %s \n",
             physical_filename_from_id.c_str());
    }

    SageBuilder::fixupSourcePositionFileSpecification(
        decl, physical_filename_from_id);
  }
  // decl->set_isModified(true);
  return decl;
}

//! Replace this->member  with this__ptr__->member
static void replaceThisExprs(ASTtools::ThisExprSet_t &this_exprs,
                             SgVariableDeclaration *decl) {
  SgVariableSymbol *sym = SageInterface::getFirstVarSym(decl);
  ROSE_ASSERT(sym);

  for (ASTtools::ThisExprSet_t::iterator i = this_exprs.begin();
       i != this_exprs.end(); ++i) {
    SgThisExp *e_this = const_cast<SgThisExp *>(*i);
    ROSE_ASSERT(e_this);

    SgVarRefExp *e_repl = SageBuilder::buildVarRefExp(sym);
    ROSE_ASSERT(e_repl);

    SgNode *e_par = e_this->get_parent();
    ROSE_ASSERT(e_par);
    if (isSgBinaryOp(e_par)) {
      SgBinaryOp *bin_op = isSgBinaryOp(e_par);
      SgExpression *lhs = isSgThisExp(bin_op->get_lhs_operand());
      if (lhs == e_this)
        bin_op->set_lhs_operand(e_repl);
      else {
        SgExpression *rhs = isSgThisExp(bin_op->get_rhs_operand());
        if (rhs == e_this)
          bin_op->set_rhs_operand(e_repl);
        else {
          ROSE_ASSERT(!"*** Binary op does not use 'this' as expected. ***");
        }
      }
    } else if (isSgUnaryOp(e_par)) {
      SgUnaryOp *un_op = isSgUnaryOp(e_par);
      SgThisExp *e = isSgThisExp(un_op->get_operand());
      if (e == e_this)
        un_op->set_operand(e_repl);
      else {
        ROSE_ASSERT(!"*** Unary op does not use 'this' as expected. ***");
      }
    } else if (isSgExprListExp(e_par)) {
      SgExprListExp *e_list = isSgExprListExp(e_par);
      SgExpressionPtrList &exprs = e_list->get_expressions();
      SgExpressionPtrList::iterator i =
          find(exprs.begin(), exprs.end(), e_this);
      if (i == exprs.end()) {
        ROSE_ASSERT(
            !"*** Expression list does not contain 'this' as expected. ***");
      } else
        *i = e_repl;
    } else if (isSgSizeOfOp(e_par)) {
      SgSizeOfOp *e_sizeof = isSgSizeOfOp(e_par);
      ROSE_ASSERT(e_sizeof->get_operand_expr() == e_this);
      e_sizeof->set_operand_expr(e_repl);
    } else if (isSgAssignInitializer(e_par)) {
      SgAssignInitializer *e_assign = isSgAssignInitializer(e_par);
      ROSE_ASSERT(e_assign->get_operand_i() == e_this);
      e_assign->set_operand_i(e_repl);
    } else // Don't know how to handle this...
    {
      cerr << "*** '" << e_par->class_name() << "' ***" << endl;
      ROSE_ASSERT(!"*** Case not handled ***");
    }

    // Set parent pointer of replacement expression.
    e_repl->set_parent(e_par);
  }
}

// =====================================================================

static SgClassSymbol *
getClassSymbolFromMemberFunctionDecl(const SgMemberFunctionDeclaration *decl) {
  if (decl == NULL)
    return NULL;

  SgDeclarationStatement *assoc_decl = decl->get_associatedClassDeclaration();
  SgClassDeclaration *class_decl = isSgClassDeclaration(assoc_decl);
  if (class_decl == NULL)
    return NULL;

  SgSymbol *sym = class_decl->get_symbol_from_symbol_table();
  if (sym == NULL && class_decl->get_scope() != NULL) {
    sym = class_decl->get_scope()->lookup_class_symbol(class_decl->get_name());
  }

  return isSgClassSymbol(sym);
}

static bool isNonStaticMemberFunctionDecl(const SgFunctionDeclaration *decl) {
  if (decl == NULL)
    return false;

  if (isSgMemberFunctionDeclaration(decl) == NULL &&
      isSgTemplateMemberFunctionDeclaration(decl) == NULL)
    return false;

  return decl->get_declarationModifier().get_storageModifier().isStatic() ==
         false;
}

static bool isImplicitMemberFunctionCall(const SgFunctionCallExp *call) {
  if (call == NULL)
    return false;

  SgExpression *func_expr = call->get_function();
  if (SgMemberFunctionRefExp *mem_ref = isSgMemberFunctionRefExp(func_expr)) {
    SgMemberFunctionSymbol *sym = mem_ref->get_symbol();
    return isNonStaticMemberFunctionDecl(sym ? sym->get_declaration() : NULL);
  }

  if (SgTemplateMemberFunctionRefExp *mem_ref =
          isSgTemplateMemberFunctionRefExp(func_expr)) {
    SgTemplateMemberFunctionSymbol *sym = mem_ref->get_symbol();
    return isNonStaticMemberFunctionDecl(sym ? sym->get_declaration() : NULL);
  }

  return false;
}

static bool isNonStaticMemberVariableDecl(const SgInitializedName *init_name) {
  if (init_name == NULL)
    return false;

  SgVariableDeclaration *decl =
      isSgVariableDeclaration(init_name->get_parent());
  if (decl == NULL)
    return false;

  if (isSgClassDefinition(decl->get_parent()) == NULL)
    return false;

  return decl->get_declarationModifier().get_storageModifier().isStatic() ==
         false;
}

static bool isImplicitMemberVarRef(const SgVarRefExp *var_ref) {
  if (var_ref == NULL)
    return false;

  SgVariableSymbol *sym = var_ref->get_symbol();
  if (sym == NULL)
    return false;

  if (!isNonStaticMemberVariableDecl(sym->get_declaration()))
    return false;

  SgNode *parent = var_ref->get_parent();
  if (isSgDotExp(parent) != NULL || isSgArrowExp(parent) != NULL)
    return false;

  return true;
}

static void replaceImplicitMemberFunctionCalls(SgBasicBlock *b,
                                               SgVariableDeclaration *decl) {
  if (b == NULL || decl == NULL)
    return;

  SgVariableSymbol *sym = SageInterface::getFirstVarSym(decl);
  ROSE_ASSERT(sym != NULL);

  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t calls = NodeQuery::querySubTree(const_cast<SgBasicBlock *>(b),
                                             V_SgFunctionCallExp);
  for (NodeList_t::iterator i = calls.begin(); i != calls.end(); ++i) {
    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    if (call == NULL || !isImplicitMemberFunctionCall(call))
      continue;

    SgExpression *func_expr = call->get_function();
    if (func_expr == NULL)
      continue;

    SgVarRefExp *this_ref = SageBuilder::buildVarRefExp(sym);
    ROSE_ASSERT(this_ref != NULL);

    SgArrowExp *arrow = SageBuilder::buildArrowExp(this_ref, func_expr);
    ROSE_ASSERT(arrow != NULL);

    call->set_function(arrow);
    arrow->set_parent(call);
    func_expr->set_parent(arrow);
    this_ref->set_parent(arrow);
  }
}

static void replaceImplicitMemberVarRefs(SgBasicBlock *b,
                                         SgVariableDeclaration *decl) {
  if (b == NULL || decl == NULL)
    return;

  SgVariableSymbol *sym = SageInterface::getFirstVarSym(decl);
  ROSE_ASSERT(sym != NULL);

  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t vars =
      NodeQuery::querySubTree(const_cast<SgBasicBlock *>(b), V_SgVarRefExp);
  for (NodeList_t::iterator i = vars.begin(); i != vars.end(); ++i) {
    SgVarRefExp *var_ref = isSgVarRefExp(*i);
    if (!isImplicitMemberVarRef(var_ref))
      continue;

    SgVarRefExp *this_ref = SageBuilder::buildVarRefExp(sym);
    ROSE_ASSERT(this_ref != NULL);

    SgVarRefExp *member_ref =
        SageBuilder::buildVarRefExp(var_ref->get_symbol());
    ROSE_ASSERT(member_ref != NULL);

    SgArrowExp *arrow = SageBuilder::buildArrowExp(this_ref, member_ref);
    ROSE_ASSERT(arrow != NULL);

    SageInterface::replaceExpression(var_ref, arrow, true);
  }
}

SgBasicBlock *Outliner::Preprocess::transformThisExprs(SgBasicBlock *b) {
#ifdef __linux__
  if (enable_debug)
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
#endif
  if (b == nullptr) {
    return b;
  }
  SgFile *file = getEnclosingFileNode(b);
  SgSourceFile *source = isSgSourceFile(file);
  if (source != nullptr && source->get_Fortran_only()) {
    return b;
  }
  // Find all 'this' expressions.
  ASTtools::ThisExprSet_t this_exprs;
  ASTtools::collectThisExpressions(b, this_exprs);
  bool has_implicit_calls = false;
  bool has_implicit_member_refs = false;
  {
    typedef Rose_STL_Container<SgNode *> NodeList_t;
    NodeList_t calls = NodeQuery::querySubTree(const_cast<SgBasicBlock *>(b),
                                               V_SgFunctionCallExp);
    for (NodeList_t::iterator i = calls.begin(); i != calls.end(); ++i) {
      if (isImplicitMemberFunctionCall(isSgFunctionCallExp(*i))) {
        has_implicit_calls = true;
        break;
      }
    }
  }
  {
    typedef Rose_STL_Container<SgNode *> NodeList_t;
    NodeList_t vars =
        NodeQuery::querySubTree(const_cast<SgBasicBlock *>(b), V_SgVarRefExp);
    for (NodeList_t::iterator i = vars.begin(); i != vars.end(); ++i) {
      if (isImplicitMemberVarRef(isSgVarRefExp(*i))) {
        has_implicit_member_refs = true;
        break;
      }
    }
  }

  if (this_exprs.empty() && !has_implicit_calls &&
      !has_implicit_member_refs) // No transformation required.
  {
#ifdef __linux__
    if (enable_debug)
      cout << "empty this expression set, exiting " << __PRETTY_FUNCTION__
           << " without create this shadow declaration. " << endl;
#endif
    return b;
  }

  if (enable_debug) {
    cout << "The input BB is:" << b << endl;
    b->get_file_info()->display();
  }
  // Get the class symbol for the set of 'this' expressions.
  SgClassSymbol *sym = NULL;
  if (!this_exprs.empty()) {
    sym = getClassSymAndVerify(this_exprs);
  } else {
    const SgFunctionDefinition *func_def = ASTtools::findFirstFuncDef(b);
    if (func_def != NULL) {
      const SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(func_def->get_declaration());
      if (member_decl != NULL) {
        sym = getClassSymbolFromMemberFunctionDecl(member_decl);
      }
    }
  }
  ROSE_ASSERT(sym != NULL);
  // Liao 10/27/2009
  // we have to consider the AST changes for both #pragma rose_outline and
  // #pragma omp parallel/task They have different layout: the first one has an
  // outlining target following the pragma
  //    generating an inner level BB and return it as the new outlining target
  //    can keep the this_ptr declaration
  // but the 2nd case has a child block as the outlining target
  //    the whole child block will be replaced and the this_ptr declaration will
  //    get lost
  // So the solution is to create the this__ptr__ declaration within the
  // enclosing member function definition No new inner level BB is created at
  // all.
  SgVariableDeclaration *decl = createThisShadowDecl(
      string("this__ptr__"), sym,
      const_cast<SgFunctionDefinition *>(ASTtools::findFirstFuncDef(b)));
  ROSE_ASSERT(decl);

  // Replace instances of SgThisExp with the shadow variable.
  replaceThisExprs(this_exprs, decl);
  if (has_implicit_calls)
    replaceImplicitMemberFunctionCalls(b, decl);
  if (has_implicit_member_refs)
    replaceImplicitMemberVarRefs(b, decl);
  if (enable_debug) {
    cout << "Debug Outliner::Preprocess::transformThisExprs() output BB is:"
         << b << endl;
    b->unparseToString();
  }

  // return b_this;
  return b;
}

// eof
