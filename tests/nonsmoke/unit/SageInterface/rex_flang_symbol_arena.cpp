// EXT-FLANG-SYMBOL-ARENA: standalone LLVM 22 diagnostic, not a REX test target.
// See docs/codebase-audit/external-exceptions.md for build/run instructions.
// Each iteration destroys its parser and semantic context before the next one.
#include <flang/Parser/parsing.h>
#include <flang/Semantics/semantics.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <string>

int parseOne(const char *path, const char *modules) {
  Fortran::parser::AllSources sources;
  Fortran::parser::AllCookedSources cooked(sources);
  Fortran::parser::Parsing parsing(cooked);
  Fortran::parser::Options options;
  const std::string filename(path);
  options.isFixedForm = filename.size() > 2 &&
                        filename.compare(filename.size() - 2, 2, ".f") == 0;
  options.searchDirectories = {modules, "."};
  parsing.Prescan(path, options);
  if (parsing.messages().AnyFatalError()) {
    parsing.messages().Emit(llvm::errs(), cooked);
    return 1;
  }
  parsing.Parse(llvm::nulls());
  if (parsing.messages().AnyFatalError() || !parsing.parseTree()) {
    parsing.messages().Emit(llvm::errs(), cooked);
    return 1;
  }
  Fortran::common::IntrinsicTypeDefaultKinds kinds;
  Fortran::common::LangOptions language;
  Fortran::semantics::SemanticsContext context(kinds, options.features,
                                               language, cooked);
  context.set_searchDirectories(options.searchDirectories);
  context.set_intrinsicModuleDirectories({"/usr/lib/llvm-22/include/flang"});
  Fortran::semantics::Semantics semantics(context, *parsing.parseTree());
  const bool success = semantics.Perform();
  semantics.EmitMessages(llvm::errs());
  return success ? 0 : 1;
}

int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  const int repetitions = std::atoi(argv[3]);
  if (repetitions < 1)
    return 2;
  for (int i = 0; i < repetitions; ++i)
    if (parseOne(argv[1], argv[2]) != 0)
      return 1;
  return 0;
}
