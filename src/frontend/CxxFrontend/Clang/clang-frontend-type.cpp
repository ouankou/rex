#include "clang-frontend-private.hpp"
#include "clang-nns-utils.hpp"

#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>
#include <cctype>

#include <clang/AST/APValue.h>

#include <clang/Basic/AttrKinds.h>
#include <clang/Basic/OperatorKinds.h>

#include <llvm/ADT/SmallString.h>

#include <functional>

namespace {
std::string buildOverloadedOperatorName(clang::OverloadedOperatorKind op) {
  const char *spelling = clang::getOperatorSpelling(op);
  ROSE_ASSERT(spelling != nullptr);

  std::string result = "operator";
  if (std::isalpha(static_cast<unsigned char>(spelling[0])) ||
      spelling[0] == '_') {
    result += ' ';
  }
  result += spelling;
  return result;
}

bool nestedNameSpecifierHasGlobal(const clang::NestedNameSpecifier *qualifier) {
  for (const clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    if (nns->getKind() == clang::NestedNameSpecifier::Global) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasTypeQualifier(
    const clang::NestedNameSpecifier *qualifier) {
  for (const clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    if (nns->getKind() == clang::NestedNameSpecifier::TypeSpec
#if LLVM_VERSION_MAJOR < 21
        || nns->getKind() == clang::NestedNameSpecifier::TypeSpecWithTemplate
#endif
    ) {
      return true;
    }
  }
  return false;
}

bool nestedNameSpecifierHasNamespaceQualifier(
    const clang::NestedNameSpecifier *qualifier) {
  for (const clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    switch (nns->getKind()) {
    case clang::NestedNameSpecifier::Namespace:
    case clang::NestedNameSpecifier::NamespaceAlias:
    case clang::NestedNameSpecifier::Global:
      return true;
    default:
      break;
    }
  }
  return false;
}

std::vector<const clang::NamespaceDecl *>
collectNamespaceContexts(clang::DeclContext *context) {
  std::vector<const clang::NamespaceDecl *> namespaces;
  for (clang::DeclContext *ctx = context; ctx != nullptr;
       ctx = ctx->getParent()) {
    if (const clang::NamespaceDecl *ns =
            llvm::dyn_cast<clang::NamespaceDecl>(ctx)) {
      if (!ns->isAnonymousNamespace()) {
        namespaces.push_back(ns);
      }
    }
  }
  std::reverse(namespaces.begin(), namespaces.end());
  return namespaces;
}

std::vector<std::string>
collectNamespaceNamesFromScope(SgScopeStatement *scope) {
  std::vector<std::string> names;
  for (SgNode *node = scope; node != nullptr; node = node->get_parent()) {
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(node)) {
      SgNamespaceDeclarationStatement *ns_decl =
          ns_def->get_namespaceDeclaration();
      if (ns_decl != nullptr && !ns_decl->get_isUnnamedNamespace()) {
        names.push_back(ns_decl->get_name().getString());
      }
    }
  }
  std::reverse(names.begin(), names.end());
  return names;
}

bool scopeIsWithinNamespaceChain(SgScopeStatement *scope,
                                 clang::DeclContext *context) {
  if (scope == nullptr || context == nullptr) {
    return false;
  }
  std::vector<const clang::NamespaceDecl *> decl_namespaces =
      collectNamespaceContexts(context);
  if (decl_namespaces.empty()) {
    return false;
  }
  std::vector<std::string> scope_names = collectNamespaceNamesFromScope(scope);
  if (scope_names.size() < decl_namespaces.size()) {
    return false;
  }
  for (size_t i = 0; i < decl_namespaces.size(); ++i) {
    const clang::NamespaceDecl *ns = decl_namespaces[i];
    if (ns == nullptr) {
      return false;
    }
    if (scope_names[i] != ns->getNameAsString()) {
      return false;
    }
  }
  return true;
}

clang::NestedNameSpecifier *
buildNamespaceQualifierForDeclContext(clang::DeclContext *context,
                                      clang::ASTContext &ast_context) {
  if (context == nullptr) {
    return nullptr;
  }
  std::vector<const clang::NamespaceDecl *> namespaces =
      collectNamespaceContexts(context);
  if (namespaces.empty()) {
    return nullptr;
  }

  clang::NestedNameSpecifier *qualifier = nullptr;
  for (const clang::NamespaceDecl *ns : namespaces) {
    if (ns == nullptr || ns->isAnonymousNamespace()) {
      continue;
    }
    qualifier = clang::NestedNameSpecifier::Create(ast_context, qualifier, ns);
  }
  return qualifier;
}

bool collectQualifierNamespaces(
    const clang::NestedNameSpecifier *qualifier,
    std::vector<const clang::NamespaceDecl *> *namespaces) {
  if (namespaces == nullptr) {
    return false;
  }
  namespaces->clear();
  for (const clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    switch (nns->getKind()) {
    case clang::NestedNameSpecifier::Namespace: {
      const clang::NamespaceDecl *ns = nns->getAsNamespace();
      if (ns == nullptr || ns->isAnonymousNamespace()) {
        return false;
      }
      namespaces->push_back(ns);
      break;
    }
    case clang::NestedNameSpecifier::NamespaceAlias:
      return false;
    case clang::NestedNameSpecifier::Super:
      return false;
    default:
      break;
    }
  }
  std::reverse(namespaces->begin(), namespaces->end());
  return true;
}

bool namespaceChainIsSuffix(
    const std::vector<const clang::NamespaceDecl *> &full_chain,
    const std::vector<const clang::NamespaceDecl *> &suffix_chain) {
  if (suffix_chain.size() > full_chain.size()) {
    return false;
  }
  size_t offset = full_chain.size() - suffix_chain.size();
  for (size_t i = 0; i < suffix_chain.size(); ++i) {
    if (full_chain[offset + i] != suffix_chain[i]) {
      return false;
    }
  }
  return true;
}

clang::NestedNameSpecifier *
cloneQualifierWithPrefix(clang::NestedNameSpecifier *prefix,
                         clang::NestedNameSpecifier *suffix,
                         clang::ASTContext &context) {
  if (suffix == nullptr) {
    return prefix;
  }
  std::vector<const clang::NestedNameSpecifier *> segments;
  for (const clang::NestedNameSpecifier *nns = suffix; nns != nullptr;
       nns = nns->getPrefix()) {
    segments.push_back(nns);
  }
  std::reverse(segments.begin(), segments.end());

  clang::NestedNameSpecifier *result = prefix;
  for (const clang::NestedNameSpecifier *segment : segments) {
    switch (segment->getKind()) {
    case clang::NestedNameSpecifier::Identifier: {
      const clang::IdentifierInfo *id = segment->getAsIdentifier();
      if (id == nullptr) {
        break;
      }
      result = clang::NestedNameSpecifier::Create(context, result, id);
      break;
    }
    case clang::NestedNameSpecifier::Namespace: {
      const clang::NamespaceDecl *ns = segment->getAsNamespace();
      if (ns == nullptr) {
        break;
      }
      result = clang::NestedNameSpecifier::Create(context, result, ns);
      break;
    }
    case clang::NestedNameSpecifier::NamespaceAlias: {
      const clang::NamespaceAliasDecl *ns = segment->getAsNamespaceAlias();
      if (ns == nullptr) {
        break;
      }
      result = clang::NestedNameSpecifier::Create(context, result, ns);
      break;
    }
    case clang::NestedNameSpecifier::TypeSpec:
#if LLVM_VERSION_MAJOR < 21
    case clang::NestedNameSpecifier::TypeSpecWithTemplate:
#endif
    {
      bool has_template = false;
#if LLVM_VERSION_MAJOR < 21
      has_template = segment->getKind() ==
                     clang::NestedNameSpecifier::TypeSpecWithTemplate;
#endif
      const clang::Type *type = segment->getAsType();
      if (type == nullptr) {
        break;
      }
      result = clang::NestedNameSpecifier::Create(context, result, has_template,
                                                  type);
      break;
    }
    case clang::NestedNameSpecifier::Global:
      result = clang::NestedNameSpecifier::GlobalSpecifier(context);
      break;
    case clang::NestedNameSpecifier::Super:
      return suffix;
    }
  }
  return result;
}

clang::NestedNameSpecifier *
prependNamespaceQualifiers(clang::NestedNameSpecifier *qualifier,
                           clang::DeclContext *decl_context,
                           clang::ASTContext &context) {
  if (qualifier == nullptr || decl_context == nullptr) {
    return qualifier;
  }
  if (nestedNameSpecifierHasGlobal(qualifier)) {
    return qualifier;
  }

  std::vector<const clang::NamespaceDecl *> decl_namespaces =
      collectNamespaceContexts(decl_context);
  if (decl_namespaces.empty()) {
    return qualifier;
  }

  std::vector<const clang::NamespaceDecl *> qualifier_namespaces;
  if (!collectQualifierNamespaces(qualifier, &qualifier_namespaces)) {
    return qualifier;
  }

  if (!qualifier_namespaces.empty() &&
      !namespaceChainIsSuffix(decl_namespaces, qualifier_namespaces)) {
    return qualifier;
  }

  size_t missing_count = decl_namespaces.size() - qualifier_namespaces.size();
  if (missing_count == 0) {
    return qualifier;
  }

  clang::NestedNameSpecifier *prefix = nullptr;
  for (size_t i = 0; i < missing_count; ++i) {
    const clang::NamespaceDecl *ns = decl_namespaces[i];
    if (ns == nullptr || ns->isAnonymousNamespace()) {
      continue;
    }
    prefix = clang::NestedNameSpecifier::Create(context, prefix, ns);
  }
  if (prefix == nullptr) {
    return qualifier;
  }
  return cloneQualifierWithPrefix(prefix, qualifier, context);
}

struct DependentTemplateSpecializationNameInfo {
  clang::NestedNameSpecifier *qualifier = nullptr;
  std::string base_name;
  bool has_template_keyword = true;
};

DependentTemplateSpecializationNameInfo getDependentTemplateSpecializationName(
    const clang::DependentTemplateSpecializationType *dts) {
  DependentTemplateSpecializationNameInfo info;
  ROSE_ASSERT(dts != nullptr);

#if LLVM_VERSION_MAJOR >= 21
  const clang::DependentTemplateStorage &name = dts->getDependentTemplateName();
  info.qualifier = name.getQualifier();
  info.has_template_keyword = name.hasTemplateKeyword();

  clang::IdentifierOrOverloadedOperator base = name.getName();
  if (const clang::IdentifierInfo *id = base.getIdentifier()) {
    info.base_name = id->getName().str();
  } else {
    info.base_name = buildOverloadedOperatorName(base.getOperator());
  }
#else
  info.qualifier = dts->getQualifier();
  const clang::IdentifierInfo *id = dts->getIdentifier();
  ROSE_ASSERT(id != nullptr);
  info.base_name = id->getName().str();
  info.has_template_keyword = true;
#endif

  ROSE_ASSERT(!info.base_name.empty());
  return info;
}

// Forward declaration to keep helper ordering simple.
std::string getTemplateNameBase(const clang::TemplateName &tname);

// Generate unique name for template declaration with full namespace
// qualification
std::string mangleTemplateName(const clang::TemplateName &tname) {
  // Get fully qualified name from the underlying TemplateDecl
  if (clang::TemplateDecl *template_decl = tname.getAsTemplateDecl()) {
    // Get qualified name from the declaration (includes namespace)
    std::string result = template_decl->getQualifiedNameAsString();
    return result;
  }

  auto qualifierToString =
      [](clang::NestedNameSpecifier *qualifier) -> std::string {
    if (qualifier == nullptr) {
      return "";
    }
    std::string result;
    llvm::raw_string_ostream stream(result);
    clang::LangOptions opts;
    clang::PrintingPolicy policy(opts);
    qualifier->print(stream, policy);
    stream.flush();
    return result;
  };

  auto appendQualifier = [](std::string qualifier,
                            const std::string &base) -> std::string {
    if (qualifier.empty()) {
      return base;
    }
    if (qualifier.size() < 2 ||
        qualifier.substr(qualifier.size() - 2) != "::") {
      qualifier += "::";
    }
    return qualifier + base;
  };

  if (const clang::QualifiedTemplateName *qtn =
          tname.getAsQualifiedTemplateName()) {
    std::string base = getTemplateNameBase(qtn->getUnderlyingTemplate());
    clang::NestedNameSpecifier *qualifier = qtn->getQualifier();
    if (qualifier != nullptr && !base.empty()) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (!qualifier_str.empty()) {
        return appendQualifier(qualifier_str, base);
      }
    }
  }

  if (const clang::DependentTemplateName *dtn =
          tname.getAsDependentTemplateName()) {
    std::string base = getTemplateNameBase(tname);
    clang::NestedNameSpecifier *qualifier = dtn->getQualifier();
    if (qualifier != nullptr && !base.empty()) {
      std::string qualifier_str = qualifierToString(qualifier);
      if (!qualifier_str.empty()) {
        return appendQualifier(qualifier_str, base);
      }
    }
  }

  // Fallback: just use the template name without qualification
  std::string result;
  llvm::raw_string_ostream stream(result);
  clang::LangOptions opts;
  clang::PrintingPolicy policy(opts);
  tname.print(stream, policy);
  stream.flush();
  return result;
}

std::string getTemplateNameBase(const clang::TemplateName &tname) {
  if (clang::TemplateDecl *template_decl = tname.getAsTemplateDecl()) {
    if (clang::TemplateTemplateParmDecl *parm =
            llvm::dyn_cast<clang::TemplateTemplateParmDecl>(template_decl)) {
      std::string name = parm->getNameAsString();
      if (!name.empty()) {
        return name;
      }
      return "__template_template_param_" + std::to_string(parm->getIndex());
    }

    std::string name = template_decl->getNameAsString();
    ROSE_ASSERT(!name.empty());
    return name;
  }

  if (const clang::QualifiedTemplateName *qtn =
          tname.getAsQualifiedTemplateName()) {
    return getTemplateNameBase(qtn->getUnderlyingTemplate());
  }

  if (const clang::DependentTemplateName *dtn =
          tname.getAsDependentTemplateName()) {
#if LLVM_VERSION_MAJOR >= 21
    clang::IdentifierOrOverloadedOperator name = dtn->getName();
    if (const clang::IdentifierInfo *id = name.getIdentifier()) {
      return id->getName().str();
    }
    return buildOverloadedOperatorName(name.getOperator());
#else
    if (dtn->isIdentifier()) {
      return dtn->getIdentifier()->getName().str();
    }
    return buildOverloadedOperatorName(dtn->getOperator());
#endif
  }

  if (const clang::SubstTemplateTemplateParmStorage *subst =
          tname.getAsSubstTemplateTemplateParm()) {
    return getTemplateNameBase(subst->getReplacement());
  }

  if (clang::UsingShadowDecl *using_shadow = tname.getAsUsingShadowDecl()) {
    return using_shadow->getNameAsString();
  }

  if (clang::AssumedTemplateStorage *assumed =
          tname.getAsAssumedTemplateName()) {
    clang::DeclarationName decl_name = assumed->getDeclName();
    if (decl_name.isIdentifier()) {
      return decl_name.getAsIdentifierInfo()->getName().str();
    }
    if (clang::OverloadedOperatorKind op = decl_name.getCXXOverloadedOperator();
        op != clang::OO_None) {
      std::string name = "operator";
      name += clang::getOperatorSpelling(op);
      return name;
    }
    return decl_name.getAsString();
  }

  if (clang::OverloadedTemplateStorage *overloaded =
          tname.getAsOverloadedTemplate()) {
    for (clang::NamedDecl *decl : overloaded->decls()) {
      if (decl != nullptr) {
        std::string name = decl->getNameAsString();
        if (!name.empty()) {
          return name;
        }
      }
    }
  }

  if (const clang::SubstTemplateTemplateParmPackStorage *pack =
          tname.getAsSubstTemplateTemplateParmPack()) {
    if (clang::TemplateTemplateParmDecl *parm = pack->getParameterPack()) {
      std::string name = parm->getNameAsString();
      if (!name.empty()) {
        return name;
      }
      return "__template_template_param_" + std::to_string(parm->getIndex());
    }
  }

  if (const clang::DeducedTemplateStorage *deduced =
          tname.getAsDeducedTemplateName()) {
    return getTemplateNameBase(deduced->getUnderlying());
  }

  ROSE_ASSERT(!"Unhandled clang::TemplateName kind");
  return "";
}

std::string trimWhitespace(std::string s) {
  size_t first = 0;
  while (first < s.size() &&
         std::isspace(static_cast<unsigned char>(s[first]))) {
    ++first;
  }
  s.erase(0, first);

  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

std::vector<std::string>
splitQualifiedNameOutsideTemplates(const std::string &name) {
  std::vector<std::string> components;
  size_t start = 0;
  int depth = 0;
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (c == '<') {
      ++depth;
    } else if (c == '>') {
      if (depth > 0) {
        --depth;
      }
    } else if (c == ':' && depth == 0 && i + 1 < name.size() &&
               name[i + 1] == ':') {
      components.push_back(name.substr(start, i - start));
      ++i;
      start = i + 1;
    }
  }
  components.push_back(name.substr(start));
  return components;
}

bool hasTemplateSyntaxComponent(const std::string &name) {
  return name.find('<') != std::string::npos &&
         name.find('>') != std::string::npos;
}

std::string stripTemplateArgs(const std::string &name) {
  std::string trimmed = trimWhitespace(name);
  size_t lt = trimmed.find('<');
  if (lt == std::string::npos) {
    return trimmed;
  }
  return trimWhitespace(trimmed.substr(0, lt));
}

std::string normalizeTemplateDeclCacheKey(const std::string &name) {
  std::vector<std::string> components =
      splitQualifiedNameOutsideTemplates(name);
  if (components.empty()) {
    return name;
  }
  std::string result;
  for (size_t i = 0; i < components.size(); ++i) {
    std::string clean = stripTemplateArgs(components[i]);
    if (clean.empty()) {
      continue;
    }
    if (!result.empty()) {
      result += "::";
    }
    result += clean;
  }
  return result.empty() ? name : result;
}

void appendTemplateInstantiationArg(std::string &result, bool &need_separator,
                                    const clang::TemplateArgument &arg) {
  if (arg.getKind() == clang::TemplateArgument::Pack) {
    for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
      appendTemplateInstantiationArg(result, need_separator, pack_arg);
    }
    return;
  }

  if (need_separator) {
    result += " , ";
  }
  need_separator = true;

  std::string arg_str;
  llvm::raw_string_ostream arg_stream(arg_str);
  arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
  arg_stream.flush();
  result += trimWhitespace(arg_str);
}

std::string
buildTemplateInstantiationName(const std::string &base_name,
                               llvm::ArrayRef<clang::TemplateArgument> args) {
  if (args.empty())
    return base_name;

  std::string result = base_name;
  result += "<";
  bool need_separator = false;
  for (const clang::TemplateArgument &arg : args) {
    appendTemplateInstantiationArg(result, need_separator, arg);
  }
  result += ">";
  return result;
}

} // namespace

// Generate unique name for template instantiation
// Note: Must not contain < > characters for ROSE mangling
std::string ClangToSageTranslator::getTemplateQualifiedName(
    SgTemplateClassDeclaration *template_decl) {
  std::string template_base_name = template_decl->get_name().getString();
  std::string template_qualified_name = template_base_name;

  SgScopeStatement *decl_scope = template_decl->get_scope();
  while (decl_scope && !isSgGlobal(decl_scope)) {
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(decl_scope)) {
      SgNamespaceDeclarationStatement *ns_decl =
          ns_def->get_namespaceDeclaration();
      template_qualified_name =
          ns_decl->get_name().getString() + "::" + template_qualified_name;
    } else if (SgClassDefinition *class_def = isSgClassDefinition(decl_scope)) {
      SgClassDeclaration *class_decl = class_def->get_declaration();
      template_qualified_name =
          class_decl->get_name().getString() + "::" + template_qualified_name;
    }
    decl_scope = decl_scope->get_scope();
  }
  return template_qualified_name;
}

std::string ClangToSageTranslator::mangleTemplateInstantiation(
    const std::string &template_name,
    const clang::TemplateSpecializationType *spec_type) {
  std::string safe_template_name = template_name;
  for (char &c : safe_template_name) {
    if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' || c == '*' ||
        c == '&') {
      c = '_';
    }
  }
  std::string result = safe_template_name + "_";
  auto args = spec_type->template_arguments();
  bool first = true;
  for (const clang::TemplateArgument &arg : args) {
    if (!first)
      result += "_";
    first = false;

    std::string arg_str;
    llvm::raw_string_ostream arg_stream(arg_str);
    arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
    arg_stream.flush();

    // Replace special characters that can't be in mangled names
    for (char &c : arg_str) {
      if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
          c == '*' || c == '&') {
        c = '_';
      }
    }
    result += arg_str;
  }
  return result;
}

std::string ClangToSageTranslator::mangleTemplateInstantiation(
    const std::string &template_name, const clang::TemplateArgumentList &args) {
  std::string safe_template_name = template_name;
  for (char &c : safe_template_name) {
    if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' || c == '*' ||
        c == '&') {
      c = '_';
    }
  }
  std::string result = safe_template_name + "_";
  bool first = true;
  for (unsigned i = 0; i < args.size(); ++i) {
    const clang::TemplateArgument &arg = args.get(i);
    if (!first)
      result += "_";
    first = false;

    std::string arg_str;
    llvm::raw_string_ostream arg_stream(arg_str);
    arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
    arg_stream.flush();

    for (char &c : arg_str) {
      if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
          c == '*' || c == '&') {
        c = '_';
      }
    }
    result += arg_str;
  }
  return result;
}

namespace {

void suppress_unparse_output(SgLocatedNode *n) {
  if (n == NULL) {
    return;
  }
  if (Sg_File_Info *fi = n->get_file_info()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_startOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_endOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
}

void diagnose_null_scope(SgDeclarationStatement *decl, const char *context) {
  if (decl == NULL || decl->get_scope() != NULL)
    return;
  MLOG_WARN_C(MLOG_FRONTEND,
              "Declaration %s (%p) created with NULL scope in %s\n",
              decl->class_name().c_str(), decl, context);
}

SgType *getTypeFromTraversedRecordDecl(ClangToSageTranslator *translator,
                                       clang::RecordDecl *record_decl) {
  if (translator == NULL || record_decl == NULL) {
    return NULL;
  }

  SgNode *tmp_decl = translator->TraverseOnDemand(record_decl);
  if (SgClassDeclaration *sg_decl = isSgClassDeclaration(tmp_decl)) {
    ROSE_ASSERT(sg_decl->get_firstNondefiningDeclaration() != NULL);
    return sg_decl->get_type();
  }

  return NULL;
}
} // anonymous namespace

SgScopeStatement *ClangToSageTranslator::getOpaqueTypeInsertionScope(
    SgScopeStatement *scope) const {
  while (scope != nullptr) {
    if (scope->containsOnlyDeclarations() || isSgBasicBlock(scope)) {
      return scope;
    }
    scope = SageInterface::getEnclosingScope(scope, false);
  }
  return nullptr;
}

SgScopeStatement *
ClangToSageTranslator::getSafeOpaqueTypeInsertionScope() const {
  SgScopeStatement *scope =
      getOpaqueTypeInsertionScope(SageBuilder::topScopeStack());
  if (scope == nullptr) {
    scope = getGlobalScope();
  }
  return scope;
}

SgType *ClangToSageTranslator::buildTypeFromQualifiedType(
    const clang::QualType &qual_type) {
  SgNode *tmp_type = Traverse(qual_type.getTypePtr());
  SgType *type = isSgType(tmp_type);

  ROSE_ASSERT(type != NULL);

  // Issue 126: A class-template specialization type (e.g. `A<>`) must never be
  // represented as the primary SgTemplateClassDeclaration type; that produces
  // invalid output such as `template A a;`. If the canonical Clang type is a
  // RecordType whose declaration is a ClassTemplateSpecializationDecl, ensure
  // we use the translated specialization/instantiation declaration's type.
  if (SgClassType *class_type = isSgClassType(type)) {
    if (SgClassDeclaration *class_decl =
            isSgClassDeclaration(class_type->get_declaration())) {
      if (isSgTemplateClassDeclaration(class_decl) != NULL &&
          isSgTemplateInstantiationDecl(class_decl) == NULL) {
        clang::QualType canonical = qual_type.getCanonicalType();
        if (!canonical.isNull()) {
          if (const clang::RecordType *record_type =
                  canonical->getAs<clang::RecordType>()) {
            clang::RecordDecl *record_decl = record_type->getDecl();
            if (llvm::isa<clang::ClassTemplateSpecializationDecl>(
                    record_decl) ||
                llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(
                    record_decl)) {
              if (SgType *inst_type =
                      getTypeFromTraversedRecordDecl(this, record_decl)) {
                type = inst_type;
              }
            }
          }
        }
      }
    }
  }

  if (qual_type.hasLocalQualifiers()) {
    SgModifierType *modified_type = new SgModifierType(type);
    SgTypeModifier &sg_modifer = modified_type->get_typeModifier();
    clang::Qualifiers qualifier = qual_type.getLocalQualifiers();

    if (qualifier.hasConst())
      sg_modifer.get_constVolatileModifier().setConst();
    if (qualifier.hasVolatile())
      sg_modifer.get_constVolatileModifier().setVolatile();
    if (qualifier.hasRestrict())
      sg_modifer.setRestrict();

    if (qualifier.hasAddressSpace()) {
      clang::LangAS addrspace = qualifier.getAddressSpace();
      switch (addrspace) {
      case clang::LangAS::opencl_global:
        sg_modifer.setOpenclGlobal();
        break;
      case clang::LangAS::opencl_local:
        sg_modifer.setOpenclLocal();
        break;
      case clang::LangAS::opencl_constant:
        sg_modifer.setOpenclConstant();
        break;
      default:
        sg_modifer.setAddressSpace();
        sg_modifer.set_address_space_value(
            static_cast<unsigned int>(addrspace));
      }
    }
    modified_type =
        SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

    return modified_type;
  } else {
    return type;
  }
}

SgType *
ClangToSageTranslator::buildTypeFromTypeLoc(const clang::TypeLoc &type_loc) {
  if (type_loc.isNull()) {
    return nullptr;
  }

  auto resolve_scope = [&]() -> SgScopeStatement * {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    return scope;
  };

  auto append_default_args_from_template_decl =
      [&](const std::string &base_name, SgScopeStatement *scope,
          SgTemplateArgumentPtrList &args) {
        if (scope == nullptr || base_name.empty()) {
          return;
        }
        SgTemplateClassDeclaration *tmpl_decl = nullptr;
        if (SgTemplateClassSymbol *tmpl_sym =
                scope->lookup_template_class_symbol(SgName(base_name), nullptr,
                                                    nullptr)) {
          tmpl_decl = isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
        }
        if (tmpl_decl == nullptr) {
          if (SgTemplateClassSymbol *tmpl_sym =
                  SageInterface::lookupTemplateClassSymbolInParentScopes(
                      SgName(base_name), nullptr, nullptr, scope)) {
            tmpl_decl =
                isSgTemplateClassDeclaration(tmpl_sym->get_declaration());
          }
        }
        if (tmpl_decl == nullptr) {
          return;
        }
        SgTemplateParameterPtrList &params =
            tmpl_decl->get_templateParameters();
        if (args.size() >= params.size()) {
          return;
        }
        for (size_t i = args.size(); i < params.size(); ++i) {
          SgTemplateParameter *param = params[i];
          if (param == nullptr) {
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::type_parameter) {
            if (SgType *def_type = param->get_defaultTypeParameter()) {
              args.push_back(new SgTemplateArgument(def_type, false));
              continue;
            }
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::nontype_parameter) {
            if (SgExpression *expr = param->get_expression()) {
              args.push_back(new SgTemplateArgument(expr, false));
              continue;
            }
            break;
          }
          if (param->get_parameterType() ==
              SgTemplateParameter::template_parameter) {
            if (SgDeclarationStatement *templ_decl =
                    param->get_templateDeclaration()) {
              args.push_back(new SgTemplateArgument(
                  SgTemplateArgument::template_template_argument,
                  /*isArrayBoundUnknownType=*/false, /*type=*/nullptr,
                  /*expression=*/nullptr, /*templateDeclaration=*/templ_decl,
                  /*explicitlySpecified=*/false));
              continue;
            }
            break;
          }
          break;
        }
      };

  auto resolve_template_decl =
      [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
    clang::TemplateName current = name;
    for (;;) {
      if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
        return decl;
      }
      if (const clang::QualifiedTemplateName *qtn =
              current.getAsQualifiedTemplateName()) {
        clang::TemplateName underlying = qtn->getUnderlyingTemplate();
        if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
          return decl;
        }
        current = underlying;
        continue;
      }
      if (const clang::SubstTemplateTemplateParmStorage *subst =
              current.getAsSubstTemplateTemplateParm()) {
        current = subst->getReplacement();
        continue;
      }
      if (clang::UsingShadowDecl *using_shadow =
              current.getAsUsingShadowDecl()) {
        return llvm::dyn_cast_or_null<clang::TemplateDecl>(
            using_shadow->getTargetDecl());
      }
      return nullptr;
    }
  };

  auto append_default_args_from_clang_template_decl =
      [&](clang::TemplateDecl *tmpl_decl, SgTemplateArgumentPtrList &args) {
        if (tmpl_decl == nullptr) {
          return;
        }
        clang::TemplateParameterList *params =
            tmpl_decl->getTemplateParameters();
        if (params == nullptr) {
          return;
        }
        if (args.size() >= params->size()) {
          return;
        }
        for (unsigned i = args.size(); i < params->size(); ++i) {
          clang::NamedDecl *param = params->getParam(i);
          if (auto *type_param =
                  llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(param)) {
            if (type_param->hasDefaultArgument()) {
              appendTemplateArguments(args, type_param->getDefaultArgument(),
                                      false);
              continue;
            }
            break;
          }
          if (auto *non_type_param =
                  llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(
                      param)) {
            if (non_type_param->hasDefaultArgument()) {
              appendTemplateArguments(
                  args, non_type_param->getDefaultArgument(), false);
              continue;
            }
            break;
          }
          if (auto *tmpl_param =
                  llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                      param)) {
            if (tmpl_param->hasDefaultArgument()) {
              appendTemplateArguments(args, tmpl_param->getDefaultArgument(),
                                      false);
              continue;
            }
            break;
          }
          break;
        }
      };

  const bool enable_default_template_args = false;

  auto build_nonreal_template_type =
      [&](const std::string &base_name, clang::NestedNameSpecifier *qualifier,
          bool has_template_keyword, SgTemplateArgumentPtrList &tpl_args,
          bool preserve_empty_template_argument_list,
          clang::TemplateDecl *template_decl) -> SgType * {
    if (base_name.empty()) {
      return nullptr;
    }
    SgScopeStatement *scope = resolve_scope();
    if (enable_default_template_args) {
      append_default_args_from_template_decl(base_name, scope, tpl_args);
    }
    ensureTemplateArgumentParents(tpl_args);

    const bool has_template_args =
        preserve_empty_template_argument_list || !tpl_args.empty();

    clang::NestedNameSpecifier *effective_qualifier = qualifier;
    if (effective_qualifier == nullptr && template_decl != nullptr &&
        p_compiler_instance != nullptr) {
      if (!scopeIsWithinNamespaceChain(scope,
                                       template_decl->getDeclContext())) {
        effective_qualifier = buildNamespaceQualifierForDeclContext(
            template_decl->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
    }

    SgNonrealType *nrtype = nullptr;
    if (effective_qualifier != nullptr) {
      nrtype = buildNonrealTypeFromNestedNameSpecifier(
          effective_qualifier, scope, SgName(base_name),
          has_template_args ? &tpl_args : nullptr);
    } else {
      nrtype = SageBuilder::buildNonrealType(
          SgName(base_name), scope, has_template_args ? &tpl_args : nullptr);
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
      }
    }
    return nrtype;
  };

  if (auto elaborated_loc = type_loc.getAs<clang::ElaboratedTypeLoc>()) {
    const clang::ElaboratedType *elaborated = elaborated_loc.getTypePtr();
    clang::NestedNameSpecifier *qualifier =
        elaborated_loc.getQualifierLoc().getNestedNameSpecifier();
    if (elaborated != nullptr && qualifier != nullptr &&
        !elaborated->isDependentType()) {
      const bool preserve_explicit_scope_qualifier =
          nestedNameSpecifierHasNamespaceQualifier(qualifier);
      if (!preserve_explicit_scope_qualifier) {
        if (SgType *resolved_type =
                buildTypeFromQualifiedType(elaborated->getNamedType())) {
          if (isSgTypeUnknown(resolved_type) == nullptr) {
            return resolved_type;
          }
        }
      }

      std::string terminal_name;
      SgTemplateArgumentPtrList terminal_args;
      bool has_terminal_args = false;
      bool has_explicit_empty_terminal_args = false;
      clang::DeclContext *terminal_decl_context = nullptr;

      if (auto named_tmpl_loc =
              elaborated_loc.getNamedTypeLoc()
                  .getAs<clang::TemplateSpecializationTypeLoc>()) {
        const clang::TemplateSpecializationType *named_tst =
            named_tmpl_loc.getTypePtr();
        if (named_tst != nullptr) {
          terminal_name = getTemplateNameBase(named_tst->getTemplateName());
          unsigned arg_count = named_tmpl_loc.getNumArgs();
          for (unsigned i = 0; i < arg_count; ++i) {
            appendTemplateArguments(terminal_args, named_tmpl_loc.getArgLoc(i),
                                    true);
          }
          has_terminal_args = !terminal_args.empty();
          has_explicit_empty_terminal_args =
              (arg_count == 0 && named_tmpl_loc.getLAngleLoc().isValid());
        }
      }

      if (terminal_name.empty()) {
        if (const clang::TypedefType *td =
                elaborated->getNamedType()->getAs<clang::TypedefType>()) {
          terminal_name = td->getDecl()->getNameAsString();
          terminal_decl_context = td->getDecl()->getDeclContext();
        } else if (const clang::TagType *tag =
                       elaborated->getNamedType()->getAs<clang::TagType>()) {
          terminal_name = tag->getDecl()->getNameAsString();
          terminal_decl_context = tag->getDecl()->getDeclContext();
        }
      }

      if (!terminal_name.empty()) {
        SgScopeStatement *scope = resolve_scope();
        clang::NestedNameSpecifier *effective_qualifier = qualifier;
        if (effective_qualifier != nullptr &&
            terminal_decl_context != nullptr &&
            p_compiler_instance != nullptr) {
          effective_qualifier = prependNamespaceQualifiers(
              effective_qualifier, terminal_decl_context,
              p_compiler_instance->getASTContext());
        }
        if (SgType *nr = buildNonrealTypeFromNestedNameSpecifier(
                effective_qualifier, scope, SgName(terminal_name),
                (has_terminal_args || has_explicit_empty_terminal_args)
                    ? &terminal_args
                    : nullptr)) {
          if (SgNonrealType *nrtype = isSgNonrealType(nr)) {
            if (SgNonrealDecl *nrdecl =
                    isSgNonrealDecl(nrtype->get_declaration())) {
              nrdecl->set_suppress_typename(true);
            }
          }
          return nr;
        }
      }
    }

    if (elaborated != nullptr && elaborated->isDependentType() &&
        qualifier != nullptr) {
      clang::TypeLoc named_loc = elaborated_loc.getNamedTypeLoc();
      if (auto tmpl_loc =
              named_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
        const clang::TemplateSpecializationType *tst = tmpl_loc.getTypePtr();
        if (tst != nullptr) {
          clang::TemplateName tname = tst->getTemplateName();
          std::string base_name = getTemplateNameBase(tname);
          if (!base_name.empty()) {
            SgTemplateArgumentPtrList tpl_args;
            unsigned arg_count = tmpl_loc.getNumArgs();
            for (unsigned i = 0; i < arg_count; ++i) {
              appendTemplateArguments(tpl_args, tmpl_loc.getArgLoc(i), true);
            }
            if (enable_default_template_args) {
              append_default_args_from_clang_template_decl(
                  resolve_template_decl(tname), tpl_args);
            }
            bool has_template_keyword = false;
            if (const clang::QualifiedTemplateName *qtn =
                    tname.getAsQualifiedTemplateName()) {
              has_template_keyword = qtn->hasTemplateKeyword();
            } else if (tname.getAsDependentTemplateName() != nullptr) {
              has_template_keyword = true;
            }
            if (SgType *nr = build_nonreal_template_type(
                    base_name, qualifier, has_template_keyword, tpl_args,
                    arg_count == 0 && tmpl_loc.getLAngleLoc().isValid(),
                    resolve_template_decl(tname))) {
              return nr;
            }
          }
        }
      }
      if (auto dep_loc = named_loc.getAs<clang::DependentNameTypeLoc>()) {
        const clang::DependentNameType *dnt = dep_loc.getTypePtr();
        if (dnt != nullptr) {
          const clang::IdentifierInfo *id = dnt->getIdentifier();
          if (id != nullptr) {
            SgScopeStatement *scope = resolve_scope();
            SgNonrealType *nrtype = buildNonrealTypeFromNestedNameSpecifier(
                qualifier, scope, SgName(id->getName().str()), nullptr);
            if (SgNonrealDecl *nrdecl = isSgNonrealDecl(
                    nrtype ? nrtype->get_declaration() : nullptr)) {
              nrdecl->set_suppress_typename(false);
            }
            if (nrtype != nullptr) {
              return nrtype;
            }
          }
        }
      }
    }
    return buildTypeFromTypeLoc(elaborated_loc.getNamedTypeLoc());
  }

  if (auto dep_name_loc = type_loc.getAs<clang::DependentNameTypeLoc>()) {
    const clang::DependentNameType *dnt = dep_name_loc.getTypePtr();
    if (dnt != nullptr) {
      const clang::IdentifierInfo *id = dnt->getIdentifier();
      if (id != nullptr) {
        SgScopeStatement *scope = resolve_scope();
        clang::NestedNameSpecifier *qualifier = dnt->getQualifier();
        if (qualifier == nullptr) {
          qualifier = dep_name_loc.getQualifierLoc().getNestedNameSpecifier();
        }
        SgNonrealType *nrtype = buildNonrealTypeFromNestedNameSpecifier(
            qualifier, scope, SgName(id->getName().str()), nullptr);
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          nrdecl->set_suppress_typename(false);
        }
        if (nrtype != nullptr) {
          return nrtype;
        }
      }
    }
  }

  if (auto dep_spec_loc =
          type_loc.getAs<clang::DependentTemplateSpecializationTypeLoc>()) {
    const clang::DependentTemplateSpecializationType *dts =
        dep_spec_loc.getTypePtr();
    if (dts != nullptr) {
      DependentTemplateSpecializationNameInfo name_info =
          getDependentTemplateSpecializationName(dts);
      if (!name_info.base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        unsigned arg_count = dep_spec_loc.getNumArgs();
        for (unsigned i = 0; i < arg_count; ++i) {
          appendTemplateArguments(tpl_args, dep_spec_loc.getArgLoc(i), true);
        }
        if (SgType *nr = build_nonreal_template_type(
                name_info.base_name, name_info.qualifier,
                name_info.has_template_keyword, tpl_args, false, nullptr)) {
          return nr;
        }
      }
    }
  }

  if (auto spec_loc = type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
    const clang::TemplateSpecializationType *tst = spec_loc.getTypePtr();
    if (tst != nullptr && tst->isDependentType()) {
      clang::TemplateName tname = tst->getTemplateName();
      std::string base_name = getTemplateNameBase(tname);
      if (!base_name.empty()) {
        SgTemplateArgumentPtrList tpl_args;
        unsigned arg_count = spec_loc.getNumArgs();
        for (unsigned i = 0; i < arg_count; ++i) {
          appendTemplateArguments(tpl_args, spec_loc.getArgLoc(i), true);
        }
        if (enable_default_template_args) {
          append_default_args_from_clang_template_decl(
              resolve_template_decl(tname), tpl_args);
        }

        clang::NestedNameSpecifier *qualifier = nullptr;
        bool has_template_keyword = false;
        if (const clang::QualifiedTemplateName *qtn =
                tname.getAsQualifiedTemplateName()) {
          qualifier = qtn->getQualifier();
          has_template_keyword = qtn->hasTemplateKeyword();
        } else if (const clang::DependentTemplateName *dtn =
                       tname.getAsDependentTemplateName()) {
          qualifier = dtn->getQualifier();
          has_template_keyword = true;
        }

        if (SgType *nr = build_nonreal_template_type(
                base_name, qualifier, has_template_keyword, tpl_args,
                arg_count == 0 && spec_loc.getLAngleLoc().isValid(),
                resolve_template_decl(tname))) {
          return nr;
        }
      }
    }
  }

  if (const clang::TemplateSpecializationType *tst =
          type_loc.getType()->getAs<clang::TemplateSpecializationType>()) {
    if (!tst->isDependentType()) {
      return buildTypeFromQualifiedType(type_loc.getType());
    }
    clang::TemplateName tname = tst->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
    if (!base_name.empty()) {
      SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
      clang::NestedNameSpecifier *qualifier = nullptr;
      bool has_template_keyword = false;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
        has_template_keyword = qtn->hasTemplateKeyword();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
        has_template_keyword = true;
      }
      if (SgType *nr = build_nonreal_template_type(
              base_name, qualifier, has_template_keyword, tpl_args, false,
              resolve_template_decl(tname))) {
        return nr;
      }
    }
  }

  return buildTypeFromQualifiedType(type_loc.getType());
}

SgNode *ClangToSageTranslator::Traverse(const clang::Type *type) {
  if (type == NULL)
    return NULL;

  std::map<const clang::Type *, SgNode *>::iterator it =
      p_type_translation_map.find(type);
#if DEBUG_TRAVERSE_TYPE
  std::cerr << "Traverse Type : " << type << " " << type->getTypeClassName()
            << std::endl;
#endif
  if (it != p_type_translation_map.end()) {
#if DEBUG_TRAVERSE_TYPE
    std::cerr << " already visited : node = " << it->second << std::endl;
#endif
    return it->second;
  }

  SgNode *result = NULL;
  bool ret_status = false;

  switch (type->getTypeClass()) {
  case clang::Type::Decayed:
    ret_status = VisitDecayedType((clang::DecayedType *)type, &result);
    break;
  case clang::Type::ConstantArray:
    ret_status =
        VisitConstantArrayType((clang::ConstantArrayType *)type, &result);
    break;
  case clang::Type::DependentSizedArray:
    ret_status = VisitDependentSizedArrayType(
        (clang::DependentSizedArrayType *)type, &result);
    break;
  case clang::Type::IncompleteArray:
    ret_status =
        VisitIncompleteArrayType((clang::IncompleteArrayType *)type, &result);
    break;
  case clang::Type::VariableArray:
    ret_status =
        VisitVariableArrayType((clang::VariableArrayType *)type, &result);
    break;
  case clang::Type::Atomic:
    ret_status = VisitAtomicType((clang::AtomicType *)type, &result);
    break;
  case clang::Type::Attributed:
    ret_status = VisitAttributedType((clang::AttributedType *)type, &result);
    break;
  case clang::Type::BlockPointer:
    ret_status =
        VisitBlockPointerType((clang::BlockPointerType *)type, &result);
    break;
  case clang::Type::Builtin:
    ret_status = VisitBuiltinType((clang::BuiltinType *)type, &result);
    break;
  case clang::Type::Complex:
    ret_status = VisitComplexType((clang::ComplexType *)type, &result);
    break;
  case clang::Type::Decltype:
    ret_status = VisitDecltypeType((clang::DecltypeType *)type, &result);
    break;
    // case clang::Type::DependentDecltype:
    //     ret_status = VisitDependentDecltypeType((clang::DependentDecltypeType
    //     *)type, &result); break;
  case clang::Type::Auto:
    ret_status = VisitAutoType((clang::AutoType *)type, &result);
    break;
  case clang::Type::DeducedTemplateSpecialization:
    ret_status = VisitDeducedTemplateSpecializationType(
        (clang::DeducedTemplateSpecializationType *)type, &result);
    break;
  case clang::Type::DependentSizedExtVector:
    ret_status = VisitDependentSizedExtVectorType(
        (clang::DependentSizedExtVectorType *)type, &result);
    break;
  case clang::Type::DependentVector:
    ret_status =
        VisitDependentVectorType((clang::DependentVectorType *)type, &result);
    break;
  case clang::Type::FunctionNoProto:
    ret_status =
        VisitFunctionNoProtoType((clang::FunctionNoProtoType *)type, &result);
    break;
  case clang::Type::FunctionProto:
    ret_status =
        VisitFunctionProtoType((clang::FunctionProtoType *)type, &result);
    break;
  case clang::Type::InjectedClassName:
    ret_status = VisitInjectedClassNameType(
        (clang::InjectedClassNameType *)type, &result);
    break;
    // case clang::Type::LocInfo:
    //     ret_status = VisitLocInfoType((clang::LocInfoType *)type, &result);
    //     break;
  case clang::Type::MacroQualified:
    ret_status =
        VisitMacroQualifiedType((clang::MacroQualifiedType *)type, &result);
    break;
  case clang::Type::MemberPointer:
    ret_status =
        VisitMemberPointerType((clang::MemberPointerType *)type, &result);
    break;
  case clang::Type::PackExpansion:
    ret_status =
        VisitPackExpansionType((clang::PackExpansionType *)type, &result);
    break;
  case clang::Type::Paren:
    ret_status = VisitParenType((clang::ParenType *)type, &result);
    break;
  case clang::Type::Pipe:
    ret_status = VisitPipeType((clang::PipeType *)type, &result);
    break;
  case clang::Type::Pointer:
    ret_status = VisitPointerType((clang::PointerType *)type, &result);
    break;
  case clang::Type::LValueReference:
    ret_status =
        VisitLValueReferenceType((clang::LValueReferenceType *)type, &result);
    break;
  case clang::Type::RValueReference:
    ret_status =
        VisitRValueReferenceType((clang::RValueReferenceType *)type, &result);
    break;
  case clang::Type::SubstTemplateTypeParmPack:
    ret_status = VisitSubstTemplateTypeParmPackType(
        (clang::SubstTemplateTypeParmPackType *)type, &result);
    break;
  case clang::Type::SubstTemplateTypeParm:
    ret_status = VisitSubstTemplateTypeParmType(
        (clang::SubstTemplateTypeParmType *)type, &result);
    break;
  case clang::Type::Enum:
    ret_status = VisitEnumType((clang::EnumType *)type, &result);
    break;
  case clang::Type::Record:
    ret_status = VisitRecordType((clang::RecordType *)type, &result);
    break;
  case clang::Type::TemplateSpecialization:
    ret_status = VisitTemplateSpecializationType(
        (clang::TemplateSpecializationType *)type, &result);
    break;
  case clang::Type::TemplateTypeParm:
    ret_status =
        VisitTemplateTypeParmType((clang::TemplateTypeParmType *)type, &result);
    break;
  case clang::Type::Typedef:
    ret_status = VisitTypedefType((clang::TypedefType *)type, &result);
    break;
  case clang::Type::TypeOfExpr:
    ret_status = VisitTypeOfExprType((clang::TypeOfExprType *)type, &result);
    break;
    //  case clang::Type::DependentTypeOfExpr:
    //      ret_status =
    //      VisitDependentTypeOfExprType((clang::DependentTypeOfExprType *)type,
    //      &result); break;
  case clang::Type::TypeOf:
    ret_status = VisitTypeOfType((clang::TypeOfType *)type, &result);
    break;
  case clang::Type::DependentName:
    ret_status =
        VisitDependentNameType((clang::DependentNameType *)type, &result);
    break;
  case clang::Type::DependentTemplateSpecialization:
    ret_status = VisitDependentTemplateSpecializationType(
        (clang::DependentTemplateSpecializationType *)type, &result);
    break;
  case clang::Type::Elaborated:
    ret_status = VisitElaboratedType((clang::ElaboratedType *)type, &result);
    break;
  case clang::Type::UnaryTransform:
    ret_status =
        VisitUnaryTransformType((clang::UnaryTransformType *)type, &result);
    break;
  case clang::Type::UnresolvedUsing:
    ret_status =
        VisitUnresolvedUsingType((clang::UnresolvedUsingType *)type, &result);
    break;
  case clang::Type::Vector:
    ret_status = VisitVectorType((clang::VectorType *)type, &result);
    break;
  case clang::Type::ExtVector:
    ret_status = VisitExtVectorType((clang::ExtVectorType *)type, &result);
    break;
  case clang::Type::Using:
    ret_status = VisitUsingType((clang::UsingType *)type, &result);
    break;

  default:
    std::cerr << "Warning: Unhandled clang::Type '" << type->getTypeClassName()
              << "'. Using opaque type." << std::endl;
    ret_status = true;
    break;
  }

  if (result == NULL) {
    result = SageBuilder::buildUnknownType();
  }

  p_type_translation_map.insert(
      std::pair<const clang::Type *, SgNode *>(type, result));

#if DEBUG_TRAVERSE_TYPE
  std::cerr << "Traverse(clang::Type : " << type << " ";
  std::cerr << " visit done : node = " << result << std::endl;
#endif
  return result;
}

/***************/
/* Visit Types */
/***************/

bool ClangToSageTranslator::VisitType(clang::Type *type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitType" << std::endl;
#endif

  if (*node == NULL) {
    std::cerr << "Runtime error: No Sage node associated with the type: "
              << type->getTypeClassName() << std::endl;
    return false;
  }
  /*
      std::cerr << "Dump type " << type->getTypeClassName() << "(" << type <<
     "): "; type->dump(); std::cerr << std::endl;
  */
  // TODO

  return true;
}

bool ClangToSageTranslator::VisitAdjustedType(
    clang::AdjustedType *adjusted_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAdjustedType" << std::endl;
#endif
  bool res = true;

  clang::QualType adjusted = adjusted_type->getAdjustedType();
  if (!adjusted.isNull()) {
    *node = buildTypeFromQualifiedType(adjusted);
  } else {
    *node = buildTypeFromQualifiedType(adjusted_type->getOriginalType());
  }

  return VisitType(adjusted_type, node) && res;
}

bool ClangToSageTranslator::VisitDecayedType(clang::DecayedType *decayed_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDecayedType" << std::endl;
#endif
  bool res = true;

  //    SgType * decayType =
  //    buildTypeFromQualifiedType(decayed_type->getDecayedType ());
  SgType *pointeeType =
      buildTypeFromQualifiedType(decayed_type->getPointeeType());

  //    *node = pointeeType;
  // Pei-Hung (04/08/2022) Building SgArrayyType to represent the DecayedType in
  // Clang, in order to match the type of ParmVarDecl  in FunctionProtoType
  // Might need to check the case when the pointeeType is a functionType
  if (decayed_type->getPointeeType()->getTypeClass() ==
          clang::Type::VariableArray ||
      decayed_type->getPointeeType()->getTypeClass() ==
          clang::Type::ConstantArray ||
      decayed_type->getPointeeType()->getTypeClass() ==
          clang::Type::DependentSizedArray ||
      decayed_type->getPointeeType()->getTypeClass() ==
          clang::Type::IncompleteArray)
    *node = SageBuilder::buildArrayType(pointeeType);
  else
    *node = pointeeType;

  return VisitAdjustedType(decayed_type, node) && res;
}

bool ClangToSageTranslator::VisitArrayType(clang::ArrayType *array_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitArrayType" << std::endl;
#endif
  bool res = true;

  // Array type handling is implemented in child visitor functions
  // (ConstantArrayType, VariableArrayType, DependentSizedArrayType,
  // IncompleteArrayType) which set *node before calling this base function
  // ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  // DQ (11/28/2020): Added assertion.
  ROSE_ASSERT(*node != NULL);

  return VisitType(array_type, node) && res;
}

bool ClangToSageTranslator::VisitConstantArrayType(
    clang::ConstantArrayType *constant_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitConstantArrayType" << std::endl;
#endif

  SgType *type =
      buildTypeFromQualifiedType(constant_array_type->getElementType());

  // TODO clang::ArrayType::ArraySizeModifier

  SgExpression *expr =
      SageBuilder::buildIntVal(constant_array_type->getSize().getSExtValue());

  *node = SageBuilder::buildArrayType(type, expr);

  return VisitArrayType(constant_array_type, node);
}

bool ClangToSageTranslator::VisitDependentSizedArrayType(
    clang::DependentSizedArrayType *dependent_sized_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentSizedArrayType"
            << std::endl;
#endif
  bool res = true;

  SgType *type =
      buildTypeFromQualifiedType(dependent_sized_array_type->getElementType());

  clang::Expr *size_expr = dependent_sized_array_type->getSizeExpr();
  if (size_expr != NULL) {
    SgNode *tmp_expr = Traverse(size_expr);
    SgExpression *array_size = isSgExpression(tmp_expr);
    ROSE_ASSERT(array_size != NULL);
    *node = SageBuilder::buildArrayType(type, array_size);
  } else {
    // Size may be deduced from an initializer; represent as unknown-size array.
    *node = SageBuilder::buildArrayType(type);
  }

  return VisitArrayType(dependent_sized_array_type, node) && res;
}

bool ClangToSageTranslator::VisitIncompleteArrayType(
    clang::IncompleteArrayType *incomplete_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitIncompleteArrayType" << std::endl;
#endif

  SgType *type =
      buildTypeFromQualifiedType(incomplete_array_type->getElementType());

  // In LLVM 20, ArraySizeModifier moved from ArrayType:: to clang:: namespace

  clang::ArraySizeModifier sizeModifier =
      incomplete_array_type->getSizeModifier();

  if (sizeModifier == clang::ArraySizeModifier::Star) {
    SgExprListExp *exprListExp =
        SageBuilder::buildExprListExp(SageBuilder::buildNullExpression());
    *node = SageBuilder::buildArrayType(type, exprListExp);
  } else if (sizeModifier == clang::ArraySizeModifier::Static) {
    // TODO check how to handle Static
    *node = SageBuilder::buildArrayType(type);
  } else // clang::ArraySizeModifier::Normal
  {
    *node = SageBuilder::buildArrayType(type);
  }

  // DQ (11/28/2020): Added assertion.
  // ROSE_ASSERT(*node != NULL);

  return VisitArrayType(incomplete_array_type, node);
}

bool ClangToSageTranslator::VisitVariableArrayType(
    clang::VariableArrayType *variable_array_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitVariableArrayType" << std::endl;
#endif
  bool res = true;

  SgType *type =
      buildTypeFromQualifiedType(variable_array_type->getElementType());

  SgNode *tmp_expr = Traverse(variable_array_type->getSizeExpr());
  SgExpression *array_size = isSgExpression(tmp_expr);

  SgArrayType *arrayType = SageBuilder::buildArrayType(type, array_size);
  arrayType->set_is_variable_length_array(true);
  *node = arrayType;

  // DQ (11/28/2020): Added assertion.
  ROSE_ASSERT(*node != NULL);

  return VisitArrayType(variable_array_type, node) && res;
}

bool ClangToSageTranslator::VisitAtomicType(clang::AtomicType *atomic_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAtomicType" << std::endl;
#endif
  bool res = true;

  SgType *base_type = buildTypeFromQualifiedType(atomic_type->getValueType());
  if (base_type == nullptr) {
    base_type = SageBuilder::buildUnknownType();
  }
  *node = base_type;

  return VisitType(atomic_type, node) && res;
}

bool ClangToSageTranslator::VisitAttributedType(
    clang::AttributedType *attributed_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAttributedType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(attributed_type->getModifiedType());
  if (type == nullptr) {
    type = SageBuilder::buildUnknownType();
  }

  SgModifierType *modified_type = SageBuilder::buildModifierType(type);
  SgTypeModifier &sg_modifer = modified_type->get_typeModifier();

  switch (attributed_type->getAttrKind()) {
  case clang::attr::NoReturn:
    sg_modifer.setGnuAttributeNoReturn();
    break;
  case clang::attr::CDecl:
    sg_modifer.setGnuAttributeCdecl();
    break;
  case clang::attr::StdCall:
    sg_modifer.setGnuAttributeStdcall();
    break;
  default:
    break;
  }

  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(attributed_type, node);
}

bool ClangToSageTranslator::VisitBlockPointerType(
    clang::BlockPointerType *block_pointer_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitBlockPointerType" << std::endl;
#endif
  bool res = true;

  SgType *pointee_type =
      buildTypeFromQualifiedType(block_pointer_type->getPointeeType());
  if (pointee_type == nullptr) {
    pointee_type = SageBuilder::buildUnknownType();
  }
  *node = SageBuilder::buildPointerType(pointee_type);

  return VisitType(block_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinType(clang::BuiltinType *builtin_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitBuiltinType" << std::endl;
#endif

  switch (builtin_type->getKind()) {
  case clang::BuiltinType::Void:
    *node = SageBuilder::buildVoidType();
    break;
  case clang::BuiltinType::Bool:
    *node = SageBuilder::buildBoolType();
    break;
  case clang::BuiltinType::Short:
    *node = SageBuilder::buildShortType();
    break;
  case clang::BuiltinType::Int:
    *node = SageBuilder::buildIntType();
    break;
  case clang::BuiltinType::Long:
    *node = SageBuilder::buildLongType();
    break;
  case clang::BuiltinType::LongLong:
    *node = SageBuilder::buildLongLongType();
    break;
  case clang::BuiltinType::Float:
    *node = SageBuilder::buildFloatType();
    break;
  case clang::BuiltinType::Double:
    *node = SageBuilder::buildDoubleType();
    break;
  case clang::BuiltinType::LongDouble:
    *node = SageBuilder::buildLongDoubleType();
    break;

  case clang::BuiltinType::Char_S:
    *node = SageBuilder::buildCharType();
    break;

  case clang::BuiltinType::UInt:
    *node = SageBuilder::buildUnsignedIntType();
    break;
  case clang::BuiltinType::UChar:
    *node = SageBuilder::buildUnsignedCharType();
    break;
  case clang::BuiltinType::SChar:
    *node = SageBuilder::buildSignedCharType();
    break;
  case clang::BuiltinType::UShort:
    *node = SageBuilder::buildUnsignedShortType();
    break;
  case clang::BuiltinType::ULong:
    *node = SageBuilder::buildUnsignedLongType();
    break;
  case clang::BuiltinType::ULongLong:
    *node = SageBuilder::buildUnsignedLongLongType();
    break;
    /*
            case clang::BuiltinType::NullPtr:    *node = SageBuilder::build();
       break;
    */
  // TODO ROSE type ?
  case clang::BuiltinType::UInt128:
    *node = SageBuilder::buildUnsignedLongLongType();
    break;
  case clang::BuiltinType::Int128:
    *node = SageBuilder::buildLongLongType();
    break;

  // Wide character and Unicode types - use wchar for wide chars, int/long for
  // char16/32
  case clang::BuiltinType::Char_U:
    *node = SageBuilder::buildCharType();
    break;
  case clang::BuiltinType::WChar_U:
    *node = SageBuilder::buildWcharType();
    break;
  case clang::BuiltinType::WChar_S:
    *node = SageBuilder::buildWcharType();
    break;
  case clang::BuiltinType::Char16:
    *node = SageBuilder::buildUnsignedShortType();
    break; // char16_t is typically 16-bit
  case clang::BuiltinType::Char32:
    *node = SageBuilder::buildUnsignedIntType();
    break; // char32_t is typically 32-bit

  case clang::BuiltinType::Dependent: {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    SgNonrealType *nrtype = nullptr;
    if (scope != nullptr) {
      if (SgNonrealSymbol *sym =
              scope->lookup_nonreal_symbol(SgName("__dependent_type"))) {
        if (SgNonrealDecl *nrdecl = sym->get_declaration()) {
          nrtype = nrdecl->get_type();
        }
      }
    }
    if (nrtype == nullptr) {
      nrtype = SageBuilder::buildNonrealType(SgName("__dependent_type"), scope,
                                             nullptr);
    }
    if (nrtype != nullptr) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
        setCompilerGeneratedFileInfo(nrdecl);
        suppress_unparse_output(nrdecl);
      }
    }
    *node = nrtype != nullptr
                ? static_cast<SgNode *>(nrtype)
                : static_cast<SgNode *>(SageBuilder::buildUnknownType());
    break;
  }

  case clang::BuiltinType::ObjCId:
  case clang::BuiltinType::ObjCClass:
  case clang::BuiltinType::ObjCSel:
  case clang::BuiltinType::Overload:
  case clang::BuiltinType::BoundMember:
  case clang::BuiltinType::UnknownAny:
  default: {
    // Fallback for unknown builtin types (e.g., ARM SVE types, vendor
    // extensions)
    std::string type_name =
        builtin_type->getName(p_compiler_instance->getLangOpts()).str();
    // Using fallback type for unknown builtin (suppressed)

    // Prefer a scope that can accept a typedef; avoid scopes like SgIfStmt.
    SgScopeStatement *scope = getSafeOpaqueTypeInsertionScope();
    ROSE_ASSERT(scope != nullptr);
    *node = SageBuilder::buildOpaqueType(type_name, scope);
    break;
  }
  }

  return VisitType(builtin_type, node);
}

bool ClangToSageTranslator::VisitComplexType(clang::ComplexType *complex_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitComplexType" << std::endl;
#endif

  bool res = true;

  SgType *type = buildTypeFromQualifiedType(complex_type->getElementType());

  *node = SageBuilder::buildComplexType(type);

  return VisitType(complex_type, node) && res;
}

bool ClangToSageTranslator::VisitDecltypeType(
    clang::DecltypeType *decltype_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDecltypeType" << std::endl;
#endif
  bool res = true;

  clang::QualType underlying_type = decltype_type->getUnderlyingType();
  if (!underlying_type.isNull()) {
    *node = buildTypeFromQualifiedType(underlying_type);
  } else {
    *node = SageBuilder::buildUnknownType();
  }

  return VisitType(decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentDecltypeType(
    clang::DependentDecltypeType *dependent_decltype_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentDecltypeType" << std::endl;
#endif
  bool res = true;

  return VisitDecltypeType(dependent_decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDeducedType(clang::DeducedType *deduced_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDeducedType" << std::endl;
#endif
  bool res = true;

  clang::QualType deduced = deduced_type->getDeducedType();
  if (!deduced.isNull()) {
    *node = buildTypeFromQualifiedType(deduced);
  } else if (*node == nullptr) {
    *node = SageBuilder::buildAutoType();
  }

  return VisitType(deduced_type, node) && res;
}

bool ClangToSageTranslator::VisitAutoType(clang::AutoType *auto_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitAutoType" << std::endl;
#endif
  bool res = true;

  // Represent C++ auto explicitly so we do not synthesize an opaque typedef.
  *node = SageBuilder::buildAutoType();

  return VisitDeducedType(auto_type, node) && res;
}

bool ClangToSageTranslator::VisitDeducedTemplateSpecializationType(
    clang::DeducedTemplateSpecializationType
        *deduced_template_specialization_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDeducedTemplateSpecializationType"
            << std::endl;
#endif
  bool res = true;
  if (deduced_template_specialization_type == nullptr) {
    *node = nullptr;
    return false;
  }

  clang::QualType deduced =
      deduced_template_specialization_type->getDeducedType();
  if (!deduced.isNull()) {
    *node = buildTypeFromQualifiedType(deduced);
    return VisitDeducedType(deduced_template_specialization_type, node) && res;
  }

  clang::TemplateName tname =
      deduced_template_specialization_type->getTemplateName();
  std::string base_name = getTemplateNameBase(tname);
  if (base_name.empty()) {
    base_name = "__deduced_template";
  }

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  if (base_scope == nullptr) {
    base_scope = getGlobalScope();
  }

  clang::NestedNameSpecifier *qualifier = nullptr;
  if (const clang::QualifiedTemplateName *qtn =
          tname.getAsQualifiedTemplateName()) {
    qualifier = qtn->getQualifier();
  } else if (const clang::DependentTemplateName *dtn =
                 tname.getAsDependentTemplateName()) {
    qualifier = dtn->getQualifier();
  }

  if (qualifier != nullptr) {
    *node = buildNonrealTypeFromNestedNameSpecifier(qualifier, base_scope,
                                                    SgName(base_name), nullptr);
  } else {
    *node =
        SageBuilder::buildNonrealType(SgName(base_name), base_scope, nullptr);
  }

  return VisitDeducedType(deduced_template_specialization_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentAddressSpaceType(
    clang::DependentAddressSpaceType *dependent_address_space_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitDependentAddressSpaceType"
            << std::endl;
#endif
  bool res = true;

  SgType *pointee_type = buildTypeFromQualifiedType(
      dependent_address_space_type->getPointeeType());
  if (pointee_type == nullptr) {
    pointee_type = SageBuilder::buildUnknownType();
  }
  SgModifierType *modified_type = new SgModifierType(pointee_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setAddressSpace();
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_address_space_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentSizedExtVectorType(
    clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentSizedExtVectorType"
            << std::endl;
#endif
  bool res = true;

  SgType *elem_type = buildTypeFromQualifiedType(
      dependent_sized_ext_vector_type->getElementType());
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
  SgModifierType *modified_type = new SgModifierType(elem_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setVectorType();
  if (const clang::Expr *size_expr =
          dependent_sized_ext_vector_type->getSizeExpr()) {
    if (const clang::IntegerLiteral *int_lit =
            llvm::dyn_cast<clang::IntegerLiteral>(size_expr)) {
      modifier.set_vector_size(int_lit->getValue().getSExtValue());
    }
  }
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_sized_ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentVectorType(
    clang::DependentVectorType *dependent_vector_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentVectorType" << std::endl;
#endif
  bool res = true;

  SgType *elem_type =
      buildTypeFromQualifiedType(dependent_vector_type->getElementType());
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
  SgModifierType *modified_type = new SgModifierType(elem_type);
  SgTypeModifier &modifier = modified_type->get_typeModifier();
  modifier.setVectorType();
  if (const clang::Expr *size_expr = dependent_vector_type->getSizeExpr()) {
    if (const clang::IntegerLiteral *int_lit =
            llvm::dyn_cast<clang::IntegerLiteral>(size_expr)) {
      modifier.set_vector_size(int_lit->getValue().getSExtValue());
    }
  }
  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(dependent_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionType(
    clang::FunctionType *function_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionType" << std::endl;
#endif
  bool res = true;

  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();
  SgType *ret_type = buildTypeFromQualifiedType(function_type->getReturnType());
  if (ret_type == nullptr) {
    ret_type = SageBuilder::buildUnknownType();
  }
  *node = SageBuilder::buildFunctionType(ret_type, param_type_list);

  return VisitType(function_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionNoProtoType(
    clang::FunctionNoProtoType *function_no_proto_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionNoProtoType" << std::endl;
#endif

  bool res = true;

  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();

  SgType *ret_type =
      buildTypeFromQualifiedType(function_no_proto_type->getReturnType());

  *node = SageBuilder::buildFunctionType(ret_type, param_type_list);

  return VisitType(function_no_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionProtoType(
    clang::FunctionProtoType *function_proto_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitFunctionProtoType" << std::endl;
#endif

  bool res = true;
  SgFunctionParameterTypeList *param_type_list =
      new SgFunctionParameterTypeList();
  for (unsigned i = 0; i < function_proto_type->getNumParams(); i++) {
#if DEBUG_VISIT_TYPE
    std::cerr << "funcProtoType: " << i << " th param" << std::endl;
#endif
    SgType *param_type =
        buildTypeFromQualifiedType(function_proto_type->getParamType(i));

    param_type_list->append_argument(param_type);
  }

  if (function_proto_type->isVariadic()) {
    param_type_list->append_argument(SgTypeEllipse::createType());
  }

  SgType *ret_type =
      buildTypeFromQualifiedType(function_proto_type->getReturnType());

  SgFunctionType *func_type =
      SageBuilder::buildFunctionType(ret_type, param_type_list);
  if (function_proto_type->isVariadic())
    func_type->set_has_ellipses(1);

  *node = func_type;

  return VisitType(function_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitInjectedClassNameType(
    clang::InjectedClassNameType *injected_class_name_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::InjectedClassNameType" << std::endl;
#endif
  bool res = true;

  // InjectedClassName represents a class referring to itself within its own
  // definition (e.g., in member functions) Desugar to get the actual
  // instantiated type
  *node = Traverse(
      injected_class_name_type->getInjectedSpecializationType().getTypePtr());

  return VisitType(injected_class_name_type, node) && res;
}

// LocInfoType was removed in LLVM 20
/*
bool ClangToSageTranslator::VisitLocInfoType(clang::LocInfoType * loc_info_type,
SgNode ** node) { #if DEBUG_VISIT_TYPE std::cerr <<
"ClangToSageTranslator::LocInfoType" << std::endl; #endif bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(loc_info_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitMacroQualifiedType(
    clang::MacroQualifiedType *macro_qualified_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::MacroQualifiedType" << std::endl;
#endif
  bool res = true;

  clang::QualType desugared = macro_qualified_type->desugar();
  if (desugared.isNull()) {
    desugared = macro_qualified_type->getUnderlyingType();
  }
  *node = buildTypeFromQualifiedType(desugared);

  return VisitType(macro_qualified_type, node) && res;
}

bool ClangToSageTranslator::VisitMemberPointerType(
    clang::MemberPointerType *member_pointer_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::MemberPointerType" << std::endl;
  std::cerr << "isMemberFunctionPointer  "
            << member_pointer_type->isMemberFunctionPointer() << std::endl;
  std::cerr << "isMemberDataPointer  "
            << member_pointer_type->isMemberDataPointer() << std::endl;
  std::cerr << "isSugared  " << member_pointer_type->isSugared() << std::endl;
#endif
  bool res = true;

#if LLVM_VERSION_MAJOR >= 21
  clang::QualType classQualType;
  if (clang::NestedNameSpecifier *qualifier =
          member_pointer_type->getQualifier()) {
    if (const clang::Type *qual_type = qualifier->getAsType()) {
      classQualType = clang::QualType(qual_type, 0);
    } else if (clang::CXXRecordDecl *record = qualifier->getAsRecordDecl()) {
      classQualType = clang::QualType(record->getTypeForDecl(), 0);
    }
  }
#else
  clang::QualType classQualType(member_pointer_type->getClass(), 0);
#endif
  SgType *classType = buildTypeFromQualifiedType(classQualType);
  if (classType == NULL) {
    classType = SageBuilder::buildUnknownType();
  }
  const clang::Type *class_type_ptr = classQualType.getTypePtrOrNull();
  SgType *classTypeStripped =
      classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
  auto needs_nonreal_fallback = [](SgType *type) -> bool {
    if (type == nullptr) {
      return true;
    }
    if (isSgClassType(type) != nullptr || isSgNonrealType(type) != nullptr) {
      return false;
    }
    if (isSgTypedefType(type) != nullptr) {
      return false;
    }
    return true;
  };
  if (needs_nonreal_fallback(classTypeStripped)) {
    SgScopeStatement *fallback_scope = SageBuilder::topScopeStack();
    if (fallback_scope == nullptr) {
      fallback_scope = getGlobalScope();
    }
    if (class_type_ptr != nullptr) {
      if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
              class_type_ptr, fallback_scope, /*prefer_current_scope=*/true)) {
        classType = nrtype;
      }
    }
    classTypeStripped =
        classType != NULL ? classType->stripTypedefsAndModifiers() : NULL;
    if (needs_nonreal_fallback(classTypeStripped)) {
      std::string class_name = classQualType.getAsString();
      if (class_name.empty()) {
        class_name = "unknown_member_class";
      }
      classType = SageBuilder::buildNonrealType(SgName(class_name),
                                                fallback_scope, nullptr);
    }
  }

  clang::QualType pointee_type = member_pointer_type->getPointeeType();
  SgType *baseType = buildTypeFromQualifiedType(pointee_type);
  ROSE_ASSERT(baseType);
  if (member_pointer_type->isMemberFunctionPointer()) {
    unsigned int mfunc_specifier = 0;
    if (const clang::FunctionProtoType *proto =
            pointee_type->getAs<clang::FunctionProtoType>()) {
      clang::Qualifiers qualifiers = proto->getMethodQuals();
      if (qualifiers.hasConst()) {
        mfunc_specifier |= SgMemberFunctionType::e_const;
      }
      if (qualifiers.hasVolatile()) {
        mfunc_specifier |= SgMemberFunctionType::e_volatile;
      }
      if (qualifiers.hasRestrict()) {
        mfunc_specifier |= SgMemberFunctionType::e_restrict;
      }
      switch (proto->getRefQualifier()) {
      case clang::RQ_LValue:
        mfunc_specifier |= SgMemberFunctionType::e_ref_qualifier_lvalue;
        break;
      case clang::RQ_RValue:
        mfunc_specifier |= SgMemberFunctionType::e_ref_qualifier_rvalue;
        break;
      case clang::RQ_None:
        break;
      }
    }
    SgFunctionType *functionType = isSgFunctionType(baseType);
    if (functionType != NULL) {
      SgMemberFunctionType *memFuncType = SageBuilder::buildMemberFunctionType(
          functionType->get_return_type(), functionType->get_argument_list(),
          classType, mfunc_specifier);
      baseType = memFuncType;
      ROSE_ASSERT(baseType);
    }
  }

  SgPointerMemberType *pointerToMemberType =
      SageBuilder::buildPointerMemberType(baseType, classType);

  *node = pointerToMemberType;

  return VisitType(member_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionType(
    clang::PackExpansionType *pack_expansion_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::PackExpansionType" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: Pack expansion types (e.g., Args... in variadic templates)
  // represent template parameter packs that are expanded
  // Try to get the pattern type (the type being expanded)
  clang::QualType pattern = pack_expansion_type->getPattern();
  SgType *pattern_type = buildTypeFromQualifiedType(pattern);

  if (pattern_type != NULL) {
    if (SgTemplateType *template_type = isSgTemplateType(pattern_type)) {
      template_type->set_packed(true);
    }
    // Use the pattern type directly - the pack expansion is handled at a higher
    // level
    *node = pattern_type;
  } else {
    // Fallback: create a packed template type to preserve template context
    SgTemplateType *pack_type =
        SageBuilder::buildTemplateType(SgName("pack_expansion"));
    pack_type->set_packed(true);
    *node = pack_type;
  }

  return VisitType(pack_expansion_type, node) && res;
}

bool ClangToSageTranslator::VisitParenType(clang::ParenType *paren_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitParenType" << std::endl;
  std::cerr << "isSugared " << paren_type->isSugared() << std::endl;
#endif

  if (paren_type->isSugared())
    *node = buildTypeFromQualifiedType(paren_type->desugar());
  else
    *node = buildTypeFromQualifiedType(paren_type->getInnerType());

  return VisitType(paren_type, node);
}

bool ClangToSageTranslator::VisitPipeType(clang::PipeType *pipe_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::PipeType" << std::endl;
#endif
  bool res = true;

  SgType *elem_type = buildTypeFromQualifiedType(pipe_type->getElementType());
  if (elem_type == nullptr) {
    elem_type = SageBuilder::buildUnknownType();
  }
  *node = SageBuilder::buildPointerType(elem_type);

  return VisitType(pipe_type, node) && res;
}

bool ClangToSageTranslator::VisitPointerType(clang::PointerType *pointer_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitPointerType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(pointer_type->getPointeeType());

  *node = SageBuilder::buildPointerType(type);

  return VisitType(pointer_type, node);
}

bool ClangToSageTranslator::VisitReferenceType(
    clang::ReferenceType *reference_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::ReferenceType" << std::endl;
#endif

  SgType *pointee_type =
      buildTypeFromQualifiedType(reference_type->getPointeeType());
  if (pointee_type == nullptr) {
    return false;
  }

  if (clang::isa<clang::RValueReferenceType>(reference_type)) {
    *node = SageBuilder::buildRvalueReferenceType(pointee_type);
  } else {
    *node = SageBuilder::buildReferenceType(pointee_type);
  }

  return VisitType(reference_type, node);
}

bool ClangToSageTranslator::VisitLValueReferenceType(
    clang::LValueReferenceType *lvalue_reference_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::LValueReferenceType" << std::endl;
#endif
  return VisitReferenceType(lvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitRValueReferenceType(
    clang::RValueReferenceType *rvalue_reference_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::RValueReferenceType" << std::endl;
#endif
  return VisitReferenceType(rvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmPackType(
    clang::SubstTemplateTypeParmPackType *subst_template_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmPackType"
            << std::endl;
#endif
  bool res = true;

  std::string name;
  if (const clang::TemplateTypeParmDecl *decl =
          subst_template_type->getReplacedParameter()) {
    name = decl->getNameAsString();
  }
  if (name.empty()) {
    if (const clang::IdentifierInfo *id =
            subst_template_type->getIdentifier()) {
      name = id->getName().str();
    }
  }
  if (name.empty()) {
    name = "__template_pack";
  }

  SgTemplateType *pack_type = SageBuilder::buildTemplateType(SgName(name));
  if (pack_type != nullptr) {
    pack_type->set_packed(true);
    clang::TemplateArgument pack_arg = subst_template_type->getArgumentPack();
    if (pack_arg.getKind() == clang::TemplateArgument::Pack) {
      for (const clang::TemplateArgument &arg : pack_arg.pack_elements()) {
        appendTemplateArguments(pack_type->get_tpl_args(), arg, false);
      }
    } else {
      appendTemplateArguments(pack_type->get_tpl_args(), pack_arg, false);
    }
    for (SgTemplateArgument *arg : pack_type->get_tpl_args()) {
      if (arg != nullptr) {
        arg->set_parent(pack_type);
      }
    }
    *node = pack_type;
  } else {
    *node = SageBuilder::buildUnknownType();
  }

  return VisitType(subst_template_type, node) && res;
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmType(
    clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmType" << std::endl;
#endif

  // SubstTemplateTypeParmType represents a type where a template parameter has
  // been substituted with a concrete type. We simply traverse to the
  // replacement type.
  clang::QualType replacement_type =
      subst_template_type_parm_type->getReplacementType();
  *node = Traverse(replacement_type.getTypePtr());

  return VisitType(subst_template_type_parm_type, node);
}

bool ClangToSageTranslator::VisitTagType(clang::TagType *tag_type,
                                         SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTagType" << std::endl;
#endif
  bool res = true;

  if (clang::RecordType *record_type =
          llvm::dyn_cast<clang::RecordType>(tag_type)) {
    return VisitRecordType(record_type, node) && res;
  }
  if (clang::EnumType *enum_type = llvm::dyn_cast<clang::EnumType>(tag_type)) {
    return VisitEnumType(enum_type, node) && res;
  }

  if (clang::TagDecl *tag_decl = tag_type->getDecl()) {
    if (SgNode *decl_node = Traverse(tag_decl)) {
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl_node)) {
        *node = class_decl->get_type();
      } else if (SgEnumDeclaration *enum_decl =
                     isSgEnumDeclaration(decl_node)) {
        *node = enum_decl->get_type();
      }
    }
  }
  if (*node == nullptr) {
    *node = SageBuilder::buildUnknownType();
  }

  return VisitType(tag_type, node) && res;
}

bool ClangToSageTranslator::VisitEnumType(clang::EnumType *enum_type,
                                          SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitEnumType" << std::endl;
  std::cerr << "VisitEnumType isSugared: " << enum_type->isSugared()
            << std::endl;
#endif

  SgSymbol *sym = GetSymbolFromSymbolTable(enum_type->getDecl());

  SgEnumSymbol *enum_sym = isSgEnumSymbol(sym);

  if (enum_sym == NULL) {
    SgNode *tmp_decl = TraverseOnDemand(enum_type->getDecl());
    SgEnumDeclaration *sg_decl = isSgEnumDeclaration(tmp_decl);

    ROSE_ASSERT(sg_decl != NULL);

    *node = sg_decl->get_type();
  } else {
    *node = enum_sym->get_type();
  }

  if (isSgEnumType(*node) != NULL) {
    if (enum_sym == NULL) {
      p_enum_type_decl_first_see_in_type.insert(
          std::pair<SgEnumType *, bool>(isSgEnumType(*node), true));
    } else
      p_enum_type_decl_first_see_in_type.insert(
          std::pair<SgEnumType *, bool>(isSgEnumType(*node), false));
  }

  return VisitType(enum_type, node);
}

bool ClangToSageTranslator::VisitRecordType(clang::RecordType *record_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitRecordType" << std::endl;
#endif

  clang::RecordDecl *record_decl = record_type->getDecl();

  bool is_specialization =
      llvm::isa<clang::ClassTemplateSpecializationDecl>(record_decl) ||
      llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl);

  // Prefer the canonical declaration for symbol lookup and type association.
  //
  // Clang's RecordType can point at a later redeclaration/definition (including
  // out-of-line member definitions). Using that as the identity for record
  // types can force on-demand translation of the definition while we're still
  // inside a different lexical context, which destabilizes ROSE's
  // defining/nondefining declaration chains and name-qualification (Issue 69).
  //
  // For class-template (partial) specializations we must keep the
  // specialization decl as the identity (Issue 126).
  clang::RecordDecl *lookup_decl = record_decl;
  if (!is_specialization && record_decl != NULL) {
    clang::TagDecl *canonical = record_decl->getCanonicalDecl();
    if (clang::RecordDecl *canonical_record =
            llvm::dyn_cast_or_null<clang::RecordDecl>(canonical)) {
      lookup_decl = canonical_record;
    }
  }

  SgClassSymbol *class_sym = NULL;

  // Record types for class-template specializations (e.g. `A<>`) must resolve
  // to the specialization/instantiation declaration, not the primary template.
  // Otherwise the unparser can emit invalid type spellings such as `template A`
  // (Issue 126).
  if (is_specialization) {
    *node = getTypeFromTraversedRecordDecl(this, record_decl);
    if (*node == NULL) {
      if (SgDeclarationStatement *resolved =
              lookupSgDeclarationForClangDecl(record_decl,
                                              /*allow_on_demand=*/true)) {
        if (SgClassDeclaration *sg_decl = isSgClassDeclaration(resolved)) {
          ROSE_ASSERT(sg_decl->get_firstNondefiningDeclaration() != NULL);
          *node = sg_decl->get_type();
        }
      }
    }
  }

  if (*node == NULL) {
    SgSymbol *sym = GetSymbolFromSymbolTable(lookup_decl);
    class_sym = isSgClassSymbol(sym);

    if (class_sym != NULL && is_specialization) {
      SgClassDeclaration *sym_decl = class_sym->get_declaration();
      if (isSgTemplateInstantiationDecl(sym_decl) == NULL) {
        // Avoid binding specializations to primary-template symbols.
        class_sym = NULL;
      }
    }

    if (class_sym == NULL) {
      if (!is_specialization) {
        if (SgDeclarationStatement *cached =
                lookupSgDeclarationForClangDecl(lookup_decl,
                                                /*allow_on_demand=*/false)) {
          if (SgClassDeclaration *sg_decl = isSgClassDeclaration(cached)) {
            ROSE_ASSERT(sg_decl->get_firstNondefiningDeclaration() != NULL);
            *node = sg_decl->get_type();
          }
        }
      }
      if (!is_specialization && *node == NULL) {
        *node = getTypeFromTraversedRecordDecl(this, lookup_decl);
      }
      if (*node == NULL) {
        if (is_specialization) {
          if (clang::ClassTemplateSpecializationDecl *spec_decl =
                  llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                      record_decl)) {
            std::string base_name =
                spec_decl->getSpecializedTemplate() != nullptr
                    ? spec_decl->getSpecializedTemplate()->getNameAsString()
                    : spec_decl->getNameAsString();
            if (base_name.empty()) {
              base_name = "__template_specialization";
            }

            SgTemplateArgumentPtrList tpl_args;
            const clang::TemplateArgumentList &args =
                spec_decl->getTemplateArgs();
            for (unsigned i = 0; i < args.size(); ++i) {
              appendTemplateArguments(tpl_args, args.get(i), false);
            }
            if (clang::ClassTemplateDecl *primary =
                    spec_decl->getSpecializedTemplate()) {
              if (clang::TemplateParameterList *params =
                      primary->getTemplateParameters()) {
                if (tpl_args.size() < params->size()) {
                  for (unsigned i = tpl_args.size(); i < params->size(); ++i) {
                    clang::NamedDecl *param = params->getParam(i);
                    if (auto *type_param =
                            llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(
                                param)) {
                      if (type_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, type_param->getDefaultArgument(), false);
                        continue;
                      }
                      break;
                    }
                    if (auto *non_type_param = llvm::dyn_cast_or_null<
                            clang::NonTypeTemplateParmDecl>(param)) {
                      if (non_type_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, non_type_param->getDefaultArgument(),
                            false);
                        continue;
                      }
                      break;
                    }
                    if (auto *tmpl_param = llvm::dyn_cast_or_null<
                            clang::TemplateTemplateParmDecl>(param)) {
                      if (tmpl_param->hasDefaultArgument()) {
                        appendTemplateArguments(
                            tpl_args, tmpl_param->getDefaultArgument(), false);
                        continue;
                      }
                      break;
                    }
                    break;
                  }
                }
              }
            }

            SgScopeStatement *tmpl_scope = nullptr;
            if (clang::DeclContext *ctx = spec_decl->getDeclContext()) {
              tmpl_scope = resolveScopeFromDeclContext(
                  ctx, SageBuilder::topScopeStack());
            }
            if (tmpl_scope == nullptr) {
              tmpl_scope = SageBuilder::topScopeStack();
            }
            if (tmpl_scope == nullptr) {
              tmpl_scope = getGlobalScope();
            }
            *node = SageBuilder::buildNonrealType(SgName(base_name), tmpl_scope,
                                                  &tpl_args);
            if (*node == NULL) {
              SgTemplateType *tmpl_type =
                  SageBuilder::buildTemplateType(SgName(base_name));
              if (tmpl_type != nullptr) {
                tmpl_type->get_tpl_args() = tpl_args;
                for (SgTemplateArgument *arg : tmpl_type->get_tpl_args()) {
                  if (arg != nullptr) {
                    arg->set_parent(tmpl_type);
                  }
                }
                *node = tmpl_type;
              }
            }
          }
        }

        if (*node == NULL && is_specialization) {
          std::string base_name = record_decl != nullptr
                                      ? record_decl->getNameAsString()
                                      : std::string();
          if (base_name.empty()) {
            base_name = "__template_specialization";
          }
          SgTemplateType *tmpl_type =
              SageBuilder::buildTemplateType(SgName(base_name));
          if (tmpl_type != nullptr) {
            *node = tmpl_type;
          }
        }

        if (*node == NULL && !is_specialization) {
          std::string qualified_name = lookup_decl->getQualifiedNameAsString();
          if (qualified_name.empty()) {
            qualified_name = "__anonymous_record";
          }
          // std::isalnum expects values representable as unsigned char; cast to
          // avoid UB for negative char.
          for (char &ch : qualified_name) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
              ch = '_';
            }
          }
          SgScopeStatement *scope = getSafeOpaqueTypeInsertionScope();
          *node = SageBuilder::buildOpaqueType(qualified_name, scope);
        }
      }
    } else {
      *node = class_sym->get_type();
    }
  }

  // After translating the declaration, the symbol should now exist; refresh the
  // lookup for the first-seen tracking (matches historical behavior).
  if (class_sym == NULL) {
    class_sym = isSgClassSymbol(GetSymbolFromSymbolTable(lookup_decl));
  }

  bool first_see_in_type = (class_sym == NULL);
  if (isSgClassType(*node) != NULL) {
    p_class_type_decl_first_see_in_type.insert(std::pair<SgClassType *, bool>(
        isSgClassType(*node), first_see_in_type));
    if (first_see_in_type) {
      isSgNamedType(*node)->set_autonomous_declaration(true);
    }
  }

  return VisitType(record_type, node);
}

// Build template parameters by inferring from instantiation arguments
std::unique_ptr<SgTemplateParameterPtrList>
ClangToSageTranslator::buildTemplateParameters(
    const clang::TemplateSpecializationType *clang_type) {

  // For Clang frontend, we don't have access to the original template parameter
  // declarations since they're in standard library headers. We need to infer
  // parameters from the instantiation arguments.

  auto param_list = std::make_unique<SgTemplateParameterPtrList>();

  auto args = clang_type->template_arguments();
  int param_position = 0;

  for (const clang::TemplateArgument &arg : args) {
    SgType *param_type = nullptr;
    SgTemplateParameter::template_parameter_enum param_kind;
    bool is_param_pack = false;

    switch (arg.getKind()) {
    case clang::TemplateArgument::Type:
      // Type parameter (e.g., typename T)
      param_kind = SgTemplateParameter::type_parameter;
      param_type = SageBuilder::buildTemplateType(
          SgName("T" + std::to_string(param_position)));
      break;

    case clang::TemplateArgument::Integral:
      // Non-type parameter (e.g., size_t N)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getIntegralType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    case clang::TemplateArgument::Template: {
      // Template template parameter
      param_kind = SgTemplateParameter::template_parameter;
      std::string param_name = "Template" + std::to_string(param_position);
      SgTemplateType *ttype =
          SageBuilder::buildTemplateType(SgName(param_name));

      // Create SgNonrealDecl
      SgScopeStatement *current_scope = SageBuilder::topScopeStack();
      ROSE_ASSERT(current_scope != NULL);
      SgDeclarationScope *decl_scope = isSgDeclarationScope(current_scope);
      if (decl_scope == NULL) {
        decl_scope = SageBuilder::buildDeclarationScope();
        decl_scope->set_parent(current_scope);
      }

      SgNonrealDecl *nrdecl =
          SageBuilder::buildNonrealDecl(SgName(param_name), decl_scope);
      diagnose_null_scope(nrdecl, "TemplateTemplateArgument");
      // nrdecl->set_type(ttype); // Removed: SgNonrealDecl expects
      // SgNonrealType

      // Translate inner parameters
      clang::TemplateName tname = arg.getAsTemplate();
      clang::TemplateDecl *tdecl = tname.getAsTemplateDecl();

      if (tdecl) {
        auto inner_params = translateTemplateParameterList(
            tdecl->getTemplateParameters(), nrdecl);
        nrdecl->get_tpl_params() = *inner_params;
      }

      SgTemplateParameter *param = SageBuilder::buildTemplateParameter(
          SgTemplateParameter::template_parameter, ttype);
      param->set_templateDeclaration(nrdecl);

      param_list->push_back(param);
      param_position++;
      continue;
    }

    case clang::TemplateArgument::Pack:
      // Parameter pack (e.g., typename ... Args)
      // We infer it as a type parameter pack
      param_kind = SgTemplateParameter::type_parameter;
      param_type = SageBuilder::buildTemplateType(
          SgName("Args" + std::to_string(param_position)));
      if (SgTemplateType *ttype = isSgTemplateType(param_type)) {
        ttype->set_packed(true);
      }
      is_param_pack = true;
      break;

    case clang::TemplateArgument::Expression:
      // Expression argument (e.g., sizeof(T))
      // We infer it as a non-type parameter
      param_kind = SgTemplateParameter::nontype_parameter;
      if (clang::Expr *expr = arg.getAsExpr()) {
        param_type = buildTypeFromQualifiedType(expr->getType());
      } else {
        // Fallback if expression is null (shouldn't happen for Expression kind)
        param_type = SageBuilder::buildIntType();
      }
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    case clang::TemplateArgument::Declaration: {
      // Non-type parameter (e.g., void (*F)())
      param_kind = SgTemplateParameter::nontype_parameter;
      clang::ValueDecl *decl = arg.getAsDecl();
      if (decl) {
        param_type = buildTypeFromQualifiedType(decl->getType());
      } else {
        // Should not happen for Declaration kind
        param_type = SageBuilder::buildVoidType();
      }
      if (param_type == nullptr) {
        param_type = SageBuilder::buildVoidType();
      }
      break;
    }

    case clang::TemplateArgument::NullPtr:
      // Non-type parameter (e.g., nullptr)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getNullPtrType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildNullptrType();
      }
      break;

    case clang::TemplateArgument::StructuralValue:
      // Non-type parameter (C++20 structural)
      param_kind = SgTemplateParameter::nontype_parameter;
      param_type = buildTypeFromQualifiedType(arg.getStructuralValueType());
      if (param_type == nullptr) {
        param_type = SageBuilder::buildIntType();
      }
      break;

    default:
      std::cerr << "Warning: Unsupported template parameter kind: "
                << arg.getKind() << " (Pack=" << clang::TemplateArgument::Pack
                << ", Expression=" << clang::TemplateArgument::Expression
                << ", Template=" << clang::TemplateArgument::Template
                << ", Integral=" << clang::TemplateArgument::Integral
                << ", Type=" << clang::TemplateArgument::Type
                << ", Declaration=" << clang::TemplateArgument::Declaration
                << ", NullPtr=" << clang::TemplateArgument::NullPtr
                << ", Null=" << clang::TemplateArgument::Null << ")"
                << std::endl;
      continue;
    }

    SgTemplateParameter *param =
        SageBuilder::buildTemplateParameter(param_kind, param_type);
    if (param != nullptr) {
      if (is_param_pack) {
        param->set_is_parameter_pack(true);
      }
      if (param_kind == SgTemplateParameter::nontype_parameter) {
        std::string name = "__non_type_param_" + std::to_string(param_position);
        SgInitializedName *init_name =
            SageBuilder::buildInitializedName(SgName(name), param_type);
        param->set_initializedName(init_name);
        init_name->set_parent(param);
        if (is_param_pack) {
          init_name->set_is_parameter_pack(true);
        }
      }
      param_list->push_back(param);
    }
    param_position++;
  }

  return param_list;
}

SgTemplateClassDeclaration *
ClangToSageTranslator::getOrCreateTemplateDeclaration(
    const std::string &template_name,
    const clang::TemplateSpecializationType *clang_type,
    SgScopeStatement *scope_override) {

  std::string cache_key = normalizeTemplateDeclCacheKey(template_name);
  auto it = p_template_decl_cache.find(cache_key);
  if (it != p_template_decl_cache.end()) {
    return it->second;
  }

  std::vector<std::string> components =
      splitQualifiedNameOutsideTemplates(template_name);
  std::string base_name =
      components.empty() ? template_name : components.back();
  base_name = stripTemplateArgs(base_name);

  SgScopeStatement *scope = scope_override;
  if (scope == nullptr) {
    scope = getGlobalScope();
    if (components.size() > 1) {
      for (size_t i = 0; i + 1 < components.size(); ++i) {
        std::string component = trimWhitespace(components[i]);
        if (component.empty()) {
          continue;
        }
        bool has_template = hasTemplateSyntaxComponent(component);
        std::string name = stripTemplateArgs(component);
        if (name.empty()) {
          continue;
        }

        if (has_template) {
          SgNonrealType *nr_type =
              SageBuilder::buildNonrealType(SgName(name), scope, nullptr);
          SgNonrealDecl *nr_decl = isSgNonrealDecl(nr_type->get_declaration());
          ROSE_ASSERT(nr_decl != nullptr);
          scope = nr_decl->get_nonreal_decl_scope();
          continue;
        }

        SgNamespaceSymbol *ns_sym =
            scope->lookup_namespace_symbol(SgName(name));
        if (ns_sym) {
          scope = ns_sym->get_declaration()->get_definition();
        } else {
          SgNamespaceDeclarationStatement *ns_decl =
              SageBuilder::buildNamespaceDeclaration(SgName(name), scope);
          scope = ns_decl->get_definition();
        }
      }
    }
  }

  // Build template parameters
  auto params = buildTemplateParameters(clang_type);

  // Create empty template argument list for primary template
  SgTemplateArgumentPtrList empty_args;

  // Create template class declaration
  SgTemplateClassDeclaration *template_decl =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          SgName(base_name),
          SgClassDeclaration::e_class, // Assume class (could be struct)
          scope, params.get(),
          &empty_args // No specialization arguments for primary template
      );

  // REX FIX: Ensure firstNondefiningDeclaration is set to avoid unparser
  // crash (ua test).
  template_decl->set_firstNondefiningDeclaration(template_decl);

  // Mark as compiler generated and forward declaration
  template_decl->setForward();
  template_decl->set_isUnNamed(false);
  template_decl->get_file_info()->setCompilerGenerated();
  template_decl->get_file_info()->unsetOutputInCodeGeneration();

  // Ensure the template declaration scope matches the enclosing scope so
  // symbol insertion does not mismatch the declaration's scope.
  if (scope != nullptr) {
    template_decl->set_scope(scope);
    if (template_decl->get_parent() == nullptr) {
      template_decl->set_parent(scope);
    }
  }

  // Do not manually insert a SgClassSymbol here.
  // SageBuilder::buildNondefiningTemplateClassDeclaration_nfi() installs the
  // appropriate SgTemplateClassSymbol; inserting a SgClassSymbol for a
  // SgTemplateClassDeclaration violates AST invariants and triggers
  // AstConsistencyTests assertions.

  // Cache it
  p_template_decl_cache[cache_key] = template_decl;

  return template_decl;
}

namespace {
// Build a literal expression from an APSInt while preserving sign and (as text)
// width.
SgExpression *buildIntegralTemplateArgExpr(const llvm::APSInt &value,
                                           SgType *int_type) {
  if (int_type != nullptr && isSgTypeBool(int_type)) {
    return SageBuilder::buildBoolValExp(value.getBoolValue());
  }

  const bool is_signed = value.isSigned();
  const unsigned bitwidth = value.getBitWidth();

  SgExpression *expr = NULL;
  if (is_signed) {
    // Use the widest native builder we have; valueString keeps the full
    // precision.
    long long v = (bitwidth <= 63) ? value.getSExtValue() : 0;
    expr = SageBuilder::buildLongLongIntVal(v);
  } else {
    unsigned long long v = (bitwidth <= 64) ? value.getZExtValue() : 0;
    expr = SageBuilder::buildUnsignedLongLongIntVal(v);
  }

  if (expr != NULL) {
    llvm::SmallString<64> buf;
    value.toString(buf, 10, value.isSigned());
    std::string text(buf.begin(), buf.end());

    if (SgLongLongIntVal *ll = isSgLongLongIntVal(expr)) {
      ll->set_valueString(text);
    } else if (SgUnsignedLongLongIntVal *ull =
                   isSgUnsignedLongLongIntVal(expr)) {
      ull->set_valueString(text);
    }
  }

  return expr;
}

size_t countExpandedTemplateArgument(const clang::TemplateArgument &arg) {
  if (arg.getKind() == clang::TemplateArgument::Pack) {
    size_t count = 0;
    for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
      count += countExpandedTemplateArgument(pack_arg);
    }
    return count;
  }
  return 1;
}
} // namespace

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgument &arg, bool explicitlySpecified) {
  if (arg.isPackExpansion()) {
    clang::TemplateArgument pattern = arg.getPackExpansionPattern();
    if (SgTemplateArgument *sg_pattern =
            translateTemplateArgument(pattern, explicitlySpecified)) {
      sg_pattern->set_is_pack_element(true);
      return sg_pattern;
    }
  }

  SgTemplateArgument *sg_arg = nullptr;

  auto build_decl_expr =
      [&](clang::ValueDecl *decl,
          SgInitializedName **out_init_name) -> SgExpression * {
    if (out_init_name != nullptr) {
      *out_init_name = nullptr;
    }
    if (decl == nullptr) {
      return nullptr;
    }

    SgSymbol *sym = GetSymbolFromSymbolTable(decl);
    if (sym == nullptr) {
      Traverse(decl);
      sym = GetSymbolFromSymbolTable(decl);
    }
    if (SgAliasSymbol *alias_sym = isSgAliasSymbol(sym)) {
      if (SgSymbol *base_sym = alias_sym->get_base()) {
        sym = base_sym;
      }
    }

    if (SgVariableSymbol *var_sym = isSgVariableSymbol(sym)) {
      if (out_init_name != nullptr) {
        *out_init_name = var_sym->get_declaration();
      }
      return SageBuilder::buildVarRefExp(var_sym);
    }
    if (SgMemberFunctionSymbol *member_sym = isSgMemberFunctionSymbol(sym)) {
      return SageBuilder::buildMemberFunctionRefExp_nfi(member_sym, false,
                                                        false);
    }
    if (SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym)) {
      return SageBuilder::buildFunctionRefExp(func_sym);
    }
    if (SgEnumFieldSymbol *enum_sym = isSgEnumFieldSymbol(sym)) {
      SgInitializedName *init_name = enum_sym->get_declaration();
      if (out_init_name != nullptr) {
        *out_init_name = init_name;
      }

      SgEnumDeclaration *enum_decl = nullptr;
      if (init_name != nullptr && init_name->get_type() != nullptr) {
        if (SgEnumType *enum_type = isSgEnumType(init_name->get_type())) {
          enum_decl = isSgEnumDeclaration(enum_type->get_declaration());
        }
      }
      if (enum_decl == nullptr && init_name != nullptr) {
        enum_decl = isSgEnumDeclaration(init_name->get_parent());
      }
      if (enum_decl != nullptr) {
        const clang::EnumConstantDecl *enum_const_decl =
            llvm::dyn_cast<clang::EnumConstantDecl>(decl);
        ROSE_ASSERT(enum_const_decl != nullptr);
        long long enum_value = enum_const_decl->getInitVal().getExtValue();
        return SageBuilder::buildEnumVal_nfi(enum_value, enum_decl,
                                             enum_sym->get_name());
      }
    }

    return nullptr;
  };
  auto build_structural_expr = [&](const clang::APValue &value,
                                   SgType *value_type) -> SgExpression * {
    if (value.isInt()) {
      return buildIntegralTemplateArgExpr(value.getInt(), value_type);
    }
    if (value.isFloat()) {
      llvm::SmallString<64> buf;
      value.getFloat().toString(buf);
      std::string text(buf.begin(), buf.end());

      if (isSgTypeFloat(value_type)) {
        return SageBuilder::buildFloatVal_nfi(0.0f, text);
      }
      if (isSgTypeLongDouble(value_type)) {
        return SageBuilder::buildLongDoubleVal_nfi(0.0L, text);
      }
      return SageBuilder::buildDoubleVal_nfi(0.0, text);
    }
    if (value.isLValue()) {
      clang::APValue::LValueBase base = value.getLValueBase();
      if (const clang::ValueDecl *decl =
              base.dyn_cast<const clang::ValueDecl *>()) {
        return build_decl_expr(const_cast<clang::ValueDecl *>(decl), nullptr);
      }
      if (const clang::Expr *expr = base.dyn_cast<const clang::Expr *>()) {
        SgNode *sg_node = Traverse(const_cast<clang::Expr *>(expr));
        return isSgExpression(sg_node);
      }
    }
    return nullptr;
  };
  auto build_template_decl_argument =
      [&](clang::TemplateName template_name) -> SgDeclarationStatement * {
    clang::TemplateDecl *template_decl = template_name.getAsTemplateDecl();
    SgDeclarationStatement *sg_decl = nullptr;
    if (template_decl != nullptr) {
      SgNode *traverse_result = TraverseOnDemand(template_decl);
      sg_decl = isSgDeclarationStatement(traverse_result);

      // Traverse returns SgTemplateParameter for TemplateTemplateParmDecl via
      // VisitTemplateTemplateParmDecl. SgTemplateParameter is NOT an
      // SgDeclarationStatement, but it holds the SgNonrealDecl we need.
      if (SgTemplateParameter *param = isSgTemplateParameter(traverse_result)) {
        if (SgDeclarationStatement *inner_decl =
                param->get_templateDeclaration()) {
          if (isSgNonrealDecl(inner_decl)) {
            sg_decl = inner_decl;
          }
        }
      }
    }

    clang::NestedNameSpecifier *qualifier = nullptr;
    std::string name_str;
    bool has_template_keyword = false;
    if (const clang::QualifiedTemplateName *qualified =
            template_name.getAsQualifiedTemplateName()) {
      qualifier = qualified->getQualifier();
      has_template_keyword = qualified->hasTemplateKeyword();
      const clang::TemplateDecl *qualified_decl =
          qualified->getUnderlyingTemplate().getAsTemplateDecl();
      name_str = qualified_decl
                     ? qualified_decl->getNameAsString()
                     : (template_decl ? template_decl->getNameAsString() : "");
    } else if (const clang::DependentTemplateName *dependent =
                   template_name.getAsDependentTemplateName()) {
      qualifier = dependent->getQualifier();
      has_template_keyword = true;
      name_str = getTemplateNameBase(template_name);
    } else {
      name_str = template_decl ? template_decl->getNameAsString() : "";
    }

    if (sg_decl == nullptr && name_str.empty()) {
      name_str = getTemplateNameBase(template_name);
    }

    if (qualifier != nullptr && !name_str.empty()) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      SgNonrealType *nr_type = buildNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(name_str), nullptr);
      if (SgNonrealDecl *nr_decl =
              isSgNonrealDecl(nr_type ? nr_type->get_declaration() : nullptr)) {
        if (SgTemplateDeclaration *template_decl_stmt =
                isSgTemplateDeclaration(sg_decl)) {
          nr_decl->set_templateDeclaration(template_decl_stmt);
        }
        if (has_template_keyword) {
          nr_decl->set_has_template_keyword(true);
        }
        sg_decl = nr_decl;
      }
    }

    if (sg_decl == nullptr && !name_str.empty()) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      SgNonrealType *nr_type =
          SageBuilder::buildNonrealType(SgName(name_str), scope, nullptr);
      if (SgNonrealDecl *nr_decl =
              isSgNonrealDecl(nr_type ? nr_type->get_declaration() : nullptr)) {
        if (has_template_keyword) {
          nr_decl->set_has_template_keyword(true);
        }
        sg_decl = nr_decl;
      }
    }

    return sg_decl;
  };

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    SgType *arg_type = buildTypeFromQualifiedType(arg.getAsType());
    if (const clang::TemplateSpecializationType *arg_tst =
            llvm::dyn_cast_or_null<clang::TemplateSpecializationType>(
                arg.getAsType().getTypePtrOrNull())) {
      auto build_template_specialization_type =
          [&](const clang::TemplateSpecializationType *tst) -> SgType * {
        if (tst == nullptr) {
          return nullptr;
        }
        clang::TemplateName tname = tst->getTemplateName();
        std::string base_name = getTemplateNameBase(tname);
        if (base_name.empty()) {
          return nullptr;
        }

        SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
        clang::NestedNameSpecifier *qualifier = nullptr;
        bool has_template_keyword = false;
        if (const clang::QualifiedTemplateName *qtn =
                tname.getAsQualifiedTemplateName()) {
          qualifier = qtn->getQualifier();
          has_template_keyword = qtn->hasTemplateKeyword();
        } else if (const clang::DependentTemplateName *dtn =
                       tname.getAsDependentTemplateName()) {
          qualifier = dtn->getQualifier();
          has_template_keyword = true;
        }

        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }

        SgNonrealType *nrtype = nullptr;
        if (qualifier != nullptr) {
          nrtype = buildNonrealTypeFromNestedNameSpecifier(
              qualifier, scope, SgName(base_name),
              tpl_args.empty() ? nullptr : &tpl_args);
        } else {
          nrtype = SageBuilder::buildNonrealType(
              SgName(base_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
        }
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          if (has_template_keyword) {
            nrdecl->set_has_template_keyword(true);
          }
        }
        return nrtype;
      };

      bool needs_rebuild = false;
      bool is_alias = arg_tst->isTypeAlias();
      if (SgNonrealType *nrtype = isSgNonrealType(arg_type)) {
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype->get_declaration())) {
          if (nrdecl->get_tpl_args().empty() &&
              (is_alias || !arg_tst->template_arguments().empty())) {
            needs_rebuild = true;
          }
        }
      } else if (isSgTypedefType(arg_type) != nullptr) {
        if (is_alias || !arg_tst->template_arguments().empty()) {
          needs_rebuild = true;
        }
      }

      if (needs_rebuild) {
        if (SgType *spec_type = build_template_specialization_type(arg_tst)) {
          arg_type = spec_type;
        }
      }
    }

    if (const clang::DependentTemplateSpecializationType *arg_dts =
            llvm::dyn_cast_or_null<clang::DependentTemplateSpecializationType>(
                arg.getAsType().getTypePtrOrNull())) {
      auto build_dependent_template_type =
          [&](const clang::DependentTemplateSpecializationType *dts)
          -> SgType * {
        if (dts == nullptr) {
          return nullptr;
        }
        DependentTemplateSpecializationNameInfo name_info =
            getDependentTemplateSpecializationName(dts);
        if (name_info.base_name.empty()) {
          return nullptr;
        }
        SgTemplateArgumentPtrList tpl_args;
        for (const clang::TemplateArgument &tpl_arg :
             dts->template_arguments()) {
          appendTemplateArguments(tpl_args, tpl_arg, true);
        }
        if (tpl_args.empty()) {
          return nullptr;
        }
        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }
        SgNonrealType *nrtype = nullptr;
        if (name_info.qualifier != nullptr) {
          nrtype = buildNonrealTypeFromNestedNameSpecifier(
              name_info.qualifier, scope, SgName(name_info.base_name),
              &tpl_args);
        } else {
          nrtype = SageBuilder::buildNonrealType(SgName(name_info.base_name),
                                                 scope, &tpl_args);
        }
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
          if (name_info.has_template_keyword) {
            nrdecl->set_has_template_keyword(true);
          }
        }
        return nrtype;
      };

      bool needs_rebuild = false;
      if (SgNonrealType *nrtype = isSgNonrealType(arg_type)) {
        if (SgNonrealDecl *nrdecl =
                isSgNonrealDecl(nrtype->get_declaration())) {
          if (nrdecl->get_tpl_args().empty() &&
              !arg_dts->template_arguments().empty()) {
            needs_rebuild = true;
          }
        }
      } else if (isSgTypedefType(arg_type) != nullptr) {
        needs_rebuild = !arg_dts->template_arguments().empty();
      }

      if (needs_rebuild) {
        if (SgType *dep_type = build_dependent_template_type(arg_dts)) {
          arg_type = dep_type;
        }
      }
    }

    if (arg_type != NULL) {
      sg_arg = new SgTemplateArgument(arg_type, explicitlySpecified);
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    llvm::APSInt value = arg.getAsIntegral();
    SgType *int_type = buildTypeFromQualifiedType(arg.getIntegralType());

    SgExpression *value_expr = nullptr;
    if (const clang::EnumType *enum_type =
            arg.getIntegralType()->getAs<clang::EnumType>()) {
      if (const clang::EnumDecl *enum_decl = enum_type->getDecl()) {
        for (const clang::EnumConstantDecl *enum_const :
             enum_decl->enumerators()) {
          if (enum_const != nullptr && enum_const->getInitVal() == value) {
            value_expr = build_decl_expr(
                const_cast<clang::EnumConstantDecl *>(enum_const), nullptr);
            if (value_expr != nullptr) {
              break;
            }
          }
        }
      }
    }
    if (value_expr == nullptr) {
      value_expr = buildIntegralTemplateArgExpr(value, int_type);
    }

    if (value_expr != nullptr) {
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                 false /*isArrayBoundUnknownType*/, int_type,
                                 value_expr, nullptr, explicitlySpecified);
      value_expr->set_parent(sg_arg);
    }
    break;
  }

  case clang::TemplateArgument::Declaration: {
    clang::ValueDecl *decl = arg.getAsDecl();
    SgType *param_type = buildTypeFromQualifiedType(arg.getParamTypeForDecl());
    if (param_type == nullptr) {
      param_type = SageBuilder::buildIntType();
    }
    SgInitializedName *init_name = nullptr;
    SgExpression *decl_expr = build_decl_expr(decl, &init_name);
    if (decl_expr != nullptr || init_name != nullptr) {
      sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                      /*isArrayBoundUnknownType=*/false,
                                      /*type=*/param_type,
                                      /*expression=*/decl_expr,
                                      /*templateDeclaration=*/nullptr,
                                      explicitlySpecified);
      if (decl_expr != nullptr) {
        decl_expr->set_parent(sg_arg);
      }
      if (decl_expr == nullptr && init_name != nullptr) {
        sg_arg->set_initializedName(init_name);
      }
    }
    break;
  }

  case clang::TemplateArgument::NullPtr: {
    SgType *null_type = buildTypeFromQualifiedType(arg.getNullPtrType());
    SgExpression *null_expr = SageBuilder::buildNullptrValExp_nfi();
    sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                    /*isArrayBoundUnknownType=*/false,
                                    /*type=*/null_type,
                                    /*expression=*/null_expr,
                                    /*templateDeclaration=*/nullptr,
                                    explicitlySpecified);
    null_expr->set_parent(sg_arg);
    break;
  }

  case clang::TemplateArgument::StructuralValue: {
    SgType *value_type =
        buildTypeFromQualifiedType(arg.getStructuralValueType());
    const clang::APValue &value = arg.getAsStructuralValue();
    SgExpression *value_expr = build_structural_expr(value, value_type);
    if (value_expr != nullptr) {
      sg_arg = new SgTemplateArgument(SgTemplateArgument::nontype_argument,
                                      /*isArrayBoundUnknownType=*/false,
                                      /*type=*/value_type,
                                      /*expression=*/value_expr,
                                      /*templateDeclaration=*/nullptr,
                                      explicitlySpecified);
      value_expr->set_parent(sg_arg);
    }
    break;
  }

  case clang::TemplateArgument::Template: {
    clang::TemplateName template_name = arg.getAsTemplate();
    SgDeclarationStatement *sg_decl =
        build_template_decl_argument(template_name);

    if (sg_decl != nullptr) {
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                                 /*isArrayBoundUnknownType=*/false,
                                 /*type=*/nullptr,
                                 /*expression=*/nullptr,
                                 /*templateDeclaration=*/sg_decl,
                                 /*explicitlySpecified=*/explicitlySpecified);
    } else {
      MLOG_WARN_C(MLOG_FRONTEND,
                  "Warning: Failed to translate template declaration for "
                  "template argument.\n");
    }
    break;
  }

  case clang::TemplateArgument::TemplateExpansion: {
    clang::TemplateName template_name = arg.getAsTemplateOrTemplatePattern();
    SgDeclarationStatement *sg_decl =
        build_template_decl_argument(template_name);

    if (sg_decl != nullptr) {
      sg_arg =
          new SgTemplateArgument(SgTemplateArgument::template_template_argument,
                                 /*isArrayBoundUnknownType=*/false,
                                 /*type=*/nullptr,
                                 /*expression=*/nullptr,
                                 /*templateDeclaration=*/sg_decl,
                                 /*explicitlySpecified=*/explicitlySpecified);
    } else {
      MLOG_WARN_C(MLOG_FRONTEND,
                  "Warning: Failed to translate template declaration for "
                  "template argument.\n");
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    clang::Expr *clang_expr = arg.getAsExpr();
    if (clang_expr != nullptr) {
      SgNode *node = Traverse(clang_expr);
      if (SgExpression *sg_expr = isSgExpression(node)) {
        sg_arg = new SgTemplateArgument(sg_expr, explicitlySpecified);
      }
    }
    break;
  }

  case clang::TemplateArgument::Pack: {
    SgTemplateArgument *pack_marker = new SgTemplateArgument();
    pack_marker->set_argumentType(
        SgTemplateArgument::start_of_pack_expansion_argument);
    sg_arg = pack_marker;
    break;
  }

  case clang::TemplateArgument::Null:
    return nullptr;

  default:
    std::cerr << "Warning: Unsupported template argument kind: "
              << arg.getKind() << "\n";
    break;
  }

  if (sg_arg != nullptr && sg_arg->get_parent() == nullptr) {
    SgNode *fallback_parent = nullptr;
    if (p_sage_source_file != nullptr) {
      fallback_parent = p_sage_source_file;
    }
    if (fallback_parent == nullptr) {
      fallback_parent = SageBuilder::topScopeStack();
    }
    if (fallback_parent == nullptr) {
      fallback_parent = getGlobalScope();
    }
    if (fallback_parent != nullptr) {
      sg_arg->set_parent(fallback_parent);
    }
  }

  return sg_arg;
}

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  const clang::TemplateArgument &arg = arg_loc.getArgument();

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    if (const clang::TypeSourceInfo *type_info = arg_loc.getTypeSourceInfo()) {
      if (SgType *arg_type = buildTypeFromTypeLoc(type_info->getTypeLoc())) {
        auto collect_template_args_from_type_loc =
            [&](SgTemplateArgumentPtrList &tpl_args,
                bool *has_explicit_empty = nullptr) -> bool {
          clang::TypeLoc type_loc = type_info->getTypeLoc();
          while (!type_loc.isNull()) {
            if (auto elab_loc = type_loc.getAs<clang::ElaboratedTypeLoc>()) {
              type_loc = elab_loc.getNamedTypeLoc();
              continue;
            }
            if (auto tst_loc =
                    type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
              const unsigned arg_count = tst_loc.getNumArgs();
              for (unsigned i = 0; i < arg_count; ++i) {
                appendTemplateArguments(tpl_args, tst_loc.getArgLoc(i), true);
              }
              if (has_explicit_empty != nullptr) {
                *has_explicit_empty =
                    (arg_count == 0 && tst_loc.getLAngleLoc().isValid());
              }
              return true;
            }
            if (auto dep_tst_loc =
                    type_loc.getAs<
                        clang::DependentTemplateSpecializationTypeLoc>()) {
              const unsigned arg_count = dep_tst_loc.getNumArgs();
              for (unsigned i = 0; i < arg_count; ++i) {
                appendTemplateArguments(tpl_args, dep_tst_loc.getArgLoc(i),
                                        true);
              }
              if (has_explicit_empty != nullptr) {
                *has_explicit_empty = false;
              }
              return true;
            }
            type_loc = type_loc.getNextTypeLoc();
          }
          return false;
        };

        auto build_template_specialization_type =
            [&](const clang::TemplateSpecializationType *tst) -> SgType * {
          if (tst == nullptr) {
            return nullptr;
          }
          clang::TemplateName tname = tst->getTemplateName();
          std::string base_name = getTemplateNameBase(tname);
          if (base_name.empty()) {
            return nullptr;
          }

          SgTemplateArgumentPtrList tpl_args;
          bool has_explicit_empty_template_args = false;
          if (!collect_template_args_from_type_loc(
                  tpl_args, &has_explicit_empty_template_args)) {
            tpl_args = buildTemplateArguments(tst);
          }
          clang::NestedNameSpecifier *qualifier = nullptr;
          bool has_template_keyword = false;
          if (const clang::QualifiedTemplateName *qtn =
                  tname.getAsQualifiedTemplateName()) {
            qualifier = qtn->getQualifier();
            has_template_keyword = qtn->hasTemplateKeyword();
          } else if (const clang::DependentTemplateName *dtn =
                         tname.getAsDependentTemplateName()) {
            qualifier = dtn->getQualifier();
            has_template_keyword = true;
          }

          SgScopeStatement *scope = SageBuilder::topScopeStack();
          if (scope == nullptr) {
            scope = getGlobalScope();
          }

          const bool has_template_args =
              !tpl_args.empty() || has_explicit_empty_template_args;
          SgNonrealType *nrtype = nullptr;
          if (qualifier != nullptr) {
            nrtype = buildNonrealTypeFromNestedNameSpecifier(
                qualifier, scope, SgName(base_name),
                has_template_args ? &tpl_args : nullptr);
          } else {
            nrtype = SageBuilder::buildNonrealType(SgName(base_name), scope,
                                                   has_template_args ? &tpl_args
                                                                     : nullptr);
          }
          if (SgNonrealDecl *nrdecl = isSgNonrealDecl(
                  nrtype ? nrtype->get_declaration() : nullptr)) {
            if (has_template_keyword) {
              nrdecl->set_has_template_keyword(true);
            }
          }
          return nrtype;
        };

        auto find_template_specialization_loc = [&](clang::TypeLoc type_loc)
            -> clang::TemplateSpecializationTypeLoc {
          while (!type_loc.isNull()) {
            if (auto elab_loc = type_loc.getAs<clang::ElaboratedTypeLoc>()) {
              type_loc = elab_loc.getNamedTypeLoc();
              continue;
            }
            if (auto tst_loc =
                    type_loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
              return tst_loc;
            }
            type_loc = type_loc.getNextTypeLoc();
          }
          return clang::TemplateSpecializationTypeLoc();
        };

        auto find_dependent_template_specialization_loc =
            [&](clang::TypeLoc type_loc)
            -> clang::DependentTemplateSpecializationTypeLoc {
          while (!type_loc.isNull()) {
            if (auto elab_loc = type_loc.getAs<clang::ElaboratedTypeLoc>()) {
              type_loc = elab_loc.getNamedTypeLoc();
              continue;
            }
            if (auto dep_loc =
                    type_loc.getAs<
                        clang::DependentTemplateSpecializationTypeLoc>()) {
              return dep_loc;
            }
            type_loc = type_loc.getNextTypeLoc();
          }
          return clang::DependentTemplateSpecializationTypeLoc();
        };

        const clang::TemplateSpecializationType *arg_tst = nullptr;
        const clang::Type *arg_type_ptr = arg.getAsType().getTypePtrOrNull();
        if (arg_type_ptr != nullptr) {
          arg_tst =
              llvm::dyn_cast<clang::TemplateSpecializationType>(arg_type_ptr);
        }
        clang::TemplateSpecializationTypeLoc written_tst_loc =
            find_template_specialization_loc(type_info->getTypeLoc());

        SgType *resolved_type = arg_type;
        if (arg_tst == nullptr && !written_tst_loc.isNull()) {
          arg_tst = written_tst_loc.getTypePtr();
        }
        if (arg_tst != nullptr) {
          if (SgType *spelled_type =
                  build_template_specialization_type(arg_tst)) {
            resolved_type = spelled_type;
          }
        } else {
          const clang::Type *raw_type = arg.getAsType().getTypePtrOrNull();
          const clang::RecordType *record_type =
              raw_type != nullptr ? raw_type->getAs<clang::RecordType>()
                                  : nullptr;
          const clang::RecordDecl *record_decl =
              record_type != nullptr ? record_type->getDecl() : nullptr;
          if (record_decl != nullptr &&
              (resolved_type == nullptr ||
               isSgTypeUnknown(resolved_type) != nullptr)) {
            SgClassDeclaration *record_sg_decl =
                isSgClassDeclaration(lookupSgDeclarationForClangDecl(
                    const_cast<clang::RecordDecl *>(record_decl),
                    /*allow_on_demand=*/true));
            if (record_sg_decl == nullptr &&
                p_decl_translation_in_progress.find(
                    const_cast<clang::RecordDecl *>(record_decl)) ==
                    p_decl_translation_in_progress.end()) {
              if (SgNode *record_node = TraverseOnDemand(
                      const_cast<clang::RecordDecl *>(record_decl))) {
                record_sg_decl = isSgClassDeclaration(record_node);
              }
            }
            if (record_sg_decl != nullptr) {
              if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
                      record_sg_decl->get_firstNondefiningDeclaration())) {
                record_sg_decl = first_nondef;
              }
              resolved_type = SgClassType::createType(record_sg_decl);
            }
          }
        }

        return new SgTemplateArgument(resolved_type, explicitlySpecified);
      }
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    // Prefer source-expression spelling from TemplateArgumentLoc. Canonical
    // argument expressions may drop dependent qualification.
    const clang::Expr *expr = arg_loc.getSourceExpression();
    if (expr == nullptr) {
      expr = arg.getAsExpr();
    }
    if (expr != nullptr) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        return new SgTemplateArgument(sg_expr, explicitlySpecified);
      }
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    // Preserve source-level spelling for integral non-type template arguments
    // (e.g., enum constants) when available.
    if (const clang::Expr *expr = arg_loc.getSourceIntegralExpression()) {
      SgNode *node = Traverse(const_cast<clang::Expr *>(expr));
      if (SgExpression *sg_expr = isSgExpression(node)) {
        SgType *int_type = buildTypeFromQualifiedType(arg.getIntegralType());
        SgTemplateArgument *sg_arg = new SgTemplateArgument(
            SgTemplateArgument::nontype_argument,
            /*isArrayBoundUnknownType=*/false, int_type, sg_expr,
            /*templateDeclaration=*/nullptr, explicitlySpecified);
        sg_expr->set_parent(sg_arg);
        return sg_arg;
      }
    }
    break;
  }

  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion: {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(arg, explicitlySpecified)) {
      if (sg_arg->get_argumentType() ==
              SgTemplateArgument::template_template_argument &&
          sg_arg->get_templateDeclaration() != nullptr) {
        clang::NestedNameSpecifierLoc qualifier_loc =
            arg_loc.getTemplateQualifierLoc();
        clang::NestedNameSpecifier *qualifier =
            qualifier_loc.getNestedNameSpecifier();
        if (qualifier != nullptr) {
          std::string name_str;
          if (SgTemplateDeclaration *template_decl =
                  isSgTemplateDeclaration(sg_arg->get_templateDeclaration())) {
            name_str = template_decl->get_name().getString();
          } else if (SgNonrealDecl *nonreal_decl =
                         isSgNonrealDecl(sg_arg->get_templateDeclaration())) {
            name_str = nonreal_decl->get_name().getString();
          }
          if (!name_str.empty()) {
            SgScopeStatement *scope = SageBuilder::topScopeStack();
            if (scope == nullptr) {
              scope = getGlobalScope();
            }
            SgNonrealType *nr_type = buildNonrealTypeFromNestedNameSpecifier(
                qualifier, scope, SgName(name_str), nullptr);
            if (SgNonrealDecl *nr_decl = isSgNonrealDecl(
                    nr_type ? nr_type->get_declaration() : nullptr)) {
              if (SgTemplateDeclaration *template_decl =
                      isSgTemplateDeclaration(
                          sg_arg->get_templateDeclaration())) {
                nr_decl->set_templateDeclaration(template_decl);
              }
              sg_arg->set_templateDeclaration(nr_decl);
            }
          }
        }
      }
      return sg_arg;
    }
    break;
  }

  default:
    break;
  }

  return translateTemplateArgument(arg, explicitlySpecified);
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list, const clang::TemplateArgument &arg,
    bool explicitlySpecified) {
  if (arg.isPackExpansion()) {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(arg, explicitlySpecified)) {
      arg_list.push_back(sg_arg);
      if (sg_arg->get_parent() == nullptr) {
        ensureTemplateArgumentParents(arg_list);
      }
    }
    return;
  }

  if (arg.getKind() == clang::TemplateArgument::Pack) {
    auto elements = arg.pack_elements();
    if (elements.empty()) {
      SgTemplateArgument *pack_marker = new SgTemplateArgument();
      pack_marker->set_argumentType(
          SgTemplateArgument::start_of_pack_expansion_argument);
      arg_list.push_back(pack_marker);
    } else {
      for (const clang::TemplateArgument &pack_arg : elements) {
        appendTemplateArguments(arg_list, pack_arg, explicitlySpecified);
      }
    }
    return;
  }

  if (SgTemplateArgument *sg_arg =
          translateTemplateArgument(arg, explicitlySpecified)) {
    arg_list.push_back(sg_arg);
    if (sg_arg->get_parent() == nullptr) {
      ensureTemplateArgumentParents(arg_list);
    }
  }
}

void ClangToSageTranslator::appendTemplateArguments(
    SgTemplateArgumentPtrList &arg_list,
    const clang::TemplateArgumentLoc &arg_loc, bool explicitlySpecified) {
  const clang::TemplateArgument &arg = arg_loc.getArgument();

  if (arg.isPackExpansion()) {
    if (SgTemplateArgument *sg_arg =
            translateTemplateArgument(arg, explicitlySpecified)) {
      arg_list.push_back(sg_arg);
      if (sg_arg->get_parent() == nullptr) {
        ensureTemplateArgumentParents(arg_list);
      }
    }
    return;
  }

  if (arg.getKind() == clang::TemplateArgument::Pack) {
    auto elements = arg.pack_elements();
    if (elements.empty()) {
      SgTemplateArgument *pack_marker = new SgTemplateArgument();
      pack_marker->set_argumentType(
          SgTemplateArgument::start_of_pack_expansion_argument);
      arg_list.push_back(pack_marker);
    } else {
      for (const clang::TemplateArgument &pack_arg : elements) {
        appendTemplateArguments(arg_list, pack_arg, explicitlySpecified);
      }
    }
    return;
  }

  if (SgTemplateArgument *sg_arg =
          translateTemplateArgument(arg_loc, explicitlySpecified)) {
    arg_list.push_back(sg_arg);
    if (sg_arg->get_parent() == nullptr) {
      ensureTemplateArgumentParents(arg_list);
    }
  }
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateSpecializationType *clang_type) {

  SgTemplateArgumentPtrList arg_list;

  if (clang_type == nullptr) {
    return arg_list;
  }

  auto args_as_written = clang_type->template_arguments();
  const clang::TemplateArgumentList *full_args = nullptr;

  if (clang::QualType qt(clang_type, 0); !qt.isNull()) {
    if (const clang::CXXRecordDecl *record_decl = qt->getAsCXXRecordDecl()) {
      if (const auto *spec_decl =
              llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                  record_decl)) {
        full_args = &spec_decl->getTemplateArgs();
      }
    }
  }

  const bool use_full_args =
      !clang_type->isTypeAlias() && !clang_type->isCurrentInstantiation() &&
      full_args != nullptr && full_args->size() > args_as_written.size();

  if (use_full_args) {
    for (unsigned i = 0; i < full_args->size(); ++i) {
      appendTemplateArguments(arg_list, full_args->get(i), false);
    }
  } else {
    for (const clang::TemplateArgument &arg : args_as_written) {
      appendTemplateArguments(arg_list, arg, true);
    }
  }

  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified) {
  SgTemplateArgumentPtrList arg_list;

  for (const clang::TemplateArgumentLoc &arg_loc : arg_info.arguments()) {
    appendTemplateArguments(arg_list, arg_loc, explicitlySpecified);
  }

  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentList &args, size_t explicit_count) {
  SgTemplateArgumentPtrList arg_list;
  for (unsigned i = 0; i < args.size(); ++i) {
    appendTemplateArguments(arg_list, args.get(i), false);
  }

  applyExplicitTemplateArgumentFlags(arg_list, explicit_count);
  ensureTemplateArgumentParents(arg_list);
  return arg_list;
}

void ClangToSageTranslator::ensureTemplateArgumentParents(
    SgTemplateArgumentPtrList &args) {
  SgNode *fallback_parent = nullptr;
  if (p_sage_source_file != NULL) {
    fallback_parent = p_sage_source_file;
  }
  if (fallback_parent == nullptr) {
    fallback_parent = SageBuilder::topScopeStack();
  }
  if (fallback_parent == nullptr) {
    fallback_parent = getGlobalScope();
  }
  if (fallback_parent == nullptr) {
    return;
  }

  for (SgTemplateArgument *arg : args) {
    if (arg != NULL && arg->get_parent() == NULL) {
      arg->set_parent(fallback_parent);
    }
  }
}

void ClangToSageTranslator::applyExplicitTemplateArgumentFlags(
    SgTemplateArgumentPtrList &args, size_t explicit_count) {
  if (explicit_count == 0) {
    return;
  }

  size_t limit = explicit_count < args.size() ? explicit_count : args.size();
  for (size_t i = 0; i < limit; ++i) {
    SgTemplateArgument *arg = args[i];
    if (arg != NULL && !arg->get_explicitlySpecified()) {
      arg->set_explicitlySpecified(true);
    }
  }
}

size_t ClangToSageTranslator::countExpandedTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info) {
  size_t count = 0;
  for (const clang::TemplateArgumentLoc &loc : arg_info.arguments()) {
    count += countExpandedTemplateArgument(loc.getArgument());
  }
  return count;
}

SgNonrealType *
ClangToSageTranslator::buildNonrealTypeForNestedNameSpecifierType(
    const clang::Type *clang_type, SgScopeStatement *scope,
    bool prefer_current_scope) {
  if (clang_type == nullptr) {
    return nullptr;
  }

  if (const clang::ElaboratedType *elab =
          llvm::dyn_cast<clang::ElaboratedType>(clang_type)) {
    return buildNonrealTypeForNestedNameSpecifierType(
        elab->getNamedType().getTypePtrOrNull(), scope, prefer_current_scope);
  }

  if (const clang::SubstTemplateTypeParmType *subst =
          llvm::dyn_cast<clang::SubstTemplateTypeParmType>(clang_type)) {
    clang::QualType replacement = subst->getReplacementType();
    return buildNonrealTypeForNestedNameSpecifierType(
        replacement.getTypePtrOrNull(), scope, prefer_current_scope);
  }

  if (const clang::SubstTemplateTypeParmPackType *pack =
          llvm::dyn_cast<clang::SubstTemplateTypeParmPackType>(clang_type)) {
    std::string name_str;
    if (const clang::TemplateTypeParmDecl *decl =
            pack->getReplacedParameter()) {
      if (decl->getDeclName().isIdentifier()) {
        name_str = decl->getNameAsString();
      }
      if (name_str.empty()) {
        name_str = "template_type_param_" + std::to_string(decl->getDepth()) +
                   "_" + std::to_string(decl->getIndex());
      }
    }
    if (name_str.empty()) {
      if (const clang::IdentifierInfo *id = pack->getIdentifier()) {
        name_str = id->getName().str();
      }
    }
    if (name_str.empty()) {
      name_str = "template_type_param_" + std::to_string(pack->getIndex());
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype =
        SageBuilder::buildNonrealType(SgName(name_str), scope, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
    }
    return nrtype;
  }

  auto sanitize_nonreal_name = [&](const std::string &raw) -> std::string {
    if (raw.empty()) {
      return raw;
    }
    std::vector<std::string> components =
        splitQualifiedNameOutsideTemplates(raw);
    std::string base = components.empty() ? raw : components.back();
    base = stripTemplateArgs(base);
    return trimWhitespace(base);
  };

  auto build_with_qualifier =
      [&](clang::NestedNameSpecifier *qualifier, const std::string &name,
          const SgTemplateArgumentPtrList *tpl_args) -> SgNonrealType * {
    std::string base_name = sanitize_nonreal_name(name);
    ROSE_ASSERT(!base_name.empty());
    if (prefer_current_scope) {
      qualifier = nullptr;
    }
    if (qualifier != nullptr) {
      return buildNonrealTypeFromNestedNameSpecifier(
          qualifier, scope, SgName(base_name), tpl_args);
    }
    return SageBuilder::buildNonrealType(SgName(base_name), scope, tpl_args);
  };
  auto fallback_type_name = [&](const clang::Type *type) -> std::string {
    if (type == nullptr) {
      return "";
    }
    const char *class_name = type->getTypeClassName();
    std::string result = "__";
    if (class_name != nullptr && class_name[0] != '\0') {
      result += class_name;
    } else {
      result += "unknown_type";
    }
    result += "_" + Rose::StringUtility::numberToString(
                        reinterpret_cast<uintptr_t>(type));
    return result;
  };

  if (const clang::UnresolvedUsingType *uut =
          llvm::dyn_cast<clang::UnresolvedUsingType>(clang_type)) {
    const clang::UnresolvedUsingTypenameDecl *decl = uut->getDecl();
    std::string name_str = decl ? decl->getNameAsString() : "";
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(decl ? decl->getQualifier() : nullptr, name_str,
                                nullptr);
  }

  if (const clang::DependentNameType *dnt =
          llvm::dyn_cast<clang::DependentNameType>(clang_type)) {
    const clang::IdentifierInfo *id = dnt->getIdentifier();
    ROSE_ASSERT(id != nullptr);
    return build_with_qualifier(dnt->getQualifier(), id->getName().str(),
                                nullptr);
  }

  if (const clang::DependentTemplateSpecializationType *dts =
          llvm::dyn_cast<clang::DependentTemplateSpecializationType>(
              clang_type)) {
    DependentTemplateSpecializationNameInfo name_info =
        getDependentTemplateSpecializationName(dts);

    SgTemplateArgumentPtrList tpl_args;
    for (const clang::TemplateArgument &arg : dts->template_arguments()) {
      appendTemplateArguments(tpl_args, arg, true);
    }
    if (!tpl_args.empty()) {
      ensureTemplateArgumentParents(tpl_args);
    }

    SgNonrealType *nrtype = build_with_qualifier(
        name_info.qualifier, name_info.base_name, &tpl_args);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (name_info.has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
      }
    }
    return nrtype;
  }

  if (const clang::TemplateSpecializationType *tst =
          llvm::dyn_cast<clang::TemplateSpecializationType>(clang_type)) {
    clang::TemplateName tname = tst->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
    ROSE_ASSERT(!base_name.empty());

    SgTemplateArgumentPtrList tpl_args = buildTemplateArguments(tst);
    applyExplicitTemplateArgumentFlags(tpl_args, tpl_args.size());
    clang::NestedNameSpecifier *qualifier = nullptr;
    bool has_template_keyword = false;
    auto resolve_template_decl =
        [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
      clang::TemplateName current = name;
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return decl;
        }
        if (const clang::QualifiedTemplateName *qtn =
                current.getAsQualifiedTemplateName()) {
          clang::TemplateName underlying = qtn->getUnderlyingTemplate();
          if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
            return decl;
          }
          current = underlying;
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *subst =
                current.getAsSubstTemplateTemplateParm()) {
          current = subst->getReplacement();
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          return llvm::dyn_cast_or_null<clang::TemplateDecl>(
              using_shadow->getTargetDecl());
        }
        return nullptr;
      }
    };

    clang::TemplateDecl *template_decl = nullptr;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
      has_template_keyword = qtn->hasTemplateKeyword();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
      has_template_keyword = true;
    }
    if (prefer_current_scope) {
      qualifier = nullptr;
    }
    template_decl = resolve_template_decl(tname);
    SgNonrealType *nrtype = nullptr;
    if (qualifier != nullptr) {
      nrtype = build_with_qualifier(qualifier, base_name, &tpl_args);
    } else {
      clang::NestedNameSpecifier *ns_qualifier = nullptr;
      if (!prefer_current_scope && template_decl != nullptr &&
          p_compiler_instance != nullptr) {
        ns_qualifier = buildNamespaceQualifierForDeclContext(
            template_decl->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
      if (ns_qualifier != nullptr) {
        nrtype = build_with_qualifier(ns_qualifier, base_name, &tpl_args);
      } else {
        SgScopeStatement *template_scope = scope;
        if (!prefer_current_scope && template_decl != nullptr) {
          if (clang::DeclContext *decl_context =
                  template_decl->getDeclContext()) {
            if (SgScopeStatement *resolved_scope =
                    resolveScopeFromDeclContext(decl_context, nullptr)) {
              template_scope = resolved_scope;
            } else {
              clang::DeclContext *scope_ctx = decl_context;
              while (scope_ctx != nullptr && !scope_ctx->isNamespace() &&
                     !scope_ctx->isTranslationUnit()) {
                scope_ctx = scope_ctx->getParent();
              }
              if (clang::NamespaceDecl *ns_decl =
                      llvm::dyn_cast_or_null<clang::NamespaceDecl>(
                          llvm::dyn_cast_or_null<clang::Decl>(scope_ctx))) {
                if (SgNamespaceDeclarationStatement *ns_stmt =
                        ensureNamespaceDeclaration(ns_decl)) {
                  if (ns_stmt->get_definition() != nullptr) {
                    template_scope = ns_stmt->get_definition();
                  }
                }
              } else if (scope_ctx != nullptr &&
                         scope_ctx->isTranslationUnit()) {
                template_scope = getGlobalScope();
              }
            }
          }
        }
        nrtype = SageBuilder::buildNonrealType(SgName(base_name),
                                               template_scope, &tpl_args);
      }
    }
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
      }
      if (!tst->isDependentType()) {
        nrdecl->set_suppress_typename(true);
      }
    }
    return nrtype;
  }

  if (const clang::TemplateTypeParmType *ttp =
          llvm::dyn_cast<clang::TemplateTypeParmType>(clang_type)) {
    std::string name_str;
    SgScopeStatement *template_param_scope = scope;

    if (const clang::TemplateTypeParmDecl *decl = ttp->getDecl()) {
      auto map_it = p_decl_translation_map.find(
          const_cast<clang::TemplateTypeParmDecl *>(decl));
      if (map_it != p_decl_translation_map.end()) {
        if (SgTemplateParameter *sg_param =
                isSgTemplateParameter(map_it->second)) {
          if (SgTemplateType *existing_type =
                  isSgTemplateType(sg_param->get_type())) {
            name_str = existing_type->get_name().getString();
            if (existing_type->get_packed() || ttp->isParameterPack()) {
              existing_type->set_packed(true);
            }
          }

          if (SgDeclarationStatement *owner_decl = isSgDeclarationStatement(
                  sg_param->get_templateDeclaration())) {
            if (SgDeclarationScope *decl_scope =
                    SageBuilder::getOrCreateNonrealDeclarationScope(
                        owner_decl)) {
              template_param_scope = decl_scope;
            }
          }
        }
      }

      if (name_str.empty() && decl->getDeclName().isIdentifier()) {
        name_str = decl->getNameAsString();
      }
      if (name_str.empty()) {
        name_str = decl->getNameAsString();
      }
    }

    if (name_str.empty()) {
      name_str = "template_type_param_" + std::to_string(ttp->getDepth()) +
                 "_" + std::to_string(ttp->getIndex());
    }
    ROSE_ASSERT(!name_str.empty());

    SgNonrealType *nrtype = SageBuilder::buildNonrealType(
        SgName(name_str), template_param_scope, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      nrdecl->set_is_template_param(true);
      nrdecl->set_suppress_typename(true);
    }
    return nrtype;
  }

  if (const clang::TypedefType *tdef =
          llvm::dyn_cast<clang::TypedefType>(clang_type)) {
    std::string name_str = tdef->getDecl()->getNameAsString();
    ROSE_ASSERT(!name_str.empty());
    clang::NestedNameSpecifier *qualifier = nullptr;
    if (!prefer_current_scope && p_compiler_instance != nullptr) {
      if (!scopeIsWithinNamespaceChain(scope,
                                       tdef->getDecl()->getDeclContext())) {
        qualifier = buildNamespaceQualifierForDeclContext(
            tdef->getDecl()->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
    }
    return build_with_qualifier(qualifier, name_str, nullptr);
  }

  if (const clang::UsingType *using_type =
          llvm::dyn_cast<clang::UsingType>(clang_type)) {
    clang::UsingShadowDecl *using_decl = using_type->getFoundDecl();
    std::string name_str = using_decl ? using_decl->getNameAsString() : "";
    if (name_str.empty()) {
      name_str = fallback_type_name(clang_type);
    }
    ROSE_ASSERT(!name_str.empty());
    return build_with_qualifier(nullptr, name_str, nullptr);
  }

  if (const clang::TagType *tag = llvm::dyn_cast<clang::TagType>(clang_type)) {
    std::string name_str = tag->getDecl()->getNameAsString();
    if (name_str.empty()) {
      name_str = fallback_type_name(clang_type);
    }
    ROSE_ASSERT(!name_str.empty());
    clang::NestedNameSpecifier *qualifier = nullptr;
    if (!prefer_current_scope && p_compiler_instance != nullptr) {
      if (!scopeIsWithinNamespaceChain(scope,
                                       tag->getDecl()->getDeclContext())) {
        qualifier = buildNamespaceQualifierForDeclContext(
            tag->getDecl()->getDeclContext(),
            p_compiler_instance->getASTContext());
      }
    }
    SgNonrealType *nrtype = build_with_qualifier(qualifier, name_str, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (clang::TagDecl *tag_decl = tag->getDecl()) {
        if (SgDeclarationStatement *sg_decl =
                lookupSgDeclarationForClangDecl(tag_decl,
                                                /*allow_on_demand=*/true)) {
          nrdecl->set_templateDeclaration(sg_decl);
        }
      }
    }
    return nrtype;
  }

  if (const clang::InjectedClassNameType *inj =
          llvm::dyn_cast<clang::InjectedClassNameType>(clang_type)) {
    // Preserve injected-template arguments (e.g., Outer<T>) when this type
    // appears in nested-name specifiers. Falling back to the plain record name
    // drops '<T>' and corrupts dependent qualification.
    clang::QualType injected_qt = inj->getInjectedSpecializationType();
    const clang::Type *injected_ty = injected_qt.getTypePtrOrNull();
    if (injected_ty != nullptr && injected_ty != clang_type) {
      if (SgNonrealType *nrtype = buildNonrealTypeForNestedNameSpecifierType(
              injected_ty, scope, prefer_current_scope)) {
        return nrtype;
      }
    }

    std::string name_str = inj->getDecl()->getNameAsString();
    if (name_str.empty()) {
      name_str = fallback_type_name(clang_type);
    }
    ROSE_ASSERT(!name_str.empty());
    SgNonrealType *nrtype = build_with_qualifier(nullptr, name_str, nullptr);
    if (SgNonrealDecl *nrdecl =
            isSgNonrealDecl(nrtype ? nrtype->get_declaration() : nullptr)) {
      if (clang::CXXRecordDecl *decl = inj->getDecl()) {
        if (SgDeclarationStatement *sg_decl =
                lookupSgDeclarationForClangDecl(decl,
                                                /*allow_on_demand=*/true)) {
          nrdecl->set_templateDeclaration(sg_decl);
        }
      }
    }
    return nrtype;
  }

  std::string name_str;
  if (const clang::TypeDecl *decl = clang_type->getAsTagDecl()) {
    name_str = decl->getNameAsString();
  }
  if (name_str.empty()) {
    name_str = fallback_type_name(clang_type);
  }
  ROSE_ASSERT(!name_str.empty());
  return build_with_qualifier(nullptr, name_str, nullptr);
}

SgNonrealType *ClangToSageTranslator::buildNonrealTypeFromNestedNameSpecifier(
    clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope,
    const SgName &terminalName,
    const SgTemplateArgumentPtrList *terminalTemplateArgs) {
  SgScopeStatement *effective_scope = scope;
  if (effective_scope == nullptr) {
    effective_scope = SageBuilder::topScopeStack();
  }
  bool has_global_qualifier = nestedNameSpecifierHasGlobal(qualifier);
  if (has_global_qualifier ||
      nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    effective_scope = getGlobalScope();
  }
  ROSE_ASSERT(effective_scope != nullptr);

  std::function<SgScopeStatement *(clang::NestedNameSpecifier *,
                                   SgScopeStatement *)>
      build_chain;
  build_chain = [&](clang::NestedNameSpecifier *nns,
                    SgScopeStatement *current_scope) -> SgScopeStatement * {
    if (nns == nullptr) {
      return current_scope;
    }

    current_scope = build_chain(nns->getPrefix(), current_scope);

    SgNonrealType *segment_type = nullptr;
    switch (nns->getKind()) {
    case clang::NestedNameSpecifier::Identifier: {
      const clang::IdentifierInfo *id = nns->getAsIdentifier();
      std::string name_str = id ? id->getName().str() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }

    case clang::NestedNameSpecifier::Namespace: {
      clang::NamespaceDecl *ns = nns->getAsNamespace();
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }

    case clang::NestedNameSpecifier::NamespaceAlias: {
      clang::NamespaceAliasDecl *ns = nns->getAsNamespaceAlias();
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }

    case clang::NestedNameSpecifier::TypeSpec:
#if LLVM_VERSION_MAJOR < 21
    case clang::NestedNameSpecifier::TypeSpecWithTemplate:
#endif
    {
      bool prefer_current = (nns->getPrefix() != nullptr);
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          nns->getAsType(), current_scope, prefer_current);
      break;
    }

    case clang::NestedNameSpecifier::Global:
      break;

    case clang::NestedNameSpecifier::Super: {
      clang::CXXRecordDecl *record = nns->getAsRecordDecl();
      std::string name_str = record ? record->getNameAsString() : "";
      if (name_str.empty()) {
        name_str = "__super";
      }
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    }

    if (segment_type != nullptr) {
      SgNonrealDecl *segment_decl =
          isSgNonrealDecl(segment_type->get_declaration());
      ROSE_ASSERT(segment_decl != nullptr);
      if (nestedNameSpecifierHasTemplateKeyword(nns)) {
        segment_decl->set_has_template_keyword(true);
      }
      if (clang::NestedNameSpecifier *prefix = nns->getPrefix()) {
        if (prefix->getKind() == clang::NestedNameSpecifier::Global) {
          segment_decl->set_has_global_qualifier(true);
        }
      }
      current_scope = segment_decl->get_nonreal_decl_scope();
    }

    return current_scope;
  };

  SgScopeStatement *chain_scope = build_chain(qualifier, effective_scope);
  ROSE_ASSERT(chain_scope != nullptr);

  auto qualifier_requires_typename = [](clang::NestedNameSpecifier *nns) {
    for (clang::NestedNameSpecifier *it = nns; it != nullptr;
         it = it->getPrefix()) {
      switch (it->getKind()) {
      case clang::NestedNameSpecifier::Global:
      case clang::NestedNameSpecifier::Namespace:
      case clang::NestedNameSpecifier::NamespaceAlias:
        continue;
      case clang::NestedNameSpecifier::Identifier:
      case clang::NestedNameSpecifier::TypeSpec:
#if LLVM_VERSION_MAJOR < 21
      case clang::NestedNameSpecifier::TypeSpecWithTemplate:
#endif
      case clang::NestedNameSpecifier::Super:
        return true;
      }
    }
    return false;
  };

  SgNonrealType *nrtype = SageBuilder::buildNonrealType(
      terminalName, chain_scope, terminalTemplateArgs);
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);
  if (qualifier != nullptr && !qualifier_requires_typename(qualifier)) {
    nrdecl->set_suppress_typename(true);
  }

  // Preserve a leading global qualifier for terminal references so namespace
  // lookup does not accidentally bind to a local shadow.
  if (qualifier != nullptr && nestedNameSpecifierHasGlobal(qualifier)) {
    nrdecl->set_has_global_qualifier(true);
  }

  return nrtype;
}

SgScopeStatement *
ClangToSageTranslator::buildNonrealScopeFromNestedNameSpecifier(
    clang::NestedNameSpecifier *qualifier, SgScopeStatement *scope) {
  if (qualifier == nullptr) {
    return scope;
  }

  std::vector<clang::NestedNameSpecifier *> segments;
  for (clang::NestedNameSpecifier *nns = qualifier; nns != nullptr;
       nns = nns->getPrefix()) {
    segments.push_back(nns);
  }

  SgScopeStatement *current_scope = scope;
  if (nestedNameSpecifierHasNamespaceQualifier(qualifier)) {
    current_scope = getGlobalScope();
  }
  for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
    clang::NestedNameSpecifier *nns = *it;
    if (nns->getKind() == clang::NestedNameSpecifier::Global) {
      current_scope = getGlobalScope();
      continue;
    }

    SgNonrealType *segment_type = nullptr;
    switch (nns->getKind()) {
    case clang::NestedNameSpecifier::Identifier: {
      const clang::IdentifierInfo *id = nns->getAsIdentifier();
      std::string name_str = id ? id->getName().str() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Namespace: {
      clang::NamespaceDecl *ns = nns->getAsNamespace();
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::NamespaceAlias: {
      clang::NamespaceAliasDecl *ns = nns->getAsNamespaceAlias();
      std::string name_str = ns ? ns->getNameAsString() : "";
      ROSE_ASSERT(!name_str.empty());
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::TypeSpec:
#if LLVM_VERSION_MAJOR < 21
    case clang::NestedNameSpecifier::TypeSpecWithTemplate:
#endif
    {
      bool prefer_current = (nns->getPrefix() != nullptr);
      segment_type = buildNonrealTypeForNestedNameSpecifierType(
          nns->getAsType(), current_scope, prefer_current);
      break;
    }
    case clang::NestedNameSpecifier::Super: {
      clang::CXXRecordDecl *record = nns->getAsRecordDecl();
      std::string name_str = record ? record->getNameAsString() : "";
      if (name_str.empty()) {
        name_str = "__super";
      }
      segment_type = SageBuilder::buildNonrealType(SgName(name_str),
                                                   current_scope, nullptr);
      break;
    }
    case clang::NestedNameSpecifier::Global:
      break;
    }

    if (segment_type != nullptr) {
      SgNonrealDecl *segment_decl =
          isSgNonrealDecl(segment_type->get_declaration());
      ROSE_ASSERT(segment_decl != nullptr);
      if (nestedNameSpecifierHasTemplateKeyword(nns)) {
        segment_decl->set_has_template_keyword(true);
      }
      if (clang::NestedNameSpecifier *prefix = nns->getPrefix()) {
        if (prefix->getKind() == clang::NestedNameSpecifier::Global) {
          segment_decl->set_has_global_qualifier(true);
        }
      }
      current_scope = segment_decl->get_nonreal_decl_scope();
    }
  }

  return current_scope;
}

SgTemplateInstantiationDecl *
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration *template_decl,
    const clang::TemplateSpecializationType *clang_type) {

  // Extract both base name and qualified name for the template
  std::string template_base_name = template_decl->get_name().getString();

  // ROOT CAUSE FIX: Check if template declaration has namespace qualification
  // stored Use qualified name (e.g., "std::array") for instantiation name
  // instead of just base name
  std::string template_qualified_name = getTemplateQualifiedName(template_decl);

  // Use qualified name in cache key to avoid namespace collisions
  // Example: "std::array<int>" and "my_ns::array<int>" must have different
  // cache keys Otherwise they would both mangle to "array_int" and collide
  std::string inst_name_full =
      mangleTemplateInstantiation(template_qualified_name, clang_type);
  std::string inst_display_name = buildTemplateInstantiationName(
      template_base_name, clang_type->template_arguments());
  auto ensure_file_info = [this](SgTemplateInstantiationDecl *decl) {
    if (decl == nullptr) {
      return;
    }
    Sg_File_Info *start = decl->get_startOfConstruct();
    Sg_File_Info *end = decl->get_endOfConstruct();
    if (start == nullptr && end == nullptr) {
      setCompilerGeneratedFileInfo(decl);
      return;
    }
    if (start != nullptr && end == nullptr) {
      Sg_File_Info *end_copy = new Sg_File_Info(*start);
      decl->set_endOfConstruct(end_copy);
      end_copy->set_parent(decl);
    } else if (start == nullptr && end != nullptr) {
      Sg_File_Info *start_copy = new Sg_File_Info(*end);
      decl->set_startOfConstruct(start_copy);
      start_copy->set_parent(decl);
    }
  };
  auto suppress_unparse = [](SgLocatedNode *node) {
    if (node == nullptr) {
      return;
    }
    auto mark = [](Sg_File_Info *fi) {
      if (fi == nullptr) {
        return;
      }
      fi->setCompilerGenerated();
      fi->unsetOutputInCodeGeneration();
    };
    mark(node->get_file_info());
    mark(node->get_startOfConstruct());
    mark(node->get_endOfConstruct());
    if (SgExpression *expr = isSgExpression(node)) {
      mark(expr->get_operatorPosition());
    }
  };

  // Check cache
  auto it = p_template_inst_cache.find(inst_name_full);
  if (it != p_template_inst_cache.end()) {
    SgTemplateInstantiationDecl *inst_decl = it->second;
    if (inst_decl != nullptr) {
      if (inst_decl->get_templateArguments().empty()) {
        inst_decl->get_templateArguments() = buildTemplateArguments(clang_type);
      }
      if (inst_decl->get_deducedTemplateArguments().empty()) {
        inst_decl->get_deducedTemplateArguments() =
            buildTemplateArguments(clang_type);
      }
      if (inst_decl->get_specializedTemplateDeclaration() == nullptr) {
        inst_decl->set_specializedTemplateDeclaration(template_decl);
      }
      SageBuilder::setTemplateArgumentParents(inst_decl);
      if (!inst_decl->get_nameResetFromMangledForm()) {
        inst_decl->set_nameResetFromMangledForm(true);
      }
    }
    ensure_file_info(inst_decl);
    suppress_unparse(inst_decl);
    if (SgClassDefinition *defn = inst_decl->get_definition()) {
      suppress_unparse(defn);
    }
    return inst_decl;
  }

  // Build template arguments
  SgTemplateArgumentPtrList args = buildTemplateArguments(clang_type);
  SgTemplateArgumentPtrList deduced_args = buildTemplateArguments(clang_type);

  // Create class type first (will be set on instantiation)
  SgClassType *class_type = nullptr;

  // Create template instantiation declaration with all parameters
  // ROOT CAUSE FIX: Use qualified name (e.g., "std::array") not just base name
  // ("array") This ensures the unparser outputs the correct namespace
  // qualification
  SgTemplateInstantiationDecl *inst_decl = new SgTemplateInstantiationDecl(
      SgName(inst_display_name), SgClassDeclaration::e_class,
      class_type, // type (initially nullptr, will be set)
      nullptr,    // definition
      template_decl, args);
  inst_decl->get_deducedTemplateArguments() = deduced_args;
  inst_decl->set_specializedTemplateDeclaration(template_decl);

  inst_decl->get_templateArguments() = args;

  // Set file info and mark as compiler generated
  // Create synthetic file info since this is a compiler-generated node
  Sg_File_Info *file_info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  inst_decl->set_file_info(file_info);
  setCompilerGeneratedFileInfo(inst_decl);
  ensure_file_info(inst_decl);
  suppress_unparse(inst_decl);
  inst_decl->setForward();
  inst_decl->set_definingDeclaration(nullptr);
  inst_decl->set_firstNondefiningDeclaration(inst_decl);
  SageBuilder::setTemplateArgumentParents(inst_decl);

  if (inst_decl->get_templateDeclaration() == NULL) {
    std::cerr << "CRITICAL ERROR: inst_decl->get_templateDeclaration() is NULL "
                 "immediately after creation! Setting it explicitly."
              << std::endl;
    inst_decl->set_templateDeclaration(template_decl);
  }

  // Use the template declaration scope as the instantiation scope so we
  // stay consistent with later declaration-based instantiation handling.
  SgScopeStatement *inst_scope = template_decl->get_scope();
  if (clang_type != nullptr) {
    clang::TemplateName clang_tname = clang_type->getTemplateName();
    if (clang::TemplateDecl *clang_template_decl =
            clang_tname.getAsTemplateDecl()) {
      if (SgScopeStatement *context_scope = resolveScopeFromDeclContext(
              clang_template_decl->getDeclContext(), nullptr)) {
        inst_scope = context_scope;
      }
    } else {
      clang::NestedNameSpecifier *qualifier = nullptr;
      if (const clang::QualifiedTemplateName *qtn =
              clang_tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     clang_tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
      }
      if (qualifier != nullptr) {
        SgScopeStatement *base_scope = SageBuilder::topScopeStack();
        if (nestedNameSpecifierHasGlobal(qualifier)) {
          base_scope = getGlobalScope();
        }
        if (SgScopeStatement *context_scope =
                buildNonrealScopeFromNestedNameSpecifier(qualifier,
                                                         base_scope)) {
          inst_scope = context_scope;
        }
      }
    }
  }
  if (inst_scope == nullptr) {
    inst_scope = getGlobalScope();
  }

  inst_decl->set_scope(inst_scope);
  inst_decl->set_parent(inst_scope);

  // CRITICAL: Set template name before creating type
  // get_mangled_name() requires this to be set and will assert if it's null
  // Use ONLY base name for templateName (e.g., "array" not "std::array")
  // The qualified name is in the declaration name above
  // NameQualificationTraversal will add the necessary qualification (e.g.
  // "std::")
  inst_decl->set_templateName(SgName(template_base_name));
  inst_decl->set_nameResetFromMangledForm(true);

  // Create class type pointing to this instantiation
  class_type = SgClassType::createType(inst_decl);
  inst_decl->set_type(class_type);

  // Insert symbol via the unified registration path to avoid duplicates.
  registerDeclarationSymbol(inst_decl);

  // Cache it with full name
  p_template_inst_cache[inst_name_full] = inst_decl;

  return inst_decl;
}

bool ClangToSageTranslator::VisitTemplateSpecializationType(
    clang::TemplateSpecializationType *template_specialization_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TemplateSpecializationType" << std::endl;
#endif

  // Don't desugar or use canonical type for template specializations
  // We want to create proper SgTemplateInstantiationDecl nodes with template
  // arguments Desugaring would lose the template argument information

  if (template_specialization_type->isTypeAlias()) {
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    auto resolve_template_decl =
        [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
      clang::TemplateName current = name;
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return decl;
        }
        if (const clang::QualifiedTemplateName *qtn =
                current.getAsQualifiedTemplateName()) {
          clang::TemplateName underlying = qtn->getUnderlyingTemplate();
          if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
            return decl;
          }
          current = underlying;
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *subst =
                current.getAsSubstTemplateTemplateParm()) {
          current = subst->getReplacement();
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          return llvm::dyn_cast_or_null<clang::TemplateDecl>(
              using_shadow->getTargetDecl());
        }
        return nullptr;
      }
    };
    clang::TemplateDecl *clang_template_decl = resolve_template_decl(tname);
    clang::TypeAliasTemplateDecl *alias_decl =
        llvm::dyn_cast_or_null<clang::TypeAliasTemplateDecl>(
            clang_template_decl);

    SgTemplateTypedefDeclaration *alias_sg_decl = nullptr;
    if (alias_decl != nullptr) {
      if (SgDeclarationStatement *found_decl =
              lookupSgDeclarationForClangDecl(alias_decl,
                                              /*allow_on_demand=*/true)) {
        alias_sg_decl = isSgTemplateTypedefDeclaration(found_decl);
      }
    }
    SgTemplateArgumentPtrList template_args =
        buildTemplateArguments(template_specialization_type);
    SgTemplateArgumentPtrList deduced_args =
        buildTemplateArguments(template_specialization_type);
    if (alias_decl != nullptr) {
      size_t param_count = 0;
      bool has_parameter_pack = false;
      if (clang::TemplateParameterList *params =
              alias_decl->getTemplateParameters()) {
        param_count = params->size();
        for (unsigned i = 0; i < params->size(); ++i) {
          clang::NamedDecl *param = params->getParam(i);
          if ((llvm::dyn_cast_or_null<clang::TemplateTypeParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::TemplateTypeParmDecl>(param)
                   ->isParameterPack()) ||
              (llvm::dyn_cast_or_null<clang::NonTypeTemplateParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::NonTypeTemplateParmDecl>(param)
                   ->isParameterPack()) ||
              (llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(param) !=
                   nullptr &&
               llvm::cast<clang::TemplateTemplateParmDecl>(param)
                   ->isParameterPack())) {
            has_parameter_pack = true;
            break;
          }
        }
      }
      // Alias templates with parameter packs may legitimately carry more
      // expanded arguments than the raw parameter count.
      if (param_count > 0 && !has_parameter_pack) {
        if (template_args.size() > param_count) {
          template_args.resize(param_count);
        }
        if (deduced_args.size() > param_count) {
          deduced_args.resize(param_count);
        }
      }
    }

    if (template_specialization_type->isDependentType()) {
      clang::TemplateName tname =
          template_specialization_type->getTemplateName();
      clang::NestedNameSpecifier *qualifier = nullptr;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
      }

      std::string alias_name =
          alias_decl != nullptr ? alias_decl->getNameAsString() : std::string();
      if (alias_name.empty()) {
        alias_name = getTemplateNameBase(tname);
      }
      if (alias_name.empty()) {
        alias_name = "__alias_template";
      }

      SgScopeStatement *base_scope = SageBuilder::topScopeStack();
      if (base_scope == nullptr) {
        base_scope = getGlobalScope();
      }

      SgType *dep_alias_type = nullptr;
      if (qualifier != nullptr) {
        dep_alias_type = buildNonrealTypeFromNestedNameSpecifier(
            qualifier, base_scope, SgName(alias_name),
            template_args.empty() ? nullptr : &template_args);
      } else {
        dep_alias_type = SageBuilder::buildNonrealType(
            SgName(alias_name), base_scope,
            template_args.empty() ? nullptr : &template_args);
      }

      if (dep_alias_type != nullptr) {
        *node = dep_alias_type;
        return VisitType(template_specialization_type, node);
      }
    }

    SgScopeStatement *alias_type_scope = nullptr;
    if (alias_sg_decl != nullptr) {
      alias_type_scope = alias_sg_decl->get_scope();
    }
    if (alias_type_scope == nullptr && alias_decl != nullptr) {
      alias_type_scope = resolveScopeFromDeclContext(
          alias_decl->getDeclContext(), SageBuilder::topScopeStack());
    }

    auto build_alias_type_in_scope = [&](clang::QualType qt) -> SgType * {
      if (qt.isNull()) {
        return nullptr;
      }
      if (alias_type_scope == nullptr ||
          alias_type_scope == SageBuilder::topScopeStack()) {
        return buildTypeFromQualifiedType(qt);
      }

      SageBuilder::pushScopeStack(alias_type_scope);
      SgType *type_in_alias_scope = buildTypeFromQualifiedType(qt);
      SageBuilder::popScopeStack();
      return type_in_alias_scope;
    };

    SgType *aliased_type = build_alias_type_in_scope(
        template_specialization_type->getAliasedType());
    if (isSgNonrealType(aliased_type) != nullptr &&
        !template_specialization_type->isDependentType()) {
      clang::QualType canonical_qt =
          template_specialization_type->getCanonicalTypeInternal();
      if (!canonical_qt.isNull()) {
        if (SgType *canonical_type = build_alias_type_in_scope(canonical_qt)) {
          aliased_type = canonical_type;
        }
      }
    }

    auto has_pack_marker = [](const SgTemplateArgumentPtrList &args) -> bool {
      for (SgTemplateArgument *arg : args) {
        if (arg != nullptr &&
            arg->get_argumentType() ==
                SgTemplateArgument::start_of_pack_expansion_argument) {
          return true;
        }
      }
      return false;
    };
    const bool dependent_pack =
        template_specialization_type->containsUnexpandedParameterPack() ||
        template_specialization_type->isDependentType();
    const bool needs_alias_fallback =
        (template_args.empty() || has_pack_marker(template_args)) &&
        dependent_pack;

    if (needs_alias_fallback && aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
    }

    bool alias_in_record_context = false;
    if (alias_decl != nullptr) {
      clang::DeclContext *alias_context = alias_decl->getDeclContext();
      while (alias_context != nullptr &&
             llvm::isa<clang::LinkageSpecDecl>(alias_context)) {
        alias_context = alias_context->getParent();
      }
      alias_in_record_context =
          alias_context != nullptr && alias_context->isRecord();
    }
    if (alias_in_record_context) {
      // Preserve class-scope alias template spelling (e.g.
      // `Outer<T>::template Alias<U>`) rather than eagerly materializing the
      // resolved aliased type, which can lose qualification outside the record
      // context.
      clang::NestedNameSpecifier *alias_qualifier = nullptr;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        alias_qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        alias_qualifier = dtn->getQualifier();
      }

      std::string alias_name =
          alias_decl != nullptr ? alias_decl->getNameAsString() : std::string();
      if (alias_name.empty()) {
        alias_name = getTemplateNameBase(tname);
      }
      if (alias_name.empty()) {
        alias_name = "__alias_template";
      }

      SgScopeStatement *base_scope = SageBuilder::topScopeStack();
      if (base_scope == nullptr) {
        base_scope = getGlobalScope();
      }

      SgType *record_alias_type = nullptr;
      if (alias_qualifier != nullptr) {
        if (nestedNameSpecifierHasGlobal(alias_qualifier)) {
          base_scope = getGlobalScope();
        }
        record_alias_type = buildNonrealTypeFromNestedNameSpecifier(
            alias_qualifier, base_scope, SgName(alias_name),
            template_args.empty() ? nullptr : &template_args);
      } else {
        record_alias_type = SageBuilder::buildNonrealType(
            SgName(alias_name), base_scope,
            template_args.empty() ? nullptr : &template_args);
      }

      if (record_alias_type != nullptr) {
        *node = record_alias_type;
        return VisitType(template_specialization_type, node);
      }

      clang::QualType canonical_qt =
          template_specialization_type->getCanonicalTypeInternal();
      if (!canonical_qt.isNull()) {
        if (SgType *canonical_type = build_alias_type_in_scope(canonical_qt)) {
          *node = canonical_type;
          return VisitType(template_specialization_type, node);
        }
      }
      if (aliased_type != nullptr) {
        *node = aliased_type;
        return VisitType(template_specialization_type, node);
      }
    }

    SgScopeStatement *scope = nullptr;
    if (alias_sg_decl != nullptr) {
      scope = alias_sg_decl->get_scope();
    }
    if (scope == nullptr && alias_decl != nullptr) {
      scope = resolveScopeFromDeclContext(alias_decl->getDeclContext(),
                                          SageBuilder::topScopeStack());
    }
    if (scope == nullptr) {
      scope = SageBuilder::topScopeStack();
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (scope == nullptr) {
      scope = getGlobalScope();
    }

    auto scope_reachable_from_current_file =
        [&](SgScopeStatement *candidate) -> bool {
      if (candidate == nullptr || p_sage_source_file == nullptr) {
        return false;
      }
      SgGlobal *file_global = p_sage_source_file->get_globalScope();
      if (file_global == nullptr) {
        return false;
      }

      auto attached_to_parent = [](SgNode *child, SgNode *parent) -> bool {
        if (child == nullptr || parent == nullptr) {
          return false;
        }

        if (SgScopeStatement *parent_scope = isSgScopeStatement(parent)) {
          if (SgStatement *stmt = isSgStatement(child)) {
            return parent_scope->statementExistsInScope(stmt);
          }
        }

        if (SgNamespaceDeclarationStatement *ns_decl =
                isSgNamespaceDeclarationStatement(parent)) {
          return ns_decl->get_definition() == child;
        }
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(parent)) {
          return class_decl->get_definition() == child;
        }
        if (SgFunctionDeclaration *fn_decl = isSgFunctionDeclaration(parent)) {
          return fn_decl->get_definition() == child;
        }

        return true;
      };

      for (SgNode *cursor = candidate; cursor != nullptr;
           cursor = cursor->get_parent()) {
        if (cursor == file_global || cursor == p_sage_source_file) {
          return true;
        }

        SgNode *parent = cursor->get_parent();
        if (parent == nullptr) {
          return false;
        }
        if (!attached_to_parent(cursor, parent)) {
          return false;
        }
      }
      return false;
    };

    auto resolve_reachable_namespace_scope =
        [&](clang::DeclContext *decl_context) -> SgScopeStatement * {
      if (decl_context == nullptr) {
        return nullptr;
      }
      SgScopeStatement *reachable_scope = getGlobalScope();
      if (reachable_scope == nullptr && p_sage_source_file != nullptr) {
        reachable_scope = p_sage_source_file->get_globalScope();
      }
      if (reachable_scope == nullptr) {
        return nullptr;
      }

      std::vector<const clang::NamespaceDecl *> namespaces =
          collectNamespaceContexts(decl_context);
      for (const clang::NamespaceDecl *ns_decl : namespaces) {
        if (ns_decl == nullptr || ns_decl->isAnonymousNamespace()) {
          continue;
        }

        SgName ns_name(ns_decl->getNameAsString());
        SgNamespaceDeclarationStatement *ns_stmt = nullptr;
        if (SgNamespaceSymbol *ns_symbol =
                reachable_scope->lookup_namespace_symbol(ns_name)) {
          ns_stmt = ns_symbol->get_declaration();
        }

        if (ns_stmt == nullptr || ns_stmt->get_definition() == nullptr ||
            !scope_reachable_from_current_file(ns_stmt->get_definition())) {
          // Materialize/reopen a namespace in the reachable scope rather than
          // reusing detached canonical namespace nodes from system-header
          // contexts.
          ns_stmt = SageBuilder::buildNamespaceDeclaration_nfi(ns_name, false,
                                                               reachable_scope);
        }

        if (ns_stmt == nullptr || ns_stmt->get_definition() == nullptr) {
          return nullptr;
        }

        if (ns_stmt->get_parent() != reachable_scope) {
          ensureDeclInScopeChildList(
              ns_stmt, reachable_scope,
              "VisitTemplateSpecializationType:reachable-namespace");
        }

        reachable_scope = ns_stmt->get_definition();
      }

      return reachable_scope;
    };

    if (!scope_reachable_from_current_file(scope) && alias_decl != nullptr) {
      if (SgScopeStatement *reachable_scope =
              resolve_reachable_namespace_scope(alias_decl->getDeclContext())) {
        scope = reachable_scope;
      }
    }

    if (!scope_reachable_from_current_file(scope)) {
      if (SgScopeStatement *global_scope = getGlobalScope()) {
        scope = global_scope;
      } else if (p_sage_source_file != nullptr &&
                 p_sage_source_file->get_globalScope() != nullptr) {
        scope = p_sage_source_file->get_globalScope();
      }
    }

    if (alias_sg_decl != nullptr && scope != nullptr &&
        !scope_reachable_from_current_file(alias_sg_decl->get_scope())) {
      SgName reachable_alias_name = alias_sg_decl->get_name();
      if (alias_decl != nullptr) {
        reachable_alias_name = SgName(alias_decl->getNameAsString());
      }
      if (SgTemplateTypedefSymbol *reachable_alias_sym =
              scope->lookup_template_typedef_symbol(reachable_alias_name)) {
        if (SgTemplateTypedefDeclaration *reachable_alias_decl =
                isSgTemplateTypedefDeclaration(
                    reachable_alias_sym->get_declaration())) {
          alias_sg_decl = reachable_alias_decl;
        }
      }
    }

    if (alias_sg_decl != nullptr && aliased_type != nullptr &&
        scope != nullptr) {
      SgName alias_name = alias_sg_decl->get_name();
      if (alias_decl != nullptr) {
        alias_name = SgName(alias_decl->getNameAsString());
      }
      SgName alias_name_with_args =
          SageBuilder::appendTemplateArgumentsToName(alias_name, template_args);
      if (SgTemplateTypedefSymbol *existing_symbol =
              scope->lookup_template_typedef_symbol(alias_name_with_args)) {
        if (isSgTemplateInstantiationTypedefDeclaration(
                existing_symbol->get_declaration()) == nullptr) {
          SgScopeStatement *base_scope = scope;
          if (base_scope == nullptr) {
            base_scope = SageBuilder::topScopeStack();
          }
          if (base_scope == nullptr) {
            base_scope = getGlobalScope();
          }
          *node = SageBuilder::buildNonrealType(
              alias_name, base_scope,
              template_args.empty() ? nullptr : &template_args);
          return VisitType(template_specialization_type, node);
        }
      }
      SgTemplateInstantiationTypedefDeclaration *inst_decl =
          SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
              alias_name, aliased_type, scope, /*has_defining_base=*/false,
              alias_sg_decl, template_args);
      if (inst_decl != nullptr) {
        inst_decl->get_templateArguments() = template_args;
        inst_decl->get_deducedTemplateArguments() = deduced_args;
        inst_decl->set_nameResetFromMangledForm(true);
        inst_decl->unsetCompilerGenerated();
        if (Sg_File_Info *fi = inst_decl->get_file_info()) {
          fi->unsetOutputInCodeGeneration();
        }
        SageBuilder::setTemplateArgumentParents(inst_decl);
        if (inst_decl->get_specializedTemplateDeclaration() == nullptr) {
          inst_decl->set_specializedTemplateDeclaration(alias_sg_decl);
        }
        registerDeclarationSymbol(inst_decl);
        // Keep alias-template instantiations attached to their semantic
        // scope. If the original scope belongs to a detached header fragment,
        // normalize to an equivalent scope reachable from the current source
        // file before attachment.
        ensureDeclInScopeChildList(inst_decl, scope,
                                   "VisitTemplateSpecializationType");
        *node = inst_decl->get_type();
        return VisitType(template_specialization_type, node);
      }
    }

    if (aliased_type != nullptr) {
      *node = aliased_type;
      return VisitType(template_specialization_type, node);
    }

    // Fallback: preserve alias spelling as a nonreal type.
    std::string alias_name =
        alias_decl != nullptr ? alias_decl->getNameAsString() : std::string();
    if (alias_name.empty()) {
      alias_name = getTemplateNameBase(tname);
    }
    if (alias_name.empty()) {
      alias_name = "__alias_template";
    }

    clang::NestedNameSpecifier *alias_qualifier = nullptr;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      alias_qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      alias_qualifier = dtn->getQualifier();
    }

    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    if (base_scope == nullptr) {
      base_scope = getGlobalScope();
    }

    if (alias_qualifier != nullptr) {
      if (nestedNameSpecifierHasGlobal(alias_qualifier)) {
        base_scope = getGlobalScope();
      }
      if (SgType *qualified_alias = buildNonrealTypeFromNestedNameSpecifier(
              alias_qualifier, base_scope, SgName(alias_name),
              template_args.empty() ? nullptr : &template_args)) {
        *node = qualified_alias;
        return VisitType(template_specialization_type, node);
      }
    }

    *node = SageBuilder::buildNonrealType(
        SgName(alias_name), base_scope,
        template_args.empty() ? nullptr : &template_args);
    return VisitType(template_specialization_type, node);
  }

  if (template_specialization_type->isDependentType()) {
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    std::string base_name = getTemplateNameBase(tname);
    ROSE_ASSERT(!base_name.empty());

    SgTemplateArgumentPtrList tpl_args =
        buildTemplateArguments(template_specialization_type);

    SgScopeStatement *base_scope = SageBuilder::topScopeStack();
    ROSE_ASSERT(base_scope != nullptr);

    clang::NestedNameSpecifier *qualifier = nullptr;
    if (const clang::QualifiedTemplateName *qtn =
            tname.getAsQualifiedTemplateName()) {
      qualifier = qtn->getQualifier();
    } else if (const clang::DependentTemplateName *dtn =
                   tname.getAsDependentTemplateName()) {
      qualifier = dtn->getQualifier();
    }

    if (qualifier != nullptr) {
      *node = buildNonrealTypeFromNestedNameSpecifier(
          qualifier, base_scope, SgName(base_name), &tpl_args);
      return VisitType(template_specialization_type, node);
    }

    // For dependent names without an explicit qualifier, preserve the spelling
    // as-written. Synthesizing declaration-context qualifiers here loses
    // dependent template arguments (e.g., `Outer<T>::Inner<U>` becoming
    // `Outer::Inner<U>`), which breaks correctness.
    *node =
        SageBuilder::buildNonrealType(SgName(base_name), base_scope, &tpl_args);
    return VisitType(template_specialization_type, node);
  }

  // Extract template name
  clang::TemplateName tname = template_specialization_type->getTemplateName();
  std::string template_name = mangleTemplateName(tname);

  // Get or create template class declaration
  SgTemplateClassDeclaration *template_decl = NULL;

  clang::TemplateDecl *clang_template_decl = tname.getAsTemplateDecl();
  if (clang_template_decl) {
    SgNode *tmp_node = TraverseOnDemand(clang_template_decl);
    template_decl = isSgTemplateClassDeclaration(tmp_node);
  }

  if (template_decl == NULL) {
    SgScopeStatement *template_scope = nullptr;
    if (clang_template_decl != nullptr) {
      clang::DeclContext *decl_context = clang_template_decl->getDeclContext();
      template_scope = resolveScopeFromDeclContext(
          decl_context, SageBuilder::topScopeStack());
    }

    if (template_scope == nullptr) {
      clang::NestedNameSpecifier *qualifier = nullptr;
      if (const clang::QualifiedTemplateName *qtn =
              tname.getAsQualifiedTemplateName()) {
        qualifier = qtn->getQualifier();
      } else if (const clang::DependentTemplateName *dtn =
                     tname.getAsDependentTemplateName()) {
        qualifier = dtn->getQualifier();
      }

      if (qualifier != nullptr) {
        SgScopeStatement *base_scope = SageBuilder::topScopeStack();
        if (nestedNameSpecifierHasGlobal(qualifier)) {
          base_scope = getGlobalScope();
        }
        template_scope =
            buildNonrealScopeFromNestedNameSpecifier(qualifier, base_scope);
      }
    }

    template_decl = getOrCreateTemplateDeclaration(
        template_name, template_specialization_type, template_scope);
  }

  if (template_decl == NULL) {
    std::cerr << "CRITICAL ERROR: template_decl is NULL for " << template_name
              << std::endl;
  }

  // Get or create template instantiation
  SgTemplateInstantiationDecl *inst_decl = getOrCreateTemplateInstantiation(
      template_decl, template_specialization_type);

  // Return the class type
  *node = inst_decl->get_type();
  ROSE_ASSERT(*node != nullptr);

  return VisitType(template_specialization_type, node);
}

bool ClangToSageTranslator::VisitTemplateTypeParmType(
    clang::TemplateTypeParmType *template_type_parm_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTemplateTypeParmType" << std::endl;
#endif
  bool res = true;

  // CLANG FRONTEND FIX: Proper template type parameter support
  // Get the template parameter declaration to extract the name
  const clang::TemplateTypeParmDecl *param_decl =
      template_type_parm_type->getDecl();
  std::string param_name;

  if (param_decl && param_decl->getDeclName().isIdentifier()) {
    // Use the actual template parameter name (e.g., "T")
    param_name = param_decl->getNameAsString();
  } else {
    // Fallback to a generic name if we can't get the actual name
    param_name = "template_type_param";
  }

  // Create a proper template type with the actual parameter name
  *node = SageBuilder::buildTemplateType(SgName(param_name));

  return VisitType(template_type_parm_type, node) && res;
}

bool ClangToSageTranslator::VisitTypedefType(clang::TypedefType *typedef_type,
                                             SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTypedefType" << std::endl;
#endif

  bool res = true;

  auto build_specialized_member_typedef =
      [&](const clang::TypedefNameDecl *typedef_decl) -> SgType * {
    if (typedef_decl == nullptr) {
      return nullptr;
    }
    const clang::DeclContext *decl_context = typedef_decl->getDeclContext();
    const clang::CXXRecordDecl *record_decl =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl_context);
    const clang::ClassTemplateSpecializationDecl *spec_decl =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            record_decl);
    if (spec_decl == nullptr || record_decl == nullptr) {
      return nullptr;
    }

    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    if (scope == nullptr) {
      return nullptr;
    }

    std::vector<const clang::DeclContext *> contexts;
    for (const clang::DeclContext *dc = record_decl->getDeclContext();
         dc != nullptr && !dc->isTranslationUnit(); dc = dc->getParent()) {
      contexts.push_back(dc);
    }
    for (auto it = contexts.rbegin(); it != contexts.rend(); ++it) {
      if (const clang::NamespaceDecl *ns =
              llvm::dyn_cast<clang::NamespaceDecl>(*it)) {
        std::string ns_name = ns->getNameAsString();
        if (!ns_name.empty()) {
          SgNonrealType *ns_type =
              SageBuilder::buildNonrealType(SgName(ns_name), scope, nullptr);
          if (SgNonrealDecl *ns_decl = isSgNonrealDecl(
                  ns_type ? ns_type->get_declaration() : nullptr)) {
            scope = ns_decl->get_nonreal_decl_scope();
          }
        }
      } else if (const clang::CXXRecordDecl *ctx_record =
                     llvm::dyn_cast<clang::CXXRecordDecl>(*it)) {
        std::string record_name = ctx_record->getNameAsString();
        if (!record_name.empty()) {
          const clang::ClassTemplateSpecializationDecl *ctx_spec =
              llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                  ctx_record);
          SgTemplateArgumentPtrList ctx_args;
          const SgTemplateArgumentPtrList *ctx_args_ptr = nullptr;
          if (ctx_spec != nullptr) {
            ctx_args = buildTemplateArguments(ctx_spec->getTemplateArgs(), 0);
            if (!ctx_args.empty()) {
              ctx_args_ptr = &ctx_args;
            }
          }
          SgNonrealType *record_type = SageBuilder::buildNonrealType(
              SgName(record_name), scope, ctx_args_ptr);
          if (SgNonrealDecl *record_decl = isSgNonrealDecl(
                  record_type ? record_type->get_declaration() : nullptr)) {
            scope = record_decl->get_nonreal_decl_scope();
          }
        }
      }
    }

    SgTemplateArgumentPtrList tpl_args =
        buildTemplateArguments(spec_decl->getTemplateArgs(), 0);
    std::string spec_name = spec_decl->getNameAsString();
    if (!spec_name.empty()) {
      SgNonrealType *spec_type = SageBuilder::buildNonrealType(
          SgName(spec_name), scope, tpl_args.empty() ? nullptr : &tpl_args);
      if (SgNonrealDecl *spec_decl_node = isSgNonrealDecl(
              spec_type ? spec_type->get_declaration() : nullptr)) {
        scope = spec_decl_node->get_nonreal_decl_scope();
      }
    }

    std::string typedef_name = typedef_decl->getNameAsString();
    if (typedef_name.empty()) {
      return nullptr;
    }
    SgNonrealType *member_type =
        SageBuilder::buildNonrealType(SgName(typedef_name), scope, nullptr);
    if (SgNonrealDecl *member_decl = isSgNonrealDecl(
            member_type ? member_type->get_declaration() : nullptr)) {
      if (!spec_decl->isDependentType()) {
        member_decl->set_suppress_typename(true);
      }
    }
    return member_type;
  };

  if (SgType *specialized_type =
          build_specialized_member_typedef(typedef_type->getDecl())) {
    *node = specialized_type;
    return VisitType(typedef_type, node) && res;
  }

  SgTypedefDeclaration *sg_typedef_decl = NULL;
  auto it = p_decl_translation_map.find(typedef_type->getDecl());
  if (it != p_decl_translation_map.end()) {
    sg_typedef_decl = isSgTypedefDeclaration(it->second);
  }

  if (sg_typedef_decl == NULL) {
    TraverseOnDemand(
        const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()));
    it = p_decl_translation_map.find(typedef_type->getDecl());
    if (it != p_decl_translation_map.end()) {
      sg_typedef_decl = isSgTypedefDeclaration(it->second);
    }
  }

  if (sg_typedef_decl == NULL) {
    clang::TypedefNameDecl *typedef_decl = typedef_type->getDecl();
    clang::DeclContext *scope_context = typedef_decl->getDeclContext();
    while (scope_context != NULL &&
           llvm::isa<clang::LinkageSpecDecl>(scope_context)) {
      scope_context = scope_context->getParent();
    }
    SgScopeStatement *decl_scope =
        resolveScopeFromDeclContext(scope_context, NULL);
    if (decl_scope == NULL) {
      if (clang::RecordDecl *record_ctx =
              llvm::dyn_cast_or_null<clang::RecordDecl>(scope_context)) {
        clang::RecordDecl *def_ctx = record_ctx->getDefinition();
        clang::RecordDecl *canon_ctx =
            llvm::dyn_cast_or_null<clang::RecordDecl>(
                record_ctx->getCanonicalDecl());
        bool in_progress =
            p_decl_translation_in_progress.count(record_ctx) > 0 ||
            (def_ctx && p_decl_translation_in_progress.count(def_ctx) > 0) ||
            (canon_ctx && p_decl_translation_in_progress.count(canon_ctx) > 0);
        if (in_progress) {
          SgScopeStatement *current = SageBuilder::topScopeStack();
          if (isSgClassDefinition(current) != nullptr ||
              isSgTemplateClassDefinition(current) != nullptr ||
              isSgTemplateInstantiationDefn(current) != nullptr) {
            decl_scope = current;
          }
        }
      }
    }
    if (decl_scope == NULL) {
      decl_scope = SageBuilder::topScopeStack();
    }
    if (decl_scope != NULL) {
      SgName name(typedef_decl->getNameAsString());
      SgType *underlying_type =
          buildTypeFromQualifiedType(typedef_decl->getUnderlyingType());
      sg_typedef_decl = SageBuilder::buildTypedefDeclaration_nfi(
          name, underlying_type, decl_scope);
      sg_typedef_decl->set_parent(decl_scope);
      sg_typedef_decl->set_typedef_type(
          llvm::isa<clang::TypeAliasDecl>(typedef_decl)
              ? SgTypedefDeclaration::e_using
              : SgTypedefDeclaration::e_typedef);
      setCompilerGeneratedFileInfo(sg_typedef_decl);
      suppress_unparse_output(sg_typedef_decl);
      p_decl_translation_map.insert(
          std::make_pair(typedef_decl, sg_typedef_decl));
    }
  }

  if (sg_typedef_decl != NULL) {
    *node = sg_typedef_decl->get_type();
  } else {
    SgSymbol *sym = GetSymbolFromSymbolTable(typedef_type->getDecl());
    SgTypedefSymbol *tdef_sym = isSgTypedefSymbol(sym);

    if (tdef_sym == NULL && SgProject::get_verbose() > 0) {
      std::cerr << "CFE: Missing typedef symbol for '"
                << typedef_type->getDecl()->getNameAsString() << "'"
                << std::endl;
    }

    *node = (tdef_sym != NULL) ? tdef_sym->get_type()
                               : SageBuilder::buildUnknownType();
  }

  return VisitType(typedef_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfExprType(
    clang::TypeOfExprType *type_of_expr_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TypeOfExprType" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_expr = Traverse(type_of_expr_type->getUnderlyingExpr());

  // printf ("In VisitTypeOfExprType(): tmp_expr = %p = %s
  // \n",tmp_expr,tmp_expr->class_name().c_str());

  SgExpression *expr = isSgExpression(tmp_expr);
  SgType *type = SageBuilder::buildTypeOfType(expr, NULL);

  *node = type;

  return VisitType(type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentTypeOfExprType(
    clang::DependentTypeOfExprType *dependent_type_of_expr_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentTypeOfExprType" << std::endl;
#endif
  bool res = true;

  return VisitTypeOfExprType(dependent_type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfType(clang::TypeOfType *type_of_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::TypeOfType" << std::endl;
#endif
  bool res = true;

  // In LLVM 20, getUnderlyingType() was renamed to getUnmodifiedType()
  SgType *underlyinigType =
      buildTypeFromQualifiedType(type_of_type->getUnmodifiedType());

  SgType *type = SageBuilder::buildTypeOfType(NULL, underlyinigType);

  *node = type;

  return VisitType(type_of_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeWithKeyword(
    clang::TypeWithKeyword *type_with_keyword, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitTypeWithKeyword" << std::endl;
#endif
  bool res = true;

  if (*node == nullptr) {
    clang::QualType desugared;
    if (const clang::ElaboratedType *elaborated =
            llvm::dyn_cast<clang::ElaboratedType>(type_with_keyword)) {
      desugared = elaborated->getNamedType();
    }
    if (desugared.isNull()) {
      clang::QualType qt(type_with_keyword, 0);
      desugared = qt.getCanonicalType();
    }

    if (!desugared.isNull() && desugared.getTypePtr() != type_with_keyword) {
      *node = buildTypeFromQualifiedType(desugared);
    } else {
      *node = SageBuilder::buildUnknownType();
    }
  }

  return VisitType(type_with_keyword, node) && res;
}

bool ClangToSageTranslator::VisitDependentNameType(
    clang::DependentNameType *dependent_name_type, SgNode **node) {
  bool res = true;

  const clang::IdentifierInfo *id = dependent_name_type->getIdentifier();
  ROSE_ASSERT(id != nullptr);

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(base_scope != nullptr);
  *node = buildNonrealTypeFromNestedNameSpecifier(
      dependent_name_type->getQualifier(), base_scope,
      SgName(id->getName().str()), nullptr);
  if (SgNonrealType *nrtype = isSgNonrealType(*node)) {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
      nrdecl->set_suppress_typename(false);
    }
  }

  return VisitTypeWithKeyword(dependent_name_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentTemplateSpecializationType(
    clang::DependentTemplateSpecializationType
        *dependent_template_specialization_type,
    SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::DependentTemplateSpecializationType"
            << std::endl;
#endif
  bool res = true;

  DependentTemplateSpecializationNameInfo name_info =
      getDependentTemplateSpecializationName(
          dependent_template_specialization_type);

  SgTemplateArgumentPtrList tpl_args;
  for (const clang::TemplateArgument &arg :
       dependent_template_specialization_type->template_arguments()) {
    appendTemplateArguments(tpl_args, arg, false);
  }

  SgScopeStatement *base_scope = SageBuilder::topScopeStack();
  ROSE_ASSERT(base_scope != nullptr);
  *node = buildNonrealTypeFromNestedNameSpecifier(
      name_info.qualifier, base_scope, SgName(name_info.base_name), &tpl_args);
  if (SgNonrealType *nrtype = isSgNonrealType(*node)) {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration())) {
      if (name_info.has_template_keyword) {
        nrdecl->set_has_template_keyword(true);
      }
    }
  }

  return VisitTypeWithKeyword(dependent_template_specialization_type, node) &&
         res;
}

bool ClangToSageTranslator::VisitElaboratedType(
    clang::ElaboratedType *elaborated_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitElaboratedType" << std::endl;
#endif

  clang::NestedNameSpecifier *qualifier = elaborated_type->getQualifier();
  if (qualifier != nullptr &&
      (nestedNameSpecifierHasTypeQualifier(qualifier) ||
       nestedNameSpecifierHasNamespaceQualifier(qualifier))) {
    if (!elaborated_type->isDependentType()) {
      const bool preserve_explicit_scope_qualifier =
          nestedNameSpecifierHasNamespaceQualifier(qualifier);
      if (!preserve_explicit_scope_qualifier) {
        if (SgType *resolved =
                buildTypeFromQualifiedType(elaborated_type->getNamedType())) {
          *node = resolved;
          return VisitTypeWithKeyword(elaborated_type, node);
        }
      }
    }

    if (const clang::TemplateSpecializationType *named_tst =
            llvm::dyn_cast<clang::TemplateSpecializationType>(
                elaborated_type->getNamedType().getTypePtrOrNull())) {
      if (named_tst->isTypeAlias()) {
        if (SgType *alias_type =
                buildTypeFromQualifiedType(elaborated_type->getNamedType())) {
          *node = alias_type;
          return VisitTypeWithKeyword(elaborated_type, node);
        }
      }
    }

    struct TerminalTypeInfo {
      SgName name;
      SgTemplateArgumentPtrList args;
      bool has_args = false;
      clang::DeclContext *decl_context = nullptr;
    };

    auto resolve_template_decl =
        [&](const clang::TemplateName &name) -> clang::TemplateDecl * {
      clang::TemplateName current = name;
      for (;;) {
        if (clang::TemplateDecl *decl = current.getAsTemplateDecl()) {
          return decl;
        }
        if (const clang::QualifiedTemplateName *qtn =
                current.getAsQualifiedTemplateName()) {
          clang::TemplateName underlying = qtn->getUnderlyingTemplate();
          if (clang::TemplateDecl *decl = underlying.getAsTemplateDecl()) {
            return decl;
          }
          current = underlying;
          continue;
        }
        if (const clang::SubstTemplateTemplateParmStorage *subst =
                current.getAsSubstTemplateTemplateParm()) {
          current = subst->getReplacement();
          continue;
        }
        if (clang::UsingShadowDecl *using_shadow =
                current.getAsUsingShadowDecl()) {
          return llvm::dyn_cast_or_null<clang::TemplateDecl>(
              using_shadow->getTargetDecl());
        }
        return nullptr;
      }
    };

    auto get_terminal_info = [&](clang::QualType named_type,
                                 TerminalTypeInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      const clang::Type *named_ptr = named_type.getTypePtrOrNull();
      if (named_ptr == nullptr) {
        return false;
      }

      if (const clang::TemplateSpecializationType *tst =
              llvm::dyn_cast<clang::TemplateSpecializationType>(named_ptr)) {
        std::string base_name = getTemplateNameBase(tst->getTemplateName());
        if (base_name.empty()) {
          return false;
        }
        info->name = SgName(base_name);
        info->args = buildTemplateArguments(tst);
        info->has_args = !info->args.empty();
        if (clang::TemplateDecl *template_decl =
                resolve_template_decl(tst->getTemplateName())) {
          info->decl_context = template_decl->getDeclContext();
        }
        return true;
      }

      if (const clang::DependentTemplateSpecializationType *dts =
              llvm::dyn_cast<clang::DependentTemplateSpecializationType>(
                  named_ptr)) {
        DependentTemplateSpecializationNameInfo name_info =
            getDependentTemplateSpecializationName(dts);
        if (name_info.base_name.empty()) {
          return false;
        }
        info->name = SgName(name_info.base_name);
        for (const clang::TemplateArgument &arg : dts->template_arguments()) {
          appendTemplateArguments(info->args, arg, false);
        }
        info->has_args = !info->args.empty();
        return true;
      }

      if (const clang::DependentNameType *dnt =
              llvm::dyn_cast<clang::DependentNameType>(named_ptr)) {
        const clang::IdentifierInfo *id = dnt->getIdentifier();
        if (id == nullptr) {
          return false;
        }
        info->name = SgName(id->getName().str());
        return true;
      }

      if (const clang::TemplateTypeParmType *ttp =
              llvm::dyn_cast<clang::TemplateTypeParmType>(named_ptr)) {
        std::string name_str;
        if (const clang::TemplateTypeParmDecl *decl = ttp->getDecl()) {
          name_str = decl->getNameAsString();
        }
        if (name_str.empty()) {
          return false;
        }
        info->name = SgName(name_str);
        return true;
      }

      if (const clang::TypedefType *tdef =
              llvm::dyn_cast<clang::TypedefType>(named_ptr)) {
        std::string name_str = tdef->getDecl()->getNameAsString();
        if (name_str.empty()) {
          return false;
        }
        info->name = SgName(name_str);
        info->decl_context = tdef->getDecl()->getDeclContext();
        return true;
      }

      if (const clang::TagType *tag =
              llvm::dyn_cast<clang::TagType>(named_ptr)) {
        std::string name_str = tag->getDecl()->getNameAsString();
        if (name_str.empty()) {
          return false;
        }
        info->name = SgName(name_str);
        info->decl_context = tag->getDecl()->getDeclContext();
        return true;
      }

      if (const clang::InjectedClassNameType *inj =
              llvm::dyn_cast<clang::InjectedClassNameType>(named_ptr)) {
        std::string name_str = inj->getDecl()->getNameAsString();
        if (name_str.empty()) {
          return false;
        }
        info->name = SgName(name_str);
        info->decl_context = inj->getDecl()->getDeclContext();
        return true;
      }

      return false;
    };

    TerminalTypeInfo info;
    if (get_terminal_info(elaborated_type->getNamedType(), &info)) {
      clang::NestedNameSpecifier *effective_qualifier = qualifier;
      if (effective_qualifier != nullptr && info.decl_context != nullptr &&
          p_compiler_instance != nullptr) {
        effective_qualifier =
            prependNamespaceQualifiers(effective_qualifier, info.decl_context,
                                       p_compiler_instance->getASTContext());
      }

      auto qualifier_has_namespace =
          [](clang::NestedNameSpecifier *nns) -> bool {
        for (clang::NestedNameSpecifier *cur = nns; cur != nullptr;
             cur = cur->getPrefix()) {
          switch (cur->getKind()) {
          case clang::NestedNameSpecifier::Namespace:
          case clang::NestedNameSpecifier::NamespaceAlias:
          case clang::NestedNameSpecifier::Global:
            return true;
          default:
            break;
          }
        }
        return false;
      };

      SgScopeStatement *base_scope = SageBuilder::topScopeStack();
      if (base_scope == nullptr) {
        base_scope = getGlobalScope();
      }
      if (qualifier_has_namespace(effective_qualifier)) {
        base_scope = getGlobalScope();
      } else if (info.decl_context != nullptr) {
        clang::DeclContext *ctx = info.decl_context;
        while (ctx != nullptr && !ctx->isNamespace() &&
               !ctx->isTranslationUnit()) {
          ctx = ctx->getParent();
        }
        if (SgScopeStatement *resolved =
                resolveScopeFromDeclContext(ctx, nullptr)) {
          base_scope = resolved;
        } else if (ctx != nullptr && ctx->isTranslationUnit()) {
          base_scope = getGlobalScope();
        }
      }
      *node = buildNonrealTypeFromNestedNameSpecifier(
          effective_qualifier, base_scope, info.name,
          info.has_args ? &info.args : nullptr);
      if (!elaborated_type->isDependentType()) {
        if (SgNonrealType *nrtype = isSgNonrealType(*node)) {
          if (SgNonrealDecl *nrdecl =
                  isSgNonrealDecl(nrtype->get_declaration())) {
            nrdecl->set_suppress_typename(true);
          }
        }
      }
      return VisitTypeWithKeyword(elaborated_type, node);
    }
  }

  SgType *type = buildTypeFromQualifiedType(elaborated_type->getNamedType());

  clang::TagDecl *ownedTagDecl = elaborated_type->getOwnedTagDecl();
#if DEBUG_VISIT_TYPE
  if (ownedTagDecl != nullptr) {
    std::cerr << "ClangToSageTranslator::VisitElaboratedType has ownedTagDecl "
              << "and isThisDeclarationADefinition = "
              << ownedTagDecl->isThisDeclarationADefinition() << "\n";
  }
#endif

  // CLANG FRONTEND NOTE: ElaboratedType contains namespace qualifiers (e.g.,
  // "std::" in "std::string") and struct/class/enum keywords that provide
  // "sugar" for the type reference.
  //
  // WARNING: Do NOT mutate the underlying SgTypedefDeclaration or other type
  // declarations! The same declaration is shared by all uses, so modifying it
  // causes corruption:
  //   1st use: "string" → "std::string"
  //   2nd use: "std::string" → "std::std::string"
  //   3rd use: "std::std::string" → "std::std::std::string"
  //
  // TODO: ROSE needs a proper way to represent elaborated types with
  // qualifiers. Possible solutions:
  //   - Create SgQualifiedNameType or similar wrapper
  //   - Store qualifier info as attributes on the type
  //   - Handle qualification during unparsing only
  //
  // For now, we just desugar to the named type (same as legacy frontend
  // behavior).

  *node = type;

  return VisitTypeWithKeyword(elaborated_type, node);
}

bool ClangToSageTranslator::VisitUnaryTransformType(
    clang::UnaryTransformType *unary_transform_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::UnaryTransformType" << std::endl;
#endif
  bool res = true;

  // UnaryTransformType is sugar that wraps another type (e.g.,
  // __underlying_type). Desugar it so downstream consumers see the real
  // underlying type instead of an unknown placeholder.
  clang::QualType underlying = unary_transform_type->desugar();
  if (underlying.isNull()) {
    underlying = unary_transform_type->getUnderlyingType();
  }
  if (underlying.isNull()) {
    underlying = unary_transform_type->getBaseType();
  }

  if (underlying.isNull()) {
    *node = SageBuilder::buildUnknownType();
  } else {
    *node = buildTypeFromQualifiedType(underlying);
  }

  return VisitType(unary_transform_type, node) && res;
}

// DependentUnaryTransformType was removed/renamed in LLVM 20
/*
bool
ClangToSageTranslator::VisitDependentUnaryTransformType(clang::DependentUnaryTransformType
* dependent_unary_transform_type, SgNode ** node) { #if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentUnaryTransformType" <<
std::endl; #endif bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitUnaryTransformType(dependent_unary_transform_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnresolvedUsingType(
    clang::UnresolvedUsingType *unresolved_using_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::UnresolvedUsingType" << std::endl;
#endif
  bool res = true;

  // Preserve the type as an unresolved dependent name rather than leaving a
  // null node (which triggers runtime errors and forces an unknown type later).
  *node = SageBuilder::buildUnknownType();

  return VisitType(unresolved_using_type, node) && res;
}

bool ClangToSageTranslator::VisitVectorType(clang::VectorType *vector_type,
                                            SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitVectorType" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(vector_type->getElementType());

  SgModifierType *modified_type = new SgModifierType(type);
  SgTypeModifier &sg_modifer = modified_type->get_typeModifier();

  sg_modifer.setVectorType();
  sg_modifer.set_vector_size(vector_type->getNumElements());

  *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

  return VisitType(vector_type, node);
}

bool ClangToSageTranslator::VisitExtVectorType(
    clang::ExtVectorType *ext_vector_type, SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitExtVectorType" << std::endl;
#endif
  bool res = true;

  return VisitVectorType(ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitUsingType(clang::UsingType *using_type,
                                           SgNode **node) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToSageTranslator::VisitUsingType" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: UsingType is a type alias from a using declaration
  // Desugar it to get the underlying type
  clang::QualType underlying = using_type->desugar();
  *node = buildTypeFromQualifiedType(underlying);

  return res;
}
