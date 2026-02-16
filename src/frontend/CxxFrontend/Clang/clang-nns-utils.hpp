#ifndef ROSE_CLANG_NNS_UTILS_HPP
#define ROSE_CLANG_NNS_UTILS_HPP

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Casting.h>

#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/Type.h>

inline bool
nestedNameSpecifierHasTemplateKeyword(const clang::NestedNameSpecifier *nns) {
  if (nns == nullptr) {
    return false;
  }
  if (nns->getKind() != clang::NestedNameSpecifier::TypeSpec) {
    return false;
  }
  const clang::Type *type = nns->getAsType();
  if (const auto *elab = llvm::dyn_cast_or_null<clang::ElaboratedType>(type)) {
    type = elab->getNamedType().getTypePtr();
  }
  if (const auto *dependent =
          llvm::dyn_cast_or_null<clang::DependentTemplateSpecializationType>(
              type)) {
    return dependent->getDependentTemplateName().hasTemplateKeyword();
  }
  return false;
}

#endif
