// EXT-CLANG-DEDUCTION-LEAK: https://github.com/ouankou/rex/issues/885
// Standalone Clang lifetime probe; intentionally does not link REX.
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  if (argc != 5)
    return 2;
  const int repeats = std::stoi(argv[2]);
  if (repeats < 1)
    return 2;
  std::ifstream input(argv[1]);
  if (!input)
    return 2;
  const std::string code{std::istreambuf_iterator<char>(input), {}};
  for (int count = 0; count < repeats; ++count) {
    auto ast = clang::tooling::buildASTFromCodeWithArgs(
        code, {"-std=c++20", std::string("-resource-dir=") + argv[4]},
        "rex_clang_deduction_input.cpp", argv[3]);
    if (!ast || ast->getDiagnostics().hasErrorOccurred())
      return 3;
  }
}
