/**
 *  \file ThisExprs.cc
 *  \brief Preprocessor phase to convert 'this' expressions
 *  to-be-outlined into references to a local variable.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>
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
 *  Checks that a set of 'this' expressions has the same exact class or
 *  current-instantiation symbol, and returns that symbol.
 */
static SgSymbol *getThisSymbolAndVerify(const ASTtools::ThisExprSet_t &E,
                                        SgType **this_pointer_type) {
  SgSymbol *sym = NULL;
  if (this_pointer_type != NULL) {
    *this_pointer_type = NULL;
  }
  if (!E.empty()) {
    for (ASTtools::ThisExprSet_t::const_iterator i = E.begin(); i != E.end();
         ++i) {
      const SgThisExp *t = *i;
      ROSE_ASSERT(t);
      SgClassSymbol *class_symbol = t->get_class_symbol();
      SgNonrealSymbol *nonreal_symbol = t->get_nonreal_symbol();
      if ((class_symbol == NULL) == (nonreal_symbol == NULL)) {
        cerr << "REX_AST_INVARIANT[outliner-this-symbol]: outlined 'this' "
                "expression must have exactly one class or current-"
                "instantiation symbol"
             << endl;
        ROSE_ABORT();
      }
      SgSymbol *this_symbol = class_symbol != NULL
                                  ? static_cast<SgSymbol *>(class_symbol)
                                  : static_cast<SgSymbol *>(nonreal_symbol);
      if (sym == NULL)
        sym = this_symbol;
      else if (sym != this_symbol) {
        cerr << "REX_AST_INVARIANT[outliner-this-symbol]: outlined 'this' "
                "expressions use different exact receiver symbols"
             << endl;
        ROSE_ABORT();
      }
      if (this_pointer_type != NULL) {
        SgType *current_pointer_type = t->get_type();
        if (current_pointer_type == NULL ||
            isSgPointerType(current_pointer_type) == NULL) {
          cerr << "REX_AST_INVARIANT[outliner-this-result-type]: outlined "
                  "'this' expression has no exact pointer result type"
               << endl;
          ROSE_ABORT();
        }
        if (*this_pointer_type == NULL) {
          *this_pointer_type = current_pointer_type;
        } else if (*this_pointer_type != current_pointer_type &&
                   !SageInterface::isEquivalentType(*this_pointer_type,
                                                    current_pointer_type)) {
          cerr << "REX_AST_INVARIANT[outliner-this-result-type]: outlined "
                  "'this' expressions have different semantic pointer types"
               << endl;
          ROSE_ABORT();
        }
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
    SgSymbol *sym, /* exact class/current-instantiation symbol */
    SgFunctionDefinition *func_def, /*The enclosing class member function*/
    SgType *this_pointer_type)
//                      SgScopeStatement* scope)
{
#ifdef __linux__
  if (enable_debug)
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
#endif
  SgVariableDeclaration *decl = NULL;
  ROSE_ASSERT(sym && func_def);

  // Analyze function definition.
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
    if (this_pointer_type == NULL ||
        isSgPointerType(this_pointer_type) == nullptr) {
      cerr << "REX_AST_INVARIANT[outliner-this-result-type]: this shadow "
              "requires the original exact pointer type"
           << endl;
      ROSE_ABORT();
    }
    SgType *var_type = this_pointer_type;
    ROSE_ASSERT(var_type);

    // Build initial value: this pointer
    SgThisExp *this_expr = SageBuilder::buildThisExp(sym, this_pointer_type);
    ROSE_ASSERT(this_expr);
    SgAssignInitializer *init =
        SageBuilder::buildAssignInitializer(this_expr, var_type);

    // Build final declaration.
    decl = SageBuilder::buildVariableDeclaration(var_name, var_type, init,
                                                 func_body);
    // SageBuilder::buildVariableDeclaration (var_name, var_type, init, scope);
    ROSE_ASSERT(decl->get_baseTypeDefiningDeclaration() == nullptr);
    SageInterface::prependStatement(decl, func_body);
  }
  ROSE_ASSERT(decl);

  // We insert it to the enclosing member function definition
  if (enable_debug) {
    cout << "prepending a statement declaring this__ptr into a function body:"
         << func_body << endl;
    cout << "The function body's file info is:" << endl;
    func_body->get_file_info()->display();
    func_body->unparseToString();
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
    SageInterface::publishGeneratedSubtreeOutputOwner(e_repl, e_this);

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

static SgSymbol *getPublishedSymbolForThisPointerType(SgType *pointer_type) {
  SgPointerType *pointer = isSgPointerType(pointer_type);
  SgType *pointee = pointer != NULL ? pointer->get_base_type() : NULL;
  if (pointee != NULL) {
    pointee = pointee->stripType(SgType::STRIP_MODIFIER_TYPE |
                                 SgType::STRIP_TYPEDEF_TYPE);
  }

  if (SgClassType *class_type = isSgClassType(pointee)) {
    SgClassDeclaration *declaration =
        isSgClassDeclaration(class_type->get_declaration());
    SgClassDeclaration *first =
        declaration != NULL
            ? isSgClassDeclaration(
                  declaration->get_firstNondefiningDeclaration())
            : NULL;
    for (SgClassDeclaration *candidate : {declaration, first}) {
      SgSymbol *symbol =
          candidate != NULL ? candidate->get_symbol_from_symbol_table() : NULL;
      if (isSgClassSymbol(symbol) != NULL) {
        return symbol;
      }
    }
    return NULL;
  }

  if (SgNonrealType *nonreal_type = isSgNonrealType(pointee)) {
    SgNonrealDecl *declaration =
        isSgNonrealDecl(nonreal_type->get_declaration());
    return declaration != NULL
               ? isSgNonrealSymbol(declaration->get_symbol_from_symbol_table())
               : NULL;
  }

  return NULL;
}

static SgTemplateClassDeclaration *
getTemplateClassDeclarationFromSymbol(SgClassSymbol *sym) {
  if (sym == NULL)
    return NULL;

  SgClassDeclaration *class_decl = isSgClassDeclaration(sym->get_declaration());
  if (class_decl == NULL)
    return NULL;

  if (SgTemplateClassDeclaration *template_decl =
          isSgTemplateClassDeclaration(class_decl))
    return template_decl;

  // An instantiated class already has an exact concrete class type.  Reusing
  // the primary template parameters here would incorrectly turn Class<int>
  // back into the dependent current instantiation Class<T>.
  if (isSgTemplateInstantiationDecl(class_decl) != NULL)
    return NULL;

  if (SgTemplateClassDeclaration *template_decl = isSgTemplateClassDeclaration(
          class_decl->get_firstNondefiningDeclaration()))
    return template_decl;

  if (SgTemplateClassDeclaration *template_decl =
          isSgTemplateClassDeclaration(class_decl->get_definingDeclaration()))
    return template_decl;

  return NULL;
}

static SgTemplateArgument *
buildTemplateArgumentFromParameter(SgTemplateParameter *param,
                                   size_t position) {
  if (param == NULL) {
    cerr << "REX_TRANSFORM_INVARIANT[current-instantiation-parameter]: "
            "template parameter is null"
         << endl;
    ROSE_ABORT();
  }

  SgTemplateArgument *argument = NULL;
  switch (param->get_parameterType()) {
  case SgTemplateParameter::type_parameter: {
    if (SgType *param_type = param->get_type()) {
      // This argument belongs to the generated declaration of the outlined
      // function's explicit this pointer.  It is therefore a written use-site
      // argument, not an implicit/defaulted semantic argument.
      argument = new SgTemplateArgument(param_type, true);
    }
    break;
  }

  case SgTemplateParameter::nontype_parameter: {
    SgInitializedName *init_name = param->get_initializedName();
    SgType *param_type = init_name != NULL ? init_name->get_type() : NULL;
    const std::string param_name =
        init_name != NULL ? init_name->get_name().getString() : std::string();
    if (init_name == NULL || param_type == NULL || param_name.empty()) {
      break;
    }
    SgTemplateParameterVal *param_value =
        SageBuilder::buildTemplateParameterVal_nfi(position, param_name);
    ROSE_ASSERT(param_value != NULL);
    param_value->set_valueType(param_type);

    argument = new SgTemplateArgument(
        SgTemplateArgument::nontype_argument, false /*isArrayBoundUnknownType*/,
        param_type, param_value, NULL, true /*explicitlySpecified*/);
    param_value->set_parent(argument);
    break;
  }

  case SgTemplateParameter::template_parameter:
    if (SgDeclarationStatement *template_decl =
            param->get_templateDeclaration()) {
      argument = new SgTemplateArgument(
          SgTemplateArgument::template_template_argument,
          false /*isArrayBoundUnknownType*/, NULL /*type*/, NULL /*expression*/,
          template_decl, true /*explicitlySpecified*/);
    }
    break;

  case SgTemplateParameter::parameter_undefined:
    break;
  }

  if (argument == NULL) {
    cerr << "REX_TRANSFORM_INVARIANT[current-instantiation-parameter]: "
            "template parameter at position "
         << position << " has no exact semantic argument" << endl;
    ROSE_ABORT();
  }

  ASTtools::publishTemplateParameterPackExpansion(param, argument);
  return argument;
}

static std::string
templateParameterSemanticName(SgTemplateParameter *parameter) {
  ASSERT_not_null(parameter);
  std::string name;
  switch (parameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter:
    if (SgTemplateType *type = isSgTemplateType(parameter->get_type()))
      name = type->get_name().getString();
    break;
  case SgTemplateParameter::nontype_parameter:
    if (SgInitializedName *initialized_name = parameter->get_initializedName())
      name = initialized_name->get_name().getString();
    break;
  case SgTemplateParameter::template_parameter:
    if (SgTemplateDeclaration *declaration =
            isSgTemplateDeclaration(parameter->get_templateDeclaration())) {
      name = SageInterface::get_name(declaration);
    }
    break;
  case SgTemplateParameter::parameter_undefined:
    break;
  }
  if (name.empty()) {
    std::cerr << "REX_TRANSFORM_INVARIANT[unnamed-template-parameter]: "
                 "outlining cannot construct a current-instantiation type\n";
    ROSE_ABORT();
  }
  if (parameter->get_is_parameter_pack())
    name += "...";
  return name;
}

static SgType *buildCurrentInstantiationThisBaseType(SgClassSymbol *sym) {
  SgTemplateClassDeclaration *template_decl =
      getTemplateClassDeclarationFromSymbol(sym);
  if (template_decl == NULL || template_decl->get_templateParameters().empty())
    return NULL;

  SgTemplateArgumentPtrList template_args;
  SgTemplateParameterPtrList &template_params =
      template_decl->get_templateParameters();
  for (size_t i = 0; i < template_params.size(); ++i) {
    SgTemplateArgument *arg =
        buildTemplateArgumentFromParameter(template_params[i], i);
    ROSE_ASSERT(arg != NULL);
    template_args.push_back(arg);
  }

  SgName template_name = template_decl->get_templateName();
  if (template_name.is_null() || template_name.getString().empty())
    template_name = template_decl->get_name();
  ROSE_ASSERT(!template_name.is_null());

  std::string semantic_name = template_name.getString() + "<";
  for (size_t i = 0; i < template_params.size(); ++i) {
    if (i != 0)
      semantic_name += ", ";
    semantic_name += templateParameterSemanticName(template_params[i]);
  }
  semantic_name += ">";
  const SgName semantic_instantiation_name(semantic_name);

  SgScopeStatement *scope = template_decl->get_scope();
  if (scope == NULL) {
    cerr << "REX_TRANSFORM_INVARIANT[current-instantiation-scope]: template "
            "class declaration has no exact semantic scope"
         << endl;
    ROSE_ABORT();
  }

  SgNonrealType *current_instantiation_type =
      SageBuilder::buildSemanticNonrealType(
          template_name, scope, &template_args, &semantic_instantiation_name);
  SgNonrealDecl *current_instantiation_declaration =
      current_instantiation_type != NULL
          ? isSgNonrealDecl(current_instantiation_type->get_declaration())
          : NULL;
  if (current_instantiation_declaration == NULL ||
      current_instantiation_declaration->get_nonreal_template_role() !=
          SgNonrealDecl::e_nonreal_template_id ||
      current_instantiation_declaration->get_templateDeclaration() != NULL) {
    cerr << "REX_TRANSFORM_INVARIANT[current-instantiation-declaration]: "
            "generated dependent type does not have one unbound typed "
            "declaration"
         << endl;
    ROSE_ABORT();
  }
  current_instantiation_declaration->set_templateDeclaration(template_decl);
  if (current_instantiation_declaration->get_templateDeclaration() !=
      template_decl) {
    cerr << "REX_TRANSFORM_INVARIANT[current-instantiation-declaration]: "
            "generated dependent type did not publish its primary class "
            "template"
         << endl;
    ROSE_ABORT();
  }

  return current_instantiation_type;
}

static SgType *
buildImplicitThisPointerType(SgClassSymbol *sym,
                             const SgMemberFunctionDeclaration *member_decl) {
  SgMemberFunctionType *member_type =
      member_decl != NULL ? isSgMemberFunctionType(member_decl->get_type())
                          : NULL;
  if (sym == NULL || member_decl == NULL || member_type == NULL) {
    cerr << "REX_AST_INVARIANT[outliner-this-result-type]: implicit this use "
            "requires exact class and member-function "
            "semantic types"
         << endl;
    ROSE_ABORT();
  }

  SgType *base_type = buildCurrentInstantiationThisBaseType(sym);
  if (base_type == NULL)
    base_type = sym->get_type();
  if (base_type == NULL) {
    cerr << "REX_AST_INVARIANT[outliner-this-result-type]: class symbol has "
            "no exact semantic type"
         << endl;
    ROSE_ABORT();
  }

  if (member_type->isConstFunc() || member_type->isVolatileFunc() ||
      member_type->isRestrictFunc()) {
    SgModifierType *qualified_type = new SgModifierType(base_type);
    SgTypeModifier &modifier = qualified_type->get_typeModifier();
    if (member_type->isConstFunc())
      modifier.get_constVolatileModifier().setConst();
    if (member_type->isVolatileFunc())
      modifier.get_constVolatileModifier().setVolatile();
    if (member_type->isRestrictFunc())
      modifier.setRestrict();

    SgModifierType *canonical =
        SgModifierType::insertModifierTypeIntoTypeTable(qualified_type);
    if (canonical != qualified_type)
      delete qualified_type;
    base_type = canonical;
  }

  return SgPointerType::createType(base_type);
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

  // Overloaded operator syntax such as `cout << value` can lower to a
  // member-function call wrapped by a user-defined operator node. These calls
  // already have an explicit receiver and must not be rewritten as implicit
  // `this` uses.
  if (call->get_uses_operator_syntax()) {
    return false;
  }

  SgNode *call_parent = call->get_parent();
  if (isSgUserDefinedBinaryOp(call_parent) != NULL ||
      isSgUserDefinedUnaryOp(call_parent) != NULL) {
    return false;
  }

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

  SgClassDefinition *class_scope = isSgClassDefinition(decl->get_scope());
  SgDeclarationGroupStatement *group =
      isSgDeclarationGroupStatement(decl->get_parent());
  const bool direct_member = class_scope != NULL &&
                             decl->get_parent() == class_scope &&
                             class_scope->statementExistsInScope(decl);
  const bool grouped_member =
      class_scope != NULL && group != NULL &&
      group->get_parent() == class_scope && group->get_scope() == class_scope &&
      class_scope->statementExistsInScope(group) &&
      std::count(group->get_declarations().begin(),
                 group->get_declarations().end(), decl) == 1;
  if (!direct_member && !grouped_member)
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

  // A qualified member name denotes the member entity itself, most notably in
  // a pointer-to-member expression such as `&C::field`.  It does not consume
  // an implicit object.  The frontend records that written distinction
  // directly on the reference; using only the surrounding dot/arrow shape
  // mistakes qualified entity references for hidden `this` uses.
  if (var_ref->get_explicit_name_qualification_length() > 0 ||
      var_ref->get_explicit_global_qualification() ||
      !var_ref->get_explicit_name_qualification_tokens().empty()) {
    return false;
  }

  SgNode *parent = var_ref->get_parent();
  SgBinaryOp *binary_parent = isSgBinaryOp(parent);
  if ((isSgDotExp(parent) != NULL || isSgArrowExp(parent) != NULL) &&
      binary_parent != NULL && binary_parent->get_rhs_operand() == var_ref) {
    return false;
  }

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
    if (func_expr == NULL || func_expr->get_parent() != call) {
      cerr << "REX_OUTLINER_INVARIANT[implicit-member-call-owner]: source "
              "callee must be owned directly by its call"
           << endl;
      ROSE_ABORT();
    }
    SgType *function_type = func_expr->get_type();
    if (function_type == NULL) {
      cerr << "REX_OUTLINER_INVARIANT[implicit-member-call-type]: source "
              "callee has no exact semantic type"
           << endl;
      ROSE_ABORT();
    }

    SgVarRefExp *this_ref = SageBuilder::buildVarRefExp(sym);
    ROSE_ASSERT(this_ref != NULL);

    // The strict binary-expression builder adopts detached operands.  The
    // callee is initially owned by the call, so detach that exact edge before
    // publishing it under the new arrow expression.
    call->set_function(NULL);
    func_expr->set_parent(NULL);
    if (call->get_function() != NULL || func_expr->get_parent() != NULL) {
      cerr << "REX_OUTLINER_INVARIANT[implicit-member-call-detach]: callee "
              "ownership was not detached exactly once"
           << endl;
      ROSE_ABORT();
    }

    SgArrowExp *arrow =
        SageBuilder::buildArrowExp(this_ref, func_expr, function_type);
    ROSE_ASSERT(arrow != NULL);
    SageInterface::publishGeneratedSubtreeOutputOwner(arrow, call);

    call->set_function(arrow);
    arrow->set_parent(call);
    if (call->get_function() != arrow || arrow->get_parent() != call ||
        arrow->get_lhs_operand() != this_ref ||
        arrow->get_rhs_operand() != func_expr ||
        this_ref->get_parent() != arrow || func_expr->get_parent() != arrow) {
      cerr << "REX_OUTLINER_INVARIANT[implicit-member-call-publish]: arrow "
              "receiver/callee ownership was not published exactly"
           << endl;
      ROSE_ABORT();
    }
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

    SgArrowExp *arrow = SageBuilder::buildArrowExp(this_ref, member_ref,
                                                   member_ref->get_type());
    ROSE_ASSERT(arrow != NULL);
    SageInterface::publishGeneratedSubtreeOutputOwner(arrow, var_ref);

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
  // Get the exact class/current-instantiation identity for the receiver.
  SgSymbol *sym = NULL;
  SgType *this_pointer_type = NULL;
  const SgFunctionDefinition *func_def = ASTtools::findFirstFuncDef(b);
  const SgMemberFunctionDeclaration *member_decl =
      func_def != NULL
          ? isSgMemberFunctionDeclaration(func_def->get_declaration())
          : NULL;
  if (!this_exprs.empty()) {
    sym = getThisSymbolAndVerify(this_exprs, &this_pointer_type);
  } else {
    SgClassSymbol *class_symbol =
        getClassSymbolFromMemberFunctionDecl(member_decl);
    this_pointer_type = buildImplicitThisPointerType(class_symbol, member_decl);
    sym = getPublishedSymbolForThisPointerType(this_pointer_type);
  }
  if (sym == NULL || this_pointer_type == NULL ||
      isSgPointerType(this_pointer_type) == NULL ||
      (isSgClassSymbol(sym) == NULL && isSgNonrealSymbol(sym) == NULL)) {
    cerr << "REX_AST_INVARIANT[outliner-this-identity]: receiver requires an "
            "exact class/current-instantiation symbol and matching pointer "
            "type"
         << endl;
    ROSE_ABORT();
  }
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
      string("this__ptr__"), sym, const_cast<SgFunctionDefinition *>(func_def),
      this_pointer_type);
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
