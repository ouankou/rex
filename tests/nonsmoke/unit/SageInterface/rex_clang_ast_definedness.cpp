// Standalone reproducer for EXT-CLANG-PACKAGED-DEFINEDNESS in
// https://github.com/ouankou/rex/issues/885. Link only Clang/LLVM, without
// REX. The unresolved reports are diagnostic evidence, not accepted exceptions.
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Tooling.h>

#include <fstream>
#include <iostream>
#include <iterator>

namespace {
struct Queries : clang::RecursiveASTVisitor<Queries> {
  unsigned functions = 0, initializers = 0, ranges = 0;

  bool shouldVisitTemplateInstantiations() const { return true; }
  bool shouldVisitImplicitCode() const { return true; }

  bool VisitDecl(clang::Decl *declaration) {
    if (auto *function = llvm::dyn_cast<clang::FunctionDecl>(declaration)) {
      if (function->doesThisDeclarationHaveABody()) {
        clang::Stmt *body = function->getBody();
        if (body)
          ++functions;
      }
    }
    if (auto *variable = llvm::dyn_cast<clang::VarDecl>(declaration)) {
      clang::Expr *initializer = variable->getInit();
      if (initializer)
        ++initializers;
    }
    clang::SourceRange range = declaration->getSourceRange();
    if (range.isValid())
      ++ranges;
    return true;
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: rex_clang_ast_definedness SOURCE -std=STANDARD\n";
    return 2;
  }
  std::ifstream stream(argv[1]);
  if (!stream)
    return 2;
  std::string source((std::istreambuf_iterator<char>(stream)), {});
  if (stream.bad())
    return 2;
  auto ast =
      clang::tooling::buildASTFromCodeWithArgs(source, {argv[2]}, argv[1]);
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    return 3;
  Queries queries;
  if (!queries.TraverseDecl(ast->getASTContext().getTranslationUnitDecl()))
    return 4;
  std::cout << "functions=" << queries.functions
            << " initializers=" << queries.initializers
            << " ranges=" << queries.ranges << '\n';
}
