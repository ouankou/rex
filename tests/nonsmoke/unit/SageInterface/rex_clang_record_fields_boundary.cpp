// Manual diagnostic for EXT-CLANG-RECORD-FIELD-FLAG. The negative control
// supplies an undefined caller-owned flag to the real Sema::ActOnFields API.
// It must remain visible under the complete policy; no production AST is
// annotated. See https://github.com/ouankou/rex/issues/885.
#include <clang/AST/Decl.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/Tooling.h>
#include <valgrind/memcheck.h>
class Consumer : public clang::ASTConsumer {
  clang::CompilerInstance &compiler;
  bool negative;

public:
  Consumer(clang::CompilerInstance &compiler, bool negative)
      : compiler(compiler), negative(negative) {}
  void HandleTranslationUnit(clang::ASTContext &context) override {
    auto *tu = context.getTranslationUnitDecl();
    auto record = [&](const char *name) {
      auto *r =
          clang::RecordDecl::Create(context, clang::TagTypeKind::Struct, tu, {},
                                    {}, &context.Idents.get(name));
      r->startDefinition();
      tu->addDecl(r);
      return r;
    };
    auto field = [&](clang::RecordDecl *owner, clang::QualType type) {
      auto *f = clang::FieldDecl::Create(
          context, owner, {}, {}, &context.Idents.get("value"), type, nullptr,
          nullptr, false, clang::ICIS_NoInit);
      owner->addDecl(f);
      return f;
    };
    auto *inner = record("rex_inner");
    field(inner, context.IntTy);
    inner->completeDefinition();
    bool flexible = false;
    if (negative)
      VALGRIND_MAKE_MEM_UNDEFINED(&flexible, sizeof flexible);
    inner->setHasFlexibleArrayMember(flexible);
    auto *outer = record("rex_outer");
    clang::Decl *f = field(outer, context.getCanonicalTagType(inner));
    compiler.getSema().ActOnFields(nullptr, {}, outer,
                                   llvm::ArrayRef<clang::Decl *>(&f, 1), {}, {},
                                   clang::ParsedAttributesView{});
  }
};
class Action : public clang::ASTFrontendAction {
  bool negative;

public:
  explicit Action(bool negative) : negative(negative) {}
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &c, llvm::StringRef) override {
    return std::make_unique<Consumer>(c, negative);
  }
};
int main(int argc, char **) {
  return !clang::tooling::runToolOnCodeWithArgs(
      std::make_unique<Action>(argc > 1), "", {"-xc", "-std=c11"});
}
