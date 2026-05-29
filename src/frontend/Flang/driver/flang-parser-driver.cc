//===-- src/frontend/Flang/driver/flang-parser-driver.cc ------------------===//
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
// environment variable (defaulting to flang).  The Fortran preprocessor is
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

#include "flang-parser-args.h"

#include "../sage-build.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Path.h>
#include <system_error>

std::vector<std::string> filesToDelete;
std::string includeTempDir;

namespace {
struct CompileResult {
  bool succeeded{true};
  bool hadErrors{false};
  bool hadFatalErrors{false};
  bool astBuilt{false};
  std::string relocatablePath;
};

bool HasErrorDiagnostics(Fortran::parser::Messages &messages) {
  for (const auto &message : messages.messages()) {
    if (message.severity() == Fortran::parser::Severity::Error ||
        message.severity() == Fortran::parser::Severity::Todo) {
      return true;
    }
  }
  return false;
}

bool IsIncludeDirective(const std::string &line, size_t &headerEnd,
                        size_t &trailingStart) {
  size_t pos = line.find_first_not_of(" \t");
  if (pos == std::string::npos || line[pos] != '#') {
    return false;
  }
  pos = line.find_first_not_of(" \t", pos + 1);
  if (pos == std::string::npos) {
    return false;
  }
  constexpr const char *kInclude = "include";
  constexpr size_t kIncludeLen = 7;
  if (line.compare(pos, kIncludeLen, kInclude) != 0) {
    return false;
  }
  pos += kIncludeLen;
  if (pos < line.size() &&
      std::isalpha(static_cast<unsigned char>(line[pos]))) {
    return false;
  }
  pos = line.find_first_not_of(" \t", pos);
  if (pos == std::string::npos) {
    return false;
  }
  char opener = line[pos];
  char closer = '\0';
  if (opener == '<') {
    closer = '>';
  } else if (opener == '"') {
    closer = '"';
  } else {
    return false;
  }
  size_t end = line.find(closer, pos + 1);
  if (end == std::string::npos) {
    return false;
  }
  headerEnd = end + 1;
  trailingStart = line.find_first_not_of(" \t", headerEnd);
  return trailingStart != std::string::npos;
}

bool ShouldSplitIncludeLine(const std::string &line) {
  size_t headerEnd = 0;
  size_t trailingStart = 0;
  if (!IsIncludeDirective(line, headerEnd, trailingStart)) {
    return false;
  }
  const char ch = line[trailingStart];
  if (ch == '!') {
    return false;
  }
  if (ch == '/' && trailingStart + 1 < line.size()) {
    const char next = line[trailingStart + 1];
    if (next == '/' || next == '*') {
      return false;
    }
  }
  return true;
}

bool WriteIncludeFixedCopy(const std::string &path, std::string &fixedPath) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return false;
  }

  std::string output;
  bool changed = false;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (ShouldSplitIncludeLine(line)) {
      size_t headerEnd = 0;
      size_t trailingStart = 0;
      if (IsIncludeDirective(line, headerEnd, trailingStart)) {
        output.append(line.substr(0, headerEnd));
        output.push_back('\n');
        output.append(line.substr(trailingStart));
        output.push_back('\n');
        changed = true;
        continue;
      }
    }
    output.append(line);
    output.push_back('\n');
  }

  if (!changed) {
    return false;
  }

  std::filesystem::path dir;
  if (!includeTempDir.empty()) {
    dir = includeTempDir;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
      llvm::errs() << "f18-parse-demo: could not create include temp dir "
                   << dir.string() << ": " << ec.message() << "\n";
      std::exit(EXIT_FAILURE);
    }
  } else {
    dir = std::filesystem::path(path).parent_path();
    if (dir.empty()) {
      dir = ".";
    }
  }
  llvm::SmallString<256> pattern(dir.string());
  llvm::sys::path::append(pattern, "rose-f18-include-%%%%.F90");
  int fd = -1;
  llvm::SmallString<256> tmpPath;
  if (std::error_code ec =
          llvm::sys::fs::createUniqueFile(pattern, fd, tmpPath)) {
    llvm::errs() << "f18-parse-demo: could not create temp file in "
                 << dir.string() << ": " << ec.message() << "\n";
    std::exit(EXIT_FAILURE);
  }
  llvm::raw_fd_ostream tmpOut(fd, /*shouldClose*/ true);
  tmpOut << output;
  fixedPath = llvm::StringRef(tmpPath.data(), tmpPath.size());
  filesToDelete.push_back(fixedPath);
  return true;
}
} // namespace

void flang_parser_driver_set_include_tmpdir(const char *path) {
  if (path == nullptr || *path == '\0') {
    includeTempDir.clear();
    return;
  }
  includeTempDir = path;
}

static bool IsFixedFormFile(const std::string &path) {
  auto dot = path.rfind('.');
  if (dot == std::string::npos) {
    return false;
  }
  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  static const std::set<std::string> fixedExt{"f",   "for", "ftn",
                                              "f66", "f77", "ff"};
  static const std::set<std::string> freeExt{"f90",  "f95",   "f03",   "f08",
                                             "f18",  "f2003", "f2008", "ff90",
                                             "ff95", "ff03",  "ff08",  "ff18"};
  if (fixedExt.find(ext) != fixedExt.end()) {
    return true;
  }
  if (freeExt.find(ext) != freeExt.end()) {
    return false;
  }
  return false;
}

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

CompileResult CompileOtherLanguage(const std::string &path,
                                   const DriverOptions &driver) {
  CompileResult result;
  if (driver.verbose) {
    llvm::errs() << "f18-parse-demo: not compiling \"" << path
                 << "\" because it's not a Fortran source file\n";
  }
  return result;
}

CompileResult CompileFortran(const std::string &path,
                             const Fortran::parser::Options &optionsIn,
                             const DriverOptions &driver) {
  CompileResult result;

  // TODO: should be able to use -o to set object file name
  // TODO: support Unicode names in filesystem?
  if (driver.verbose) {
    llvm::errs() << "Compiling \"" << path << "\"\n";
  }

  Fortran::parser::Options options{optionsIn};
  options.searchDirectories = driver.searchDirectories;
  options.encoding = driver.encoding;
  options.expandIncludeLinesInPreprocessedOutput = driver.lineDirectives;
  if (!driver.forcedForm && path != "-") {
    options.isFixedForm = IsFixedFormFile(path);
  }

  Fortran::parser::AllSources allSources;
  allSources.set_encoding(driver.encoding);
  allSources.setShowColors(options.showColors);
  Fortran::parser::AllCookedSources allCookedSources{allSources};
  Fortran::parser::Parsing parsing{allCookedSources};

  std::string prescanPath = path;
  std::string fixedPath;
  if (WriteIncludeFixedCopy(path, fixedPath)) {
    prescanPath = fixedPath;
  }

  parsing.Prescan(prescanPath, options);

  auto &messages = parsing.messages();
  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  const bool hasPrescanErrors = HasErrorDiagnostics(messages);
  const bool hasPrescanFatalErrors = messages.AnyFatalError();
  if (hasPrescanErrors || hasPrescanFatalErrors) {
    result.succeeded = false;
    result.hadErrors = hasPrescanErrors || hasPrescanFatalErrors;
    result.hadFatalErrors = hasPrescanFatalErrors;
    return result;
  }

  if (options.prescanAndReformat) {
    parsing.EmitPreprocessedSource(llvm::outs(), driver.lineDirectives);
    return result;
  }

  parsing.Parse(llvm::nulls());

  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  const bool hasParseErrors = HasErrorDiagnostics(messages);
  const bool hasParseFatalErrors = messages.AnyFatalError();
  if (hasParseErrors || hasParseFatalErrors) {
    result.succeeded = false;
    result.hadErrors = hasParseErrors || hasParseFatalErrors;
    result.hadFatalErrors = hasParseFatalErrors;
    return result;
  }

  auto &parseTreeOpt = parsing.parseTree();
  if (!parseTreeOpt) {
    result.succeeded = false;
    result.hadErrors = true;
    return result;
  }
  auto &parseTree{*parseTreeOpt};

  // Transform the parse tree using Rose::builder
  if (driver.externalBuilder) {
    auto start{CPUseconds()};
    Rose::builder::Build(parseTree, allCookedSources);
    result.astBuilt = true;
    auto stop{CPUseconds()};
    if (canTime) {
      llvm::outs() << "Rose::Build time for " << path << ": " << (stop - start)
                   << " CPU milliseconds\n";
    }
  }

  if (driver.dumpProvenance) {
    parsing.DumpProvenance(llvm::outs());
    return result;
  }
  if (driver.dumpParseTree) {
    Fortran::parser::DumpTree(llvm::outs(), parseTree);
    return result;
  }
  if (driver.dumpUnparse) {
    Unparse(llvm::outs(), parseTree, driver.langOpts, driver.encoding,
            true /*capitalize*/,
            options.features.IsEnabled(
                Fortran::common::LanguageFeature::BackslashEscapes));
    return result;
  }
  if (driver.syntaxOnly) {
    return result;
  }
  if (driver.externalBuilder) {
    // ROSE only needs the parse tree; avoid invoking the external compiler.
    return result;
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
    Unparse(tmpSource, parseTree, driver.langOpts, driver.encoding,
            true /*capitalize*/,
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
    return result;
  }
  if (driver.outputPath.empty()) {
    result.relocatablePath = objectPath;
    return result;
  } else {
    return result;
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

int flang_parser_driver_main(int argc, char *const argv[],
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
    CompileResult result = CompileFortran("-", ctx.options, ctx.driver);
    return result.succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  bool hadFrontendErrors = false;

  for (const auto &path : ctx.fortranSources) {
    CompileResult result = CompileFortran(path, ctx.options, ctx.driver);
    if (!result.succeeded) {
      hadFrontendErrors = true;
      continue;
    }
    if (!ctx.driver.compileOnly && !result.relocatablePath.empty()) {
      ctx.relocatables.push_back(result.relocatablePath);
    }
  }

  for (const auto &path : ctx.otherSources) {
    CompileResult result = CompileOtherLanguage(path, ctx.driver);
    if (!result.succeeded) {
      hadFrontendErrors = true;
      continue;
    }
    if (!ctx.driver.compileOnly && !result.relocatablePath.empty()) {
      ctx.relocatables.push_back(result.relocatablePath);
    }
  }

  if (hadFrontendErrors) {
    return EXIT_FAILURE;
  }

  if (!ctx.relocatables.empty()) {
    Link(ctx.relocatables, ctx.driver);
  }

  return EXIT_SUCCESS;
}
