/**
 *  \file Transform/GenerateFunc.cc
 *
 *  \brief Generates an outlined (independent) C-callable function
 *  from an SgBasicBlock.
 *
 *  This outlining implementation specifically generates C-callable
 *  routines for use in an empirical tuning application. Such routines
 *  can be isolated into their own, dynamically shareable modules.
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

// DQ (8/28/2012): Added this so that we can see where the macros
// are used to control the use of new vs. old template support.
#include "rose_config.h"

#include "sageBuilder.h"

#include <iostream>

#include <map>

#include <set>

#include <sstream>

#include <string>

#include <unordered_map>

#include <vector>

#include "ASTtools.hh"

#include "Copy.hh"

#include "Outliner.hh"

#include "RoseAst.h"

#include "StmtRewrite.hh"

#include "VarSym.hh"

//! Stores a variable symbol remapping.
typedef std::map<const SgVariableSymbol *, SgVariableSymbol *> VarSymRemap_t;

extern std::map<SgOmpExecStatement *,
                std::map<SgInitializedName *, SgExpression *> *>
    clause_variable_renaming_record;
// =====================================================================

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;

/* ===========================================================
 */

namespace {

bool isExactFortranSemanticProcedurePublication(SgFunctionSymbol *symbol) {
  SgProcedureHeaderStatement *declaration =
      symbol != NULL ? isSgProcedureHeaderStatement(symbol->get_declaration())
                     : NULL;
  SgScopeStatement *scope = symbol != NULL ? symbol->get_scope() : NULL;
  SgSymbolTable *table = scope != NULL ? scope->get_symbol_table() : NULL;
  SgAuxiliaryDeclarationList *owner =
      declaration != NULL
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : NULL;
  Sg_File_Info *source =
      declaration != NULL ? declaration->get_file_info() : NULL;
  const SgDeclarationStatementPtrList *declarations =
      owner != NULL ? &owner->get_declarations() : NULL;
  return declaration != NULL && scope != NULL && table != NULL &&
         owner != NULL && owner->get_parent() == scope &&
         scope->get_auxiliary_declarations() == owner &&
         declaration->get_scope() == scope &&
         declaration->get_firstNondefiningDeclaration() == declaration &&
         declaration->get_fortran_procedure_source_form() ==
             SgProcedureHeaderStatement::
                 e_fortran_procedure_source_form_semantic_only &&
         source != NULL && source->isCompilerGenerated() &&
         source->isOutputInCodeGeneration() && declarations != NULL &&
         std::count(declarations->begin(), declarations->end(), declaration) ==
             1 &&
         symbol->get_declaration() == declaration &&
         symbol->get_parent() == table && table->exists(symbol) &&
         scope->find_symbol_from_declaration(declaration) == symbol;
}

struct OutlinedFunctionParameterPlan {
  SgFunctionParameterList *definition_parameters = NULL;
  SgFunctionParameterList *syntax_parameters = NULL;
  bool has_distinct_source_parameters = false;
  SgInitializedName *wrapper_parameter = NULL;
  std::map<const SgVariableSymbol *, SgInitializedName *> direct_parameters;
  std::map<const SgVariableSymbol *, SgInitializedName *>
      direct_syntax_parameters;
};

SgFunctionParameterList *
cloneCanonicalParameterList(SgFunctionParameterList *definition_parameters) {
  if (definition_parameters == NULL ||
      definition_parameters->get_parent() != NULL) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[function-signature-construction]: "
                    "defining parameter list is null or already owned\n");
    ROSE_ABORT();
  }

  SgFunctionParameterList *canonical_parameters =
      buildFunctionParameterList_nfi();
  if (canonical_parameters == NULL ||
      canonical_parameters == definition_parameters ||
      canonical_parameters->get_parent() != NULL ||
      !canonical_parameters->get_args().empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[function-signature-construction]: "
            "canonical parameter list is not one detached semantic surface\n");
    ROSE_ABORT();
  }

  for (SgInitializedName *definition_parameter :
       definition_parameters->get_args()) {
    if (definition_parameter == NULL ||
        definition_parameter->get_type() == NULL ||
        definition_parameter->get_initializer() != NULL ||
        definition_parameter->get_parent() != definition_parameters) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-construction]: "
              "defining parameter is null, untyped, initialized, or "
              "structurally detached\n");
      ROSE_ABORT();
    }
    SgInitializedName *canonical_parameter = buildInitializedName_nfi(
        definition_parameter->get_name(), definition_parameter->get_type(),
        /*initializer=*/NULL);
    canonical_parameters->append_arg(canonical_parameter);
  }

  for (size_t index = 0; index < definition_parameters->get_args().size();
       ++index) {
    SgInitializedName *definition_parameter =
        definition_parameters->get_args()[index];
    SgInitializedName *canonical_parameter =
        canonical_parameters->get_args()[index];
    if (definition_parameter == NULL || canonical_parameter == NULL ||
        definition_parameter == canonical_parameter ||
        definition_parameter->get_parent() != definition_parameters ||
        canonical_parameter->get_parent() != canonical_parameters ||
        definition_parameter->get_name() != canonical_parameter->get_name() ||
        definition_parameter->get_type() != canonical_parameter->get_type()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-construction]: "
              "parameter index=%zu has no distinct exact canonical copy\n",
              index);
      ROSE_ABORT();
    }
  }
  return canonical_parameters;
}

void validateOutlinedFunctionSignature(
    SgFunctionDeclaration *definition,
    SgFunctionParameterList *expected_definition_parameters) {
  SgFunctionDeclaration *canonical =
      definition != NULL ? isSgFunctionDeclaration(
                               definition->get_firstNondefiningDeclaration())
                         : NULL;
  SgFunctionParameterList *definition_parameters =
      definition != NULL ? definition->get_parameterList() : NULL;
  SgFunctionParameterList *canonical_parameters =
      canonical != NULL ? canonical->get_parameterList() : NULL;
  SgFunctionType *definition_type =
      definition != NULL ? definition->get_type() : NULL;
  SgFunctionType *canonical_type =
      canonical != NULL ? canonical->get_type() : NULL;

  if (definition == NULL || definition->get_definition() == NULL ||
      definition->get_definingDeclaration() != definition ||
      canonical == NULL || canonical == definition ||
      canonical->get_firstNondefiningDeclaration() != canonical ||
      canonical->get_definingDeclaration() != definition ||
      definition_parameters == NULL ||
      definition_parameters != expected_definition_parameters ||
      definition_parameters->get_parent() != definition ||
      canonical_parameters == NULL ||
      canonical_parameters == definition_parameters ||
      canonical_parameters->get_parent() != canonical ||
      definition_type == NULL || canonical_type != definition_type) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[function-signature-family]: defining and "
            "canonical declarations were not published as one exact typed "
            "family\n");
    ROSE_ABORT();
  }

  const SgInitializedNamePtrList &definition_arguments =
      definition_parameters->get_args();
  const SgInitializedNamePtrList &canonical_arguments =
      canonical_parameters->get_args();
  const SgTypePtrList &function_argument_types =
      definition_type->get_arguments();
  if (definition_arguments.size() != canonical_arguments.size() ||
      definition_arguments.size() != function_argument_types.size()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[function-signature-family]: declaration "
            "and function-type arities differ\n");
    ROSE_ABORT();
  }

  for (size_t index = 0; index < definition_arguments.size(); ++index) {
    SgInitializedName *definition_argument = definition_arguments[index];
    SgInitializedName *canonical_argument = canonical_arguments[index];
    if (definition_argument == NULL || canonical_argument == NULL ||
        definition_argument == canonical_argument ||
        definition_argument->get_parent() != definition_parameters ||
        canonical_argument->get_parent() != canonical_parameters ||
        definition_argument->get_name() != canonical_argument->get_name() ||
        definition_argument->get_type() != canonical_argument->get_type() ||
        definition_argument->get_type() != function_argument_types[index]) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-family]: parameter "
              "index=%zu differs across the exact declaration family\n",
              index);
      ROSE_ABORT();
    }
  }
}

} // namespace

//! Creates a non-member function.
static SgFunctionDeclaration *
createFuncSkeleton(const string &name, SgType *ret_type,
                   SgFunctionParameterList *params, SgScopeStatement *scope) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(Outliner::isValidOutliningScope(scope));
  SgFunctionDeclaration *func;
  SgProcedureHeaderStatement *fortranRoutine;
  // Liao 12/13/2007, generate SgProcedureHeaderStatement for Fortran code
  if (SageInterface::is_Fortran_language()) {
    SgFunctionParameterList *canonical_params =
        cloneCanonicalParameterList(params);
    fortranRoutine = SageBuilder::buildProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::sourceLexicalIn(scope),
        SgName(name), ret_type, params, canonical_params,
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
        scope);
    func = isSgFunctionDeclaration(fortranRoutine);
  } else {
    SgFunctionParameterList *canonical_params =
        cloneCanonicalParameterList(params);
    SgFunctionDeclaration *canonical =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            name, ret_type, canonical_params, scope);
    if (canonical == NULL ||
        canonical->get_firstNondefiningDeclaration() != canonical) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-construction]: "
              "outlined function has no exact canonical declaration\n");
      ROSE_ABORT();
    }
    func = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexicalIn(scope),
        name, ret_type, params, scope,
        /*buildTemplateInstantiation=*/false, canonical,
        /*templateArgumentsList=*/NULL,
        /*forceFreeFunctionScope=*/false);
  }

  validateOutlinedFunctionSignature(func, params);

  // Preserve scope language semantics for generated outlined routines.
  if (SageInterface::is_Fortran_language() || scope->isCaseInsensitive()) {
    if (SgFunctionDefinition *func_def = func->get_definition()) {
      func_def->setCaseInsensitive(true);
      if (SgBasicBlock *func_body = func_def->get_body())
        func_body->setCaseInsensitive(true);
    }
  }

  SgFunctionSymbol *func_symbol =
      scope->lookup_function_symbol(func->get_name());
  ROSE_ASSERT(func_symbol != NULL);
  if (Outliner::enable_debug) {
    printf("Found function symbol in %p for function:%s\n", scope,
           func->get_name().getString().c_str());
  }
  return func;
}

static bool collectNamedNamespaceQualifierTokens(SgScopeStatement *scope,
                                                 SgStringList &tokens) {
  tokens.clear();

  std::vector<std::string> reversed_tokens;
  std::set<SgScopeStatement *> visited;
  for (SgScopeStatement *current = scope; current != NULL;
       current = current->get_scope()) {
    if (!visited.insert(current).second)
      break;

    if (isSgGlobal(current) != NULL)
      break;

    SgNamespaceDefinitionStatement *namespace_definition =
        isSgNamespaceDefinitionStatement(current);
    if (namespace_definition == NULL)
      continue;

    SgNamespaceDeclarationStatement *namespace_declaration =
        namespace_definition->get_namespaceDeclaration();
    ROSE_ASSERT(namespace_declaration != NULL);

    const std::string name = namespace_declaration->get_name().getString();
    if (name.empty())
      return false;

    // These are source-spelling fragments for a newly generated qualified
    // reference, not semantic namespace names.  Preserve the delimiter in the
    // producer-owned token exactly as the Clang frontend does for written
    // qualifiers.
    reversed_tokens.push_back(name + "::");
  }

  if (reversed_tokens.empty())
    return false;

  for (std::vector<std::string>::reverse_iterator it = reversed_tokens.rbegin();
       it != reversed_tokens.rend(); ++it) {
    tokens.push_back(*it);
  }
  return true;
}

static void preserveMovedNamespaceFunctionBindings(SgBasicBlock *func_body) {
  ROSE_ASSERT(func_body != NULL);

  RoseAst ast(func_body);
  for (RoseAst::iterator it = ast.begin(); it != ast.end(); ++it) {
    SgFunctionRefExp *function_ref = isSgFunctionRefExp(*it);
    if (function_ref == NULL)
      continue;

    SgFunctionSymbol *function_symbol = function_ref->get_symbol();
    if (function_symbol == NULL)
      continue;

    SgFunctionDeclaration *function_declaration =
        function_symbol->get_declaration();
    if (function_declaration == NULL)
      continue;

    SgStringList qualifier_tokens;
    if (!collectNamedNamespaceQualifierTokens(function_declaration->get_scope(),
                                              qualifier_tokens))
      continue;

    function_ref->set_explicit_global_qualification(true);
    function_ref->set_explicit_name_qualification_tokens(qualifier_tokens);
    function_ref->set_explicit_name_qualification_length(
        static_cast<int>(qualifier_tokens.size()));
    function_ref->set_global_qualification_required(true);
    function_ref->set_name_qualification_length(
        static_cast<int>(qualifier_tokens.size()));
    function_ref->markAsModified();
  }
}

static void publishMovedFortranProcedureDependencies(SgBasicBlock *func_body) {
  ROSE_ASSERT(func_body != NULL);

  const Rose_STL_Container<SgNode *> reference_nodes =
      NodeQuery::querySubTree(func_body, V_SgFunctionRefExp);
  std::map<SgFunctionSymbol *, SgRenameSymbol *> generic_bindings;
  for (SgNode *node : reference_nodes) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    SgFunctionSymbol *source_semantic =
        reference != NULL ? reference->get_symbol() : NULL;
    SgFunctionDeclaration *source_declaration =
        source_semantic != NULL ? source_semantic->get_declaration() : NULL;
    SgInterfaceBody *source_body =
        source_declaration != NULL
            ? isSgInterfaceBody(source_declaration->get_parent())
            : NULL;
    SgInterfaceStatement *source_interface =
        source_body != NULL ? isSgInterfaceStatement(source_body->get_parent())
                            : NULL;
    SgFunctionType *type = source_semantic != NULL
                               ? isSgFunctionType(source_semantic->get_type())
                               : NULL;
    if (reference == NULL || source_semantic == NULL ||
        source_declaration == NULL || type == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[fortran-interface-dependency]: "
              "outlined function contains a malformed procedure reference\n");
      ROSE_ABORT();
    }

    SgFunctionSymbol *visible =
        SageInterface::lookupFunctionSymbolInParentScopes(
            source_semantic->get_name(), type, func_body);
    if (visible == source_semantic) {
      continue;
    }
    if (source_interface == NULL || source_interface->get_scope() == NULL ||
        source_declaration->get_scope() != source_interface->get_scope() ||
        source_body->get_functionDeclaration() != source_declaration ||
        source_body->get_use_function_name()) {
      continue;
    }

    SgFunctionRefExp *replacement = SageBuilder::buildFunctionRefExp(
        source_semantic->get_name(), type, func_body);
    SgFunctionSymbol *destination_semantic =
        replacement != NULL ? replacement->get_symbol() : NULL;
    SgFunctionSymbol *destination_source =
        replacement != NULL ? replacement->get_fortran_source_visible_symbol()
                            : NULL;
    auto destination_kind =
        replacement != NULL
            ? replacement->get_fortran_source_visible_binding_kind()
            : SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable;

    const auto source_binding_kind =
        reference->get_fortran_source_visible_binding_kind();
    const bool source_rename_binding =
        source_binding_kind ==
            SgFunctionRefExp::e_fortran_source_visible_binding_use_rename ||
        source_binding_kind ==
            SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload;
    SgRenameSymbol *source_rename =
        isSgRenameSymbol(reference->get_fortran_source_visible_symbol());
    if (source_rename_binding) {
      if (source_rename == NULL ||
          source_rename->get_original_symbol() != source_semantic) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-interface-dependency]: "
                "renamed source binding=%p kind=%d does not name semantic "
                "symbol=%p\n",
                static_cast<void *>(source_rename),
                static_cast<int>(source_binding_kind),
                static_cast<void *>(source_semantic));
        ROSE_ABORT();
      }
      auto inserted = generic_bindings.emplace(source_rename, nullptr);
      if (inserted.second) {
        inserted.first->second =
            new SgRenameSymbol(destination_semantic->get_declaration(),
                               destination_semantic, source_rename->get_name());
        ASSERT_not_null(inserted.first->second);
        func_body->insert_symbol(inserted.first->second->get_name(),
                                 inserted.first->second);
      }
      destination_source = inserted.first->second;
      destination_kind = source_binding_kind;
      replacement->set_fortran_source_visible_symbol(destination_source);
      replacement->set_fortran_source_visible_binding_kind(destination_kind);
    }

    SgScopeStatement *destination_scope =
        destination_semantic != NULL ? destination_semantic->get_scope() : NULL;
    SgSymbolTable *destination_table =
        destination_scope != NULL ? destination_scope->get_symbol_table()
                                  : NULL;
    if (replacement == NULL || destination_semantic == NULL ||
        destination_semantic == source_semantic ||
        destination_semantic->get_declaration() == NULL ||
        destination_semantic->get_type() != type || destination_table == NULL ||
        destination_semantic->get_parent() != destination_table ||
        !destination_table->exists(destination_semantic) ||
        destination_source == NULL ||
        destination_kind ==
            SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[fortran-interface-dependency]: "
              "reference=%p source=%p replacement=%p destination=%p "
              "source-visible=%p kind=%d has no exact semantic publication\n",
              static_cast<void *>(reference),
              static_cast<void *>(source_semantic),
              static_cast<void *>(replacement),
              static_cast<void *>(destination_semantic),
              static_cast<void *>(destination_source),
              static_cast<int>(destination_kind));
      ROSE_ABORT();
    }
    SageInterface::replaceExpression(reference, replacement);
  }
}

static SgTemplateParameterPtrList *
copyTemplateParameterList(const SgTemplateParameterPtrList &params) {
  SgTemplateParameterPtrList *copy = new SgTemplateParameterPtrList();
  for (SgTemplateParameterPtrList::const_iterator it = params.begin();
       it != params.end(); ++it) {
    SgTemplateParameter *param = *it;
    if (param == NULL)
      continue;
    SgTemplateParameter *param_copy =
        SageInterface::cloneDetachedGeneratedTemplateParameter(
            param, "outlined-function-template-parameter");
    copy->push_back(param_copy);
  }
  return copy;
}

static void setTemplateParameterParents(SgTemplateDeclaration *decl) {
  if (decl == NULL)
    return;

  SgTemplateParameterPtrList &params = decl->get_templateParameters();
  for (SgTemplateParameterPtrList::iterator it = params.begin();
       it != params.end(); ++it) {
    if (*it != NULL)
      (*it)->set_parent(decl);
  }
}

static std::string getTemplateParameterName(const SgTemplateParameter *param) {
  if (param == NULL)
    return "";

  if (SgInitializedName *init_name = param->get_initializedName())
    return init_name->get_name().getString();

  if (SgNonrealType *nonreal_type = isSgNonrealType(param->get_type()))
    return nonreal_type->get_name().getString();

  if (SgTemplateType *template_type = isSgTemplateType(param->get_type()))
    return template_type->get_name().getString();

  if (SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(param->get_templateDeclaration()))
    return nonreal_decl->get_name().getString();

  if (SgTemplateDeclaration *template_decl =
          isSgTemplateDeclaration(param->get_templateDeclaration()))
    return template_decl->get_name().getString();

  return "";
}

static bool hasTemplateParameterName(const SgTemplateParameterPtrList &params,
                                     const SgTemplateParameter *candidate) {
  std::string candidate_name = getTemplateParameterName(candidate);
  if (candidate_name.empty())
    return false;

  for (SgTemplateParameter *param : params) {
    if (getTemplateParameterName(param) == candidate_name)
      return true;
  }

  return false;
}

static const SgTemplateParameterPtrList *
getDirectTemplateParametersFromDecl(const SgFunctionDeclaration *decl) {
  if (decl == NULL)
    return NULL;

  if (const SgTemplateFunctionDeclaration *tmpl_decl =
          isSgTemplateFunctionDeclaration(decl)) {
    return &(tmpl_decl->get_templateParameters());
  }
  if (const SgTemplateMemberFunctionDeclaration *tmpl_decl =
          isSgTemplateMemberFunctionDeclaration(decl)) {
    return &(tmpl_decl->get_templateParameters());
  }

  if (const SgTemplateInstantiationFunctionDecl *tmpl_inst =
          isSgTemplateInstantiationFunctionDecl(decl)) {
    if (const SgTemplateFunctionDeclaration *tmpl_decl =
            tmpl_inst->get_templateDeclaration()) {
      return &(tmpl_decl->get_templateParameters());
    }
  }
  if (const SgTemplateInstantiationMemberFunctionDecl *tmpl_inst =
          isSgTemplateInstantiationMemberFunctionDecl(decl)) {
    if (const SgTemplateMemberFunctionDeclaration *tmpl_decl =
            tmpl_inst->get_templateDeclaration()) {
      return &(tmpl_decl->get_templateParameters());
    }
  }

  if (const SgFunctionDeclaration *first =
          isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration())) {
    if (const SgTemplateFunctionDeclaration *tmpl_decl =
            isSgTemplateFunctionDeclaration(first)) {
      return &(tmpl_decl->get_templateParameters());
    }
    if (const SgTemplateMemberFunctionDeclaration *tmpl_decl =
            isSgTemplateMemberFunctionDeclaration(first)) {
      return &(tmpl_decl->get_templateParameters());
    }
  }
  if (const SgFunctionDeclaration *def =
          isSgFunctionDeclaration(decl->get_definingDeclaration())) {
    if (const SgTemplateFunctionDeclaration *tmpl_decl =
            isSgTemplateFunctionDeclaration(def)) {
      return &(tmpl_decl->get_templateParameters());
    }
    if (const SgTemplateMemberFunctionDeclaration *tmpl_decl =
            isSgTemplateMemberFunctionDeclaration(def)) {
      return &(tmpl_decl->get_templateParameters());
    }
  }

  return NULL;
}

static const SgTemplateParameterPtrList *
getTemplateParametersFromClassDecl(const SgClassDeclaration *class_decl) {
  if (class_decl == NULL)
    return NULL;

  if (const SgTemplateClassDeclaration *tmpl_decl =
          isSgTemplateClassDeclaration(class_decl)) {
    return &(tmpl_decl->get_templateParameters());
  }

  if (const SgTemplateInstantiationDecl *tmpl_inst =
          isSgTemplateInstantiationDecl(class_decl)) {
    if (const SgTemplateClassDeclaration *tmpl_decl =
            tmpl_inst->get_templateDeclaration()) {
      return &(tmpl_decl->get_templateParameters());
    }
  }

  if (const SgClassDeclaration *first =
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration())) {
    if (const SgTemplateClassDeclaration *tmpl_decl =
            isSgTemplateClassDeclaration(first)) {
      return &(tmpl_decl->get_templateParameters());
    }
  }

  if (const SgClassDeclaration *def =
          isSgClassDeclaration(class_decl->get_definingDeclaration())) {
    if (const SgTemplateClassDeclaration *tmpl_decl =
            isSgTemplateClassDeclaration(def)) {
      return &(tmpl_decl->get_templateParameters());
    }
  }

  return NULL;
}

static const SgTemplateParameterPtrList *
getTemplateParametersFromMemberClass(const SgFunctionDeclaration *decl) {
  if (decl == NULL)
    return NULL;

  if (const SgTemplateMemberFunctionDeclaration *template_member_decl =
          isSgTemplateMemberFunctionDeclaration(decl)) {
    if (const SgClassDeclaration *class_decl = isSgClassDeclaration(
            template_member_decl->get_associatedClassDeclaration())) {
      return getTemplateParametersFromClassDecl(class_decl);
    }
  }

  const SgMemberFunctionDeclaration *member_decl =
      isSgMemberFunctionDeclaration(decl);
  if (member_decl == NULL)
    return NULL;

  if (const SgClassDeclaration *class_decl =
          isSgClassDeclaration(member_decl->get_associatedClassDeclaration())) {
    return getTemplateParametersFromClassDecl(class_decl);
  }

  if (const SgClassDefinition *class_def =
          isSgClassDefinition(member_decl->get_class_scope())) {
    return getTemplateParametersFromClassDecl(class_def->get_declaration());
  }

  return NULL;
}

static void appendTemplateParameters(const SgTemplateParameterPtrList *source,
                                     SgTemplateParameterPtrList &destination) {
  if (source == NULL)
    return;

  for (SgTemplateParameter *param : *source) {
    if (param != NULL && !hasTemplateParameterName(destination, param))
      destination.push_back(param);
  }
}

static void collectTemplateParametersForOutlinedFunction(
    const SgFunctionDeclaration *decl,
    SgTemplateParameterPtrList &template_params) {
  if (decl == NULL)
    return;

  appendTemplateParameters(getTemplateParametersFromMemberClass(decl),
                           template_params);
  appendTemplateParameters(getDirectTemplateParametersFromDecl(decl),
                           template_params);
}

static Outliner::OutlinedLocalTypeTemplateEntry *
findLocalTypeTemplateEntry(Outliner::OutlinedLocalTypeTemplatePlan &plan,
                           SgType *source_type) {
  for (Outliner::OutlinedLocalTypeTemplateEntry &entry : plan.entries) {
    if (entry.source_type == source_type)
      return &entry;
  }
  return NULL;
}

static const Outliner::OutlinedLocalTypeTemplateEntry *
findLocalTypeTemplateEntry(const Outliner::OutlinedLocalTypeTemplatePlan &plan,
                           SgType *source_type) {
  for (const Outliner::OutlinedLocalTypeTemplateEntry &entry : plan.entries) {
    if (entry.source_type == source_type)
      return &entry;
  }
  return NULL;
}

static void collectLocalTypeTemplateParameters(
    const ASTtools::VarSymSet_t &symbols,
    SgFunctionDeclaration *enclosing_function,
    SgTemplateParameterPtrList &template_params,
    Outliner::OutlinedLocalTypeTemplatePlan &plan) {
  if (!plan.entries.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[local-type-plan]: caller supplied a "
            "non-empty output plan\n");
    ROSE_ABORT();
  }
  if (!SageInterface::is_Cxx_language())
    return;

  for (const SgVariableSymbol *symbol : symbols) {
    SgInitializedName *declaration =
        symbol != NULL ? symbol->get_declaration() : NULL;
    SgType *source_type = declaration != NULL ? declaration->get_type() : NULL;
    SgFunctionDeclaration *owner =
        ASTtools::functionOwningHiddenNamedType(source_type);
    if (owner == NULL)
      continue;
    if (declaration == NULL || source_type == NULL ||
        !ASTtools::sameFunctionFamily(owner, enclosing_function)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-owner]: captured "
              "symbol=%p declaration=%p type=%p owner=%p does not belong to "
              "the outlined source function=%p\n",
              static_cast<const void *>(symbol),
              static_cast<void *>(declaration),
              static_cast<void *>(source_type), static_cast<void *>(owner),
              static_cast<void *>(enclosing_function));
      ROSE_ABORT();
    }
    if (findLocalTypeTemplateEntry(plan, source_type) != NULL)
      continue;

    const std::size_t generated_index = plan.entries.size();
    std::string parameter_name;
    for (std::size_t suffix = 0;; ++suffix) {
      parameter_name =
          "__rex_outlined_local_type_" + std::to_string(generated_index);
      if (suffix != 0)
        parameter_name += "_" + std::to_string(suffix);
      bool occupied = false;
      for (SgTemplateParameter *parameter : template_params) {
        if (getTemplateParameterName(parameter) == parameter_name) {
          occupied = true;
          break;
        }
      }
      if (!occupied)
        break;
    }

    SgTemplateType *parameter_type =
        SageBuilder::buildTemplateType(SgName(parameter_name));
    SgTemplateParameter *parameter = SageBuilder::buildTemplateParameter(
        SgTemplateParameter::type_parameter, parameter_type,
        SgName(parameter_name), NULL, SgTemplateParameter::keyword_typename);
    parameter_type->set_template_parameter(parameter);
    if (parameter == NULL || parameter->get_type() != parameter_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-plan]: local source type=%p "
              "did not produce one exact generated type parameter\n",
              static_cast<void *>(source_type));
      ROSE_ABORT();
    }

    Outliner::OutlinedLocalTypeTemplateEntry entry;
    entry.source_type = source_type;
    entry.owning_function = owner;
    entry.template_parameter_index = template_params.size();
    template_params.push_back(parameter);
    plan.entries.push_back(entry);
  }
}

static void bindLocalTypeTemplatePlanToDefinition(
    SgFunctionDeclaration *function,
    Outliner::OutlinedLocalTypeTemplatePlan &plan) {
  if (plan.entries.empty())
    return;

  SgTemplateFunctionDeclaration *template_function =
      isSgTemplateFunctionDeclaration(function);
  const SgTemplateParameterPtrList *parameters =
      template_function != NULL ? &template_function->get_templateParameters()
                                : NULL;
  if (parameters == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[local-type-plan]: generated local-type "
            "plan has no defining function template\n");
    ROSE_ABORT();
  }

  for (Outliner::OutlinedLocalTypeTemplateEntry &entry : plan.entries) {
    if (entry.source_type == NULL || entry.owning_function == NULL ||
        entry.template_parameter_index >= parameters->size()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-plan]: entry source=%p "
              "owner=%p index=%zu is incomplete for %zu template "
              "parameters\n",
              static_cast<void *>(entry.source_type),
              static_cast<void *>(entry.owning_function),
              entry.template_parameter_index, parameters->size());
      ROSE_ABORT();
    }
    SgTemplateParameter *parameter =
        (*parameters)[entry.template_parameter_index];
    SgTemplateType *parameter_type =
        parameter != NULL ? isSgTemplateType(parameter->get_type()) : NULL;
    if (parameter_type == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-plan]: generated parameter "
              "at index=%zu is not one exact type parameter\n",
              entry.template_parameter_index);
      ROSE_ABORT();
    }
    entry.defining_parameter_type = parameter_type;
  }
}

static SgFunctionDeclaration *
createTemplateFuncSkeleton(const string &name, SgType *ret_type,
                           SgFunctionParameterList *params,
                           SgScopeStatement *scope,
                           const SgTemplateParameterPtrList &template_params) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(isSgGlobal(scope) != NULL);

  SgFunctionParameterList *nondef_params = cloneCanonicalParameterList(params);
  SgTemplateParameterPtrList *template_params_copy =
      copyTemplateParameterList(template_params);
  SgTemplateParameterPtrList *defining_template_params_copy =
      copyTemplateParameterList(template_params);

  SgTemplateFunctionDeclaration *nondef =
      SageBuilder::buildNondefiningTemplateFunctionDeclaration(
          SageBuilder::function_declaration_ownership::semanticAuxiliary(),
          name, ret_type, nondef_params, scope, template_params_copy);
  delete template_params_copy;
  ROSE_ASSERT(nondef != NULL);
  setTemplateParameterParents(nondef);

  SgTemplateFunctionDeclaration *def =
      SageBuilder::buildDefiningTemplateFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexicalIn(scope),
          name, ret_type, params, scope, nondef, defining_template_params_copy);
  delete defining_template_params_copy;
  ROSE_ASSERT(def != NULL);
  setTemplateParameterParents(def);
  validateOutlinedFunctionSignature(def, params);

  return def;
}

static void assertFunctionSymbolPresent(SgScopeStatement *scope,
                                        const SgFunctionDeclaration *func) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(func != NULL);

  if (scope->lookup_function_symbol(func->get_name()) != NULL)
    return;

  const SgTemplateFunctionDeclaration *template_func =
      isSgTemplateFunctionDeclaration(func);
  if (template_func != NULL) {
    SgTemplateParameterPtrList *template_params =
        const_cast<SgTemplateParameterPtrList *>(
            &(template_func->get_templateParameters()));
    SgTemplateFunctionSymbol *template_sym =
        scope->lookup_template_function_symbol(template_func->get_name(),
                                               template_func->get_type(),
                                               template_params);
    ROSE_ASSERT(template_sym != NULL);
    return;
  }

  ROSE_ASSERT(!"Missing function symbol for outlined function");
}

static SgTypedefDeclaration *
canonicalTypedefDeclaration(SgDeclarationStatement *decl) {
  SgTypedefDeclaration *typedef_decl = isSgTypedefDeclaration(decl);
  if (typedef_decl == NULL)
    return NULL;

  if (SgTypedefDeclaration *defining_decl =
          isSgTypedefDeclaration(typedef_decl->get_definingDeclaration())) {
    return defining_decl;
  }

  return typedef_decl;
}

static SgClassDefinition *classDefinitionFromDeclaration(SgNode *node) {
  SgClassDeclaration *class_decl = isSgClassDeclaration(node);
  if (class_decl == NULL)
    return NULL;

  if (SgClassDefinition *definition = class_decl->get_definition())
    return definition;

  if (SgClassDeclaration *defining_decl =
          isSgClassDeclaration(class_decl->get_definingDeclaration())) {
    return defining_decl->get_definition();
  }

  return NULL;
}

static SgTypedefDeclaration *
lookupTypedefDeclarationInScope(const SgName &name, SgScopeStatement *scope) {
  if (scope == NULL)
    return NULL;

  if (SgTypedefSymbol *symbol = scope->lookup_typedef_symbol(name)) {
    if (SgTypedefDeclaration *typedef_decl =
            canonicalTypedefDeclaration(symbol->get_declaration())) {
      return typedef_decl;
    }
  }

  if (SgDeclarationScope *declaration_scope = isSgDeclarationScope(scope)) {
    if (SgClassDefinition *class_definition =
            classDefinitionFromDeclaration(declaration_scope->get_parent())) {
      if (SgTypedefSymbol *symbol =
              class_definition->lookup_typedef_symbol(name)) {
        return canonicalTypedefDeclaration(symbol->get_declaration());
      }
    }
  }

  return NULL;
}

static SgTypedefDeclaration *
typedefDeclarationFromNonrealType(SgNonrealType *nonreal_type) {
  if (nonreal_type == NULL)
    return NULL;

  SgNonrealDecl *nonreal_decl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  if (nonreal_decl == NULL)
    return NULL;

  if (SgTypedefDeclaration *typedef_decl = canonicalTypedefDeclaration(
          nonreal_decl->get_templateDeclaration())) {
    return typedef_decl;
  }

  return lookupTypedefDeclarationInScope(nonreal_decl->get_name(),
                                         nonreal_decl->get_scope());
}

static void
collectTypedefsFromType(SgType *type,
                        std::vector<SgTypedefDeclaration *> &typedefs,
                        std::set<SgTypedefDeclaration *> &seen) {
  if (type == NULL)
    return;

  if (SgModifierType *modifier_type = isSgModifierType(type)) {
    collectTypedefsFromType(modifier_type->get_base_type(), typedefs, seen);
    return;
  }
  if (SgPointerType *pointer_type = isSgPointerType(type)) {
    collectTypedefsFromType(pointer_type->get_base_type(), typedefs, seen);
    return;
  }
  if (SgReferenceType *reference_type = isSgReferenceType(type)) {
    collectTypedefsFromType(reference_type->get_base_type(), typedefs, seen);
    return;
  }
  if (SgRvalueReferenceType *reference_type = isSgRvalueReferenceType(type)) {
    collectTypedefsFromType(reference_type->get_base_type(), typedefs, seen);
    return;
  }
  if (SgArrayType *array_type = isSgArrayType(type)) {
    collectTypedefsFromType(array_type->get_base_type(), typedefs, seen);
    return;
  }

  if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
    SgTypedefDeclaration *typedef_decl =
        canonicalTypedefDeclaration(typedef_type->get_declaration());
    if (typedef_decl == NULL)
      return;

    collectTypedefsFromType(typedef_type->get_base_type(), typedefs, seen);

    if (seen.insert(typedef_decl).second)
      typedefs.push_back(typedef_decl);
  } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    SgTypedefDeclaration *typedef_decl =
        typedefDeclarationFromNonrealType(nonreal_type);
    if (typedef_decl == NULL)
      return;

    collectTypedefsFromType(typedef_decl->get_base_type(), typedefs, seen);
    if (seen.insert(typedef_decl).second)
      typedefs.push_back(typedef_decl);
  }
}

static void
collectExplicitTypeReferences(SgNode *root,
                              std::vector<SgTypedefDeclaration *> &typedefs,
                              std::set<SgTypedefDeclaration *> &seen) {
  ROSE_ASSERT(root != NULL);
  RoseAst ast(root);
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    SgNode *node = *i;
    if (SgInitializedName *initialized_name = isSgInitializedName(node))
      collectTypedefsFromType(initialized_name->get_type(), typedefs, seen);

    // The moved outlined body can contain only a reference to a variable that
    // is reconstructed later as an outlined-function local.  Its typedef is
    // therefore observable through the reference symbol even though no
    // SgInitializedName is structurally present in the moved block yet.
    if (SgVarRefExp *var_ref = isSgVarRefExp(node)) {
      SgVariableSymbol *symbol = var_ref->get_symbol();
      SgInitializedName *declaration =
          symbol != NULL ? symbol->get_declaration() : NULL;
      if (declaration == NULL || declaration->get_type() == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[local-typedef-source]: var-ref=%p "
                "has no typed declaration\n",
                static_cast<void *>(var_ref));
        ROSE_ABORT();
      }
      collectTypedefsFromType(declaration->get_type(), typedefs, seen);
    }

    if (SgConstructorInitializer *ctor_init =
            isSgConstructorInitializer(node)) {
      collectTypedefsFromType(ctor_init->get_expression_type(), typedefs, seen);
    }

    if (SgExpression *expression = isSgExpression(node)) {
      if (expression->hasExplicitType())
        collectTypedefsFromType(expression->get_type(), typedefs, seen);
    }
  }
}

static bool isScopeVisibleFromScope(SgScopeStatement *decl_scope,
                                    SgScopeStatement *scope) {
  if (decl_scope == NULL || scope == NULL)
    return false;

  for (SgScopeStatement *current = scope; current != NULL;) {
    if (current == decl_scope)
      return true;

    if (current->get_parent() == NULL || isSgGlobal(current) != NULL)
      break;
    current = current->get_scope();
  }

  return false;
}

static bool isTypedefVisibleFromScope(SgTypedefDeclaration *typedef_decl,
                                      SgScopeStatement *scope) {
  if (typedef_decl == NULL || scope == NULL)
    return true;

  return isScopeVisibleFromScope(typedef_decl->get_scope(), scope);
}

static SgArrayType *
getNonFortranGlobalArrayType(const SgInitializedName *name) {
  if (name == NULL || SageInterface::is_Fortran_language() ||
      isSgGlobal(name->get_scope()) == NULL) {
    return NULL;
  }

  return isSgArrayType(name->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE));
}

static SgType *buildCArrayDecayPointerType(SgArrayType *array_type) {
  ROSE_ASSERT(array_type != NULL);
  return SageBuilder::buildPointerType(array_type->get_base_type());
}

static bool isExactCArrayParameterDecay(SgType *source_type,
                                        SgType *parameter_type) {
  if (SageInterface::is_Fortran_language())
    return false;

  SgArrayType *source_array = isSgArrayType(source_type);
  SgPointerType *parameter_pointer = isSgPointerType(parameter_type);
  return source_array != NULL && parameter_pointer != NULL &&
         source_array->get_base_type() == parameter_pointer->get_base_type();
}

static std::vector<SgTypedefDeclaration *>
addMissingLocalTypedefAliases(SgFunctionDeclaration *func) {
  ROSE_ASSERT(func != NULL);
  ROSE_ASSERT(func->get_type() != NULL);
  ROSE_ASSERT(func->get_definition() != NULL);
  SgBasicBlock *func_body = func->get_definition()->get_body();
  ROSE_ASSERT(func_body != NULL);

  std::vector<SgTypedefDeclaration *> typedefs;
  std::set<SgTypedefDeclaration *> seen;
  collectTypedefsFromType(func->get_type()->get_return_type(), typedefs, seen);
  SgFunctionParameterList *parameters = func->get_parameterList();
  ROSE_ASSERT(parameters != NULL);
  for (SgInitializedName *parameter : parameters->get_args()) {
    if (parameter == NULL || parameter->get_type() == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-typedef-source]: function=%s "
              "has an untyped parameter\n",
              func->get_name().str());
      ROSE_ABORT();
    }
    collectTypedefsFromType(parameter->get_type(), typedefs, seen);
  }
  // Run this only after variable handling has built every captured local and
  // wrapper access.  Earlier scans see only the moved expression body and miss
  // typedefs that become source-visible during parameter reconstruction.
  collectExplicitTypeReferences(func_body, typedefs, seen);

  std::vector<SgTypedefDeclaration *> aliases;
  std::set<std::string> alias_names;
  for (SgTypedefDeclaration *typedef_decl : typedefs) {
    if (typedef_decl == NULL || typedef_decl->get_base_type() == NULL)
      continue;
    if (isTypedefVisibleFromScope(typedef_decl, func_body))
      continue;

    std::string name = typedef_decl->get_name().getString();
    if (name.empty() || !alias_names.insert(name).second)
      continue;

    // This alias is generated source, not a semantic no-file-info carrier.
    // Give it source-visible transformation ownership at construction; the
    // complete outlined function receives its exact output-file identity at
    // the publication boundary below.
    SgTypedefDeclaration *alias = SageBuilder::buildTypedefDeclaration(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        typedef_decl->get_typedef_type(), name, typedef_decl->get_base_type(),
        func_body);
    if (alias == NULL || alias->get_file_info() == NULL ||
        !alias->get_file_info()->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-typedef-owner]: alias=%p name=%s "
              "was not constructed as generated source output\n",
              static_cast<void *>(alias), name.c_str());
      ROSE_ABORT();
    }
    aliases.push_back(alias);
  }

  for (std::vector<SgTypedefDeclaration *>::reverse_iterator i =
           aliases.rbegin();
       i != aliases.rend(); ++i) {
    SageInterface::removeStatement(*i, false);
    ROSE_ASSERT((*i)->get_parent() == NULL);
    SageInterface::prependStatement(*i, func_body);
    if ((*i)->get_parent() != func_body ||
        std::find(func_body->get_statements().begin(),
                  func_body->get_statements().end(),
                  *i) == func_body->get_statements().end()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-typedef-owner]: alias=%p name=%s "
              "lost structural ownership during insertion\n",
              static_cast<void *>(*i), (*i)->get_name().str());
      ROSE_ABORT();
    }
  }

  return aliases;
}

//! Returns 'true' if the base type is a primitive type.
static bool isBaseTypePrimitive(const SgType *type) {
  if (!type)
    return false;
  const SgType *base_type = type->findBaseType();
  if (base_type)
    switch (base_type->variantT()) {
    case V_SgTypeBool:
    case V_SgTypeChar:
    case V_SgTypeDouble:
    case V_SgTypeFloat:
    case V_SgTypeInt:
    case V_SgTypeLong:
    case V_SgTypeLongDouble:
    case V_SgTypeLongLong:
    case V_SgTypeShort:
    case V_SgTypeSignedChar:
    case V_SgTypeSignedInt:
    case V_SgTypeSignedLong:
    case V_SgTypeSignedShort:
    case V_SgTypeUnsignedChar:
    case V_SgTypeUnsignedInt:
    case V_SgTypeUnsignedLong:
    case V_SgTypeUnsignedShort:
    case V_SgTypeVoid:
    case V_SgTypeWchar:
      return true;
    default:
      break;
    }
  return false;
}

//! Stores the semantic parameter identity and its optional exact source type.
struct OutlinedFuncParam_t {
  string name;
  SgType *semantic_type = NULL;
  SgType *source_type = NULL;
};

/*!
 *  \brief Creates a new outlined-function parameter for a given
 *  variable. The requirement is to preserve data read/write semantics.
 *
 *  This function is only used when wrapper parameter is not used
 *  so individual parameter needs to be created for each variable passed to the
 * outlined function.
 *
 *  For C/C++: we use pointer dereferencing to implement pass-by-reference
 *    So the parameter needs to be &a, which is a pointer type of a's base type
 *
 *    In a recent implementation, side effect analysis is used to find out
 *    variables which are not modified so pointer types are not used.
 *
 *  For Fortran, all parameters are passed by reference by default.
 *
 *  Given a variable (i.e., its type and name) whose references are to
 *  be outlined, create a suitable outlined-function parameter.
 *  For C/C++, the  parameter is created as a pointer, to support parameter
 * passing of aggregate types in C programs. Moreover, the type is made 'void'
 * if the base type is not a primitive type.
 *
 *  An original type may need adjustments before we can make a pointer type from
 * it. For example: a)Array types from a function parameter: its first dimension
 * is auto converted to a pointer type
 *
 *    b) Pointer to a C++ reference type is illegal, we create a pointer to its
 *    base type in this case. It also match the semantics for addressof(refType)
 *
 *
 *  The implementation follows two steps:
 *     step 1: adjust a variable's base type
 *     step 2: decide on its function parameter type
 *  Liao, 8/14/2009
 */
static bool isExactAdjustedParameterTypePair(SgType *semanticType,
                                             SgType *sourceType) {
  if (semanticType == NULL || sourceType == NULL)
    return false;
  if (SageInterface::cxxSourceTypeMatchesSemanticType(sourceType, semanticType))
    return true;

  SgType *sourceBase = sourceType->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_MODIFIER_TYPE);
  SgType *adjustedSource = NULL;
  if (SgArrayType *arrayType = isSgArrayType(sourceBase)) {
    adjustedSource = SageBuilder::buildPointerType(arrayType->get_base_type());
  } else if (isSgFunctionType(sourceBase) != NULL) {
    adjustedSource = SageBuilder::buildPointerType(sourceBase);
  }
  return adjustedSource != NULL &&
         SageInterface::cxxSourceTypeMatchesSemanticType(adjustedSource,
                                                         semanticType);
}

static SgType *
exactCapturedVariableSourceType(const SgInitializedName *initializedName) {
  ROSE_ASSERT(initializedName != NULL);
  SgType *semanticType = initializedName->get_type();
  ROSE_ASSERT(semanticType != NULL);
  SgType *sourceType = initializedName->get_cxx_source_type();
  if (sourceType != NULL && (sourceType == semanticType ||
                             !SageInterface::cxxSourceTypeMatchesSemanticType(
                                 sourceType, semanticType))) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[parameter-source-type]: parameter=%s "
            "has a non-distinct or inequivalent direct source type\n",
            initializedName->get_name().getString().c_str());
    ROSE_ABORT();
  }

  // The frontend publishes a declaration's exact written parameter type in a
  // separately owned syntax list when Clang adjusted the semantic parameter
  // type.  This is required in both C and C++: typedef identity can carry ABI
  // semantics (for example va_list), not merely preferred spelling.
  SgFunctionParameterList *semanticParameters =
      isSgFunctionParameterList(initializedName->get_parent());
  SgFunctionDeclaration *function =
      semanticParameters != NULL
          ? isSgFunctionDeclaration(semanticParameters->get_parent())
          : NULL;
  SgFunctionParameterList *syntaxParameters =
      function != NULL ? function->get_parameterList_syntax() : NULL;
  if (function == NULL || function->get_parameterList() != semanticParameters ||
      syntaxParameters == NULL || syntaxParameters == semanticParameters) {
    return sourceType;
  }

  const SgInitializedNamePtrList &semanticArgs = semanticParameters->get_args();
  const SgInitializedNamePtrList &syntaxArgs = syntaxParameters->get_args();
  const auto semanticPosition =
      std::find(semanticArgs.begin(), semanticArgs.end(), initializedName);
  const size_t index = semanticPosition != semanticArgs.end()
                           ? static_cast<size_t>(std::distance(
                                 semanticArgs.begin(), semanticPosition))
                           : semanticArgs.size();
  SgInitializedName *syntaxName =
      index < syntaxArgs.size() ? syntaxArgs[index] : NULL;
  SgType *syntaxType = syntaxName != NULL ? syntaxName->get_type() : NULL;
  if (semanticPosition == semanticArgs.end() ||
      semanticArgs.size() != syntaxArgs.size() || syntaxName == NULL ||
      syntaxName->get_parent() != syntaxParameters ||
      syntaxName->get_name() != initializedName->get_name() ||
      syntaxType == NULL ||
      !isExactAdjustedParameterTypePair(semanticType, syntaxType)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[parameter-source-type]: "
            "function=%p name=%s semantic-list=%p syntax-list=%p "
            "index=%zu semantic-type=%p syntax-type=%p has no exact "
            "source parameter pairing\n",
            static_cast<void *>(function),
            function->get_name().getString().c_str(),
            static_cast<void *>(semanticParameters),
            static_cast<void *>(syntaxParameters), index,
            static_cast<void *>(semanticType), static_cast<void *>(syntaxType));
    ROSE_ABORT();
  }
  if (syntaxType == semanticType)
    return sourceType;
  if (sourceType != NULL && sourceType != syntaxType) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[parameter-source-type]: "
            "parameter=%s has conflicting direct and function source types\n",
            initializedName->get_name().getString().c_str());
    ROSE_ABORT();
  }
  return syntaxType;
}

static OutlinedFuncParam_t
createParam(const SgInitializedName
                *i_name, // the variable to be passed into the outlined function
            bool classic_original_type =
                false) // flag to decide if the variable's adjusted type is used
                       // directly, only applicable when
                       // -rose:outline:enable_classic  is turned on
{
  ROSE_ASSERT(i_name);
  SgType *init_type = i_name->get_type();
  ROSE_ASSERT(init_type);
  SgType *source_init_type = exactCapturedVariableSourceType(i_name);

  // A Fortran POINTER actual passed to the variadic KMPC fork wrapper is an
  // implicit-reference argument to its associated target storage.  The
  // outlined microtask therefore receives the pointee value type, not a
  // Fortran descriptor and not another POINTER declaration attribute.  A
  // pointer-association operation in the outlined region must be rejected by
  // its producer; silently treating target storage as a descriptor is invalid.
  if (SageInterface::is_Fortran_language()) {
    if (SgPointerType *pointer = isSgPointerType(init_type)) {
      init_type = pointer->get_base_type();
      if (init_type == nullptr) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-pointer-abi]: captured "
                "POINTER '%s' has no exact associated-target type\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }
  }

  // Store the adjusted original types into param_base_type
  // primitive types: --> original type
  // complex types: void
  // array types from function parameters:  pointer type for 1st dimension
  // C++ reference type: use base type since we want to have uniform way to
  // generate a pointer to the original type
  SgType *param_base_type = 0;
  SgType *source_param_base_type = NULL;
  if (SageInterface::is_Fortran_language()) {
    param_base_type = init_type;
  } else if (isBaseTypePrimitive(init_type) || Outliner::enable_classic)
  // for classic translation, there is no additional unpacking statement to
  // convert void* type to non-primitive type of the parameter
  // So we don't convert the type to void* here
  {
    // Duplicate the initial type.
    param_base_type = init_type; //!< \todo Is shallow copy here OK?
    // param_base_type = const_cast<SgType *> (init_type); //!< \todo Is shallow
    // copy here OK?

    // Adjust the original types for array or function types (TODO function
    // types) which are passed as function parameters convert the first
    // dimension of an array type function parameter to a pointer type, This is
    // called the auto type conversion for function or array typed variables
    // that are passed as function parameters
    // Liao 4/24/2009
    if (isSgArrayType(param_base_type))
      if (isSgFunctionDefinition(i_name->get_scope()))
        param_base_type = SageBuilder::buildPointerType(
            isSgArrayType(param_base_type)->get_base_type());

    // For C++ reference type, we use its base type since pointer to a reference
    // type is not allowed Liao, 8/14/2009
    param_base_type =
        ASTtools::buildAddressOfResultType(param_base_type)->get_base_type();

    if (source_init_type != NULL) {
      source_param_base_type = source_init_type;
      if (isSgFunctionDefinition(i_name->get_scope())) {
        SgType *source_adjustment = source_param_base_type->stripType(
            SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
        if (SgArrayType *arrayType = isSgArrayType(source_adjustment)) {
          source_param_base_type =
              SageBuilder::buildPointerType(arrayType->get_base_type());
        } else if (isSgFunctionType(source_adjustment) != NULL) {
          source_param_base_type =
              SageBuilder::buildPointerType(source_adjustment);
        }
      }
      source_param_base_type =
          ASTtools::buildAddressOfResultType(source_param_base_type)
              ->get_base_type();
    }

    ROSE_ASSERT(param_base_type);
  } else // for non-primitive types, we use void as its base type
  {
    param_base_type = SgTypeVoid::createType();
    ROSE_ASSERT(param_base_type);
    // Take advantage of the const modifier
    if (ASTtools::isConstObj(init_type)) {
      SgModifierType *mod = SageBuilder::buildConstType(param_base_type);
      param_base_type = mod;
    }
  }

  // Stores the real parameter type to be used in new_param_type
  string init_name = i_name->get_name().str();
  // The parameter name reflects the type: the same name means the same type,
  // p__ means a pointer type
  string new_param_name = init_name;
  SgType *new_param_type = NULL;
  SgType *new_source_param_type = NULL;

  // For classic behavior, read only variables are passed by values for C/C++
  // They share the same name and type
  if (Outliner::enable_classic) {
    // read only parameter: pass-by-value, the same type and name
    if (classic_original_type) {
      new_param_type = param_base_type;
      new_source_param_type = source_param_base_type;
    } else {
      new_param_name += "p__";
      new_param_type = SgPointerType::createType(param_base_type);
      if (source_param_base_type != NULL)
        new_source_param_type =
            SgPointerType::createType(source_param_base_type);
    }
  } else // The big assumption of this function is within the context of no
         // wrapper parameter is used very conservative one, assume the worst
         // side effects (all are written)
         // TODO, why not use  classic_original_type to control this!!??
  {
    ROSE_ASSERT(Outliner::useParameterWrapper == false &&
                Outliner::useStructureWrapper == false);
    if (!SageInterface::is_Fortran_language()) {
      new_param_type = SgPointerType::createType(param_base_type);
      ROSE_ASSERT(new_param_type);
      new_param_name += "p__";
      if (source_param_base_type != NULL)
        new_source_param_type =
            SgPointerType::createType(source_param_base_type);
    }
  }

  // C parameter adjustment and the outliner pass-by-reference layer can
  // collapse a distinct source array/function declarator onto the exact same
  // canonical pointer type as the semantic parameter.  At that point there is
  // only one generated declarator surface; publishing the same type through a
  // second "source" channel would falsely claim that distinct syntax remains.
  if (new_source_param_type == new_param_type)
    new_source_param_type = NULL;

  // Fortran parameters are passed by reference by default,
  // So use base type directly
  // C/C++ parameters will use their new param type to implement
  // pass-by-reference
  OutlinedFuncParam_t result;
  result.name = new_param_name;
  result.semantic_type =
      SageInterface::is_Fortran_language() ? param_base_type : new_param_type;
  result.source_type = new_source_param_type;
  if (result.semantic_type == NULL ||
      (result.source_type != NULL &&
       (result.source_type == result.semantic_type ||
        !SageInterface::cxxSourceTypeMatchesSemanticType(
            result.source_type, result.semantic_type)))) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[parameter-source-type]: parameter=%s "
            "semantic=%p source=%p is not one distinct equivalent typed "
            "declarator pair\n",
            result.name.c_str(), static_cast<void *>(result.semantic_type),
            static_cast<void *>(result.source_type));
    ROSE_ABORT();
  }
  return result;
}

static OutlinedFunctionParameterPlan buildOutlinedFunctionParameterPlan(
    const ASTtools::VarSymSet_t &syms,
    const ASTtools::VarSymSet_t &pointer_dereference_symbols) {
  OutlinedFunctionParameterPlan plan;
  plan.definition_parameters = buildFunctionParameterList();
  if (plan.definition_parameters == NULL ||
      plan.definition_parameters->get_parent() != NULL ||
      !plan.definition_parameters->get_args().empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[function-signature-plan]: parameter "
            "planning did not start with one detached empty list\n");
    ROSE_ABORT();
  }

  if (!Outliner::enable_classic && Outliner::useParameterWrapper) {
    SgName wrapper_name = "__out_argv";
    SgType *wrapper_type = NULL;
    if (SageInterface::is_Fortran_language()) {
      wrapper_name = "out_argv";
      wrapper_type = buildIntType();
    } else if (Outliner::useStructureWrapper) {
      wrapper_type = buildPointerType(buildVoidType());
    } else {
      wrapper_type = buildPointerType(buildPointerType(buildVoidType()));
    }
    plan.wrapper_parameter = buildInitializedName(wrapper_name, wrapper_type);
    appendArg(plan.definition_parameters, plan.wrapper_parameter);
    plan.syntax_parameters = plan.definition_parameters;
  } else {
    struct ParameterSpecification {
      const SgVariableSymbol *symbol;
      OutlinedFuncParam_t parameter;
    };
    std::vector<ParameterSpecification> specifications;
    specifications.reserve(syms.size());
    for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin();
         i != syms.rend(); ++i) {
      const SgVariableSymbol *symbol = isSgVariableSymbol(*i);
      const SgInitializedName *declaration =
          symbol != NULL ? symbol->get_declaration() : NULL;
      if (symbol == NULL || declaration == NULL ||
          declaration->get_type() == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-plan]: captured "
                "symbol has no exact typed declaration\n");
        ROSE_ABORT();
      }

      const bool use_original_type = pointer_dereference_symbols.find(symbol) ==
                                     pointer_dereference_symbols.end();
      const OutlinedFuncParam_t parameter =
          createParam(declaration, use_original_type);
      if (parameter.name.empty() || parameter.semantic_type == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-plan]: captured "
                "symbol produced an empty name or null type\n");
        ROSE_ABORT();
      }
      plan.has_distinct_source_parameters |= parameter.source_type != NULL;
      specifications.push_back({symbol, parameter});
    }

    for (const ParameterSpecification &specification : specifications) {
      SgInitializedName *definition_parameter = buildInitializedName(
          specification.parameter.name, specification.parameter.semantic_type);
      prependArg(plan.definition_parameters, definition_parameter);
      if (!plan.direct_parameters
               .emplace(specification.symbol, definition_parameter)
               .second) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-plan]: captured "
                "symbol occurs more than once\n");
        ROSE_ABORT();
      }
    }

    if (plan.has_distinct_source_parameters) {
      plan.syntax_parameters = buildFunctionParameterList();
      if (plan.syntax_parameters == NULL ||
          plan.syntax_parameters == plan.definition_parameters ||
          plan.syntax_parameters->get_parent() != NULL ||
          !plan.syntax_parameters->get_args().empty()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-plan]: source "
                "parameter planning did not start with one detached empty "
                "list\n");
        ROSE_ABORT();
      }
      for (const ParameterSpecification &specification : specifications) {
        SgInitializedName *syntax_parameter =
            buildInitializedName(specification.parameter.name,
                                 specification.parameter.source_type != NULL
                                     ? specification.parameter.source_type
                                     : specification.parameter.semantic_type);
        prependArg(plan.syntax_parameters, syntax_parameter);
        if (!plan.direct_syntax_parameters
                 .emplace(specification.symbol, syntax_parameter)
                 .second) {
          ROSE_ABORT();
        }
      }
    } else {
      plan.syntax_parameters = plan.definition_parameters;
      plan.direct_syntax_parameters = plan.direct_parameters;
    }
  }

  const size_t expected_arity =
      plan.wrapper_parameter != NULL ? 1 : plan.direct_parameters.size();
  if (plan.syntax_parameters == NULL ||
      plan.definition_parameters->get_args().size() != expected_arity ||
      plan.syntax_parameters->get_args().size() != expected_arity) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[function-signature-plan]: planned "
                    "parameter arity is inconsistent\n");
    ROSE_ABORT();
  }
  return plan;
}

/*!
 *  \brief Initializes unpacking statements for array types
 *  The function takes into account that array types must be initialized element
 * by element The function also skips typedef types to get the real type
 *
 *  \param lhs Left-hand side of the assignment
 *  \param rhs Right-hand side of the assignment
 *  \param type Current type being initialized
 *  \param scope Scope where the assignments will be placed
 *  \param loop_indexes Indexes of all loops, to be declared after calling this
 * function So they are initialized before the most outer loop
 *
 *  Example:
 *    Outlined parameters struct:
 *        struct OUT__1__7768___data {
 *            void *a_p;
 *            int (*b_p)[10UL];
 *            int c[10UL];
 *            void *d_p;
 *        };
 *    Unpacking statements:
 *        int *a = (int *)(((struct OUT__1__7768___data *)__out_argv) -> a_p);
 * -> shared scalar int (*b)[10UL] = (int (*)[10UL])(((struct
 * OUT__1__7768___data *)__out_argv) -> b_p);      -> shared static array int
 * __i0__; for (__i0__ = 0; __i0__ < 10UL; __i0__++) -> firstprivate array
 *            c[__i0__] = ((struct OUT__1__7768___data *)__out_argv) ->
 * c[__i0__]; int **d = (int **)(((struct OUT__1__7768___data *)__out_argv) ->
 * d_p);                    -> shared dynamic array
 */
static SgStatement *
build_array_unpacking_statement(SgExpression *lhs, SgExpression *rhs,
                                SgType *type, SgScopeStatement *scope,
                                SgStatementPtrList &loop_indexes) {
  ROSE_ASSERT(isSgArrayType(type));
  ROSE_ASSERT(scope);

  // Loop initializer
  std::string loop_index_name =
      SageInterface::generateUniqueVariableName(scope, "i");
  SgVariableDeclaration *loop_index = buildVariableDeclaration(
      loop_index_name, buildIntType(), NULL /* initializer */, scope);
  loop_indexes.push_back(loop_index);
  SgStatement *loop_init = buildAssignStatement(
      buildVarRefExp(loop_index_name, scope), buildIntVal(0));

  // Loop test
  SgStatement *loop_test = buildExprStatement(buildLessThanOp(
      buildVarRefExp(loop_index_name, scope), isSgArrayType(type)->get_index(),
      SageInterface::is_C_language() ? static_cast<SgType *>(buildIntType())
                                     : static_cast<SgType *>(buildBoolType())));

  // Loop increment
  SgExpression *loop_increment =
      buildPlusPlusOp(buildVarRefExp(loop_index_name, scope), buildIntType(),
                      SgUnaryOp::postfix);

  // Loop body
  SgExpression *assign_lhs =
      buildPntrArrRefExp(lhs, buildVarRefExp(loop_index_name, scope),
                         SageInterface::getElementType(lhs->get_type()));
  SgExpression *assign_rhs =
      buildPntrArrRefExp(rhs, buildVarRefExp(loop_index_name, scope),
                         SageInterface::getElementType(rhs->get_type()));
  SgStatement *loop_body = NULL;
  SgType *base_type = isSgArrayType(type)->get_base_type()->stripType(
      SgType::STRIP_TYPEDEF_TYPE);
  if (isSgArrayType(base_type)) {
    loop_body = build_array_unpacking_statement(assign_lhs, assign_rhs,
                                                base_type, scope, loop_indexes);
  } else {
    loop_body = buildAssignStatement(assign_lhs, assign_rhs);
  }

  // Loop satement
  return buildForStatement(loop_init, loop_test, loop_increment, loop_body);
}

/*!
 *  \brief Creates a local variable declaration to "unpack" an
 * outlined-function's parameter int index is optionally used as an offset
 * inside a wrapper parameter for multiple variables
 *
 *  The key is to set local_name, local_type, and local_val for all cases
 *
 *  There are three choices:
 *  Case 1: unpack one variable from one parameter
 *  -----------------------------------------------
 *  OUT_XXX(int *ip__)
 *  {
 *    // This is called unpacking declaration for a read-only variable, Liao,
 * 9/11/2008 int i = * (int *) ip__;
 *  }
 *
 *  Case 2: unpack one variable from an array of pointers
 *  -----------------------------------------------
 *  OUT_XXX (void * __out_argv[n]) // for written variables, we have to use
 * pointers
 *  {
 *    int * _p_i =  (int*)__out_argv[0];
 *    int * _p_j =  (int*)__out_argv[1];
 *    ....
 *  }
 *
 *  Case 3: unpack one variable from a structure
 *  -----------------------------------------------
 *  OUT__xxx (struct OUT__xxx__data *__out_argv)
 *  {
 *    int i = __out_argv->i;
 *    int j = __out_argv->j;
 *    int (*sum)[100UL] = __out_argv->sum_p;*
 *  }
 *
 * case 1 and case 2 have two variants:
 *   using conventional pointer dereferencing or
 *   using cloned variables(temp_variable)
 *
 *
 * Notes for handling reference type in OpenMP outlining
 * ------------------------------------------------
 * Reference type handling when a wrapper data structure is requested:
 *   It is not allowed to create a pointer type to a reference type.
 *   We create a pointer type to the base type of the reference type instead.
 *   Anything else is handled the same since &var means to get the address of
 * the original variable even var is a reference type.
 *
 *   struct OUT__1__8577___data
 *   {
 *     void *dthydro_p;
 *   }
 *
 *  some_function( Real_t& dthydro )
 * dthydro is a reference type, & dthydro is equal to & of its original object
 *  __out_argv1__8577__ . OUT__1__8577___data::dthydro_p = ((void *)(&dthydro));
 *
 *  Real_t *dthydro = (Real_t *)(((struct OUT__1__8577___data *)__out_argv) ->
 * OUT__1__8577___data::dthydro_p);
 *
 *  the original use is replaced as pointer dereferences
 *   Real_t dthydro_tmp =  *dthydro;
 */
static SgName allocateExactUnpackLocalName(const SgName &requestedName,
                                           SgScopeStatement *scope) {
  if (requestedName.getString().empty() || scope == NULL ||
      scope->get_symbol_table() == NULL ||
      scope->get_symbol_table()->get_table() == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[unpack-local-name]: requested=%s "
            "scope=%p has no exact nonempty name/table identity\n",
            requestedName.getString().c_str(), static_cast<void *>(scope));
    ROSE_ABORT();
  }

  const std::string baseName = requestedName.getString();
  for (size_t suffix = 0;; ++suffix) {
    const SgName candidate(suffix == 0 ? baseName
                                       : baseName + "__rex_unpack_" +
                                             std::to_string(suffix));
    SgSymbolTable *table = scope->get_symbol_table();
    if (!table->exists(candidate))
      return candidate;

    size_t exactOccupants = 0;
    const auto range = table->get_table()->equal_range(candidate);
    for (auto current = range.first; current != range.second; ++current) {
      SgSymbol *occupied = current->second;
      if (occupied == NULL || occupied->get_parent() != table ||
          !table->exists(occupied) || occupied->get_scope() != scope) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[unpack-local-name]: candidate=%s "
                "has malformed occupant=%p in scope=%p\n",
                candidate.getString().c_str(), static_cast<void *>(occupied),
                static_cast<void *>(scope));
        ROSE_ABORT();
      }
      ++exactOccupants;
    }
    if (exactOccupants == 0) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[unpack-local-name]: candidate=%s is "
              "reported occupied without an exact table entry in scope=%p\n",
              candidate.getString().c_str(), static_cast<void *>(scope));
      ROSE_ABORT();
    }
  }
}

static SgVariableDeclaration *createUnpackDecl(
    SgInitializedName *param, // the function parameter
    int index,                // the index to the array of pointers type
    bool isPointerDeref,      // must use pointer deference or not
    const SgInitializedName
        *i_name, // original variable to be passed as the function parameter
    SgClassDeclaration
        *struct_decl, // the struct declaration type used to wrap parameters
    SgScopeStatement
        *scope, // the scope into which the statement will be inserted
    SgType *local_type_template_parameter) {
  ROSE_ASSERT(param && scope && i_name);

  // keep the original name
  const string orig_var_name = i_name->get_name().str();

  //---------------step 1 -----------------------------------------------
  // decide on the type : local_type
  // the original data type of the variable passed via parameter
  SgType *orig_var_type = i_name->get_type();
  SgType *orig_var_source_type = SageInterface::is_Fortran_language()
                                     ? NULL
                                     : exactCapturedVariableSourceType(i_name);
  if (local_type_template_parameter != NULL) {
    if (!SageInterface::is_Cxx_language() ||
        isSgTemplateType(local_type_template_parameter) == NULL ||
        ASTtools::functionOwningHiddenNamedType(orig_var_type) == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-type-unpack]: captured "
              "declaration=%p type=%p cannot bind generated template type=%p\n",
              static_cast<const void *>(i_name),
              static_cast<void *>(orig_var_type),
              static_cast<void *>(local_type_template_parameter));
      ROSE_ABORT();
    }
    orig_var_type = local_type_template_parameter;
    orig_var_source_type = NULL;
  }
  if (SageInterface::is_Fortran_language()) {
    if (SgPointerType *pointer = isSgPointerType(orig_var_type)) {
      orig_var_type = pointer->get_base_type();
      if (orig_var_type == nullptr) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-pointer-abi]: captured "
                "POINTER '%s' has no exact associated-target type\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }
  }
  SgArrayType *global_array_type = getNonFortranGlobalArrayType(i_name);
  bool is_array_parameter = false;
  if (!SageInterface::is_Fortran_language()) {
    // Convert an array type parameter's first dimension to a pointer type
    // This conversion is implicit for C/C++ language.
    // We have to make it explicit to get the right type
    // Liao, 4/24/2009  TODO we should only adjust this for the case 1
    if (isSgFunctionDefinition(i_name->get_scope())) {
      SgArrayType *semantic_array = isSgArrayType(orig_var_type);
      SgType *source_base =
          orig_var_source_type != NULL
              ? orig_var_source_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                                SgType::STRIP_MODIFIER_TYPE)
              : NULL;
      SgArrayType *source_array = isSgArrayType(source_base);
      if (semantic_array != NULL || source_array != NULL) {
        if (semantic_array != NULL) {
          orig_var_type =
              SageBuilder::buildPointerType(semantic_array->get_base_type());
        }
        if (source_array != NULL) {
          SgType *adjusted_source =
              SageBuilder::buildPointerType(source_array->get_base_type());
          if (!SageInterface::cxxSourceTypeMatchesSemanticType(adjusted_source,
                                                               orig_var_type)) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                    "array parameter=%s has contradictory adjusted semantic "
                    "and source types\n",
                    i_name->get_name().getString().c_str());
            ROSE_ABORT();
          }
          orig_var_source_type =
              adjusted_source != orig_var_type ? adjusted_source : NULL;
        } else if (orig_var_source_type != NULL) {
          std::cerr << "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                       "array parameter="
                    << i_name->get_name()
                    << " has no exact source array type\n";
          ROSE_ABORT();
        }
        if (orig_var_type == NULL || isSgPointerType(orig_var_type) == NULL) {
          std::cerr << "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                       "array parameter="
                    << i_name->get_name()
                    << " has no exact adjusted pointer type\n";
          ROSE_ABORT();
        }
        is_array_parameter = true;
      }
    }
  }

  const bool use_cxx_reference_for_pointer_deref =
      SageInterface::is_Cxx_language() && Outliner::temp_variable &&
      !Outliner::useStructureWrapper && isPointerDeref &&
      global_array_type == NULL;

  SgType *local_type = NULL;
  SgType *local_source_type = NULL;
  if (SageInterface::is_Fortran_language())
    local_type = orig_var_type;
  else if (Outliner::temp_variable || Outliner::useStructureWrapper)
  // unique processing for C/C++ if temp variables are used
  {
    if (use_cxx_reference_for_pointer_deref) {
      local_type = isSgReferenceType(orig_var_type)
                       ? orig_var_type
                       : SgReferenceType::createType(orig_var_type);
      if (orig_var_source_type != NULL) {
        local_source_type =
            isSgReferenceType(orig_var_source_type)
                ? orig_var_source_type
                : SgReferenceType::createType(orig_var_source_type);
      }
    } else if (isPointerDeref || (!isPointerDeref && is_array_parameter)) {
      // Liao 3/11/2015. For a parameter of a reference type, we have to
      // specially tweak the unpacking statement It is not allowed to create a
      // pointer to a reference type. So we use a pointer to its raw type
      // (stripped reference type) instead. use pointer dereferencing for some
      if (!isPointerDeref && is_array_parameter) {
        // A C/C++ array parameter already has its first dimension adjusted to
        // a pointer in orig_var_type above.  The generated unpack local
        // represents that adjusted parameter value itself; adding another
        // pointer layer produces T (**)[N] and changes the program type.
        local_type = orig_var_type;
        local_source_type = orig_var_source_type;
      } else if (global_array_type != NULL) {
        local_type = buildCArrayDecayPointerType(global_array_type);
        if (orig_var_source_type != NULL) {
          SgArrayType *source_array =
              isSgArrayType(orig_var_source_type->stripType(
                  SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE));
          if (source_array == NULL) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                    "global array=%s has no exact source array type\n",
                    i_name->get_name().getString().c_str());
            ROSE_ABORT();
          }
          local_source_type = buildCArrayDecayPointerType(source_array);
        }
      } else if (SgReferenceType *rtype = isSgReferenceType(orig_var_type)) {
        local_type = buildPointerType(rtype->get_base_type());
        if (orig_var_source_type != NULL) {
          SgReferenceType *source_reference =
              isSgReferenceType(orig_var_source_type);
          if (source_reference == NULL) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                    "reference=%s has no exact source reference type\n",
                    i_name->get_name().getString().c_str());
            ROSE_ABORT();
          }
          local_source_type =
              buildPointerType(source_reference->get_base_type());
        }
      } else {
        local_type = buildPointerType(orig_var_type);
        if (orig_var_source_type != NULL)
          local_source_type = buildPointerType(orig_var_source_type);
      }
    } else { // use variable clone instead for others
      local_type = orig_var_type;
      local_source_type = orig_var_source_type;
    }
  } else // all other cases: non-fortran, not using variable clones
  {
    if (is_C_language()) {
      // we use pointer types for all variables to be passed
      // the classic outlining will not use unpacking statement, but use the
      // parameters directly. So we can safely always use pointer dereferences
      // here
      local_type = global_array_type != NULL
                       ? buildCArrayDecayPointerType(global_array_type)
                       : buildPointerType(orig_var_type);
      if (orig_var_source_type != NULL) {
        if (global_array_type != NULL) {
          SgArrayType *source_array =
              isSgArrayType(orig_var_source_type->stripType(
                  SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE));
          if (source_array == NULL) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
                    "global array=%s has no exact source array type\n",
                    i_name->get_name().getString().c_str());
            ROSE_ABORT();
          }
          local_source_type = buildCArrayDecayPointerType(source_array);
        } else {
          local_source_type = buildPointerType(orig_var_source_type);
        }
      }
    } else // C++ language
           // Rich's idea was to leverage C++'s reference type: two cases:
           //  a) for variables of reference type: no additional work
           //  b) for others: make a reference type to them
           //   all variable accesses in the outlined function will have
           //   access the address of the by default, not variable substitution
           //   is needed
    {
      local_type = isSgReferenceType(orig_var_type)
                       ? orig_var_type
                       : SgReferenceType::createType(orig_var_type);
      if (orig_var_source_type != NULL) {
        local_source_type =
            isSgReferenceType(orig_var_source_type)
                ? orig_var_source_type
                : SgReferenceType::createType(orig_var_source_type);
      }
    }
  }
  ROSE_ASSERT(local_type);
  if (local_source_type != NULL &&
      !SageInterface::cxxSourceTypeMatchesSemanticType(local_source_type,
                                                       local_type)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[unpack-source-type]: captured "
            "variable=%s has an inequivalent generated source type\n",
            i_name->get_name().getString().c_str());
    ROSE_ABORT();
  }
  // The source-type slot represents a distinct lexical spelling.  Type
  // interning may collapse the transformed semantic and source wrappers even
  // when the captured declaration originally needed both; in that case the
  // generated declaration has one exact type surface and must leave the
  // optional source slot empty.
  if (local_source_type == local_type) {
    local_source_type = NULL;
  }

  SgAssignInitializer *local_val = NULL;

  // Declare a local variable to store the dereferenced argument.
  SgName local_name(orig_var_name.c_str());
  if (SageInterface::is_Fortran_language()) {
    local_name = SgName(param->get_name());
  } else {
    // Nested outlining can move a distinct same-spelling local into the new
    // function before captured-variable unpacking.  The remap is identity
    // based, so allocate a distinct lexical name instead of manufacturing a
    // false redeclaration relationship.
    local_name = allocateExactUnpackLocalName(local_name, scope);
  }

  // This is the right hand of the assignment we want to build
  //
  // ----------step  2. Create the right hand
  // expression------------------------------------ No need to have right hand
  // for fortran
  if (SageInterface::is_Fortran_language()) {
    local_val = NULL;
    SgType *source_type = i_name->get_fortran_source_type();
    if (SgPointerType *source_pointer = isSgPointerType(source_type)) {
      source_type = source_pointer->get_base_type();
      if (source_type == nullptr) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-pointer-abi]: captured "
                "POINTER '%s' has no exact associated-target source type\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }
    SgVariableDeclaration *declaration =
        buildVariableDeclaration(local_name, local_type, local_val, scope);
    SgInitializedName *generated_name =
        SageInterface::getFirstInitializedName(declaration);
    if (generated_name == NULL ||
        generated_name->get_fortran_source_type() == NULL ||
        !SageInterface::fortranSourceTypeMatchesSemanticType(
            generated_name->get_fortran_source_type(), local_type)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[fortran-source-type]: captured "
              "variable=%p/%s did not produce one exact generated source "
              "surface for declaration=%p\n",
              static_cast<const void *>(i_name),
              i_name->get_name().getString().c_str(),
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    if (source_type == NULL) {
      SgVariableDeclaration *implicit_declaration =
          isSgVariableDeclaration(i_name->get_parent());
      const bool exact_parameter_absence =
          isSgFunctionParameterList(i_name->get_parent()) != NULL;
      const bool exact_implicit_absence =
          implicit_declaration != NULL &&
          implicit_declaration->get_fortran_declaration_origin() ==
              SgVariableDeclaration::e_fortran_semantic_only_declaration &&
          isSgAuxiliaryDeclarationList(implicit_declaration->get_parent()) !=
              NULL;
      if ((!exact_parameter_absence && !exact_implicit_absence) ||
          i_name->get_fortran_source_derived_type_symbol() != NULL ||
          i_name->get_fortran_type_spec() !=
              SgInitializedName::e_fortran_type_spec_default ||
          !i_name->get_fortran_procedure_interface().is_null()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[fortran-source-type]: captured "
                "variable=%p/%s has no source type outside the exact typed "
                "procedure-parameter absence contract\n",
                static_cast<const void *>(i_name),
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
      return declaration;
    }
    if (source_type == local_type && (isSgClassType(local_type) != NULL ||
                                      isSgFunctionType(local_type) != NULL)) {
      return declaration;
    }
    SgType *copied_source_type = SageInterface::deepCopy(source_type);
    if (copied_source_type == NULL || copied_source_type == source_type ||
        !SageInterface::fortranSourceTypeMatchesSemanticType(copied_source_type,
                                                             local_type)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[fortran-source-type]: captured "
              "variable=%p/%s did not produce one independent compatible "
              "source type for declaration=%p\n",
              static_cast<const void *>(i_name),
              i_name->get_name().getString().c_str(),
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    generated_name->set_fortran_source_type(copied_source_type);
    return declaration;
  }
  // for non-fortran language
  // Create an expression that "unpacks" the parameter.
  //   case 1: default is to use the parameter directly
  //   case 2:  for array of pointer type parameter,  build an array element
  //   reference case 3: for structure type parameter, build a field reference
  //     we use type casting to have generic void * parameter and void* data
  //     structure fields so less types are to exposed during translation
  //   class Hello **this__ptr__ = (class Hello **)(((struct OUT__1__1527___data
  //   *)__out_argv) -> this__ptr___p);
  SgExpression *param_ref = NULL;
  if (Outliner::useParameterWrapper) // using index for a wrapper parameter
  {
    if (Outliner::useStructureWrapper) { // case 3: structure type parameter
      if (struct_decl != NULL) {
        SgClassDefinition *struct_def =
            isSgClassDeclaration(struct_decl->get_definingDeclaration())
                ->get_definition();
        ROSE_ASSERT(struct_def != NULL);
        string field_name = orig_var_name;
        if (isPointerDeref)
          field_name = field_name + "_p";
        // __out_argv->i  or __out_argv->sum_p , depending on if pointer
        // deference is needed param_ref = buildArrowExp(buildVarRefExp(param,
        // scope), buildVarRefExp(field_name, struct_def)); We use void* for all
        // pointer types elements within the data structure. So type casting is
        // needed here e.g.   class Hello **this__ptr__ = (class Hello
        // **)(((struct OUT__1__1527___data *)__out_argv) -> this__ptr___p);

        SgVarRefExp *field_ref = buildVarRefExp(field_name, struct_def);
        param_ref = buildArrowExp(
            buildCastExp(buildVarRefExp(param, scope),
                         buildPointerType(struct_decl->get_type())),
            field_ref, field_ref->get_type());
        if (!isSgArrayType(local_type)) {
          // When necessary, we must catch the address before we do the casting
          if (!isPointerDeref && is_array_parameter) {
            param_ref = buildAddressOfOp(
                param_ref,
                ASTtools::buildAddressOfResultType(param_ref->get_type()));
          }

          param_ref = buildCastExp(param_ref, local_type);
        }
      } else {
        cerr << "Outliner::createUnpackDecl(): no need to unpack anything "
                "since struct_decl is NULL."
             << endl;
        ROSE_ABORT();
      }
    } else // case 2: array of pointers
    {
      SgVarRefExp *parameter_ref = buildVarRefExp(param, scope);
      param_ref = buildPntrArrRefExp(
          parameter_ref, buildIntVal(index),
          SageInterface::getElementType(parameter_ref->get_type()));
    }
  } else // default case 1:  each variable has a pointer typed parameter ,
         // this is not necessary but we have a classic model for optimizing
         // this
  {
    param_ref = buildVarRefExp(param, scope);
  }

  ROSE_ASSERT(param_ref != NULL);

  if (Outliner::useStructureWrapper) {
    // Or for structure type paramter
    // int (*sum)[100UL] = __out_argv->sum_p; // is PointerDeref type
    // int i = __out_argv->i;
    local_val = buildAssignInitializer(param_ref, local_type);
  } else {
    // TODO: This is only needed for case 2 or C++ case 1,
    //  not for case 1 and case 3 since the source right hand already has the
    //  right type since the array has generic void* elements. We need to cast
    //  from 'void *' to 'LOCAL_VAR_TYPE *' Special handling for C++ reference
    //  type: addressOf (refType) == addressOf(baseType) So unpacking it to
    //  baseType*
    SgReferenceType *ref = isSgReferenceType(orig_var_type);
    SgType *local_var_type_ptr =
        global_array_type != NULL
            ? local_type
            : SgPointerType::createType(ref ? ref->get_base_type()
                                            : orig_var_type);
    ROSE_ASSERT(local_var_type_ptr);
    SgCastExp *cast_expr =
        buildCastExp(param_ref, local_var_type_ptr, SgCastExp::e_C_style_cast);
    if (orig_var_source_type != NULL) {
      SgReferenceType *source_ref = isSgReferenceType(orig_var_source_type);
      SgType *source_cast_type =
          global_array_type != NULL
              ? local_source_type
              : SgPointerType::createType(source_ref
                                              ? source_ref->get_base_type()
                                              : orig_var_source_type);
      if (source_cast_type == NULL ||
          !SageInterface::cxxSourceTypeMatchesSemanticType(
              source_cast_type, local_var_type_ptr) ||
          cast_expr->get_source_type() != local_var_type_ptr) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[unpack-cast-source-type]: captured "
                "variable=%s has no exact source cast type\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
      cast_expr->set_source_type(source_cast_type);
      if (cast_expr->get_type() != local_var_type_ptr ||
          cast_expr->get_source_type() != source_cast_type) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[unpack-cast-source-type]: captured "
                "variable=%s did not preserve semantic and source cast "
                "types\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }

    if (Outliner::temp_variable) // variable cloning is enabled
    {
      // int* ip = (int *)(__out_argv[1]); // isPointerDeref == true
      // int i = *(int *)(__out_argv[1]);
      if (isPointerDeref && !use_cxx_reference_for_pointer_deref) {
        local_val = buildAssignInitializer(
            cast_expr, local_type); // casting is enough for pointer types
      } else // temp variable need additional dereferencing from the parameter
             // on the right side
      {
        local_val = buildAssignInitializer(
            buildPointerDerefExp(cast_expr, local_type), local_type);
      }
    } else // conventional pointer dereferencing algorithm
    {
      // int* ip = (int *)(__out_argv[1]);
      if (is_C_language()) // using pointer dereferences
      {
        local_val = buildAssignInitializer(cast_expr, local_type);
      } else if (is_Cxx_language())
      // We use reference type in the outlined function's body for C++
      // need the original value from a dereferenced type
      // using pointer dereferences to get the original type
      //  we use reference type instead of pointer type for C++
      /*
       * extern "C" void OUT__1__8452__(int *ip__,int *jp__,int
       * (*sump__)[100UL]) { int &i =  *((int *)ip__); int &j =  *((int *)jp__);
       *   int (&sum)[100UL] =  *((int (*)[100UL])sump__);
       *   ...
       * };
       */
      {
        local_val = buildAssignInitializer(
            buildPointerDerefExp(cast_expr, local_type), local_type);
      } else {
        printf("No other languages are supported by outlining currently. \n");
        ROSE_ABORT();
      }
    }
  }

  SgVariableDeclaration *decl;
  if (isSgArrayType(local_type->stripType(
          SgType::STRIP_TYPEDEF_TYPE))) { // The original variable was no
                                          // statically allocated and passed as
                                          // private or firstprivate
    // We need to copy every element of the array
    decl = buildVariableDeclaration(local_name, local_type, NULL, scope);

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(decl,"testing
    // Outliner createUnpackDecl(): 1") == false);

    SgStatementPtrList loop_indexes;
    SgStatement *array_init = build_array_unpacking_statement(
        buildVarRefExp(decl), param_ref,
        local_type->stripType(SgType::STRIP_TYPEDEF_TYPE), scope, loop_indexes);
    SageInterface::prependStatement(array_init, scope);
    SageInterface::prependStatementList(loop_indexes, scope);

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(decl,"testing
    // Outliner createUnpackDecl(): 2") == false);
  } else {
    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(scope,"testing
    // Outliner createUnpackDecl(): 3-scope") == false);

    decl = buildVariableDeclaration(local_name, local_type, local_val, scope);

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(scope,"testing
    // Outliner createUnpackDecl(): 4-scope") == false);
    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(decl,"testing
    // Outliner createUnpackDecl(): 4") == false);
  }

  if (local_source_type != NULL) {
    SgInitializedName *local_name_node =
        SageInterface::getFirstInitializedName(decl);
    if (local_name_node == NULL ||
        local_name_node->get_cxx_source_type() != NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[unpack-source-type]: generated "
              "declaration=%p for variable=%s has no empty exact source-type "
              "slot\n",
              static_cast<void *>(decl),
              i_name->get_name().getString().c_str());
      ROSE_ABORT();
    }
    local_name_node->set_cxx_source_type(local_source_type);
    if (local_name_node->get_cxx_source_type() != local_source_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[unpack-source-type]: generated "
              "declaration=%p for variable=%s did not publish its exact "
              "source type\n",
              static_cast<void *>(decl),
              i_name->get_name().getString().c_str());
      ROSE_ABORT();
    }
  }

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(decl,"testing Outliner
  // createUnpackDecl(): 5") == false);

  return decl;
}

/*!
 *  \brief Creates an assignment to "pack" a local variable back into
 *  an outlined-function parameter that has been passed as a pointer
 *  value.
 *  Only applicable when variable cloning is turned on.
 *
 *  The concept of pack/unpack is associated with parameter wrapping
 *  In the outlined function, we first
 *    unpack the wrapper parameter to get individual parameters
 *  then
 *    pack individual parameter into the wrapper.
 *
 *  It is also write-back or transfer-back the values of clones to their
 * original pointer variables
 *
 *  This routine takes the original "unpack" definition, of the form
 *
 *    TYPE local_unpack_var = *outlined_func_arg; // no parameter wrapping
 *    int i = *(int *)(__out_argv[1]); // parameter wrapping case
 *
 *  and creates the "re-pack" assignment expression,
 *
 *    *outlined_func_arg = local_unpack_var // no-parameter wrapping case
 *    *(int *)(__out_argv[1]) =i; // parameter wrapping case
 *
 *  C++ variables of reference types do not need this step.
 */
static SgAssignOp *createPackExpr(SgInitializedName *local_unpack_def) {
  if (!Outliner::temp_variable) {
    if (is_C_language()) // skip for pointer dereferencing used in C language
    {
      if (Outliner::enable_debug) {
        cout << "createPackExpr() in " << __FILE__
             << " skiping creating restoring expresion because of "
                "!temp_variable && is_C "
             << endl;
      }
      return NULL;
    }
  }
  // reference types do not need copy the value back in any cases
  if (isSgReferenceType(local_unpack_def->get_type())) {
    if (Outliner::enable_debug) {
      cout << "createPackExpr() in " << __FILE__
           << " skiping creating restoring expresion because of reference type "
              "of C++ "
           << endl;
    }
    return NULL;
  }

  // Liao 10/26/2009, Most of time, using data structure of parameters don't
  // need copy a local variable's value back to its original parameter
  if (Outliner::useStructureWrapper) {
    if (is_Cxx_language()) {
      if (Outliner::enable_debug) {
        cout << "createPackExpr() in " << __FILE__
             << " skiping creating restoring expresion because of "
                "useStructureWrapper && is_Cxx "
             << endl;
      }
      return NULL;
    }
  }

  if (local_unpack_def)
  //      && !isConstType(local_unpack_def->get_type ()))
  {
    SgName local_var_name(local_unpack_def->get_name());

    SgAssignInitializer *local_var_init =
        isSgAssignInitializer(local_unpack_def->get_initializer());
    ROSE_ASSERT(local_var_init);

    // Create the LHS, which derefs the function argument, by
    // copying the original dereference expression.
    //
    SgPointerDerefExp *param_deref_unpack =
        isSgPointerDerefExp(local_var_init->get_operand_i());
    if (param_deref_unpack == NULL) {
      cout << "packing statement is:"
           << local_unpack_def->get_declaration()->unparseToString() << endl;
      cout << "local unpacking stmt's initializer's operand has non-pointer "
              "dereferencing type:"
           << local_var_init->get_operand_i()->class_name() << endl;
      ROSE_ASSERT(param_deref_unpack);
    }

    SgPointerDerefExp *param_deref_pack =
        isSgPointerDerefExp(ASTtools::deepCopy(param_deref_unpack));
    ROSE_ASSERT(param_deref_pack);

    // Create the RHS, which references the local variable.
    SgScopeStatement *scope = local_unpack_def->get_scope();
    ROSE_ASSERT(scope);
    SgVariableSymbol *local_var_sym = scope->lookup_var_symbol(local_var_name);
    ROSE_ASSERT(local_var_sym);
    SgVarRefExp *local_var_ref = SageBuilder::buildVarRefExp(local_var_sym);
    ROSE_ASSERT(local_var_ref);

    // Assemble the final assignment expression.
    return SageBuilder::buildAssignOp(param_deref_pack, local_var_ref,
                                      param_deref_pack->get_type());
  }
  return 0;
}

/*!
 *  \brief Creates a pack (write-back) statement , used to support variable
cloning in outlining.
 *
 *
 *  This routine creates an SgExprStatement wrapper around the return
 *  of createPackExpr.
 *
 *  void OUT__1__4305__(int *ip__,int *sump__)
 * {
 *   // variable clones for pointer types
 *   int i =  *((int *)ip__);
 *   int sum =  *((int *)sump__);
 *
 *  // clones participate computation
 *   for (i = 0; i < 100; i++) {
 *     sum += i;
 *   }
 *  // write back the values from clones to their original pointers
 *  //The following are called (re)pack statements
 *    *((int *)sump__) = sum;
 *    *((int *)ip__) = i;
}

 */
static SgExprStatement *createPackStmt(SgInitializedName *local_unpack_def) {
  // No repacking for Fortran for now
  if (local_unpack_def == NULL || SageInterface::is_Fortran_language())
    return NULL;

  SgAssignOp *pack_expr = createPackExpr(local_unpack_def);

  if (pack_expr)
    return SageBuilder::buildExprStatement(pack_expr);
  else
    return 0;
}

/*!
 *  \brief Records a mapping between two variable symbols, and record
 *  the new symbol.
 *
 *  This routine creates the target variable symbol from the specified
 *  SgInitializedName object. If the optional scope is specified
 *  (i.e., is non-NULL), then this routine also inserts the new
 *  variable symbol into the scope's symbol table.
 */
static void recordSymRemap(const SgVariableSymbol *orig_sym,
                           SgInitializedName *name_new, SgScopeStatement *scope,
                           VarSymRemap_t &sym_remap) {
  if (orig_sym && name_new) {
    ROSE_ASSERT(name_new->get_name().is_null() == false);

    // Name lookup can legally find an older same-spelled declaration in a
    // parent scope.  Publication is keyed by the generated declaration's exact
    // identity in the destination table, never by spelling visibility.
    SgVariableSymbol *sym_new =
        scope != NULL
            ? isSgVariableSymbol(scope->find_symbol_from_declaration(name_new))
            : NULL;
    if (sym_new == NULL) {
      sym_new = new SgVariableSymbol(name_new);

      if (scope) {
        scope->insert_symbol(name_new->get_name(), sym_new);
        name_new->set_scope(scope);
      }
    }
    SgSymbolTable *table = scope != NULL ? scope->get_symbol_table() : NULL;
    if (sym_new == NULL || scope == NULL || table == NULL ||
        sym_new == orig_sym || sym_new->get_declaration() != name_new ||
        name_new->get_scope() != scope || sym_new->get_parent() != table ||
        !table->exists(sym_new) ||
        !sym_remap.emplace(orig_sym, sym_new).second) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[symbol-remap-publication]: "
              "source=%p generated-name=%p scope=%p symbol=%p table=%p has "
              "no unique exact generated identity\n",
              static_cast<const void *>(orig_sym),
              static_cast<void *>(name_new), static_cast<void *>(scope),
              static_cast<void *>(sym_new), static_cast<void *>(table));
      ROSE_ABORT();
    }
  }
}

/*!
 *  \brief Records a mapping between variable symbols.
 *    Used when generating unpacking statements from parameters.
 *    A mapping is a mapping between
 *       1. original variable in the program, to be passed as a parameter
 *       2. locally declared variable in the outlined function, accepting the
 * value of the parameter
 *  \pre The variable declaration must contain only 1 initialized
 *  name.
 *  orig_sym: the original variable to be passed into the outlined function
 *  new_decl: locally declared variable in the outlined function
 *  scope: the function body scope of the outlined function, also the scope of
 * new_decl
 */
static void recordSymRemap(const SgVariableSymbol *orig_sym,
                           SgVariableDeclaration *new_decl,
                           SgScopeStatement *scope, VarSymRemap_t &sym_remap) {
  if (orig_sym && new_decl) {
    //     ROSE_ASSERT (new_decl->get_scope == scope);
    SgInitializedNamePtrList &vars = new_decl->get_variables();
    ROSE_ASSERT(vars.size() == 1);
    for (SgInitializedNamePtrList::iterator i = vars.begin(); i != vars.end();
         ++i)
      recordSymRemap(orig_sym, *i, scope, sym_remap);
  }
}

struct CheckedCastSnapshot {
  SgCastExp *cast;
  SgCastExp::cast_type_enum source_surface;
  SgCastExp::semantic_conversion_kind_enum semantic_conversion;
  SgCastExp::value_category_enum value_category;
  SgType *result_type;
  SgTypePtrList base_path;
};

static SgType *removeExactReferenceLayer(SgType *type) {
  if (SgReferenceType *reference = isSgReferenceType(type))
    return reference->get_base_type();
  if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type))
    return reference->get_base_type();
  return type;
}

static void validateCheckedCasts(SgNode *root, const char *phase) {
  ROSE_ASSERT(root != NULL);
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    if (SgCastExp *cast = isSgCastExp(*current)) {
      cast->validate_semantic_conversion();
      if (cast->get_operand() == NULL ||
          cast->get_operand()->get_parent() != cast) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[checked-cast-owner]: %s cast=%p has "
                "no exclusively owned operand\n",
                phase, static_cast<void *>(cast));
        ROSE_ABORT();
      }
    }
  }
}

static std::vector<CheckedCastSnapshot>
captureEnclosingCheckedCasts(SgExpression *expression) {
  ROSE_ASSERT(expression != NULL);
  std::vector<CheckedCastSnapshot> snapshots;
  std::set<SgNode *> visited;
  for (SgNode *current = expression->get_parent(); current != NULL;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[checked-cast-owner]: expression=%p has "
              "a cycle in its enclosing owner chain\n",
              static_cast<void *>(expression));
      ROSE_ABORT();
    }
    if (SgCastExp *cast = isSgCastExp(current)) {
      cast->validate_semantic_conversion();
      snapshots.push_back({cast, cast->get_cast_type(),
                           cast->get_semantic_conversion_kind(),
                           cast->get_value_category(), cast->get_type(),
                           cast->get_conversion_base_path()});
    }
    if (isSgStatement(current) != NULL)
      break;
  }
  return snapshots;
}

static void validateEnclosingCheckedCasts(
    const std::vector<CheckedCastSnapshot> &snapshots) {
  for (const CheckedCastSnapshot &snapshot : snapshots) {
    SgCastExp *cast = snapshot.cast;
    if (cast == NULL || cast->get_cast_type() != snapshot.source_surface ||
        cast->get_semantic_conversion_kind() != snapshot.semantic_conversion ||
        cast->get_value_category() != snapshot.value_category ||
        cast->get_type() != snapshot.result_type ||
        cast->get_conversion_base_path() != snapshot.base_path) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[checked-cast-remap]: cast=%p changed "
              "source surface, semantic conversion, value category, result "
              "type, or checked base path during variable remapping\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }
    cast->validate_semantic_conversion();
  }
}

static void requireExactVarRefRetargetTypes(
    SgVarRefExp *reference, SgVariableSymbol *replacement,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_plan) {
  SgVariableSymbol *original =
      reference != NULL ? reference->get_symbol() : NULL;
  SgInitializedName *original_declaration =
      original != NULL ? original->get_declaration() : NULL;
  SgInitializedName *replacement_declaration =
      replacement != NULL ? replacement->get_declaration() : NULL;
  SgType *original_declared_type =
      original_declaration != NULL ? original_declaration->get_type() : NULL;
  SgType *replacement_declared_type = replacement_declaration != NULL
                                          ? replacement_declaration->get_type()
                                          : NULL;
  SgType *original_reference_type =
      reference != NULL ? reference->get_type() : NULL;
  SgPointerType *fortran_original_pointer =
      SageInterface::is_Fortran_language()
          ? isSgPointerType(original_declared_type)
          : nullptr;
  const bool exact_fortran_associated_target_retarget =
      fortran_original_pointer != nullptr &&
      original_reference_type == original_declared_type &&
      replacement_declared_type == fortran_original_pointer->get_base_type();
  const bool exact_c_array_parameter_decay = isExactCArrayParameterDecay(
      removeExactReferenceLayer(original_reference_type),
      removeExactReferenceLayer(replacement_declared_type));
  const Outliner::OutlinedLocalTypeTemplateEntry *local_type_entry =
      findLocalTypeTemplateEntry(local_type_plan, original_declared_type);
  const bool exact_local_type_template_retarget =
      local_type_entry != NULL &&
      original_reference_type == local_type_entry->source_type &&
      removeExactReferenceLayer(replacement_declared_type) ==
          local_type_entry->defining_parameter_type;
  if (reference == NULL || original == NULL || replacement == NULL ||
      original_declaration == NULL || replacement_declaration == NULL ||
      original_declared_type == NULL || replacement_declared_type == NULL ||
      original_reference_type != original_declared_type ||
      replacement->get_type() != replacement_declared_type ||
      (!exact_fortran_associated_target_retarget &&
       !exact_c_array_parameter_decay && !exact_local_type_template_retarget &&
       removeExactReferenceLayer(original_reference_type) !=
           removeExactReferenceLayer(replacement_declared_type))) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[var-ref-remap-type]: reference=%p "
            "original-symbol=%p replacement-symbol=%p does not preserve one "
            "exact declared/referenced type (apart from an explicit C++ "
            "reference alias layer)\n",
            static_cast<void *>(reference), static_cast<void *>(original),
            static_cast<void *>(replacement));
    ROSE_ABORT();
  }
}

// ===========================================================
//! Fixes up references in a block to point to alternative symbols.
// based on an existing symbol-to-symbol map
// Also called variable substitution or variable replacement
static void remapVarSyms(
    const VarSymRemap_t &vsym_remap,    // regular shared variables
    const VarSymRemap_t &private_remap, // variables using private copies
    SgBasicBlock *b,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_plan) {
  validateCheckedCasts(b, "before variable remapping");

  // Check if variable remapping is even needed.
  if (vsym_remap.empty() && private_remap.empty())
    return;

  // A spelling is not declaration identity: a captured local and a class data
  // member can legally have the same name.  Keep every ordinary remap keyed by
  // the exact symbol or declaration.  The separately checked global-name map
  // exists only for declarations reparsed into a distinct output file.
  std::map<const SgInitializedName *, SgVariableSymbol *> remap_by_decl;
  std::map<std::string, SgVariableSymbol *> remap_by_global_name;
  for (VarSymRemap_t::const_iterator i = vsym_remap.begin();
       i != vsym_remap.end(); ++i) {
    const SgVariableSymbol *orig_sym = i->first;
    SgVariableSymbol *new_sym = i->second;
    if (orig_sym == NULL || new_sym == NULL)
      continue;

    const SgInitializedName *orig_decl = orig_sym->get_declaration();
    if (orig_decl == NULL)
      continue;

    const std::pair<
        std::map<const SgInitializedName *, SgVariableSymbol *>::iterator, bool>
        declaration_remap = remap_by_decl.emplace(orig_decl, new_sym);
    if (!declaration_remap.second &&
        declaration_remap.first->second != new_sym) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[var-ref-remap-identity]: declaration=%p "
              "maps to more than one replacement symbol\n",
              static_cast<const void *>(orig_decl));
      ROSE_ABORT();
    }

    if (isSgGlobal(orig_decl->get_scope()) != NULL) {
      const std::pair<std::map<std::string, SgVariableSymbol *>::iterator, bool>
          global_remap = remap_by_global_name.emplace(
              orig_decl->get_name().getString(), new_sym);
      if (!global_remap.second && global_remap.first->second != new_sym) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[var-ref-remap-identity]: global "
                "name=%s maps to more than one replacement symbol\n",
                orig_decl->get_name().str());
        ROSE_ABORT();
      }
    }
  }

  auto findRegularRemap = [&](SgVarRefExp *ref_orig) -> SgVariableSymbol * {
    ROSE_ASSERT(ref_orig != NULL);
    SgVariableSymbol *ref_sym = ref_orig->get_symbol();
    if (ref_sym == NULL)
      return NULL;

    VarSymRemap_t::const_iterator ref_new = vsym_remap.find(ref_sym);
    if (ref_new != vsym_remap.end())
      return ref_new->second;

    SgInitializedName *ref_decl = ref_sym->get_declaration();
    if (ref_decl == NULL)
      return NULL;

    std::map<const SgInitializedName *, SgVariableSymbol *>::const_iterator
        decl_match = remap_by_decl.find(ref_decl);
    if (decl_match != remap_by_decl.end())
      return decl_match->second;

    if (Outliner::useNewFile && isSgGlobal(ref_decl->get_scope()) != NULL) {
      std::map<std::string, SgVariableSymbol *>::const_iterator name_match =
          remap_by_global_name.find(ref_decl->get_name().getString());
      if (name_match != remap_by_global_name.end())
        return name_match->second;
    }

    return NULL;
  };

  auto clearQualification = [](SgVarRefExp *var_ref) {
    ROSE_ASSERT(var_ref != NULL);
    var_ref->set_name_qualification_length(0);
    var_ref->set_global_qualification_required(false);
    var_ref->set_explicit_name_qualification_length(-1);
    var_ref->set_explicit_global_qualification(false);
    var_ref->set_explicit_name_qualification_tokens(SgStringList());
  };

  auto markLocatedRewrite = [b](SgLocatedNode *node) {
    ROSE_ASSERT(node != NULL);
    node->markAsModified();
    // Rewriting semantic-only frontend structure turns that exact node into a
    // lexical part of the outlined function. Promote it at this producer
    // boundary against the destination body; untouched semantic descendants
    // retain their non-lexical provenance.
    if (SageInterface::hasSemanticOnlyFrontendSourcePosition(node)) {
      SageInterface::promoteSemanticOnlyNodeToGeneratedOutput(node, b);
    } else {
      node->setTransformation();
    }
  };

  auto propagateTransformationToStatementAncestors = [&markLocatedRewrite](
                                                         SgStatement *stmt) {
    for (SgNode *node = stmt; node != NULL; node = node->get_parent()) {
      SgStatement *ancestor = isSgStatement(node);
      if (ancestor == NULL)
        continue;

      AttachedPreprocessingInfoType *records =
          ancestor->getAttachedPreprocessingInfo();
      if (records != NULL) {
        for (PreprocessingInfo *record : *records) {
          if (record == NULL || !record->has_file_info()) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[rewritten-preprocessing-owner]: "
                    "statement=%p owns an incomplete preprocessing record\n",
                    static_cast<void *>(ancestor));
            ROSE_ABORT();
          }
          if (record->getOutputPlacement() ==
              PreprocessingInfo::source_position) {
            SageInterface::publishPreprocessingInfoPhysicalOutputOwner(
                record, ancestor);
          }
        }
      }

      ancestor->set_containsTransformation(true);
      ancestor->set_containsTransformationToSurroundingWhitespace(true);
      markLocatedRewrite(ancestor);

      if (isSgFunctionDeclaration(ancestor) != NULL)
        break;
    }
  };

  std::set<SgVarRefExp *> intentionally_retired_references;
  std::vector<SgExpression *> retired_original_expression_trees;
  std::vector<SgVarRefExp *> retired_replaced_references;

  auto detachReplacedReference = [&](SgVarRefExp *original,
                                     SgExpression *replacement,
                                     SgNode *exactOwner) {
    if (original == NULL || replacement == NULL || exactOwner == NULL ||
        original->get_parent() != NULL ||
        replacement->get_parent() != exactOwner) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[var-ref-remap-retirement]: "
              "original=%p replacement=%p owner=%p did not complete one "
              "exact replacement transaction\n",
              static_cast<void *>(original), static_cast<void *>(replacement),
              static_cast<void *>(exactOwner));
      ROSE_ABORT();
    }
    size_t originalEdges = 0;
    size_t replacementEdges = 0;
    for (const auto &edge : exactOwner->returnDataMemberPointers()) {
      originalEdges += edge.first == original ? 1 : 0;
      replacementEdges += edge.first == replacement ? 1 : 0;
    }
    if (originalEdges != 0 || replacementEdges != 1) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[var-ref-remap-retirement]: "
              "original=%p has %zu owner edges and replacement=%p has %zu "
              "instead of zero and one\n",
              static_cast<void *>(original), originalEdges,
              static_cast<void *>(replacement), replacementEdges);
      ROSE_ABORT();
    }
    retired_replaced_references.push_back(original);
  };

  auto dropOriginalExpressionTreesInAncestors = [&](SgNode *node) {
    for (SgNode *ancestor = node; ancestor != NULL;
         ancestor = ancestor->get_parent()) {
      if (SgExpression *expr = isSgExpression(ancestor)) {
        if (SgExpression *source = expr->get_originalExpressionTree()) {
          if (source->get_parent() != expr) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[original-expression-provenance]: "
                    "outliner found a source expression without its exact "
                    "owner\n");
            ROSE_ABORT();
          }
          for (SgNode *source_node :
               NodeQuery::querySubTree(source, V_SgVarRefExp)) {
            SgVarRefExp *retired_reference = isSgVarRefExp(source_node);
            ROSE_ASSERT(retired_reference != NULL);
            intentionally_retired_references.insert(retired_reference);
          }
          expr->set_originalExpressionTree(NULL);
          source->set_parent(nullptr);
          retired_original_expression_trees.push_back(source);
          markLocatedRewrite(expr);
        }
      }

      if (isSgStatement(ancestor) != NULL)
        break;
    }
  };

  auto retireEnclosingMacroSurfaces = [&](SgExpression *expression) {
    std::vector<SgMacroExpansionExp *> macros;
    for (SgNode *owner = expression; owner != NULL;
         owner = owner->get_parent()) {
      if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(owner)) {
        macros.push_back(macro);
      }
      if (isSgStatement(owner) != NULL)
        break;
    }

    for (SgMacroExpansionExp *macro : macros) {
      SgExpression *expanded = macro->get_expanded_expression_checked();
      SgNode *owner = macro->get_parent();
      size_t exact_edges = 0;
      for (SgNode *edge : macro->get_traversalSuccessorContainer())
        exact_edges += edge == expanded ? 1 : 0;
      if (owner == NULL || expanded->get_parent() != macro ||
          exact_edges != 1) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[macro-surface-retirement]: "
                "macro=%p owner=%p expanded=%p edges=%zu has no exact "
                "semantic publication transaction\n",
                static_cast<void *>(macro), static_cast<void *>(owner),
                static_cast<void *>(expanded), exact_edges);
        ROSE_ABORT();
      }

      macro->set_expanded_expression(NULL);
      expanded->set_parent(NULL);
      SageInterface::replaceExpression(macro, expanded, true);
      if (macro->get_parent() != NULL || expanded->get_parent() != owner) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[macro-surface-retirement]: "
                "macro=%p was not retired in favor of semantic expression=%p "
                "under owner=%p\n",
                static_cast<void *>(macro), static_cast<void *>(expanded),
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
      SageInterface::deleteAST(macro,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (SgNode::isLiveNode(macro)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[macro-surface-retirement]: "
                "detached macro=%p remained live\n",
                static_cast<void *>(macro));
        ROSE_ABORT();
      }
    }
  };

  auto markVarRefRewrite = [&](SgVarRefExp *ref) {
    ROSE_ASSERT(ref != NULL);
    dropOriginalExpressionTreesInAncestors(ref);
    markLocatedRewrite(ref);
    if (SgStatement *stmt = SageInterface::getEnclosingStatement(ref)) {
      markLocatedRewrite(stmt);
      stmt->set_containsTransformationToSurroundingWhitespace(true);
      propagateTransformationToStatementAncestors(stmt);
    }
  };

  auto clearLocalVarRefQualification = [&](SgVarRefExp *ref) {
    ROSE_ASSERT(ref != NULL);

    SgVariableSymbol *symbol = ref->get_symbol();
    if (symbol == NULL || symbol->get_declaration() == NULL)
      return;

    SgInitializedName *decl = symbol->get_declaration();
    if (!SageInterface::isAncestor(b, decl))
      return;

    SgVariableDeclaration *var_decl =
        isSgVariableDeclaration(decl->get_parent());
    if (var_decl == NULL)
      return;

    SgScopeStatement *scope = var_decl->get_scope();
    if (scope == NULL || isSgGlobal(scope) != NULL ||
        isSgNamespaceDefinitionStatement(scope) != NULL ||
        isSgClassDefinition(scope) != NULL)
      return;

    clearQualification(ref);
    markVarRefRewrite(ref);
  };

  auto retargetVarRefToSymbol = [&](SgVarRefExp *ref,
                                    SgVariableSymbol *symbol) {
    ROSE_ASSERT(ref != NULL);
    ROSE_ASSERT(symbol != NULL);
    requireExactVarRefRetargetTypes(ref, symbol, local_type_plan);
    const std::vector<CheckedCastSnapshot> enclosing_casts =
        captureEnclosingCheckedCasts(ref);

    SgStatement *stmt = SageInterface::getEnclosingStatement(ref);
    SgNode *exact_owner = ref->get_parent();
    const bool can_replace_expression =
        exact_owner != NULL &&
        getEnclosingNode<SgOmpClause>(ref, true) == NULL &&
        getEnclosingNode<SgOmpFlushStatement>(ref, true) == NULL;

    if (can_replace_expression) {
      SgVarRefExp *replacement = SageBuilder::buildVarRefExp(symbol);
      if (replacement == NULL ||
          replacement->get_type() != symbol->get_declaration()->get_type()) {
        fprintf(
            stderr,
            "REX_OUTLINER_INVARIANT[var-ref-remap-type]: replacement "
            "reference does not publish its symbol's exact declared type\n");
        ROSE_ABORT();
      }
      replacement->set_need_paren(ref->get_need_paren());
      dropOriginalExpressionTreesInAncestors(ref);
      clearQualification(ref);
      clearQualification(replacement);
      markLocatedRewrite(replacement);
      if (stmt != NULL) {
        markLocatedRewrite(stmt);
        stmt->set_containsTransformationToSurroundingWhitespace(true);
        propagateTransformationToStatementAncestors(stmt);
      }
      SageInterface::replaceExpression(isSgExpression(ref),
                                       isSgExpression(replacement), true);
      detachReplacedReference(ref, replacement, exact_owner);
      validateEnclosingCheckedCasts(enclosing_casts);
      return;
    }

    ref->set_symbol(symbol);
    clearQualification(ref);
    markVarRefRewrite(ref);
    validateEnclosingCheckedCasts(enclosing_casts);
  };

  auto remapToPointerDeref = [&](SgVarRefExp *ref_orig,
                                 SgVariableSymbol *sym_new) {
    ROSE_ASSERT(ref_orig != NULL);
    ROSE_ASSERT(sym_new != NULL);
    if (ref_orig->get_parent() == NULL || sym_new->get_declaration() == NULL ||
        sym_new->get_declaration()->get_type() == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[var-ref-remap-owner]: active reference "
              "or replacement symbol lost its exact structural/type owner\n");
      ROSE_ABORT();
    }

    const std::vector<CheckedCastSnapshot> enclosing_casts =
        captureEnclosingCheckedCasts(ref_orig);
    SgNode *exact_owner = ref_orig->get_parent();
    SgType *replacement_declared_type = sym_new->get_declaration()->get_type();
    SgPointerType *replacement_pointer_type =
        isSgPointerType(replacement_declared_type);
    SgType *replacement_pointee_type =
        replacement_pointer_type != NULL
            ? replacement_pointer_type->get_base_type()
            : NULL;
    SgType *original_value_type =
        removeExactReferenceLayer(ref_orig->get_type());
    if (replacement_pointer_type == NULL || replacement_pointee_type == NULL ||
        replacement_pointee_type != original_value_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[pointer-deref-remap-type]: reference=%p "
              "replacement-symbol=%p does not preserve one exact pointee/value "
              "type\n",
              static_cast<void *>(ref_orig), static_cast<void *>(sym_new));
      ROSE_ABORT();
    }
    SgVarRefExp *replacement_reference = buildVarRefExp(sym_new);
    if (replacement_reference == NULL ||
        replacement_reference->get_type() != replacement_declared_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[pointer-deref-remap-type]: replacement "
              "pointer reference has no exact declared type\n");
      ROSE_ABORT();
    }
    SgPointerDerefExp *deref_exp = SageBuilder::buildPointerDerefExp(
        replacement_reference, replacement_pointee_type);
    if (deref_exp == NULL || deref_exp->get_type() != original_value_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[pointer-deref-remap-type]: generated "
              "dereference has no exact original value type\n");
      ROSE_ABORT();
    }
    deref_exp->set_need_paren(true);

    if (SgOmpClause *omp_clause =
            getEnclosingNode<SgOmpClause>(ref_orig, true)) {
      SgOmpClauseList *clause_list =
          isSgOmpClauseList(omp_clause->get_parent());
      if (clause_list == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-clause-list]: outlining found a "
                "clause without exact clause-list ownership\n");
        ROSE_ABORT();
      }
      const SgOmpClausePtrList &clauses = clause_list->get_clauses();
      if (std::find(clauses.begin(), clauses.end(), omp_clause) ==
          clauses.end()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-clause-list]: outlining found a "
                "clause missing from its owning clause list\n");
        ROSE_ABORT();
      }
      SgOmpExecStatement *directive =
          isSgOmpExecStatement(clause_list->get_parent());
      SgOmpClauseBodyStatement *body_directive =
          isSgOmpClauseBodyStatement(directive);
      SgOmpClauseStatement *clause_directive =
          isSgOmpClauseStatement(directive);
      if (directive == NULL ||
          (body_directive == NULL && clause_directive == NULL) ||
          (body_directive != NULL &&
           body_directive->get_clause_list() != clause_list) ||
          (clause_directive != NULL &&
           clause_directive->get_clause_list() != clause_list)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[openmp-clause-list]: outlining found a "
                "clause list without an executable directive owner\n");
        ROSE_ABORT();
      }
      SgInitializedName *original_name =
          ref_orig->get_symbol()->get_declaration();
      if (original_name == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
                "directive=%p reference=%p has no exact initialized-name "
                "identity\n",
                static_cast<void *>(directive), static_cast<void *>(ref_orig));
        ROSE_ABORT();
      }

      auto directive_mapping = clause_variable_renaming_record.find(directive);
      if (directive_mapping == clause_variable_renaming_record.end()) {
        directive_mapping =
            clause_variable_renaming_record
                .emplace(directive,
                         new std::map<SgInitializedName *, SgExpression *>())
                .first;
      }
      std::map<SgInitializedName *, SgExpression *> *name_mapping =
          directive_mapping->second;
      if (name_mapping == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
                "directive=%p has a null exact identity map\n",
                static_cast<void *>(directive));
        ROSE_ABORT();
      }
      const auto inserted = name_mapping->emplace(original_name, deref_exp);
      if (!inserted.second) {
        SgExpression *existing_expression = inserted.first->second;
        SgPointerDerefExp *existing_deref =
            isSgPointerDerefExp(existing_expression);
        SgPointerDerefExp *existing_leaf_owner = existing_deref;
        SgVarRefExp *existing_reference = NULL;
        bool exact_existing_chain = existing_deref != NULL;
        while (exact_existing_chain && existing_leaf_owner != NULL) {
          SgExpression *operand = existing_leaf_owner->get_operand();
          SgPointerType *operand_pointer =
              operand != NULL ? isSgPointerType(operand->get_type()) : NULL;
          if (operand == NULL || operand->get_parent() != existing_leaf_owner ||
              operand_pointer == NULL ||
              operand_pointer->get_base_type() !=
                  existing_leaf_owner->get_type()) {
            exact_existing_chain = false;
            break;
          }
          if (SgPointerDerefExp *nested = isSgPointerDerefExp(operand)) {
            existing_leaf_owner = nested;
            continue;
          }
          existing_reference = isSgVarRefExp(operand);
          exact_existing_chain = existing_reference != NULL;
          break;
        }
        SgVarRefExp *new_reference = isSgVarRefExp(deref_exp->get_operand());
        SgVariableSymbol *existing_symbol =
            existing_reference != NULL
                ? isSgVariableSymbol(existing_reference->get_symbol())
                : NULL;
        SgInitializedName *existing_backing =
            existing_symbol != NULL ? existing_symbol->get_declaration() : NULL;
        SgPointerType *existing_pointer =
            existing_reference != NULL
                ? isSgPointerType(existing_reference->get_type())
                : NULL;
        SgPointerType *new_pointer =
            new_reference != NULL ? isSgPointerType(new_reference->get_type())
                                  : NULL;
        const bool exact_current_backing =
            exact_existing_chain && existing_reference != NULL &&
            new_reference != NULL &&
            existing_reference->get_symbol() == sym_new &&
            new_reference->get_symbol() == sym_new &&
            existing_reference->get_type() == sym_new->get_type() &&
            new_reference->get_type() == sym_new->get_type() &&
            existing_deref != NULL &&
            existing_deref->get_type() == deref_exp->get_type();
        const bool exact_nested_backing =
            exact_existing_chain && existing_symbol != NULL &&
            existing_backing != NULL && existing_symbol != sym_new &&
            existing_reference->get_type() == existing_symbol->get_type() &&
            new_reference->get_symbol() == sym_new &&
            new_reference->get_type() == sym_new->get_type() &&
            new_pointer != NULL &&
            new_pointer->get_base_type() == deref_exp->get_type() &&
            deref_exp->get_type() == existing_reference->get_type();
        const bool exact_superseded_backing =
            exact_existing_chain && existing_symbol != NULL &&
            existing_backing != NULL && existing_symbol != sym_new &&
            existing_backing != original_name &&
            existing_reference->get_type() == existing_symbol->get_type() &&
            new_reference->get_symbol() == sym_new &&
            new_reference->get_type() == sym_new->get_type() &&
            existing_pointer != NULL && new_pointer != NULL &&
            existing_deref != NULL &&
            existing_pointer->get_base_type() == existing_deref->get_type() &&
            new_pointer->get_base_type() == deref_exp->get_type() &&
            SageInterface::isEquivalentType(existing_deref->get_type(),
                                            deref_exp->get_type());
        if (existing_expression == NULL ||
            existing_expression->get_parent() != NULL ||
            deref_exp->get_parent() != NULL || existing_deref == NULL ||
            existing_reference == NULL || new_reference == NULL ||
            existing_leaf_owner == NULL ||
            existing_reference->get_parent() != existing_leaf_owner ||
            new_reference->get_parent() != deref_exp ||
            (!exact_current_backing && !exact_nested_backing &&
             !exact_superseded_backing)) {
          fprintf(
              stderr,
              "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
              "directive=%p initialized-name=%p existing-expression=%p "
              "existing-symbol=%p new-expression=%p new-symbol=%p "
              "expected-symbol=%p existing-type=%p new-type=%p "
              "expected-type=%p existing-backing=%p original-name=%p "
              "existing-pointer=%p existing-base=%p existing-result=%p "
              "new-pointer=%p new-base=%p new-result=%p equivalent=%d "
              "has conflicting exact backing symbol or type identities\n",
              static_cast<void *>(directive),
              static_cast<void *>(original_name),
              static_cast<void *>(existing_expression),
              static_cast<void *>(existing_reference != NULL
                                      ? existing_reference->get_symbol()
                                      : NULL),
              static_cast<void *>(deref_exp),
              static_cast<void *>(
                  new_reference != NULL ? new_reference->get_symbol() : NULL),
              static_cast<void *>(sym_new),
              static_cast<void *>(existing_reference != NULL
                                      ? existing_reference->get_type()
                                      : NULL),
              static_cast<void *>(
                  new_reference != NULL ? new_reference->get_type() : NULL),
              static_cast<void *>(sym_new->get_type()),
              static_cast<void *>(existing_backing),
              static_cast<void *>(original_name),
              static_cast<void *>(existing_pointer),
              static_cast<void *>(existing_pointer != NULL
                                      ? existing_pointer->get_base_type()
                                      : NULL),
              static_cast<void *>(
                  existing_deref != NULL ? existing_deref->get_type() : NULL),
              static_cast<void *>(new_pointer),
              static_cast<void *>(
                  new_pointer != NULL ? new_pointer->get_base_type() : NULL),
              static_cast<void *>(deref_exp->get_type()),
              existing_deref != NULL &&
                      SageInterface::isEquivalentType(
                          existing_deref->get_type(), deref_exp->get_type())
                  ? 1
                  : 0);
          ROSE_ABORT();
        }
        if (exact_nested_backing) {
          existing_leaf_owner->set_operand_i(deref_exp);
          deref_exp->set_parent(existing_leaf_owner);
          existing_reference->set_parent(NULL);
          if (existing_leaf_owner->get_operand() != deref_exp ||
              deref_exp->get_parent() != existing_leaf_owner ||
              existing_reference->get_parent() != NULL) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
                    "directive=%p failed exact nested pointer composition\n",
                    static_cast<void *>(directive));
            ROSE_ABORT();
          }
          SageInterface::deleteAST(
              existing_reference,
              SageInterface::DeleteAstMode::kRequireIsolated);
          if (SgNode::isLiveNode(existing_reference)) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
                    "directive=%p did not retire the superseded pointer "
                    "leaf\n",
                    static_cast<void *>(directive));
            ROSE_ABORT();
          }
        } else if (exact_superseded_backing) {
          inserted.first->second = deref_exp;
          SageInterface::deleteAST(
              existing_expression,
              SageInterface::DeleteAstMode::kRequireIsolated);
          if (SgNode::isLiveNode(existing_expression)) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[clause-variable-renaming-record]: "
                    "directive=%p did not retire the superseded exact "
                    "backing expression\n",
                    static_cast<void *>(directive));
            ROSE_ABORT();
          }
        } else {
          SageInterface::deleteAST(
              deref_exp, SageInterface::DeleteAstMode::kRequireIsolated);
        }
      }
      validateEnclosingCheckedCasts(enclosing_casts);
      return;
    }

    // flush lists are lowered to runtime calls and should not be rewritten as
    // dereference expressions in-place.
    if (getEnclosingNode<SgOmpFlushStatement>(ref_orig, true) != NULL) {
      SageInterface::deleteAST(deref_exp,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (SgNode::isLiveNode(deref_exp)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[flush-reference-remap]: detached "
                "dereference=%p remained live after exact retirement\n",
                static_cast<void *>(deref_exp));
        ROSE_ABORT();
      }
      validateEnclosingCheckedCasts(enclosing_casts);
      return;
    }

    // Replacing a reference inside a macro-expanded expression invalidates
    // both legacy original-expression trees and the typed macro invocation
    // surface.  Publish the expanded semantic expression before inserting the
    // dereference so unparsing cannot replay the pre-remap invocation spelling.
    dropOriginalExpressionTreesInAncestors(ref_orig);
    retireEnclosingMacroSurfaces(ref_orig);
    if (SgStatement *statement =
            SageInterface::getEnclosingStatement(ref_orig)) {
      markLocatedRewrite(statement);
      statement->set_containsTransformationToSurroundingWhitespace(true);
      propagateTransformationToStatementAncestors(statement);
    }

    // Keep the old node detached (instead of deep-deleting) while iterating
    // over a pre-collected reference list to avoid stale pointer reuse.
    SageInterface::replaceExpression(isSgExpression(ref_orig),
                                     isSgExpression(deref_exp), true);
    detachReplacedReference(ref_orig, deref_exp, exact_owner);
    validateEnclosingCheckedCasts(enclosing_casts);
  };

  // Find all variable references
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t refs = NodeQuery::querySubTree(b, V_SgVarRefExp);
  // For each of the references ,
  for (NodeList_t::iterator i = refs.begin(); i != refs.end(); ++i) {
    // Reference possibly in need of fix-up.
    SgVarRefExp *ref_orig = isSgVarRefExp(*i);
    ROSE_ASSERT(ref_orig);
    if (intentionally_retired_references.count(ref_orig) != 0) {
      if (SageInterface::isAncestor(b, ref_orig)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[var-ref-remap-owner]: intentionally "
                "retired reference=%p is still owned by the active body\n",
                static_cast<void *>(ref_orig));
        ROSE_ABORT();
      }
      continue;
    }
    if (ref_orig->get_parent() == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[var-ref-remap-owner]: untracked "
              "reference=%p became detached during variable remapping\n",
              static_cast<void *>(ref_orig));
      ROSE_ABORT();
    }
    clearLocalVarRefQualification(ref_orig);

    // Search for a symbol which need to be replaced.
    SgVariableSymbol *regular_remap = findRegularRemap(ref_orig);
    VarSymRemap_t::const_iterator ref_private =
        private_remap.find(ref_orig->get_symbol());

    // a variable could be both a variable needing passing original value and
    // private variable such as OpenMP firstprivate, lastprivate and reduction
    // variable For variable substitution, private remap has higher priority
    // remapping private variables
    if (ref_private != private_remap.end()) {
      // get the replacement variable
      SgVariableSymbol *sym_new = ref_private->second;
      // Do the replacement
      retargetVarRefToSymbol(ref_orig, sym_new);
    } else if (regular_remap !=
               NULL) // Needs replacement, regular shared variables
    {
      SgVariableSymbol *sym_new = regular_remap;
      SgInitializedName *replacement_declaration =
          sym_new != NULL ? sym_new->get_declaration() : NULL;
      SgType *replacement_type = replacement_declaration != NULL
                                     ? replacement_declaration->get_type()
                                     : NULL;
      SgType *original_value_type =
          removeExactReferenceLayer(ref_orig->get_type());
      SgType *replacement_value_type =
          removeExactReferenceLayer(replacement_type);
      SgPointerType *replacement_pointer = isSgPointerType(replacement_type);
      SgPointerType *fortran_original_pointer =
          SageInterface::is_Fortran_language()
              ? isSgPointerType(original_value_type)
              : nullptr;
      const bool exact_c_array_parameter_decay = isExactCArrayParameterDecay(
          original_value_type, replacement_value_type);
      const Outliner::OutlinedLocalTypeTemplateEntry *local_type_entry =
          findLocalTypeTemplateEntry(local_type_plan, original_value_type);
      const bool exact_local_type_template_retarget =
          local_type_entry != NULL &&
          replacement_value_type == local_type_entry->defining_parameter_type;

      // The replacement declaration's type is the complete remapping policy.
      // A same-value declaration (including one explicit C++ reference alias
      // layer) is retargeted directly.  A pointer parameter whose pointee is
      // the original value is dereferenced.  Language and command-line modes
      // cannot override either typed relationship.
      if (exact_local_type_template_retarget) {
        retargetVarRefToSymbol(ref_orig, sym_new);
      } else if (original_value_type != NULL &&
                 replacement_value_type == original_value_type) {
        retargetVarRefToSymbol(ref_orig, sym_new);
      } else if (exact_c_array_parameter_decay) {
        // C and C++ adjust an array parameter to a pointer to its element type.
        // The outlined declaration publishes that exact adjusted type, so the
        // body reference must bind directly to the parameter; adding another
        // dereference would change the original array expression semantics.
        retargetVarRefToSymbol(ref_orig, sym_new);
      } else if (fortran_original_pointer != nullptr &&
                 replacement_value_type ==
                     fortran_original_pointer->get_base_type()) {
        // The Fortran fork ABI maps a captured POINTER actual to an ordinary
        // implicit-reference dummy for its associated target.  References in
        // the outlined body therefore bind directly to that exact pointee
        // declaration; inserting a C-style dereference would manufacture an
        // expression that Fortran does not have.
        retargetVarRefToSymbol(ref_orig, sym_new);
      } else if (original_value_type != NULL && replacement_pointer != NULL &&
                 replacement_pointer->get_base_type() == original_value_type) {
        remapToPointerDeref(ref_orig, sym_new);
      } else {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[var-ref-remap-type]: reference=%p "
                "original-symbol=%p name=%s original-type=%p/%s "
                "replacement-symbol=%p name=%s replacement-type=%p/%s "
                "replacement-base=%p/%s equivalent-value=%d "
                "equivalent-base=%d has neither the original exact value "
                "type nor a pointer to that type\n",
                static_cast<void *>(ref_orig),
                static_cast<void *>(ref_orig->get_symbol()),
                ref_orig->get_symbol()->get_name().str(),
                static_cast<void *>(original_value_type),
                original_value_type != NULL
                    ? original_value_type->class_name().c_str()
                    : "<null>",
                static_cast<void *>(sym_new), sym_new->get_name().str(),
                static_cast<void *>(replacement_value_type),
                replacement_value_type != NULL
                    ? replacement_value_type->class_name().c_str()
                    : "<null>",
                static_cast<void *>(replacement_pointer != NULL
                                        ? replacement_pointer->get_base_type()
                                        : NULL),
                replacement_pointer != NULL &&
                        replacement_pointer->get_base_type() != NULL
                    ? replacement_pointer->get_base_type()->class_name().c_str()
                    : "<null>",
                original_value_type != NULL && replacement_value_type != NULL &&
                        SageInterface::isEquivalentType(original_value_type,
                                                        replacement_value_type)
                    ? 1
                    : 0,
                original_value_type != NULL && replacement_pointer != NULL &&
                        replacement_pointer->get_base_type() != NULL &&
                        SageInterface::isEquivalentType(
                            original_value_type,
                            replacement_pointer->get_base_type())
                    ? 1
                    : 0);
        ROSE_ABORT();
      }
    } // find an entry
  } // for every refs

  for (SgExpression *retired_tree : retired_original_expression_trees)
    SageInterface::deepDelete(retired_tree);
  for (SgVarRefExp *retired_reference : retired_replaced_references) {
    SageInterface::deleteAST(
        retired_reference,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
  }
  validateCheckedCasts(b, "after variable remapping");
}

/*!
 *  \brief Creates new function parameters for a set of variable symbols.
 *
 *  We have several options for the organization of function parameters:
 *
 *  1. default: each variable to be passed has a function parameter
 *           To support both C and C++ programs, this routine assumes parameters
 * passed using pointers (rather than the C++ -specific reference types). 2,
 * useParameterWrapper: use an array as the function parameter, each pointer
 * stores the address of the variable to be passed
 *  3. useStructureWrapper: use a structure, each field stores a variable's
 *              value or address according to use-by-address or not semantics
 *
 *  It inserts "unpacking/unwrapping" and "repacking" statements at the
 *  beginning and end of the function body, respectively, when necessary.
 *
 *  This routine records the mapping between the given variable symbols and the
 * new symbols corresponding to the new parameters.
 *
 *  Finally, it performs variable replacement in the end.
 *
 */
static void variableHandling(
    const ASTtools::VarSymSet_t
        &syms, // all variables passed to the outlined function: //regular
               // (shared) parameters?
    const ASTtools::VarSymSet_t
        &pdSyms, // those must use pointer dereference: use pass-by-reference
    //              const std::set<SgInitializedName*> & readOnlyVars, //
    //              optional analysis: those which can use pass-by-value, used
    //              for classic outlining without parameter wrapping, and also
    //              for variable clone to decide on if write-back is needed
    //              const std::set<SgInitializedName*> & liveOutVars, //
    //              optional analysis: used to control if a write-back is needed
    //              when variable cloning is used.
    const std::set<SgInitializedName *>
        &restoreVars, // variables to be restored after variable cloning
    SgClassDeclaration
        *struct_decl, // an optional struct wrapper for all variables
    const OutlinedFunctionParameterPlan &parameter_plan,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_template_plan,
    SgFunctionDeclaration *func) // the outlined function
{
  VarSymRemap_t sym_remap; // variable remapping for regular(shared) variables:
                           // all passed by reference using pointer types?
  VarSymRemap_t private_remap; // variable remapping for
                               // private/firstprivate/reduction variables
  ROSE_ASSERT(func);
  SgFunctionParameterList *params = func->get_parameterList();
  if (params == NULL || params != parameter_plan.definition_parameters) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[function-signature-use]: variable "
                    "handling did not receive the published parameter plan\n");
    ROSE_ABORT();
  }
  SgFunctionDefinition *def = func->get_definition();
  ROSE_ASSERT(def);
  SgBasicBlock *body = def->get_body();
  ROSE_ASSERT(body);

  // Place in which to put new outlined variable symbols.
  SgScopeStatement *args_scope = isSgScopeStatement(body);
  ROSE_ASSERT(args_scope);

  // For each variable symbol, create an equivalent function parameter.
  // Also create unpacking and repacking statements.
  int counter = 0;
  SgInitializedName *parameter1 = NULL; // the wrapper parameter
  SgVariableDeclaration *local_var_decl = NULL;

  // handle OpenMP private variables/ or those which are neither live-in or
  // live-out
  //  handlePrivateVariables(pSyms, body, private_remap);
  //  This is done before calling the outliner now, by transOmpVariables()

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing Outliner
  // variableHandling(): 1") == false);

  // Liao, 2020/9/21
  // We now decide for a function with 0 variables to pass, we still has a dummy
  // void** parameter This is necessary to have compatible runtime library 's
  // function prototype for the outlined function The runtime expects (void*)
  // (void**) function pointers. enable_classic overrules useParameterWrapper
  SgBasicBlock *func_body = func->get_definition()->get_body();
  if (!Outliner::enable_classic && Outliner::useParameterWrapper) {
    SgName var1_name = "__out_argv";
    // This is needed to support the pass-by-value semantics across different
    // thread local stacks In this situation, pointer dereferencing cannot be
    // used to get the value of an inactive parent thread's local variables
    SgType *ptype = NULL;

    // A dummy integer parameter for Fortran outlined function
    if (SageInterface::is_Fortran_language()) {
      var1_name = "out_argv";
      ptype = buildIntType();
      SgVariableDeclaration *var_decl =
          buildVariableDeclaration(var1_name, ptype, NULL, func_body);
      prependStatement(var_decl, func_body);

    } else {
      if (Outliner::useStructureWrapper) // OpenMP code triggers this branch
      {
        // To have strict type matching in C++ model
        // between the outlined function and the function pointer passed to the
        // gomp runtime lib we use void* for the parameter type
        ptype = buildPointerType(buildVoidType());
      } else // use array of pointers, regardless of the pass-by-value vs.
             // pass-by-reference difference
      { // this is to be compatible with dlopen() runtime's function pointer
        // type
        ptype = buildPointerType(buildPointerType(buildVoidType()));
      }
    }
    parameter1 = parameter_plan.wrapper_parameter;
    if (parameter1 == NULL || parameter1->get_name() != var1_name ||
        parameter1->get_type() != ptype || parameter1->get_parent() != params) {
      fprintf(stderr, "REX_OUTLINER_INVARIANT[function-signature-use]: wrapper "
                      "parameter does not match the published plan\n");
      ROSE_ABORT();
    }
  }

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing Outliner
  // variableHandling(): 2") == false);

  // --------------------------------------------------
  // for each parameters passed to the outlined function
  // They include parameters for
  // *  regular shared variables and also
  // *  shared copies for firstprivate and reduction variables
  for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin();
       i != syms.rend(); ++i) {
    // Basic information about the variable to be passed into the outlined
    // function Variable symbol name
    const SgInitializedName *i_name = (*i)->get_declaration();
    ROSE_ASSERT(i_name);
    const SgVariableSymbol *sym = isSgVariableSymbol(*i);

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
    // Outliner variableHandling(): 2.1") == false);

    // SgType* i_type = i_name->get_type ();
    //    bool readOnly = false;
    bool use_orig_type = false;
    //    if (readOnlyVars.find(const_cast<SgInitializedName*> (i_name)) !=
    //    readOnlyVars.end())
    //      readOnly = true;
    if (pdSyms.find(sym) ==
        pdSyms.end()) // not a variable to use AddressOf, then it should be a
                      // variable using its original type
      use_orig_type = true;
    // step 1. Create parameters and insert it into the parameter list of the
    // outlined function.
    // ----------------------------------------
    SgInitializedName *p_init_name = NULL;
    // Case 1: using a wrapper for all variables
    //   two choices: array of pointers (default)  vs. structure
    if (!Outliner::enable_classic &&
        Outliner::useParameterWrapper) // Liao 3/26/2013. enable_classic
                                       // overrules useParameterWrapper
                                       //   if (Outliner::useParameterWrapper)
    {
      p_init_name = parameter1; // set the source parameter to the wrapper
    } else { // case 3: use a parameter for each variable, the default case and
             // the classic case
      const auto planned_parameter = parameter_plan.direct_parameters.find(sym);
      if (planned_parameter == parameter_plan.direct_parameters.end()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-use]: captured "
                "symbol is absent from the published parameter plan\n");
        ROSE_ABORT();
      }
      p_init_name = planned_parameter->second;
      const auto planned_syntax_parameter =
          parameter_plan.direct_syntax_parameters.find(sym);
      const OutlinedFuncParam_t expected_parameter =
          createParam(i_name, use_orig_type);
      if (p_init_name == NULL || p_init_name->get_parent() != params ||
          planned_syntax_parameter ==
              parameter_plan.direct_syntax_parameters.end() ||
          planned_syntax_parameter->second == NULL ||
          planned_syntax_parameter->second->get_parent() !=
              parameter_plan.syntax_parameters ||
          p_init_name->get_name() != SgName(expected_parameter.name) ||
          p_init_name->get_type() != expected_parameter.semantic_type ||
          planned_syntax_parameter->second->get_name() !=
              SgName(expected_parameter.name) ||
          planned_syntax_parameter->second->get_type() !=
              (expected_parameter.source_type != NULL
                   ? expected_parameter.source_type
                   : expected_parameter.semantic_type)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-use]: captured "
                "symbol does not match its published exact parameter\n");
        ROSE_ABORT();
      }
    }

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
    // Outliner variableHandling(): 2.2") == false);

    // step 2. Create unpacking/unwrapping statements, also record variables to
    // be replaced
    // ----------------------------------------
    // The parameter plan, not the selected wrapper/unpack layout, defines
    // whether a capture denotes an address-backed value.  Gating this identity
    // on temp-variable or structure-wrapper modes produced pointer locals whose
    // body references were retargeted directly instead of dereferenced.
    const SgVariableSymbol *i_sym =
        isSgVariableSymbol(i_name->get_symbol_from_symbol_table());
    if (i_sym == NULL || i_sym != sym) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[pointer-deref-plan]: captured "
              "declaration=%p has no exact source symbol=%p\n",
              static_cast<const void *>(i_name),
              static_cast<const void *>(sym));
      ROSE_ABORT();
    }
    const bool isPointerDeref = pdSyms.find(i_sym) != pdSyms.end();

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
    // Outliner variableHandling(): 2.2.5") == false);

    if (Outliner::enable_classic)
    // classic methods use parameters directly, no unpacking is needed
    {
      // Parameter spelling is not declaration identity. Even a same-type,
      // same-name direct parameter must explicitly replace the captured source
      // symbol; relying on later name lookup leaves references bound to a
      // detached source declaration. Pointer parameters use the same exact map
      // and are rewritten through the typed dereference path below.
      recordSymRemap(*i, p_init_name, args_scope, sym_remap);

      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
      // Outliner variableHandling(): 2.2.6.0") == false);

    } else { // create unwrapping statements from parameters/ or the array
             // parameter for pointers
      // if (SageInterface::is_Fortran_language())
      //   args_scope = NULL; // not sure about Fortran scope

      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
      // Outliner variableHandling(): 2.2.6.1.1") == false); DQ (7/14/2021):
      // Adding test before initialization (from any previous iteration where
      // applicable). if (local_var_decl != NULL)
      //    {
      //      ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(local_var_decl,"testing
      //      Outliner variableHandling(): 2.2.6.1.1a") == false);
      //    }

      // Not true: even without parameter wrapping, we still need to transfer
      // the function parameter to a local declaration, which is also called
      // unpacking must be a case of using parameter wrapping ROSE_ASSERT
      // (Outliner::useStructureWrapper || Outliner::useParameterWrapper);
      const Outliner::OutlinedLocalTypeTemplateEntry *local_type_entry =
          findLocalTypeTemplateEntry(local_type_template_plan,
                                     i_name->get_type());
      local_var_decl = createUnpackDecl(
          p_init_name, counter, isPointerDeref, i_name, struct_decl, body,
          local_type_entry != NULL ? local_type_entry->defining_parameter_type
                                   : NULL);
      ROSE_ASSERT(local_var_decl);

      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
      // Outliner variableHandling(): 2.2.6.1.2") == false);
      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(local_var_decl,"testing
      // Outliner variableHandling(): 2.2.6.1.2a") == false);
      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(body,"testing
      // Outliner variableHandling(): 2.2.6.1.2b") == false);

      prependStatement(local_var_decl, body);

      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
      // Outliner variableHandling(): 2.2.6.1.3") == false);

      // regular and shared variables used the first local declaration
      recordSymRemap(*i, local_var_decl, args_scope, sym_remap);
      // transfer the value for firstprivate variables.
      // TODO

      // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
      // Outliner variableHandling(): 2.2.6.1.9") == false);
    }

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
    // Outliner variableHandling(): 2.3") == false);

    // step 3. Create and insert companion re-pack statement in the end of the
    // function body
    // ----------------------------------------
    SgInitializedName *local_var_init = NULL;
    if (local_var_decl != NULL) {
      SgInitializedNamePtrList &local_vars = local_var_decl->get_variables();
      ROSE_ASSERT(local_vars.size() == 1);
      local_var_init = local_vars.front();
    }

    if (!SageInterface::is_Fortran_language() && !Outliner::enable_classic)
      ROSE_ASSERT(local_var_init != NULL);

    // Only generate restoring statement for non-pointer dereferencing cases
    // if temp variable mode is enabled
    if (Outliner::temp_variable) {
      if (!isPointerDeref) {
        if (restoreVars.find(const_cast<SgInitializedName *>(i_name)) !=
            restoreVars.end()) {
          if (Outliner::enable_debug && local_var_init != NULL)
            cout << "Generating restoring statement for non-read-only variable:"
                 << local_var_init->unparseToString() << endl;

          SgExprStatement *pack_stmt = createPackStmt(local_var_init);
          if (pack_stmt)
            appendStatement(pack_stmt, body);
        } else {
          if (Outliner::enable_debug && local_var_init != NULL)
            cout << "skipping a read-only variable for restoring its value:"
                 << local_var_init->unparseToString() << endl;
        }
      } else {
        if (Outliner::enable_debug && local_var_init != NULL)
          cout << "skipping a variable using pointer-dereferencing for "
                  "restoring its value:"
               << local_var_init->unparseToString() << endl;
      }
    } else {
      // TODO: why do we have this packing statement at all if no variable
      // cloning is used??
      SgExprStatement *pack_stmt = createPackStmt(local_var_init);
      if (pack_stmt) {
        appendStatement(pack_stmt, body);
        cerr << "Error: createPackStmt() is called while "
                "Outliner::temp_variable is false!"
             << endl;
        ROSE_ABORT();
      }
    }
    counter++;

    // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
    // Outliner variableHandling(): 2.9") == false);
  } // end for

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing Outliner
  // variableHandling(): 3") == false);

  // variable substitution
  remapVarSyms(sym_remap, private_remap, func_body, local_type_template_plan);

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing Outliner
  // variableHandling(): 4") == false);
}

// same input file: two SgSourceFile nodes are created from it.
// we build symbol mapping from first to second.
// This is used to support reset symbols of AST subtree moved from first to
// second file.
class SymbolMapOfTwoFiles {
public:
  static SgSymbol *requireMappedSymbol(SgScopeStatement *sf1,
                                       SgScopeStatement *sf2,
                                       SgSymbol *source_symbol) {
    return get_inst(sf1, sf2)->mapSymbol(source_symbol);
  }

private:
  static std::string
  requireStableDeclarationKey(SgLocatedNode *declaration,
                              const std::string &declaration_name,
                              const SgName &mangled_name, const char *kind) {
    if (declaration == NULL || kind == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-key]: declaration=%p "
              "kind=%s is incomplete\n",
              static_cast<void *>(declaration), kind != NULL ? kind : "<null>");
      ROSE_ABORT();
    }

    Sg_File_Info *start = declaration->get_startOfConstruct();
    Sg_File_Info *end = declaration->get_endOfConstruct();
    const bool exactSourceOccurrence =
        start != NULL && end != NULL && start->get_parent() == declaration &&
        end->get_parent() == declaration && !start->isShared() &&
        !end->isShared() && !start->isCompilerGenerated() &&
        !end->isCompilerGenerated() && !start->isTransformation() &&
        !end->isTransformation() &&
        !start->isSourcePositionUnavailableInFrontend() &&
        !end->isSourcePositionUnavailableInFrontend() &&
        !start->get_filenameString().empty() &&
        start->get_filenameString() == end->get_filenameString() &&
        start->get_line() > 0 && start->get_col() > 0 && end->get_line() > 0 &&
        end->get_col() > 0;
    if (exactSourceOccurrence) {
      std::string sourceIdentity = start->get_filenameString();
      SgSourceFile *sourceFile =
          SageInterface::getEnclosingSourceFile(declaration);
      if (sourceFile != NULL) {
        std::string primaryInput = sourceFile->get_sourceFileNameWithPath();
        if (primaryInput.empty())
          primaryInput = sourceFile->getFileName();
        const std::string primaryOutput =
            sourceFile->get_unparse_output_filename();
        if (!primaryInput.empty() &&
            (sourceIdentity == primaryInput ||
             sourceIdentity == sourceFile->getFileName() ||
             (!primaryOutput.empty() && sourceIdentity == primaryOutput))) {
          sourceIdentity = primaryInput;
        }
      }
      std::ostringstream key;
      key << kind << ":source:" << declaration->variantT() << ':'
          << declaration_name << ':' << sourceIdentity << ':'
          << start->get_line() << ':' << start->get_col() << ':'
          << end->get_line() << ':' << end->get_col();
      return key.str();
    }

    if (mangled_name.is_null() || mangled_name.getString().empty()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-key]: %s "
              "declaration=%p has neither an exact source occurrence nor a "
              "stable semantic mangled identity\n",
              kind, static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return std::string(kind) + ":semantic:" + mangled_name.getString();
  }

  static std::string requireStableSymbolKey(SgSymbol *symbol,
                                            SgScopeStatement *scope) {
    SgFunctionSymbol *function_symbol = isSgFunctionSymbol(symbol);
    SgVariableSymbol *variable_symbol = isSgVariableSymbol(symbol);
    SgClassSymbol *class_symbol = isSgClassSymbol(symbol);
    SgLocatedNode *declaration = NULL;
    std::string declaration_name;
    SgName mangled_name;
    const char *kind = NULL;
    if (function_symbol != NULL && function_symbol->get_declaration() != NULL) {
      SgFunctionDeclaration *function = function_symbol->get_declaration();
      declaration = function;
      declaration_name = function->get_name().getString();
      mangled_name = function->get_mangled_name();
      kind = "function";
    } else if (variable_symbol != NULL &&
               variable_symbol->get_declaration() != NULL) {
      SgInitializedName *variable = variable_symbol->get_declaration();
      declaration = variable;
      declaration_name = variable->get_name().getString();
      mangled_name = variable->get_mangled_name();
      kind = "variable";
    } else if (class_symbol != NULL &&
               class_symbol->get_declaration() != NULL) {
      SgClassDeclaration *canonical = isSgClassDeclaration(
          class_symbol->get_declaration()->get_firstNondefiningDeclaration());
      if (canonical == NULL || canonical != class_symbol->get_declaration() ||
          canonical->get_symbol_from_symbol_table() != class_symbol) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-symbol-key]: class symbol=%p "
                "declaration=%p scope=%p has no exact canonical basis\n",
                static_cast<void *>(class_symbol),
                static_cast<void *>(class_symbol->get_declaration()),
                static_cast<void *>(scope));
        ROSE_ABORT();
      }
      declaration = canonical;
      declaration_name = canonical->get_name().getString();
      mangled_name = canonical->get_mangled_name();
      kind = "class";
    } else {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-key]: symbol=%p "
              "scope=%p is not an exactly declared function, variable, or "
              "class\n",
              static_cast<void *>(symbol), static_cast<void *>(scope));
      ROSE_ABORT();
    }

    return requireStableDeclarationKey(declaration, declaration_name,
                                       mangled_name, kind);
  }

  static SymbolMapOfTwoFiles *get_inst(SgScopeStatement *sf1,
                                       SgScopeStatement *sf2) {
    if (sf1 == NULL || sf2 == NULL) {
      fprintf(stderr, "REX_OUTLINER_INVARIANT[copied-symbol-map]: source or "
                      "destination scope is null\n");
      ROSE_ABORT();
    }
    const std::pair<SgScopeStatement *, SgScopeStatement *> key(sf1, sf2);
    std::map<std::pair<SgScopeStatement *, SgScopeStatement *>,
             SymbolMapOfTwoFiles *>::const_iterator found = instances.find(key);
    if (found != instances.end())
      return found->second;

    SymbolMapOfTwoFiles *created = new SymbolMapOfTwoFiles(sf1, sf2);
    if (!instances.emplace(key, created).second) {
      fprintf(stderr, "REX_OUTLINER_INVARIANT[copied-symbol-map]: source and "
                      "destination scope pair was published more than once\n");
      ROSE_ABORT();
    }
    return created;
  }

  std::map<SgSymbol *, SgSymbol *> dict; // symbol from file1 to file 2
  SgScopeStatement *destination_root;

  static std::map<std::pair<SgScopeStatement *, SgScopeStatement *>,
                  SymbolMapOfTwoFiles *>
      instances;
  static SgSymbol *requireUniqueDestinationSymbol(SgScopeStatement *root,
                                                  const std::string &key) {
    if (isSgGlobal(root) == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-map]: root=%p/%s is "
              "not one exact source-file global scope\n",
              static_cast<void *>(root),
              root != NULL ? root->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    SgSymbol *match = NULL;
    size_t match_count = 0;
    const size_t sourceMarker = key.find(":source:");
    if (sourceMarker != std::string::npos) {
      SgSymbol *surface_symbol = NULL;
      size_t surface_count = 0;
      RoseAst surface_ast(root);
      for (RoseAst::iterator node = surface_ast.begin();
           node != surface_ast.end(); ++node) {
        SgLocatedNode *candidate = NULL;
        std::string candidate_name;
        SgName candidate_mangled;
        const char *candidate_kind = NULL;
        if (SgFunctionDeclaration *function = isSgFunctionDeclaration(*node)) {
          candidate = function;
          candidate_name = function->get_name().getString();
          candidate_mangled = function->get_mangled_name();
          candidate_kind = "function";
        } else if (SgInitializedName *variable = isSgInitializedName(*node)) {
          candidate = variable;
          candidate_name = variable->get_name().getString();
          candidate_mangled = variable->get_mangled_name();
          candidate_kind = "variable";
        } else if (SgClassDeclaration *class_declaration =
                       isSgClassDeclaration(*node)) {
          candidate = class_declaration;
          candidate_name = class_declaration->get_name().getString();
          candidate_mangled = class_declaration->get_mangled_name();
          candidate_kind = "class";
        }
        if (candidate == NULL ||
            requireStableDeclarationKey(candidate, candidate_name,
                                        candidate_mangled,
                                        candidate_kind) != key) {
          continue;
        }

        SgSymbol *candidate_symbol = NULL;
        if (SgFunctionDeclaration *function =
                isSgFunctionDeclaration(candidate)) {
          candidate_symbol = function->get_symbol_from_symbol_table();
        } else if (SgInitializedName *variable =
                       isSgInitializedName(candidate)) {
          candidate_symbol = variable->get_symbol_from_symbol_table();
        } else if (SgClassDeclaration *class_declaration =
                       isSgClassDeclaration(candidate)) {
          candidate_symbol = class_declaration->get_symbol_from_symbol_table();
        }
        if (candidate_symbol == NULL) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[copied-symbol-identity]: "
                  "destination source declaration=%p key='%s' has no exact "
                  "published semantic symbol\n",
                  static_cast<void *>(candidate), key.c_str());
          ROSE_ABORT();
        }
        ++surface_count;
        if (surface_symbol == NULL)
          surface_symbol = candidate_symbol;
        else if (surface_symbol != candidate_symbol) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[copied-symbol-identity]: "
                  "destination key='%s' is published by competing symbols=%p "
                  "and %p\n",
                  key.c_str(), static_cast<void *>(surface_symbol),
                  static_cast<void *>(candidate_symbol));
          ROSE_ABORT();
        }
      }
      if (surface_count != 1 || surface_symbol == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-symbol-identity]: root=%p "
                "key='%s' has %zu exact destination source surfaces and "
                "symbol=%p\n",
                static_cast<void *>(root), key.c_str(), surface_count,
                static_cast<void *>(surface_symbol));
        ROSE_ABORT();
      }
      return surface_symbol;
    }

    RoseAst ast(root);
    for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
      SgScopeStatement *scope = isSgScopeStatement(*node);
      if (scope == NULL)
        continue;
      SgSymbolTable *symbol_table = scope->get_symbol_table();
      if (symbol_table == NULL || symbol_table->get_parent() != scope) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[symbol-table-owner]: scope=%p "
                "type=%s table=%p table-parent=%p\n",
                static_cast<void *>(scope), scope->class_name().c_str(),
                static_cast<void *>(symbol_table),
                static_cast<void *>(
                    symbol_table != NULL ? symbol_table->get_parent() : NULL));
        ROSE_ABORT();
      }

      rose_hash_multimap *entries = symbol_table->get_table();
      if (entries == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[symbol-table-owner]: scope=%p "
                "type=%s has no symbol entries\n",
                static_cast<void *>(scope), scope->class_name().c_str());
        ROSE_ABORT();
      }
      for (rose_hash_multimap::iterator entry = entries->begin();
           entry != entries->end(); ++entry) {
        SgSymbol *symbol = entry->second;
        if (isSgFunctionSymbol(symbol) == NULL &&
            isSgVariableSymbol(symbol) == NULL &&
            isSgClassSymbol(symbol) == NULL) {
          continue;
        }
        const std::string candidateKey = requireStableSymbolKey(symbol, scope);
        if (candidateKey != key)
          continue;
        ++match_count;
        if (match == NULL)
          match = symbol;
      }
    }
    // Parameters and other declaration-family surfaces can legitimately
    // repeat a mangled name in an independently parsed file. They must already
    // have been remapped by variable handling and are never looked up here.
    // For a symbol that remains tied to the source translation unit, however,
    // the complete kind+mangled identity must select exactly one destination
    // declaration. Resolve only that requested identity so unrelated repeated
    // symbols cannot either mask or reject a valid cross-file mapping.
    if (match_count != 1) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-identity]: root=%p "
              "key='%s' matches=%zu first=%p\n",
              static_cast<void *>(root), key.c_str(), match_count,
              static_cast<void *>(match));
      ROSE_ABORT();
    }
    return match;
  }

  SymbolMapOfTwoFiles(SgScopeStatement *sf1, SgScopeStatement *sf2)
      : destination_root(sf2) {
    if (sf1 == sf2) {
      fprintf(stderr, "REX_OUTLINER_INVARIANT[copied-symbol-map]: source and "
                      "destination scopes are identical\n");
      ROSE_ABORT();
    }
  }

  SgSymbol *mapSymbol(SgSymbol *source_symbol) {
    if (source_symbol == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-map]: source symbol is "
              "null\n");
      ROSE_ABORT();
    }
    const std::map<SgSymbol *, SgSymbol *>::const_iterator cached =
        dict.find(source_symbol);
    if (cached != dict.end())
      return cached->second;

    const std::string key =
        requireStableSymbolKey(source_symbol, source_symbol->get_scope());
    SgSymbol *destination_symbol =
        requireUniqueDestinationSymbol(destination_root, key);
    if ((isSgFunctionSymbol(source_symbol) != NULL) !=
            (isSgFunctionSymbol(destination_symbol) != NULL) ||
        (isSgVariableSymbol(source_symbol) != NULL) !=
            (isSgVariableSymbol(destination_symbol) != NULL) ||
        (isSgClassSymbol(source_symbol) != NULL) !=
            (isSgClassSymbol(destination_symbol) != NULL)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-kind]: source-symbol=%p "
              "destination-symbol=%p key='%s'\n",
              static_cast<void *>(source_symbol),
              static_cast<void *>(destination_symbol), key.c_str());
      ROSE_ABORT();
    }
    if (!dict.emplace(source_symbol, destination_symbol).second) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-symbol-map]: source-symbol=%p "
              "key='%s' was mapped more than once\n",
              static_cast<void *>(source_symbol), key.c_str());
      ROSE_ABORT();
    }
    return destination_symbol;
  }
}; // end SymbolMapOfTwoFiles

std::map<std::pair<SgScopeStatement *, SgScopeStatement *>,
         SymbolMapOfTwoFiles *>
    SymbolMapOfTwoFiles::instances;

class MovedBodyClassTypeEdgeCollector : public SimpleReferenceToPointerHandler {
public:
  void operator()(SgNode *&node, const SgName &, bool) override {
    if (SgClassType *class_type = isSgClassType(node))
      class_types.insert(class_type);
  }

  std::set<SgClassType *> class_types;
};

class MovedBodyTypeEdgeRewriter : public SimpleReferenceToPointerHandler {
public:
  explicit MovedBodyTypeEdgeRewriter(
      const std::map<SgNode *, SgNode *> &replacement_map,
      SgScopeStatement *target_scope)
      : replacements(replacement_map), target_scope(target_scope) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    const std::map<SgNode *, SgNode *>::const_iterator replacement =
        replacements.find(node);
    if (replacement != replacements.end()) {
      node = replacement->second;
      return;
    }

    SgType *source_type = isSgType(node);
    SgType *named_core = source_type;
    while (named_core != NULL) {
      SgType *base = NULL;
      if (SgPointerType *pointer = isSgPointerType(named_core))
        base = pointer->get_base_type();
      else if (SgReferenceType *reference = isSgReferenceType(named_core))
        base = reference->get_base_type();
      else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(named_core))
        base = reference->get_base_type();
      else if (SgModifierType *modifier = isSgModifierType(named_core))
        base = modifier->get_base_type();
      else if (SgArrayType *array = isSgArrayType(named_core))
        base = array->get_base_type();
      if (base == NULL)
        break;
      named_core = base;
    }
    if (source_type == NULL || named_core == NULL ||
        replacements.find(named_core) == replacements.end())
      return;

    SgClassType *target_named_core =
        isSgClassType(replacements.find(named_core)->second);
    SgClassDeclaration *target_declaration =
        target_named_core != NULL
            ? isSgClassDeclaration(target_named_core->get_declaration())
            : NULL;
    SgScopeStatement *exact_target_scope =
        target_declaration != NULL ? target_declaration->get_scope() : NULL;
    if (target_named_core == NULL || target_declaration == NULL ||
        exact_target_scope == NULL ||
        SageInterface::getGlobalScope(target_declaration) != target_scope) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[moved-class-wrapper-owner]: "
              "source=%p/%s core=%p target-core=%p declaration=%p scope=%p "
              "global=%p expected=%p has no exact target declaration owner\n",
              static_cast<void *>(source_type),
              source_type->class_name().c_str(),
              static_cast<void *>(named_core),
              static_cast<void *>(target_named_core),
              static_cast<void *>(target_declaration),
              static_cast<void *>(exact_target_scope),
              static_cast<void *>(
                  target_declaration != NULL
                      ? SageInterface::getGlobalScope(target_declaration)
                      : NULL),
              static_cast<void *>(target_scope));
      ROSE_ABORT();
    }

    SgType *target_type =
        SageBuilder::getTargetFileType(source_type, exact_target_scope);
    SgType *rebuilt_target_core = target_type;
    while (rebuilt_target_core != NULL) {
      SgType *base = NULL;
      if (SgPointerType *pointer = isSgPointerType(rebuilt_target_core))
        base = pointer->get_base_type();
      else if (SgReferenceType *reference =
                   isSgReferenceType(rebuilt_target_core))
        base = reference->get_base_type();
      else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(rebuilt_target_core))
        base = reference->get_base_type();
      else if (SgModifierType *modifier = isSgModifierType(rebuilt_target_core))
        base = modifier->get_base_type();
      else if (SgArrayType *array = isSgArrayType(rebuilt_target_core))
        base = array->get_base_type();
      if (base == NULL)
        break;
      rebuilt_target_core = base;
    }
    if (target_type == NULL || target_type == source_type ||
        target_type->variantT() != source_type->variantT() ||
        rebuilt_target_core != target_named_core) {
      fprintf(
          stderr,
          "REX_OUTLINER_INVARIANT[moved-class-wrapper-type]: "
          "source=%p/%s core=%p target=%p/%s target-core=%p expected-core=%p "
          "scope=%p has no exact target-file identity\n",
          static_cast<void *>(source_type), source_type->class_name().c_str(),
          static_cast<void *>(named_core), static_cast<void *>(target_type),
          target_type != NULL ? target_type->class_name().c_str() : "<null>",
          static_cast<void *>(rebuilt_target_core),
          static_cast<void *>(target_named_core),
          static_cast<void *>(exact_target_scope));
      ROSE_ABORT();
    }
    node = target_type;
  }

private:
  const std::map<SgNode *, SgNode *> &replacements;
  SgScopeStatement *target_scope;
};

class StaleMovedBodyTypeEdgeDetector : public SimpleReferenceToPointerHandler {
public:
  explicit StaleMovedBodyTypeEdgeDetector(
      const std::map<SgNode *, SgNode *> &replacement_map)
      : replacements(replacement_map) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    if (replacements.find(node) != replacements.end())
      stale_edges.insert(node);
  }

  std::set<SgNode *> stale_edges;

private:
  const std::map<SgNode *, SgNode *> &replacements;
};

void remapMovedBodyClassTypeIdentities(SgGlobal *source_global,
                                       SgGlobal *destination_global,
                                       SgBasicBlock *moved_body) {
  if (source_global == NULL || destination_global == NULL ||
      moved_body == NULL || source_global == destination_global ||
      SageInterface::getGlobalScope(moved_body) != destination_global) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[copied-class-type-plan]: source=%p "
            "destination=%p body=%p body-global=%p has no exact copied-file "
            "publication plan\n",
            static_cast<void *>(source_global),
            static_cast<void *>(destination_global),
            static_cast<void *>(moved_body),
            static_cast<void *>(moved_body != NULL
                                    ? SageInterface::getGlobalScope(moved_body)
                                    : NULL));
    ROSE_ABORT();
  }

  MovedBodyClassTypeEdgeCollector collector;
  RoseAst moved_body_ast(moved_body);
  for (RoseAst::iterator node = moved_body_ast.begin();
       node != moved_body_ast.end(); ++node)
    (*node)->processDataMemberReferenceToPointers(&collector);

  std::map<SgNode *, SgNode *> replacements;
  for (SgClassType *source_type : collector.class_types) {
    SgClassDeclaration *source_canonical =
        source_type != NULL
            ? isSgClassDeclaration(source_type->get_declaration())
            : NULL;
    if (source_canonical == NULL ||
        SageInterface::getGlobalScope(source_canonical) != source_global) {
      continue;
    }

    SgClassSymbol *source_symbol =
        isSgClassSymbol(source_canonical->get_symbol_from_symbol_table());
    SgClassSymbol *destination_symbol =
        isSgClassSymbol(SymbolMapOfTwoFiles::requireMappedSymbol(
            source_global, destination_global, source_symbol));
    SgClassDeclaration *destination_canonical =
        destination_symbol != NULL ? destination_symbol->get_declaration()
                                   : NULL;
    SgScopeStatement *destination_scope =
        destination_canonical != NULL ? destination_canonical->get_scope()
                                      : NULL;
    SgSymbolTable *destination_symbol_table =
        destination_scope != NULL ? destination_scope->get_symbol_table()
                                  : NULL;
    SgTypeTable *destination_type_table =
        destination_scope != NULL ? destination_scope->get_type_table() : NULL;
    SgClassDeclaration *destination_defining =
        destination_canonical != NULL
            ? isSgClassDeclaration(
                  destination_canonical->get_definingDeclaration())
            : NULL;
    SgClassType *destination_type =
        destination_canonical != NULL
            ? isSgClassType(destination_canonical->get_type())
            : NULL;
    if (source_canonical->get_firstNondefiningDeclaration() !=
            source_canonical ||
        source_canonical->get_type() != source_type || source_symbol == NULL ||
        source_symbol->get_declaration() != source_canonical ||
        source_symbol->get_symbol_basis() != source_canonical ||
        destination_symbol == NULL || destination_canonical == NULL ||
        destination_canonical == source_canonical ||
        destination_canonical->get_firstNondefiningDeclaration() !=
            destination_canonical ||
        destination_canonical->variantT() != source_canonical->variantT() ||
        SageInterface::getGlobalScope(destination_canonical) !=
            destination_global ||
        destination_scope == NULL ||
        destination_scope == source_canonical->get_scope() ||
        destination_symbol_table == NULL ||
        destination_symbol_table->get_parent() != destination_scope ||
        destination_symbol->get_parent() != destination_symbol_table ||
        !destination_symbol_table->exists(destination_symbol) ||
        destination_symbol->get_symbol_basis() != destination_canonical ||
        destination_type_table == NULL ||
        destination_type_table->get_parent() != destination_scope ||
        destination_type == NULL ||
        (destination_defining != NULL &&
         (destination_defining->get_firstNondefiningDeclaration() !=
              destination_canonical ||
          destination_defining->get_definingDeclaration() !=
              destination_defining ||
          destination_defining->get_scope() != destination_scope ||
          destination_defining->get_type() != destination_type))) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-class-type-family]: "
              "source-type=%p declaration=%p symbol=%p destination-type=%p "
              "declaration=%p defining=%p symbol=%p scope=%p symbol-table=%p "
              "type-table=%p has no exact copied declaration family\n",
              static_cast<void *>(source_type),
              static_cast<void *>(source_canonical),
              static_cast<void *>(source_symbol),
              static_cast<void *>(destination_type),
              static_cast<void *>(destination_canonical),
              static_cast<void *>(destination_defining),
              static_cast<void *>(destination_symbol),
              static_cast<void *>(destination_scope),
              static_cast<void *>(destination_symbol_table),
              static_cast<void *>(destination_type_table));
      ROSE_ABORT();
    }

    const bool exact_shared_project_type =
        destination_type == source_type &&
        SageInterface::isExactTagTypeIdentity(destination_type,
                                              destination_canonical);
    if (!exact_shared_project_type &&
        (destination_type == source_type ||
         destination_type->get_declaration() != destination_canonical ||
         destination_canonical->get_type() != destination_type ||
         (destination_defining != NULL &&
          destination_defining->get_type() != destination_type))) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-class-type-owner]: "
              "source-type=%p destination-type=%p declaration=%p owner=%p "
              "canonical-type=%p defining-type=%p shared-project=%d\n",
              static_cast<void *>(source_type),
              static_cast<void *>(destination_type),
              static_cast<void *>(destination_canonical),
              static_cast<void *>(destination_type->get_declaration()),
              static_cast<void *>(destination_canonical->get_type()),
              static_cast<void *>(destination_defining != NULL
                                      ? destination_defining->get_type()
                                      : NULL),
              exact_shared_project_type ? 1 : 0);
      ROSE_ABORT();
    }
    if (exact_shared_project_type)
      continue;

    const std::pair<std::map<SgNode *, SgNode *>::iterator, bool> inserted =
        replacements.emplace(source_type, destination_type);
    if (!inserted.second && inserted.first->second != destination_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[copied-class-type-map]: source=%p "
              "targets=%p/%p is ambiguous\n",
              static_cast<void *>(source_type),
              static_cast<void *>(inserted.first->second),
              static_cast<void *>(destination_type));
      ROSE_ABORT();
    }
  }

  MovedBodyTypeEdgeRewriter rewriter(replacements, destination_global);
  RoseAst rewritten_body_ast(moved_body);
  for (RoseAst::iterator node = rewritten_body_ast.begin();
       node != rewritten_body_ast.end(); ++node)
    (*node)->processDataMemberReferenceToPointers(&rewriter);

  StaleMovedBodyTypeEdgeDetector stale_detector(replacements);
  RoseAst validated_body_ast(moved_body);
  for (RoseAst::iterator node = validated_body_ast.begin();
       node != validated_body_ast.end(); ++node)
    (*node)->processDataMemberReferenceToPointers(&stale_detector);
  if (!stale_detector.stale_edges.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[moved-class-type-replacement]: "
            "body=%p retains %zu direct edges to source class types\n",
            static_cast<void *>(moved_body), stale_detector.stale_edges.size());
    ROSE_ABORT();
  }
}

// =====================================================================

// DQ (2/25/2009): Modified function interface to pass "SgBasicBlock*" as not
// const parameter.
//! Create a function named 'func_name_str', with a parameter list from 'syms'
SgFunctionDeclaration *Outliner::generateFunction(
    SgBasicBlock *s,             // block to be outlined
    const string &func_name_str, // function name provided
    const ASTtools::VarSymSet_t
        &syms, // variables to be passed in/out the outlined function
    const ASTtools::VarSymSet_t
        &pdSyms, // variables to be passed using its address. using pointer
                 // dereferencing (AddressOf() for pass-by-reference), most use
                 // for struct wrapper
                 //                          const std::set<SgInitializedName*>&
    //                          readOnlyVars, // optional readOnly variables to
    //                          guide classic outlining's parameter handling and
    //                          variable cloning's write-back generation const
    //                          std::set< SgInitializedName *>& liveOuts, //
    //                          optional live out variables, used to optimize
    //                          variable cloning
    const std::set<SgInitializedName *>
        &restoreVars, // optional information about variables to be restored
                      // after variable clones finish computation
    SgClassDeclaration
        *struct_decl, // an optional wrapper structure for parameters
    SgScopeStatement *scope,
    OutlinedLocalTypeTemplatePlan &local_type_template_plan) {
  ROSE_ASSERT(s && scope);
  ROSE_ASSERT(isValidOutliningScope(scope));

  // step 2. Create function skeleton, 'func'.
  //  -----------------------------------------

  SgName func_name(func_name_str);
  OutlinedFunctionParameterPlan parameter_plan =
      buildOutlinedFunctionParameterPlan(syms, pdSyms);
  SgFunctionParameterList *parameterList = parameter_plan.definition_parameters;

  SgFunctionDeclaration *enclosing_func = getEnclosingFunctionDeclaration(s);
  ROSE_ASSERT(enclosing_func != NULL);

  SgTemplateParameterPtrList template_params;
  collectTemplateParametersForOutlinedFunction(enclosing_func, template_params);
  collectLocalTypeTemplateParameters(syms, enclosing_func, template_params,
                                     local_type_template_plan);
  if (!local_type_template_plan.entries.empty() &&
      (Outliner::enable_classic || !Outliner::useParameterWrapper)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[local-type-call-abi]: captured "
            "function-local types require the explicit wrapper ABI so their "
            "exact types are carried only as template arguments\n");
    ROSE_ABORT();
  }
  bool is_template_instantiation =
      isSgTemplateInstantiationFunctionDecl(enclosing_func) != NULL ||
      isSgTemplateInstantiationMemberFunctionDecl(enclosing_func) != NULL;

  SgFunctionDeclaration *func = NULL;
  if (!template_params.empty() && !is_template_instantiation) {
    func = createTemplateFuncSkeleton(func_name, SgTypeVoid::createType(),
                                      parameterList, scope, template_params);
  } else {
    func = createFuncSkeleton(func_name, SgTypeVoid::createType(),
                              parameterList, scope);
  }
  ROSE_ASSERT(func);
  bindLocalTypeTemplatePlanToDefinition(func, local_type_template_plan);
  if (parameter_plan.syntax_parameters == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[function-signature-syntax]: outlined "
            "function has no exact source parameter surface\n");
    ROSE_ABORT();
  }
  if (parameter_plan.has_distinct_source_parameters) {
    if (parameter_plan.syntax_parameters == parameterList ||
        parameter_plan.syntax_parameters->get_parent() != NULL ||
        parameter_plan.syntax_parameters->get_args().size() !=
            parameterList->get_args().size() ||
        (func->get_parameterList_syntax() != NULL &&
         func->get_parameterList_syntax() != parameterList)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-syntax]: outlined "
              "function has no exact detached source parameter surface\n");
      ROSE_ABORT();
    }
    func->set_parameterList_syntax(parameter_plan.syntax_parameters);
    parameter_plan.syntax_parameters->set_parent(func);
    parameter_plan.syntax_parameters->set_scope(func->get_scope());

    SgFunctionType *semantic_type = func->get_type();
    SgPartialFunctionType *partial =
        semantic_type != NULL
            ? new SgPartialFunctionType(semantic_type->get_return_type(),
                                        semantic_type->get_has_ellipses())
            : NULL;
    SgFunctionParameterTypeList *partial_arguments =
        partial != NULL ? partial->get_argument_list() : NULL;
    if (semantic_type == NULL || partial == NULL || partial_arguments == NULL ||
        partial_arguments->get_parent() != partial) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-syntax]: outlined "
              "function cannot construct one declaration-local syntax type\n");
      ROSE_ABORT();
    }
    for (SgInitializedName *syntax_parameter :
         parameter_plan.syntax_parameters->get_args()) {
      if (syntax_parameter == NULL || syntax_parameter->get_type() == NULL ||
          syntax_parameter->get_parent() != parameter_plan.syntax_parameters) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[function-signature-syntax]: source "
                "parameter is null, untyped, or structurally detached\n");
        ROSE_ABORT();
      }
      partial_arguments->append_argument(syntax_parameter->get_type());
    }
    SgFunctionType *syntax_type = SgFunctionType::createType(partial);
    partial->set_argument_list(NULL);
    partial_arguments->set_parent(NULL);
    SageInterface::deleteAST(
        partial_arguments,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
    delete partial;
    if (syntax_type == NULL || syntax_type == semantic_type ||
        syntax_type->get_argument_list() == NULL ||
        syntax_type->get_argument_list()->get_arguments().size() !=
            parameter_plan.syntax_parameters->get_args().size()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-syntax]: source "
              "parameter surface did not produce one distinct syntax type\n");
      ROSE_ABORT();
    }
    syntax_type->set_parent(func);
    func->set_type_syntax(syntax_type);
    func->set_type_syntax_is_available(true);
    if (func->get_parameterList_syntax() != parameter_plan.syntax_parameters ||
        parameter_plan.syntax_parameters->get_parent() != func ||
        parameter_plan.syntax_parameters->get_scope() != func->get_scope() ||
        func->get_type_syntax() != syntax_type ||
        !func->get_type_syntax_is_available() ||
        syntax_type->get_parent() != func) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[function-signature-syntax]: outlined "
              "function rejected its exact source signature surface\n");
      ROSE_ABORT();
    }
  } else if (parameter_plan.syntax_parameters != parameterList) {
    ROSE_ABORT();
  }
  validateOutlinedFunctionSignature(func, parameterList);

  // Inherit enclosing function's inline property: avoid linking error when
  // linking multiple .lib files with the outlined functions
  if (enclosing_func->get_functionModifier().isInline()) {
    func->get_functionModifier().setInline();
  }

  // Liao, 4/15/2009 , enforce C-bindings  for C++ outlined code
  // enable C code to call this outlined function
  // Only apply to C++ , pure C has trouble in recognizing extern "C"
  // Another way is to attach the function with preprocessing info:
  // #if __cplusplus
  // extern "C"
  // #endif
  // We don't choose it since the language linkage information is not explicit
  // in AST if (!SageInterface::is_Fortran_language())
  bool is_template_func = (isSgTemplateFunctionDeclaration(func) != NULL ||
                           isSgTemplateMemberFunctionDeclaration(func) != NULL);
  if (!is_template_func &&
      (SageInterface::is_Cxx_language() || is_mixed_C_and_Cxx_language() ||
       is_mixed_Fortran_and_Cxx_language() ||
       is_mixed_Fortran_and_C_and_Cxx_language())) {
    // Make function 'extern "C"'
    func->get_declarationModifier().get_storageModifier().setExtern();
    func->set_linkage("C");
  }

  // step 3. Create the function body
  //  -----------------------------------------
  //  Generate the function body by deep-copying 's'.
  SgBasicBlock *func_body = func->get_definition()->get_body();
  ROSE_ASSERT(func_body != NULL);

  // This does a copy of the statements in "s" to the function body of the
  // outlined function.
  ROSE_ASSERT(func_body->get_statements().empty() == true);
  const SgStatementPtrList moved_statements = s->get_statements();
  AttachedPreprocessingInfoType moved_inside_preprocessing;
  SageInterface::cutPreprocessingInfo(s, PreprocessingInfo::inside,
                                      moved_inside_preprocessing);
  SageInterface::moveStatementsBetweenBlocks(s, func_body);

  Sg_File_Info *source_owner_info = s->get_file_info();
  Sg_File_Info *target_owner_info = func_body->get_file_info();
  const bool source_semantic_transfer =
      SageInterface::hasSemanticOnlyFrontendSourcePosition(s);
  const bool source_pending_transformation =
      SageInterface::hasDetachedTransformationSourcePosition(s);
  const bool target_semantic_owner =
      SageInterface::hasSemanticOnlyFrontendSourcePosition(func_body);
  const bool exact_semantic_transfer =
      (source_semantic_transfer || source_pending_transformation) &&
      (target_semantic_owner ||
       (target_owner_info != NULL && !target_owner_info->isShared() &&
        target_owner_info->get_physical_file_id() >= 0));
  if (source_owner_info == NULL || target_owner_info == NULL ||
      source_owner_info->isShared() || target_owner_info->isShared() ||
      (!exact_semantic_transfer &&
       (source_owner_info->get_physical_file_id() < 0 ||
        target_owner_info->get_physical_file_id() < 0))) {
    fprintf(
        stderr,
        "REX_OUTLINER_INVARIANT[moved-body-owner]: source=%p target=%p "
        "does not identify two exact physical owners or one exact "
        "semantic-only transfer (source-info=%p bits=%u file=%d "
        "physical=%d semantic=%d target-info=%p bits=%u file=%d "
        "physical=%d semantic=%d)\n",
        static_cast<void *>(s), static_cast<void *>(func_body),
        static_cast<void *>(source_owner_info),
        source_owner_info != NULL
            ? source_owner_info->get_classificationBitField()
            : 0,
        source_owner_info != NULL ? source_owner_info->get_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
        source_owner_info != NULL ? source_owner_info->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
        SageInterface::hasSemanticOnlyFrontendSourcePosition(s),
        static_cast<void *>(target_owner_info),
        target_owner_info != NULL
            ? target_owner_info->get_classificationBitField()
            : 0,
        target_owner_info != NULL ? target_owner_info->get_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
        target_owner_info != NULL ? target_owner_info->get_physical_file_id()
                                  : Sg_File_Info::BAD_FILE_ID,
        SageInterface::hasSemanticOnlyFrontendSourcePosition(func_body));
    ROSE_ABORT();
  }
  const bool crosses_physical_output_boundary =
      !source_semantic_transfer && !source_pending_transformation &&
      source_owner_info->get_physical_file_id() !=
          target_owner_info->get_physical_file_id();
  const bool publishes_pending_transformation =
      source_pending_transformation && !target_semantic_owner;
  if (crosses_physical_output_boundary || publishes_pending_transformation) {
    for (SgStatement *moved_statement : moved_statements) {
      if (moved_statement == NULL ||
          moved_statement->get_parent() != func_body) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[moved-body-owner]: moved statement=%p "
                "has no exact destination body owner\n",
                static_cast<void *>(moved_statement));
        ROSE_ABORT();
      }
      if (publishes_pending_transformation) {
        SageInterface::publishGeneratedSubtreeOutputOwner(moved_statement,
                                                          func_body);
      } else {
        SageInterface::relocateGeneratedSubtreePhysicalOutputOwner(
            moved_statement, s, func_body);
      }
    }
    for (PreprocessingInfo *record : moved_inside_preprocessing) {
      if (record == NULL || !record->has_file_info()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[moved-body-preprocessing-owner]: "
                "detached transfer contains incomplete preprocessing\n");
        ROSE_ABORT();
      }
      Sg_File_Info *record_info = record->get_file_info();
      if (!publishes_pending_transformation &&
          (record->isTransformation() || record_info->isTransformation() ||
           record_info->isCompilerGenerated())) {
        SageInterface::relocateAttachedPreprocessingInfoPhysicalOutputOwner(
            record, s, func_body);
      }
    }
  }
  SageInterface::pastePreprocessingInfo(func_body, PreprocessingInfo::inside,
                                        moved_inside_preprocessing);
  preserveMovedNamespaceFunctionBindings(func_body);

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 2") == false);

  SgGlobal *copied_source_global = NULL;
  SgGlobal *copied_destination_global = NULL;
  if (Outliner::useNewFile) {
    SgSourceFile *target_source_file = saved_source_file_for_dynamic_library;
    if (target_source_file == NULL) {
      target_source_file = SageInterface::getEnclosingSourceFile(scope, true);
    }
    if (target_source_file == NULL) {
      target_source_file =
          SageInterface::getEnclosingSourceFile(func_body, true);
    }
    ROSE_ASSERT(target_source_file != NULL);
    string output_filename = target_source_file->get_unparse_output_filename();

    Sg_File_Info::addFilenameToMap(output_filename);
    int source_file_physical_file_id =
        Sg_File_Info::getIDFromFilename(output_filename);
    ROSE_ASSERT(source_file_physical_file_id >= 0);
    if (Outliner::copy_origFile) {
      copied_destination_global = isSgGlobal(scope);
      copied_source_global =
          const_cast<SgGlobal *>(SageInterface::getGlobalScope(s));
      if (copied_destination_global == NULL || copied_source_global == NULL ||
          copied_destination_global == copied_source_global) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-symbol-remap]: copied output "
                "does not publish distinct source and destination globals\n");
        ROSE_ABORT();
      }
    }
  }

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 5") == false);

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 6") == false);

  // step 4: variable handling, including:
  //  -----------------------------------------
  //    consume the parameters finalized before declaration construction
  //    add statements to unwrap the parameters if necessary
  //    add repacking statements if necessary
  //    replace variables to access to parameters, directly or indirectly
  // variableHandling(syms, pdSyms, readOnlyVars, liveOuts, struct_decl, func);
  variableHandling(syms, pdSyms, restoreVars, struct_decl, parameter_plan,
                   local_type_template_plan, func);
  ROSE_ASSERT(func != NULL);
  validateOutlinedFunctionSignature(func, parameterList);

  if (SageInterface::is_Fortran_language()) {
    publishMovedFortranProcedureDependencies(func_body);
    for (SgNode *node :
         NodeQuery::querySubTree(func_body, V_SgFunctionRefExp)) {
      SgFunctionRefExp *reference = isSgFunctionRefExp(node);
      SgFunctionSymbol *semantic =
          reference != NULL ? reference->get_symbol() : NULL;
      SgFunctionSymbol *moved_semantic = semantic;
      SgFunctionDeclaration *moved_declaration =
          semantic != NULL ? semantic->get_declaration() : NULL;
      SgFunctionDeclaration *canonical_declaration =
          moved_declaration != NULL
              ? isSgFunctionDeclaration(
                    moved_declaration->get_firstNondefiningDeclaration())
              : NULL;
      SgScopeStatement *canonical_scope =
          canonical_declaration != NULL ? canonical_declaration->get_scope()
                                        : NULL;
      SgSymbolTable *canonical_table =
          canonical_scope != NULL ? canonical_scope->get_symbol_table() : NULL;
      SgFunctionSymbol *canonical_symbol =
          canonical_scope != NULL && canonical_declaration != NULL
              ? isSgFunctionSymbol(
                    canonical_scope->find_symbol_from_declaration(
                        canonical_declaration))
              : NULL;
      if (reference == NULL || moved_semantic == NULL ||
          moved_declaration == NULL || canonical_declaration == NULL ||
          canonical_scope == NULL || canonical_table == NULL ||
          canonical_symbol == NULL ||
          canonical_symbol->get_declaration() != canonical_declaration ||
          canonical_declaration->get_scope() != canonical_scope ||
          canonical_symbol->get_parent() != canonical_table ||
          !canonical_table->exists(canonical_symbol)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[moved-function-semantic-binding]: "
                "reference=%p moved-symbol=%p declaration=%p canonical=%p "
                "canonical-scope=%p table=%p canonical-symbol=%p has no "
                "exact declaration-owned target after the move\n",
                static_cast<void *>(reference),
                static_cast<void *>(moved_semantic),
                static_cast<void *>(moved_declaration),
                static_cast<void *>(canonical_declaration),
                static_cast<void *>(canonical_scope),
                static_cast<void *>(canonical_table),
                static_cast<void *>(canonical_symbol));
        ROSE_ABORT();
      }
      if (moved_semantic != canonical_symbol) {
        reference->set_symbol(canonical_symbol);
        semantic = canonical_symbol;
      }
      SgFunctionType *type =
          semantic != NULL ? isSgFunctionType(semantic->get_type()) : NULL;
      SgScopeStatement *use_scope =
          reference != NULL ? SageInterface::getEnclosingScope(reference)
                            : NULL;
      SgFunctionSymbol *exact_type_visible =
          type != NULL && use_scope != NULL
              ? SageInterface::lookupFunctionSymbolInParentScopes(
                    semantic->get_name(), type, use_scope)
              : NULL;
      SgFunctionSymbol *source_visible =
          reference != NULL ? reference->get_fortran_source_visible_symbol()
                            : NULL;
      if (source_visible == moved_semantic) {
        source_visible = canonical_symbol;
      }
      const auto old_binding_kind =
          reference != NULL
              ? reference->get_fortran_source_visible_binding_kind()
              : SgFunctionRefExp::
                    e_fortran_source_visible_binding_not_applicable;
      SgScopeStatement *source_scope =
          source_visible != NULL ? source_visible->get_scope() : NULL;
      SgSymbolTable *source_table =
          source_scope != NULL ? source_scope->get_symbol_table() : NULL;
      SgStatement *use_statement =
          reference != NULL ? SageInterface::getEnclosingStatement(reference)
                            : NULL;
      const bool exact_intrinsic_shadow =
          old_binding_kind ==
              SgFunctionRefExp::
                  e_fortran_source_visible_binding_intrinsic_shadow &&
          semantic != NULL && source_visible != NULL &&
          source_visible != semantic &&
          source_visible->get_name() == semantic->get_name() &&
          source_scope != NULL && source_table != NULL &&
          source_visible->get_parent() == source_table &&
          source_table->exists(source_visible) && use_statement != NULL &&
          (source_scope == use_scope ||
           SageInterface::isAncestor(source_scope, use_statement));
      const bool semantic_publication =
          isExactFortranSemanticProcedurePublication(semantic);

      auto new_binding_kind =
          SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable;
      if (exact_intrinsic_shadow) {
        new_binding_kind =
            SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow;
      } else if (exact_type_visible != NULL) {
        source_visible = exact_type_visible;
        new_binding_kind =
            SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed;
      } else if (semantic_publication) {
        source_visible = semantic;
        new_binding_kind = SgFunctionRefExp::
            e_fortran_source_visible_binding_semantic_publication;
      }
      SgFunctionDeclaration *source_declaration =
          source_visible != NULL ? source_visible->get_declaration() : NULL;
      source_scope =
          source_visible != NULL ? source_visible->get_scope() : NULL;
      source_table =
          source_scope != NULL ? source_scope->get_symbol_table() : NULL;
      if (reference == NULL || semantic == NULL || type == NULL ||
          use_scope == NULL || source_visible == NULL ||
          source_declaration == NULL ||
          new_binding_kind ==
              SgFunctionRefExp::
                  e_fortran_source_visible_binding_not_applicable ||
          source_visible->get_name() != semantic->get_name() ||
          source_table == NULL ||
          source_visible->get_parent() != source_table ||
          !source_table->exists(source_visible)) {
        SgProcedureHeaderStatement *source_procedure =
            isSgProcedureHeaderStatement(source_declaration);
        SgAuxiliaryDeclarationList *source_owner =
            source_declaration != NULL
                ? isSgAuxiliaryDeclarationList(source_declaration->get_parent())
                : NULL;
        Sg_File_Info *source_info = source_declaration != NULL
                                        ? source_declaration->get_file_info()
                                        : NULL;
        const SgDeclarationStatementPtrList *source_declarations =
            source_owner != NULL ? &source_owner->get_declarations() : NULL;
        fprintf(
            stderr,
            "REX_OUTLINER_INVARIANT[moved-function-source-binding]: "
            "reference=%p semantic=%p type=%p use-scope=%p "
            "source-visible=%p declaration=%p source-scope=%p "
            "source-table=%p old-kind=%d exact-type-visible=%p "
            "semantic-publication=%d declaration-kind=%s "
            "declaration-parent=%p/%s source-form=%d has no exact "
            "post-move Fortran source binding; owner-parent=%p "
            "scope-auxiliary=%p declaration-scope=%p canonical=%p "
            "compiler-generated=%d output=%d owner-count=%zu "
            "symbol-parent=%p table-exists=%d declaration-symbol=%p\n",
            static_cast<void *>(reference), static_cast<void *>(semantic),
            static_cast<void *>(type), static_cast<void *>(use_scope),
            static_cast<void *>(source_visible),
            static_cast<void *>(source_declaration),
            static_cast<void *>(source_scope),
            static_cast<void *>(source_table),
            static_cast<int>(old_binding_kind),
            static_cast<void *>(exact_type_visible),
            semantic_publication ? 1 : 0,
            source_declaration != NULL
                ? source_declaration->class_name().c_str()
                : "<null>",
            static_cast<void *>(source_declaration != NULL
                                    ? source_declaration->get_parent()
                                    : NULL),
            source_declaration != NULL &&
                    source_declaration->get_parent() != NULL
                ? source_declaration->get_parent()->class_name().c_str()
                : "<null>",
            source_declaration != NULL &&
                    isSgProcedureHeaderStatement(source_declaration) != NULL
                ? static_cast<int>(
                      isSgProcedureHeaderStatement(source_declaration)
                          ->get_fortran_procedure_source_form())
                : -1,
            static_cast<void *>(
                source_owner != NULL ? source_owner->get_parent() : NULL),
            static_cast<void *>(source_scope != NULL
                                    ? source_scope->get_auxiliary_declarations()
                                    : NULL),
            static_cast<void *>(source_declaration != NULL
                                    ? source_declaration->get_scope()
                                    : NULL),
            static_cast<void *>(
                source_declaration != NULL
                    ? source_declaration->get_firstNondefiningDeclaration()
                    : NULL),
            source_info != NULL && source_info->isCompilerGenerated() ? 1 : 0,
            source_info != NULL && source_info->isOutputInCodeGeneration() ? 1
                                                                           : 0,
            source_declarations != NULL && source_declaration != NULL
                ? static_cast<size_t>(std::count(source_declarations->begin(),
                                                 source_declarations->end(),
                                                 source_declaration))
                : 0,
            static_cast<void *>(semantic != NULL ? semantic->get_parent()
                                                 : NULL),
            source_table != NULL && semantic != NULL &&
                    source_table->exists(semantic)
                ? 1
                : 0,
            static_cast<void *>(
                source_scope != NULL && source_declaration != NULL
                    ? source_scope->find_symbol_from_declaration(
                          source_declaration)
                    : NULL));
        ROSE_ABORT();
      }
      reference->set_fortran_source_visible_symbol(source_visible);
      reference->set_fortran_source_visible_binding_kind(new_binding_kind);
    }
  }

  if (copied_source_global != NULL) {
    remapMovedBodyClassTypeIdentities(copied_source_global,
                                      copied_destination_global, func_body);

    // Captured locals and parameters must first be rewritten to the outlined
    // function's own declarations.  Only then can references that still point
    // into the original translation unit be mapped to the independently
    // reparsed destination declarations.
    RoseAst ast(func_body);
    for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
      SgFunctionRefExp *function_reference = isSgFunctionRefExp(*i);
      SgVarRefExp *variable_reference = isSgVarRefExp(*i);
      if (function_reference == NULL && variable_reference == NULL)
        continue;

      SgSymbol *source_symbol =
          function_reference != NULL
              ? isSgSymbol(function_reference->get_symbol())
              : isSgSymbol(variable_reference->get_symbol());
      if (source_symbol == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-symbol-remap]: reference=%p "
                "has no source symbol\n",
                static_cast<void *>(
                    function_reference != NULL
                        ? static_cast<SgExpression *>(function_reference)
                        : static_cast<SgExpression *>(variable_reference)));
        ROSE_ABORT();
      }
      if (getGlobalScope(source_symbol) != copied_source_global)
        continue;

      SgSymbol *destination_symbol = SymbolMapOfTwoFiles::requireMappedSymbol(
          copied_source_global, copied_destination_global, source_symbol);
      SgFunctionSymbol *destination_function =
          isSgFunctionSymbol(destination_symbol);
      SgVariableSymbol *destination_variable =
          isSgVariableSymbol(destination_symbol);
      if ((function_reference != NULL && destination_function == NULL) ||
          (variable_reference != NULL && destination_variable == NULL)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-symbol-remap]: "
                "source-symbol=%p has no exact destination symbol of the "
                "same kind\n",
                static_cast<void *>(source_symbol));
        ROSE_ABORT();
      }
      if (function_reference != NULL) {
        SgCastExp *function_decay =
            isSgCastExp(function_reference->get_parent());
        SgFunctionType *source_function_type = isSgFunctionType(
            function_reference->get_type()->stripTypedefsAndModifiers());
        SgPointerType *source_decay_type =
            function_decay != NULL &&
                    function_decay->get_semantic_conversion_kind() ==
                        SgCastExp::e_semantic_conversion_FunctionToPointerDecay
                ? isSgPointerType(function_decay->get_type())
                : NULL;
        if (function_decay != NULL &&
            function_decay->get_semantic_conversion_kind() ==
                SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
          if (function_decay->get_operand() != function_reference ||
              source_function_type == NULL || source_decay_type == NULL ||
              source_decay_type->get_base_type() !=
                  function_reference->get_type()) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[copied-function-decay]: "
                    "reference=%p source-type=%p cast=%p result=%p has no "
                    "exact pre-remap function conversion\n",
                    static_cast<void *>(function_reference),
                    static_cast<void *>(function_reference->get_type()),
                    static_cast<void *>(function_decay),
                    static_cast<void *>(source_decay_type));
            ROSE_ABORT();
          }
        }
        function_reference->set_symbol(destination_function);
        if (SageInterface::is_Fortran_language()) {
          const bool semantic_publication =
              isExactFortranSemanticProcedurePublication(destination_function);
          function_reference->set_fortran_source_visible_symbol(
              destination_function);
          function_reference->set_fortran_source_visible_binding_kind(
              semantic_publication
                  ? SgFunctionRefExp::
                        e_fortran_source_visible_binding_semantic_publication
                  : SgFunctionRefExp::
                        e_fortran_source_visible_binding_exact_typed);
        }
        if (function_reference->get_symbol() != destination_function ||
            function_reference->get_type() !=
                destination_function->get_type() ||
            (SageInterface::is_Fortran_language() &&
             function_reference->get_fortran_source_visible_symbol() !=
                 destination_function) ||
            (SageInterface::is_Fortran_language() &&
             (isExactFortranSemanticProcedurePublication(
                  destination_function) !=
              (function_reference->get_fortran_source_visible_binding_kind() ==
               SgFunctionRefExp::
                   e_fortran_source_visible_binding_semantic_publication)))) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[copied-function-symbol]: "
                  "reference=%p symbol=%p type=%p expected-symbol=%p "
                  "expected-type=%p rejected its exact target binding\n",
                  static_cast<void *>(function_reference),
                  static_cast<void *>(function_reference->get_symbol()),
                  static_cast<void *>(function_reference->get_type()),
                  static_cast<void *>(destination_function),
                  static_cast<void *>(destination_function->get_type()));
          ROSE_ABORT();
        }
        if (source_decay_type != NULL) {
          SgPointerType *destination_decay_type =
              SageBuilder::buildPointerType(function_reference->get_type());
          if (destination_decay_type == NULL ||
              destination_decay_type->get_base_type() !=
                  function_reference->get_type()) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[copied-function-decay]: "
                    "reference=%p target-type=%p did not produce one exact "
                    "pointer result\n",
                    static_cast<void *>(function_reference),
                    static_cast<void *>(function_reference->get_type()));
            ROSE_ABORT();
          }
          function_decay->set_explicitly_stored_type(destination_decay_type);
          function_decay->validate_semantic_conversion();
        }
      } else
        variable_reference->set_symbol(destination_variable);
    }
  }

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 7") == false);

  // Retest this...
  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  // printf ("After resetting the parent: func->get_definition() = %p
  // func->get_definition()->get_body()->get_parent() = %p
  // \n",func->get_definition(),func->get_definition()->get_body()->get_parent());
  //
  assertFunctionSymbolPresent(scope, func);

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 8") == false);

  addMissingLocalTypedefAliases(func);

  // ROSE_ASSERT(findFirstSgCastExpMarkedAsTransformation(func,"testing
  // Outliner::generateFunction(): 10") == false);

  // Outlined Fortran procedures must keep case-insensitive scope semantics.
  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = func->get_definition();
    ROSE_ASSERT(func_def != NULL);
    func_def->setCaseInsensitive(true);
    SgBasicBlock *func_body = func_def->get_body();
    ROSE_ASSERT(func_body != NULL);
    func_body->setCaseInsensitive(true);
  }

  return func;
}

// eof
