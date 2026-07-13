#include "clang-frontend-private.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/Basic/Builtins.h>
#include <clang/Tooling/Tooling.h>

#include <cstring>
#include <memory>

namespace {

clang::FunctionDecl *makeFunctionDecl(clang::ASTContext &context,
                                      const char *name) {
  clang::IdentifierInfo &identifier = context.Idents.get(name);
  const clang::QualType type =
      context.getFunctionType(context.VoidTy, llvm::ArrayRef<clang::QualType>(),
                              clang::FunctionProtoType::ExtProtoInfo());
  return clang::FunctionDecl::Create(
      context, context.getTranslationUnitDecl(), clang::SourceLocation(),
      clang::SourceLocation(), clang::DeclarationName(&identifier), type,
      nullptr, clang::SC_None);
}

} // namespace

int main(int argc, char **argv) {
  std::unique_ptr<clang::ASTUnit> ast = clang::tooling::buildASTFromCode(
      "", "rex_frontend_function_source_location_contract.cpp");
  if (ast == nullptr) {
    return 3;
  }

  clang::ASTContext &context = ast->getASTContext();
  clang::FunctionDecl *explicit_invalid =
      makeFunctionDecl(context, "rex_explicit_invalid");

  if (argc == 2 && std::strcmp(argv[1], "explicit-invalid") == 0) {
    requireClangFunctionDeclSourceProvenanceForFrontend(explicit_invalid,
                                                        nullptr, nullptr);
    return 4;
  }
  if (argc != 1) {
    return 2;
  }

  if (clangFunctionDeclIsBuiltinSupportForFrontend(explicit_invalid, nullptr,
                                                   nullptr)) {
    return 1;
  }

  clang::FunctionDecl *implicit_invalid =
      makeFunctionDecl(context, "rex_implicit_invalid");
  implicit_invalid->setImplicit();
  if (!clangFunctionDeclIsBuiltinSupportForFrontend(implicit_invalid, nullptr,
                                                    nullptr)) {
    return 1;
  }
  requireClangFunctionDeclSourceProvenanceForFrontend(implicit_invalid, nullptr,
                                                      nullptr);

  clang::FunctionDecl *builtin_id_invalid =
      makeFunctionDecl(context, "rex_builtin_id_invalid");
  builtin_id_invalid->addAttr(clang::BuiltinAttr::CreateImplicit(
      context, clang::Builtin::BI__builtin_trap));
  if (builtin_id_invalid->getBuiltinID() == clang::Builtin::NotBuiltin ||
      !clangFunctionDeclIsBuiltinSupportForFrontend(builtin_id_invalid, nullptr,
                                                    nullptr)) {
    return 1;
  }
  requireClangFunctionDeclSourceProvenanceForFrontend(builtin_id_invalid,
                                                      nullptr, nullptr);

  clang::FunctionDecl *builtin_attr_invalid =
      makeFunctionDecl(context, "rex_builtin_attr_invalid");
  builtin_attr_invalid->addAttr(clang::BuiltinAttr::CreateImplicit(
      context, clang::Builtin::BI__builtin_counted_by_ref));
  if (builtin_attr_invalid->getBuiltinID() != clang::Builtin::NotBuiltin ||
      !clangDeclHasBuiltinAttrDefinedForFrontend(builtin_attr_invalid) ||
      !clangFunctionDeclIsBuiltinSupportForFrontend(builtin_attr_invalid,
                                                    nullptr, nullptr)) {
    return 1;
  }
  requireClangFunctionDeclSourceProvenanceForFrontend(builtin_attr_invalid,
                                                      nullptr, nullptr);

  return 0;
}
