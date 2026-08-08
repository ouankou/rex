#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Analysis/CFG.h>
#include <clang/Driver/Driver.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Parse/Parser.h>
#include <clang/Sema/Sema.h>
#include <clang/Serialization/ASTReader.h>
#include <llvm/Frontend/OpenMP/OMP.h>

int main() {
  clang::CompilerInstance compiler;
  (void)compiler.hasDiagnostics();
  return 0;
}
