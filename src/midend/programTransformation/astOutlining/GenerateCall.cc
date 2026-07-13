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

#include <algorithm>

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
    if (iname == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[call-argument-identity]: variable "
              "symbol=%p name=%s has no exact declaration\n",
              static_cast<const void *>(*i), (*i)->get_name().str());
      ROSE_ABORT();
    }
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
        i_arg = SageBuilder::buildAddressOfOp(
            v_ref, ASTtools::buildAddressOfResultType(v_ref->get_type()));
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
  if (e_list == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[call-argument-list]: wrapper=%s has no "
            "exact call argument list\n",
            arg_name.c_str());
    ROSE_ABORT();
  }
  if ((Outliner::useParameterWrapper || Outliner::useStructureWrapper) &&
      (syms.size() > 0)) {
    ROSE_ASSERT(scope != NULL);
    if (Outliner::useStructureWrapper) {
      // using &_out_argv as a wrapper
      SgVarRefExp *wrapper_ref = SageBuilder::buildVarRefExp(arg_name, scope);
      SageInterface::appendExpression(
          e_list, SageBuilder::buildAddressOfOp(
                      wrapper_ref, ASTtools::buildAddressOfResultType(
                                       wrapper_ref->get_type())));
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

enum class ExplicitTemplateArgumentSurface {
  semantic_declaration,
  generated_reference
};

static SgFunctionDeclaration *
canonicalFunctionFamilyDeclaration(SgFunctionDeclaration *declaration) {
  if (declaration == NULL)
    return NULL;
  if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration()))
    return first;
  if (SgFunctionDeclaration *defining =
          isSgFunctionDeclaration(declaration->get_definingDeclaration()))
    return defining;
  return declaration;
}

static const Outliner::OutlinedLocalTypeTemplateEntry *
findLocalTypeTemplateArgument(
    const Outliner::OutlinedLocalTypeTemplatePlan &plan,
    std::size_t parameter_index) {
  const Outliner::OutlinedLocalTypeTemplateEntry *result = NULL;
  for (const Outliner::OutlinedLocalTypeTemplateEntry &entry : plan.entries) {
    if (entry.template_parameter_index != parameter_index)
      continue;
    if (result != NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-call-argument]: template "
              "parameter index=%zu has more than one exact actual type\n",
              parameter_index);
      ROSE_ABORT();
    }
    result = &entry;
  }
  return result;
}

static void appendExplicitTemplateArguments(
    const SgTemplateParameterPtrList &params, SgTemplateArgumentPtrList &args,
    ExplicitTemplateArgumentSurface surface, SgScopeStatement *call_scope,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_template_plan) {
  int param_index = 0;
  for (SgTemplateParameter *param : params) {
    if (param == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-argument-identity]: null "
              "template parameter at position %d\n",
              param_index);
      ROSE_ABORT();
    }
    SgTemplateArgument *argument = nullptr;
    switch (param->get_parameterType()) {
    case SgTemplateParameter::type_parameter: {
      SgType *arg_type = param->get_type();
      const Outliner::OutlinedLocalTypeTemplateEntry *local_type_entry =
          findLocalTypeTemplateArgument(local_type_template_plan, param_index);
      if (local_type_entry != NULL) {
        SgFunctionDeclaration *call_function =
            SageInterface::getEnclosingFunctionDeclaration(call_scope, true);
        if (local_type_entry->source_type == NULL ||
            local_type_entry->defining_parameter_type == NULL ||
            local_type_entry->owning_function == NULL ||
            canonicalFunctionFamilyDeclaration(call_function) !=
                canonicalFunctionFamilyDeclaration(
                    local_type_entry->owning_function) ||
            isSgTemplateType(arg_type) == NULL) {
          fprintf(
              stderr,
              "REX_OUTLINER_INVARIANT[local-type-call-argument]: "
              "parameter index=%d source=%p generated=%p owner=%p "
              "call-function=%p does not identify one visible exact local "
              "type argument\n",
              param_index, static_cast<void *>(local_type_entry->source_type),
              static_cast<void *>(local_type_entry->defining_parameter_type),
              static_cast<void *>(local_type_entry->owning_function),
              static_cast<void *>(call_function));
          ROSE_ABORT();
        }
        arg_type = local_type_entry->source_type;
      }
      if (isSgTemplateType(arg_type) == NULL &&
          isSgNonrealType(arg_type) == NULL && local_type_entry == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[template-argument-identity]: type "
                "parameter at position %d has no exact template-parameter "
                "type identity\n",
                param_index);
        ROSE_ABORT();
      }
      argument = new SgTemplateArgument(arg_type, true);
      break;
    }
    case SgTemplateParameter::nontype_parameter: {
      SgInitializedName *initialized_name = param->get_initializedName();
      if (initialized_name == NULL ||
          initialized_name->get_name().getString().empty()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[template-argument-identity]: "
                "non-type parameter at position %d has no exact name\n",
                param_index);
        ROSE_ABORT();
      }
      SgTemplateParameterVal *arg_expr =
          SageBuilder::buildTemplateParameterVal_nfi(
              param_index, initialized_name->get_name().getString());
      SgType *value_type = param->get_type();
      if (value_type == NULL)
        value_type = initialized_name->get_type();
      if (value_type == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[template-argument-identity]: "
                "non-type parameter at position %d has no exact type\n",
                param_index);
        ROSE_ABORT();
      }
      arg_expr->set_valueType(value_type);
      if (surface == ExplicitTemplateArgumentSurface::generated_reference) {
        SageInterface::setOneSourcePositionForTransformation(arg_expr);
      }
      argument = new SgTemplateArgument(arg_expr, true);
      break;
    }
    case SgTemplateParameter::template_parameter: {
      SgTemplateDeclaration *template_decl =
          isSgTemplateDeclaration(param->get_templateDeclaration());
      if (template_decl == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[template-argument-identity]: "
                "template parameter at position %d has no exact template "
                "declaration\n",
                param_index);
        ROSE_ABORT();
      }
      argument = new SgTemplateArgument(template_decl, true);
      break;
    }
    default:
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-argument-identity]: template "
              "parameter at position %d has unsupported kind=%d\n",
              param_index, static_cast<int>(param->get_parameterType()));
      ROSE_ABORT();
    }
    ROSE_ASSERT(argument != nullptr);
    ASTtools::publishTemplateParameterPackExpansion(param, argument);
    args.push_back(argument);
    ++param_index;
  }
}

static SgNonrealRefExp *buildExplicitTemplateReferenceForCall(
    SgTemplateFunctionDeclaration *source_template,
    SgScopeStatement *call_scope,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_template_plan) {
  ROSE_ASSERT(source_template != NULL);
  ROSE_ASSERT(call_scope != NULL);

  const SgTemplateParameterPtrList &parameters =
      source_template->get_templateParameters();
  if (parameters.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[template-call-identity]: source "
            "template=%p name=%s has no template parameters\n",
            static_cast<void *>(source_template),
            source_template->get_name().str());
    ROSE_ABORT();
  }

  SgTemplateArgumentPtrList declaration_arguments;
  appendExplicitTemplateArguments(
      parameters, declaration_arguments,
      ExplicitTemplateArgumentSurface::semantic_declaration, call_scope,
      local_type_template_plan);
  ROSE_ASSERT(declaration_arguments.size() == parameters.size());

  // A dependent template-id needs a distinct spelling identity in the
  // declaration scope used by SgNonrealRefExp.  Its typed arguments remain the
  // source of truth; this semantic key is used only to keep different
  // dependent argument lists from aliasing one declaration.
  std::ostringstream semantic_name;
  semantic_name << source_template->get_name().str() << '<';
  bool first_argument = true;
  for (SgTemplateArgument *argument : declaration_arguments) {
    if (argument == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-call-identity]: template=%s "
              "contains a null explicit argument\n",
              source_template->get_name().str());
      ROSE_ABORT();
    }
    const std::string mangled_argument = argument->get_mangled_name().str();
    if (mangled_argument.empty()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-call-identity]: template=%s "
              "contains an argument without semantic identity\n",
              source_template->get_name().str());
      ROSE_ABORT();
    }
    if (!first_argument)
      semantic_name << ',';
    first_argument = false;
    semantic_name << mangled_argument.size() << ':' << mangled_argument;
  }
  semantic_name << '>';
  const SgName semantic_identity(semantic_name.str());

  SgNonrealType *spelling_type = SageBuilder::buildSemanticNonrealType(
      source_template->get_name(), call_scope, &declaration_arguments,
      &semantic_identity);
  SgNonrealDecl *spelling_declaration =
      spelling_type != NULL ? isSgNonrealDecl(spelling_type->get_declaration())
                            : NULL;
  SgNonrealSymbol *spelling_symbol =
      spelling_declaration != NULL
          ? isSgNonrealSymbol(
                spelling_declaration->get_symbol_from_symbol_table())
          : NULL;
  if (spelling_declaration == NULL || spelling_symbol == NULL ||
      spelling_symbol->get_declaration() != spelling_declaration) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[template-call-spelling]: template=%s "
            "did not produce one exact typed spelling declaration\n",
            source_template->get_name().str());
    ROSE_ABORT();
  }
  spelling_declaration->set_templateDeclaration(source_template);

  SgNonrealRefExp *reference =
      SageBuilder::buildNonrealRefExp_nfi(spelling_symbol);
  ROSE_ASSERT(reference != NULL);
  SgTemplateArgumentPtrList reference_arguments;
  appendExplicitTemplateArguments(
      parameters, reference_arguments,
      ExplicitTemplateArgumentSurface::generated_reference, call_scope,
      local_type_template_plan);
  reference->get_templateArguments() = reference_arguments;
  SageBuilder::setTemplateArgumentParents(reference);
  reference->set_explicit_template_argument_list(true);
  reference->set_resolved_function_declaration(source_template);
  SageInterface::setOneSourcePositionForTransformation(reference);
  SageInterface::requireResolvedFunctionTemplateReference(
      reference, "Outliner generated template call");
  if (reference->get_resolved_function_declaration() != source_template ||
      reference->get_templateArguments().size() != parameters.size() ||
      spelling_declaration->get_templateDeclaration() != source_template) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[template-call-spelling]: template=%s "
            "lost its exact source declaration or explicit typed arguments\n",
            source_template->get_name().str());
    ROSE_ABORT();
  }
  return reference;
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
    SgFunctionDeclaration
        *source_call_declaration, // exact source-visible declaration to call
    const ASTtools::VarSymSet_t
        &syms, // variables for generating function arguments
    const std::set<SgInitializedName *>
        varsUsingOriginalForm, // used to the classic outlining without wrapper:
                               // using a (originalForm) vs. &a
    std::string wrapper_name,  // when parameter wrapping is used, provide
                               // wrapper argument's name
    SgScopeStatement *scope,   // the scope in which we insert the function call
    const OutlinedLocalTypeTemplatePlan &local_type_template_plan) {
  // Create a reference to the function.
  SgGlobal *glob_scope = SageInterface::getGlobalScope(scope);
  ROSE_ASSERT(glob_scope != NULL);
  if (source_call_declaration == NULL ||
      source_call_declaration->get_firstNondefiningDeclaration() !=
          source_call_declaration ||
      source_call_declaration->get_scope() == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[source-call-declaration]: declaration=%p "
            "must be the exact canonical declaration with an explicit "
            "semantic scope\n",
            static_cast<void *>(source_call_declaration));
    ROSE_ABORT();
  }

  SgScopeStatement *declaration_scope = source_call_declaration->get_scope();
  SgSymbol *exact_symbol =
      source_call_declaration->get_symbol_from_symbol_table();
  SgSymbol *scope_symbol =
      declaration_scope->find_symbol_from_declaration(source_call_declaration);
  if (exact_symbol == NULL || scope_symbol != exact_symbol ||
      exact_symbol->get_symbol_basis() != source_call_declaration ||
      exact_symbol->get_parent() != declaration_scope->get_symbol_table()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[source-call-symbol]: declaration=%p "
            "name=%s symbol=%p scope-symbol=%p has no exact semantic "
            "publication\n",
            static_cast<void *>(source_call_declaration),
            source_call_declaration->get_name().str(),
            static_cast<void *>(exact_symbol),
            static_cast<void *>(scope_symbol));
    ROSE_ABORT();
  }

  if (!SageInterface::is_Fortran_language()) {
    const SgDeclarationStatementPtrList &declarations =
        glob_scope->get_declarations();
    if (declaration_scope != glob_scope ||
        source_call_declaration->get_parent() != glob_scope ||
        std::count(declarations.begin(), declarations.end(),
                   source_call_declaration) != 1 ||
        source_call_declaration->get_file_info() == NULL ||
        !source_call_declaration->get_file_info()->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-call-declaration]: "
              "declaration=%p name=%s is not one exact source-global "
              "declaration\n",
              static_cast<void *>(source_call_declaration),
              source_call_declaration->get_name().str());
      ROSE_ABORT();
    }
  }

  SgExpression *func_ref = NULL;
  if (SgTemplateFunctionDeclaration *template_func =
          isSgTemplateFunctionDeclaration(source_call_declaration)) {
    SgTemplateFunctionSymbol *template_symbol =
        isSgTemplateFunctionSymbol(exact_symbol);
    if (template_symbol == NULL ||
        template_symbol->get_declaration() != source_call_declaration) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-call-symbol]: source template=%p "
              "name=%s has no exact template-function symbol\n",
              static_cast<void *>(source_call_declaration),
              source_call_declaration->get_name().str());
      ROSE_ABORT();
    }
    if (template_func->get_templateParameters().empty()) {
      SgTemplateFunctionRefExp *template_ref =
          SageBuilder::buildTemplateFunctionRefExp_nfi(template_symbol);
      SageInterface::setOneSourcePositionForTransformation(template_ref);
      func_ref = template_ref;
    } else {
      func_ref = buildExplicitTemplateReferenceForCall(
          template_func, scope, local_type_template_plan);
    }
  } else {
    SgFunctionSymbol *func_symbol = isSgFunctionSymbol(exact_symbol);
    if (func_symbol == NULL ||
        isSgTemplateFunctionSymbol(func_symbol) != NULL ||
        func_symbol->get_declaration() != source_call_declaration) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-call-symbol]: source "
              "declaration=%p name=%s has no exact function symbol\n",
              static_cast<void *>(source_call_declaration),
              source_call_declaration->get_name().str());
      ROSE_ABORT();
    }
    func_ref = SageInterface::is_Fortran_language()
                   ? static_cast<SgExpression *>(
                         SageBuilder::buildFortranFunctionRefExp(
                             func_symbol, func_symbol,
                             SgFunctionRefExp::
                                 e_fortran_source_visible_binding_exact_typed))
                   : static_cast<SgExpression *>(
                         SageBuilder::buildFunctionRefExp(func_symbol));
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
  SgFunctionType *function_type = source_call_declaration->get_type();
  ROSE_ASSERT(function_type != nullptr);
  ROSE_ASSERT(function_type->get_return_type() != nullptr);
  SgFunctionCallExp *func_call_expr = SageBuilder::buildFunctionCallExp(
      func_ref, function_type->get_return_type(), exp_list_exp);
  ROSE_ASSERT(func_call_expr);

  SgExprStatement *func_call_stmt =
      SageBuilder::buildExprStatement(func_call_expr);
  ROSE_ASSERT(func_call_stmt);

  return func_call_stmt;
}

// eof
