#ifndef ROSE_CLANG_NNS_UTILS_HPP
#define ROSE_CLANG_NNS_UTILS_HPP

#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <clang/AST/Decl.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/TemplateName.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeLoc.h>

inline clang::TemplateDecl *
resolveTemplateNameDeclaration(clang::TemplateName template_name) {
  for (;;) {
    if (clang::TemplateDecl *declaration = template_name.getAsTemplateDecl()) {
      return declaration;
    }
    if (const clang::QualifiedTemplateName *qualified =
            template_name.getAsQualifiedTemplateName()) {
      template_name = qualified->getUnderlyingTemplate();
      continue;
    }
    if (const clang::SubstTemplateTemplateParmStorage *substitution =
            template_name.getAsSubstTemplateTemplateParm()) {
      template_name = substitution->getReplacement();
      continue;
    }
    if (const clang::DeducedTemplateStorage *deduced =
            template_name.getAsDeducedTemplateName()) {
      template_name = deduced->getUnderlying();
      continue;
    }
    if (clang::UsingShadowDecl *using_shadow =
            template_name.getAsUsingShadowDecl()) {
      return llvm::dyn_cast_or_null<clang::TemplateDecl>(
          using_shadow->getTargetDecl());
    }
    return nullptr;
  }
}

inline bool
nestedNameSpecifierHasTemplateKeyword(clang::NestedNameSpecifier nns) {

  if (!nns || nns.getKind() != clang::NestedNameSpecifier::Kind::Type) {
    return false;
  }
  const clang::Type *type = nns.getAsType();

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

  llvm::report_fatal_error("unexpected nested-name-specifier kind");
}

inline const clang::NamespaceBaseDecl *
nestedNameSpecifierNamespaceBase(clang::NestedNameSpecifier nns) {

  if (!nns || nns.getKind() != clang::NestedNameSpecifier::Kind::Namespace) {
    return nullptr;
  }
  return nns.getAsNamespaceAndPrefix().Namespace;
}

inline bool nestedNameSpecifierHasGlobal(clang::NestedNameSpecifier qualifier) {
  for (clang::NestedNameSpecifier current = qualifier; current;
       current = nestedNameSpecifierPrefix(current)) {
    if (current.getKind() == clang::NestedNameSpecifier::Kind::Global)
      return true;
  }
  return false;
}

inline clang::NestedNameSpecifierLoc
nestedNameSpecifierLocPrefix(clang::NestedNameSpecifierLoc current) {
  switch (current.getNestedNameSpecifier().getKind()) {
  case clang::NestedNameSpecifier::Kind::Namespace:
    return current.getAsNamespaceAndPrefix().Prefix;
  case clang::NestedNameSpecifier::Kind::Type:
    return current.getAsTypeLoc().getPrefix();
  case clang::NestedNameSpecifier::Kind::Global:
  case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
  case clang::NestedNameSpecifier::Kind::Null:
    return clang::NestedNameSpecifierLoc();
  }
  llvm::report_fatal_error("unexpected nested-name-specifier location kind");
}

inline bool nestedNameSpecifierLocHasExplicitGlobal(
    clang::NestedNameSpecifierLoc qualifier_loc) {
  for (clang::NestedNameSpecifierLoc current = qualifier_loc; current;
       current = nestedNameSpecifierLocPrefix(current)) {
    if (current.getNestedNameSpecifier().getKind() ==
        clang::NestedNameSpecifier::Kind::Global)
      return current.getLocalBeginLoc().isValid();
  }
  return false;
}

inline const clang::NamespaceDecl *
nestedNameSpecifierNamespace(clang::NestedNameSpecifier nns) {
  return llvm::dyn_cast_or_null<clang::NamespaceDecl>(
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

    clang::TemplateName template_name =
        template_specialization->getTemplateName();

    return template_name.getQualifier();
  }
  if (const auto *deduced_template_specialization =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {

    clang::TemplateName template_name =
        deduced_template_specialization->getTemplateName();

    return template_name.getQualifier();
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
