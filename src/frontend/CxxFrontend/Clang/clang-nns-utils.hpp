#ifndef ROSE_CLANG_NNS_UTILS_HPP
#define ROSE_CLANG_NNS_UTILS_HPP

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <clang/AST/Decl.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/TemplateName.h>
#include <clang/AST/Type.h>

inline bool
nestedNameSpecifierHasTemplateKeyword(clang::NestedNameSpecifier nns) {
  if (!nns || nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
    return false;
  }
  const clang::Type *type = nns.getAsType();
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
    return nns.getAsNamespaceAndPrefix().Prefix;
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
  return nns.getAsNamespaceAndPrefix().Namespace;
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
  if (type == nullptr) {
    return std::nullopt;
  }

  if (const auto *tag = llvm::dyn_cast<clang::TagType>(type)) {
    return tag->getQualifier();
  }
  if (const auto *typedef_type = llvm::dyn_cast<clang::TypedefType>(type)) {
    return typedef_type->getQualifier();
  }
  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(type)) {
    return using_type->getQualifier();
  }
  if (const auto *unresolved_using =
          llvm::dyn_cast<clang::UnresolvedUsingType>(type)) {
    return unresolved_using->getQualifier();
  }
  if (const auto *dependent_name =
          llvm::dyn_cast<clang::DependentNameType>(type)) {
    return dependent_name->getQualifier();
  }
  if (const auto *template_specialization =
          llvm::dyn_cast<clang::TemplateSpecializationType>(type)) {
    return template_specialization->getTemplateName().getQualifier();
  }
  if (const auto *deduced_template_specialization =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {
    return deduced_template_specialization->getTemplateName().getQualifier();
  }
  if (const auto *injected_class_name =
          llvm::dyn_cast<clang::InjectedClassNameType>(type)) {
    return injected_class_name->getQualifier();
  }

  return std::nullopt;
}

inline bool qualifiedTypeHasQualifier(const clang::Type *type) {
  return static_cast<bool>(qualifiedTypeQualifier(type));
}

#endif
