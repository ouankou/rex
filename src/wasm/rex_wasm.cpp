/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "Rose/FileSystem.h"
#include "rose_paths.h"
#include "sage3basic.h"
#include "sage_support/utility_functions.h"

#include <emscripten/bind.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

#if !defined(REX_WASM_CLANG_RESOURCE_DIR)
#error "REX WASM requires the configured Clang resource path"
#endif

struct GeneratedFile {
  std::string name;
  std::string content;
};

bool hasSuffix(const std::string &text, const std::string &suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool hasAnySuffix(const std::string &text,
                  std::initializer_list<const char *> suffixes) {
  for (const char *suffix : suffixes) {
    if (hasSuffix(text, suffix)) {
      return true;
    }
  }
  return false;
}

enum class InputLanguage { C, Cxx };

struct InputFile {
  std::string name;
  InputLanguage language;
};

bool classifyInputLanguage(const std::string &name, InputLanguage &language) {
  if (hasAnySuffix(name, {".c"})) {
    language = InputLanguage::C;
    return true;
  }
  if (hasAnySuffix(name,
                   {".C", ".cc", ".cp", ".c++", ".cpp", ".cxx", ".CPP"})) {
    language = InputLanguage::Cxx;
    return true;
  }
  return false;
}

std::string jsonEscape(const std::string &input) {
  std::ostringstream out;
  for (unsigned char ch : input) {
    switch (ch) {
    case '\\':
      out << "\\\\";
      break;
    case '"':
      out << "\\\"";
      break;
    case '\b':
      out << "\\b";
      break;
    case '\f':
      out << "\\f";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (ch < 0x20) {
        const char *digits = "0123456789abcdef";
        out << "\\u00" << digits[(ch >> 4) & 0xf] << digits[ch & 0xf];
      } else {
        out << static_cast<char>(ch);
      }
      break;
    }
  }
  return out.str();
}

void assertVirtualClangResourceContract() {
  const std::filesystem::path resourceDir(REX_WASM_CLANG_RESOURCE_DIR);
  if (!resourceDir.is_absolute() || std::filesystem::is_symlink(resourceDir) ||
      !std::filesystem::is_directory(resourceDir)) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[clang-resource-dir]: configured resource "
                 "directory is missing: '%s'\n",
                 resourceDir.c_str());
    ROSE_ABORT();
  }
  for (const char *requiredHeader : {"stddef.h", "omp.h"}) {
    const std::filesystem::path header =
        resourceDir / "include" / requiredHeader;
    if (!std::filesystem::is_regular_file(header)) {
      std::fprintf(stderr,
                   "REX_WASM_INVARIANT[clang-resource-dir]: required header "
                   "is missing: '%s'\n",
                   header.c_str());
      ROSE_ABORT();
    }
  }
}

void ensureDirectoryAlias(const std::filesystem::path &target,
                          const std::filesystem::path &linkPath) {
  if (target.empty() || linkPath.empty() ||
      !std::filesystem::is_directory(target)) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[include-staging-alias]: invalid target "
                 "or alias path ('%s', '%s')\n",
                 target.c_str(), linkPath.c_str());
    ROSE_ABORT();
  }

  std::filesystem::create_directories(linkPath.parent_path());
  if (!std::filesystem::exists(linkPath) &&
      !std::filesystem::is_symlink(linkPath) &&
      symlink(target.c_str(), linkPath.c_str()) != 0) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[include-staging-alias]: cannot create "
                 "exact alias '%s' -> '%s': %s\n",
                 linkPath.c_str(), target.c_str(), std::strerror(errno));
    ROSE_ABORT();
  }

  std::error_code targetError;
  std::error_code aliasError;
  const std::filesystem::path canonicalTarget =
      std::filesystem::canonical(target, targetError);
  const std::filesystem::path canonicalAlias =
      std::filesystem::canonical(linkPath, aliasError);
  if (targetError || aliasError || canonicalTarget != canonicalAlias) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[include-staging-alias]: alias '%s' does "
                 "not resolve exactly to '%s'\n",
                 linkPath.c_str(), target.c_str());
    ROSE_ABORT();
  }
}

std::optional<InputFile> prepareInputFile(const std::string &requested) {
  if (requested.find('\0') != std::string::npos) {
    return std::nullopt;
  }
  std::string name = std::filesystem::path(requested).filename().string();
  InputLanguage language;
  if (!classifyInputLanguage(name, language)) {
    return std::nullopt;
  }

  for (char &ch : name) {
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
                    ch == '.' || ch == '+';
    if (!ok) {
      ch = '_';
    }
  }

  return InputFile{std::move(name), language};
}

std::string resultJson(bool ok, const std::string &mode, int exitCode,
                       const std::vector<GeneratedFile> &files,
                       const std::string &log) {
  std::ostringstream out;
  out << "{\"ok\":" << (ok ? "true" : "false") << ",\"mode\":\""
      << jsonEscape(mode) << "\",\"exitCode\":" << exitCode << ",\"files\":[";
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (i != 0) {
      out << ',';
    }
    out << "{\"name\":\"" << jsonEscape(files[i].name) << "\",\"content\":\""
        << jsonEscape(files[i].content) << "\"}";
  }
  out << "],\"log\":\"" << jsonEscape(log) << "\"}";
  return out.str();
}

std::vector<GeneratedFile>
collectGeneratedFiles(const std::filesystem::path &runDir,
                      const std::string &inputName) {
  std::vector<GeneratedFile> files;
  for (const auto &entry : std::filesystem::directory_iterator(runDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string name = entry.path().filename().string();
    if (name == inputName || name == "rex.log") {
      continue;
    }
    if (name.rfind("rose_", 0) != 0 && name.rfind("rex_lib_", 0) != 0) {
      continue;
    }
    files.push_back(
        {name, Rose::FileSystem::readFile<std::string>(entry.path())});
  }
  std::sort(files.begin(), files.end(),
            [](const GeneratedFile &lhs, const GeneratedFile &rhs) {
              const bool lhsRose = lhs.name.rfind("rose_", 0) == 0;
              const bool rhsRose = rhs.name.rfind("rose_", 0) == 0;
              if (lhsRose != rhsRose) {
                return lhsRose;
              }
              return lhs.name < rhs.name;
            });
  return files;
}

void ensureRuntimeRoots() {
  std::filesystem::create_directories("/rex/bin");
  std::filesystem::create_directories("/work");
  ensureDirectoryAlias("/rex/include-staging",
                       ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR);
  assertVirtualClangResourceContract();
}

std::vector<std::string> buildArgs(const std::string &mode,
                                   const InputFile &input) {
  std::vector<std::string> args = {
      "/rex/bin/rex-wasm",
      "-target",
      "wasm32-unknown-emscripten",
      std::string("-resource-dir=") + REX_WASM_CLANG_RESOURCE_DIR,
      "-rose:skipfinalCompileStep",
      "-rose:verbose",
      "0",
  };

#ifdef REX_WASM_HAS_SYSROOT
  args.push_back("--sysroot=/emscripten/sysroot");
#endif

  args.push_back("-x");
  if (input.language == InputLanguage::Cxx) {
    args.push_back("c++");
    args.push_back("-std=c++17");
  } else {
    args.push_back("c");
    args.push_back("-std=c11");
  }

  if (mode == "omp_ast") {
    args.push_back("--rex-omp-ast-only");
  } else if (mode == "omp_lowering") {
    args.push_back("--rex-omp-lowering");
  }

  args.push_back("-c");
  args.push_back("./" + input.name);
  return args;
}

struct ProjectDeleter {
  void operator()(SgProject *project) const {
    SageInterface::deleteAST(project);
  }
};

int runRexDriver(const std::vector<std::string> &args) {
  std::unique_ptr<SgProject, ProjectDeleter> project(frontend(args));
  const int frontendStatus = frontendExitStatus(project.get());
  if (frontendStatus != 0) {
    return frontendStatus;
  }
  return backend(project.get());
}

int requireIo(int result, const char *operation) {
  if (result < 0) {
    std::fprintf(stderr, "REX_WASM_INVARIANT[log-io]: %s: %s\n", operation,
                 std::strerror(errno));
    ROSE_ABORT();
  }
  return result;
}

class CapturedLog {
  int savedStdout_;
  int savedStderr_;

public:
  CapturedLog() {
    requireIo(std::fflush(stdout), "flush stdout");
    requireIo(std::fflush(stderr), "flush stderr");
    savedStdout_ = requireIo(dup(STDOUT_FILENO), "save stdout");
    savedStderr_ = requireIo(dup(STDERR_FILENO), "save stderr");
    const int log = requireIo(
        open("rex.log", O_WRONLY | O_CREAT | O_TRUNC, 0666), "open log");
    requireIo(dup2(log, STDOUT_FILENO), "capture stdout");
    requireIo(dup2(log, STDERR_FILENO), "capture stderr");
    requireIo(close(log), "close log descriptor");
  }

  ~CapturedLog() {
    requireIo(std::fflush(stdout), "flush captured stdout");
    requireIo(std::fflush(stderr), "flush captured stderr");
    requireIo(dup2(savedStdout_, STDOUT_FILENO), "restore stdout");
    requireIo(dup2(savedStderr_, STDERR_FILENO), "restore stderr");
    requireIo(close(savedStdout_), "close saved stdout");
    requireIo(close(savedStderr_), "close saved stderr");
  }

  CapturedLog(const CapturedLog &) = delete;
  CapturedLog &operator=(const CapturedLog &) = delete;
};

class RunDirectory {
  std::filesystem::path previous_;
  std::filesystem::path directory_;

public:
  explicit RunDirectory(const std::filesystem::path &directory)
      : previous_(std::filesystem::current_path()), directory_(directory) {
    std::filesystem::create_directories(directory_);
    std::filesystem::current_path(directory_);
  }

  ~RunDirectory() {
    std::filesystem::current_path(previous_);
    std::filesystem::remove_all(directory_);
  }

  RunDirectory(const RunDirectory &) = delete;
  RunDirectory &operator=(const RunDirectory &) = delete;
};

std::string runWithCapturedLog(const std::vector<std::string> &args,
                               int &exitCode) {
  {
    CapturedLog capture;
    exitCode = runRexDriver(args);
  }
  return Rose::FileSystem::readFile<std::string>("rex.log");
}

std::string runRex(const std::string &source, const std::string &filename,
                   const std::string &mode) {
  if (mode != "plain" && mode != "omp_ast" && mode != "omp_lowering") {
    return resultJson(false, mode, 2, {},
                      "error: unsupported mode '" + mode + "'");
  }

  const std::optional<InputFile> input = prepareInputFile(filename);
  if (!input) {
    return resultJson(false, mode, 2, {},
                      "error: unsupported source filename '" + filename +
                          "'; a C or C++ extension is required");
  }

  ensureRuntimeRoots();

  static int runCounter = 0;
  const std::filesystem::path runDir =
      std::filesystem::path("/work") / ("run-" + std::to_string(++runCounter));
  RunDirectory directory(runDir);

  Rose::FileSystem::writeFile(runDir / input->name, source);

  int exitCode = 0;
  const std::string log = runWithCapturedLog(buildArgs(mode, *input), exitCode);
  std::vector<GeneratedFile> files = collectGeneratedFiles(runDir, input->name);

  const bool ok = exitCode == 0;
  return resultJson(ok, mode, exitCode, files, log);
}

} // namespace

EMSCRIPTEN_BINDINGS(rex_wasm_module) {
  emscripten::function("runRex", &runRex);
}
