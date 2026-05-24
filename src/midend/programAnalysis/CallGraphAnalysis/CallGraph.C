#include "sage3basic.h"

// This fixed a reported bug which caused conflicts with configure-time macros
// (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the list of
// include files.
#include "CallGraph.h"

#include "rose_config.h"

#include "sageGeneric.h"

#include <err.h>
#include <mutex>
#include <set>

using namespace std;
using namespace Rose;

/***************************************************
 * Get the vector of base types for the current type
 **************************************************/

static bool
isUninstantiatedTemplateFunctionPattern(SgFunctionDeclaration *fdecl) {
  return isSgTemplateFunctionDeclaration(fdecl) != NULL ||
         isSgTemplateMemberFunctionDeclaration(fdecl) != NULL;
}

static bool
callGraphTypeContainsDependentTemplateType(SgType *type,
                                           std::set<SgType *> &seen);

static bool callGraphTypeContainsDependentTemplateType(SgType *type) {
  std::set<SgType *> seen;
  return callGraphTypeContainsDependentTemplateType(type, seen);
}

static bool
callGraphTypeContainsDependentTemplateType(SgType *type,
                                           std::set<SgType *> &seen) {
  if (type == NULL || !seen.insert(type).second) {
    return false;
  }

  if (isSgTemplateType(type) != NULL || isSgNonrealType(type) != NULL) {
    return true;
  }

  if (SgModifierType *modifierType = isSgModifierType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        modifierType->get_base_type(), seen);
  }
  if (SgPointerType *pointerType = isSgPointerType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        pointerType->get_base_type(), seen);
  }
  if (SgPointerMemberType *memberPointerType = isSgPointerMemberType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        memberPointerType->get_base_type(), seen);
  }
  if (SgReferenceType *referenceType = isSgReferenceType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        referenceType->get_base_type(), seen);
  }
  if (SgRvalueReferenceType *referenceType = isSgRvalueReferenceType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        referenceType->get_base_type(), seen);
  }
  if (SgArrayType *arrayType = isSgArrayType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        arrayType->get_base_type(), seen);
  }
  if (SgTypedefType *typedefType = isSgTypedefType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        typedefType->get_base_type(), seen);
  }

  if (SgFunctionType *functionType = isSgFunctionType(type)) {
    if (callGraphTypeContainsDependentTemplateType(
            functionType->get_return_type(), seen)) {
      return true;
    }
    for (SgType *argType : functionType->get_arguments()) {
      if (callGraphTypeContainsDependentTemplateType(argType, seen)) {
        return true;
      }
    }
  }

  return false;
}

static bool
isUnresolvedTemplateInstantiationCallable(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return false;
  }

  if (isSgTemplateInstantiationFunctionDecl(fdecl) == NULL &&
      isSgTemplateInstantiationMemberFunctionDecl(fdecl) == NULL) {
    return false;
  }

  if (SgTemplateInstantiationMemberFunctionDecl *instMember =
          isSgTemplateInstantiationMemberFunctionDecl(fdecl)) {
    SgTemplateMemberFunctionDeclaration *templateDecl =
        instMember->get_templateDeclaration();
    if (templateDecl == NULL) {
      templateDecl = isSgTemplateMemberFunctionDeclaration(
          instMember->get_specializedTemplateDeclaration());
    }

    if (callGraphInstantiatedMemberHasClassTemplateArguments(instMember) &&
        callGraphDeclHasDefinitionOrDefiningDeclaration(templateDecl)) {
      return false;
    }
  }

  return !callGraphDeclHasDefinitionOrDefiningDeclaration(fdecl) &&
         callGraphTypeContainsDependentTemplateType(fdecl->get_type());
}

static bool isNestedInUninstantiatedTemplateClassContext(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (isSgTemplateInstantiationDefn(cursor) != NULL ||
        isSgTemplateInstantiationDecl(cursor) != NULL) {
      return false;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return true;
    }
  }

  return false;
}

static bool isNestedInTemplateInstantiationContext(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (isSgTemplateInstantiationDefn(cursor) != NULL ||
        isSgTemplateInstantiationDecl(cursor) != NULL) {
      return true;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return false;
    }
  }

  return false;
}

static bool
isUninstantiatedTemplateClassMemberPattern(SgFunctionDeclaration *fdecl) {
  SgMemberFunctionDeclaration *member = isSgMemberFunctionDeclaration(fdecl);
  if (member == NULL) {
    return false;
  }

  if (SgClassDeclaration *associatedClass =
          isSgClassDeclaration(member->get_associatedClassDeclaration())) {
    if (isNestedInUninstantiatedTemplateClassContext(associatedClass)) {
      return true;
    }
  }

  return isNestedInUninstantiatedTemplateClassContext(member);
}

static SgFunctionDeclaration *
canonicalCallableFunctionDecl(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return NULL;
  }

  fdecl = canonicalFunctionDeclForCallGraph(fdecl);
  if (isUninstantiatedTemplateFunctionPattern(fdecl) ||
      isUninstantiatedTemplateClassMemberPattern(fdecl) ||
      isUnresolvedTemplateInstantiationCallable(fdecl)) {
    return NULL;
  }

  return fdecl;
}

static SgType *skipTypeAliases(SgType *ty) {
  ASSERT_not_null(ty);

  return ty->stripType(SgType::STRIP_TYPEDEF_TYPE);
}

static SgMemberFunctionType *
memberFunctionTypeFromMemberPointerType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  if (SgMemberFunctionType *memberFunctionType = isSgMemberFunctionType(type)) {
    return memberFunctionType;
  }

  if (SgPointerMemberType *memberPointerType = isSgPointerMemberType(type)) {
    return memberFunctionTypeFromMemberPointerType(
        memberPointerType->get_base_type());
  }

  SgType *baseType = type->findBaseType();
  if (baseType != NULL && baseType != type) {
    return memberFunctionTypeFromMemberPointerType(baseType);
  }

  return NULL;
}

static SgPointerMemberType *pointerMemberTypeFromType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  if (SgPointerMemberType *memberPointerType = isSgPointerMemberType(type)) {
    return memberPointerType;
  }

  SgType *stripped = type->stripTypedefsAndModifiers();
  if (stripped != NULL && stripped != type) {
    return pointerMemberTypeFromType(stripped);
  }

  return NULL;
}

static SgDeclarationStatementPtrList *
memberScopeDeclarationListForFunctionRef(SgScopeStatement *scope) {
  if (scope == NULL)
    return NULL;
  if (SgClassDefinition *class_def = isSgClassDefinition(scope))
    return &class_def->get_members();
  if (SgTemplateClassDefinition *template_def =
          isSgTemplateClassDefinition(scope))
    return &template_def->get_members();
  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope))
    return &inst_def->get_members();
  return NULL;
}

static SgScopeStatement *enclosingMemberScopeForFunctionRef(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(cursor)) {
      if (memberScopeDeclarationListForFunctionRef(scope) != NULL)
        return scope;
    }
  }
  return NULL;
}

static SgMemberFunctionDeclaration *
canonicalMemberFunctionDeclForRef(SgMemberFunctionDeclaration *decl) {
  if (decl == NULL)
    return NULL;
  if (SgMemberFunctionDeclaration *first_nondef = isSgMemberFunctionDeclaration(
          decl->get_firstNondefiningDeclaration()))
    return first_nondef;
  return decl;
}

static SgTemplateMemberFunctionDeclaration *
canonicalTemplateMemberPatternDecl(SgDeclarationStatement *decl) {
  SgTemplateMemberFunctionDeclaration *pattern =
      isSgTemplateMemberFunctionDeclaration(decl);
  if (pattern == NULL) {
    return NULL;
  }

  if (SgTemplateMemberFunctionDeclaration *first_nondef =
          isSgTemplateMemberFunctionDeclaration(
              pattern->get_firstNondefiningDeclaration())) {
    return first_nondef;
  }

  if (SgTemplateMemberFunctionDeclaration *defining =
          isSgTemplateMemberFunctionDeclaration(
              pattern->get_definingDeclaration())) {
    if (SgTemplateMemberFunctionDeclaration *first_nondef =
            isSgTemplateMemberFunctionDeclaration(
                defining->get_firstNondefiningDeclaration())) {
      return first_nondef;
    }
    return defining;
  }

  return pattern;
}

static bool memberFunctionDeclMatchesRefType(SgMemberFunctionDeclaration *decl,
                                             SgMemberFunctionRefExp *ref) {
  if (decl == NULL || ref == NULL)
    return false;
  SgType *decl_type = decl->get_type();
  SgType *ref_type = ref->get_type();
  if (decl_type == NULL || ref_type == NULL)
    return false;
  return decl_type == ref_type ||
         SageInterface::isEquivalentType(decl_type, ref_type);
}

static SgMemberFunctionDeclaration *
resolveMemberFunctionDeclarationFromRef(SgMemberFunctionRefExp *ref) {
  if (ref == NULL)
    return NULL;

  SgMemberFunctionSymbol *symbol = ref->get_symbol();
  if (symbol != NULL) {
    if (SgMemberFunctionDeclaration *decl =
            isSgMemberFunctionDeclaration(symbol->get_declaration()))
      return decl;
  }

  if (symbol == NULL)
    return NULL;

  SgScopeStatement *member_scope = enclosingMemberScopeForFunctionRef(ref);
  SgDeclarationStatementPtrList *members =
      memberScopeDeclarationListForFunctionRef(member_scope);
  if (members == NULL)
    return NULL;

  const SgName target_name = symbol->get_name();
  std::vector<SgMemberFunctionDeclaration *> name_matches;
  std::vector<SgMemberFunctionDeclaration *> type_matches;
  std::set<SgMemberFunctionDeclaration *> seen;

  for (SgDeclarationStatement *stmt : *members) {
    SgMemberFunctionDeclaration *candidate =
        canonicalMemberFunctionDeclForRef(isSgMemberFunctionDeclaration(stmt));
    if (candidate == NULL || candidate->get_name() != target_name ||
        !seen.insert(candidate).second)
      continue;

    name_matches.push_back(candidate);
    if (memberFunctionDeclMatchesRefType(candidate, ref))
      type_matches.push_back(candidate);
  }

  if (type_matches.size() == 1)
    return type_matches.front();
  if (name_matches.size() == 1)
    return name_matches.front();
  return NULL;
}

static SgMemberFunctionDeclaration *
resolveTemplateMemberFunctionDeclarationFromRef(
    SgTemplateMemberFunctionRefExp *ref) {
  if (ref == NULL)
    return NULL;

  SgTemplateMemberFunctionSymbol *symbol = ref->get_symbol();
  if (symbol == NULL)
    return NULL;

  return isSgMemberFunctionDeclaration(symbol->get_declaration());
}

static SgMemberFunctionDeclaration *
resolveMemberFunctionDeclarationFromExpression(SgExpression *expr) {
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    return resolveMemberFunctionDeclarationFromRef(ref);
  }

  if (SgTemplateMemberFunctionRefExp *ref =
          isSgTemplateMemberFunctionRefExp(expr)) {
    return resolveTemplateMemberFunctionDeclarationFromRef(ref);
  }

  return NULL;
}

static bool memberFunctionRefNeedsQualifier(SgExpression *expr) {
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    return ref->get_need_qualifier() != 0;
  }

  if (SgTemplateMemberFunctionRefExp *ref =
          isSgTemplateMemberFunctionRefExp(expr)) {
    return ref->get_need_qualifier() != 0;
  }

  return true;
}

static bool is_types_equal(SgType *t1, SgType *t2);

static SgClassType *
getClassTypeFromDeclaration(SgClassDeclaration *class_decl) {
  if (class_decl == NULL) {
    return NULL;
  }

  if (class_decl->get_type() != NULL) {
    return isSgClassType(class_decl->get_type());
  }

  if (SgClassDeclaration *nonDefDecl =
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration())) {
    if (nonDefDecl->get_type() != NULL) {
      return isSgClassType(nonDefDecl->get_type());
    }
  }

  if (SgClassDeclaration *defDecl =
          isSgClassDeclaration(class_decl->get_definingDeclaration())) {
    if (defDecl->get_type() != NULL) {
      return isSgClassType(defDecl->get_type());
    }
  }

  return NULL;
}

static SgClassType *
resolveClassTypeFromMemberDecl(SgMemberFunctionDeclaration *member_decl) {
  if (member_decl == NULL)
    return NULL;

  if (SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
          member_decl->get_firstNondefiningDeclaration())) {
    member_decl = nonDefDecl;
  }

  if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
          isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
    if (SgClassDeclaration *associated_class = isSgClassDeclaration(
            inst_member->get_associatedClassDeclaration())) {
      if (SgClassType *associated_type =
              getClassTypeFromDeclaration(associated_class)) {
        return associated_type;
      }
    }
  }

  SgScopeStatement *scope = member_decl->get_scope();
  if (scope == NULL)
    scope = isSgScopeStatement(member_decl->get_parent());

  if (SgClassDefinition *class_def = isSgClassDefinition(scope))
    return getClassTypeFromDeclaration(class_def->get_declaration());

  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope))
    return getClassTypeFromDeclaration(inst_def->get_declaration());

  if (SgTemplateClassDefinition *tmpl_def = isSgTemplateClassDefinition(scope))
    return getClassTypeFromDeclaration(tmpl_def->get_declaration());

  return NULL;
}

static SgMemberFunctionDeclaration *
enclosingMemberFunctionDeclaration(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (SgFunctionDefinition *def = isSgFunctionDefinition(cursor)) {
      return isSgMemberFunctionDeclaration(def->get_declaration());
    }

    if (SgMemberFunctionDeclaration *decl =
            isSgMemberFunctionDeclaration(cursor)) {
      return decl;
    }
  }

  return NULL;
}

static SgClassDeclaration *
canonicalClassDeclaration(SgClassDeclaration *class_decl) {
  if (class_decl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *nonDefDecl =
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration())) {
    return nonDefDecl;
  }

  return class_decl;
}

static SgClassType *
resolveClassTypeFromScopeLookup(SgScopeStatement *scope,
                                SgNonrealDecl *nonreal_decl) {
  if (scope == NULL || nonreal_decl == NULL) {
    return NULL;
  }

  SgTemplateArgumentPtrList *tpl_args = nonreal_decl->get_tpl_args().empty()
                                            ? NULL
                                            : &nonreal_decl->get_tpl_args();
  SgName symbol_name = nonreal_decl->get_name();
  if (tpl_args != NULL) {
    symbol_name =
        SageBuilder::appendTemplateArgumentsToName(symbol_name, *tpl_args);
  }

  SgClassSymbol *class_symbol =
      scope->lookup_class_symbol(symbol_name, tpl_args);
  if (class_symbol == NULL) {
    class_symbol =
        scope->lookup_class_symbol(nonreal_decl->get_name(), tpl_args);
  }
  if (class_symbol == NULL) {
    return NULL;
  }

  SgClassDeclaration *class_decl = canonicalClassDeclaration(
      isSgClassDeclaration(class_symbol->get_declaration()));
  return getClassTypeFromDeclaration(class_decl);
}

static SgClassType *resolveClassTypeFromType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  if (SgClassType *class_type = isSgClassType(type)) {
    return class_type;
  }

  SgNonrealType *nonreal_type = isSgNonrealType(type);
  if (nonreal_type == NULL) {
    return NULL;
  }

  SgNonrealDecl *nonreal_decl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  if (nonreal_decl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *class_decl =
          isSgClassDeclaration(nonreal_decl->get_templateDeclaration())) {
    if (isSgTemplateClassDeclaration(class_decl) != NULL &&
        (!nonreal_decl->get_tpl_args().empty() ||
         nonreal_decl->get_templateDeclaration() != NULL)) {
      std::vector<SgTemplateParameter *> tpl_params;
      std::vector<SgTemplateArgument *> tpl_args;
      if (SgClassType *instantiated_type =
              isSgClassType(Rose::Builder::Templates::instantiateNonrealTypes(
                  nonreal_type, tpl_params, tpl_args))) {
        return instantiated_type;
      }
    }

    return getClassTypeFromDeclaration(class_decl);
  }

  if (!nonreal_decl->get_tpl_args().empty()) {
    std::set<SgScopeStatement *> visited_scopes;
    for (SgNode *cursor = nonreal_decl->get_scope(); cursor != NULL;
         cursor = cursor->get_parent()) {
      SgScopeStatement *scope = isSgScopeStatement(cursor);
      if (scope == NULL || !visited_scopes.insert(scope).second) {
        continue;
      }

      if (SgClassType *resolved_type =
              resolveClassTypeFromScopeLookup(scope, nonreal_decl)) {
        return resolved_type;
      }
    }
  }

  return NULL;
}

struct TemplateInstantiationAnalysisContext {
  SgTemplateFunctionDeclaration *templateFunction = NULL;
  std::vector<SgTemplateArgument *> templateArguments;

  bool empty() const {
    return templateFunction == NULL || templateArguments.empty();
  }
};

static SgName templateParameterName(SgTemplateParameter *parameter) {
  if (parameter == NULL) {
    return SgName();
  }

  if (SgInitializedName *name = parameter->get_initializedName()) {
    return name->get_name();
  }

  if (SgType *type = parameter->get_type()) {
    if (SgTemplateType *templateType = isSgTemplateType(type)) {
      return templateType->get_name();
    }
  }

  return SgName();
}

static SgTemplateArgument *templateArgumentForParameterName(
    const TemplateInstantiationAnalysisContext *context, const SgName &name) {
  if (context == NULL || context->templateFunction == NULL || name.is_null()) {
    return NULL;
  }

  const SgTemplateParameterPtrList &parameters =
      context->templateFunction->get_templateParameters();
  for (size_t i = 0;
       i < parameters.size() && i < context->templateArguments.size(); ++i) {
    if (templateParameterName(parameters[i]) == name) {
      return context->templateArguments[i];
    }
  }

  return NULL;
}

static SgTemplateArgument *templateArgumentForTemplateType(
    SgTemplateType *type, const TemplateInstantiationAnalysisContext *context) {
  if (type == NULL || context == NULL) {
    return NULL;
  }

  const int position = type->get_template_parameter_position();
  if (position >= 0 &&
      static_cast<size_t>(position) < context->templateArguments.size()) {
    return context->templateArguments[position];
  }

  if (SgTemplateArgument *argument =
          templateArgumentForParameterName(context, type->get_name())) {
    return argument;
  }

  if (SgTemplateParameter *parameter = type->get_template_parameter()) {
    return templateArgumentForParameterName(context,
                                            templateParameterName(parameter));
  }

  return NULL;
}

static SgTemplateArgument *templateArgumentForNonrealType(
    SgNonrealType *type, const TemplateInstantiationAnalysisContext *context) {
  if (type == NULL || context == NULL) {
    return NULL;
  }

  SgNonrealDecl *decl = isSgNonrealDecl(type->get_declaration());
  return decl != NULL
             ? templateArgumentForParameterName(context, decl->get_name())
             : NULL;
}

static SgType *substituteTemplateParameterType(
    SgType *type, const TemplateInstantiationAnalysisContext *context) {
  if (type == NULL || context == NULL || context->empty()) {
    return type;
  }

  SgType *stripped = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
  if (stripped == NULL) {
    return type;
  }

  SgTemplateArgument *argument = NULL;
  if (SgTemplateType *templateType = isSgTemplateType(stripped)) {
    argument = templateArgumentForTemplateType(templateType, context);
  } else if (SgNonrealType *nonrealType = isSgNonrealType(stripped)) {
    argument = templateArgumentForNonrealType(nonrealType, context);
  }

  if (argument != NULL &&
      argument->get_argumentType() == SgTemplateArgument::type_argument &&
      argument->get_type() != NULL) {
    return argument->get_type();
  }

  return type;
}

static TemplateInstantiationAnalysisContext
buildTemplateInstantiationAnalysisContext(
    SgTemplateInstantiationFunctionDecl *instantiation) {
  TemplateInstantiationAnalysisContext context;
  if (instantiation == NULL) {
    return context;
  }

  context.templateFunction =
      isSgTemplateFunctionDeclaration(instantiation->get_templateDeclaration());
  if (context.templateFunction == NULL) {
    return context;
  }

  context.templateArguments = instantiation->get_templateArguments();
  if (context.templateArguments.empty()) {
    context.templateArguments = instantiation->get_deducedTemplateArguments();
  }

  return context;
}

static bool templateArgumentsAreEquivalent(SgTemplateArgument *lhs,
                                           SgTemplateArgument *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  if (lhs->get_argumentType() != rhs->get_argumentType()) {
    return false;
  }

  switch (lhs->get_argumentType()) {
  case SgTemplateArgument::type_argument:
    return lhs->get_type() != NULL && rhs->get_type() != NULL &&
           is_types_equal(skipTypeAliases(lhs->get_type()),
                          skipTypeAliases(rhs->get_type()));

  case SgTemplateArgument::template_template_argument:
    return lhs->get_templateDeclaration() != NULL &&
           lhs->get_templateDeclaration() == rhs->get_templateDeclaration();

  case SgTemplateArgument::nontype_argument:
    return lhs->get_initializedName() != NULL &&
           lhs->get_initializedName() == rhs->get_initializedName();

  case SgTemplateArgument::start_of_pack_expansion_argument:
    return true;

  case SgTemplateArgument::argument_undefined:
  default:
    return false;
  }
}

static bool
templateArgumentListsAreEquivalent(const SgTemplateArgumentPtrList &lhs,
                                   const SgTemplateArgumentPtrList &rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!templateArgumentsAreEquivalent(lhs[i], rhs[i])) {
      return false;
    }
  }

  return true;
}

static SgTemplateInstantiationMemberFunctionDecl *
resolveMemberFunctionInstantiationFromNonrealType(
    SgNonrealType *nonreal_type,
    SgMemberFunctionDeclaration *memberFunctionDeclaration) {
  if (nonreal_type == NULL || memberFunctionDeclaration == NULL) {
    return NULL;
  }

  SgNonrealDecl *nonreal_decl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  if (nonreal_decl == NULL || nonreal_decl->get_tpl_args().empty()) {
    return NULL;
  }

  SgTemplateMemberFunctionDeclaration *template_pattern =
      isSgTemplateMemberFunctionDeclaration(memberFunctionDeclaration);
  if (template_pattern == NULL) {
    return NULL;
  }

  VariantVector variants(V_SgTemplateInstantiationMemberFunctionDecl);
  Rose_STL_Container<SgNode *> instantiations =
      NodeQuery::queryMemoryPool(variants);
  for (Rose_STL_Container<SgNode *>::const_iterator it = instantiations.begin();
       it != instantiations.end(); ++it) {
    SgTemplateInstantiationMemberFunctionDecl *candidate =
        isSgTemplateInstantiationMemberFunctionDecl(*it);
    if (candidate == NULL) {
      continue;
    }

    if (candidate->get_templateDeclaration() != template_pattern &&
        candidate->get_specializedTemplateDeclaration() != template_pattern) {
      continue;
    }

    SgTemplateInstantiationMemberFunctionDecl *candidate_nondef =
        isSgTemplateInstantiationMemberFunctionDecl(
            candidate->get_firstNondefiningDeclaration());
    if (candidate_nondef != NULL) {
      candidate = candidate_nondef;
    }

    SgTemplateInstantiationDecl *associated_class =
        isSgTemplateInstantiationDecl(
            candidate->get_associatedClassDeclaration());
    if (associated_class == NULL) {
      continue;
    }

    if (SgTemplateInstantiationDecl *associated_nondef =
            isSgTemplateInstantiationDecl(
                associated_class->get_firstNondefiningDeclaration())) {
      associated_class = associated_nondef;
    }

    SgName template_name = associated_class->get_templateName();
    if (template_name.is_null() &&
        associated_class->get_templateDeclaration() != NULL) {
      template_name = associated_class->get_templateDeclaration()->get_name();
    }
    if (template_name != nonreal_decl->get_name()) {
      continue;
    }

    if (!templateArgumentListsAreEquivalent(
            associated_class->get_templateArguments(),
            nonreal_decl->get_tpl_args())) {
      continue;
    }

    return candidate;
  }

  return NULL;
}

namespace {
template <class SageNode> SgType *genericUnderType(SageNode *ty) {
  ASSERT_not_null(ty);

  return ty->get_base_type();
}

SgType *getUnderType(SgType *ty) { return NULL /* base case */; }
SgType *getUnderType(SgModifierType *ty) { return genericUnderType(ty); }
SgType *getUnderType(SgPointerType *ty) { return genericUnderType(ty); }
SgType *getUnderType(SgReferenceType *ty) { return genericUnderType(ty); }
SgType *getUnderType(SgRvalueReferenceType *ty) { return genericUnderType(ty); }
SgType *getUnderType(SgArrayType *ty) { return genericUnderType(ty); }
SgType *getUnderType(SgTypedefType *ty) { return genericUnderType(ty); }
} // namespace

static std::vector<SgType *> get_type_vector(SgType *currentType) {
  std::vector<SgType *> returnVector;

  while (true) {
    ASSERT_not_null(currentType);

    returnVector.push_back(currentType);

    if (SgModifierType *modType = isSgModifierType(currentType)) {
      currentType = getUnderType(modType);
    } else if (SgReferenceType *refType = isSgReferenceType(currentType)) {
      currentType = getUnderType(refType);
    } else if (SgRvalueReferenceType *rvRefType =
                   isSgRvalueReferenceType(currentType)) {
      currentType = getUnderType(rvRefType);
    } else if (SgPointerType *pointType = isSgPointerType(currentType)) {
      currentType = getUnderType(pointType);
    } else if (SgArrayType *arrayType = isSgArrayType(currentType)) {
      currentType = getUnderType(arrayType);
    } else if (SgTypedefType *typedefType = isSgTypedefType(currentType)) {
      // DQ (6/21/2005): Added support for typedef types to be uncovered by
      // findBaseType()

      currentType = getUnderType(typedefType);
      returnVector.pop_back(); // PP (29/01/20) typedef types should be used for
                               // comparisons
    } else {
      // \todo PP: templated types, using aliases

      // Exit the while(true){} loop!
      break;
    }
  }

  return returnVector;
};

static bool is_functions_types_equal(SgFunctionType *f1, SgFunctionType *f2);

static bool typeEquality(SgType *, SgType *) { return true; }

static bool typeEquality(SgNamedType *type1, SgNamedType *type2) {
  ASSERT_not_null(type1);
  ASSERT_not_null(type2);

  SgDeclarationStatement &decl1 = SG_DEREF(type1->get_declaration());
  SgDeclarationStatement &decl2 = SG_DEREF(type2->get_declaration());

  return decl1.get_firstNondefiningDeclaration() ==
         decl2.get_firstNondefiningDeclaration();
}

static bool typeEquality(SgFunctionType *type1, SgFunctionType *type2) {
  ASSERT_not_null(type1);
  ASSERT_not_null(type2);

  return is_functions_types_equal(type1, type2);
}

static bool typeEquality(SgModifierType *type1, SgModifierType *type2) {
  ASSERT_not_null(type1);
  ASSERT_not_null(type2);

  bool types_are_equal = true;
  SgTypeModifier &typeModifier1 = type1->get_typeModifier();
  SgTypeModifier &typeModifier2 = type1->get_typeModifier();

  if (typeModifier1.get_modifierVector() != typeModifier2.get_modifierVector())
    types_are_equal = false;

  if (typeModifier1.get_constVolatileModifier().get_modifier() !=
      typeModifier2.get_constVolatileModifier().get_modifier())
    types_are_equal = false;

  if (typeModifier1.get_elaboratedTypeModifier().get_modifier() !=
      typeModifier2.get_elaboratedTypeModifier().get_modifier())
    types_are_equal = false;

  return types_are_equal;
}

static bool is_types_equal(SgType *t1, SgType *t2) {
  if (t1 == t2)
    return true;

  bool types_are_equal = true;
  std::vector<SgType *> f1_vec = get_type_vector(t1);
  std::vector<SgType *> f2_vec = get_type_vector(t2);

  if (f1_vec.size() == f2_vec.size()) {
    for (size_t i = 0; i < f1_vec.size(); i++) {
      if (f1_vec[i]->variantT() == f2_vec[i]->variantT()) {
        // The named types do not point to the same declaration
        if (isSgNamedType(f1_vec[i]) != NULL) {
          if (!typeEquality(isSgNamedType(f1_vec[i]), isSgNamedType(f2_vec[i])))
            types_are_equal = false;
        }

        if (isSgModifierType(f1_vec[i]) != NULL) {
          if (!typeEquality(isSgModifierType(f1_vec[i]),
                            isSgModifierType(f2_vec[i])))
            types_are_equal = false;
        }

        // Function types are not the same
        if (isSgFunctionType(f1_vec[i]) != NULL) {
          if (!typeEquality(isSgFunctionType(f1_vec[i]),
                            isSgFunctionType(f2_vec[i])))
            types_are_equal = false;
        }
      } else {
        // Variant is different
        types_are_equal = false;
      }

      if (types_are_equal == false)
        break;
    }

  } else {
    // The size of the type vectors are not the same
    types_are_equal = false;
  }

  return types_are_equal;
};

static bool is_functions_types_equal(SgFunctionType *f1, SgFunctionType *f2) {
  bool functions_are_equal = false;

  // Optimization: Function type objects are the same for functions that
  // have exactly the same signature
  if (f1 == f2)
    return true;

  // See if the function types match
  if (is_types_equal(f1->get_return_type(), f2->get_return_type())) {
    SgTypePtrList &args_f1 = f1->get_arguments();
    SgTypePtrList &args_f2 = f2->get_arguments();

    // See if the arguments match

    if (args_f1.size() == args_f2.size()) {
      functions_are_equal = true;

      for (size_t i = 0; i < args_f1.size(); i++) {
        if (is_types_equal(args_f1[i], args_f2[i]) == false) {
          functions_are_equal = false;
          break;
        }
      }
    } // Different number of arguments
  }

  // std::cout << "is_functions_types_equal: " << f1->unparseToString() << " "
  // << f2->unparseToString() <<  ( functions_are_equal == true ? " true " : "
  // false " ) << std::endl;

  return functions_are_equal;
}

struct CovarianceChecker : sg::DispatchHandler<bool> {
  explicit CovarianceChecker(SgType &baseType,
                             ClassHierarchyWrapper *hierarchy = NULL)
      : base(&baseType), chw(hierarchy) {}

  template <class T>
  static bool typeChk(T &derived, T &base, ClassHierarchyWrapper *) {
    return ::typeEquality(&derived, &base);
  }

  static bool typeChk(SgNamedType &derived, SgNamedType &base,
                      ClassHierarchyWrapper *chw) {
    typedef ClassHierarchyWrapper::ClassDefSet ClassDefSet;

    const bool sameTypes = ::typeEquality(&derived, &base);
    if (!chw || sameTypes)
      return sameTypes;

    SgClassType *derivedClass = isSgClassType(&derived);
    if (!derivedClass)
      return false;

    SgClassType *baseClass = isSgClassType(&base);
    if (!baseClass)
      return false;

    SgClassDeclaration &derivedDcl0 = SG_ASSERT_TYPE(
        SgClassDeclaration, SG_DEREF(derivedClass->get_declaration()));
    SgClassDeclaration &baseDcl0 = SG_ASSERT_TYPE(
        SgClassDeclaration, SG_DEREF(baseClass->get_declaration()));
    SgClassDeclaration *derivedDcl =
        isSgClassDeclaration(derivedDcl0.get_definingDeclaration());
    SgClassDeclaration *baseDcl =
        isSgClassDeclaration(baseDcl0.get_definingDeclaration());
    if (!derivedDcl || !baseDcl)
      return false;

    SgClassDefinition *derivedDef = derivedDcl->get_definition();
    SgClassDefinition *baseDef = baseDcl->get_definition();
    const ClassDefSet &ancestors = chw->getAncestorClasses(derivedDef);

    return ancestors.find(baseDef) != ancestors.end();
  }

  template <class T>
  static bool underChk(T &derived, T &base, ClassHierarchyWrapper *chw) {
    SgType *derived_base = getUnderType(&derived);

    // if there is no underlying type and everything matched up to
    //   here, we have found a winner.
    if (!derived_base)
      return true;

    return isCovariantType(derived_base, &base, chw);
  }

  ReturnType descend(SgType *ty) const { return sg::dispatch(*this, ty); }

  /// generic template routine to check for covariance
  /// if @chw is null, the check tests for strict equality
  template <class T>
  bool check(T &derivedTy, ClassHierarchyWrapper *nextChw = NULL) {
    base = skipTypeAliases(base);

    // symmetric check for strict type equality
    if (typeid(derivedTy) != typeid(*base))
      return false;

    T &baseTy = SG_ASSERT_TYPE(T, *base);

    if (&derivedTy == &baseTy)
      return true;

    return (typeChk(derivedTy, baseTy, nextChw) &&
            underChk(derivedTy, baseTy, nextChw));
  }

  void handle(SgNode &derived) { SG_UNEXPECTED_NODE(derived); }

  void handle(SgType &derived) { res = check(derived); }

  // skip typedefs
  void handle(SgTypedefType &derived) { res = descend(getUnderType(&derived)); }

  // check equality of underlying types
  // @{
  void handle(SgPointerType &derived) { res = check(derived); }
  void handle(SgArrayType &derived) { res = check(derived); }
  // @}

  // covariance is only maintained through classes and modifiers
  // @{
  void handle(SgNamedType &derived) { res = check(derived, chw); }
  void handle(SgModifierType &derived) { res = check(derived, chw); }
  // @}

  // should have been removed by PolymorphicRootsFinder
  // @{
  void handle(SgReferenceType &derived) { SG_UNEXPECTED_NODE(derived); }
  void handle(SgRvalueReferenceType &derived) { SG_UNEXPECTED_NODE(derived); }
  // @}

private:
  SgType *base;
  ClassHierarchyWrapper *chw;
};

struct PolymorphicRootsFinder
    : sg::DispatchHandler<std::pair<SgType *, SgType *>> {
  explicit PolymorphicRootsFinder(SgType &baseType) : base(&baseType) {}

  static bool typeChk(SgType &derived, SgType &base) { return true; }

  static bool typeChk(SgModifierType &derived, SgModifierType &base) {
    return ::typeEquality(&derived, &base);
  }

  ReturnType descend(SgType *ty) const { return sg::dispatch(*this, ty); }

  /// generic template routine to check for covariance
  /// if @chw is null, the check tests for strict equality
  template <class T> ReturnType check(T &derivedTy) {
    ReturnType res(NULL, NULL);

    base = skipTypeAliases(base);

    // symmetric check for strict type equality
    if (derivedTy.variantT() != (*base).variantT())
      return res;

    T &baseTy = SG_ASSERT_TYPE(T, *base);

    if (typeChk(derivedTy, baseTy)) {
      res = ReturnType(getUnderType(&derivedTy), getUnderType(&baseTy));
    }

    return res;
  }

  void handle(SgNode &derived) { SG_UNEXPECTED_NODE(derived); }

  void handle(SgType &derived) {}

  // skip typedefs
  void handle(SgTypedefType &derived) { res = descend(getUnderType(&derived)); }

  // the modifiers must equal
  void handle(SgModifierType &derived) { res = check(derived); }

  // polymorphic root types (must also equal)
  // @{
  void handle(SgReferenceType &derived) { res = check(derived); }
  void handle(SgRvalueReferenceType &derived) { res = check(derived); }
  void handle(SgPointerType &derived) { res = check(derived); }
  // @}
private:
  SgType *base;
};

/// tests if @derived is a covariant type of @base
/// \param derived a non-null type
/// \param base    a non-null type
/// \param chw     the "unrolled" class hierarchy information
/// \brief if chw is NULL, strict type equality is checked
static bool isCovariantType(SgType *derived, SgType *base,
                            ClassHierarchyWrapper *chw) {
  // find polymorphic root (e.g., reference, pointer)
  std::pair<SgType *, SgType *> rootTypes =
      sg::dispatch(PolymorphicRootsFinder(SG_DEREF(base)), derived);

  if (!rootTypes.first)
    return sg::dispatch(CovarianceChecker(SG_DEREF(base)), derived);

  // test if the roots are covariant
  return sg::dispatch(CovarianceChecker(SG_DEREF(rootTypes.second), chw),
                      rootTypes.first);
}

/// returns @derived is an overriding type of @base
/// \param derived a function type of an overrider candidate
/// \param base    a function type
/// \param chw     a class hierarchy
/// \pre derived != NULL && base != NULL
/// \details
///    isOverridingType checks if derived overrides base, meaning
///    @derived's arguments must equal @base's argument, and @derived's
///    return type may be covariabt with respect to @base's return type.
static bool isOverridingType(SgMemberFunctionType *derived,
                             SgMemberFunctionType *base,
                             ClassHierarchyWrapper *chw) {
  ASSERT_not_null(derived);
  ASSERT_not_null(base);

  if (derived == base)
    return true;

  if (derived->get_mfunc_specifier() != base->get_mfunc_specifier()
      //~ || derived->get_ref_qualifiers()  != base->get_ref_qualifiers()
      || derived->get_has_ellipses() != base->get_has_ellipses()) {
    return false;
  }

  if (!isCovariantType(derived->get_return_type(), base->get_return_type(),
                       chw))
    return false;

  SgTypePtrList &args_derived = derived->get_arguments();
  SgTypePtrList &args_base = base->get_arguments();

  // See if the arguments match
  if (args_derived.size() != args_base.size())
    return false;

  SgTypePtrList::iterator derived_end = args_derived.end();

  return std::mismatch(args_derived.begin(), derived_end, args_base.begin(),
                       is_types_equal)
             .first == derived_end;
}

/// tests if derived overrides base
static bool isOverridingFunction(SgMemberFunctionDeclaration *candidate,
                                 SgMemberFunctionDeclaration *member,
                                 ClassHierarchyWrapper *chw) {
  ASSERT_not_null(candidate);
  ASSERT_not_null(member);

  return (candidate->get_name() == member->get_name() &&
          isOverridingType(isSgMemberFunctionType(candidate->get_type()),
                           isSgMemberFunctionType(member->get_type()), chw));
}

SgFunctionDeclaration *
is_function_exists(SgClassDefinition *cls,
                   SgMemberFunctionDeclaration *memberFunctionDeclaration) {
  SgFunctionDeclaration *resultDecl = NULL;
  string f1 = memberFunctionDeclaration->get_name().getString();
  string f2;

  SgDeclarationStatementPtrList &clsMembers = cls->get_members();
  for (SgDeclarationStatement *cls_mb : clsMembers) {
    SgMemberFunctionDeclaration *cls_mb_decl =
        isSgMemberFunctionDeclaration(cls_mb);
    if (cls_mb_decl == NULL)
      continue;

    ASSERT_not_null(cls_mb_decl);
    SgMemberFunctionType *funcType1 =
        isSgMemberFunctionType(memberFunctionDeclaration->get_type());
    SgMemberFunctionType *funcType2 =
        isSgMemberFunctionType(cls_mb_decl->get_type());
    f2 = cls_mb_decl->get_name().getString();

    if (f1 != f2)
      continue;
    if (funcType1 == NULL || funcType2 == NULL)
      continue;
    if (is_functions_types_equal(funcType1, funcType2)) {
      SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
          cls_mb_decl->get_firstNondefiningDeclaration());
      SgMemberFunctionDeclaration *defDecl =
          isSgMemberFunctionDeclaration(cls_mb_decl->get_definingDeclaration());

      // ROSE_ASSERT ( (!nonDefDecl && defDecl == cls_mb_decl) || (nonDefDecl ==
      // cls_mb_decl && nonDefDecl) );

      resultDecl = (nonDefDecl) ? nonDefDecl : defDecl;
      ASSERT_not_null(resultDecl);
      if (!(resultDecl->get_functionModifier().isPureVirtual())) {
        return resultDecl;
        // resultDecl = functionDeclarationInClass;
        // functionList.push_back( functionDeclarationInClass );
      }
    }
  }

  return canonicalCallableFunctionDecl(resultDecl);
}

bool dummyFilter::operator()(SgFunctionDeclaration *node) const {
  return canonicalCallableFunctionDecl(node) != NULL;
};

bool builtinFilter::operator()(SgFunctionDeclaration *funcDecl) const {
  funcDecl = canonicalCallableFunctionDecl(funcDecl);
  if (funcDecl == NULL) {
    return false;
  }

  bool returnValue = true;
  string filename = funcDecl->get_file_info()->get_filename();
  std::string func_name = funcDecl->get_name().getString();
  string stripped_file_name = StringUtility::stripPathFromFileName(filename);
  // string::size_type loc;

  // Filter out functions from the ROSE preinclude header file
  if (filename.find("rose_required_macros_and_functions") != string::npos)
    returnValue = false;
  // Filter out compiler generated functions
  else if (funcDecl->get_file_info()->isCompilerGenerated() == true)
    returnValue = false;
  // Filter out compiler generated functions
  else if (funcDecl->get_file_info()->isFrontendSpecific() == true)
    returnValue = false;
  // filter out other built in functions
  //      else if( func_name.find ("__",0)== 0);
  //         returnValue = false;
  // _IO_getc _IO_putc _IO_feof, etc.
  // loc = func_name.find ("_IO_",0);
  // if (loc == 0 ) returnValue = false;

  // skip functions from standard system headers
  // TODO Need more rigid check
  else if (stripped_file_name == string("assert.h") ||
           stripped_file_name == string("complex.h") ||
           stripped_file_name == string("ctype.h") ||
           stripped_file_name == string("errno.h") ||
           stripped_file_name == string("float.h") ||

           stripped_file_name == string("limits.h") ||
           stripped_file_name == string("locale.h") ||
           stripped_file_name == string("math.h") ||
           stripped_file_name == string("setjmp.h") ||
           stripped_file_name == string("signal.h") ||

           stripped_file_name == string("stdarg.h") ||
           stripped_file_name == string("stddef.h") ||

           stripped_file_name == string("stdio.h") ||
           stripped_file_name == string("stdlib.h") ||
           stripped_file_name == string("string.h") ||
           stripped_file_name == string("time.h") ||

           // GCC specific ???
           stripped_file_name == string("libio.h") ||
           stripped_file_name == string("select.h") ||
           stripped_file_name == string("mathcalls.h"))
    returnValue = false;
  if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
    cout << "Debug: CallGraph.C ... " << func_name
         << " from file:" << stripped_file_name
         << " predicate function returns: " << returnValue << endl;
  return returnValue;
}

bool FunctionData::isDefined() { return hasDefinition; }

CallGraphBuilder::CallGraphBuilder(SgProject *proj) {
  project = proj;
  graph = NULL;
}

SgIncidenceDirectedGraph *CallGraphBuilder::getGraph() { return graph; }

/**
 * CallTargetSet::solveFunctionPointerCallsFunctional
 *
 * \brief Checks if the functionDeclaration (node) matches functionType
 *
 * This is a filter called by solveFunctionPointerCall. It checks that node is
 * a functiondeclaration (or template instantiation) of type functionType.
 * If it does, it is added to a functionList and returned.  So function list can
 * have at most 1 entry.
 *
 * @param[in] node : The node we are checking.  It must be an
 *SgFunctionDeclaration
 * @param[in] functionType : The function type being checked.
 * @return: If node matched functionType, it is added on functionList and
 *returned.  Otherwise functionList is empty.
 **/
Rose_STL_Container<SgFunctionDeclaration *>
CallTargetSet::solveFunctionPointerCallsFunctional(
    SgNode *node, SgFunctionType *functionType) {
  ASSERT_not_null(functionType);

  Rose_STL_Container<SgFunctionDeclaration *> functionList;

  SgFunctionDeclaration *fctDecl =
      canonicalCallableFunctionDecl(isSgFunctionDeclaration(node));
  if (fctDecl == NULL) {
    return functionList;
  }

  // Find all function declarations which is both first non-defining declaration
  // and has a mangled name which is equal to the mangled name of 'functionType'
  if (functionType->get_mangled().getString() ==
      fctDecl->get_type()->get_mangled().getString()) {
    functionList.push_back(fctDecl);
  }
  return functionList;
}

/**
 * CallTargetSet::solveFunctionPointerCall
 *
 * \brief Finds all functions that match the function type of pointerDerefExp
 *
 * Resolving function pointer calls is hard, so the CallGraph generator doesn't
 * try very hard at it.  When asked to resolve a function pointer call, it
 *simply finds all functions that match that type in the memory pool a returns a
 *list of them.
 *
 * @param[in] pointerDerefExp : A function pointer dereference.
 * @return: A vector of all functionDeclarations that match the type of the
 *function dereferenced in pointerDerefExp
 **/
std::vector<SgFunctionDeclaration *>
CallTargetSet::solveFunctionPointerCall(SgPointerDerefExp *pointerDerefExp) {
  SgFunctionType *fctType =
      isSgFunctionType(pointerDerefExp->get_type()->findBaseType());
  ASSERT_not_null(fctType);

  // SgUnparse_Info ui;
  // string type1str = fctType->get_mangled( ui ).str();
  // string type1str = fctType->get_mangled().str();
  // cout << "Return type of function pointer " << type1str << "\n";
  // cout << " Line: " << pointerDerefExp->get_file_info()->get_filenameString()
  // <<
  //  " l" << pointerDerefExp->get_file_info()->get_line() <<
  //  " c" << pointerDerefExp->get_file_info()->get_col()  << std::endl;
  // getting all possible functions with the same type

  // DQ (1/31/2006): Changed name and made global function type symbol table a
  // static data member.
  ASSERT_not_null(SgNode::get_globalFunctionTypeTable());

  // if there are multiple forward declarations of the same function
  // there will be multiple nodes in the AST containing them
  // but just one link in the call graph

  // AS (09/23/06) Query the memory pool instead of subtree of project
  // AS (10/2/06)  Modified query to only query for functions or function
  // templates
  VariantVector vv;
  vv.push_back(V_SgFunctionDeclaration);
  vv.push_back(V_SgTemplateInstantiationFunctionDecl);

  // Replaced deprecated functions std::bind2nd and std::ptr_fun [Rasmussen,
  // 2023.08.07]
  std::function<Rose_STL_Container<SgFunctionDeclaration *>(SgNode *,
                                                            SgFunctionType *)>
      ptrFun = solveFunctionPointerCallsFunctional;

  return AstQueryNamespace::queryMemoryPool(
      std::bind(ptrFun, std::placeholders::_1, fctType), &vv);
}

std::vector<SgFunctionDeclaration *>
CallTargetSet::solveMemberFunctionPointerCall(
    SgExpression *functionExp, ClassHierarchyWrapper *classHierarchy) {
  ASSERT_require(isSgArrowStarOp(functionExp) || isSgDotStarOp(functionExp));

  SgBinaryOp *binaryExp = isSgBinaryOp(functionExp);

  SgExpression *left = NULL, *right = NULL;
  SgClassType *classType = NULL;
  SgClassDefinition *classDefinition = NULL;
  std::vector<SgFunctionDeclaration *> functionList;
  SgMemberFunctionType *memberFunctionType = NULL;

  left = binaryExp->get_lhs_operand();
  right = binaryExp->get_rhs_operand();
  ASSERT_not_null(left->get_type());

  SgType *leftBase = left->get_type()->findBaseType();
  SgType *rightType = right->get_type();
  SgType *rightBase = rightType != NULL ? rightType->findBaseType() : NULL;
  const bool dependentMemberPointerType =
      isSgTemplateType(rightBase) || isSgNonrealType(rightBase);
  if (isSgTypeUnknown(rightBase)) {
    ASSERT_require(functionList.empty());
    return functionList;
  }

  classType = isSgClassType(leftBase);

  if (!dependentMemberPointerType) {
    // right side of the concrete expression should have member function type
    memberFunctionType = memberFunctionTypeFromMemberPointerType(rightType);
    ASSERT_not_null(memberFunctionType);

    if (SgPointerMemberType *memberPointerType =
            pointerMemberTypeFromType(rightType)) {
      if (SgClassDeclaration *memberPointerClassDecl = isSgClassDeclaration(
              memberPointerType->get_class_declaration_of())) {
        if (SgClassDeclaration *memberPointerDefiningDecl =
                isSgClassDeclaration(
                    memberPointerClassDecl->get_definingDeclaration())) {
          if (memberPointerDefiningDecl->get_definition() != NULL) {
            classType = getClassTypeFromDeclaration(memberPointerClassDecl);
            classDefinition = memberPointerDefiningDecl->get_definition();
          }
        }
      }
    }
  }

  // In CFE-built ASTs, expressions that produce the object for `.*`/`->*`
  // can retain a dependent/nonreal type after template-library traversal
  // (e.g. `vector<T>::iterator::operator*`). The member-pointer type still
  // carries the exact declaring class, so use it as the semantic owner.
  if (classType == NULL) {
    ASSERT_require(functionList.empty());
    return functionList;
  }
  ASSERT_not_null(classType->get_declaration());
  ASSERT_not_null(classType->get_declaration()->get_definingDeclaration());

  SgClassDeclaration *definingClassDeclaration = isSgClassDeclaration(
      classType->get_declaration()->get_definingDeclaration());
  ASSERT_not_null(definingClassDeclaration);

  classDefinition = definingClassDeclaration->get_definition();
  ASSERT_not_null(classDefinition);

  SgDeclarationStatementPtrList &allMembers = classDefinition->get_members();
  for (SgDeclarationStatementPtrList::iterator it = allMembers.begin();
       it != allMembers.end(); it++) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        isSgMemberFunctionDeclaration(*it);
    if (memberFunctionDeclaration) {
      SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
          memberFunctionDeclaration->get_firstNondefiningDeclaration());
      if (nonDefDecl)
        memberFunctionDeclaration = nonDefDecl;

      // FIXME: Make this use the is_functions_types_equal function
      if (dependentMemberPointerType ||
          is_functions_types_equal(
              isSgMemberFunctionType(memberFunctionDeclaration->get_type()),
              memberFunctionType)) {
        if (!(memberFunctionDeclaration->get_functionModifier()
                  .isPureVirtual())) {
          functionList.push_back(memberFunctionDeclaration);
        }
      }

      // for virtual functions in polymorphic calls, we need to search down in
      // the hierarchy of classes and retrieve all declarations of member
      // functions with the same type
      if ((memberFunctionDeclaration->get_functionModifier().isVirtual() ||
           memberFunctionDeclaration->get_functionModifier().isPureVirtual())) {
        const ClassHierarchyWrapper::ClassDefSet &subclasses =
            classHierarchy->getSubclasses(classDefinition);
        // cout << "Virtual function " <<
        // memberFunctionDeclaration->get_mangled_name().str() << "\n";
        for (ClassHierarchyWrapper::ClassDefSet::const_iterator it_cls =
                 subclasses.begin();
             it_cls != subclasses.end(); it_cls++) {
          SgClassDefinition *cls = isSgClassDefinition(*it_cls);
          SgDeclarationStatementPtrList &clsMembers = cls->get_members();

          for (SgDeclarationStatementPtrList::iterator it_cls_mb =
                   clsMembers.begin();
               it_cls_mb != clsMembers.end(); it_cls_mb++) {
            SgMemberFunctionDeclaration *cls_mb_decl =
                isSgMemberFunctionDeclaration(*it_cls_mb);

            // TV (10/26/2018): cannot expect that all class members would be
            // methods FIXME ROSE-1487
            if (cls_mb_decl == NULL)
              continue;

            if (dependentMemberPointerType ||
                is_functions_types_equal(
                    isSgMemberFunctionType(
                        memberFunctionDeclaration->get_type()),
                    isSgMemberFunctionType(cls_mb_decl->get_type()))) {
              SgMemberFunctionDeclaration *nonDefDecl =
                  isSgMemberFunctionDeclaration(
                      cls_mb_decl->get_firstNondefiningDeclaration());
              SgMemberFunctionDeclaration *defDecl =
                  isSgMemberFunctionDeclaration(
                      cls_mb_decl->get_definingDeclaration());

              // TV (10/26/2018): this case happens in generated docs, not sure
              // it is valid... FIXME ROSE-1487 ROSE_ASSERT((!nonDefDecl &&
              // defDecl == cls_mb_decl) || (nonDefDecl == cls_mb_decl &&
              // nonDefDecl));

              if (nonDefDecl) {
                if (!(nonDefDecl->get_functionModifier().isPureVirtual()) &&
                    nonDefDecl->get_functionModifier().isVirtual()) {
                  functionList.push_back(nonDefDecl);
                }
              } else if (!(defDecl->get_functionModifier().isPureVirtual()) &&
                         defDecl->get_functionModifier().isVirtual()) {
                functionList.push_back(defDecl);
              }
            }
          }
        }
      }
    }
  }

  // cout << "Function list size: " << functionList.size() << "\n";
  return functionList;
}

static bool isPureVirtual(SgMemberFunctionDeclaration *dcl) {
  ASSERT_not_null(dcl);

  return dcl->get_functionModifier().isPureVirtual();
}

static bool callGraphDeclFileInfoIsUsable(SgDeclarationStatement *decl) {
  if (decl == NULL || decl->get_file_info() == NULL) {
    return false;
  }

  Sg_File_Info *fileInfo = decl->get_file_info();
  const std::string filename = fileInfo->get_filename();
  return !filename.empty() && filename != "NULL_FILE" &&
         filename != "compilerGenerated" && !fileInfo->isCompilerGenerated() &&
         !fileInfo->isFrontendSpecific();
}

static std::string
callGraphUsableDeclarationFilename(SgDeclarationStatement *decl) {
  if (!callGraphDeclFileInfoIsUsable(decl)) {
    return "";
  }

  return decl->get_file_info()->get_filename();
}

static bool callGraphProjectContainsSourceFile(SgProject *project,
                                               const std::string &filename) {
  if (project == NULL || filename.empty()) {
    return false;
  }

  for (SgFile *file : project->get_fileList()) {
    if (file == NULL) {
      continue;
    }

    if (filename == file->getFileName() ||
        filename == file->get_sourceFileNameWithPath()) {
      return true;
    }
  }

  return false;
}

static SgClassDeclaration *
callGraphAssociatedClassDeclaration(SgMemberFunctionDeclaration *memberDecl) {
  if (memberDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *associatedClass =
          isSgClassDeclaration(memberDecl->get_associatedClassDeclaration())) {
    return associatedClass;
  }

  if (SgClassDefinition *classDefinition =
          isSgClassDefinition(memberDecl->get_scope())) {
    return classDefinition->get_declaration();
  }

  if (SgClassDefinition *classDefinition =
          isSgClassDefinition(memberDecl->get_parent())) {
    return classDefinition->get_declaration();
  }

  return NULL;
}

static SgTemplateInstantiationDecl *
callGraphEnclosingTemplateInstantiationDeclaration(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (SgTemplateInstantiationDefn *instDefn =
            isSgTemplateInstantiationDefn(cursor)) {
      return isSgTemplateInstantiationDecl(instDefn->get_declaration());
    }
    if (SgTemplateInstantiationDecl *instDecl =
            isSgTemplateInstantiationDecl(cursor)) {
      return instDecl;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return NULL;
    }
  }

  return NULL;
}

static std::string
callGraphClassDeclarationSourceFilename(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return "";
  }

  if (std::string filename = callGraphUsableDeclarationFilename(classDecl);
      !filename.empty()) {
    return filename;
  }

  if (SgClassDeclaration *firstNondef =
          isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(firstNondef);
        !filename.empty()) {
      return filename;
    }
  }

  if (SgClassDeclaration *definingDecl =
          isSgClassDeclaration(classDecl->get_definingDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(definingDecl);
        !filename.empty()) {
      return filename;
    }
  }

  return "";
}

static std::string
callGraphInstantiatedClassPatternSourceFilename(SgClassDeclaration *classDecl) {
  SgTemplateInstantiationDecl *instDecl =
      callGraphEnclosingTemplateInstantiationDeclaration(classDecl);
  if (instDecl == NULL || instDecl->get_templateDeclaration() == NULL) {
    return "";
  }

  return callGraphClassDeclarationSourceFilename(
      isSgClassDeclaration(instDecl->get_templateDeclaration()));
}

static bool
callGraphMemberFunctionIsDefaultConstructor(SgMemberFunctionDeclaration *decl) {
  if (decl == NULL || !decl->get_specialFunctionModifier().isConstructor()) {
    return false;
  }

  for (SgInitializedName *param : decl->get_args()) {
    if (param != NULL && param->get_initializer() == NULL) {
      return false;
    }
  }

  return true;
}

static bool
callGraphMemberFunctionIsUserProvided(SgMemberFunctionDeclaration *decl) {
  if (decl == NULL || decl->get_file_info() == NULL) {
    return false;
  }

  Sg_File_Info *fileInfo = decl->get_file_info();
  return !fileInfo->isCompilerGenerated() && !fileInfo->isFrontendSpecific();
}

static SgClassDeclaration *
callGraphDefiningClassDeclaration(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return NULL;
  }

  SgClassDeclaration *definingDecl =
      isSgClassDeclaration(classDecl->get_definingDeclaration());
  return definingDecl != NULL ? definingDecl : classDecl;
}

static SgClassDeclaration *
callGraphClassDeclarationFromClassType(SgClassType *classType) {
  if (classType == NULL) {
    return NULL;
  }

  return callGraphDefiningClassDeclaration(
      isSgClassDeclaration(classType->get_declaration()));
}

static SgClassDeclaration *
callGraphClassDeclarationFromNonrealDecl(SgNonrealDecl *nonrealDecl) {
  if (nonrealDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *decl = callGraphClassDeclarationFromClassType(
          resolveClassTypeFromType(nonrealDecl->get_type()))) {
    return decl;
  }

  if (!nonrealDecl->get_tpl_args().empty()) {
    for (SgNode *cursor = nonrealDecl->get_scope(); cursor != NULL;
         cursor = cursor->get_parent()) {
      SgScopeStatement *scope = isSgScopeStatement(cursor);
      if (scope == NULL) {
        continue;
      }
      if (SgClassDeclaration *decl = callGraphClassDeclarationFromClassType(
              resolveClassTypeFromScopeLookup(scope, nonrealDecl))) {
        return decl;
      }
    }
  }

  return callGraphDefiningClassDeclaration(
      isSgClassDeclaration(nonrealDecl->get_templateDeclaration()));
}

static SgClassDeclaration *
callGraphBaseClassDeclaration(SgBaseClass *baseClass) {
  if (baseClass == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *decl = baseClass->get_base_class()) {
    return callGraphDefiningClassDeclaration(decl);
  }

  if (SgNonrealBaseClass *nonrealBase = isSgNonrealBaseClass(baseClass)) {
    return callGraphClassDeclarationFromNonrealDecl(
        nonrealBase->get_base_class_nonreal());
  }

  return NULL;
}

static SgClassDeclaration *
callGraphCanonicalClassDeclaration(SgClassDeclaration *classDecl) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *firstNondef =
          isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration())) {
    return firstNondef;
  }

  return classDecl;
}

static SgType *callGraphStripClassValueType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  return type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
}

static SgClassDeclaration *
callGraphClassDeclarationFromValueType(SgType *type) {
  type = callGraphStripClassValueType(type);
  if (type == NULL) {
    return NULL;
  }

  SgClassType *classType = isSgClassType(type);
  if (classType == NULL) {
    return NULL;
  }

  return callGraphCanonicalClassDeclaration(
      isSgClassDeclaration(classType->get_declaration()));
}

static bool callGraphSameClassDeclaration(SgClassDeclaration *lhs,
                                          SgClassDeclaration *rhs) {
  lhs = callGraphCanonicalClassDeclaration(lhs);
  rhs = callGraphCanonicalClassDeclaration(rhs);
  return lhs != NULL && lhs == rhs;
}

static SgMemberFunctionDeclaration *
callGraphCanonicalMemberFunctionDeclaration(SgMemberFunctionDeclaration *decl) {
  if (decl == NULL) {
    return NULL;
  }

  if (SgMemberFunctionDeclaration *firstNondef = isSgMemberFunctionDeclaration(
          decl->get_firstNondefiningDeclaration())) {
    return firstNondef;
  }

  if (SgMemberFunctionDeclaration *definingDecl =
          isSgMemberFunctionDeclaration(decl->get_definingDeclaration())) {
    return definingDecl;
  }

  return decl;
}

static SgClassDeclaration *
callGraphCopyConstructorParameterClass(SgInitializedName *param) {
  if (param == NULL || param->get_type() == NULL) {
    return NULL;
  }

  SgType *paramType = param->get_type()->stripType(SgType::STRIP_MODIFIER_TYPE |
                                                   SgType::STRIP_TYPEDEF_TYPE);
  SgReferenceType *refType = isSgReferenceType(paramType);
  if (refType == NULL) {
    return NULL;
  }

  return callGraphClassDeclarationFromValueType(refType->get_base_type());
}

static bool callGraphTrailingConstructorParametersAreDefaulted(
    const SgInitializedNamePtrList &params) {
  for (size_t i = 1; i < params.size(); ++i) {
    SgInitializedName *param = params[i];
    if (param != NULL && param->get_initializer() == NULL) {
      return false;
    }
  }

  return true;
}

static SgMemberFunctionDeclaration *
callGraphFindCopyConstructorForClass(SgClassDeclaration *classDecl) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL || classDecl->get_definition() == NULL) {
    return NULL;
  }

  SgClassDeclaration *canonicalClass =
      callGraphCanonicalClassDeclaration(classDecl);
  for (SgDeclarationStatement *member :
       classDecl->get_definition()->get_members()) {
    SgMemberFunctionDeclaration *memberFunction =
        isSgMemberFunctionDeclaration(member);
    if (memberFunction == NULL ||
        !memberFunction->get_specialFunctionModifier().isConstructor()) {
      continue;
    }

    const SgInitializedNamePtrList &params = memberFunction->get_args();
    if (params.empty() ||
        !callGraphTrailingConstructorParametersAreDefaulted(params)) {
      continue;
    }

    if (callGraphSameClassDeclaration(
            callGraphCopyConstructorParameterClass(params.front()),
            canonicalClass)) {
      return callGraphCanonicalMemberFunctionDeclaration(memberFunction);
    }
  }

  return NULL;
}

static SgMemberFunctionDeclaration *
callGraphResolveSameClassCopyConstructor(SgConstructorInitializer *ctorInit) {
  if (ctorInit == NULL || ctorInit->get_declaration() != NULL) {
    return NULL;
  }

  SgClassDeclaration *targetClass = ctorInit->get_class_decl();
  if (targetClass == NULL) {
    targetClass =
        callGraphClassDeclarationFromValueType(ctorInit->get_expression_type());
  }
  if (targetClass == NULL) {
    return NULL;
  }

  SgExprListExp *args = isSgExprListExp(ctorInit->get_args());
  if (args == NULL || args->get_expressions().size() != 1) {
    return NULL;
  }

  SgExpression *sourceExpression = args->get_expressions().front();
  if (sourceExpression == NULL ||
      !callGraphSameClassDeclaration(
          callGraphClassDeclarationFromValueType(sourceExpression->get_type()),
          targetClass)) {
    return NULL;
  }

  return callGraphFindCopyConstructorForClass(targetClass);
}

static SgClassDeclaration *
callGraphConstructedClassDeclarationFromType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  type = type->stripTypedefsAndModifiers();
  while (SgArrayType *arrayType = isSgArrayType(type)) {
    type = arrayType->get_base_type();
    if (type != NULL) {
      type = type->stripTypedefsAndModifiers();
    }
  }

  if (type == NULL || isSgPointerType(type) != NULL ||
      isSgReferenceType(type) != NULL ||
      isSgRvalueReferenceType(type) != NULL) {
    return NULL;
  }

  SgClassType *classType = isSgClassType(type);
  if (classType == NULL) {
    return NULL;
  }

  return callGraphDefiningClassDeclaration(
      isSgClassDeclaration(classType->get_declaration()));
}

static bool callGraphClassHasNontrivialDefaultConstruction(
    SgClassDeclaration *classDecl, std::set<SgClassDeclaration *> &active);

static bool
callGraphClassDefinitionHasVirtualMember(SgClassDefinition *classDefinition) {
  if (classDefinition == NULL) {
    return false;
  }

  for (SgDeclarationStatement *member : classDefinition->get_members()) {
    SgMemberFunctionDeclaration *memberFunction =
        isSgMemberFunctionDeclaration(member);
    if (memberFunction == NULL) {
      continue;
    }

    if (memberFunction->get_functionModifier().isVirtual() ||
        memberFunction->get_functionModifier().isPureVirtual()) {
      return true;
    }
  }

  for (SgBaseClass *baseClass : classDefinition->get_inheritances()) {
    SgClassDeclaration *baseDecl = callGraphBaseClassDeclaration(baseClass);
    SgClassDeclaration *baseDefiningDecl =
        callGraphDefiningClassDeclaration(baseDecl);
    if (baseDefiningDecl != NULL && callGraphClassDefinitionHasVirtualMember(
                                        baseDefiningDecl->get_definition())) {
      return true;
    }
  }

  return false;
}

static bool callGraphClassHasVirtualMember(SgClassDeclaration *classDecl) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL) {
    return false;
  }

  return callGraphClassDefinitionHasVirtualMember(classDecl->get_definition());
}

static bool
callGraphDefaultConstructorIsUserProvided(SgClassDeclaration *classDecl) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL) {
    return false;
  }

  SgMemberFunctionDeclaration *defaultConstructor =
      SageInterface::getDefaultConstructor(classDecl);
  return callGraphMemberFunctionIsDefaultConstructor(defaultConstructor) &&
         callGraphMemberFunctionIsUserProvided(defaultConstructor);
}

static bool callGraphClassHasNontrivialDefaultConstructedSubobject(
    SgClassDeclaration *classDecl, std::set<SgClassDeclaration *> &active) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL || classDecl->get_definition() == NULL) {
    return false;
  }

  SgClassDefinition *definition = classDecl->get_definition();
  for (SgBaseClass *baseClass : definition->get_inheritances()) {
    SgClassDeclaration *baseDecl = callGraphBaseClassDeclaration(baseClass);
    if (callGraphClassHasNontrivialDefaultConstruction(baseDecl, active)) {
      return true;
    }
  }

  for (SgDeclarationStatement *member : definition->get_members()) {
    SgVariableDeclaration *varDecl = isSgVariableDeclaration(member);
    if (varDecl == NULL) {
      continue;
    }

    for (SgInitializedName *initName : varDecl->get_variables()) {
      SgClassDeclaration *memberClass =
          initName == NULL ? NULL
                           : callGraphConstructedClassDeclarationFromType(
                                 initName->get_type());
      if (callGraphClassHasNontrivialDefaultConstruction(memberClass, active)) {
        return true;
      }
    }
  }

  return false;
}

static bool callGraphClassHasNontrivialDefaultConstruction(
    SgClassDeclaration *classDecl, std::set<SgClassDeclaration *> &active) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL || !active.insert(classDecl).second) {
    return false;
  }

  const bool result =
      callGraphDefaultConstructorIsUserProvided(classDecl) ||
      callGraphClassHasVirtualMember(classDecl) ||
      callGraphClassHasNontrivialDefaultConstructedSubobject(classDecl, active);
  active.erase(classDecl);
  return result;
}

static bool
callGraphClassHasNontrivialDefaultConstruction(SgClassDeclaration *classDecl) {
  std::set<SgClassDeclaration *> active;
  return callGraphClassHasNontrivialDefaultConstruction(classDecl, active);
}

bool CallGraphBuilder::shouldMaterializeImplicitCallTarget(
    SgFunctionDeclaration *fdecl) const {
  if (fdecl == NULL) {
    return false;
  }

  SgMemberFunctionDeclaration *memberDecl =
      isSgMemberFunctionDeclaration(canonicalFunctionDeclForCallGraph(fdecl));
  if (memberDecl == NULL ||
      !callGraphMemberFunctionIsDefaultConstructor(memberDecl)) {
    return false;
  }

  Sg_File_Info *fileInfo = memberDecl->get_file_info();
  if (fileInfo == NULL ||
      (!fileInfo->isCompilerGenerated() && !fileInfo->isFrontendSpecific())) {
    return false;
  }

  SgClassDeclaration *classDecl =
      callGraphAssociatedClassDeclaration(memberDecl);
  std::string classFilename =
      callGraphClassDeclarationSourceFilename(classDecl);
  if (classFilename.empty()) {
    classFilename = callGraphInstantiatedClassPatternSourceFilename(classDecl);
  }
  return callGraphProjectContainsSourceFile(project, classFilename) &&
         callGraphClassHasNontrivialDefaultConstruction(classDecl);
}

bool CallGraphBuilder::shouldMaterializeResolvedCallTarget(
    SgFunctionDeclaration *fdecl) const {
  fdecl = canonicalCallableFunctionDecl(fdecl);
  if (fdecl == NULL) {
    return false;
  }

  if (shouldMaterializeImplicitCallTarget(fdecl)) {
    return true;
  }

  SgMemberFunctionDeclaration *memberDecl =
      isSgMemberFunctionDeclaration(fdecl);
  if (memberDecl == NULL) {
    return false;
  }

  if (callGraphMemberFunctionIsDefaultConstructor(memberDecl)) {
    return false;
  }

  SgClassDeclaration *associatedClass =
      callGraphAssociatedClassDeclaration(memberDecl);
  if (!isNestedInTemplateInstantiationContext(associatedClass)) {
    return false;
  }

  std::string classFilename =
      callGraphClassDeclarationSourceFilename(associatedClass);
  if (classFilename.empty()) {
    classFilename =
        callGraphInstantiatedClassPatternSourceFilename(associatedClass);
  }
  return callGraphProjectContainsSourceFile(project, classFilename);
}

SgGraphNode *CallGraphBuilder::ensureGraphNodeForImplicitCallTarget(
    SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  if (SgGraphNode *existing = getGraphNodeFor(unique)) {
    return existing;
  }

  if (!shouldMaterializeImplicitCallTarget(unique)) {
    return NULL;
  }

  std::string functionName = unique->get_qualified_name().getString();
  SgGraphNode *graphNode = new SgGraphNode(functionName);
  graphNode->set_SgNode(unique);
  graphNodes[unique] = graphNode;
  graph->addNode(graphNode);
  return graphNode;
}

SgGraphNode *CallGraphBuilder::ensureGraphNodeForResolvedCallTarget(
    SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  if (SgGraphNode *existing = getGraphNodeFor(unique)) {
    return existing;
  }

  if (!shouldMaterializeResolvedCallTarget(unique)) {
    return NULL;
  }

  std::string functionName = unique->get_qualified_name().getString();
  SgGraphNode *graphNode = new SgGraphNode(functionName);
  graphNode->set_SgNode(unique);
  graphNodes[unique] = graphNode;
  graph->addNode(graphNode);
  return graphNode;
}

void CallGraphBuilder::materializeNonFunctionImplicitCallTargets(
    ClassHierarchyWrapper *classHierarchy) {
  ASSERT_not_null(classHierarchy);

  VariantVector callSiteVariants;
  callSiteVariants.push_back(V_SgConstructorInitializer);
  Rose_STL_Container<SgNode *> callSites =
      NodeQuery::querySubTree(project, callSiteVariants);

  for (SgNode *callSite : callSites) {
    SgExpression *callExpr = isSgExpression(callSite);
    if (callExpr == NULL || SageInterface::getEnclosingFunctionDefinition(
                                callExpr, false) != NULL) {
      continue;
    }

    Rose_STL_Container<SgFunctionDeclaration *> targets;
    CallTargetSet::getPropertiesForExpression(callExpr, classHierarchy,
                                              targets);
    for (SgFunctionDeclaration *target : targets) {
      if (target != NULL) {
        ensureGraphNodeForImplicitCallTarget(
            canonicalFunctionDeclForCallGraph(target));
      }
    }
  }
}

static void solveVirtualFunctionCall(
    SgClassType *crtClass, ClassHierarchyWrapper *classHierarchy,
    SgMemberFunctionDeclaration *memberFunctionDeclaration,
    SgMemberFunctionDeclaration *functionDeclarationInClass,
    bool includePureVirtualFunc, std::vector<SgFunctionDeclaration *> &result) {
  ASSERT_not_null(classHierarchy);
  ASSERT_not_null(memberFunctionDeclaration);
  ASSERT_not_null(functionDeclarationInClass);

  // If it's not pure virtual then the current function declaration is a
  // candidate function to be called
  if (includePureVirtualFunc || !isPureVirtual(functionDeclarationInClass))
    result.push_back(functionDeclarationInClass);

  // Search down the class hierarchy to get all redeclarations of the current
  // member function which may be the ones being called via polymorphism.
  SgClassDefinition *crtClsDef = nullptr;

  // selecting the root of the hierarchy
  if (crtClass) {
    SgClassDeclaration *tmp = isSgClassDeclaration(crtClass->get_declaration());
    ASSERT_not_null(tmp);
    SgClassDeclaration *tmp2 =
        isSgClassDeclaration(tmp->get_definingDeclaration());
    ASSERT_not_null(tmp2);

    crtClsDef = tmp2->get_definition();
  } else {
    crtClsDef = isSgClassDefinition(memberFunctionDeclaration->get_scope());
  }

  ASSERT_not_null(crtClsDef);
  // For virtual functions, we need to search down in the hierarchy of classes
  // and retrieve all declarations of member functions with the same name and
  // type.  Names are not important for destructors.
  const ClassHierarchyWrapper::ClassDefSet &subclasses =
      classHierarchy->getSubclasses(crtClsDef);

  std::string f1 = memberFunctionDeclaration->get_mangled_name().str();
  ASSERT_require(!memberFunctionDeclaration->get_name().getString().empty());

  const bool isDestructor1 =
      '~' == memberFunctionDeclaration->get_name().getString()[0];

  for (ClassHierarchyWrapper::ClassDefSet::const_iterator sci =
           subclasses.begin();
       sci != subclasses.end(); ++sci) {
    SgClassDefinition *cls = isSgClassDefinition(*sci);
    SgDeclarationStatementPtrList &clsMembers = cls->get_members();

    for (SgDeclarationStatementPtrList::iterator cmi = clsMembers.begin();
         cmi != clsMembers.end(); ++cmi) {
      SgMemberFunctionDeclaration *cls_mb_decl =
          isSgMemberFunctionDeclaration(*cmi);
      if (cls_mb_decl == NULL)
        continue;

      ASSERT_require(!cls_mb_decl->get_name().getString().empty());

      const bool isDestructor2 = '~' == cls_mb_decl->get_name().getString()[0];
      const bool keep =
          ((isDestructor1 && isDestructor2) ||
           isOverridingFunction(cls_mb_decl, memberFunctionDeclaration,
                                classHierarchy));

      if (keep) {
        SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
            cls_mb_decl->get_firstNondefiningDeclaration());
        SgMemberFunctionDeclaration *defDecl = isSgMemberFunctionDeclaration(
            cls_mb_decl->get_definingDeclaration());

        // MD 2010/07/08 defDecl might be NULL
        SgMemberFunctionDeclaration *candidate =
            nonDefDecl ? nonDefDecl : defDecl;
        ASSERT_not_null(candidate);

        if (includePureVirtualFunc || !isPureVirtual(candidate))
          result.push_back(candidate);
      }
    }
  }
}

static SgMemberFunctionDeclaration *findMemberDeclarationInReceiverClass(
    SgClassType *crtClass,
    SgMemberFunctionDeclaration *memberFunctionDeclaration) {
  if (crtClass == NULL || memberFunctionDeclaration == NULL) {
    return NULL;
  }

  SgClassDeclaration *class_decl =
      isSgClassDeclaration(crtClass->get_declaration());
  if (class_decl == NULL) {
    return NULL;
  }

  SgClassDeclaration *defining_class_decl =
      isSgClassDeclaration(class_decl->get_definingDeclaration());
  if (defining_class_decl == NULL ||
      defining_class_decl->get_definition() == NULL) {
    return NULL;
  }

  SgMemberFunctionDeclaration *pattern_decl = memberFunctionDeclaration;
  if (SgMemberFunctionDeclaration *first_nondef = isSgMemberFunctionDeclaration(
          memberFunctionDeclaration->get_firstNondefiningDeclaration())) {
    pattern_decl = first_nondef;
  }

  SgTemplateMemberFunctionDeclaration *template_pattern =
      canonicalTemplateMemberPatternDecl(pattern_decl);
  SgTemplateInstantiationMemberFunctionDecl *template_instantiation =
      isSgTemplateInstantiationMemberFunctionDecl(pattern_decl);
  SgDeclarationStatementPtrList &members =
      defining_class_decl->get_definition()->get_members();
  for (SgDeclarationStatementPtrList::iterator it = members.begin();
       it != members.end(); ++it) {
    SgMemberFunctionDeclaration *candidate = isSgMemberFunctionDeclaration(*it);
    if (candidate == NULL) {
      continue;
    }

    if (SgMemberFunctionDeclaration *first_nondef =
            isSgMemberFunctionDeclaration(
                candidate->get_firstNondefiningDeclaration())) {
      candidate = first_nondef;
    }

    if (candidate->get_name() != pattern_decl->get_name()) {
      continue;
    }

    if (template_instantiation != NULL) {
      SgTemplateInstantiationMemberFunctionDecl *inst_candidate =
          isSgTemplateInstantiationMemberFunctionDecl(candidate);
      if (inst_candidate == NULL) {
        continue;
      }

      SgTemplateMemberFunctionDeclaration *pattern_template =
          canonicalTemplateMemberPatternDecl(
              template_instantiation->get_templateDeclaration());
      if (pattern_template != NULL &&
          canonicalTemplateMemberPatternDecl(
              inst_candidate->get_templateDeclaration()) != pattern_template) {
        if (canonicalTemplateMemberPatternDecl(
                inst_candidate->get_specializedTemplateDeclaration()) !=
            pattern_template) {
          continue;
        }
      }

      if (!templateArgumentListsAreEquivalent(
              inst_candidate->get_templateArguments(),
              template_instantiation->get_templateArguments())) {
        continue;
      }

      if (!template_instantiation->get_deducedTemplateArguments().empty() &&
          !templateArgumentListsAreEquivalent(
              inst_candidate->get_deducedTemplateArguments(),
              template_instantiation->get_deducedTemplateArguments())) {
        continue;
      }

      return candidate;
    }

    if (template_pattern != NULL) {
      SgTemplateInstantiationMemberFunctionDecl *inst_candidate =
          isSgTemplateInstantiationMemberFunctionDecl(candidate);
      if (inst_candidate == NULL) {
        continue;
      }

      SgTemplateMemberFunctionDeclaration *candidate_template =
          canonicalTemplateMemberPatternDecl(
              inst_candidate->get_templateDeclaration());
      SgTemplateMemberFunctionDeclaration *candidate_specialized_template =
          canonicalTemplateMemberPatternDecl(
              inst_candidate->get_specializedTemplateDeclaration());

      if (candidate_template == template_pattern ||
          candidate_specialized_template == template_pattern) {
        return candidate;
      }

      continue;
    }

    if (candidate == pattern_decl ||
        candidate->get_type()->get_mangled() ==
            pattern_decl->get_type()->get_mangled()) {
      return candidate;
    }
  }

  return NULL;
}

static void collectMemberFunctionDeclarationsByName(
    SgClassDeclaration *classDecl, const SgName &name,
    std::set<SgClassDeclaration *> &visitedClasses,
    std::vector<SgMemberFunctionDeclaration *> &result) {
  classDecl = callGraphDefiningClassDeclaration(classDecl);
  if (classDecl == NULL || classDecl->get_definition() == NULL ||
      !visitedClasses.insert(classDecl).second) {
    return;
  }

  for (SgDeclarationStatement *member :
       classDecl->get_definition()->get_members()) {
    if (SgMemberFunctionDeclaration *memberFunction =
            isSgMemberFunctionDeclaration(member)) {
      memberFunction =
          callGraphCanonicalMemberFunctionDeclaration(memberFunction);
      if (memberFunction != NULL && memberFunction->get_name() == name) {
        result.push_back(memberFunction);
      }
    }
  }

  for (SgBaseClass *baseClass :
       classDecl->get_definition()->get_inheritances()) {
    if (baseClass == NULL) {
      continue;
    }

    collectMemberFunctionDeclarationsByName(
        isSgClassDeclaration(baseClass->get_base_class()), name, visitedClasses,
        result);
  }
}

static std::vector<SgMemberFunctionDeclaration *>
resolveNonrealMemberFunctionDeclarations(SgExpression *memberRefExp,
                                         SgClassType *receiverClass) {
  std::vector<SgMemberFunctionDeclaration *> result;
  SgNonrealRefExp *nonrealRef = isSgNonrealRefExp(memberRefExp);
  if (nonrealRef == NULL || nonrealRef->get_symbol() == NULL ||
      receiverClass == NULL) {
    return result;
  }

  std::set<SgClassDeclaration *> visitedClasses;
  collectMemberFunctionDeclarationsByName(
      isSgClassDeclaration(receiverClass->get_declaration()),
      nonrealRef->get_symbol()->get_name(), visitedClasses, result);
  return result;
}

std::vector<SgFunctionDeclaration *> CallTargetSet::solveMemberFunctionCall(
    SgClassType *crtClass, ClassHierarchyWrapper *classHierarchy,
    SgMemberFunctionDeclaration *memberFunctionDeclaration, bool polymorphic,
    bool includePureVirtualFunc) {
  ASSERT_not_null(memberFunctionDeclaration);

  std::vector<SgFunctionDeclaration *> functionList;

  SgMemberFunctionDeclaration *functionDeclarationInClass = NULL;
  if (SgDeclarationStatement *nonDefDeclInClass =
          memberFunctionDeclaration->get_firstNondefiningDeclaration()) {
    // memberFunctionDeclaration is outside the class
    functionDeclarationInClass =
        isSgMemberFunctionDeclaration(nonDefDeclInClass);
  } else {
    // In class declaration, since there is no non-defining declaration
    functionDeclarationInClass = memberFunctionDeclaration;
  }
  ASSERT_not_null(functionDeclarationInClass);

  // We need the inclass declaration so we can determine if it is a virtual
  // function
  if (functionDeclarationInClass->get_functionModifier().isVirtual() &&
      polymorphic) {
    // DQ (12/10/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable. SgFunctionDefinition *functionDefinition
    // = NULL;

    // SgMemberFunctionDeclaration* memberFunctionDefDeclaration =
    //     isSgMemberFunctionDeclaration(memberFunctionDeclaration->get_definingDeclaration());

    // DQ (12/10/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable. if (memberFunctionDefDeclaration !=
    // NULL) {
    //     functionDefinition = memberFunctionDefDeclaration->get_definition();
    // }
    solveVirtualFunctionCall(
        crtClass, classHierarchy, memberFunctionDeclaration,
        functionDeclarationInClass, includePureVirtualFunc, functionList);
  } else {
    // Non virtual (standard) member function or call not polymorphic (or both)
    // Always pushing the in-class declaration, so we need to find that one
    SgDeclarationStatement *nonDefDeclInClass =
        memberFunctionDeclaration->get_firstNondefiningDeclaration();
    SgMemberFunctionDeclaration *functionDeclarationInClass = NULL;
    if (nonDefDeclInClass) {
      // memberFunctionDeclaration is outside the class
      functionDeclarationInClass =
          isSgMemberFunctionDeclaration(nonDefDeclInClass);
    } else {
      // In class declaration, since there is no non-defining declaration
      functionDeclarationInClass = memberFunctionDeclaration;
    }
    ASSERT_not_null(functionDeclarationInClass);
    if (SgMemberFunctionDeclaration *resolved_decl =
            findMemberDeclarationInReceiverClass(crtClass,
                                                 functionDeclarationInClass)) {
      functionDeclarationInClass = resolved_decl;
    }
    functionList.push_back(functionDeclarationInClass);
  }
  return functionList;
}

Rose_STL_Container<SgFunctionDeclaration *>
solveFunctionPointerCallsFunctional(SgNode *node,
                                    SgFunctionType *functionType) {
  ASSERT_not_null(functionType);

  Rose_STL_Container<SgFunctionDeclaration *> functionList;

  SgFunctionDeclaration *fctDecl =
      canonicalCallableFunctionDecl(isSgFunctionDeclaration(node));
  if (fctDecl == NULL) {
    return functionList;
  }
  // if ( functionType == fctDecl->get_type() )
  // Find all function declarations which is both first non-defining declaration
  // and has a mangled name which is equal to the mangled name of 'functionType'
  if (functionType->get_mangled().getString() ==
      fctDecl->get_type()->get_mangled().getString()) {
    functionList.push_back(fctDecl);
  }

  return functionList;
}

/**
 * This function determines all the constructors called in a constructor
 * initializer.  For example:
 *   Bar::Bar() : foo() {}
 *   In this case, we need to list foo() as having been called, and the
 *constructors for all of foo's BaseClasses.  (These are returned as
 *SgFunctionDeclarations, in the props vector)
 **/
std::vector<SgFunctionDeclaration *> CallTargetSet::solveConstructorInitializer(
    SgConstructorInitializer *sgCtorInit) {
  std::vector<SgFunctionDeclaration *> props;
  SgMemberFunctionDeclaration *memFunDecl = sgCtorInit->get_declaration();

  // It's possible to have a null constructor declaration, in case of
  // compiler-generated default constructors.  Since the below special handling
  // is for
  //      (memFunDecl->get_file_info()->isCompilerGenerated() &&
  //      !isSgTemplateInstantiationMemberFunctionDecl(memFunDecl) &&
  //      !isSgTemplateMemberFunctionDecl(memFunDecl))
  if (memFunDecl != NULL) {
    SgMemberFunctionDeclaration *decl =
        callGraphCanonicalMemberFunctionDeclaration(memFunDecl);
    ASSERT_not_null(decl);
    props.push_back(decl);
  } else if (SgMemberFunctionDeclaration *copyConstructor =
                 callGraphResolveSameClassCopyConstructor(sgCtorInit)) {
    props.push_back(copyConstructor);
  }

  // If there are superclasses, the constructors for those classes may have been
  // called. We need to return them

  // Sometimes constructor initializers appear for primitive types. (e.g. x() in
  // a constructor initializer list)
  if (sgCtorInit->get_class_decl() != NULL) {
    // The worklist contains classes that are initialized through
    // compiler-generated default constructors
    vector<SgClassDeclaration *> worklist;
    worklist.push_back(sgCtorInit->get_class_decl());

    while (!worklist.empty()) {
      SgClassDeclaration *currClassDecl = worklist.back();
      worklist.pop_back();

      SgClassDeclaration *defClassDecl =
          isSgClassDeclaration(currClassDecl->get_definingDeclaration());
      if (defClassDecl == NULL) { // Can get a NULL here if a primative type is
                                  // being constructed.
        continue;                 // For example, a pointer
      }
      SgClassDefinition *currClass = defClassDecl->get_definition();
      if (currClass == NULL) { // Can get a NULL here if class is an anonymous
                               // compiler generated BaseClass
        continue;
      }

      for (SgBaseClass *baseClass : currClass->get_inheritances()) {
        if (baseClass == NULL || baseClass->get_base_class() == NULL) {
          // Clang translation can materialize placeholder inheritance entries
          // for anonymous or otherwise unresolved bases. They do not denote a
          // callable constructor target and should not abort call-graph
          // discovery for the enclosing constructor initializer.
          continue;
        }

        SgMemberFunctionDeclaration *constructorCalled =
            SageInterface::getDefaultConstructor(baseClass->get_base_class());
        if (constructorCalled != NULL) {
          SgMemberFunctionDeclaration *constructorCalledUnique =
              isSgMemberFunctionDeclaration(
                  constructorCalled->get_firstNondefiningDeclaration());
          props.push_back(constructorCalledUnique);
        } else {
          worklist.push_back(baseClass->get_base_class());
        }
      }
    }
  }

  return props;
}

//
// Add the declaration for functionCallExp to functionList. In the case of
// function pointers and virtual functions, append the set of declarations
// to functionList.
void getPropertiesForSgConstructorInitializer(
    SgConstructorInitializer *sgCtorInit, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList) {
  // currently, all constructor initializers can be handled by
  // solveConstructorInitializer
  std::vector<SgFunctionDeclaration *> props =
      CallTargetSet::solveConstructorInitializer(sgCtorInit);
  functionList.insert(functionList.end(), props.begin(), props.end());
}

/** Add the declaration for functionCallExp to functionList. In the case of
 * function pointers and virtual functions, append the set of declarations to
 * functionList. */
void getPropertiesForSgFunctionCallExp(
    SgFunctionCallExp *sgFunCallExp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc = false,
    const TemplateInstantiationAnalysisContext *templateContext = NULL) {
  SgExpression *functionExp = sgFunCallExp->get_function();
  ASSERT_not_null(functionExp);

  while (SgCommaOpExp *comma = isSgCommaOpExp(functionExp))
    functionExp = comma->get_rhs_operand();

  while (SgAddressOfOp *address_of = isSgAddressOfOp(functionExp))
    functionExp = address_of->get_operand();

  switch (functionExp->variantT()) {
  case V_SgArrowStarOp:
  case V_SgDotStarOp: {
    std::vector<SgFunctionDeclaration *> fD =
        CallTargetSet::solveMemberFunctionPointerCall(functionExp,
                                                      classHierarchy);
    functionList.insert(functionList.end(), fD.begin(), fD.end());
    break;
  }

  case V_SgDotExp:
  case V_SgArrowExp: {
    ASSERT_not_null(isSgBinaryOp(functionExp));

    SgExpression *leftSide = isSgBinaryOp(functionExp)->get_lhs_operand();
    SgType *const receiverType = leftSide->get_type();
    SgType *const leftType = receiverType->findBaseType();
    SgClassType *crtClass = resolveClassTypeFromType(leftType);

    SgExpression *memberRefExp = isSgBinaryOp(functionExp)->get_rhs_operand();
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        resolveMemberFunctionDeclarationFromExpression(memberRefExp);
    if (memberFunctionDeclaration != NULL) {
      // AS(122805) In the case of a constructor initializer it is possible that
      // a call to a constructor initializer may return a type corresponding to
      // an operator some-type() declared within the constructed class. An
      // example is:
      //   struct Foo {
      //      operator  bool () const
      //          { return true; }
      //   };
      //
      //   struct Bar {
      //      bool foobar()
      //          { return Foo (); }
      //   };
      // where the call to the constructor of the class Foo will cause a call to
      // the operator bool(), where bool corresponds type of the member function
      // foobar declared within Bar.
      if (isSgConstructorInitializer(leftSide)) {
        SgClassDeclaration *constInit =
            isSgConstructorInitializer(leftSide)->get_class_decl();
        if (constInit) {
          crtClass = constInit->get_type();
        } else {
          // AS(010306) A compiler constructed SgConstructorInitializer may wrap
          // a function call which return a class type. In an dot or arrow
          // expression this returned class type may be used as an expression
          // left hand side. To handle this case the returned class type must be
          // extracted from the expression list. An example demonstrating this
          // is:
          // class Vector3d {
          //   public:
          //    Vector3d(){};
          //    Vector3d(const Vector3d &vector3d){};
          //   Vector3d     cross() const
          //        { return Vector3d();};
          //   void   GetZ(){};
          //};
          // void foo(){
          //  Vector3d vn1;
          //  (vn1.cross()).GetZ();
          //}
          SgExprListExp *expLst =
              isSgExprListExp(isSgConstructorInitializer(leftSide)->get_args());
          ASSERT_not_null(expLst);
          ASSERT_require(expLst->get_expressions().size() == 1);
          SgClassType *lhsClassType = isSgClassType(
              isSgFunctionCallExp(*expLst->get_expressions().begin())
                  ->get_type()
                  ->stripType(SgType::STRIP_TYPEDEF_TYPE));
          crtClass = lhsClassType;
        }
        ASSERT_not_null(crtClass);
      }

      // Set function to first non-defining declaration
      SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
          memberFunctionDeclaration->get_firstNondefiningDeclaration());
      if (nonDefDecl)
        memberFunctionDeclaration = nonDefDecl;

      if (crtClass == NULL) {
        if (SgTemplateInstantiationMemberFunctionDecl *instantiated_member =
                resolveMemberFunctionInstantiationFromNonrealType(
                    isSgNonrealType(leftType), memberFunctionDeclaration)) {
          functionList.push_back(instantiated_member);
          break;
        }

        crtClass = resolveClassTypeFromMemberDecl(memberFunctionDeclaration);
      }

      if (crtClass == NULL) {
        // Some frontend paths preserve the receiver expression as a nonreal
        // type even though the member reference is already bound to a concrete
        // instantiation. If we still cannot recover the owning class from the
        // bound declaration, fall back to the historical conservative exit.
        break; // FIXME ROSE-1487
      }

      ASSERT_not_null(crtClass);

      // test if the memberFunctionRefExp is scope qualified
      //   in which case the vcall is suppressed.
      // \note to handle constructors correctly, we would need the assumed call
      // stack.

      SgType *const receiverBaseType =
          receiverType->stripTypedefsAndModifiers();
      const bool polymorphicType = (isSgPointerType(receiverBaseType) ||
                                    isSgReferenceType(receiverBaseType) ||
                                    isSgRvalueReferenceType(receiverBaseType) ||
                                    isSgArrayType(receiverBaseType));

      const bool polymorphicCall =
          (polymorphicType && !memberFunctionRefNeedsQualifier(memberRefExp));

      std::vector<SgFunctionDeclaration *> fD =
          CallTargetSet::solveMemberFunctionCall(
              crtClass, classHierarchy, memberFunctionDeclaration,
              polymorphicCall, includePureVirtualFunc);
      functionList.insert(functionList.end(), fD.begin(), fD.end());
    } else if (isSgNonrealRefExp(memberRefExp) != NULL &&
               templateContext != NULL && !templateContext->empty()) {
      SgType *substitutedReceiverType =
          substituteTemplateParameterType(leftType, templateContext);
      SgClassType *substitutedClass =
          resolveClassTypeFromType(substitutedReceiverType);
      const bool polymorphicType =
          isSgPointerType(receiverType->stripTypedefsAndModifiers()) != NULL ||
          isSgReferenceType(receiverType->stripTypedefsAndModifiers()) !=
              NULL ||
          isSgRvalueReferenceType(receiverType->stripTypedefsAndModifiers()) !=
              NULL ||
          isSgArrayType(receiverType->stripTypedefsAndModifiers()) != NULL;
      const bool polymorphicCall =
          polymorphicType && !memberFunctionRefNeedsQualifier(memberRefExp);

      std::vector<SgMemberFunctionDeclaration *> memberFunctions =
          resolveNonrealMemberFunctionDeclarations(memberRefExp,
                                                   substitutedClass);
      for (SgMemberFunctionDeclaration *candidate : memberFunctions) {
        std::vector<SgFunctionDeclaration *> fD =
            CallTargetSet::solveMemberFunctionCall(
                substitutedClass, classHierarchy, candidate, polymorphicCall,
                includePureVirtualFunc);
        functionList.insert(functionList.end(), fD.begin(), fD.end());
      }
    }
    break;
  }

  case V_SgPointerDerefExp: {
    SgPointerDerefExp *exp = isSgPointerDerefExp(functionExp);
    // If the thing pointed to is ultimately a SgFunctionRefExp then we
    // can figure out the exact function that's being pointed to just by
    // following the pointers to the SgFunctionRefExp.  Some frontends
    // never generated this kind of AST because it removed the
    // extraneous SgPointerDerefExp nodes.  I.e., for input like this:
    //   void g() { (********g)(); }
    // Some frontend ASTs would not have any SgFunctionRefExp nodes,
    // while others leave all of them there. [Robb Matzke 2012-12-28]
    SgFunctionRefExp *fref = NULL;
    while (exp && !fref) {
      fref = isSgFunctionRefExp(exp->get_operand_i());
      exp = isSgPointerDerefExp(exp->get_operand_i());
    }
    if (!fref) {
      // We don't know what function is being called, only its type.  So assume
      // that all functions whose type matches could be called. [Robb Matzke
      // 2012-12-28]
      std::vector<SgFunctionDeclaration *> fD =
          CallTargetSet::solveFunctionPointerCall(
              isSgPointerDerefExp(functionExp));
      functionList.insert(functionList.end(), fD.begin(), fD.end());
      break;
    } else {
      // We know the function being called, so fall through to the
      // SgFunctionRefExp case.
      functionExp = fref;
    }
  }
    // fall through...

  case V_SgMemberFunctionRefExp:
  case V_SgTemplateMemberFunctionRefExp:
  case V_SgFunctionRefExp: {
    if (SgMemberFunctionDeclaration *memberFunctionDeclaration =
            resolveMemberFunctionDeclarationFromExpression(functionExp)) {
      SgClassType *crtClass = NULL;
      if (SgMemberFunctionDeclaration *enclosingMember =
              enclosingMemberFunctionDeclaration(functionExp)) {
        crtClass = resolveClassTypeFromMemberDecl(enclosingMember);
      }
      if (crtClass == NULL) {
        crtClass = resolveClassTypeFromMemberDecl(memberFunctionDeclaration);
      }

      const bool polymorphicCall =
          !memberFunctionRefNeedsQualifier(functionExp);

      std::vector<SgFunctionDeclaration *> fD =
          CallTargetSet::solveMemberFunctionCall(
              crtClass, classHierarchy, memberFunctionDeclaration,
              polymorphicCall, includePureVirtualFunc);
      functionList.insert(functionList.end(), fD.begin(), fD.end());
      break;
    }

    SgFunctionDeclaration *fctDecl = NULL;
    if (SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(functionExp)) {
      fctDecl = isSgFunctionDeclaration(
          functionRefExp->get_symbol()->get_declaration());
    }

    fctDecl = canonicalCallableFunctionDecl(fctDecl);
    if (fctDecl == NULL) {
      break;
    }

    functionList.push_back(fctDecl);
    break;
  }

  case V_SgVarRefExp: {
    using SgFunctionDeclarationPtrList =
        Rose_STL_Container<SgFunctionDeclaration *>;

    // This is an indirect function call, as in:
    //    |void f() {
    //    |    void (*g)();
    //    |    g();              <------------
    //    |}
    // We don't know what is being called, only its type.  So assume that all
    // functions whose type matches could be called. [Robb P. Matzke 2013-01-24]
    VariantVector vv;
    vv.push_back(V_SgFunctionDeclaration);
    vv.push_back(V_SgTemplateInstantiationFunctionDecl);
    SgType *type = isSgVarRefExp(functionExp)->get_type();
    while (isSgTypedefType(type))
      type = isSgTypedefType(type)->get_base_type();
    SgPointerType *functionPointerType = isSgPointerType(type);

    if (functionPointerType == NULL)
      break; // FIXME ROSE-1487

    ASSERT_not_null(functionPointerType);
    SgFunctionType *fctType =
        isSgFunctionType(functionPointerType->findBaseType());
    ASSERT_not_null(fctType);

    // Replaced deprecated functions std::bind2nd and std::ptr_fun [Rasmussen,
    // 2023.08.07]
    std::function<SgFunctionDeclarationPtrList(SgNode *, SgFunctionType *)>
        ptrFun = solveFunctionPointerCallsFunctional;

    SgFunctionDeclarationPtrList matches = AstQueryNamespace::queryMemoryPool(
        std::bind(ptrFun, std::placeholders::_1, fctType), &vv);
    functionList.insert(functionList.end(), matches.begin(), matches.end());
    break;
  }

  case V_SgTemplateParameterVal: {
    using SgFunctionDeclarationPtrList =
        Rose_STL_Container<SgFunctionDeclaration *>;

    // Calls through non-type template parameters (e.g. `FUNC();` inside
    // `template<void (*FUNC)()>`) are indirect calls whose concrete target is
    // not always encoded as a direct function-ref in the generic AST. Resolve
    // them conservatively from the parameter's callable type, just like we do
    // for ordinary function-pointer calls.
    SgType *type = isSgTemplateParameterVal(functionExp)->get_type();
    while (isSgTypedefType(type))
      type = isSgTypedefType(type)->get_base_type();

    if (type == NULL)
      break;

    SgFunctionType *fctType = isSgFunctionType(type->findBaseType());
    if (fctType == NULL)
      break;

    VariantVector vv;
    vv.push_back(V_SgFunctionDeclaration);
    vv.push_back(V_SgTemplateInstantiationFunctionDecl);

    std::function<SgFunctionDeclarationPtrList(SgNode *, SgFunctionType *)>
        ptrFun = solveFunctionPointerCallsFunctional;

    SgFunctionDeclarationPtrList matches = AstQueryNamespace::queryMemoryPool(
        std::bind(ptrFun, std::placeholders::_1, fctType), &vv);
    functionList.insert(functionList.end(), matches.begin(), matches.end());
    break;
  }

  case V_SgPntrArrRefExp:
  case V_SgCastExp:
    break; // FIXME ROSE-1487

  // \todo
  // PP (04/06/20)
  case V_SgTemplateFunctionRefExp:
  case V_SgNonrealRefExp:
  case V_SgConstructorInitializer:
  case V_SgFunctionCallExp:
    break;

  default: {
    cout << "Error, unexpected type of functionRefExp: "
         << functionExp->sage_class_name() << "!!!\n";
    ROSE_ABORT();
  }
  }
}
// Add the declaration for functionCallExp to functionList. In the case of
// function pointers and virtual functions, append the set of declarations
// to functionList.

static SgExpression *
semanticCallExpressionForLoweredOperatorSyntax(SgExpression *expr) {
  if (expr == NULL) {
    return NULL;
  }

  SgExpression *original = expr->get_originalExpressionTree();
  if (original == NULL || original == expr) {
    return NULL;
  }

  if (isSgFunctionCallExp(original) != NULL ||
      isSgConstructorInitializer(original) != NULL) {
    return original;
  }

  return NULL;
}

static void getPropertiesForExpressionWithTemplateContext(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc,
    const TemplateInstantiationAnalysisContext *templateContext);

void CallTargetSet::getPropertiesForExpression(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc) {
  getPropertiesForExpressionWithTemplateContext(
      sgexp, classHierarchy, functionList, includePureVirtualFunc, NULL);
}

static void getPropertiesForExpressionWithTemplateContext(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc,
    const TemplateInstantiationAnalysisContext *templateContext) {
  if (SgExpression *semantic_call =
          semanticCallExpressionForLoweredOperatorSyntax(sgexp)) {
    getPropertiesForExpressionWithTemplateContext(
        semantic_call, classHierarchy, functionList, includePureVirtualFunc,
        templateContext);
  } else if (SgFunctionCallExp *fncall = isSgFunctionCallExp(sgexp)) {
    getPropertiesForSgFunctionCallExp(fncall, classHierarchy, functionList,
                                      includePureVirtualFunc, templateContext);
  } else if (SgConstructorInitializer *ctorini =
                 isSgConstructorInitializer(sgexp)) {
    getPropertiesForSgConstructorInitializer(ctorini, classHierarchy,
                                             functionList);
  } else if (isSgPntrArrRefExp(sgexp) != NULL ||
             isSgPointerDerefExp(sgexp) != NULL) {
    // Built-in subscripts/dereferences are not calls. Syntax-lowered overloaded
    // operators are handled through their preserved semantic call above.
  } else {
    std::cerr << "Cannot determine Properties for " << sgexp->class_name()
              << std::endl;
  }
}

void CallTargetSet::getDeclarationsForExpression(
    SgExpression *exp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &defList,
    bool includePureVirtualFunc) {
  Rose_STL_Container<SgFunctionDeclaration *> props;
  CallTargetSet::getPropertiesForExpression(exp, classHierarchy, props,
                                            includePureVirtualFunc);

  for (SgFunctionDeclaration *candidateDecl : props) {
    candidateDecl = canonicalCallableFunctionDecl(candidateDecl);
    if (candidateDecl != NULL) {
      defList.push_back(candidateDecl);
    }
  }
}

void CallTargetSet::getDefinitionsForExpression(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDefinition *> &defList) {
  Rose_STL_Container<SgFunctionDeclaration *> props;
  CallTargetSet::getPropertiesForExpression(sgexp, classHierarchy, props);

  for (SgFunctionDeclaration *candidateDecl : props) {
    candidateDecl = canonicalCallableFunctionDecl(candidateDecl);
    if (candidateDecl == NULL) {
      continue;
    }
    candidateDecl =
        isSgFunctionDeclaration(candidateDecl->get_definingDeclaration());
    if (candidateDecl != NULL) {
      SgFunctionDefinition *candidateDef = candidateDecl->get_definition();
      if (candidateDef != NULL) {
        defList.push_back(candidateDef);
      }
    }
  }
}

namespace {

using DefinitionExpressionIndex =
    std::map<SgFunctionDefinition *, Rose_STL_Container<SgExpression *>>;

std::mutex &definitionExpressionIndexInstallMutex() {
  static std::mutex install_mutex;
  return install_mutex;
}

const std::string &definitionExpressionIndexAttributeName() {
  static const std::string attribute_name =
      "rose.callGraph.definitionExpressionIndex";
  return attribute_name;
}

class DefinitionExpressionIndexAttribute : public AstAttribute {
public:
  std::recursive_mutex mutex;
  DefinitionExpressionIndex index;
  bool initialized = false;
  uint64_t astModificationSequence = 0;
  const ClassHierarchyWrapper *classHierarchy = nullptr;

  AstAttribute::OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "DefinitionExpressionIndexAttribute";
  }
};

static void
indexDefinitionExpression(SgExpression *exp,
                          ClassHierarchyWrapper *classHierarchy,
                          DefinitionExpressionIndex &definitionIndex) {
  ASSERT_not_null(exp);
  ASSERT_not_null(classHierarchy);

  Rose_STL_Container<SgFunctionDefinition *> candidateDefs;
  CallTargetSet::getDefinitionsForExpression(exp, classHierarchy,
                                             candidateDefs);

  std::set<SgFunctionDefinition *> uniqueCandidateDefs;
  for (SgFunctionDefinition *candidateDef : candidateDefs) {
    if (candidateDef == NULL ||
        !uniqueCandidateDefs.insert(candidateDef).second) {
      continue;
    }

    definitionIndex[candidateDef].push_back(exp);
  }
}

class DefinitionExpressionIndexBuilder : public AstSimpleProcessing {
public:
  DefinitionExpressionIndexBuilder(ClassHierarchyWrapper *classHierarchy,
                                   DefinitionExpressionIndex &definitionIndex)
      : classHierarchy_(classHierarchy), definitionIndex_(definitionIndex) {
    ASSERT_not_null(classHierarchy_);
  }

  void visit(SgNode *node) override {
    if (node == NULL) {
      return;
    }

    switch (node->variantT()) {
    case V_SgFunctionCallExp:
    case V_SgConstructorInitializer:
      if (SgExpression *exp = isSgExpression(node)) {
        indexDefinitionExpression(exp, classHierarchy_, definitionIndex_);
      }
      break;

    case V_SgPntrArrRefExp:
    case V_SgPointerDerefExp:
    case V_SgAddressOfOp:
    case V_SgAssignOp:
    case V_SgBitComplementOp:
    case V_SgMinusMinusOp:
    case V_SgMinusOp:
    case V_SgNotOp:
    case V_SgPlusPlusOp:
    case V_SgUnaryAddOp:
      if (SgExpression *exp = isSgExpression(node)) {
        if (semanticCallExpressionForLoweredOperatorSyntax(exp) != NULL) {
          indexDefinitionExpression(exp, classHierarchy_, definitionIndex_);
        }
      }
      break;

    default:
      break;
    }
  }

private:
  ClassHierarchyWrapper *classHierarchy_ = nullptr;
  DefinitionExpressionIndex &definitionIndex_;
};

static void
buildDefinitionExpressionIndex(SgProject *project,
                               ClassHierarchyWrapper *classHierarchy,
                               DefinitionExpressionIndex &definitionIndex) {
  ASSERT_not_null(project);
  ASSERT_not_null(classHierarchy);

  definitionIndex.clear();

  DefinitionExpressionIndexBuilder builder(classHierarchy, definitionIndex);
  builder.traverse(project, preorder);
}

static DefinitionExpressionIndexAttribute *
getOrCreateDefinitionExpressionIndexAttribute(SgProject *project) {
  ASSERT_not_null(project);

  std::lock_guard<std::mutex> install_guard(
      definitionExpressionIndexInstallMutex());

  const std::string &attribute_name = definitionExpressionIndexAttributeName();
  AstAttribute *attribute = project->getAttribute(attribute_name);
  if (attribute == NULL) {
    project->addNewAttribute(attribute_name,
                             new DefinitionExpressionIndexAttribute);
    attribute = project->getAttribute(attribute_name);
  }

  ASSERT_not_null(attribute);
  DefinitionExpressionIndexAttribute *index_attribute =
      dynamic_cast<DefinitionExpressionIndexAttribute *>(attribute);
  ASSERT_not_null(index_attribute);
  return index_attribute;
}

static void
appendExpressionsForDefinition(SgFunctionDefinition *targetDef,
                               const DefinitionExpressionIndex &definitionIndex,
                               Rose_STL_Container<SgExpression *> &exps) {
  ASSERT_not_null(targetDef);

  DefinitionExpressionIndex::const_iterator expressionIt =
      definitionIndex.find(targetDef);
  if (expressionIt == definitionIndex.end()) {
    return;
  }

  for (SgExpression *exp : expressionIt->second) {
    if (exp != NULL) {
      exps.push_back(exp);
    }
  }
}

} // namespace

void CallTargetSet::getExpressionsForDefinition(
    SgFunctionDefinition *targetDef, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgExpression *> &exps) {
  ASSERT_not_null(targetDef);
  ASSERT_not_null(classHierarchy);

  SgProject *project = SageInterface::getProject(targetDef);
  ASSERT_not_null(project);

  DefinitionExpressionIndexAttribute *index_attribute =
      getOrCreateDefinitionExpressionIndexAttribute(project);
  std::lock_guard<std::recursive_mutex> index_guard(index_attribute->mutex);
  uint64_t currentSequence = SgNode::get_globalAstModificationSequence();
  if (index_attribute->initialized == false ||
      index_attribute->astModificationSequence != currentSequence ||
      index_attribute->classHierarchy != classHierarchy) {
    buildDefinitionExpressionIndex(project, classHierarchy,
                                   index_attribute->index);
    index_attribute->initialized = true;
    index_attribute->astModificationSequence = currentSequence;
    index_attribute->classHierarchy = classHierarchy;
  }

  appendExpressionsForDefinition(targetDef, index_attribute->index, exps);
}

FunctionData::FunctionData(SgFunctionDeclaration *inputFunctionDeclaration,
                           SgProject *project,
                           ClassHierarchyWrapper *classHierarchy) {
  hasDefinition = false;

  functionDeclaration = canonicalCallableFunctionDecl(inputFunctionDeclaration);
  ASSERT_not_null(functionDeclaration);

  SgFunctionDeclaration *defDecl =
      (inputFunctionDeclaration->get_definition() != NULL
           ? inputFunctionDeclaration
           : isSgFunctionDeclaration(
                 functionDeclaration->get_definingDeclaration()));

  SgClassType *templateInstantiationReceiverClass = NULL;
  TemplateInstantiationAnalysisContext templateInstantiationContext =
      buildTemplateInstantiationAnalysisContext(
          isSgTemplateInstantiationFunctionDecl(functionDeclaration));
  if (!templateInstantiationContext.empty() &&
      (defDecl == NULL || defDecl->get_definition() == NULL)) {
    SgTemplateFunctionDeclaration *templateDecl =
        templateInstantiationContext.templateFunction;
    if (templateDecl->get_definition() != NULL) {
      defDecl = templateDecl;
    } else {
      defDecl =
          isSgFunctionDeclaration(templateDecl->get_definingDeclaration());
    }
  }

  if (SgTemplateInstantiationMemberFunctionDecl *instMember =
          isSgTemplateInstantiationMemberFunctionDecl(functionDeclaration)) {
    templateInstantiationReceiverClass =
        resolveClassTypeFromMemberDecl(instMember);

    if (defDecl == NULL || defDecl->get_definition() == NULL) {
      SgTemplateMemberFunctionDeclaration *templateDecl =
          instMember->get_templateDeclaration();
      if (templateDecl == NULL) {
        templateDecl = isSgTemplateMemberFunctionDeclaration(
            instMember->get_specializedTemplateDeclaration());
      }

      if (templateDecl != NULL) {
        if (templateDecl->get_definition() != NULL) {
          defDecl = templateDecl;
        } else {
          defDecl =
              isSgFunctionDeclaration(templateDecl->get_definingDeclaration());
        }
      }
    }
  }

  if (defDecl != NULL && defDecl->get_definition() == NULL) {
    defDecl = NULL;
    std::cerr << " **** If you see this error message. Report to the ROSE team "
                 "that a function declaration ****\n"
              << " **** has a defining declaration but no definition           "
                 "                            ****\n";
  }

  // cout << "!!!" << inputFunctionDeclaration->get_name().str() << " has
  // definition " << defDecl << "\n";
  //      cout << "Input declaration: " << inputFunctionDeclaration << " as
  //      opposed to " << functionDeclaration << "\n";

  // Test for a forward declaration (declaration without a definition)
  if (defDecl != NULL) {
    hasDefinition = true;

    Rose_STL_Container<SgNode *> functionCallExpList =
        NodeQuery::querySubTree(defDecl, V_SgFunctionCallExp);
    for (SgNode *functionCallExp : functionCallExpList) {
      getPropertiesForExpressionWithTemplateContext(
          isSgExpression(functionCallExp), classHierarchy, functionList, false,
          templateInstantiationContext.empty() ? NULL
                                               : &templateInstantiationContext);
    }

    Rose_STL_Container<SgNode *> ctorInitList =
        NodeQuery::querySubTree(defDecl, V_SgConstructorInitializer);
    for (SgNode *ctorInit : ctorInitList) {
      getPropertiesForExpressionWithTemplateContext(
          isSgExpression(ctorInit), classHierarchy, functionList, false,
          templateInstantiationContext.empty() ? NULL
                                               : &templateInstantiationContext);
    }

    VariantVector callSiteVariants;
    callSiteVariants.push_back(V_SgPntrArrRefExp);
    callSiteVariants.push_back(V_SgPointerDerefExp);
    callSiteVariants.push_back(V_SgAddressOfOp);
    callSiteVariants.push_back(V_SgAssignOp);
    callSiteVariants.push_back(V_SgBitComplementOp);
    callSiteVariants.push_back(V_SgMinusMinusOp);
    callSiteVariants.push_back(V_SgMinusOp);
    callSiteVariants.push_back(V_SgNotOp);
    callSiteVariants.push_back(V_SgPlusPlusOp);
    callSiteVariants.push_back(V_SgUnaryAddOp);
    Rose_STL_Container<SgNode *> callSiteList =
        NodeQuery::querySubTree(defDecl, callSiteVariants);
    for (SgNode *callSite : callSiteList) {
      SgExpression *callSiteExpr = isSgExpression(callSite);
      if (isSgPntrArrRefExp(callSiteExpr) == NULL &&
          isSgPointerDerefExp(callSiteExpr) == NULL) {
        if (semanticCallExpressionForLoweredOperatorSyntax(callSiteExpr) ==
            NULL) {
          continue;
        }
      }
      getPropertiesForExpressionWithTemplateContext(
          callSiteExpr, classHierarchy, functionList, false,
          templateInstantiationContext.empty() ? NULL
                                               : &templateInstantiationContext);
    }

    if (templateInstantiationReceiverClass != NULL) {
      for (SgFunctionDeclaration *&callee : functionList) {
        SgMemberFunctionDeclaration *memberCallee =
            isSgMemberFunctionDeclaration(callee);
        if (memberCallee == NULL) {
          continue;
        }

        if (SgMemberFunctionDeclaration *instantiatedCallee =
                findMemberDeclarationInReceiverClass(
                    templateInstantiationReceiverClass, memberCallee)) {
          callee = instantiatedCallee;
        }
      }
    }
  }
}
SgFunctionDeclaration *
CallTargetSet::getFirstVirtualFunctionDefinitionFromAncestors(
    SgClassType *crtClass,
    SgMemberFunctionDeclaration *memberFunctionDeclaration,
    ClassHierarchyWrapper *classHierarchy) {

  ASSERT_not_null(memberFunctionDeclaration);
  ASSERT_not_null(classHierarchy);

  SgFunctionDeclaration *resultDecl = NULL;

  //  memberFunctionDeclaration->get_file_info()->display( "Member function we
  //  are considering" );
  SgDeclarationStatement *nonDefDeclInClass = NULL;

  nonDefDeclInClass =
      memberFunctionDeclaration->get_firstNondefiningDeclaration();
  SgMemberFunctionDeclaration *functionDeclarationInClass = NULL;

  // memberFunctionDeclaration is outside the class
  if (nonDefDeclInClass) {
    functionDeclarationInClass =
        isSgMemberFunctionDeclaration(nonDefDeclInClass);
  }
  // in class declaration, since there is no non-defining declaration
  else {
    functionDeclarationInClass = memberFunctionDeclaration;
    //      functionDeclarationInClass->get_file_info()->display("declaration in
    //      class already");
  }

  ASSERT_not_null(functionDeclarationInClass);

  if (!(functionDeclarationInClass->get_functionModifier().isVirtual()))
    return NULL;
  // for virtual functions, we need to search down in the hierarchy of classes
  // and retrieve all declarations of member functions with the same type

  SgClassDefinition *crtClsDef = nullptr;

  if (crtClass) {
    SgClassDeclaration *tmp = isSgClassDeclaration(crtClass->get_declaration());
    ASSERT_not_null(tmp);
    SgClassDeclaration *tmp2 =
        isSgClassDeclaration(tmp->get_definingDeclaration());
    ASSERT_not_null(tmp2);
    crtClsDef = tmp2->get_definition();
  } else {
    crtClsDef = isSgClassDefinition(memberFunctionDeclaration->get_scope());
  }

  ASSERT_not_null(crtClsDef);

  functionDeclarationInClass = NULL;
  vector<SgClassDefinition *> worklist;
  worklist.push_back(crtClsDef);

  while (!worklist.empty()) {
    SgClassDefinition *ancestor = worklist.back();
    worklist.pop_back();
    SgClassDefinition *cls = isSgClassDefinition(ancestor);
    resultDecl = is_function_exists(cls, memberFunctionDeclaration);
    if (resultDecl != NULL)
      return resultDecl;
    const ClassHierarchyWrapper::ClassDefSet &ancestors =
        classHierarchy->getAncestorClasses(cls);
    worklist.insert(worklist.end(), ancestors.begin(), ancestors.end());
  }

  return NULL;
}

void CallGraphBuilder::buildCallGraph() { buildCallGraph(dummyFilter()); }

/**
 *  CallGraphBuilder::hasGraphNodeFor
 *
 * \brief Checks the graphNodes map for a graph node match fdecl
 *
 * This does a lookup on the CallGraph map to see if a given function is in it.
 * This is used in constructing the CallGraph.  If multiple files are input on
 * the command line firstNondefiningDeclartion may not be unique, so not finding
 * a graphNode for fdecl does not guarantee that the target function does not
 *exist in the graph, only that it is not in the lookup map.  Use
 *getGraphNodeFor to be sure.  Again, this is mainly used in constructing the
 *call graph.
 *
 * \param[in] fdecl The declaration of the function to look for in the graph
 *
 **/
SgGraphNode *
CallGraphBuilder::hasGraphNodeFor(SgFunctionDeclaration *fdecl) const {
  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  GraphNodes::const_iterator lookedup = graphNodes.find(unique);
  if (lookedup != graphNodes.end()) {
    return lookedup->second;
  }
  return NULL;
}

static bool
callGraphFileInfoCanDistinguishDeclarations(const Sg_File_Info *fi) {
  return fi != NULL && fi->get_line() > 0 && fi->get_col() > 0 &&
         !fi->isCompilerGenerated() && !fi->isFrontendSpecific() &&
         !fi->isTransformation();
}

static const Sg_File_Info *
callGraphBestSourceIdentityFileInfo(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *decls[] = {
      fdecl, isSgFunctionDeclaration(fdecl->get_firstNondefiningDeclaration()),
      isSgFunctionDeclaration(fdecl->get_definingDeclaration())};

  for (SgFunctionDeclaration *decl : decls) {
    if (decl == NULL) {
      continue;
    }

    const Sg_File_Info *fileInfo = decl->get_file_info();
    if (callGraphFileInfoCanDistinguishDeclarations(fileInfo)) {
      return fileInfo;
    }
  }

  return NULL;
}

static bool
callGraphDeclarationsShareSourceIdentity(SgFunctionDeclaration *lhs,
                                         SgFunctionDeclaration *rhs) {
  if (lhs == rhs) {
    return true;
  }

  const Sg_File_Info *lhsFileInfo = callGraphBestSourceIdentityFileInfo(lhs);
  const Sg_File_Info *rhsFileInfo = callGraphBestSourceIdentityFileInfo(rhs);

  if (lhsFileInfo == NULL || rhsFileInfo == NULL) {
    return true;
  }

  return lhsFileInfo->get_file_id() == rhsFileInfo->get_file_id() &&
         lhsFileInfo->get_line() == rhsFileInfo->get_line() &&
         lhsFileInfo->get_col() == rhsFileInfo->get_col() &&
         lhsFileInfo->get_filenameString() == rhsFileInfo->get_filenameString();
}

SgGraphNode *CallGraphBuilder::getGraphNodeForConstruction(
    SgFunctionDeclaration *fdecl) const {
  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  GraphNodes::const_iterator lookedup = graphNodes.find(unique);
  if (lookedup != graphNodes.end()) {
    return lookedup->second;
  }

  const std::string fname = unique->get_mangled_name();
  for (GraphNodes::const_iterator it = graphNodes.begin();
       it != graphNodes.end(); ++it) {
    if (it->first->get_mangled_name() == fname &&
        callGraphDeclarationsShareSourceIdentity(it->first, unique)) {
      return it->second;
    }
  }

  return NULL;
}

/**
 *  CallGraphBuilder::getGraphNodeFor
 *
 * \brief Double checks that the Call Graph has the function
 *
 * This double checks for a call graph node.  If multiple files are input on the
 *command line firstNondefiningDeclartion may not be unique.  (As expected) So
 *we can double check on the name.  This is useful only outside the CallGraph,
 *if the CallGraph is fully constructed, and we need a particular node from it.
 *
 * \param[in] fdecl The declaration of the function to look for in the graph
 *
 **/
SgGraphNode *
CallGraphBuilder::getGraphNodeFor(SgFunctionDeclaration *fdecl) const {
  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  GraphNodes::const_iterator lookedup = graphNodes.find(unique);
  if (lookedup != graphNodes.end()) {
    return lookedup->second;
  }

  // Fall back on old slow method (When putting multiple
  std::string fname = fdecl->get_mangled_name();
  for (GraphNodes::const_iterator it = graphNodes.begin();
       it != graphNodes.end(); ++it)
    if (it->first->get_mangled_name() == fname)
      return it->second;
  return NULL;
}

GetOneFuncDeclarationPerFunction::result_type
GetOneFuncDeclarationPerFunction::operator()(SgNode *node) {
  result_type returnType;
  SgFunctionDeclaration *funcDecl = isSgFunctionDeclaration(node);
  if (funcDecl != NULL && canonicalCallableFunctionDecl(funcDecl) != NULL) {
    if (funcDecl == canonicalFunctionDeclForCallGraph(funcDecl)) {
      returnType.push_back(node);
    }
  }
  return returnType;
}
