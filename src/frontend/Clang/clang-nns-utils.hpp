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

inline void markClangNnsNamespaceAndPrefixStorageDefined(
    clang::NestedNameSpecifier nns, const clang::NamespaceAndPrefix &value) {
#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
  if (!RUNNING_ON_VALGRIND || !value.Prefix) {
    return;
  }

  const clang::NestedNameSpecifier::Kind prefix_kind =
      readClangNnsApiValueDefined([&]() { return value.Prefix.getKind(); });
  if (prefix_kind != clang::NestedNameSpecifier::Kind::Namespace) {
    return;
  }

  constexpr uintptr_t nns_flag_bits = 2;
  constexpr uintptr_t nns_flag_offset = 1;
  constexpr uintptr_t nns_ptr_offset = nns_flag_bits + nns_flag_offset;
  constexpr uintptr_t nns_ptr_mask = (uintptr_t(1) << nns_ptr_offset) - 1;
  const uintptr_t encoded = reinterpret_cast<uintptr_t>(nns.getAsVoidPointer());
  void *storage = reinterpret_cast<void *>(encoded & ~nns_ptr_mask);
  if (storage != nullptr) {
    VALGRIND_MAKE_MEM_DEFINED(storage,
                              sizeof(clang::NamespaceAndPrefixStorage));
  }
#else
  (void)nns;
  (void)value;
#endif
}

inline clang::NamespaceAndPrefix
nestedNameSpecifierNamespaceAndPrefix(clang::NestedNameSpecifier nns) {
  clang::NamespaceAndPrefix result = readClangNnsApiValueDefined(
      [&]() { return nns.getAsNamespaceAndPrefix(); });
  markClangNnsNamespaceAndPrefixStorageDefined(nns, result);
  markClangNnsNamespaceBaseDeclDefined(result.Namespace);
  markClangNnsValueDefined(result.Prefix);
  return result;
}

inline bool
nestedNameSpecifierHasTemplateKeyword(clang::NestedNameSpecifier nns) {
  markClangNnsValueDefined(nns);
  if (!nns || readClangNnsApiValueDefined([&]() { return nns.getKind(); }) !=
                  clang::NestedNameSpecifier::Kind::Type) {
    return false;
  }
  const clang::Type *type = markClangNnsTypeObjectDefined(
      readClangNnsApiValueDefined([&]() { return nns.getAsType(); }));
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
  markClangNnsValueDefined(nns);
  if (!nns) {
    return std::nullopt;
  }

  switch (readClangNnsApiValueDefined([&]() { return nns.getKind(); })) {
  case clang::NestedNameSpecifier::Kind::Null:
  case clang::NestedNameSpecifier::Kind::Global:
  case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
    return std::nullopt;
  case clang::NestedNameSpecifier::Kind::Namespace:
    return nestedNameSpecifierNamespaceAndPrefix(nns).Prefix;
  case clang::NestedNameSpecifier::Kind::Type:
    if (const clang::Type *type = markClangNnsTypeObjectDefined(
            readClangNnsApiValueDefined([&]() { return nns.getAsType(); }))) {
      return readClangNnsApiValueDefined([&]() { return type->getPrefix(); });
    }
    return std::nullopt;
  }

  llvm_unreachable("unexpected nested-name-specifier kind");
}

inline const clang::NamespaceBaseDecl *
nestedNameSpecifierNamespaceBase(clang::NestedNameSpecifier nns) {
  markClangNnsValueDefined(nns);
  if (!nns || readClangNnsApiValueDefined([&]() { return nns.getKind(); }) !=
                  clang::NestedNameSpecifier::Kind::Namespace) {
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
    return readClangNnsApiValueDefined([&]() { return tag->getQualifier(); });
  }
  if (const auto *typedef_type = llvm::dyn_cast<clang::TypedefType>(type)) {
    typedef_type = markClangNnsAstObjectDefined(typedef_type);
    return readClangNnsApiValueDefined(
        [&]() { return typedef_type->getQualifier(); });
  }
  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(type)) {
    using_type = markClangNnsAstObjectDefined(using_type);
    return readClangNnsApiValueDefined(
        [&]() { return using_type->getQualifier(); });
  }
  if (const auto *unresolved_using =
          llvm::dyn_cast<clang::UnresolvedUsingType>(type)) {
    unresolved_using = markClangNnsAstObjectDefined(unresolved_using);
    return readClangNnsApiValueDefined(
        [&]() { return unresolved_using->getQualifier(); });
  }
  if (const auto *dependent_name =
          llvm::dyn_cast<clang::DependentNameType>(type)) {
    dependent_name = markClangNnsAstObjectDefined(dependent_name);
    return readClangNnsApiValueDefined(
        [&]() { return dependent_name->getQualifier(); });
  }
  if (const auto *template_specialization =
          llvm::dyn_cast<clang::TemplateSpecializationType>(type)) {
    template_specialization =
        markClangNnsAstObjectDefined(template_specialization);
    clang::TemplateName template_name = readClangNnsApiValueDefined(
        [&]() { return template_specialization->getTemplateName(); });
    markClangNnsValueDefined(template_name);
    return readClangNnsApiValueDefined(
        [&]() { return template_name.getQualifier(); });
  }
  if (const auto *deduced_template_specialization =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {
    deduced_template_specialization =
        markClangNnsAstObjectDefined(deduced_template_specialization);
    clang::TemplateName template_name = readClangNnsApiValueDefined(
        [&]() { return deduced_template_specialization->getTemplateName(); });
    markClangNnsValueDefined(template_name);
    return readClangNnsApiValueDefined(
        [&]() { return template_name.getQualifier(); });
  }
  if (const auto *injected_class_name =
          llvm::dyn_cast<clang::InjectedClassNameType>(type)) {
    injected_class_name = markClangNnsAstObjectDefined(injected_class_name);
    return readClangNnsApiValueDefined(
        [&]() { return injected_class_name->getQualifier(); });
  }

  return std::nullopt;
}

inline bool qualifiedTypeHasQualifier(const clang::Type *type) {
  return static_cast<bool>(qualifiedTypeQualifier(type));
}

#endif
