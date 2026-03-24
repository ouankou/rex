#ifndef _CLANG_DECL_UTILS_HPP_
#define _CLANG_DECL_UTILS_HPP_

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>

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
