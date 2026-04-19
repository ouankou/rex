// This is the location of all the name qualification support functions
// required for code generation (unparser) (only applicable to C++).

#include "sage3basic.h"

#include "unparser.h"

using namespace std;

namespace {

SgDeclarationStatement *
referencedDeclarationForTemplateArgument(const SgTemplateArgument *arg) {
  if (arg == NULL) {
    return NULL;
  }

  switch (arg->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    SgType *type = arg->get_type();
    return type != NULL ? type->getAssociatedDeclaration() : NULL;
  }

  case SgTemplateArgument::template_template_argument:
    return isSgDeclarationStatement(arg->get_templateDeclaration());

  default:
    return NULL;
  }
}

SgScopeStatement *
scopeForTemplateArgumentQualification(SgDeclarationStatement *declaration) {
  if (declaration == NULL) {
    return NULL;
  }

  SgScopeStatement *scope = declaration->get_scope();
  ASSERT_not_null(scope);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration);
  while (nrdecl != NULL) {
    if (nrdecl->get_is_template_param()) {
      SgScopeStatement *param_scope = nrdecl->get_scope();
      if (SgDeclarationScope *decl_scope = isSgDeclarationScope(param_scope)) {
        scope = decl_scope;
      } else {
        scope = param_scope;
      }
      ASSERT_not_null(scope);
      break;
    }

    if (nrdecl->get_templateDeclaration() == NULL) {
      SgDeclarationScope *decl_scope =
          isSgDeclarationScope(nrdecl->get_scope());
      if (decl_scope == NULL) {
        scope = nrdecl->get_scope();
        ASSERT_not_null(scope);
        break;
      }

      SgNode *decl_scope_parent = decl_scope->get_parent();
      ASSERT_not_null(decl_scope_parent);

      if (SgNonrealDecl *nr_parent = isSgNonrealDecl(decl_scope_parent)) {
        ROSE_ASSERT(nr_parent != nrdecl);
        nrdecl = nr_parent;
      } else {
        SgScopeStatement *parent_scope = isSgScopeStatement(decl_scope_parent);
        if (parent_scope == NULL) {
          parent_scope = SageInterface::getEnclosingScope(decl_scope_parent);
        }
        ASSERT_not_null(parent_scope);
        scope = parent_scope;
        break;
      }
    } else {
      scope = nrdecl->get_templateDeclaration()->get_scope();
      ASSERT_not_null(scope);
      break;
    }
  }

  return scope;
}

std::string
nameForTemplateArgumentQualificationScope(SgScopeStatement *scope,
                                          SgScopeStatement *&next_scope) {
  if (scope == NULL) {
    next_scope = NULL;
    return "";
  }

  next_scope = scope->get_scope();
  std::string scope_name;

  if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(scope->get_parent())) {
      SgName nonreal_name = nrdecl->get_name();
      if (!nrdecl->get_tpl_args().empty()) {
        nonreal_name = SageBuilder::appendTemplateArgumentsToName(
            nonreal_name, nrdecl->get_tpl_args());
      }
      scope_name = nonreal_name.getString();
      if (SgScopeStatement *nr_scope = nrdecl->get_scope()) {
        next_scope = nr_scope;
      }
    } else if (next_scope == NULL) {
      next_scope = SageInterface::getEnclosingScope(scope);
    }
  } else if (SgNamespaceDefinitionStatement *ns_def =
                 isSgNamespaceDefinitionStatement(scope)) {
    SgNamespaceDeclarationStatement *ns_decl =
        ns_def->get_namespaceDeclaration();
    ASSERT_not_null(ns_decl);
    if (!ns_decl->get_isUnnamedNamespace()) {
      scope_name = ns_decl->get_name().getString();
    }
  } else if (SgTemplateInstantiationDefn *inst_def =
                 isSgTemplateInstantiationDefn(scope)) {
    SgTemplateInstantiationDecl *inst_decl =
        isSgTemplateInstantiationDecl(inst_def->get_declaration());
    ASSERT_not_null(inst_decl);
    scope_name = inst_decl->get_name().getString();
  } else if (SgTemplateClassDefinition *template_def =
                 isSgTemplateClassDefinition(scope)) {
    SgTemplateClassDeclaration *template_decl = template_def->get_declaration();
    ASSERT_not_null(template_decl);
    scope_name = template_decl->get_name().getString();
  } else if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    SgClassDeclaration *class_decl = class_def->get_declaration();
    ASSERT_not_null(class_decl);
    if (!class_decl->get_isUnNamed()) {
      scope_name = class_decl->get_name().getString();
    }
  } else if (isSgGlobal(scope) == NULL) {
    scope_name = SageInterface::get_name(scope);
  }

  if (scope_name == "undefined_name") {
    scope_name.clear();
  }

  if (scope_name.rfind("__anonymous_0x", 0) == 0) {
    scope_name.clear();
  }

  if (scope_name.rfind("0x", 0) == 0 && isSgGlobal(scope) == NULL) {
    scope_name.clear();
  }

  return scope_name;
}

SgName synthesizeTemplateArgumentQualifier(SgTemplateArgument *arg) {
  if (arg == NULL) {
    return SgName();
  }

  int qualification_length = arg->get_name_qualification_length_for_type();
  bool global_qualification = arg->get_global_qualification_required_for_type();

  if (qualification_length <= 0 && !global_qualification) {
    qualification_length = arg->get_name_qualification_length();
    global_qualification = arg->get_global_qualification_required();
  }

  if (qualification_length <= 0 && !global_qualification) {
    return SgName();
  }

  SgDeclarationStatement *declaration =
      referencedDeclarationForTemplateArgument(arg);
  if (declaration == NULL) {
    return SgName();
  }

  SgScopeStatement *scope = scopeForTemplateArgumentQualification(declaration);
  if (scope == NULL) {
    return SgName();
  }

  std::vector<std::string> components;
  while (scope != NULL &&
         static_cast<int>(components.size()) < qualification_length) {
    if (isSgGlobal(scope) != NULL) {
      break;
    }

    SgScopeStatement *next_scope = NULL;
    std::string scope_name =
        nameForTemplateArgumentQualificationScope(scope, next_scope);
    if (!scope_name.empty()) {
      components.push_back(scope_name);
    }

    if (next_scope == scope) {
      break;
    }
    scope = next_scope;
  }

  if (components.empty() && !global_qualification) {
    return SgName();
  }

  std::string qualifier;
  if (global_qualification) {
    qualifier = "::";
  }

  for (std::vector<std::string>::reverse_iterator i = components.rbegin();
       i != components.rend(); ++i) {
    qualifier += *i;
    qualifier += "::";
  }

  return SgName(qualifier);
}

} // namespace

// DQ (5/11/2011): New name qualification for ROSE (the 4th try).
// This is a part of a rewrite of the name qualification support in ROSE with
// the follwoing properties:
//    1) It is exact (no over qualification).
//    2) It handles visibility of names constructs
//    3) It resolves ambiguity of named constructs.
//    4) It resolves where type elaboration is required.
//    5) The inputs are carried in the SgUnparse_Info object for uniform
//    handling. 6) The the values in the SgUnparse_Info object are copied from
//    the AST references to the named
//       constructs to avoid where named constructs are referenced from multiple
//       locations and the name qualification might be different.
//
//    7) What about base class qualification? I might have forgotten this one!
//    No, this works,
//       but might not generate the minimum length qualified name.

SgName Unparser_Nameq::lookup_generated_qualified_name(SgNode *referencedNode) {
  // These are all of the types of IR nodes that can reference anything that is
  // qualified. It is a longer list than I expected (or designed for initially),
  // but still not unreasonable.

  SgName nameQualifier;

  if (referencedNode == NULL) {
    // DQ (6/25/2011): This is the case of the using the unparseToString()
    // function.  Our more sophisticated name qualification support is not
    // possible to support in this case (because we don;'t have the scope from
    // which to compute the qualified name) and so a fully qualified name is
    // generated by default.

    // printf ("Note that info.set_reference_node_for_qualification(SgNode*)
    // should have been called before calling this function. \n"); DQ
    // (6/23/2011): This test fails this assertion:
    // tests/nonsmoke/functional/roseTests/programAnalysisTests/testCallGraphAnalysis/test3.C
    // but allow it to pass as a test.
    // printf ("WARNING: referencedNode in
    // Unparser_Nameq::lookup_generated_qualified_name() should be a valid
    // pointer! \n");
    return nameQualifier;
  }
  ASSERT_not_null(referencedNode);

  // TV (10/24/2018): (ROSE-1399) unparsing template from AST requires to
  // namequal expressions in template arguments
  SgExpression *expr = isSgExpression(referencedNode);
  if (expr != NULL) {
    nameQualifier = expr->get_qualified_name_prefix_for_referenced_type();
    return nameQualifier;
  }

  switch (referencedNode->variantT()) {
  case V_SgInitializedName: {
    SgInitializedName *initializedName = isSgInitializedName(referencedNode);
    nameQualifier = initializedName->get_qualified_name_prefix_for_type();
    break;
  }

    // DQ (12/29/2011): Added cases for new template IR nodes.
  case V_SgTemplateFunctionDeclaration:
  case V_SgTemplateMemberFunctionDeclaration:

  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration:
  case V_SgTemplateInstantiationFunctionDecl:
  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgFunctionDeclaration *node = isSgFunctionDeclaration(referencedNode);
    nameQualifier = node->get_qualified_name_prefix_for_return_type();
    break;
  }

    // DQ (11/3/2014): Added support for templated typedef (part of C++11
    // support).
  case V_SgTemplateTypedefDeclaration:
  case V_SgTemplateInstantiationTypedefDeclaration:
  case V_SgTypedefDeclaration: {
    SgTypedefDeclaration *node = isSgTypedefDeclaration(referencedNode);
    nameQualifier = node->get_qualified_name_prefix_for_base_type();
    break;
  }

    // DQ (2/18/2019): Adding support for name qualification of enum declaration
    // in typedef declarations (and SgClassDeclaration,
    // SgTemplateInstantiationDecl).
  case V_SgTemplateInstantiationDecl:
  case V_SgClassDeclaration:
  case V_SgEnumDeclaration: {
    // SgEnumDeclaration* node = isSgEnumDeclaration(referencedNode);
    SgDeclarationStatement *node = isSgDeclarationStatement(referencedNode);
    // DQ (2/18/2019): If this works then we might want to generate an
    // associated get_qualified_name_prefix_for_base_type() function for the
    // SgEnumDeclaration. nameQualifier =
    // node->get_qualified_name_prefix_for_base_type();

    // std::map<SgNode*,std::string>::iterator i =
    // SgNode::get_globalQualifiedNameMapForTypes().find(const_cast<SgTypedefDeclaration*>(this));
    // std::map<SgNode*,std::string>::iterator i =
    // SgNode::get_globalQualifiedNameMapForTypes().find(node);
    // std::map<SgNode*,std::string>::iterator i =
    // SgNode::get_qualifiedNameMapForNames().find(node);
    SgUnorderedMapNodeToString::iterator i =
        SgNode::get_globalQualifiedNameMapForNames().find(node);

    // if (i != SgNode::get_globalQualifiedNameMapForTypes().end())
    if (i != SgNode::get_globalQualifiedNameMapForNames().end()) {
      // DQ (2/22/2019): Added assertion.
      ROSE_ASSERT(node == i->first);

      nameQualifier = i->second;
    } else {
    }

    break;
  }

  case V_SgNonrealDecl: {
    SgNonrealDecl *node = isSgNonrealDecl(referencedNode);
    SgUnorderedMapNodeToString::iterator i =
        SgNode::get_globalQualifiedNameMapForNames().find(node);
    if (i != SgNode::get_globalQualifiedNameMapForNames().end()) {
      ROSE_ASSERT(node == i->first);
      nameQualifier = i->second;
    }
    break;
  }

  case V_SgTemplateArgument: {
    SgTemplateArgument *node = isSgTemplateArgument(referencedNode);
    nameQualifier = node->get_qualified_name_prefix_for_type();
    break;
  }

    // DQ (7/13/2013): I think we need this here, but wait until we generate the
    // error to drive it to be introduced. Also this does not permit handling of
    // multiple types requiring different name qualification (same as for throw
    // support). DQ (7/12/2013): Added support to type trait builtin functions
  case V_SgTypeTraitBuiltinOperator:

  case V_SgAssignInitializer:

    // DQ (8/19/2013): Added support for constructor initializers that might
    // have an associated qualified name string associated with the templated
    // class or instantiated template class.
  case V_SgConstructorInitializer:

    // DQ (9/5/2015): I think this is the support we need for test2015_57.C
    // (compound literals used as expressions).
  case V_SgAggregateInitializer:

    // DQ (9/12/2016): Adding support for whatever types are used within alignOf
    // operators.
  case V_SgAlignOfOp:

    // DQ (1/19/2019): Added support for SgDotExp (required for some
    // unparseToString_tests test codes (e.g. test2010_24.C, and a dozen
    // others).
  case V_SgDotExp:

  case V_SgTypeIdOp:
  case V_SgSizeOfOp:
  case V_SgNewExp:
  case V_SgCastExp: {
    // SgCastExp* node = isSgCastExp(referencedNode);
    SgExpression *node = isSgExpression(referencedNode);
    nameQualifier = node->get_qualified_name_prefix_for_referenced_type();
    break;
  }
  case V_SgClassType: {
    // These can appear in throw expression lists...ignore for now...
    // SgType* node = isSgType(referencedNode);
    // nameQualifier = node->get_qualified_name_prefix_for_type();
    // printf ("WARNING: Note that qualified types in throw expression lists are
    // not yet supported... \n");
    break;
  }

  case V_SgFunctionType: {
    // These can appear in typedefs of function pointers...ignore for now...
    // SgType* node = isSgType(referencedNode);
    // nameQualifier = node->get_qualified_name_prefix_for_type();
    // printf ("WARNING: Note that qualified types in function pointer typedefs
    // are not yet supported... \n");
    break;
  }

  case V_SgTypedefType: {
    // These can appear in typedef types...ignore for now...
    // SgType* node = isSgType(referencedNode);
    // nameQualifier = node->get_qualified_name_prefix_for_type();
    // printf ("WARNING: Note that qualified types in typedef types are not yet
    // supported... \n");
    break;
  }

  case V_SgPointerMemberType: {
    SgPointerMemberType *node = isSgPointerMemberType(referencedNode);
    // nameQualifier = node->get_qualified_name_prefix_for_type();

    // nameQualifier = node->get_qualified_name_prefix_for_base_type();
    nameQualifier = node->get_qualified_name_prefix_for_class_of();
    break;
  }

  default: {
    printf("In Unparser_Nameq::lookup_generated_qualified_name(): Sorry not "
           "implemented case of name qualification for "
           "info.get_reference_node_for_qualification() = %s \n",
           referencedNode->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return nameQualifier;
}

// DQ (3/14/2019): Adding debugging support to output the map of names.
void Unparser_Nameq::outputNameQualificationMap(
    const SgUnorderedMapNodeToString &qualifiedNameMap) {
  printf("qualifiedNameMap.size() = %zu \n", qualifiedNameMap.size());
  SgUnorderedMapNodeToString::const_iterator i = qualifiedNameMap.begin();
  while (i != qualifiedNameMap.end()) {
    ASSERT_not_null(i->first);

    printf(" --- *i = i->first = %p = %s i->second = %s \n", i->first,
           i->first->class_name().c_str(), i->second.c_str());

    i++;
  }
}
