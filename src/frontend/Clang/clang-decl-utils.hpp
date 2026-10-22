#ifndef _CLANG_DECL_UTILS_HPP_
#define _CLANG_DECL_UTILS_HPP_

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Type.h>
#include <vector>

class SgFunctionDeclaration;
namespace Rose::ClangFrontend {
SgFunctionDeclaration *canonical_function_identity(SgFunctionDeclaration *decl);
}

const clang::TemplateParameterList *
templateParametersForDeclContext(const clang::DeclContext *context);

std::vector<const clang::TemplateParameterList *>
collectTemplateParameterLevelsFromDeclContext(
    const clang::DeclContext *start_context,
    bool use_defining_record_contexts = false);

// Clang's getAsCXXRecordDecl selects a definition; retain the canonical tag.
inline const clang::CXXRecordDecl *
cxxRecordDeclFromQualTypeWithoutDefinitionLookup(clang::QualType type) {
  if (type.isNull())
    return nullptr;
  const auto *tag =
      llvm::dyn_cast<clang::TagType>(type.getCanonicalType().getTypePtr());
  return tag != nullptr ? llvm::dyn_cast<clang::CXXRecordDecl>(tag->getDecl())
                        : nullptr;
}

const clang::ParmVarDecl *
defaultArgumentParameterForLambda(const clang::CXXRecordDecl *declaration);

inline const clang::Expr *
getParmVarDeclDefaultArgExpr(const clang::ParmVarDecl *param_var_decl) {
  if (param_var_decl == nullptr || !param_var_decl->hasDefaultArg() ||
      param_var_decl->hasUnparsedDefaultArg() ||
      param_var_decl->hasInheritedDefaultArg()) {
    return nullptr;
  }

  if (param_var_decl->hasUninstantiatedDefaultArg()) {
    return param_var_decl->getUninstantiatedDefaultArg();
  }

  return param_var_decl->getDefaultArg();
}

#endif
