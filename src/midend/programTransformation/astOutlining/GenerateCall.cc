/**
 *  \file Transform/GenerateCall.cc
 *
 *  \brief Given the outlined-function, this routine generates the
 *  actual function call.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageInterface.h"

#include "sageBuilder.h"

#include <iostream>

#include <sstream>

#include <string>

#include "ASTtools.hh"

#include "Outliner.hh"

#include "StmtRewrite.hh"

#include "VarSym.hh"

// =====================================================================

using namespace std;

// =====================================================================

// Generate a parameter list for a function call:
//  The caller of this function should analyze (figure out) exactly which form
//  to use . This function only takes care of the actual transformation.
//
// const ASTtools::VarSymSet_t& syms: a set of variables to be used in the
// parameter list
//    each input variable in the syms will be converted to a function parameter
//    either using its original form (a) or addressOf form (&a)
// std::set<SgInitializedName*> varUsingOriginalType:
//             indicate if some of the syms should using original types:  passed
//             by value (original type) for scalar, in C/C++;  arrays for syms -
//             varUsingOriginalType==> the rest will be variables using address
//             of (&A): passed-by-reference: original type for arrays  or
//             address of for others
void Outliner::appendIndividualFunctionCallArgs(
    const ASTtools::VarSymSet_t &syms,
    const std::set<SgInitializedName *> varUsingOriginalType,
    SgExprListExp *e_list) {
  for (ASTtools::VarSymSet_t::const_iterator i = syms.begin(); i != syms.end();
       ++i) {
    bool using_orig_type = false;
    SgInitializedName *iname = (*i)->get_declaration();
    if (iname)
      if (varUsingOriginalType.find(iname) != varUsingOriginalType.end())
        using_orig_type = true;

    // Create variable reference to pass to the function.
    SgVarRefExp *v_ref =
        SageBuilder::buildVarRefExp(const_cast<SgVariableSymbol *>(*i));
    ROSE_ASSERT(v_ref);
    // Liao, 12/14/2007  Pass by reference is default behavior for Fortran
    if (SageInterface::is_Fortran_language())
      e_list->append_expression(v_ref);
    else // C/C++ call convention
    {
      // Construct actual function argument. //TODO: consider array types, they
      // can only be passed by reference, no further addressing/de-referencing
      // is needed
      SgExpression *i_arg = NULL;
      if (Outliner::enable_classic &&
          using_orig_type) // TODO expand to the default case also: using local
                           // declaration for transfer parameters
                           //      if (using_orig_type ) // using a
      { // classic translation, read only variable, pass by value directly
        i_arg = v_ref;
      } else // conservatively always use &a for the default case (no wrapper,
             // none classic)
      {
        i_arg = SageBuilder::buildAddressOfOp(v_ref);
      }
      ROSE_ASSERT(i_arg);
      e_list->append_expression(i_arg);
    } // end if
  } // end for
}

// Append a single wrapper argument for a call to the outlined function
//
// ASTtools::VarSymSet_t& syms: original list of variables to be passed to the
// outlined function
//  std::string arg_name: name for the wrapper argument enclosing all syms
//  SgExprListExp* e_list: parameter list to be expanded
// SgScopeStatement* scope : scope of the function call to be inserted into
static void appendSingleWrapperArgument(const ASTtools::VarSymSet_t &syms,
                                        std::string arg_name,
                                        SgExprListExp *e_list,
                                        SgScopeStatement *scope) {
  if (!e_list)
    return;
  if ((Outliner::useParameterWrapper || Outliner::useStructureWrapper) &&
      (syms.size() > 0)) {
    ROSE_ASSERT(scope != NULL);
    if (Outliner::useStructureWrapper) {
      // using &_out_argv as a wrapper
      SageInterface::appendExpression(
          e_list, SageBuilder::buildAddressOfOp(
                      SageBuilder::buildVarRefExp(arg_name, scope)));
    } else // using array of pointers wrapper
    {
      // using void * __out_argv[n] as a wrapper
      SageInterface::appendExpression(
          e_list, SageBuilder::buildVarRefExp(arg_name, scope));
    }
  } else if ((Outliner::useStructureWrapper || Outliner::useParameterWrapper) &&
             (syms.size() == 0)) {
    // TODO: move this outside of outliner since it is OpenMP-specific
    // For OpenMP lowering, we have to have a void * parameter even if there is
    // no need to pass any parameters in order to match the gomp runtime lib 's
    // function prototype for function pointers
    SgFile *cur_file = SageInterface::getEnclosingFileNode(scope);
    ROSE_ASSERT(cur_file != NULL);
    // if (cur_file->get_openmp_lowering ())
    {
      SageInterface::appendExpression(e_list, SageBuilder::buildIntVal(0));
    }
  }
}

static void
appendExplicitTemplateArguments(const SgTemplateParameterPtrList &params,
                                SgTemplateArgumentPtrList &args) {
  int param_index = 0;
  for (SgTemplateParameter *param : params) {
    if (param == NULL) {
      ++param_index;
      continue;
    }
    switch (param->get_parameterType()) {
    case SgTemplateParameter::type_parameter: {
      SgType *arg_type = param->get_type();
      if (arg_type == NULL)
        arg_type = param->get_defaultTypeParameter();
      if (arg_type != NULL) {
        args.push_back(new SgTemplateArgument(arg_type, true));
      }
      break;
    }
    case SgTemplateParameter::nontype_parameter: {
      SgExpression *arg_expr = param->get_expression();
      if (arg_expr == NULL) {
        SgTemplateParameterVal *param_val =
            SageBuilder::buildTemplateParameterVal_nfi(param_index, "");
        SgType *value_type = param->get_type();
        if (value_type == NULL && param->get_initializedName() != NULL)
          value_type = param->get_initializedName()->get_type();
        if (value_type != NULL)
          param_val->set_valueType(value_type);
        arg_expr = param_val;
      }
      if (arg_expr != NULL) {
        args.push_back(new SgTemplateArgument(arg_expr, true));
      }
      break;
    }
    case SgTemplateParameter::template_parameter: {
      SgTemplateDeclaration *template_decl =
          isSgTemplateDeclaration(param->get_templateDeclaration());
      if (template_decl != NULL) {
        args.push_back(new SgTemplateArgument(template_decl, true));
      }
      break;
    }
    default:
      break;
    }
    ++param_index;
  }
}

static SgFunctionDeclaration *
buildTemplateInstantiationForCall(SgTemplateFunctionDeclaration *template_func,
                                  SgScopeStatement *scope) {
  if (template_func == NULL || scope == NULL)
    return NULL;

  const SgTemplateParameterPtrList *params =
      &(template_func->get_templateParameters());
  SgTemplateFunctionDeclaration *template_decl = template_func;
  if (params->empty()) {
    if (SgTemplateFunctionDeclaration *first = isSgTemplateFunctionDeclaration(
            template_func->get_firstNondefiningDeclaration())) {
      params = &(first->get_templateParameters());
      template_decl = first;
    }
  }
  if (params->empty())
    return NULL;

  SgTemplateArgumentPtrList template_args;
  appendExplicitTemplateArguments(*params, template_args);
  if (template_args.empty())
    return NULL;

  SgFunctionParameterList *param_list =
      SageInterface::deepCopy<SgFunctionParameterList>(
          template_func->get_parameterList());
  ROSE_ASSERT(param_list != NULL);

  SgFunctionType *func_type = template_func->get_type();
  ROSE_ASSERT(func_type != NULL);

  SgFunctionDeclaration *inst_decl =
      SageBuilder::buildNondefiningFunctionDeclaration(
          template_func->get_name(), func_type->get_return_type(), param_list,
          scope, true, &template_args,
          template_func->get_declarationModifier()
              .get_storageModifier()
              .get_modifier());
  ROSE_ASSERT(inst_decl != NULL);

  SgTemplateInstantiationFunctionDecl *inst_func =
      isSgTemplateInstantiationFunctionDecl(inst_decl);
  ROSE_ASSERT(inst_func != NULL);

  inst_func->set_templateDeclaration(template_decl);
  inst_func->set_template_argument_list_is_explicit(true);

  if (inst_func->get_startOfConstruct() != NULL) {
    inst_func->get_startOfConstruct()->setCompilerGenerated();
    inst_func->get_startOfConstruct()->unsetOutputInCodeGeneration();
  }
  if (inst_func->get_endOfConstruct() != NULL) {
    inst_func->get_endOfConstruct()->setCompilerGenerated();
    inst_func->get_endOfConstruct()->unsetOutputInCodeGeneration();
  }

  return inst_decl;
}

// =====================================================================

//! Generate a call to the outlined function
//  We have two ways to generate the corresponding parameter lists
//  Choice 1: each variable is converted to a function parameter.
//      varsUsingOriginalForm: is used to decide if  original form (a) should be
//      used, the rest should use addressOf form (&a) wrapper_name: is
//      irrelevant in this case
//  Choice 2: all variables are wrapped into a single parameter
//     wrapper_name: is the name of the wrapper parameter
//     varsUsingOriginalForm: is irrelevant in this choice
SgStatement *Outliner::generateCall(
    SgFunctionDeclaration *out_func, // the outlined function we want to call
    const ASTtools::VarSymSet_t
        &syms, // variables for generating function arguments
    const std::set<SgInitializedName *>
        varsUsingOriginalForm, // used to the classic outlining without wrapper:
                               // using a (originalForm) vs. &a
    std::string wrapper_name,  // when parameter wrapping is used, provide
                               // wrapper argument's name
    SgScopeStatement *scope)   // the scope in which we insert the function call
{
  // Create a reference to the function.
  SgGlobal *glob_scope = SageInterface::getGlobalScope(scope);
  ROSE_ASSERT(glob_scope != NULL);
  SgExpression *func_ref = NULL;
  if (SgTemplateFunctionDeclaration *template_func =
          isSgTemplateFunctionDeclaration(out_func)) {
    SgFunctionDeclaration *inst_decl =
        buildTemplateInstantiationForCall(template_func, glob_scope);
    if (inst_decl != NULL) {
      func_ref = SageBuilder::buildFunctionRefExp(inst_decl);
    } else {
      SgTemplateFunctionSymbol *template_symbol =
          glob_scope->lookup_template_function_symbol(
              template_func->get_name(), template_func->get_type(),
              &(template_func->get_templateParameters()));
      ROSE_ASSERT(template_symbol != NULL);
      func_ref = SageBuilder::buildTemplateFunctionRefExp_nfi(template_symbol);
    }
  } else {
    SgFunctionSymbol *func_symbol = glob_scope->lookup_function_symbol(
        out_func->get_name(), out_func->get_type());
    if (func_symbol == NULL) {
      func_symbol = glob_scope->lookup_function_symbol(out_func->get_name());
    }
    if (func_symbol == NULL) {
      SgFunctionDeclaration *first_nondef =
          isSgFunctionDeclaration(out_func->get_firstNondefiningDeclaration());
      if (first_nondef != NULL) {
        func_symbol = isSgFunctionSymbol(
            glob_scope->find_symbol_from_declaration(first_nondef));
        if (func_symbol == NULL) {
          func_symbol =
              isSgFunctionSymbol(first_nondef->get_symbol_from_symbol_table());
        }
        if (func_symbol == NULL) {
          SgFunctionDeclaration *bridge_decl = first_nondef;
          if (bridge_decl->get_scope() != glob_scope) {
            bridge_decl = SageBuilder::buildNondefiningFunctionDeclaration(
                bridge_decl, glob_scope);
          }
          func_symbol = new SgFunctionSymbol(bridge_decl);
          glob_scope->insert_symbol(bridge_decl->get_name(), func_symbol);
        }
      }
    }
    if (func_symbol == NULL) {
      printf("Failed to find a function symbol in %p for function %s\n",
             glob_scope, out_func->get_name().getString().c_str());
      ROSE_ASSERT(func_symbol != NULL);
    }
    ROSE_ASSERT(func_symbol);
    func_ref = SageBuilder::buildFunctionRefExp(func_symbol);
  }
  ROSE_ASSERT(func_ref != NULL);

  // Create an argument list.
  SgExprListExp *exp_list_exp = SageBuilder::buildExprListExp();
  ROSE_ASSERT(exp_list_exp);
  // appendArgs (syms, readOnlyVars, wrapper_name, exp_list_exp,scope);
  if (Outliner::useParameterWrapper || Outliner::useStructureWrapper)
    appendSingleWrapperArgument(syms, wrapper_name, exp_list_exp, scope);
  else
    appendIndividualFunctionCallArgs(syms, varsUsingOriginalForm, exp_list_exp);

  // Generate the actual call.
  SgFunctionCallExp *func_call_expr =
      SageBuilder::buildFunctionCallExp(func_ref, exp_list_exp);
  ROSE_ASSERT(func_call_expr);

  SgExprStatement *func_call_stmt =
      SageBuilder::buildExprStatement(func_call_expr);
  ROSE_ASSERT(func_call_stmt);

  return func_call_stmt;
}

// eof
