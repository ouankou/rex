/**
 *  \file Transform/Insert.cc
 *
 *  \brief Inserts the outlined function declarations (i.e., including
 *  prototypes) and function calls.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>

#include <iostream>

#include <list>

#include <set>

#include <sstream>

#include <string>

#include "ASTtools.hh"

#include "Copy.hh"

#include "Outliner.hh"
#include "Preprocess.hh"

#include "PreprocessingInfo.hh"

#include "RoseAst.h"

// =====================================================================

typedef std::vector<SgFunctionDeclaration *> FuncDeclList_t;

// =====================================================================

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;
using namespace Outliner;

// =====================================================================

//! Creates a 'prototype' (forward declaration) for a function.
static std::string getTemplateParameterName(SgTemplateParameter *param) {
  if (param == NULL)
    return "";

  if (SgTemplateType *template_type = isSgTemplateType(param->get_type())) {
    std::string name = template_type->get_name().getString();
    if (!name.empty())
      return name;
  }
  if (SgNonrealType *nonreal_type = isSgNonrealType(param->get_type())) {
    std::string name = nonreal_type->get_name().getString();
    if (!name.empty())
      return name;
  }
  if (SgInitializedName *init_name = param->get_initializedName()) {
    std::string name = init_name->get_name().getString();
    if (!name.empty())
      return name;
  }
  if (SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(param->get_templateDeclaration())) {
    std::string name = nonreal_decl->get_name().getString();
    if (!name.empty())
      return name;
  }

  return "";
}

static void
collectTemplateParameterNames(const SgTemplateParameterPtrList &params,
                              std::set<std::string> &names) {
  for (SgTemplateParameter *param : params) {
    std::string name = getTemplateParameterName(param);
    if (!name.empty())
      names.insert(name);
  }
}

static void
collectTemplateParameterNamesFromClassDecl(SgClassDeclaration *class_decl,
                                           std::set<std::string> &names) {
  if (class_decl == NULL)
    return;

  if (SgTemplateClassDeclaration *template_decl =
          isSgTemplateClassDeclaration(class_decl)) {
    collectTemplateParameterNames(template_decl->get_templateParameters(),
                                  names);
    return;
  }

  if (SgTemplateInstantiationDecl *inst_decl =
          isSgTemplateInstantiationDecl(class_decl)) {
    if (SgTemplateClassDeclaration *template_decl =
            inst_decl->get_templateDeclaration()) {
      collectTemplateParameterNames(template_decl->get_templateParameters(),
                                    names);
    }
  }
}

static std::set<std::string>
collectEnclosingTemplateParameterNames(SgScopeStatement *scope) {
  std::set<std::string> names;
  std::set<SgScopeStatement *> visited;
  for (SgScopeStatement *current = scope;
       current != NULL && visited.insert(current).second;
       current = current->get_scope()) {
    if (SgClassDefinition *class_def = isSgClassDefinition(current)) {
      collectTemplateParameterNamesFromClassDecl(class_def->get_declaration(),
                                                 names);
      continue;
    }
    if (SgTemplateClassDefinition *template_def =
            isSgTemplateClassDefinition(current)) {
      collectTemplateParameterNamesFromClassDecl(
          template_def->get_declaration(), names);
      continue;
    }
    if (SgTemplateInstantiationDefn *inst_def =
            isSgTemplateInstantiationDefn(current)) {
      collectTemplateParameterNamesFromClassDecl(inst_def->get_declaration(),
                                                 names);
    }
  }

  return names;
}

static bool isClassLikeScope(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != NULL ||
         isSgTemplateClassDefinition(scope) != NULL ||
         isSgTemplateInstantiationDefn(scope) != NULL;
}

static std::string
makeAvailableTemplateParameterName(size_t index,
                                   std::set<std::string> &unavailable) {
  std::string candidate = "rose_out_tparam_" + std::to_string(index);
  for (size_t suffix = 0; unavailable.count(candidate) != 0; ++suffix) {
    candidate = "rose_out_tparam_" + std::to_string(index) + "_" +
                std::to_string(suffix);
  }
  unavailable.insert(candidate);
  return candidate;
}

static SgTemplateParameter *
cloneRenamedTemplateParameter(const SgTemplateParameter *source,
                              const std::string &name) {
  ROSE_ASSERT(source != NULL);
  ROSE_ASSERT(!name.empty());

  if (source->get_parameterType() == SgTemplateParameter::nontype_parameter) {
    SgType *parameter_type = source->get_type();
    const SgInitializedName *source_name = source->get_initializedName();
    if (SageInterface::containsUnknownType(parameter_type) ||
        source_name == nullptr || source_name->get_parent() != source ||
        source_name->get_type() != parameter_type) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-parameter-source]: source "
              "requires one exact typed declarator before cloning\n");
      ROSE_ABORT();
    }
  }

  SgTemplateParameter *copy =
      SageInterface::cloneDetachedGeneratedTemplateParameter(
          source, "outliner-prototype-template-parameter-rename");
  const SgName replacement(name);
  switch (copy->get_parameterType()) {
  case SgTemplateParameter::type_parameter:
  case SgTemplateParameter::template_parameter: {
    SgTemplateType *type = isSgTemplateType(copy->get_type());
    if (type == nullptr || type->get_template_parameter() != copy) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-parameter-rename]: copied "
              "type has no exact parameter owner\n");
      ROSE_ABORT();
    }
    type->set_name(replacement);
    if (copy->get_parameterType() == SgTemplateParameter::template_parameter) {
      SgTemplateDeclaration *declaration =
          isSgTemplateDeclaration(copy->get_templateDeclaration());
      if (declaration == nullptr ||
          declaration == source->get_templateDeclaration()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[template-parameter-rename]: copied "
                "template has no independent declaration\n");
        ROSE_ABORT();
      }
      declaration->set_name(replacement);
      if (SgTemplateDeclaration *surface =
              copy->get_sourceSpelledTemplateDeclaration()) {
        if (surface == source->get_sourceSpelledTemplateDeclaration()) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[template-parameter-rename]: copied "
                  "template shares its source declaration\n");
          ROSE_ABORT();
        }
        surface->set_name(replacement);
      }
    }
    break;
  }
  case SgTemplateParameter::nontype_parameter: {
    SgInitializedName *declarator = copy->get_initializedName();
    if (declarator == nullptr || declarator == source->get_initializedName() ||
        declarator->get_parent() != copy ||
        declarator->get_type() != copy->get_type()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[template-parameter-rename]: copied "
              "non-type parameter has no independent typed declarator\n");
      ROSE_ABORT();
    }
    declarator->set_name(replacement);
    break;
  }
  default:
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[template-parameter-rename]: kind=%d "
            "has no exact independent rename construction\n",
            static_cast<int>(copy->get_parameterType()));
    ROSE_ABORT();
  }

  return copy;
}

static SgTemplateParameter *
copyTemplateParameterForPrototype(SgTemplateParameter *source,
                                  std::set<std::string> *unavailable,
                                  size_t index) {
  ROSE_ASSERT(source != NULL);

  std::string source_name = getTemplateParameterName(source);
  if (unavailable != NULL && !source_name.empty() &&
      unavailable->count(source_name) != 0) {
    std::string replacement_name =
        makeAvailableTemplateParameterName(index, *unavailable);
    return cloneRenamedTemplateParameter(source, replacement_name);
  } else if (unavailable != NULL && !source_name.empty()) {
    unavailable->insert(source_name);
  }

  return SageInterface::cloneDetachedGeneratedTemplateParameter(
      source, "outliner-prototype-template-parameter");
}

enum class PrototypeDefinitionPolicy {
  forbidCrossOutputDefinition,
  preserveSameOutputFamily,
};

enum class PrototypeFamilyPolicy {
  canonicalReplacement,
  friendMember,
};

static SgFunctionDeclaration *
generatePrototype(const SgFunctionDeclaration *full_decl,
                  const SageBuilder::function_declaration_ownership &ownership,
                  SgScopeStatement *scope, bool forceFreeFunctionScope,
                  PrototypeDefinitionPolicy definitionPolicy,
                  PrototypeFamilyPolicy familyPolicy) {
  if (full_decl == NULL || scope == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[prototype-input]: declaration=%p "
            "scope=%p must name one exact prototype construction\n",
            static_cast<const void *>(full_decl), static_cast<void *>(scope));
    ROSE_ABORT();
  }

  SgFunctionDeclaration *definition =
      isSgFunctionDeclaration(full_decl->get_definingDeclaration());
  SgFunctionDeclaration *priorCanonical =
      isSgFunctionDeclaration(full_decl->get_firstNondefiningDeclaration());
  SgSourceFile *definitionFile =
      SageInterface::getEnclosingSourceFile(full_decl);
  SgSourceFile *prototypeFile = SageInterface::getEnclosingSourceFile(scope);
  if (definition == NULL || definition != full_decl || priorCanonical == NULL ||
      definitionFile == NULL || prototypeFile == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
            "definition=%p canonical=%p definition-file=%p "
            "prototype-file=%p has no exact construction-time family\n",
            static_cast<void *>(definition),
            static_cast<void *>(priorCanonical),
            static_cast<void *>(definitionFile),
            static_cast<void *>(prototypeFile));
    ROSE_ABORT();
  }

  std::vector<SgFunctionDeclaration *> sameOutputFamily;
  if (definitionPolicy == PrototypeDefinitionPolicy::preserveSameOutputFamily) {
    if (definitionFile != prototypeFile) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
              "definition=%p and prototype scope=%p belong to distinct "
              "physical output files\n",
              static_cast<const void *>(full_decl), static_cast<void *>(scope));
      ROSE_ABORT();
    }
    for (SgNode *node :
         NodeQuery::querySubTree(prototypeFile, V_SgFunctionDeclaration)) {
      SgFunctionDeclaration *familyMember = isSgFunctionDeclaration(node);
      if (familyMember != NULL && familyMember->get_scope() == scope &&
          familyMember->get_firstNondefiningDeclaration() == priorCanonical) {
        if (familyMember->get_name() != full_decl->get_name() ||
            familyMember->get_type() != full_decl->get_type()) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
                  "family member=%p differs from definition=%p\n",
                  static_cast<void *>(familyMember),
                  static_cast<const void *>(full_decl));
          ROSE_ABORT();
        }
        sameOutputFamily.push_back(familyMember);
      }
    }
    if (std::find(sameOutputFamily.begin(), sameOutputFamily.end(),
                  full_decl) == sameOutputFamily.end() ||
        std::find(sameOutputFamily.begin(), sameOutputFamily.end(),
                  priorCanonical) == sameOutputFamily.end()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
              "destination family omits definition=%p or canonical=%p\n",
              static_cast<const void *>(full_decl),
              static_cast<void *>(priorCanonical));
      ROSE_ABORT();
    }
  }

  auto enforce_definition_edge_contract =
      [&](SgFunctionDeclaration *prototype) -> SgFunctionDeclaration * {
    ROSE_ASSERT(prototype != NULL);
    if (definitionPolicy ==
        PrototypeDefinitionPolicy::preserveSameOutputFamily) {
      if (prototype->get_scope() != scope ||
          prototype->get_type() != full_decl->get_type()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
                "prototype=%p does not match the exact destination family\n",
                static_cast<void *>(prototype));
        ROSE_ABORT();
      }

      if (familyPolicy == PrototypeFamilyPolicy::friendMember) {
        if (prototype->get_firstNondefiningDeclaration() == prototype) {
          // A source-surface friend may be the first emitted declaration for
          // a family whose previous canonical declaration was semantic-only.
          // The builder deliberately replaces that hidden canonical identity;
          // complete the same construction-time transaction for every prior
          // destination member.
          for (SgFunctionDeclaration *familyMember : sameOutputFamily) {
            familyMember->set_firstNondefiningDeclaration(prototype);
            if (familyMember != definition)
              familyMember->set_definingDeclaration(definition);
          }
          prototype->set_definingDeclaration(definition);
          definition->set_firstNondefiningDeclaration(prototype);
          for (SgFunctionDeclaration *familyMember : sameOutputFamily) {
            if (familyMember->get_firstNondefiningDeclaration() != prototype ||
                familyMember->get_definingDeclaration() != definition) {
              fprintf(stderr,
                      "REX_OUTLINER_INVARIANT"
                      "[prototype-definition-policy]: destination friend=%p "
                      "did not replace semantic canonical=%p for "
                      "definition=%p\n",
                      static_cast<void *>(prototype),
                      static_cast<void *>(priorCanonical),
                      static_cast<void *>(definition));
              ROSE_ABORT();
            }
          }
          return prototype;
        }
        if (prototype == priorCanonical ||
            prototype->get_firstNondefiningDeclaration() != priorCanonical ||
            prototype->get_definingDeclaration() != definition ||
            definition->get_firstNondefiningDeclaration() != priorCanonical ||
            priorCanonical->get_definingDeclaration() != definition) {
          fprintf(
              stderr,
              "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
              "friend=%p first=%p defining=%p canonical=%p "
              "canonical-defining=%p definition=%p definition-first=%p "
              "in the same output family\n",
              static_cast<void *>(prototype),
              static_cast<void *>(prototype->get_firstNondefiningDeclaration()),
              static_cast<void *>(prototype->get_definingDeclaration()),
              static_cast<void *>(priorCanonical),
              static_cast<void *>(priorCanonical->get_definingDeclaration()),
              static_cast<void *>(definition),
              static_cast<void *>(
                  definition->get_firstNondefiningDeclaration()));
          ROSE_ABORT();
        }
        return prototype;
      }

      if (prototype->get_firstNondefiningDeclaration() != prototype) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
                "prototype=%p did not become the exact destination "
                "canonical declaration\n",
                static_cast<void *>(prototype));
        ROSE_ABORT();
      }
      for (SgFunctionDeclaration *familyMember : sameOutputFamily) {
        familyMember->set_firstNondefiningDeclaration(prototype);
        if (familyMember != definition)
          familyMember->set_definingDeclaration(definition);
      }
      prototype->set_definingDeclaration(definition);
      definition->set_firstNondefiningDeclaration(prototype);

      for (SgFunctionDeclaration *familyMember : sameOutputFamily) {
        if (familyMember->get_firstNondefiningDeclaration() != prototype ||
            familyMember->get_definingDeclaration() != definition) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
                  "family member=%p did not join prototype=%p definition=%p\n",
                  static_cast<void *>(familyMember),
                  static_cast<void *>(prototype),
                  static_cast<void *>(definition));
          ROSE_ABORT();
        }
      }
      return prototype;
    }

    if (definitionFile == prototypeFile) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
              "cross-output policy was selected for one physical output "
              "file\n");
      ROSE_ABORT();
    }

    std::set<SgFunctionDeclaration *> source_declarations;
    source_declarations.insert(prototype);
    if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
            prototype->get_firstNondefiningDeclaration()))
      source_declarations.insert(first_nondef);

    for (SgFunctionDeclaration *source_decl : source_declarations) {
      if (source_decl == priorCanonical || source_decl == definition) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
                "cross-output prototype reused destination declaration=%p\n",
                static_cast<void *>(source_decl));
        ROSE_ABORT();
      }
      source_decl->set_definingDeclaration(NULL);
      ROSE_ASSERT(source_decl->get_definingDeclaration() == NULL);
    }
    if (full_decl->get_firstNondefiningDeclaration() != priorCanonical ||
        definition->get_definingDeclaration() != definition) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[prototype-definition-policy]: "
              "cross-output prototype mutated destination family "
              "definition=%p canonical=%p\n",
              static_cast<void *>(definition),
              static_cast<void *>(priorCanonical));
      ROSE_ABORT();
    }
    return prototype;
  };

  if (SgTemplateFunctionDeclaration *template_decl =
          isSgTemplateFunctionDeclaration(
              const_cast<SgFunctionDeclaration *>(full_decl))) {
    SgFunctionType *funcType = template_decl->get_type();
    SgType *return_type = funcType->get_return_type();
    SgFunctionParameterList *paralist =
        SageBuilder::buildGeneratedFunctionParameterList(
            template_decl->get_parameterList());
    SgTemplateParameterPtrList *template_params =
        new SgTemplateParameterPtrList();
    std::set<std::string> unavailable_template_names;
    std::set<std::string> *unavailable_template_name_ptr = NULL;
    SgScopeStatement *templateContext =
        ownership.getSourceLexicalOwner() != nullptr
            ? ownership.getSourceLexicalOwner()
            : scope;
    if (isClassLikeScope(templateContext)) {
      unavailable_template_names =
          collectEnclosingTemplateParameterNames(templateContext);
      unavailable_template_name_ptr = &unavailable_template_names;
    }
    size_t template_param_index = 0;
    for (SgTemplateParameterPtrList::const_iterator it =
             template_decl->get_templateParameters().begin();
         it != template_decl->get_templateParameters().end();
         ++it, ++template_param_index) {
      if (*it == NULL)
        continue;
      SgTemplateParameter *param_copy = copyTemplateParameterForPrototype(
          *it, unavailable_template_name_ptr, template_param_index);
      template_params->push_back(param_copy);
    }

    SgTemplateFunctionDeclaration *proto =
        SageBuilder::buildNondefiningTemplateFunctionDeclaration(
            ownership, template_decl->get_name(), return_type, paralist, scope,
            template_params);
    delete template_params;
    ROSE_ASSERT(proto != NULL);
    for (SgTemplateParameterPtrList::iterator it =
             proto->get_templateParameters().begin();
         it != proto->get_templateParameters().end(); ++it) {
      if (*it != NULL)
        (*it)->set_parent(proto);
    }

    if (full_decl->get_functionModifier().isInline())
      proto->get_functionModifier().setInline();

    proto->set_linkage(full_decl->get_linkage());
    if (full_decl->get_declarationModifier().get_storageModifier().isExtern() ==
        true) {
      proto->get_declarationModifier().get_storageModifier().setExtern();
    }

    ROSE_ASSERT(proto->get_firstNondefiningDeclaration() != NULL);
    return enforce_definition_edge_contract(proto);
  }

  // DQ (2/23/2009): Use this code instead.
  SgFunctionType *funcType = full_decl->get_type();
  SgType *return_type = funcType->get_return_type();
  SgFunctionParameterList *paralist =
      SageBuilder::buildGeneratedFunctionParameterList(
          full_decl->get_parameterList());
  SgFunctionDeclaration *proto =
      SageBuilder::buildNondefiningFunctionDeclaration(
          ownership, full_decl->get_name(), return_type, paralist, scope, false,
          NULL, SgStorageModifier::e_default, forceFreeFunctionScope);
  ROSE_ASSERT(proto != NULL);

  // Inherit defining function's inline property: avoid linking error when
  // linking multiple .lib files with the outlined functions
  if (full_decl->get_functionModifier().isInline())
    proto->get_functionModifier().setInline();

  // Keep language linkage consistent with the defining declaration (e.g.,
  // outlined functions may be forced to `extern "C"`).
  proto->set_linkage(full_decl->get_linkage());
  if (full_decl->get_declarationModifier().get_storageModifier().isExtern() ==
      true) {
    proto->get_declarationModifier().get_storageModifier().setExtern();
  }

  ROSE_ASSERT(full_decl->get_definition() != NULL);
  ROSE_ASSERT(full_decl->get_definingDeclaration() == full_decl);

  ROSE_ASSERT(proto != NULL);
  ROSE_ASSERT(proto->get_firstNondefiningDeclaration() != NULL);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(proto) != NULL);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(scope) != NULL);

  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(
                  proto->get_firstNondefiningDeclaration()) != NULL);

  return enforce_definition_edge_contract(proto);
}

//! Generates a 'friend' declaration from a given function declaration.
// For a friend declaration, two scopes are involved.
//'scope' is the class definition within which the friend declaration is
// inserted. 'class_scope' is the class definition's SgClassDeclaration's scope
// in which the function symbol should be created, if not exist.
static SgFunctionDeclaration *
generateFriendPrototype(const SgFunctionDeclaration *full_decl,
                        SgScopeStatement *scope,
                        SgScopeStatement *class_scope) {
  ROSE_ASSERT(class_scope != NULL);

  SgGlobal *definitionGlobal = SageInterface::getGlobalScope(full_decl);
  SgGlobal *friendGlobal = SageInterface::getGlobalScope(class_scope);
  if (definitionGlobal == NULL || friendGlobal == NULL ||
      friendGlobal != class_scope) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[friend-family-policy]: definition=%p "
            "definition-global=%p friend-scope=%p friend-global=%p has no "
            "exact global family\n",
            static_cast<const void *>(full_decl),
            static_cast<void *>(definitionGlobal),
            static_cast<void *>(class_scope),
            static_cast<void *>(friendGlobal));
    ROSE_ABORT();
  }
  const PrototypeDefinitionPolicy definitionPolicy =
      definitionGlobal == friendGlobal
          ? PrototypeDefinitionPolicy::preserveSameOutputFamily
          : PrototypeDefinitionPolicy::forbidCrossOutputDefinition;

  if (enable_debug) {
#ifdef __linux__
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
    cout << "\t source func decl is:" << full_decl << endl;
    full_decl->get_file_info()->display();
#endif

    cout << "\t target class definition is:" << scope << endl;
    scope->get_file_info()->display();

    cout << "\t target class declaration's scope is:" << class_scope << endl;
    class_scope->get_file_info()->display();
  }

  SgFunctionDeclaration *proto = generatePrototype(
      full_decl,
      SageBuilder::function_declaration_ownership::sourceLexicalAtTop(scope),
      class_scope, true, definitionPolicy, PrototypeFamilyPolicy::friendMember);
  ROSE_ASSERT(proto != NULL);

  // Remove any 'extern' modifiers
  proto->get_declarationModifier().get_storageModifier().reset();

  // Set the 'friend' modifier
  proto->get_declarationModifier().setFriend();
  SgAccessModifier &friendAccess =
      proto->get_declarationModifier().get_accessModifier();
  friendAccess.setNotApplicable();
  friendAccess.set_is_explicit(false);

  // The declaration is lexically owned by the class but semantically belongs
  // to the class declaration's enclosing scope.  Its symbol is therefore
  // published in class_scope directly; no class-scope symbol is created and no
  // late symbol/scope repair is required.
  ROSE_ASSERT(proto->get_parent() == scope);
  ROSE_ASSERT(proto->get_scope() == class_scope);

  ROSE_ASSERT(
      proto->get_definingDeclaration() ==
      (definitionPolicy == PrototypeDefinitionPolicy::preserveSameOutputFamily
           ? full_decl
           : NULL));
  return proto;
}

/*!
 *  \brief Beginning at the given declaration statement, this routine
 *  resolves the exact lexical global insertion boundary for this declaration.
 *  A declaration owned by SgAuxiliaryDeclarationList is semantic lookup state,
 *  not a lexical boundary.  A generated source declaration needed by such a
 *  semantic target must precede the emitted global declaration sequence; the
 *  first direct declaration is therefore its exact insertion boundary.  A
 *  null result means the global declaration sequence is empty.
 */
static SgDeclarationStatement *
findClosestGlobalInsertPoint(SgDeclarationStatement *f) {
  ROSE_ASSERT(f);
  SgDeclarationStatement *closest = f;
  SgNode *cur_parent = f->get_parent();
  while (cur_parent && !isSgGlobal(cur_parent)) {
    if (isSgDeclarationStatement(cur_parent))
      closest = isSgDeclarationStatement(cur_parent);
    cur_parent = cur_parent->get_parent();
  }
  SgGlobal *global = isSgGlobal(cur_parent);
  if (global == NULL)
    return NULL;

  const SgDeclarationStatementPtrList &declarations =
      global->getDeclarationList();
  if (std::find(declarations.begin(), declarations.end(), closest) !=
      declarations.end())
    return closest;

  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(f->get_parent());
  if (auxiliary == NULL || auxiliary->get_parent() != global ||
      f->get_scope() != global ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), f) != 1) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-insertion-boundary]: "
            "declaration=%p/%s name=%s parent=%p scope=%p has neither one "
            "direct global source owner nor one exact auxiliary semantic "
            "owner\n",
            static_cast<void *>(f), f->class_name().c_str(),
            SageInterface::get_name(f).c_str(),
            static_cast<void *>(f->get_parent()),
            static_cast<void *>(f->get_scope()));
    ROSE_ABORT();
  }

  if (!declarations.empty() && declarations.front() == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-insertion-boundary]: global=%p "
            "contains a null first lexical declaration\n",
            static_cast<void *>(global));
    ROSE_ABORT();
  }
  return declarations.empty() ? NULL : declarations.front();
}

static bool declarationAppearsNoLaterThan(SgGlobal *scope,
                                          SgDeclarationStatement *candidate,
                                          SgDeclarationStatement *limit) {
  ROSE_ASSERT(scope != NULL);
  if (limit == NULL || candidate == limit)
    return true;
  if (candidate == NULL)
    return false;

  const SgDeclarationStatementPtrList &declarations =
      scope->getDeclarationList();
  for (SgDeclarationStatementPtrList::const_iterator it = declarations.begin();
       it != declarations.end(); ++it) {
    if (*it == candidate)
      return true;
    if (*it == limit)
      return false;
  }

  SgDeclarationGroupStatement *candidate_group =
      isSgDeclarationGroupStatement(candidate->get_parent());
  SgDeclarationGroupStatement *limit_group =
      isSgDeclarationGroupStatement(limit->get_parent());
  auto group_member_index = [](SgDeclarationGroupStatement *group,
                               SgDeclarationStatement *member) {
    if (group == NULL)
      return static_cast<size_t>(-1);
    const SgDeclarationStatementPtrList &members = group->get_declarations();
    SgDeclarationStatementPtrList::const_iterator position =
        std::find(members.begin(), members.end(), member);
    return position != members.end()
               ? static_cast<size_t>(std::distance(members.begin(), position))
               : static_cast<size_t>(-1);
  };
  fprintf(
      stderr,
      "REX_OUTLINER_INVARIANT[prototype-insertion-order]: scope=%p "
      "candidate=%p/%s name=%s parent=%p/%s grandparent=%p scope=%p "
      "global=%p group-index=%zu group-size=%zu "
      "limit=%p/%s name=%s parent=%p/%s grandparent=%p scope=%p "
      "global=%p group-index=%zu group-size=%zu are not both present in "
      "the requested global declaration list\n",
      static_cast<void *>(scope), static_cast<void *>(candidate),
      candidate->class_name().c_str(),
      SageInterface::get_name(candidate).c_str(),
      static_cast<void *>(candidate->get_parent()),
      candidate->get_parent() != NULL
          ? candidate->get_parent()->class_name().c_str()
          : "<null>",
      static_cast<void *>(candidate->get_parent() != NULL
                              ? candidate->get_parent()->get_parent()
                              : NULL),
      static_cast<void *>(candidate->get_scope()),
      static_cast<void *>(SageInterface::getGlobalScope(candidate)),
      group_member_index(candidate_group, candidate),
      candidate_group != NULL ? candidate_group->get_declarations().size() : 0,
      static_cast<void *>(limit),
      limit != NULL ? limit->class_name().c_str() : "<null>",
      limit != NULL ? SageInterface::get_name(limit).c_str() : "<null>",
      static_cast<void *>(limit != NULL ? limit->get_parent() : NULL),
      limit != NULL && limit->get_parent() != NULL
          ? limit->get_parent()->class_name().c_str()
          : "<null>",
      static_cast<void *>(limit != NULL && limit->get_parent() != NULL
                              ? limit->get_parent()->get_parent()
                              : NULL),
      static_cast<void *>(limit != NULL ? limit->get_scope() : NULL),
      static_cast<void *>(limit != NULL ? SageInterface::getGlobalScope(limit)
                                        : NULL),
      group_member_index(limit_group, limit),
      limit_group != NULL ? limit_group->get_declarations().size() : 0);
  ROSE_ABORT();
}

static bool
isPureCommentOrBlankPreprocessingInfo(const PreprocessingInfo *info) {
  if (info == NULL)
    return false;

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
  case PreprocessingInfo::CpreprocessorBlankLine:
    return true;
  default:
    break;
  }

  return false;
}

void Outliner::Preprocess::moveLeadingPreprocessorPrefixForPrototype(
    SgStatement *src, SgStatement *dest) {
  ROSE_ASSERT(src != NULL);
  ROSE_ASSERT(dest != NULL);

  AttachedPreprocessingInfoType *src_info =
      src->get_attachedPreprocessingInfoPtr();
  if (src_info == NULL || src_info->empty())
    return;

  AttachedPreprocessingInfoType::size_type last_directive = src_info->size();
  for (AttachedPreprocessingInfoType::size_type i = 0; i < src_info->size();
       ++i) {
    PreprocessingInfo *info = (*src_info)[i];
    if (info != NULL &&
        info->getRelativePosition() == PreprocessingInfo::before &&
        !isPureCommentOrBlankPreprocessingInfo(info)) {
      last_directive = i;
    }
  }

  if (last_directive == src_info->size())
    return;

  AttachedPreprocessingInfoType moved;
  for (AttachedPreprocessingInfoType::size_type i = 0; i < src_info->size();
       ++i) {
    PreprocessingInfo *info = (*src_info)[i];
    if (i <= last_directive && info != NULL &&
        info->getRelativePosition() == PreprocessingInfo::before) {
      moved.push_back(info);
    }
  }

  for (AttachedPreprocessingInfoType::reverse_iterator info = moved.rbegin();
       info != moved.rend(); ++info) {
    SageInterface::publishPreprocessingInfoPhysicalOutputOwner(*info, dest);
    src->transferPreprocessingInfo(
        *info, dest, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::front);
  }
}

static SgFunctionDeclaration *
findExactCanonicalDeclarationInScope(SgFunctionDeclaration *definition,
                                     const FuncDeclList_t &friend_declarations,
                                     SgScopeStatement *source_scope) {
  ROSE_ASSERT(definition != NULL);
  ROSE_ASSERT(source_scope != NULL);

  SgFunctionDeclaration *result = NULL;
  auto consider = [&](SgFunctionDeclaration *declaration) {
    if (declaration == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-prototype-canonical]: source "
              "declaration family contains a null declaration\n");
      ROSE_ABORT();
    }
    if (declaration->get_scope() != source_scope)
      return;

    SgFunctionDeclaration *canonical =
        isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
    SgFunctionSymbol *symbol =
        canonical != NULL
            ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
            : NULL;
    SgSymbolTable *symbol_table = source_scope->get_symbol_table();
    if (canonical == NULL || canonical->get_scope() != source_scope ||
        canonical->get_firstNondefiningDeclaration() != canonical ||
        symbol == NULL || symbol->get_symbol_basis() != canonical ||
        symbol_table == NULL || symbol->get_parent() != symbol_table ||
        !symbol_table->exists(symbol)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-prototype-canonical]: "
              "declaration=%p canonical=%p symbol=%p basis=%p scope=%p does "
              "not identify one exact source declaration family\n",
              static_cast<void *>(declaration), static_cast<void *>(canonical),
              static_cast<void *>(symbol),
              static_cast<void *>(symbol != NULL ? symbol->get_symbol_basis()
                                                 : NULL),
              static_cast<void *>(source_scope));
      ROSE_ABORT();
    }
    if (result != NULL && result != canonical) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-prototype-canonical]: "
              "definition=%p has distinct canonical declarations %p and %p "
              "in source scope=%p\n",
              static_cast<void *>(definition), static_cast<void *>(result),
              static_cast<void *>(canonical),
              static_cast<void *>(source_scope));
      ROSE_ABORT();
    }
    result = canonical;
  };

  consider(definition);
  for (SgFunctionDeclaration *friend_declaration : friend_declarations)
    consider(friend_declaration);
  return result;
}

void Outliner::Preprocess::preparePrototypeParameterDeclarations(
    const SgFunctionDeclaration *prototype, SgGlobal *scope,
    SgDeclarationStatement *insert_point) {
  ASSERT_not_null(prototype);
  ASSERT_not_null(scope);
  ROSE_ASSERT(insert_point != nullptr && insert_point->get_parent() == scope &&
              scope->statementExistsInScope(insert_point));
  for (SgDeclarationStatement *dependency :
       SageInterface::getDependentDeclarations(
           prototype->get_parameterList())) {
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(dependency);
    if (classDeclaration == NULL ||
        classDeclaration->variantT() != V_SgClassDeclaration ||
        classDeclaration->get_scope() != scope ||
        findClosestGlobalInsertPoint(classDeclaration) != insert_point) {
      continue;
    }
    SgClassDeclaration *canonical = isSgClassDeclaration(
        classDeclaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != NULL);
    ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
    ROSE_ASSERT(canonical->get_scope() == scope);
    ROSE_ASSERT(!canonical->get_isUnNamed());
    const SgDeclarationStatementPtrList &declarations =
        scope->get_declarations();
    const auto boundary =
        std::find(declarations.begin(), declarations.end(), insert_point);
    ROSE_ASSERT(boundary != declarations.end());
    const bool alreadyIntroduced = std::any_of(
        declarations.begin(), boundary,
        [canonical](SgDeclarationStatement *declaration) {
          SgClassDeclaration *introduction = isSgClassDeclaration(declaration);
          return introduction != NULL &&
                 introduction->get_firstNondefiningDeclaration() == canonical;
        });
    if (alreadyIntroduced)
      continue;

    SgClassDeclaration *forward = buildNondefiningClassDeclaration_nfi(
        declaration_ownership::transformationDetached(), canonical->get_name(),
        canonical->get_class_type(), scope, NULL, NULL, NULL, NULL);
    ROSE_ASSERT(forward->get_firstNondefiningDeclaration() == canonical);
    ROSE_ASSERT(forward->get_type() == canonical->get_type());
    insertStatementBefore(insert_point, forward, false);
    moveLeadingPreprocessorPrefixForPrototype(insert_point, forward);
  }
}

static SgFunctionDeclaration *
insertGlobalPrototypeAtBoundary(SgFunctionDeclaration *def_decl,
                                SgGlobal *scope, SgDeclarationStatement *target,
                                SgFunctionDeclaration *canonical_target,
                                bool requires_external_linkage) {
  ASSERT_not_null(def_decl);
  ASSERT_not_null(scope);
  SgGlobal *definition_global = SageInterface::getGlobalScope(def_decl);
  ASSERT_not_null(definition_global);
  const bool separate_outputs = definition_global != scope;
  SgDeclarationStatement *insert_point =
      target != NULL ? findClosestGlobalInsertPoint(target) : NULL;
  if (insert_point != nullptr)
    Outliner::Preprocess::preparePrototypeParameterDeclarations(def_decl, scope,
                                                                insert_point);

  SageBuilder::function_declaration_ownership ownership =
      canonical_target != NULL
          ? insert_point != NULL
                ? SageBuilder::function_declaration_ownership::
                      sourceLexicalCanonicalReplacementBefore(
                          scope, insert_point, canonical_target)
                : SageBuilder::function_declaration_ownership::
                      sourceLexicalCanonicalReplacementIn(scope,
                                                          canonical_target)
      : insert_point != NULL
          ? SageBuilder::function_declaration_ownership::sourceLexicalBefore(
                scope, insert_point)
          : SageBuilder::function_declaration_ownership::sourceLexicalIn(scope);
  SgFunctionDeclaration *proto = generatePrototype(
      def_decl, ownership, scope, false,
      separate_outputs ? PrototypeDefinitionPolicy::forbidCrossOutputDefinition
                       : PrototypeDefinitionPolicy::preserveSameOutputFamily,
      PrototypeFamilyPolicy::canonicalReplacement);
  ROSE_ASSERT(proto);

  if (insert_point != NULL)
    Outliner::Preprocess::moveLeadingPreprocessorPrefixForPrototype(
        insert_point, proto);
  ROSE_ASSERT(insert_point == NULL ||
              find(scope->getDeclarationList().begin(),
                   scope->getDeclarationList().end(),
                   insert_point) != scope->getDeclarationList().end());

  ROSE_ASSERT(proto->get_parent() == scope);
  ROSE_ASSERT(proto->get_scope() == scope);

  Sg_File_Info *scope_file_info = scope->get_file_info();
  const int scope_physical_file_id =
      scope_file_info != NULL ? scope_file_info->get_physical_file_id() : -1;
  if (scope_physical_file_id < 0) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-prototype-ownership]: "
            "prototype=%p name=%s target global scope has no exact "
            "physical file id\n",
            static_cast<void *>(proto), proto->get_name().str());
    ROSE_ABORT();
  }
  if (proto->get_file_info() == NULL ||
      proto->get_file_info()->get_physical_file_id() !=
          scope_physical_file_id ||
      !proto->get_file_info()->isTransformation() ||
      !proto->get_file_info()->isOutputInCodeGeneration()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-prototype-ownership]: "
            "prototype=%p name=%s was not published by its typed lexical "
            "builder in physical file=%d\n",
            static_cast<void *>(proto), proto->get_name().str(),
            scope_physical_file_id);
    ROSE_ABORT();
  }

  if (!separate_outputs) {
    SgFunctionDeclaration *first_non_def =
        isSgFunctionDeclaration(proto->get_firstNondefiningDeclaration());
    SgFunctionSymbol *symbol =
        first_non_def != NULL
            ? isSgFunctionSymbol(first_non_def->get_symbol_from_symbol_table())
            : NULL;
    if (first_non_def != proto || symbol == NULL ||
        symbol->get_symbol_basis() != first_non_def ||
        def_decl->get_firstNondefiningDeclaration() != first_non_def ||
        proto->get_definingDeclaration() != def_decl) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[global-prototype-canonical]: "
              "prototype=%p canonical=%p symbol=%p basis=%p definition=%p "
              "definition-canonical=%p is not the exact in-file "
              "declaration family produced by the builder\n",
              static_cast<void *>(proto), static_cast<void *>(first_non_def),
              static_cast<void *>(symbol),
              static_cast<void *>(symbol != NULL ? symbol->get_symbol_basis()
                                                 : NULL),
              static_cast<void *>(def_decl),
              static_cast<void *>(def_decl->get_firstNondefiningDeclaration()));
      ROSE_ABORT();
    }
    if (requires_external_linkage || Outliner::useNewFile) {
      const auto hasExternalOrDefaultStorage =
          [](SgFunctionDeclaration *declaration) {
            const SgStorageModifier &storage =
                declaration->get_declarationModifier().get_storageModifier();
            return storage.isDefault() || storage.isExtern();
          };
      if (!hasExternalOrDefaultStorage(proto) ||
          !hasExternalOrDefaultStorage(def_decl) ||
          !hasExternalOrDefaultStorage(first_non_def)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-function-linkage]: "
                "friend-visible outlined family does not have external or "
                "default storage\n");
        ROSE_ABORT();
      }
    } else {
      SageInterface::setStatic(proto);
      SageInterface::setStatic(def_decl);
      SageInterface::setStatic(first_non_def);
    }
  }

  return proto;
}

//! Inserts a prototype into the original global scope of the outline target
static SgFunctionDeclaration *insertGlobalPrototype(
    SgFunctionDeclaration *def, FuncDeclList_t &friendFunctionPrototypeList,
    SgGlobal *scope,
    SgDeclarationStatement
        *default_target) // The enclosing function for the outlining target
{
  SgFunctionDeclaration *prototype = NULL;
  SgFunctionDeclaration *source_canonical_before_insertion = NULL;

  if (def == NULL || scope == NULL || default_target == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-prototype-input]: definition=%p "
            "scope=%p default-target=%p must identify one exact source "
            "prototype transaction\n",
            static_cast<void *>(def), static_cast<void *>(scope),
            static_cast<void *>(default_target));
    ROSE_ABORT();
  }

  source_canonical_before_insertion = findExactCanonicalDeclarationInScope(
      def, friendFunctionPrototypeList, scope);
  SgGlobal *definition_global = SageInterface::getGlobalScope(def);
  ASSERT_not_null(definition_global);
  const bool separate_outputs = definition_global != scope;
  const bool fresh_cross_output_source_family =
      separate_outputs && friendFunctionPrototypeList.empty();
  if (source_canonical_before_insertion == NULL &&
      !fresh_cross_output_source_family) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[source-prototype-canonical]: "
            "definition=%p definition-global=%p source-global=%p friends=%zu "
            "has neither an exact source canonical declaration nor one fresh "
            "cross-output source-family construction\n",
            static_cast<void *>(def), static_cast<void *>(definition_global),
            static_cast<void *>(scope), friendFunctionPrototypeList.size());
    ROSE_ABORT();
  }

  SgDeclarationStatement *insert_point =
      findClosestGlobalInsertPoint(default_target);
  if (insert_point == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[global-insertion-boundary]: "
            "default-target=%p has no exact global lexical boundary\n",
            static_cast<void *>(default_target));
    ROSE_ABORT();
  }
  for (SgFunctionDeclaration *friend_declaration :
       friendFunctionPrototypeList) {
    SgDeclarationStatement *friend_insert_point =
        findClosestGlobalInsertPoint(friend_declaration);
    if (friend_insert_point == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[global-insertion-boundary]: "
              "friend=%p has no exact global lexical owner\n",
              static_cast<void *>(friend_declaration));
      ROSE_ABORT();
    }
    if (declarationAppearsNoLaterThan(scope, friend_insert_point,
                                      insert_point)) {
      insert_point = friend_insert_point;
    }
  }
  prototype = insertGlobalPrototypeAtBoundary(
      def, scope, insert_point, source_canonical_before_insertion,
      !friendFunctionPrototypeList.empty());

  // Complete the exact declaration-family transaction for every generated
  // friend that shared the replaced canonical identity.
  if (prototype != NULL) {
    SgFunctionDeclaration *canonical =
        isSgFunctionDeclaration(prototype->get_firstNondefiningDeclaration());
    SgFunctionSymbol *canonical_symbol =
        canonical != NULL
            ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
            : NULL;
    SgSymbolTable *canonical_symbol_table = scope->get_symbol_table();
    if (canonical != prototype || canonical_symbol == NULL ||
        canonical_symbol->get_symbol_basis() != canonical ||
        canonical->get_scope() != scope || canonical_symbol_table == NULL ||
        canonical_symbol->get_parent() != canonical_symbol_table ||
        !canonical_symbol_table->exists(canonical_symbol) ||
        (source_canonical_before_insertion != NULL &&
         source_canonical_before_insertion != canonical &&
         source_canonical_before_insertion->get_firstNondefiningDeclaration() !=
             canonical)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[global-prototype-canonical]: "
              "prototype=%p canonical=%p symbol=%p basis=%p prior=%p did not "
              "atomically replace the exact source declaration identity\n",
              static_cast<void *>(prototype), static_cast<void *>(canonical),
              static_cast<void *>(canonical_symbol),
              static_cast<void *>(canonical_symbol != NULL
                                      ? canonical_symbol->get_symbol_basis()
                                      : NULL),
              static_cast<void *>(source_canonical_before_insertion));
      ROSE_ABORT();
    }
    for (FuncDeclList_t::iterator i = friendFunctionPrototypeList.begin();
         i != friendFunctionPrototypeList.end(); ++i) {
      SgFunctionDeclaration *proto_i = *i;
      SgFunctionDeclaration *prior =
          proto_i != NULL ? isSgFunctionDeclaration(
                                proto_i->get_firstNondefiningDeclaration())
                          : NULL;
      if (proto_i == NULL || source_canonical_before_insertion == NULL ||
          (prior != source_canonical_before_insertion && prior != canonical)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-prototype-canonical]: "
                "friend=%p prior=%p expected-prior=%p canonical=%p is not a "
                "member of the exact source declaration family\n",
                static_cast<void *>(proto_i), static_cast<void *>(prior),
                static_cast<void *>(source_canonical_before_insertion),
                static_cast<void *>(canonical));
        ROSE_ABORT();
      }
      proto_i->set_firstNondefiningDeclaration(canonical);

      if (!separate_outputs)
        proto_i->set_definingDeclaration(def);

      SgClassDefinition *friend_owner =
          isSgClassDefinition(proto_i->get_parent());
      const size_t lexical_memberships =
          friend_owner != NULL
              ? static_cast<size_t>(
                    std::count(friend_owner->get_members().begin(),
                               friend_owner->get_members().end(), proto_i))
              : 0;
      if (proto_i->get_firstNondefiningDeclaration() != canonical ||
          proto_i->get_scope() != scope || friend_owner == NULL ||
          lexical_memberships != 1 ||
          !proto_i->get_declarationModifier().isFriend() ||
          proto_i->get_name() != canonical->get_name() ||
          proto_i->get_type() != canonical->get_type() ||
          proto_i->get_linkage() != canonical->get_linkage() ||
          canonical_symbol->get_symbol_basis() != canonical ||
          canonical_symbol->get_parent() != canonical_symbol_table ||
          !canonical_symbol_table->exists(canonical_symbol) ||
          (separate_outputs ? proto_i->get_definingDeclaration() != NULL
                            : proto_i->get_definingDeclaration() != def)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-prototype-canonical]: "
                "friend=%p owner=%p lexical-memberships=%zu semantic-scope=%p "
                "canonical=%p canonical-symbol=%p basis=%p definition=%p did "
                "not join the exact source declaration family\n",
                static_cast<void *>(proto_i), static_cast<void *>(friend_owner),
                lexical_memberships, static_cast<void *>(proto_i->get_scope()),
                static_cast<void *>(canonical),
                static_cast<void *>(canonical_symbol),
                static_cast<void *>(canonical_symbol->get_symbol_basis()),
                static_cast<void *>(proto_i->get_definingDeclaration()));
        ROSE_ABORT();
      }
      SgDeclarationStatement *friend_boundary =
          findClosestGlobalInsertPoint(proto_i);
      if (friend_boundary == NULL ||
          !declarationAppearsNoLaterThan(scope, canonical, friend_boundary)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-prototype-order]: "
                "canonical=%p linkage=%s was not published before friend=%p "
                "owner=%p boundary=%p\n",
                static_cast<void *>(canonical),
                canonical->get_linkage().c_str(), static_cast<void *>(proto_i),
                static_cast<void *>(friend_owner),
                static_cast<void *>(friend_boundary));
        ROSE_ABORT();
      }
    }

    if (prototype->get_parent() != scope ||
        prototype->get_firstNondefiningDeclaration() != prototype ||
        (!separate_outputs &&
         def->get_firstNondefiningDeclaration() != prototype)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[global-prototype-canonical]: "
              "prototype=%p parent=%p scope=%p definition=%p "
              "definition-canonical=%p lost its exact source family after "
              "friend publication\n",
              static_cast<void *>(prototype),
              static_cast<void *>(prototype->get_parent()),
              static_cast<void *>(scope), static_cast<void *>(def),
              static_cast<void *>(def->get_firstNondefiningDeclaration()));
      ROSE_ABORT();
    }

    SgDeclarationStatement *prototype_definition =
        prototype->get_definingDeclaration();
    if (separate_outputs) {
      if (prototype_definition != NULL) {
        std::cerr
            << "REX_OUTLINER_INVARIANT[global-prototype]: a declaration in "
               "the source file must not retain a defining-declaration edge "
               "to a definition moved to a separate output file\n";
        ROSE_ABORT();
      }
      SgFunctionDeclaration *source_first_nondef =
          isSgFunctionDeclaration(prototype->get_firstNondefiningDeclaration());
      if (source_first_nondef == NULL ||
          source_first_nondef->get_definingDeclaration() != NULL) {
        std::cerr << "REX_OUTLINER_INVARIANT[global-prototype]: the source "
                     "prototype chain retains a cross-file definition edge\n";
        ROSE_ABORT();
      }
    } else if (prototype_definition != def) {
      std::cerr << "REX_OUTLINER_INVARIANT[global-prototype]: an in-file "
                   "prototype must refer to its exact function definition\n";
      ROSE_ABORT();
    }
  }

  return prototype;
}

// DQ (1/17/2020): Adding support for outlining functions in a new file where
// the original file has a defining class declaration. This is an issue because
// the new file used to build the file where we place the outlined functions is
// a copy of the original file and so it will contain a second copy of the
// defining class declaration.  This second copy of the defining class
// declaration is not a violation of ODR if it has the same token sequence
// (which should include the friend declaration), however more critically, the
// outline function will not compile if it has a reference to a private or
// protected member variable or function and so the friend function must be also
// inserted into the second defining class declaration.

class FindMatchingDefiningClassDeclarationTraversal
    : public SgSimpleProcessing {
public:
  SgClassDeclaration *pattern;
  std::string pattern_class_name;
  SgClassDeclaration *matchingClassDeclaration;
  // target is actually the source pattern we want to find a match for it.
  FindMatchingDefiningClassDeclarationTraversal(SgClassDeclaration *target);

  void visit(SgNode *astNode);
};

FindMatchingDefiningClassDeclarationTraversal::
    FindMatchingDefiningClassDeclarationTraversal(SgClassDeclaration *target) {
  ROSE_ASSERT(target != NULL);
  pattern = target;
  pattern_class_name = pattern->get_mangled_name();
  matchingClassDeclaration = NULL;
}
//! Check if a node is a defining class declaration and its mangled name match a
//! given pattern class name.
void FindMatchingDefiningClassDeclarationTraversal::visit(SgNode *astNode) {
  SgClassDeclaration *target = isSgClassDeclaration(astNode);

  ROSE_ASSERT(pattern != NULL);

  // Looking only for defining declarations.
  if (target != NULL && target == target->get_definingDeclaration()) {
    string target_class_name = target->get_mangled_name();
    if (target_class_name == pattern_class_name) {
      if (enable_debug) {
        printf("Found a matching name! target_class_name = %s \n",
               target_class_name.c_str());
        target->get_file_info()->display();
      }

      matchingClassDeclaration = target;
    }
  }
}

SgClassDeclaration *
findMatchingDefiningClassDeclaration(SgClassDeclaration *target);

SgClassDeclaration *
findMatchingDefiningClassDeclaration(SgSourceFile *targetFile,
                                     SgClassDeclaration *target) {
  // DQ (1/16/2020): Adding support for when the matching class is in the "*.C"
  // source file, and thus appears in the generated "_lib.C" file as well. Since
  // we no long share class declarations, we need to insert the friend function
  // into all possible defining class declarations across all files.
  if (enable_debug) {
#ifdef __linux__
    cout << "Entering " << __PRETTY_FUNCTION__
         << " for the following pattern class declaration: " << endl;
#endif
    target->get_file_info()->display();
  }

  FindMatchingDefiningClassDeclarationTraversal t(target);

  t.traverseWithinFile(targetFile, preorder);

  return t.matchingClassDeclaration;
}

/*!
 *  \brief Given a 'friend' declaration, insert it into the given
 *  class definition.
 */
static SgFunctionDeclaration *
insertFriendDecl(const SgFunctionDeclaration *func, SgGlobal *sourceGlobal,
                 SgClassDefinition *cls_def, FuncDeclList_t &sourceFriends,
                 FuncDeclList_t &destinationFriends) {
  SgFunctionDeclaration *friend_proto = 0;

  if (enable_debug)
    printf("Entering insertFriendDecl(): func = %p \n", func);

  if (func && sourceGlobal && cls_def) {
    SgGlobal *definitionGlobal = SageInterface::getGlobalScope(func);
    SgGlobal *classGlobal = SageInterface::getGlobalScope(cls_def);
    if (definitionGlobal == NULL || classGlobal == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[friend-family-policy]: definition=%p "
              "definition-global=%p class=%p class-global=%p has no exact "
              "output family\n",
              static_cast<const void *>(func),
              static_cast<void *>(definitionGlobal),
              static_cast<void *>(cls_def), static_cast<void *>(classGlobal));
      ROSE_ABORT();
    }

    auto publishFriendFamily = [&](SgFunctionDeclaration *friendDeclaration) {
      SgGlobal *friendGlobal = SageInterface::getGlobalScope(friendDeclaration);
      FuncDeclList_t *family = NULL;
      if (friendGlobal == sourceGlobal) {
        family = &sourceFriends;
      } else if (friendGlobal == definitionGlobal) {
        family = &destinationFriends;
      } else {
        // More than one outlined region may already have copied the declaring
        // class into a different generated translation unit.  A friend added
        // to that earlier copy is a third, independent physical declaration
        // family: it grants access in that class definition, but it must not
        // acquire a defining edge into either the source file or the current
        // outlined file.
        SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
            friendDeclaration->get_firstNondefiningDeclaration());
        SgFunctionSymbol *symbol =
            canonical != NULL
                ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
                : NULL;
        SgSymbolTable *table =
            friendGlobal != NULL ? friendGlobal->get_symbol_table() : NULL;
        if (friendGlobal == NULL ||
            friendDeclaration->get_scope() != friendGlobal ||
            canonical != friendDeclaration ||
            friendDeclaration->get_definingDeclaration() != NULL ||
            symbol == NULL || symbol->get_symbol_basis() != canonical ||
            table == NULL || symbol->get_parent() != table ||
            !table->exists(symbol)) {
          fprintf(
              stderr,
              "REX_OUTLINER_INVARIANT[friend-independent-family]: "
              "friend=%p global=%p scope=%p canonical=%p defining=%p "
              "symbol=%p basis=%p table=%p does not identify one exact "
              "independent output family\n",
              static_cast<void *>(friendDeclaration),
              static_cast<void *>(friendGlobal),
              static_cast<void *>(friendDeclaration->get_scope()),
              static_cast<void *>(canonical),
              static_cast<void *>(friendDeclaration->get_definingDeclaration()),
              static_cast<void *>(symbol),
              static_cast<void *>(symbol != NULL ? symbol->get_symbol_basis()
                                                 : NULL),
              static_cast<void *>(table));
          ROSE_ABORT();
        }
        return;
      }
      if (family == NULL || std::find(family->begin(), family->end(),
                                      friendDeclaration) != family->end()) {
        SgSourceFile *friendFile =
            SageInterface::getEnclosingSourceFile(friendDeclaration);
        SgSourceFile *sourceFile =
            SageInterface::getEnclosingSourceFile(sourceGlobal);
        SgSourceFile *definitionFile =
            SageInterface::getEnclosingSourceFile(definitionGlobal);
        SgSourceFile *classFile =
            SageInterface::getEnclosingSourceFile(classGlobal);
        fprintf(
            stderr,
            "REX_OUTLINER_INVARIANT[friend-family-policy]: friend=%p "
            "global=%p file=%s source=%p file=%s destination=%p file=%s "
            "class-global=%p file=%s has no unique exact family\n",
            static_cast<void *>(friendDeclaration),
            static_cast<void *>(friendGlobal),
            friendFile != NULL ? friendFile->getFileName().c_str() : "<null>",
            static_cast<void *>(sourceGlobal),
            sourceFile != NULL ? sourceFile->getFileName().c_str() : "<null>",
            static_cast<void *>(definitionGlobal),
            definitionFile != NULL ? definitionFile->getFileName().c_str()
                                   : "<null>",
            static_cast<void *>(classGlobal),
            classFile != NULL ? classFile->getFileName().c_str() : "<null>");
        ROSE_ABORT();
      }
      family->push_back(friendDeclaration);
    };

    // A friend is semantically declared in the global scope belonging to its
    // exact lexical class.  A copied destination class must never publish its
    // symbol in the original translation unit.
    friend_proto = generateFriendPrototype(func, cls_def, classGlobal);
    ROSE_ASSERT(friend_proto != NULL);
    ROSE_ASSERT(friend_proto->get_parent() == cls_def);
    ROSE_ASSERT(friend_proto->get_scope() == classGlobal);
    ROSE_ASSERT(cls_def->get_members().front() == friend_proto);
    publishFriendFamily(friend_proto);

    if (enable_debug) {
      printf("In insertFriendDecl(): Built SgFunctionDeclaration: friend_proto "
             "= %p = %s name = %s \n",
             friend_proto, friend_proto->class_name().c_str(),
             friend_proto->get_name().str());
      {
        bool isExtern = friend_proto->get_declarationModifier()
                            .get_storageModifier()
                            .isExtern();
        bool linkageSpecified = (friend_proto->get_linkage().empty() == false);
        bool isFriend = friend_proto->get_declarationModifier().isFriend();

        printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s \n",
               isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
               isFriend ? "true" : "false");
      }
    }

    bool includingSelf = true;
    SgSourceFile *sourceFile =
        SageInterface::getEnclosingNode<SgSourceFile>(cls_def, includingSelf);
    ROSE_ASSERT(sourceFile != NULL);

    if (enable_debug) {
      printf(
          "In insertFriendDecl(): sourceFile->get_unparseHeaderFiles() = %s \n",
          sourceFile->get_unparseHeaderFiles() ? "true" : "false");
      printf(
          "In insertFriendDecl(): Outliner::useNewFile                 = %s \n",
          Outliner::useNewFile ? "true" : "false");
    }

    if (Outliner::useNewFile == true) {
      if (enable_debug) {
        printf("#################################################### \n");
        printf("Inserting friend_proto = %p into cls_def = %p (this should "
               "have been defered) \n",
               friend_proto, cls_def);
        printf("#################################################### \n");
      }
      // DQ (1/17/2020): Now we need to see the class definition is from the
      // input file (*.C) file and if so there will be another class definition
      // in the generated file for the outline functions (*_lib.C file). And we
      // need to add a copy of the friend function there as well.  This is
      // important for ODR generally, but more specifically becasue the outlined
      // function accessing the member variable has will be in the generated
      // file with the second class definition.

      string filename =
          cls_def->get_startOfConstruct()->get_physical_filename();
      bool includingSelf = false;
      SgClassDeclaration *targetClassDeclaration = cls_def->get_declaration();
      ROSE_ASSERT(targetClassDeclaration != NULL);

      SgSourceFile *sourceFileOfClassDeclaration =
          getEnclosingSourceFile(targetClassDeclaration, includingSelf);
      ROSE_ASSERT(sourceFileOfClassDeclaration != NULL);
      bool filename_matches_source_file =
          (filename == sourceFileOfClassDeclaration->getFileName());
      if (filename_matches_source_file == true) {
        if (enable_debug)
          printf("Look for other matching class definitions in the associated "
                 "file where the outlined functions are put \n");

        SgProject *project = getEnclosingNode<SgProject>(
            sourceFileOfClassDeclaration, includingSelf);

        // DQ (1/16/2020): Check for the possability of a second class if this
        // class was in the source file that was copied.
        SgFileList *fileListNode = project->get_fileList_ptr();
        SgFilePtrList &fileList = fileListNode->get_listOfFiles();
        if (enable_debug)
          printf("#################### fileList.size() = %zu \n",
                 fileList.size());

        for (size_t i = 0; i < fileList.size(); i++) {
          SgSourceFile *alternativeSourceFile = isSgSourceFile(fileList[i]);
          ROSE_ASSERT(alternativeSourceFile != NULL);
          if (enable_debug)
            printf("alternativeSourceFile = %p = %s \n", alternativeSourceFile,
                   alternativeSourceFile->getFileName().c_str());

          if (alternativeSourceFile != sourceFileOfClassDeclaration) {
            // Search for defining class declaration matching
            // sourceFileOfClassDeclaration.
            if (enable_debug)
              printf("Search for class name = %s in file = %s \n",
                     targetClassDeclaration->get_name().str(),
                     alternativeSourceFile->getFileName().c_str());

            // Search in file for additional defining declaration matching
            // targetClassDeclaration.
            SgClassDeclaration *matchingClassDeclaration =
                findMatchingDefiningClassDeclaration(alternativeSourceFile,
                                                     targetClassDeclaration);
            if (matchingClassDeclaration != NULL) {

              SgClassDefinition *matchingClassDefinition =
                  matchingClassDeclaration->get_definition();
              ROSE_ASSERT(matchingClassDefinition != NULL);

              // insert a copy of the friend function declaration into the
              // matchingClassDeclaration.
              size_t orig_count = matchingClassDefinition->get_members().size();
              if (enable_debug) {
                printf(" --- insert a copy of the friend function declaration "
                       "into the matchingClassDeclaration's definition: "
                       "members size=%lu\n",
                       matchingClassDefinition->get_members().size());
                matchingClassDefinition->get_file_info()->display();
              }

              // Initially we can test this by making a copy of the pointer, but
              // later it should be deep copy.
              // SgFunctionDeclaration* friendFunction = friend_proto;
              SgGlobal *alternativeGlobalScope =
                  alternativeSourceFile->get_globalScope();
              ROSE_ASSERT(alternativeGlobalScope != NULL);
              SgFunctionDeclaration *friendFunction = generateFriendPrototype(
                  func, matchingClassDefinition, alternativeGlobalScope);
              ROSE_ASSERT(friendFunction->get_parent() ==
                          matchingClassDefinition);
              ROSE_ASSERT(friendFunction->get_scope() ==
                          alternativeGlobalScope);
              ROSE_ASSERT(matchingClassDefinition->get_members().front() ==
                          friendFunction);
              publishFriendFamily(friendFunction);

              // Also mark the class definition as transformed.
              matchingClassDefinition->markAsModified();
              ROSE_ASSERT(orig_count + 1 ==
                          matchingClassDefinition->get_members().size());
              Sg_File_Info *matching_class_info =
                  matchingClassDefinition->get_file_info();
              const int matching_class_physical_id =
                  matching_class_info != NULL
                      ? matching_class_info->get_physical_file_id()
                      : -1;
              if (matching_class_physical_id < 0) {
                fprintf(stderr,
                        "REX_OUTLINER_INVARIANT[friend-ownership]: class=%s "
                        "in generated source=%s has no exact physical "
                        "ownership\n",
                        matchingClassDeclaration->get_name().str(),
                        alternativeSourceFile->getFileName().c_str());
                ROSE_ABORT();
              }
              if (friendFunction->get_file_info() == NULL ||
                  friendFunction->get_file_info()->get_physical_file_id() !=
                      matching_class_physical_id ||
                  !friendFunction->get_file_info()->isTransformation() ||
                  !friendFunction->get_file_info()
                       ->isOutputInCodeGeneration()) {
                fprintf(stderr,
                        "REX_OUTLINER_INVARIANT[friend-ownership]: "
                        "friend=%s was not published by its typed lexical "
                        "builder in physical file=%d\n",
                        friendFunction->get_name().str(),
                        matching_class_physical_id);
                ROSE_ABORT();
              }
              friendFunction->markAsModified();
              if (enable_debug) {
                cout << "after insertion, checking the matching class "
                        "definition for the result,  members size="
                     << matchingClassDefinition->get_members().size() << endl;
                matchingClassDeclaration->unparseToString();
              }
            }
          }
        } // end for (fileLIst)

      } else {
        if (enable_debug)
          cout << "The class definition is in a header file (not the input "
                  "file), so we should not have to worry about any repeated "
                  "definition."
               << endl;
      }
    }

    Sg_File_Info *class_info = cls_def->get_file_info();
    const int class_physical_id =
        class_info != NULL ? class_info->get_physical_file_id() : -1;
    if (class_physical_id < 0) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[friend-ownership]: target class for "
              "friend=%s has no exact physical ownership\n",
              friend_proto->get_name().str());
      ROSE_ABORT();
    }
    if (friend_proto->get_file_info() == NULL ||
        friend_proto->get_file_info()->get_physical_file_id() !=
            class_physical_id ||
        !friend_proto->get_file_info()->isTransformation() ||
        !friend_proto->get_file_info()->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[friend-ownership]: friend=%s was not "
              "published by its typed lexical builder in physical file=%d\n",
              friend_proto->get_name().str(), class_physical_id);
      ROSE_ABORT();
    }

    // DQ (6/4/2019): Need to mark this as a modification, so that it will be
    // detected as something to trigger the output of the header file when the
    // class declaration appears in a header file. Maybe the insert function
    // should do this?
    friend_proto->markAsModified();
    cls_def->markAsModified();
  }

  if (enable_debug) {
    ROSE_ASSERT(friend_proto != NULL);
    printf("Exiting insertFriendDecl(): func = %p friend_proto = %p "
           "friend_proto->isFriend = %s \n",
           func, friend_proto,
           friend_proto->get_declarationModifier().isFriend() ? "true"
                                                              : "false");
  }

  return friend_proto;
}

/*!
 *  \brief Returns 'true' if the given declaration statement is marked
 *  as 'private' or 'protected'.
 */
static bool isProtPriv(const SgDeclarationStatement *decl) {
  if (decl) {
    const SgAccessModifier &decl_access_mod =
        decl->get_declarationModifier().get_accessModifier();
    const SgDeclarationStatement *const chain_declarations[] = {
        decl, decl->get_firstNondefiningDeclaration(),
        decl->get_definingDeclaration()};
    for (const SgDeclarationStatement *chain_decl : chain_declarations) {
      if (chain_decl != NULL &&
          !(chain_decl->get_declarationModifier().get_accessModifier() ==
            decl_access_mod)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[declaration-chain-access]: "
                "declaration=%p/%s chain=%p/%s has incongruent access\n",
                static_cast<const void *>(decl), decl->class_name().c_str(),
                static_cast<const void *>(chain_decl),
                chain_decl->class_name().c_str());
        ROSE_ABORT();
      }
    }
    if (decl_access_mod.isPrivate() || decl_access_mod.isProtected())
      return true;
    if (decl_access_mod.isDefault()) {
      const SgScopeStatement *decl_scope = decl->get_scope();
      const SgClassDefinition *class_def = isSgClassDefinition(decl_scope);
      if (class_def != NULL && class_def->get_declaration()->get_class_type() ==
                                   SgClassDeclaration::e_class) {
        return true;
      }
    }
  }

  return false;
}

static bool classHasNonPublicConstructor(SgClassDefinition *cl_def) {
  if (cl_def == NULL)
    return false;
  for (SgDeclarationStatement *member : cl_def->get_members()) {
    SgMemberFunctionDeclaration *func_decl =
        isSgMemberFunctionDeclaration(member);
    if (func_decl == NULL)
      continue;
    if (!func_decl->get_specialFunctionModifier().isConstructor())
      continue;
    if (isProtPriv(func_decl))
      return true;
  }
  return false;
}

/*!
 *  \brief Returns 'true' if the given variable use is a 'protected'
 *  or 'private' class member.
 */
static SgClassDefinition *isProtPrivMember(SgVarRefExp *v) {
  if (v) {
    SgVariableSymbol *sym = v->get_symbol();
    if (sym) {
      SgInitializedName *name = sym->get_declaration();
      ROSE_ASSERT(name != NULL);
      SgClassDefinition *cl_def = isSgClassDefinition(name->get_scope());
      if (cl_def != NULL && isProtPriv(name->get_declaration())) {
        return cl_def;
      }
    }
  }

  return NULL; // default: is not
}

static bool
hasExactDeclaringClassSurface(SgDeclarationStatement *declaration,
                              SgClassDefinition *declaringClass,
                              std::set<SgDeclarationStatement *> &visited) {
  if (declaration == NULL || declaringClass == NULL ||
      declaration->get_scope() != declaringClass ||
      !visited.insert(declaration).second) {
    return false;
  }

  if (declaration->get_parent() == declaringClass) {
    return std::count(declaringClass->get_members().begin(),
                      declaringClass->get_members().end(), declaration) == 1;
  }

  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(declaration->get_parent())) {
    group->validate();
    return group->get_scope() == declaringClass &&
           group->get_parent() == declaringClass &&
           std::count(group->get_declarations().begin(),
                      group->get_declarations().end(), declaration) == 1 &&
           std::count(declaringClass->get_members().begin(),
                      declaringClass->get_members().end(), group) == 1;
  }

  if (SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(declaration->get_parent())) {
    auxiliary->validate_semantic_non_output_role();
    if (auxiliary->get_parent() != declaringClass ||
        declaringClass->get_auxiliary_declarations() != auxiliary ||
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(), declaration) != 1) {
      return false;
    }
    SgDeclarationStatement *definition =
        isSgDeclarationStatement(declaration->get_definingDeclaration());
    return definition != NULL && definition != declaration &&
           definition->get_firstNondefiningDeclaration() == declaration &&
           hasExactDeclaringClassSurface(definition, declaringClass, visited);
  }

  if (SgDeclarationScope *declarationScope =
          isSgDeclarationScope(declaration->get_parent())) {
    const std::vector<SgNode *> successors =
        declarationScope->get_traversalSuccessorContainer();
    if (std::count(successors.begin(), successors.end(), declaration) != 1) {
      return false;
    }
    SgNode *scopeOwner =
        SageBuilder::getDeclarationScopeOwner(declarationScope);
    if (scopeOwner == declaringClass) {
      return true;
    }
    SgDeclarationStatement *ownerDeclaration =
        isSgDeclarationStatement(scopeOwner);
    return ownerDeclaration != NULL && ownerDeclaration != declaration &&
           hasExactDeclaringClassSurface(ownerDeclaration, declaringClass,
                                         visited);
  }

  return false;
}

/*!
 *  \brief Returns 'true' if the given type was declared as a
 *  'protected' or 'private' class member.
 *  This function need to be recursive. It may involves a chain of base types
 * and some types in the middle are private class types. The deepest base type
 * may not be a private type.
 */
static SgClassDefinition *isProtPrivType(SgType *t) {
  if (t) {
    // check self
    if (t && isSgNamedType(t)) {
      SgNamedType *named = isSgNamedType(t);
      ROSE_ASSERT(named);
      SgDeclarationStatement *decl = named->get_declaration();
      if (isProtPriv(decl)) {
        // Access to a non-public named type is granted by the class that owns
        // the declaration, regardless of whether the type itself is a class,
        // enum, or typedef.  Using a nested class's own definition here grants
        // friendship in the wrong class; casting also loses private enum and
        // typedef declarations entirely.
        SgClassDefinition *declaring_class =
            decl != NULL ? isSgClassDefinition(decl->get_scope()) : NULL;
        std::set<SgDeclarationStatement *> visited;
        if (declaring_class == NULL ||
            !hasExactDeclaringClassSurface(decl, declaring_class, visited)) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[private-type-owner]: "
                  "declaration=%p/%s parent=%p/%s scope=%p/%s is non-public "
                  "but has no exact declaring-class owner\n",
                  static_cast<void *>(decl),
                  decl != NULL ? decl->class_name().c_str() : "<null>",
                  static_cast<void *>(decl != NULL ? decl->get_parent() : NULL),
                  decl != NULL && decl->get_parent() != NULL
                      ? decl->get_parent()->class_name().c_str()
                      : "<null>",
                  static_cast<void *>(decl != NULL ? decl->get_scope() : NULL),
                  decl != NULL && decl->get_scope() != NULL
                      ? decl->get_scope()->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
        return declaring_class;
      }
    }

    // recursively check base type if any
    if (SgModifierType *m_type = isSgModifierType(t))
      return isProtPrivType(m_type->get_base_type());
    else if (SgPointerType *p_type = isSgPointerType(t))
      return isProtPrivType(p_type->get_base_type());
    else if (SgArrayType *a_type = isSgArrayType(t))
      return isProtPrivType(a_type->get_base_type());
    else if (SgReferenceType *r_type = isSgReferenceType(t))
      return isProtPrivType(r_type->get_base_type());
    else if (SgTypedefType *t_type = isSgTypedefType(t))
      return isProtPrivType(t_type->get_base_type());
  }

  return NULL;
}

/*!
 *  \brief Returns 'true' if the given member function is 'protected'
 *  or 'private'.
 */
static SgClassDefinition *isProtPrivMember(SgMemberFunctionRefExp *f) {
  if (f) {
    SgMemberFunctionSymbol *sym = f->get_symbol();
    if (sym) {
      SgMemberFunctionDeclaration *f_decl = sym->get_declaration();
      ROSE_ASSERT(f_decl);
      SgClassDefinition *cl_def = sym->get_scope();
      if (cl_def && isProtPriv(f_decl))
        return cl_def;
    }
  }
  return NULL; // default: is not
}

/*!
 *  \brief Inserts all necessary friend declarations.
 *
 *  \returns A list, 'friends', of all generated friend declarations.
 *  func: the generated outlined function
 */
// static void insertFriendDecls (SgFunctionDeclaration* func, SgGlobal* scope,
// FuncDeclList_t& friends)
static
    // DQ (11/19/2020): DeferredTransformation support was moved to the
    // SageInterface namespace to support more general usage.
    // Outliner::DeferredTransformation
    SageInterface::DeferredTransformation
    insertFriendDecls(SgFunctionDeclaration *func, SgGlobal *scope,
                      FuncDeclList_t &friends) {
  if (enable_debug) {
    printf("************************************************************ \n");
    printf("TOP of insertFriendDecls(): func = %p = %s \n", func,
           func->class_name().c_str());
    bool isExtern =
        (func->get_declarationModifier().get_storageModifier().isExtern() ==
         true);
    bool linkageSpecified = (func->get_linkage().empty() == false);
    bool isFriend = (func->get_declarationModifier().isFriend() == true);
    bool isDefiningDeclaration = (func->get_definition() != NULL);
    printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s "
           "isDefiningDeclaration = %s \n",
           isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
           isFriend ? "true" : "false",
           isDefiningDeclaration ? "true" : "false");

    printf(" --- scope = %p = %s \n", scope, scope->class_name().c_str());
    printf(" --- friends list size = %" PRIuPTR " \n", friends.size());
    printf("************************************************************ \n");
  }

  SageInterface::DeferredTransformation deferedFriendTransformation;

  if (func && scope) {
    if (enable_debug)
      printf("In insertFriendDecls(): friends list size = %" PRIuPTR " \n",
             friends.size());

    // Collect a list of all classes that need a 'friend' decl.
    // The outlining target has accesses to those classes' private/protected
    // members
    typedef set<SgClassDefinition *> ClassDefSet_t;
    ClassDefSet_t classes;

    // better algorithm in one pass to find all types of nodes
    RoseAst ast(func);
    for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
      SgClassDefinition *cl_def = NULL;
      // variable declarations may use some private types.
      if (SgInitializedName *init_name = isSgInitializedName(*i))
        cl_def = isProtPrivType(init_name->get_type());
      else if (SgVarRefExp *v_ref = isSgVarRefExp(*i)) {
        cl_def = isProtPrivMember(v_ref);

        if (enable_debug) {
          printf("In insertFriendDecls(): after isProtPrivMember(): cl_def = "
                 "%p \n",
                 cl_def);
          SgVariableSymbol *variableSymbol = v_ref->get_symbol();
          ROSE_ASSERT(variableSymbol != NULL);
          SgInitializedName *initializedName =
              variableSymbol->get_declaration();
          ROSE_ASSERT(initializedName != NULL);
          printf("In insertFriendDecls(): v_ref = %p = %s initializedName name "
                 "= %s \n",
                 v_ref, v_ref->class_name().c_str(),
                 initializedName->get_name().str());
          printf("In insertFriendDecls(): cl_def = %p \n", cl_def);
        }
        if (cl_def == NULL) {
          if (enable_debug) {
            ROSE_ASSERT(v_ref->get_type() != NULL);
            printf("Calling isProtPrivType(): v_ref->get_type() = %p = %s \n",
                   v_ref->get_type(), v_ref->get_type()->class_name().c_str());
          }
          cl_def = isProtPrivType(v_ref->get_type());
        }
      } else if (SgEnumVal *v_ref = isSgEnumVal(*i)) {
        // EnumVal may have a private type: TODO: all expressions should be
        // check against private type
        if (enable_debug) {
          ROSE_ASSERT(v_ref->get_type() != NULL);
          printf("Calling isProtPrivType(): v_ref->get_type() = %p = %s \n",
                 v_ref->get_type(), v_ref->get_type()->class_name().c_str());
        }
        cl_def = isProtPrivType(v_ref->get_type());
      } else if (SgMemberFunctionRefExp *f_ref = isSgMemberFunctionRefExp(*i))
        cl_def = isProtPrivMember(f_ref);
      else if (SgConstructorInitializer *ctor =
                   isSgConstructorInitializer(*i)) {
        // C++ constructors are called through SgConstructorInitializer, not
        // SgMemberFunctionRefExp.
        SgMemberFunctionDeclaration *fuc_decl = ctor->get_declaration();
        SgClassDeclaration *cls_decl = ctor->get_class_decl();
        if (!cls_decl) {
          printf("Warning: calling classes.insert() friend decl: constructor "
                 "has no class declaration = %p = %s \n",
                 ctor, ctor->class_name().c_str());
          continue;
        } else {
          SgClassDeclaration *def_cls_decl =
              isSgClassDeclaration(cls_decl->get_definingDeclaration());
          if (!def_cls_decl) {
            printf("Calling classes.insert(): constructor's class declaration "
                   "has no defining declaration = %p = %s \n",
                   ctor, ctor->class_name().c_str());
            continue;
          }

          SgClassDefinition *class_def = def_cls_decl->get_definition();
          if (fuc_decl != NULL) {
            if (isProtPriv(fuc_decl))
              cl_def = class_def;
          } else if (classHasNonPublicConstructor(class_def)) {
            cl_def = class_def;
          }
        }
      }

      if (cl_def != NULL) {
        if (enable_debug)
          printf("Calling classes.insert(): variables: cl_def = %p = %s \n",
                 cl_def, cl_def->class_name().c_str());
        classes.insert(cl_def);
      }
    } // end for RoseAst iterator

    // DQ (8/13/2019): Set the target classes.
    if (enable_debug)
      printf("Set the targetClasses: (disabled): "
             "deferedFriendTransformation.targetClasses = classes \n");
    deferedFriendTransformation.targetClasses = classes;

    // Insert 'em
    for (ClassDefSet_t::iterator c = classes.begin(); c != classes.end(); ++c) {
      ROSE_ASSERT(*c);
      if (enable_debug) {
        SgClassDefinition *classDefinition = *c;
        SgClassDeclaration *classDeclaration =
            classDefinition->get_declaration();
        if (enable_debug)
          printf("Building friend function for classDeclaration = %p = %s name "
                 "= %s \n",
                 classDeclaration, classDeclaration->class_name().c_str(),
                 classDeclaration->get_name().str());
      }
      // scope: the global scope in which the symbols should be inserted
      // class definition: the scope in which we insert friend declarations
      SgFunctionDeclaration *friend_decl = insertFriendDecl(
          func, scope, *c, friends, deferedFriendTransformation.targetFriends);
      ROSE_ASSERT(friend_decl != NULL);
      if (enable_debug) {
        printf("+++++++++++++++++++ friend_decl = %p = %s \n", friend_decl,
               friend_decl->class_name().c_str());
        bool isExtern = (friend_decl->get_declarationModifier()
                             .get_storageModifier()
                             .isExtern() == true);
        bool linkageSpecified = (friend_decl->get_linkage().empty() == false);
        bool isFriend =
            (friend_decl->get_declarationModifier().isFriend() == true);
        bool isDefiningDeclaration = (friend_decl->get_definition() != NULL);
        printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s "
               "isDefiningDeclaration = %s \n",
               isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
               isFriend ? "true" : "false",
               isDefiningDeclaration ? "true" : "false");
      }
      const bool sourceFriend = std::find(friends.begin(), friends.end(),
                                          friend_decl) != friends.end();
      const bool destinationFriend =
          std::find(deferedFriendTransformation.targetFriends.begin(),
                    deferedFriendTransformation.targetFriends.end(),
                    friend_decl) !=
          deferedFriendTransformation.targetFriends.end();
      SgGlobal *definitionGlobal = SageInterface::getGlobalScope(func);
      SgFunctionDeclaration *sourceFamilyDefinition =
          definitionGlobal == scope ? const_cast<SgFunctionDeclaration *>(func)
                                    : NULL;
      if (sourceFriend == destinationFriend ||
          (sourceFriend && (friend_decl->get_scope() != scope ||
                            friend_decl->get_definingDeclaration() !=
                                sourceFamilyDefinition)) ||
          (destinationFriend &&
           (friend_decl->get_scope() != definitionGlobal ||
            friend_decl->get_definingDeclaration() != func))) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-family-policy]: friend=%p "
                "source=%d destination=%d semantic-scope=%p definition=%p "
                "did not publish one exact output family\n",
                static_cast<void *>(friend_decl), sourceFriend ? 1 : 0,
                destinationFriend ? 1 : 0,
                static_cast<void *>(friend_decl->get_scope()),
                static_cast<void *>(friend_decl->get_definingDeclaration()));
        ROSE_ABORT();
      }
      if (enable_debug)
        printf("friend_decl = %p friend_decl->get_definingDeclaration() = %p "
               "friends list size = %zu \n",
               friend_decl, friend_decl->get_definingDeclaration(),
               friends.size());
    }

    if (enable_debug)
      printf("friends list size = %zu \n", friends.size());

    if (deferedFriendTransformation.targetClasses.size() >= 2) {
      printf("NOTE: In insertFriendDecls(): "
             "deferedFriendTransformation.targetClasses.size() = %zu \n",
             deferedFriendTransformation.targetClasses.size());
    }
    // Existing application code requires up to four target classes.
    ROSE_ASSERT(deferedFriendTransformation.targetClasses.size() <= 4);
    for (SgFunctionDeclaration *destinationFriend :
         deferedFriendTransformation.targetFriends) {
      if (destinationFriend == NULL ||
          SageInterface::getGlobalScope(destinationFriend) !=
              SageInterface::getGlobalScope(func) ||
          destinationFriend->get_firstNondefiningDeclaration() !=
              func->get_firstNondefiningDeclaration() ||
          destinationFriend->get_definingDeclaration() != func) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[friend-family-policy]: destination "
                "friend=%p did not join definition=%p canonical=%p\n",
                static_cast<void *>(destinationFriend),
                static_cast<void *>(func),
                static_cast<void *>(func->get_firstNondefiningDeclaration()));
        ROSE_ABORT();
      }
    }
  } else {
    // DQ (1/17/2020): Adding debugging information.
    if (func == NULL) {
      printf("NOTE: In insertFriendDecls(): func == NULL \n");
    } else {
      printf("NOTE: In insertFriendDecls(): scope == NULL: func = %p = %s \n",
             func, func->class_name().c_str());
    }

    if (scope == NULL) {
      printf("NOTE: In insertFriendDecls(): scope == NULL \n");
    } else {
      printf("NOTE: In insertFriendDecls(): func == NULL: scope = %p = %s \n",
             scope, scope->class_name().c_str());
    }
  }

  if (enable_debug) {
    printf("******************************************************** \n");
    printf("Leaving insertFriendDecls(): friends list size = %" PRIuPTR " \n",
           friends.size());
    printf("******************************************************** \n");
  }

  return deferedFriendTransformation;
}

// =====================================================================
//! Insert func into scope (could be either original scope or the new scope from
//! a new file),
//  and insert necessary declarations into the global scope of
//  target's original enclosing function).

SageInterface::DeferredTransformation
Outliner::insert(SgFunctionDeclaration *func, SgScopeStatement *scope,
                 SgBasicBlock *target_outlined_code,
                 SgFunctionDeclaration *&source_call_declaration) {
  // Scope is the global scope of the outlined location (could be in a separate
  // file).
  ROSE_ASSERT(func != NULL && scope != NULL);
  ROSE_ASSERT(Outliner::isValidOutliningScope(scope));
  ROSE_ASSERT(target_outlined_code != NULL);
  source_call_declaration = NULL;

  ROSE_ASSERT(func->get_parameterList()->get_parent() != NULL);

  SgFunctionDeclaration *target_func = const_cast<SgFunctionDeclaration *>(
      SageInterface::getEnclosingFunctionDeclaration(target_outlined_code));
  ROSE_ASSERT(target_func != NULL);

  ROSE_ASSERT(target_func->get_parameterList()->get_parent() != NULL);

  // This is the global scope of the original file
  SgGlobal *src_global = SageInterface::getGlobalScope(target_func);
  ROSE_ASSERT(src_global != NULL);

  SgGlobal *definition_global = SageInterface::getGlobalScope(scope);
  ASSERT_not_null(definition_global);
  const bool separate_outputs = definition_global != src_global;
  ROSE_ASSERT(separate_outputs || scope == src_global ||
              (SageInterface::is_Fortran_language() &&
               Outliner::isFortranModuleDefinitionScope(scope)));

  // Make sure this is a defining function
  ROSE_ASSERT(func->get_definition() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration() == func);

  SgSourceFile *enclosingSourceFile = getEnclosingSourceFile(scope, false);
  ROSE_ASSERT(enclosingSourceFile != NULL);

  int enclosingSourceFileId =
      enclosingSourceFile->get_file_info()->get_physical_file_id();
  if (enclosingSourceFileId < 0) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[generated-function-ownership]: "
            "target source=%s has no exact physical file id\n",
            enclosingSourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  ROSE_ASSERT(func->get_definingDeclaration()->get_file_info() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration()->get_startOfConstruct() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration()->get_endOfConstruct() != NULL);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  // generateFunction() publishes the defining declaration in its final source
  // scope.  Insertion validates that exact construction-time ownership instead
  // of transferring it out of a semantic holding container.
  ROSE_ASSERT(func->get_scope() == scope);
  ROSE_ASSERT(func->get_parent() == scope);
  ROSE_ASSERT(scope->statementExistsInScope(func));

  // generateFunction() and every subsequent body mutation publish or relocate
  // their exact source surfaces at the point where ownership changes.  This
  // insertion boundary only validates that completed transaction; republishing
  // the whole function here would be a late repair that could overwrite stale
  // physical provenance from an earlier producer.
  if (func->get_parent() != scope || !scope->statementExistsInScope(func) ||
      func->get_scope() != scope || func->get_file_info() == NULL ||
      func->get_file_info()->get_physical_file_id() != enclosingSourceFileId ||
      !func->get_file_info()->isOutputInCodeGeneration()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[generated-function-ownership]: "
            "function=%p name=%s lost exact structural or physical source "
            "ownership after insertion\n",
            static_cast<void *>(func), func->get_name().str());
    ROSE_ABORT();
  }

  SageInterface::DeferredTransformation headerFileTransformation;

  FuncDeclList_t friendFunctionPrototypeList;

  if (SageInterface::is_Fortran_language() == false) {
    // Insert all necessary 'friend' declarations. This step will not build
    // symbols for the symbol table (although the build functions will they are
    // removed in the insertFriendDecls() function).

    headerFileTransformation =
        insertFriendDecls(func, src_global, friendFunctionPrototypeList);
  }
  SgFunctionDeclaration *sourceFileFunctionPrototype = NULL;

  // insert a pointer to function declaration if use_dlopen is true
  // insert it into the original global scope
  // No need to generate this declaration if we use the simple call convention.
  if (use_dlopen && !use_dlopen_simple) {
    SgStatement *insertion_anchor =
        SageInterface::prepareStatementInsertionAnchor(target_outlined_code);
    SgScopeStatement *insertion_scope = insertion_anchor->get_scope();
    if (insertion_scope == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[dlopen-pointer-insertion]: anchor=%p/%s "
              "has no exact post-normalization lexical scope\n",
              static_cast<void *>(insertion_anchor),
              insertion_anchor->class_name().c_str());
      ROSE_ABORT();
    }

    // void (*OUT_xxx__p) (void**); // this parameter type depends on the number
    // of variables. If zero variables, empty parameter.
    SgFunctionParameterTypeList *tlist = buildFunctionParameterTypeList();
    // Only if func's argument list is not emtpy
    SgFunctionParameterTypeList *func_para_type_list =
        func->get_type()->get_argument_list();
    if (func_para_type_list->get_arguments().size() > 0)
      tlist->append_argument(
          buildPointerType(buildPointerType(buildVoidType())));

    SgFunctionType *ftype =
        buildFunctionType(buildVoidType(), tlist); // func->get_type();
    string var_name = func->get_name().getString() + "p";
    SgVariableDeclaration *ptofunc = buildVariableDeclaration(
        var_name, buildPointerType(ftype), NULL, insertion_scope);
    SageInterface::insertStatementBefore(insertion_anchor, ptofunc);
  }
  if (SageInterface::is_Fortran_language() == false) // C/C++ only
  {
    // This is done in the original file (does not effect the separate file if
    // we outline the function there) Insert a single, global prototype (i.e., a
    // first non-defining declaration), which specifies the linkage property of
    // 'func'. insertGlobalPrototype (func, protos, src_global, target_func);

    sourceFileFunctionPrototype = insertGlobalPrototype(
        func, friendFunctionPrototypeList, src_global, target_func);

    SgFunctionDeclaration *source_first_nondef =
        sourceFileFunctionPrototype != NULL
            ? isSgFunctionDeclaration(sourceFileFunctionPrototype
                                          ->get_firstNondefiningDeclaration())
            : NULL;
    SgFunctionSymbol *sourceFileFunctionPrototypeSymbol =
        source_first_nondef != NULL
            ? isSgFunctionSymbol(
                  source_first_nondef->get_symbol_from_symbol_table())
            : NULL;
    SgFunctionDeclaration *expected_definition = separate_outputs ? NULL : func;
    if (sourceFileFunctionPrototype == NULL || source_first_nondef == NULL ||
        source_first_nondef->get_firstNondefiningDeclaration() !=
            source_first_nondef ||
        sourceFileFunctionPrototype->get_firstNondefiningDeclaration() !=
            source_first_nondef ||
        sourceFileFunctionPrototypeSymbol == NULL ||
        sourceFileFunctionPrototypeSymbol->get_declaration() !=
            source_first_nondef ||
        sourceFileFunctionPrototype->get_definingDeclaration() !=
            expected_definition ||
        source_first_nondef->get_definingDeclaration() != expected_definition) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-prototype-chain]: "
              "prototype=%p canonical=%p symbol=%p symbol-declaration=%p "
              "prototype-definition=%p canonical-definition=%p "
              "expected-definition=%p does not identify one exact source "
              "declaration family\n",
              static_cast<void *>(sourceFileFunctionPrototype),
              static_cast<void *>(source_first_nondef),
              static_cast<void *>(sourceFileFunctionPrototypeSymbol),
              static_cast<void *>(
                  sourceFileFunctionPrototypeSymbol != NULL
                      ? sourceFileFunctionPrototypeSymbol->get_declaration()
                      : NULL),
              static_cast<void *>(
                  sourceFileFunctionPrototype != NULL
                      ? sourceFileFunctionPrototype->get_definingDeclaration()
                      : NULL),
              static_cast<void *>(
                  source_first_nondef != NULL
                      ? source_first_nondef->get_definingDeclaration()
                      : NULL),
              static_cast<void *>(expected_definition));
      ROSE_ABORT();
    }
    const SgDeclarationStatementPtrList &source_declarations =
        src_global->get_declarations();
    if (source_first_nondef->get_parent() != src_global ||
        source_first_nondef->get_scope() != src_global ||
        std::count(source_declarations.begin(), source_declarations.end(),
                   source_first_nondef) != 1 ||
        sourceFileFunctionPrototypeSymbol->get_parent() !=
            src_global->get_symbol_table()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[source-call-declaration]: canonical=%p "
              "name=%s has no exact source-global lexical and symbol "
              "publication\n",
              static_cast<void *>(source_first_nondef),
              source_first_nondef->get_name().str());
      ROSE_ABORT();
    }
    source_call_declaration = source_first_nondef;
  }

  // This is the outlined function prototype that is put into the separate file
  // (when outlining is done to a separate file).
  SgFunctionDeclaration *outlinedFileFunctionPrototype = NULL;
  if (separate_outputs) {

    // Build a function prototype and insert it first (will be at the top of the
    // generated file).
    // For template outlined functions we must build a matching template
    // prototype, otherwise wiring defining/nondefining will assert due to a
    // cross-variant definingDeclaration edge.
    SgFunctionDeclaration *destination_canonical =
        findExactCanonicalDeclarationInScope(
            func, headerFileTransformation.targetFriends, scope);
    if (destination_canonical == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-prototype-canonical]: "
              "outlined definition=%p name=%s has no exact canonical "
              "declaration in destination scope=%p\n",
              static_cast<void *>(func), func->get_name().str(),
              static_cast<void *>(scope));
      ROSE_ABORT();
    }
    outlinedFileFunctionPrototype = generatePrototype(
        func,
        SageBuilder::function_declaration_ownership::
            sourceLexicalCanonicalReplacementAtTop(scope,
                                                   destination_canonical),
        scope, false, PrototypeDefinitionPolicy::preserveSameOutputFamily,
        PrototypeFamilyPolicy::canonicalReplacement);
    ROSE_ASSERT(outlinedFileFunctionPrototype != NULL);

    // Inherit defining function's inline property: avoid linking error when
    // linking multiple .lib files with the outlined functions
    if (func->get_functionModifier().isInline())
      outlinedFileFunctionPrototype->get_functionModifier().setInline();

    bool includingCompilerGenerated = false;
    SgStatement *firstStatement =
        SageInterface::getFirstStatement(scope, includingCompilerGenerated);
    ROSE_ASSERT(firstStatement != NULL);
    if (firstStatement != NULL) {
      // DQ (3/17/2021): When using the token-based unparsing we need to set
      // this so that the surrounding whitespace will be unparsed from the AST,
      // instead of the token stream.
      firstStatement->set_containsTransformationToSurroundingWhitespace(true);
    }
    ROSE_ASSERT(outlinedFileFunctionPrototype->get_parent() == scope);
    ROSE_ASSERT(scope->getDeclarationList().front() ==
                outlinedFileFunctionPrototype);
    // DQ (3/17/2021): When using the token-based unparsing we need to set this
    // so that the surrounding whitespace will be unparsed from the AST, instead
    // of the token stream.
    outlinedFileFunctionPrototype
        ->set_containsTransformationToSurroundingWhitespace(true);
    ROSE_ASSERT(
        outlinedFileFunctionPrototype->get_parameterList()->get_parent() !=
        NULL);

    ROSE_ASSERT(func->get_parameterList()->get_parent() != NULL);

    SgFunctionParameterList *functionParameterList =
        outlinedFileFunctionPrototype->get_parameterList();
    ROSE_ASSERT(functionParameterList->get_parent() != NULL);

    headerFileTransformation.targetFriends.push_back(
        outlinedFileFunctionPrototype);

    // The typed canonical-replacement builder publishes the destination
    // prototype, its declaration family, and its exact symbol atomically.
    SgFunctionSymbol *outlinedFileFunctionPrototypeSymbol = isSgFunctionSymbol(
        outlinedFileFunctionPrototype->get_symbol_from_symbol_table());
    if (outlinedFileFunctionPrototypeSymbol == NULL ||
        outlinedFileFunctionPrototypeSymbol->get_symbol_basis() !=
            outlinedFileFunctionPrototype ||
        outlinedFileFunctionPrototype->get_firstNondefiningDeclaration() !=
            outlinedFileFunctionPrototype ||
        outlinedFileFunctionPrototype->get_definingDeclaration() != func ||
        func->get_firstNondefiningDeclaration() !=
            outlinedFileFunctionPrototype ||
        destination_canonical->get_firstNondefiningDeclaration() !=
            outlinedFileFunctionPrototype ||
        outlinedFileFunctionPrototype->get_parent() != scope ||
        outlinedFileFunctionPrototype->get_scope() != scope) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-prototype-canonical]: "
              "prototype=%p symbol=%p basis=%p definition=%p "
              "definition-canonical=%p prior=%p prior-canonical=%p scope=%p "
              "parent=%p did not publish one exact destination declaration "
              "family\n",
              static_cast<void *>(outlinedFileFunctionPrototype),
              static_cast<void *>(outlinedFileFunctionPrototypeSymbol),
              static_cast<void *>(
                  outlinedFileFunctionPrototypeSymbol != NULL
                      ? outlinedFileFunctionPrototypeSymbol->get_symbol_basis()
                      : NULL),
              static_cast<void *>(
                  outlinedFileFunctionPrototype->get_definingDeclaration()),
              static_cast<void *>(func->get_firstNondefiningDeclaration()),
              static_cast<void *>(destination_canonical),
              static_cast<void *>(
                  destination_canonical->get_firstNondefiningDeclaration()),
              static_cast<void *>(scope),
              static_cast<void *>(outlinedFileFunctionPrototype->get_parent()));
      ROSE_ABORT();
    }

    Sg_File_Info *published_prototype_info =
        outlinedFileFunctionPrototype->get_file_info();
    if (published_prototype_info == NULL ||
        published_prototype_info->get_physical_file_id() !=
            enclosingSourceFileId ||
        !published_prototype_info->isTransformation() ||
        published_prototype_info->isCompilerGenerated() ||
        !published_prototype_info->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-prototype-ownership]: "
              "prototype=%p expected-physical=%d actual-file='%s' "
              "actual-physical=%d transformation=%d "
              "compiler-generated=%d output=%d\n",
              static_cast<void *>(outlinedFileFunctionPrototype),
              enclosingSourceFileId,
              published_prototype_info == NULL
                  ? "<null>"
                  : published_prototype_info->get_filenameString().c_str(),
              published_prototype_info == NULL
                  ? -1
                  : published_prototype_info->get_physical_file_id(),
              published_prototype_info != NULL &&
                  published_prototype_info->isTransformation(),
              published_prototype_info != NULL &&
                  published_prototype_info->isCompilerGenerated(),
              published_prototype_info != NULL &&
                  published_prototype_info->isOutputInCodeGeneration());
      ROSE_ABORT();
    }

    // Make sure that internal referneces are to the same file (else the symbol
    // table information will not be consistant).
    ROSE_ASSERT(func->get_firstNondefiningDeclaration() != NULL);
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func->get_scope()) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));
  } else {
    {
      // Since the outlined function has been kept in the same file we can have
      // a pointer to the defining declaration.
      if (SageInterface::is_Fortran_language() == false) {
        if (sourceFileFunctionPrototype == NULL ||
            sourceFileFunctionPrototype->get_definingDeclaration() != func ||
            func->get_firstNondefiningDeclaration() !=
                sourceFileFunctionPrototype
                    ->get_firstNondefiningDeclaration()) {
          fprintf(
              stderr,
              "REX_OUTLINER_INVARIANT[source-prototype-chain]: in-file "
              "prototype=%p definition=%p canonical=%p did not retain "
              "the exact builder-published declaration family\n",
              static_cast<void *>(sourceFileFunctionPrototype),
              static_cast<void *>(
                  sourceFileFunctionPrototype != NULL
                      ? sourceFileFunctionPrototype->get_definingDeclaration()
                      : NULL),
              static_cast<void *>(func->get_firstNondefiningDeclaration()));
          ROSE_ABORT();
        }
      }
    }
  }

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  ROSE_ASSERT(func->get_firstNondefiningDeclaration() != NULL);

  if (SageInterface::is_Fortran_language()) {
    source_call_declaration =
        isSgFunctionDeclaration(func->get_firstNondefiningDeclaration());
  }
  if (source_call_declaration == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[source-call-declaration]: outlined "
            "function=%p name=%s produced no exact source-visible call "
            "declaration\n",
            static_cast<void *>(func), func->get_name().str());
    ROSE_ABORT();
  }

  return headerFileTransformation;

} // end Outliner::insert()

// eof
