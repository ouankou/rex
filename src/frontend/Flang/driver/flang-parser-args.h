#pragma once
#include <algorithm>
#include <flang/Parser/dump-parse-tree.h>
#include <flang/Parser/message.h>
#include <flang/Parser/parse-tree-visitor.h>
#include <flang/Parser/parse-tree.h>
#include <flang/Parser/parsing.h>
#include <flang/Parser/provenance.h>
#include <flang/Parser/unparse.h>
#include <flang/Support/Fortran-features.h>
#include <flang/Support/LangOptions.h>
#include <flang/Support/OpenMP-features.h>
#include <flang/Support/default-kinds.h>

#include <filesystem>
#include <list>
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <vector>

struct DriverOptions {
  bool verbose{false};                             // -v
  std::vector<std::string> searchDirectories{"."}; // -I dir
  std::vector<std::string> intrinsicModuleDirectories;
  Fortran::common::IntrinsicTypeDefaultKinds defaultKinds;
  Fortran::common::LangOptions langOpts;
  bool forcedForm{false};             // -Mfixed or -Mfree appeared
  bool warnOnNonstandardUsage{false}; // -Mstandard
  bool warnOnSuspiciousUsage{false};  // -pedantic
  bool warningsAreErrors{false};      // -Werror
  Fortran::parser::Encoding encoding{Fortran::parser::Encoding::LATIN_1};
  bool lineDirectives{true}; // -P disables
  bool dumpProvenance{false};
  bool dumpUnparse{false};
  bool dumpParseTree{false};
  bool openMPEnabled{false};
};

struct DriverContext {
  DriverOptions driver{};
  Fortran::parser::Options options{};
};

inline void AddExistingSearchDirectory(std::vector<std::string> &dirs,
                                       const std::string &dir) {
  if (dir.empty()) {
    return;
  }
  std::filesystem::path path(dir);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return;
  }
  std::string normalized = path.lexically_normal().string();
  if (std::find(dirs.begin(), dirs.end(), normalized) == dirs.end()) {
    dirs.push_back(normalized);
  }
}

inline void AddFlangIntrinsicSearchDirectory(DriverOptions &driver) {
#ifdef ROSE_FLANG_INTRINSIC_INCLUDEDIR
  const std::filesystem::path flang_intrinsic_dir =
      std::filesystem::path(ROSE_FLANG_INTRINSIC_INCLUDEDIR);
  const std::string normalized_dir =
      flang_intrinsic_dir.lexically_normal().string();
  AddExistingSearchDirectory(driver.searchDirectories, normalized_dir);
  AddExistingSearchDirectory(driver.intrinsicModuleDirectories, normalized_dir);
#endif
}

inline int ParseFlangArgs(int argc, char *const argv[], DriverContext &ctx) {
  if (argc < 1 || argv == nullptr || argv[0] == nullptr) {
    llvm::errs() << "REX Fortran frontend: missing driver arguments\n";
    return EXIT_FAILURE;
  }

  ctx.options.features.Enable(Fortran::common::LanguageFeature::Unsigned,
                              false);

  // Transfer argv to a list of strings.
  std::list<std::string> args{};
  for (int ii = 0; ii < argc; ++ii) {
    args.emplace_back(argv[ii]);
  }

  args.pop_front();

  while (!args.empty()) {
    std::string arg{std::move(args.front())};
    args.pop_front();
    if (arg.empty() || arg == "-Xflang") {
    } else if (arg.at(0) != '-' || arg == "-") {
      // Input and object files are owned by the project. This invocation parses
      // only its already selected SgSourceFile.
    } else if (arg == "--") {
      break;
    } else if (arg == "-Mextend") {
      ctx.options.fixedFormColumns = 132;
      ctx.options.isFixedForm = true;
      ctx.driver.forcedForm = true;
    } else if (arg == "-Mfixed" || arg == "-ffixed-form" || arg == "-fixed" ||
               arg == "-qfixed") {
      ctx.options.isFixedForm = true;
      ctx.driver.forcedForm = true;
    } else if (arg == "-Mfree" || arg == "-ffree-form" || arg == "-free" ||
               arg == "-qfree" || arg == "-fno-fixed-form") {
      ctx.options.isFixedForm = false;
      ctx.driver.forcedForm = true;
    } else if (arg == "-Mstandard") {
      ctx.driver.warnOnNonstandardUsage = true;
    } else if (arg == "-pedantic") {
      ctx.driver.warnOnNonstandardUsage = true;
      ctx.driver.warnOnSuspiciousUsage = true;
    } else if (arg == "-Werror") {
      ctx.driver.warningsAreErrors = true;
    } else if (arg == "-ed") {
      ctx.options.features.Enable(
          Fortran::common::LanguageFeature::OldDebugLines);
    } else if (arg == "-E") {
      ctx.options.prescanAndReformat = true;
    } else if (arg == "-P") {
      ctx.driver.lineDirectives = false;
    } else if (arg == "-funsigned" || arg == "-fno-unsigned") {
      ctx.options.features.Enable(Fortran::common::LanguageFeature::Unsigned,
                                  arg == "-funsigned");
    } else if (arg == "-fimplicit-none" || arg == "-fno-implicit-none") {
      ctx.options.features.Enable(
          Fortran::common::LanguageFeature::ImplicitNoneTypeAlways,
          arg == "-fimplicit-none");
    } else if (arg == "-fimplicit-none-ext" ||
               arg == "-fno-implicit-none-ext") {
      ctx.options.features.Enable(
          Fortran::common::LanguageFeature::ImplicitNoneExternal,
          arg == "-fimplicit-none-ext");
    } else if (arg.rfind("-fimplicit", 0) == 0 ||
               arg.rfind("-fno-implicit", 0) == 0) {
      llvm::errs() << "REX Fortran frontend: unsupported implicit-typing "
                      "option '"
                   << arg << "'\n";
      return EXIT_FAILURE;
    } else if (arg == "-fbackslash") {
      ctx.options.features.Enable(
          Fortran::common::LanguageFeature::BackslashEscapes);
    } else if (arg == "-fno-backslash") {
      ctx.options.features.Enable(
          Fortran::common::LanguageFeature::BackslashEscapes, false);
    } else if (arg == "-fdump-provenance") {
      ctx.driver.dumpProvenance = true;
    } else if (arg == "-fdump-parse-tree") {
      ctx.driver.dumpParseTree = true;
    } else if (arg == "-funparse") {
      ctx.driver.dumpUnparse = true;
    } else if (arg == "-fopenmp") {
      ctx.options.features.Enable(Fortran::common::LanguageFeature::OpenMP);
      ctx.driver.openMPEnabled = true;
    } else if (arg == "-fopenmp-version" ||
               arg.rfind("-fopenmp-version=", 0) == 0) {
      llvm::errs()
          << "REX_FLANG_INVARIANT[removed-openmp-version-option]: option '"
          << arg
          << "' was part of the removed frontend-owned OpenMP design and is "
             "not supported\n";
      std::abort();
    } else if (arg == "-fopenacc") {
      ctx.options.features.Enable(Fortran::common::LanguageFeature::OpenACC);
      ctx.options.predefinitions.emplace_back("_OPENACC", "202211");
    } else if (arg == "-o") {
      if (args.empty()) {
        llvm::errs() << "REX Fortran frontend: missing operand for -o\n";
        return EXIT_FAILURE;
      }
      args.pop_front(); // Output is consumed only by the project backend.
    } else if (arg.rfind("-D", 0) == 0 || arg.rfind("-U", 0) == 0) {
      std::string macro = arg.substr(2);
      if (macro.empty()) {
        if (args.empty() || args.front().empty() || args.front()[0] == '-') {
          llvm::errs() << "REX Fortran frontend: missing operand for " << arg
                       << "\n";
          return EXIT_FAILURE;
        }
        macro = std::move(args.front());
        args.pop_front();
      }
      if (arg[1] == 'D') {
        const auto eq = macro.find('=');
        ctx.options.predefinitions.emplace_back(
            macro.substr(0, eq),
            eq == std::string::npos ? "1" : macro.substr(eq + 1));
      } else {
        ctx.options.predefinitions.emplace_back(macro,
                                                std::optional<std::string>{});
      }
    } else if (arg == "-r8" || arg == "-fdefault-real-8") {
      ctx.driver.defaultKinds.set_defaultRealKind(8);
    } else if (arg == "-i8" || arg == "-fdefault-integer-8") {
      ctx.driver.defaultKinds.set_defaultIntegerKind(8);
      ctx.driver.defaultKinds.set_defaultLogicalKind(8);
    } else if (arg == "-help" || arg == "--help" || arg == "-?") {
      llvm::errs()
          << "REX Fortran frontend options:\n"
          << "  -Mfixed | -Mfree     force the source form\n"
          << "  -Mextend             132-column fixed form\n"
          << "  -f[no-]backslash     enable[disable] \\escapes in literals\n"
          << "  -Mstandard           enable conformance warnings\n"
          << "  -r8 | -fdefault-real-8 | -i8 | -fdefault-integer-8  "
             "change default kinds of intrinsic types\n"
          << "  -Werror              treat warnings as errors\n"
          << "  -ed                  enable fixed form D lines\n"
          << "  -E                   prescan & preprocess only\n"
          << "  -fsyntax-only        parse only, no output except messages\n"
          << "  -funparse            parse & reformat only, no code "
             "generation\n"
          << "  -fdump-provenance    dump the provenance table (no code)\n"
          << "  -fdump-parse-tree    dump the parse tree (no code)\n"
          << "  -v -c -o -I -D -U    have their usual meanings\n"
          << "  -help                print this again\n"
          << "Compilation and linking options are handled by the REX "
             "backend.\n";
      return EXIT_SUCCESS;
    } else if (arg == "-V") {
      llvm::errs() << "\nREX Fortran frontend\n";
      return EXIT_SUCCESS;
    } else {
      if (arg == "-v") {
        ctx.driver.verbose = true;
      } else if (arg == "-I") {
        if (args.empty()) {
          llvm::errs() << "REX Fortran frontend: missing operand for -I\n";
          return EXIT_FAILURE;
        }
        ctx.driver.searchDirectories.push_back(args.front());
        args.pop_front();
      } else if (arg.substr(0, 2) == "-I") {
        ctx.driver.searchDirectories.push_back(arg.substr(2));
      }
    }
  }

  if (ctx.driver.warnOnNonstandardUsage) {
    ctx.options.features.WarnOnAllNonstandard();
  }
  if (ctx.driver.warnOnSuspiciousUsage) {
    ctx.options.features.WarnOnAllUsage();
  }
  if (ctx.driver.openMPEnabled) {
    constexpr unsigned defaultOpenMPVersion = 60;
    ctx.driver.langOpts.OpenMPVersion = defaultOpenMPVersion;
    Fortran::common::setOpenMPMacro(ctx.driver.langOpts.OpenMPVersion,
                                    ctx.options.predefinitions);
  }
  AddFlangIntrinsicSearchDirectory(ctx.driver);

  return EXIT_SUCCESS;
}
