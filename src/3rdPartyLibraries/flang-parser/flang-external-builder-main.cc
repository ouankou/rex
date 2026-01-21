//===-- tools/f18/f18-parse-demo.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// F18 parsing demonstration.
//   f18-parse-demo [ -E | -fdump-parse-tree | -funparse-only ]
//     foo.{f,F,f77,F77,f90,F90,&c.}
//
// By default, runs the supplied source files through the F18 preprocessing and
// parsing phases, reconstitutes a Fortran program from the parse tree, and
// passes that Fortran program to a Fortran compiler identified by the $F18_FC
// environment variable (defaulting to flang-20).  The Fortran preprocessor is
// always run, whatever the case of the source file extension.  Unrecognized
// options are passed through to the underlying Fortran compiler.
//
// This program is actually a stripped-down variant of f18.cpp, a temporary
// scaffolding compiler driver that can test some semantic passes of the
// F18 compiler under development.

//----------------------------------------------------------------------------//

// TODO: This should be an command line option
#define DUMP_PARSE_TREE 0

// Fortran front end driver main program for ROSE scaffolding.

#include "sage3basic.h"

#include "FlangParseArgs.hh"

#include "../../frontend/Experimental_Flang_ROSE_Connection/sage-build.h"

std::vector<std::string> filesToDelete;

static void CleanUpAtExit() {
  for (const auto &path : filesToDelete) {
    if (!path.empty()) {
      llvm::sys::fs::remove(path);
    }
  }
}

// Turn on CPU timing
#if _POSIX_C_SOURCE >= 199309L && _POSIX_TIMERS > 0 && _POSIX_CPUTIME &&       \
    defined CLOCK_PROCESS_CPUTIME_ID
static constexpr bool canTime{true};
double CPUseconds() {
  struct timespec tspec;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tspec);
  return tspec.tv_nsec * 1.0e-9 + tspec.tv_sec;
}
#else
static constexpr bool canTime{false};
double CPUseconds() { return 0; }
#endif

void Exec(std::vector<llvm::StringRef> &argv, bool verbose = false) {
  if (verbose) {
    for (size_t j{0}; j < argv.size(); ++j) {
      llvm::errs() << argv[j] << " ";
    }
    llvm::errs() << "\n";
  }
  std::string ErrMsg;
  int status =
      llvm::sys::ExecuteAndWait(argv[0], argv, std::nullopt, {}, 0, 0, &ErrMsg);
  if (status != 0) {
    llvm::errs() << ErrMsg << "\n";
    std::exit(EXIT_FAILURE);
  }
}

std::string CompileOtherLanguage(const std::string &path,
                                 const DriverOptions &driver) {
  if (driver.verbose) {
    llvm::errs() << "f18-parse-demo: not compiling \"" << path
                 << "\" because it's not a Fortran source file\n";
  }
  return {};
}

std::string CompileFortran(const std::string &path,
                           const Fortran::parser::Options &optionsIn,
                           const DriverOptions &driver) {
  // TODO: should be able to use -o to set object file name
  // TODO: support Unicode names in filesystem?
  if (driver.verbose) {
    llvm::errs() << "Compiling \"" << path << "\"\n";
  }

  Fortran::parser::Options options{optionsIn};
  options.searchDirectories = driver.searchDirectories;
  options.encoding = driver.encoding;
  options.expandIncludeLinesInPreprocessedOutput = driver.lineDirectives;

  Fortran::parser::AllSources allSources;
  allSources.set_encoding(driver.encoding);
  allSources.setShowColors(options.showColors);
  Fortran::parser::AllCookedSources allCookedSources{allSources};
  Fortran::parser::Parsing parsing{allCookedSources};

  parsing.Prescan(path, options);

  auto &messages = parsing.messages();
  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  if (messages.AnyFatalError()) {
    return {};
  }

  if (options.prescanAndReformat) {
    parsing.EmitPreprocessedSource(llvm::outs(), driver.lineDirectives);
    return {};
  }

  parsing.Parse(llvm::nulls());

  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  if (messages.AnyFatalError()) {
    return {};
  }

  auto &parseTreeOpt = parsing.parseTree();
  if (!parseTreeOpt) {
    return {};
  }
  auto &parseTree{*parseTreeOpt};

  // Transform the parse tree using Rose::builder
  if (driver.externalBuilder) {
    auto start{CPUseconds()};
    Rose::builder::Build(parseTree, allCookedSources);
    auto stop{CPUseconds()};
    if (canTime) {
      llvm::outs() << "Rose::Build time for " << path << ": " << (stop - start)
                   << " CPU milliseconds\n";
    }
  }

  if (driver.dumpProvenance) {
    parsing.DumpProvenance(llvm::outs());
    return {};
  }
  if (driver.dumpParseTree) {
    Fortran::parser::DumpTree(llvm::outs(), parseTree);
    return {};
  }
  if (driver.dumpUnparse) {
    Unparse(llvm::outs(), parseTree, driver.encoding, true /*capitalize*/,
            options.features.IsEnabled(
                Fortran::common::LanguageFeature::BackslashEscapes));
    return {};
  }
  if (driver.syntaxOnly) {
    return {};
  }
  std::string unparsedPath{path};
  if (!driver.noReformat) {
    // Unparse with line directives into a temporary file.
    std::string tmpPath;
    {
      llvm::SmallVector<char, 32> tmp;
      llvm::sys::fs::createTemporaryFile("f18", "f90", tmp);
      tmpPath = llvm::StringRef(tmp.data(), tmp.size());
    }
    int fd = -1;
    if (std::error_code ec = llvm::sys::fs::openFileForWrite(
            tmpPath, fd, llvm::sys::fs::CD_CreateAlways,
            llvm::sys::fs::OF_Text)) {
      llvm::errs() << "f18-parse-demo: could not open " << tmpPath << ": "
                   << ec.message() << "\n";
      std::exit(EXIT_FAILURE);
    }
    filesToDelete.push_back(tmpPath);
    llvm::raw_fd_ostream tmpSource(fd, /*shouldClose*/ true);
    Unparse(tmpSource, parseTree, driver.encoding, true /*capitalize*/,
            options.features.IsEnabled(
                Fortran::common::LanguageFeature::BackslashEscapes));
    unparsedPath = tmpPath;
  }

  std::vector<llvm::StringRef> argv;
  argv.push_back(driver.fcArgs[0]);
  argv.push_back("-c");
  argv.push_back(unparsedPath);
  argv.push_back("-o");
  std::string objectPath;
  if (driver.compileOnly && !driver.outputPath.empty()) {
    objectPath = driver.outputPath;
  } else if (!driver.noReformat && unparsedPath == path) {
    objectPath = path + ".o";
  } else {
    auto dot{path.rfind('.')};
    if (dot != std::string::npos) {
      objectPath = path.substr(0, dot) + ".o";
    } else {
      objectPath = path + ".o";
    }
  }
  argv.push_back(objectPath);
  for (size_t i = 1; i < driver.fcArgs.size(); ++i) {
    argv.push_back(driver.fcArgs[i]);
  }
  Exec(argv, driver.verbose);
  if (driver.compileOnly) {
    return {};
  }
  if (driver.outputPath.empty()) {
    return objectPath;
  } else {
    return {};
  }
}

void Link(const std::vector<std::string> &relocatables,
          const DriverOptions &driver) {
  if (driver.verbose) {
    llvm::errs() << "f18-parse-demo: linking\n";
  }
  std::vector<llvm::StringRef> argv;
  argv.push_back(driver.fcArgs[0]);
  for (size_t i = 1; i < driver.fcArgs.size(); ++i) {
    argv.push_back(driver.fcArgs[i]);
  }
  for (auto &path : relocatables) {
    argv.push_back(path);
  }
  if (!driver.outputPath.empty()) {
    argv.push_back("-o");
    argv.push_back(driver.outputPath);
  }
  Exec(argv, driver.verbose);
}

int flang_external_builder_main(int argc, char *const argv[],
                                SgSourceFile *roseSourceFile) {
  atexit(CleanUpAtExit);

  // The SageTreeBuilder must be initialized before using it.
  Rose::builder::setSgSourceFile(roseSourceFile);

  // Initialize Flang command-line arguments
  DriverContext ctx{};
  int exitStatus = ParseFlangArgs(argc, argv, ctx);
  if (exitStatus != EXIT_SUCCESS)
    return exitStatus;

  if (!ctx.anyFiles) {
    ctx.driver.dumpUnparse = true;
    CompileFortran("-", ctx.options, ctx.driver);
    return exitStatus;
  }

  for (const auto &path : ctx.fortranSources) {
    std::string relo{CompileFortran(path, ctx.options, ctx.driver)};
    if (!ctx.driver.compileOnly && !relo.empty()) {
      ctx.relocatables.push_back(relo);
    }
  }

  for (const auto &path : ctx.otherSources) {
    std::string relo{CompileOtherLanguage(path, ctx.driver)};
    if (!ctx.driver.compileOnly && !relo.empty()) {
      ctx.relocatables.push_back(relo);
    }
  }

  if (!ctx.relocatables.empty()) {
    Link(ctx.relocatables, ctx.driver);
  }

  return exitStatus;
}
