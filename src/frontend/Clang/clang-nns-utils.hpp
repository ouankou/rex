#ifndef ROSE_CLANG_NNS_UTILS_HPP
#define ROSE_CLANG_NNS_UTILS_HPP

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <clang/AST/Decl.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/TemplateName.h>
#include <clang/AST/Type.h>

#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
#endif

inline const clang::Type *
markClangNnsTypeObjectDefined(const clang::Type *type) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_MAKE_MEM_DEFINED(&type, sizeof(type));
    if (type != nullptr) {
      VALGRIND_MAKE_MEM_DEFINED(const_cast<clang::Type *>(type), sizeof(*type));
    }
  }
#endif
  return type;
}

template <typename T>
inline const T *markClangNnsAstObjectDefined(const T *node) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_MAKE_MEM_DEFINED(&node, sizeof(node));
    if (node != nullptr) {
      VALGRIND_MAKE_MEM_DEFINED(const_cast<T *>(node), sizeof(*node));
    }
  }
#endif
  return node;
}

template <typename T> inline const T &markClangNnsValueDefined(const T &value) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<T *>(&value), sizeof(value));
  }
#endif
  return value;
}

template <typename F>
inline auto readClangNnsApiValueDefined(F &&read) -> decltype(read()) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_DISABLE_ERROR_REPORTING;
    auto value = read();
    VALGRIND_ENABLE_ERROR_REPORTING;
    markClangNnsValueDefined(value);
    return value;
  }
#endif
  auto value = read();
  markClangNnsValueDefined(value);
  return value;
}

inline const clang::NamespaceBaseDecl *
markClangNnsNamespaceBaseDeclDefined(const clang::NamespaceBaseDecl *decl) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  decl = markClangNnsAstObjectDefined(decl);
  if (!RUNNING_ON_VALGRIND || decl == nullptr) {
    return decl;
  }

  if (const auto *namespace_decl = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
    return markClangNnsAstObjectDefined(namespace_decl);
  }
  if (const auto *alias_decl =
          llvm::dyn_cast<clang::NamespaceAliasDecl>(decl)) {
    return markClangNnsAstObjectDefined(alias_decl);
  }
#endif
  return decl;
}

inline clang::NamespaceAndPrefix
nestedNameSpecifierNamespaceAndPrefix(clang::NestedNameSpecifier nns) {
  clang::NamespaceAndPrefix result = readClangNnsApiValueDefined(
      [&]() { return nns.getAsNamespaceAndPrefix(); });
  markClangNnsNamespaceBaseDeclDefined(result.Namespace);
  markClangNnsValueDefined(result.Prefix);
  return result;
}

inline bool
nestedNameSpecifierHasTemplateKeyword(clang::NestedNameSpecifier nns) {
  if (!nns || nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
    return false;
  }
  const clang::Type *type = markClangNnsTypeObjectDefined(nns.getAsType());
  if (type == nullptr) {
    return false;
  }

  auto templateNameHasTemplateKeyword = [](const clang::TemplateName &name) {
    if (const auto *qualified = name.getAsQualifiedTemplateName()) {
      return qualified->hasTemplateKeyword();
    }
    if (const auto *dependent = name.getAsDependentTemplateName()) {
      return dependent->hasTemplateKeyword();
    }
    return false;
  };

  if (const auto *specialization =
          llvm::dyn_cast<clang::TemplateSpecializationType>(type)) {
    return templateNameHasTemplateKeyword(specialization->getTemplateName());
  }
  if (const auto *deduced =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {
    return templateNameHasTemplateKeyword(deduced->getTemplateName());
  }

  return false;
}

inline clang::NestedNameSpecifier
nestedNameSpecifierPrefix(clang::NestedNameSpecifier nns) {
  if (!nns) {
    return std::nullopt;
  }

  switch (nns.getKind()) {
  case clang::NestedNameSpecifier::Kind::Null:
  case clang::NestedNameSpecifier::Kind::Global:
  case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
    return std::nullopt;
  case clang::NestedNameSpecifier::Kind::Namespace:
    return nestedNameSpecifierNamespaceAndPrefix(nns).Prefix;
  case clang::NestedNameSpecifier::Kind::Type:
    return nns.getAsType()->getPrefix();
  }

  llvm_unreachable("unexpected nested-name-specifier kind");
}

inline const clang::NamespaceBaseDecl *
nestedNameSpecifierNamespaceBase(clang::NestedNameSpecifier nns) {
  if (!nns || nns.getKind() != clang::NestedNameSpecifier::Kind::Namespace) {
    return nullptr;
  }
  return markClangNnsNamespaceBaseDeclDefined(
      nestedNameSpecifierNamespaceAndPrefix(nns).Namespace);
}

inline const clang::NamespaceDecl *
nestedNameSpecifierNamespace(clang::NestedNameSpecifier nns) {
  return llvm::dyn_cast_or_null<clang::NamespaceDecl>(
      nestedNameSpecifierNamespaceBase(nns));
}

inline const clang::NamespaceAliasDecl *
nestedNameSpecifierNamespaceAlias(clang::NestedNameSpecifier nns) {
  return llvm::dyn_cast_or_null<clang::NamespaceAliasDecl>(
      nestedNameSpecifierNamespaceBase(nns));
}

inline clang::NestedNameSpecifier
qualifiedTypeQualifier(const clang::Type *type) {
  type = markClangNnsTypeObjectDefined(type);
  if (type == nullptr) {
    return std::nullopt;
  }

  if (const auto *tag = llvm::dyn_cast<clang::TagType>(type)) {
    tag = markClangNnsAstObjectDefined(tag);
    return tag->getQualifier();
  }
  if (const auto *typedef_type = llvm::dyn_cast<clang::TypedefType>(type)) {
    typedef_type = markClangNnsAstObjectDefined(typedef_type);
    return typedef_type->getQualifier();
  }
  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(type)) {
    using_type = markClangNnsAstObjectDefined(using_type);
    return using_type->getQualifier();
  }
  if (const auto *unresolved_using =
          llvm::dyn_cast<clang::UnresolvedUsingType>(type)) {
    unresolved_using = markClangNnsAstObjectDefined(unresolved_using);
    return unresolved_using->getQualifier();
  }
  if (const auto *dependent_name =
          llvm::dyn_cast<clang::DependentNameType>(type)) {
    dependent_name = markClangNnsAstObjectDefined(dependent_name);
    return dependent_name->getQualifier();
  }
  if (const auto *template_specialization =
          llvm::dyn_cast<clang::TemplateSpecializationType>(type)) {
    template_specialization =
        markClangNnsAstObjectDefined(template_specialization);
    return template_specialization->getTemplateName().getQualifier();
  }
  if (const auto *deduced_template_specialization =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {
    deduced_template_specialization =
        markClangNnsAstObjectDefined(deduced_template_specialization);
    return deduced_template_specialization->getTemplateName().getQualifier();
  }
  if (const auto *injected_class_name =
          llvm::dyn_cast<clang::InjectedClassNameType>(type)) {
    injected_class_name = markClangNnsAstObjectDefined(injected_class_name);
    return injected_class_name->getQualifier();
  }

  return std::nullopt;
}

inline bool qualifiedTypeHasQualifier(const clang::Type *type) {
  return static_cast<bool>(qualifiedTypeQualifier(type));
}

#endif
