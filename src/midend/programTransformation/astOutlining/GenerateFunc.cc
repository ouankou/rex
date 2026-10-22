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

#include "FortranProcedureBinding.h"
#include "sage3basic.h"

#include "rose_config.h"

#include "fixupCopy.h"
#include "sageBuilder.h"

#include <iostream>

#include <functional>

#include <map>

#include <memory>

#include <set>

#include <string>

#include <unordered_map>

#include <vector>

#include "ASTtools.hh"

#include "Copy.hh"

#include "Outliner.hh"

#include "RoseAst.h"

#include "VarSym.hh"

//! Stores a variable symbol remapping.
typedef std::map<const SgVariableSymbol *, SgVariableSymbol *> VarSymRemap_t;

extern std::map<SgOmpExecStatement *,
                std::map<SgInitializedName *, SgExpression *> *>
    clause_variable_renaming_record;

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;

namespace {

using SageInterface::Detail::isExactFortranSemanticProcedurePublication;

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
  }
  return SageBuilder::buildSemanticFunctionParameterList(definition_parameters);
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

  SgScopeStatement *scope = canonical->get_scope();
  ROSE_ASSERT(scope != nullptr && definition->get_scope() == scope);
  SgSymbolTable *table = scope->get_symbol_table();
  ROSE_ASSERT(table != nullptr && table->get_parent() == scope);
  SgSymbol *symbol = scope->find_symbol_from_declaration(canonical);
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->variantT() ==
              (isSgTemplateFunctionDeclaration(canonical) != nullptr
                   ? V_SgTemplateFunctionSymbol
                   : V_SgFunctionSymbol));
  ROSE_ASSERT(symbol->get_symbol_basis() == canonical &&
              symbol->get_type() == canonical_type &&
              symbol->get_parent() == table && table->exists(symbol));

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
        SageBuilder::functionParameterTypeFromDeclaredType(
            definition_argument->get_type()) !=
            function_argument_types[index]) {
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
                   const OutlinedFunctionParameterPlan &plan,
                   SgScopeStatement *scope) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(Outliner::isValidOutliningScope(scope));
  SgFunctionParameterList *params = plan.definition_parameters;
  ROSE_ASSERT(plan.syntax_parameters != nullptr);
  auto ownership =
      SageBuilder::function_declaration_ownership::sourceLexicalIn(scope);
  if (plan.has_distinct_source_parameters)
    ownership = ownership.withSourceParameterList(plan.syntax_parameters);
  else
    ROSE_ASSERT(plan.syntax_parameters == params);
  SgFunctionDeclaration *func;
  SgProcedureHeaderStatement *fortranRoutine;
  if (SageInterface::is_Fortran_language()) {
    SgFunctionParameterList *canonical_params =
        cloneCanonicalParameterList(params);
    fortranRoutine = SageBuilder::buildProcedureHeaderStatement(
        ownership, SgName(name), ret_type, params, canonical_params,
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
        ownership, name, ret_type, params, scope,
        /*buildTemplateInstantiation=*/false, canonical,
        /*templateArgumentsList=*/NULL,
        /*forceFreeFunctionScope=*/false);
  }

  validateOutlinedFunctionSignature(func, params);

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

static void publishFortranProcedureDependencies(SgBasicBlock *source_body,
                                                SgBasicBlock *func_body) {
  ROSE_ASSERT(source_body != NULL);
  ROSE_ASSERT(func_body != NULL);

  const Rose_STL_Container<SgNode *> reference_nodes =
      NodeQuery::querySubTree(source_body, V_SgFunctionRefExp);
  std::map<SgFunctionSymbol *, SgRenameSymbol *> generic_bindings;
  std::map<SgUseStatement *, SgUseStatement *> use_dependencies;
  std::map<SgFunctionDeclaration *, SgFunctionSymbol *> interface_dependencies;
  std::map<SgVariableSymbol *, SgVariableSymbol *> constant_dependencies;
  std::map<SgVariableSymbol *, SgExpression *> parameter_values;
  std::set<SgScopeStatement *> parameter_scopes;
  std::set<SgVariableSymbol *> active_constants;
  std::function<SgVariableSymbol *(SgVariableSymbol *)> copy_constant =
      [&](SgVariableSymbol *source) -> SgVariableSymbol * {
    ASSERT_not_null(source);
    auto existing = constant_dependencies.find(source);
    if (existing != constant_dependencies.end())
      return existing->second;
    ROSE_ASSERT(active_constants.insert(source).second);
    ROSE_ASSERT(SageInterface::isConstType(source->get_type()));
    SgInitializedName *name = source->get_declaration();
    ASSERT_not_null(name);
    ASSERT_not_null(name->get_scope());
    SgInitializer *initializer;
    if (name->get_initializer() != nullptr) {
      initializer = SageInterface::deepCopy(name->get_initializer());
    } else {
      if (parameter_scopes.insert(name->get_scope()).second) {
        for (SgNode *node : NodeQuery::querySubTree(
                 name->get_scope(), V_SgAttributeSpecificationStatement)) {
          auto *statement = isSgAttributeSpecificationStatement(node);
          if (statement->get_attribute_kind() !=
              SgAttributeSpecificationStatement::e_parameterStatement)
            continue;
          ASSERT_not_null(statement->get_parameter_list());
          for (SgExpression *expression :
               statement->get_parameter_list()->get_expressions()) {
            auto *assignment = isSgAssignOp(expression);
            ASSERT_not_null(assignment);
            auto *target = isSgVarRefExp(assignment->get_lhs_operand());
            ASSERT_not_null(target);
            ASSERT_not_null(target->get_symbol());
            ROSE_ASSERT(parameter_values
                            .emplace(target->get_symbol(),
                                     assignment->get_rhs_operand())
                            .second);
          }
        }
      }
      auto value = parameter_values.find(source);
      ROSE_ASSERT(value != parameter_values.end());
      initializer = SageBuilder::buildAssignInitializer(
          SageInterface::deepCopy(value->second), source->get_type());
    }
    ASSERT_not_null(initializer);
    for (SgNode *node : NodeQuery::querySubTree(initializer, V_SgVarRefExp)) {
      auto *reference = isSgVarRefExp(node);
      reference->set_symbol(copy_constant(reference->get_symbol()));
    }
    auto *variable = SageBuilder::buildVariableDeclaration(
        name->get_name(), source->get_type(), initializer, func_body);
    variable->get_declarationModifier()
        .get_typeModifier()
        .get_constVolatileModifier()
        .setConst();
    appendStatement(variable, func_body);
    auto *symbol = isSgVariableSymbol(
        variable->get_variables().front()->get_symbol_from_symbol_table());
    ASSERT_not_null(symbol);
    ROSE_ASSERT(constant_dependencies.emplace(source, symbol).second);
    active_constants.erase(source);
    return symbol;
  };
  for (SgNode *node : reference_nodes) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    SgFunctionSymbol *source_semantic =
        reference != NULL ? reference->get_symbol() : NULL;
    SgFunctionDeclaration *source_declaration =
        source_semantic != NULL ? source_semantic->get_declaration() : NULL;
    SgInterfaceBody *interface_body =
        source_declaration != NULL
            ? isSgInterfaceBody(source_declaration->get_parent())
            : NULL;
    SgInterfaceStatement *source_interface =
        interface_body != NULL
            ? isSgInterfaceStatement(interface_body->get_parent())
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

    SgFunctionSymbol *source_visible =
        reference->get_fortran_source_visible_symbol();
    ASSERT_not_null(source_visible);
    ASSERT_not_null(source_visible->get_scope());
    if (SageInterface::isAncestor(source_visible->get_scope(), func_body)) {
      continue;
    }
    SgRenameSymbol *source_rename =
        isSgRenameSymbol(reference->get_fortran_source_visible_symbol());
    if (source_rename != nullptr) {
      ROSE_ASSERT(source_rename->get_original_symbol() == source_semantic);
      SgNodePtrList causes;
      for (SgNode *cause : source_rename->get_causal_nodes()) {
        if (SgUseStatement *source_use = isSgUseStatement(cause)) {
          auto inserted = use_dependencies.emplace(source_use, nullptr);
          if (inserted.second) {
            ASSERT_not_null(source_use->get_module());
            SgUseStatement *use = new SgUseStatement(
                source_use->get_name(), source_use->get_only_option(),
                source_use->get_module_nature(), source_use->get_module());
            setOneSourcePositionForTransformation(use);
            use->set_firstNondefiningDeclaration(use);
            use->set_definingDeclaration(use);
            for (SgRenamePair *pair : source_use->get_rename_list()) {
              ASSERT_not_null(pair);
              SgRenamePair *copy = new SgRenamePair(pair->get_local_name(),
                                                    pair->get_use_name());
              setOneSourcePositionForTransformation(copy);
              copy->set_parent(use);
              use->get_rename_list().push_back(copy);
            }
            prependStatement(use, func_body);
            inserted.first->second = use;
          }
          causes.push_back(inserted.first->second);
        }
      }
      if (!causes.empty()) {
        auto inserted = generic_bindings.emplace(source_rename, nullptr);
        if (inserted.second) {
          auto *binding = new SgRenameSymbol(
              source_declaration, source_semantic, source_rename->get_name());
          binding->get_causal_nodes().swap(causes);
          func_body->insert_symbol(binding->get_name(), binding);
          inserted.first->second = binding;
        }
        auto *replacement = SageBuilder::buildFortranFunctionRefExp(
            source_semantic, inserted.first->second,
            reference->get_fortran_source_visible_binding_kind());
        SageInterface::replaceExpression(reference, replacement);
        continue;
      }
    }
    if (source_interface == NULL || source_interface->get_scope() == NULL ||
        source_declaration->get_scope() != source_interface->get_scope() ||
        interface_body->get_functionDeclaration() != source_declaration ||
        interface_body->get_use_function_name()) {
      continue;
    }
    auto dependency =
        interface_dependencies.emplace(source_declaration, nullptr);
    if (dependency.second) {
      SgCopyHelp::copiedNodeMapType identities;
      auto *declaration = isSgProcedureHeaderStatement(
          SageInterface::deepCopyNodeWithIdentityMap(source_declaration,
                                                     identities));
      ASSERT_not_null(declaration);
      ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() ==
                  declaration);
      std::set<SgNode *> copied_nodes;
      std::set<SgVarRefExp *> copied_references;
      for (const auto &identity : identities) {
        if (identity.first == identity.second)
          continue;
        copied_nodes.insert(identity.second);
        if (auto *reference = isSgVarRefExp(identity.second))
          copied_references.insert(reference);
        auto *syntax = isSgType(identity.second);
        if (syntax != nullptr && syntax->get_fortran_source_syntax()) {
          for (SgNode *node : NodeQuery::querySubTree(syntax, V_SgVarRefExp))
            copied_references.insert(isSgVarRefExp(node));
        }
      }
      for (SgVarRefExp *copy_ref : copied_references) {
        auto *source_symbol = copy_ref->get_symbol();
        ASSERT_not_null(source_symbol);
        if (copied_nodes.count(source_symbol->get_declaration()) != 0)
          continue;
        auto local = identities.find(source_symbol->get_declaration());
        if (local != identities.end()) {
          auto *name = isSgInitializedName(local->second);
          ASSERT_not_null(name);
          auto *symbol =
              isSgVariableSymbol(name->get_symbol_from_symbol_table());
          ASSERT_not_null(symbol);
          copy_ref->set_symbol(symbol);
          continue;
        }
        copy_ref->set_symbol(copy_constant(source_symbol));
      }
      auto *interface = new SgInterfaceStatement(
          source_interface->get_name(), source_interface->get_generic_spec());
      setOneSourcePositionForTransformation(interface);
      interface->set_firstNondefiningDeclaration(interface);
      interface->set_definingDeclaration(interface);
      auto *body =
          new SgInterfaceBody(declaration->get_name(), declaration, false);
      setOneSourcePositionForTransformation(body);
      body->set_parent(interface);
      interface->get_interface_body_list().push_back(body);
      declaration->set_scope(func_body);
      declaration->set_parent(body);
      appendStatement(interface, func_body);
      auto *symbol = new SgFunctionSymbol(declaration);
      func_body->insert_symbol(symbol->get_name(), symbol);
      dependency.first->second = symbol;
    }
    SgFunctionSymbol *destination = dependency.first->second;
    source_visible = destination;
    auto kind = SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed;
    if (source_rename != nullptr) {
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload);
      auto binding = generic_bindings.emplace(source_rename, nullptr);
      if (binding.second) {
        auto *rename =
            new SgRenameSymbol(destination->get_declaration(), destination,
                               source_rename->get_name());
        rename->get_causal_nodes().push_back(destination->get_declaration());
        func_body->insert_symbol(rename->get_name(), rename);
        binding.first->second = rename;
      }
      source_visible = binding.first->second;
      kind =
          SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload;
    }
    auto *replacement = SageBuilder::buildFortranFunctionRefExp(
        destination, source_visible, kind);
    SageInterface::replaceExpression(reference, replacement);
  }
}

static SgTemplateParameterPtrList *
copyTemplateParameterList(const SgTemplateParameterPtrList &params) {
  SgTemplateParameterPtrList *copy = new SgTemplateParameterPtrList();
  for (SgTemplateParameterPtrList::const_iterator it = params.begin();
       it != params.end(); ++it) {
    SgTemplateParameter *param = *it;
    ROSE_ASSERT(param != NULL);
    SgTemplateParameter *param_copy =
        SageInterface::cloneDetachedGeneratedTemplateParameter(
            param, "outlined-function-template-parameter");
    copy->push_back(param_copy);
  }
  return copy;
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
    ROSE_ASSERT(param != NULL);
    if (std::find(destination.begin(), destination.end(), param) ==
        destination.end())
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
                           const OutlinedFunctionParameterPlan &plan,
                           SgScopeStatement *scope,
                           const SgTemplateParameterPtrList &template_params) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(isSgGlobal(scope) != NULL);
  SgFunctionParameterList *params = plan.definition_parameters;
  ROSE_ASSERT(plan.syntax_parameters != nullptr);
  auto ownership =
      SageBuilder::function_declaration_ownership::sourceLexicalIn(scope);
  if (plan.has_distinct_source_parameters)
    ownership = ownership.withSourceParameterList(plan.syntax_parameters);
  else
    ROSE_ASSERT(plan.syntax_parameters == params);

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

  SgTemplateFunctionDeclaration *def =
      SageBuilder::buildDefiningTemplateFunctionDeclaration(
          ownership, name, ret_type, params, scope, nondef,
          defining_template_params_copy);
  delete defining_template_params_copy;
  ROSE_ASSERT(def != NULL);
  validateOutlinedFunctionSignature(def, params);

  return def;
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

static bool
isCopiedClassTypedefBaseVisibleFromScope(SgTypedefDeclaration *typedef_decl,
                                         SgScopeStatement *scope) {
  ROSE_ASSERT(typedef_decl != NULL);
  ROSE_ASSERT(typedef_decl->get_base_type() != NULL);
  ROSE_ASSERT(scope != NULL);

  SgType *root_type = typedef_decl->get_base_type()->stripType();
  ROSE_ASSERT(root_type != NULL);
  SgNamedType *named_type = isSgNamedType(root_type);
  if (named_type == NULL)
    return true;

  SgDeclarationStatement *base_decl = named_type->get_declaration();
  if (base_decl == NULL || base_decl->get_scope() == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[class-typedef-base-owner]: "
            "typedef=%s base-type=%p has no exact declaration scope\n",
            typedef_decl->get_name().getString().c_str(),
            static_cast<void *>(root_type));
    ROSE_ABORT();
  }
  return isScopeVisibleFromScope(base_decl->get_scope(), scope);
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

static void prepareLocalTypedefAliases(SgFunctionDeclaration *func,
                                       SgBasicBlock *source_body,
                                       const ASTtools::VarSymSet_t &symbols,
                                       SgCopyHelp &type_bindings) {
  ROSE_ASSERT(func != NULL);
  ROSE_ASSERT(func->get_type() != NULL);
  ROSE_ASSERT(func->get_definition() != NULL);
  SgBasicBlock *func_body = func->get_definition()->get_body();
  ROSE_ASSERT(func_body != NULL);
  ROSE_ASSERT(source_body != nullptr && func_body->get_statements().empty());

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
  collectExplicitTypeReferences(source_body, typedefs, seen);
  for (const SgVariableSymbol *symbol : symbols) {
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
    collectTypedefsFromType(symbol->get_declaration()->get_type(), typedefs,
                            seen);
  }

  for (SgTypedefDeclaration *typedef_decl : typedefs) {
    ROSE_ASSERT(typedef_decl != nullptr &&
                typedef_decl->get_base_type() != nullptr);
    if (SageInterface::isAncestor(source_body, typedef_decl)) {
      type_bindings.insertCopiedNodePair(typedef_decl, typedef_decl);
      type_bindings.insertCopiedNodePair(typedef_decl->get_type(),
                                         typedef_decl->get_type());
      continue;
    }
    if (isTypedefVisibleFromScope(typedef_decl, func_body))
      continue;

    const bool function_owned = SageInterface::getEnclosingFunctionDeclaration(
                                    typedef_decl, true) != NULL;
    const bool class_owned =
        isSgClassDefinition(typedef_decl->get_scope()) != NULL;

    // Local dependencies need declarations in the destination. Class aliases
    // can be reproduced only when their base is visible there.
    if (!function_owned &&
        (!class_owned ||
         !isCopiedClassTypedefBaseVisibleFromScope(typedef_decl, func_body)))
      continue;

    const std::string original_name = typedef_decl->get_name().getString();
    ROSE_ASSERT(!original_name.empty());
    std::string name = original_name;
    for (size_t suffix = 0; func_body->lookup_symbol(name) != nullptr ||
                            source_body->lookup_symbol(name) != nullptr;
         ++suffix)
      name = original_name + "_rex_" + std::to_string(suffix);
    SgTypedefDeclaration *alias = SageBuilder::buildTypedefDeclaration(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        typedef_decl->get_typedef_type(), name,
        SageInterface::materializeTypeWithCopyMap(typedef_decl->get_base_type(),
                                                  type_bindings),
        func_body);
    if (alias == NULL || alias->get_file_info() == NULL ||
        !alias->get_file_info()->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[local-typedef-owner]: alias=%p name=%s "
              "was not constructed as generated source output\n",
              static_cast<void *>(alias), name.c_str());
      ROSE_ABORT();
    }
    ROSE_ASSERT(alias->get_parent() == func_body);
    type_bindings.insertCopiedNodePair(typedef_decl, alias);
    type_bindings.insertCopiedNodePair(typedef_decl->get_type(),
                                       alias->get_type());
  }
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
    param_base_type = init_type;

    // For C++ reference type, we use its base type since pointer to a reference
    // type is not allowed Liao, 8/14/2009
    param_base_type =
        SageBuilder::buildAddressOfResultType(param_base_type)->get_base_type();

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
          SageBuilder::buildAddressOfResultType(source_param_base_type)
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

  string init_name = i_name->get_name().str();
  string new_param_name = init_name;
  SgType *new_param_type = NULL;
  SgType *new_source_param_type = NULL;

  if (Outliner::enable_classic) {
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
  } else {
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
    const ASTtools::VarSymSet_t &pointer_dereference_symbols,
    SgFunctionParameterList *leading_parameters) {
  if (leading_parameters != nullptr) {
    ROSE_ASSERT(leading_parameters->get_parent() == nullptr);
    for (auto *parameter : leading_parameters->get_args()) {
      ROSE_ASSERT(parameter != nullptr &&
                  parameter->get_parent() == leading_parameters &&
                  parameter->get_declptr() == nullptr &&
                  parameter->get_type() != nullptr);
    }
  }
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

  const size_t leading_arity =
      leading_parameters != nullptr ? leading_parameters->get_args().size() : 0;
  if (leading_parameters != nullptr) {
    const auto arguments = leading_parameters->get_args();
    leading_parameters->get_args().clear();
    for (auto i = arguments.rbegin(); i != arguments.rend(); ++i) {
      auto *parameter = *i;
      parameter->set_parent(nullptr);
      prependArg(plan.definition_parameters, parameter);
      if (plan.syntax_parameters != plan.definition_parameters) {
        prependArg(
            plan.syntax_parameters,
            buildInitializedName(parameter->get_name(), parameter->get_type()));
      }
    }
    delete leading_parameters;
  }
  const size_t expected_arity =
      leading_arity +
      (plan.wrapper_parameter != NULL ? 1 : plan.direct_parameters.size());
  if (plan.syntax_parameters == NULL ||
      plan.definition_parameters->get_args().size() != expected_arity ||
      plan.syntax_parameters->get_args().size() != expected_arity) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[function-signature-plan]: planned "
                    "parameter arity is inconsistent\n");
    ROSE_ABORT();
  }
  return plan;
}

// Generate element-wise copies; loop_indexes are declared before the outer
// loop.
static SgStatement *
build_array_unpacking_statement(SgExpression *lhs, SgExpression *rhs,
                                SgType *type, SgScopeStatement *scope,
                                SgStatementPtrList &loop_indexes) {
  ROSE_ASSERT(isSgArrayType(type));
  ROSE_ASSERT(scope);

  std::string loop_index_name =
      SageInterface::generateUniqueVariableName(scope, "i");
  SgVariableDeclaration *loop_index = buildVariableDeclaration(
      loop_index_name, buildIntType(), NULL /* initializer */, scope);
  ROSE_ASSERT(loop_index->get_variables().size() == 1);
  SgInitializedName *index_name = loop_index->get_variables().front();
  SgVariableSymbol *index_symbol =
      isSgVariableSymbol(index_name->get_symbol_from_symbol_table());
  ROSE_ASSERT(index_symbol);
  ROSE_ASSERT(index_symbol->get_declaration() == index_name);
  ROSE_ASSERT(index_symbol->get_scope() == scope);
  loop_indexes.push_back(loop_index);
  SgStatement *loop_init =
      buildAssignStatement(buildVarRefExp(index_symbol), buildIntVal(0));

  SgStatement *loop_test = buildExprStatement(buildLessThanOp(
      buildVarRefExp(index_symbol),
      copyExpression(isSgArrayType(type)->get_index()),
      SageInterface::is_C_language() ? static_cast<SgType *>(buildIntType())
                                     : static_cast<SgType *>(buildBoolType())));

  SgExpression *loop_increment = buildPlusPlusOp(
      buildVarRefExp(index_symbol), buildIntType(), SgUnaryOp::postfix);

  SgExpression *assign_lhs =
      buildPntrArrRefExp(lhs, buildVarRefExp(index_symbol),
                         SageInterface::getElementType(lhs->get_type()));
  SgExpression *assign_rhs =
      buildPntrArrRefExp(rhs, buildVarRefExp(index_symbol),
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

  return buildForStatement(loop_init, loop_test, loop_increment, loop_body);
}

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
    SgType *local_type_template_parameter, SgCopyHelp &type_bindings) {
  ROSE_ASSERT(param && scope && i_name);

  // keep the original name
  const string orig_var_name = i_name->get_name().str();

  //---------------step 1 -----------------------------------------------
  // decide on the type : local_type
  // the original data type of the variable passed via parameter
  SgType *orig_var_type = SageInterface::materializeTypeWithCopyMap(
      i_name->get_type(), type_bindings);
  SgType *orig_var_source_type = SageInterface::is_Fortran_language()
                                     ? NULL
                                     : exactCapturedVariableSourceType(i_name);
  if (orig_var_source_type != nullptr)
    orig_var_source_type = SageInterface::materializeTypeWithCopyMap(
        orig_var_source_type, type_bindings);
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
    if (isSgFunctionDefinition(i_name->get_scope())) {
      ROSE_ASSERT(isSgArrayType(orig_var_type) == NULL);
      SgType *source_base =
          orig_var_source_type != NULL
              ? orig_var_source_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                                SgType::STRIP_MODIFIER_TYPE)
              : NULL;
      SgArrayType *source_array = isSgArrayType(source_base);
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
  const bool use_cxx_capture_reference = SageInterface::is_Cxx_language() &&
                                         Outliner::useStructureWrapper &&
                                         !isPointerDeref;

  SgType *local_type = NULL;
  SgType *local_source_type = NULL;
  auto captureReferenceType = [](SgType *type) {
    auto *reference = isSgReferenceType(type);
    if (reference != nullptr && reference->has_source_prototype())
      return reference;
    return SgReferenceType::createType(
        reference != nullptr ? reference->get_base_type() : type);
  };
  if (SageInterface::is_Fortran_language())
    local_type = orig_var_type;
  else if (Outliner::temp_variable || Outliner::useStructureWrapper)
  // unique processing for C/C++ if temp variables are used
  {
    if (use_cxx_capture_reference) {
      local_type =
          buildReferenceType(Outliner::captureValueType(orig_var_type));
      if (orig_var_source_type != NULL)
        local_source_type = buildReferenceType(
            Outliner::captureValueType(orig_var_source_type));
    } else if (use_cxx_reference_for_pointer_deref) {
      local_type = captureReferenceType(orig_var_type);
      if (orig_var_source_type != NULL) {
        local_source_type = captureReferenceType(orig_var_source_type);
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
      local_type = captureReferenceType(orig_var_type);
      if (orig_var_source_type != NULL) {
        local_source_type = captureReferenceType(orig_var_source_type);
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
          i_name->get_fortran_procedure_interface() != nullptr) {
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
        if (!use_cxx_capture_reference && !isSgArrayType(local_type)) {
          // When necessary, we must catch the address before we do the casting
          if (!isPointerDeref && is_array_parameter) {
            param_ref = buildAddressOfOp(
                param_ref,
                SageBuilder::buildAddressOfResultType(param_ref->get_type()),
                SgAddressOfOp::e_address_operand_object);
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

  const bool unpack_array =
      isSgArrayType(local_type->stripType(SgType::STRIP_TYPEDEF_TYPE)) != NULL;
  if (Outliner::useStructureWrapper) {
    // Or for structure type paramter
    // int (*sum)[100UL] = __out_argv->sum_p; // is PointerDeref type
    // int i = __out_argv->i;
    if (!unpack_array)
      local_val = buildAssignInitializer(param_ref, local_type);
  } else if (!unpack_array) {
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
    SgType *source_cast_type = local_var_type_ptr;
    if (orig_var_source_type != NULL) {
      SgReferenceType *source_ref = isSgReferenceType(orig_var_source_type);
      source_cast_type = global_array_type != NULL
                             ? local_source_type
                             : SgPointerType::createType(
                                   source_ref ? source_ref->get_base_type()
                                              : orig_var_source_type);
      if (source_cast_type == NULL ||
          !SageInterface::cxxSourceTypeMatchesSemanticType(
              source_cast_type, local_var_type_ptr)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[unpack-cast-source-type]: captured "
                "variable=%s has no exact source cast type\n",
                i_name->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }
    SgCastExp *cast_expr =
        buildTransformationCastExp(param_ref, local_var_type_ptr,
                                   SgCastExp::e_C_style_cast, source_cast_type);
    if (orig_var_source_type != NULL) {
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
  if (unpack_array) {
    // We need to copy every element of the array
    decl = buildVariableDeclaration(local_name, local_type, NULL, scope,
                                    local_source_type);

    SgStatementPtrList loop_indexes;
    SgStatement *array_init = build_array_unpacking_statement(
        buildVarRefExp(decl), param_ref,
        local_type->stripType(SgType::STRIP_TYPEDEF_TYPE), scope, loop_indexes);
    SageInterface::prependStatement(array_init, scope);
    SageInterface::prependStatementList(loop_indexes, scope);
  } else {
    decl = buildVariableDeclaration(local_name, local_type, local_val, scope,
                                    local_source_type);
  }

  return decl;
}

static SgAssignOp *createPackExpr(SgInitializedName *local_unpack_def) {
  ROSE_ASSERT(local_unpack_def);
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

  // Create the RHS, which references the local variable.
  SgScopeStatement *scope = local_unpack_def->get_scope();
  ROSE_ASSERT(scope);
  SgVariableSymbol *local_var_sym =
      isSgVariableSymbol(local_unpack_def->get_symbol_from_symbol_table());
  ROSE_ASSERT(local_var_sym);
  ROSE_ASSERT(local_var_sym->get_declaration() == local_unpack_def);
  ROSE_ASSERT(local_var_sym->get_scope() == scope);

  SgPointerDerefExp *param_deref_pack =
      isSgPointerDerefExp(ASTtools::deepCopy(param_deref_unpack));
  ROSE_ASSERT(param_deref_pack);

  SgVarRefExp *local_var_ref = SageBuilder::buildVarRefExp(local_var_sym);
  ROSE_ASSERT(local_var_ref);

  // Assemble the final assignment expression.
  return SageBuilder::buildAssignOp(param_deref_pack, local_var_ref,
                                    param_deref_pack->get_type());
}

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

// Record a binding that the declaration builder has already published. Capture
// analysis must not create a second symbol or move the declaration's scope.
static void recordSymRemap(const SgVariableSymbol *orig_sym,
                           SgInitializedName *name_new, SgScopeStatement *scope,
                           VarSymRemap_t &sym_remap) {
  ROSE_ASSERT(orig_sym != nullptr && name_new != nullptr && scope != nullptr);
  auto *symbol =
      isSgVariableSymbol(scope->find_symbol_from_declaration(name_new));
  SgSymbolTable *table = scope->get_symbol_table();
  if (symbol == nullptr || table == nullptr || symbol == orig_sym ||
      symbol->get_declaration() != name_new || name_new->get_scope() != scope ||
      symbol->get_parent() != table || !table->exists(symbol) ||
      !sym_remap.emplace(orig_sym, symbol).second) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[symbol-remap-publication]: "
            "source=%p generated-name=%p scope=%p symbol=%p table=%p has "
            "no unique exact generated identity\n",
            static_cast<const void *>(orig_sym), static_cast<void *>(name_new),
            static_cast<void *>(scope), static_cast<void *>(symbol),
            static_cast<void *>(table));
    ROSE_ABORT();
  }
}

static void recordSymRemap(const SgVariableSymbol *orig_sym,
                           SgVariableDeclaration *new_decl,
                           SgScopeStatement *scope, VarSymRemap_t &sym_remap) {
  if (orig_sym && new_decl) {
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
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_plan,
    SgCopyHelp &type_bindings) {
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
           removeExactReferenceLayer(replacement_declared_type) &&
       removeExactReferenceLayer(SageInterface::materializeTypeWithCopyMap(
           original_reference_type, type_bindings)) !=
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
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_plan,
    SgCopyHelp &type_bindings) {
  validateCheckedCasts(b, "before variable remapping");

  // Check if variable remapping is even needed.
  if (vsym_remap.empty() && private_remap.empty())
    return;

  std::map<const SgInitializedName *, SgVariableSymbol *> remap_by_decl;
  for (VarSymRemap_t::const_iterator i = vsym_remap.begin();
       i != vsym_remap.end(); ++i) {
    const SgVariableSymbol *orig_sym = i->first;
    SgVariableSymbol *new_sym = i->second;
    ASSERT_not_null(orig_sym);
    ASSERT_not_null(new_sym);

    const SgInitializedName *orig_decl = orig_sym->get_declaration();
    ASSERT_not_null(orig_decl);

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
  }

  auto findRegularRemap = [&](SgVarRefExp *ref_orig) -> SgVariableSymbol * {
    ROSE_ASSERT(ref_orig != NULL);
    SgVariableSymbol *ref_sym = ref_orig->get_symbol();
    ASSERT_not_null(ref_sym);

    VarSymRemap_t::const_iterator ref_new = vsym_remap.find(ref_sym);
    if (ref_new != vsym_remap.end())
      return ref_new->second;

    SgInitializedName *ref_decl = ref_sym->get_declaration();
    ASSERT_not_null(ref_decl);

    std::map<const SgInitializedName *, SgVariableSymbol *>::const_iterator
        decl_match = remap_by_decl.find(ref_decl);
    if (decl_match != remap_by_decl.end())
      return decl_match->second;

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
    requireExactVarRefRetargetTypes(ref, symbol, local_type_plan, type_bindings);
    SgInitializedName *original = ref->get_symbol()->get_declaration();
    for (SgNode *owner = ref->get_parent(); owner != NULL;
         owner = owner->get_parent()) {
      if (auto *task = isSgOmpTaskStatement(owner)) {
        task->validate_semantic_data_environment();
        for (auto *variables : {&task->get_semantic_firstprivate_variables(),
                                &task->get_semantic_private_variables()})
          std::replace(variables->begin(), variables->end(), original,
                       symbol->get_declaration());
        task->validate_semantic_data_environment();
      }
      if (owner == b)
        break;
    }
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
    if (!ASTtools::requiresVariableCapture(ref_orig))
      continue;
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

static void variableHandling(
    const ASTtools::VarSymSet_t
        &syms, // all variables passed to the outlined function: //regular
               // (shared) parameters?
    const ASTtools::VarSymSet_t
        &pdSyms, // those must use pointer dereference: use pass-by-reference
    const std::set<SgInitializedName *>
        &restoreVars, // variables to be restored after variable cloning
    SgClassDeclaration
        *struct_decl, // an optional struct wrapper for all variables
    const OutlinedFunctionParameterPlan &parameter_plan,
    const Outliner::OutlinedLocalTypeTemplatePlan &local_type_template_plan,
    SgCopyHelp &type_bindings, SgStatement *type_dependency_tail,
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

  int counter = 0;
  SgInitializedName *parameter1 = NULL; // the wrapper parameter
  SgVariableDeclaration *local_var_decl = NULL;

  if (!Outliner::enable_classic && Outliner::useParameterWrapper) {
    parameter1 = parameter_plan.wrapper_parameter;
    ROSE_ASSERT(parameter1 != nullptr && parameter1->get_parent() == params);
    if (SageInterface::is_Fortran_language()) {
      SgVariableDeclaration *var_decl = buildVariableDeclaration(
          parameter1->get_name(), parameter1->get_type(), NULL, body);
      prependStatement(var_decl, body);
    }
  }

  for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin();
       i != syms.rend(); ++i) {
    const SgInitializedName *i_name = (*i)->get_declaration();
    ROSE_ASSERT(i_name);
    const SgVariableSymbol *sym = isSgVariableSymbol(*i);

    bool use_orig_type = false;
    if (pdSyms.find(sym) ==
        pdSyms.end()) // not a variable to use AddressOf, then it should be a
                      // variable using its original type
      use_orig_type = true;
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

    if (Outliner::enable_classic)
    // classic methods use parameters directly, no unpacking is needed
    {
      // Parameter spelling is not declaration identity. Even a same-type,
      // same-name direct parameter must explicitly replace the captured source
      // symbol; relying on later name lookup leaves references bound to a
      // detached source declaration. Pointer parameters use the same exact map
      // and are rewritten through the typed dereference path below.
      recordSymRemap(*i, p_init_name, func->get_definition(), sym_remap);
    } else { // create unwrapping statements from parameters/ or the array
             // parameter for pointers
      const Outliner::OutlinedLocalTypeTemplateEntry *local_type_entry =
          findLocalTypeTemplateEntry(local_type_template_plan,
                                     i_name->get_type());
      local_var_decl = createUnpackDecl(
          p_init_name, counter, isPointerDeref, i_name, struct_decl, body,
          local_type_entry != NULL ? local_type_entry->defining_parameter_type
                                   : NULL,
          type_bindings);
      ROSE_ASSERT(local_var_decl);

      if (type_dependency_tail != nullptr)
        insertStatementAfter(type_dependency_tail, local_var_decl);
      else
        prependStatement(local_var_decl, body);

      // regular and shared variables used the first local declaration
      recordSymRemap(*i, local_var_decl, args_scope, sym_remap);
    }

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
  } // end for

  // variable substitution
  remapVarSyms(sym_remap, private_remap, body, local_type_template_plan,
               type_bindings);
}

// same input file: two SgSourceFile nodes are created from it.
// we build symbol mapping from first to second.
// This is used to support reset symbols of AST subtree moved from first to
// second file.

class MovedBodyClassTypeEdgeCollector : public SimpleReferenceToPointerHandler {
public:
  void operator()(SgNode *&node, const SgName &, bool) override {
    if (SgClassType *class_type = isSgClassType(node))
      class_types.insert(class_type);
  }

  std::set<SgClassType *> class_types;
};

class OutliningTypeEdgeProjector : public SimpleReferenceToPointerHandler {
public:
  explicit OutliningTypeEdgeProjector(SgCopyHelp &type_bindings)
      : bindings(type_bindings) {}

  void operator()(SgNode *&node, const SgName &, bool) override {
    if (SgType *type = isSgType(node))
      node = SageInterface::materializeTypeWithCopyMap(type, bindings, false);
  }

private:
  SgCopyHelp &bindings;
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
    SgClassSymbol *destination_symbol = isSgClassSymbol(
        SageBuilder::findAssociatedSymbolInTargetAST(source_canonical,
                                                    destination_global));
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

  SgTreeCopy bindings;
  for (const auto &mapping : replacements) {
    auto *source = isSgType(mapping.first);
    auto *target = isSgType(mapping.second);
    ROSE_ASSERT(source != nullptr && target != nullptr && source != target &&
                source->variantT() == target->variantT());
    bindings.insertCopiedNodePair(source, target);
  }
  OutliningTypeEdgeProjector rewriter(bindings);
  RoseAst rewritten_body_ast(moved_body);
  for (RoseAst::iterator node = rewritten_body_ast.begin();
       node != rewritten_body_ast.end(); ++node)
    if (isSgType(*node) == nullptr)
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
    const std::set<SgInitializedName *>
        &restoreVars, // optional information about variables to be restored
                      // after variable clones finish computation
    SgClassDeclaration
        *struct_decl, // an optional wrapper structure for parameters
    SgScopeStatement *scope,
    OutlinedLocalTypeTemplatePlan &local_type_template_plan,
    OutlinedFunctionParameterBindings &parameter_bindings,
    SgFunctionParameterList *leading_parameters) {
  ROSE_ASSERT(parameter_bindings.empty());
  ROSE_ASSERT(s && scope);
  ROSE_ASSERT(isValidOutliningScope(scope));


  std::vector<SgFunctionSymbol *> fortran_result_functions;
  if (SageInterface::is_Fortran_language()) {
    std::set<SgFunctionDeclaration *> seen;
    for (SgNode *node : NodeQuery::querySubTree(s, V_SgFunctionRefExp)) {
      auto *reference = isSgFunctionRefExp(node);
      ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
      auto *declaration = reference->get_symbol()->get_declaration();
      ROSE_ASSERT(declaration != nullptr);
      auto *canonical = isSgFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonical != nullptr);
      if (isSgRenameSymbol(reference->get_fortran_source_visible_symbol()))
        continue;
      auto *definition =
          isSgFunctionDeclaration(canonical->get_definingDeclaration());
      if (definition == nullptr || definition->get_definition() == nullptr)
        continue;
      auto *symbol =
          isSgFunctionSymbol(canonical->get_symbol_from_symbol_table());
      ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == canonical);
      auto *type = isSgFunctionType(symbol->get_type());
      ROSE_ASSERT(type != nullptr && type->get_return_type() != nullptr);
      if (!isSgTypeVoid(type->get_return_type()) &&
          seen.insert(canonical).second)
        fortran_result_functions.push_back(symbol);
    }
  }

  SgName func_name(func_name_str);
  const SgInitializedNamePtrList leading_arguments =
      leading_parameters != nullptr ? leading_parameters->get_args()
                                    : SgInitializedNamePtrList{};
  OutlinedFunctionParameterPlan parameter_plan =
      buildOutlinedFunctionParameterPlan(syms, pdSyms, leading_parameters);
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
                                      parameter_plan, scope, template_params);
  } else {
    func = createFuncSkeleton(func_name, SgTypeVoid::createType(),
                              parameter_plan, scope);
  }
  ROSE_ASSERT(func);
  bindLocalTypeTemplatePlanToDefinition(func, local_type_template_plan);
  if (parameter_plan.has_distinct_source_parameters) {
    ROSE_ASSERT(func->get_parameterList_syntax() ==
                parameter_plan.syntax_parameters);
  }
  validateOutlinedFunctionSignature(func, parameterList);

  // Inherit enclosing function's inline property: avoid linking error when
  // linking multiple .lib files with the outlined functions
  if (enclosing_func->get_functionModifier().isInline()) {
    func->get_functionModifier().setInline();
  }

  bool is_template_func = (isSgTemplateFunctionDeclaration(func) != NULL ||
                           isSgTemplateMemberFunctionDeclaration(func) != NULL);
  if (!is_template_func &&
      (SageInterface::is_Cxx_language() || is_mixed_C_and_Cxx_language() ||
       is_mixed_Fortran_and_Cxx_language() ||
       is_mixed_Fortran_and_C_and_Cxx_language())) {
    func->get_declarationModifier().get_storageModifier().setExtern();
    func->set_linkage("C");
  }

  SgBasicBlock *func_body = func->get_definition()->get_body();
  ROSE_ASSERT(func_body != NULL);

  ROSE_ASSERT(func_body->get_statements().empty() == true);
  SgTreeCopy type_bindings;
  prepareLocalTypedefAliases(func, s, syms, type_bindings);
  if (SageInterface::is_Fortran_language()) {
    publishFortranProcedureDependencies(s, func_body);
    for (auto *parameter : leading_arguments) {
      auto *declaration = buildVariableDeclaration(
          parameter->get_name(), parameter->get_type(), nullptr, func_body);
      appendStatement(declaration, func_body);
    }
  }
  for (SgFunctionSymbol *symbol : fortran_result_functions) {
    const SgName &name = symbol->get_name();
    ROSE_ASSERT(s->lookup_variable_symbol(name) == nullptr);
    ROSE_ASSERT(func->get_definition()->lookup_variable_symbol(name) ==
                nullptr);
    auto *declaration = buildVariableDeclaration(
        name, isSgFunctionType(symbol->get_type())->get_return_type(), nullptr,
        func_body);
    ROSE_ASSERT(declaration != nullptr);
    declaration->get_declarationModifier().get_storageModifier().setExtern();
    appendStatement(declaration, func_body);
  }
  SgStatement *type_dependency_tail = func_body->get_statements().empty()
                                          ? nullptr
                                          : func_body->get_statements().back();
  OutliningTypeEdgeProjector bind_body_types(type_bindings);
  RoseAst source_body(s);
  for (SgNode *node : source_body)
    if (isSgType(node) == nullptr)
      node->processDataMemberReferenceToPointers(&bind_body_types);
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

  SgGlobal *copied_source_global = NULL;
  SgGlobal *copied_destination_global = NULL;
  if (SageInterface::getEnclosingSourceFile(scope) !=
      SageInterface::getEnclosingSourceFile(s)) {
    SgSourceFile *target_source_file =
        SageInterface::getEnclosingSourceFile(scope);
    ROSE_ASSERT(target_source_file != NULL);
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func_body) ==
                target_source_file);
    ASSERT_not_null(target_source_file->get_file_info());
    ROSE_ASSERT(target_source_file->get_file_info()->get_physical_file_id() >=
                0);
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

  variableHandling(syms, pdSyms, restoreVars, struct_decl, parameter_plan,
                   local_type_template_plan, type_bindings,
                   type_dependency_tail, func);
  ROSE_ASSERT(func != NULL);
  validateOutlinedFunctionSignature(func, parameterList);

  if (SageInterface::is_Fortran_language()) {
    for (SgNode *node :
         NodeQuery::querySubTree(func_body, V_SgFunctionRefExp)) {
      SgFunctionRefExp *reference = isSgFunctionRefExp(node);
      ASSERT_not_null(reference);
      SgFunctionSymbol *semantic = reference->get_symbol();
      SgFunctionSymbol *source_visible =
          reference->get_fortran_source_visible_symbol();
      const auto kind = reference->get_fortran_source_visible_binding_kind();
      SgScopeStatement *source_scope =
          source_visible != nullptr ? source_visible->get_scope() : nullptr;
      SgSymbolTable *source_table =
          source_scope != nullptr ? source_scope->get_symbol_table() : nullptr;
      const bool semantic_publication =
          kind == SgFunctionRefExp::
                      e_fortran_source_visible_binding_semantic_publication;
      if (!SageInterface::Detail::hasExactFortranFunctionReferenceBindings(
              semantic, source_visible, kind) ||
          (!semantic_publication &&
           !SageInterface::isAncestor(source_scope, reference))) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[moved-function-source-binding]: "
                "reference=%p semantic=%p source-visible=%p "
                "source-scope=%p source-table=%p kind=%d "
                "has no exact post-move Fortran source binding\n",
                static_cast<void *>(reference), static_cast<void *>(semantic),
                static_cast<void *>(source_visible),
                static_cast<void *>(source_scope),
                static_cast<void *>(source_table), static_cast<int>(kind));
        ROSE_ABORT();
      }
    }
  }

  if (copied_source_global != NULL) {
    std::map<SgSymbol *, SgSymbol *> copied_symbols;

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

      auto mapping = copied_symbols.find(source_symbol);
      if (mapping == copied_symbols.end()) {
        SgSymbol *target_symbol = findAssociatedSymbolInTargetAST(
            source_symbol->get_symbol_basis(), copied_destination_global);
        mapping = copied_symbols.emplace(source_symbol, target_symbol).first;
      }
      SgSymbol *destination_symbol = mapping->second;
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
    remapMovedBodyClassTypeIdentities(copied_source_global,
                                      copied_destination_global, func_body);
  }

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  validateOutlinedFunctionSignature(func, parameterList);

  parameter_bindings = parameter_plan.direct_parameters;
  return func;
}
