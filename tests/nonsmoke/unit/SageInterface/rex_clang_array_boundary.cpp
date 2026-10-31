// Manual diagnostic for EXT-CLANG-RECORD-ARRAY-FLAG. The negative control
// passes caller-owned undefined type storage to the actual public Clang API.
// It must remain an error under the complete Memcheck policy. See
// https://github.com/ouankou/rex/issues/885 for commands and interpretation.
#include <clang/AST/Expr.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/Tooling.h>
#include <cstdlib>
#include <valgrind/memcheck.h>

class Consumer : public clang::ASTConsumer {
  clang::CompilerInstance &compiler;
  bool negative;

public:
  Consumer(clang::CompilerInstance &compiler, bool negative)
      : compiler(compiler), negative(negative) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    auto element = context.IntTy;
    auto *size = clang::IntegerLiteral::Create(context, llvm::APInt(32, 2),
                                               context.IntTy, {});
    if (negative)
      VALGRIND_MAKE_MEM_UNDEFINED(&element, sizeof element);
    auto array = compiler.getSema().BuildArrayType(
        element, clang::ArraySizeModifier::Normal, size, 0, {}, {});
    if (array.isNull())
      std::abort();
  }
};

class Action : public clang::ASTFrontendAction {
  bool negative;

public:
  explicit Action(bool negative) : negative(negative) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef) override {
    return std::make_unique<Consumer>(compiler, negative);
  }
};

int main(int argc, char **) {
  return !clang::tooling::runToolOnCode(std::make_unique<Action>(argc > 1), "");
}
