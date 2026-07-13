/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "rose_paths.h"
#include "sage3basic.h"
#include "sage_support/utility_functions.h"

#include <emscripten/bind.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

#if !defined(REX_WASM_CLANG_C_DRIVER) ||                                       \
    !defined(REX_WASM_CLANG_CXX_DRIVER) ||                                     \
    !defined(REX_WASM_CLANG_RESOURCE_DIR)
#error "REX WASM requires exact virtual Clang driver and resource paths"
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

bool isCxxLanguage(InputLanguage language) {
  return language == InputLanguage::Cxx;
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

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void writeFile(const std::filesystem::path &path, const std::string &content) {
  std::ofstream output(path, std::ios::binary);
  output << content;
  if (!output) {
    std::fprintf(stderr, "REX_WASM_INVARIANT[file-write]: cannot write '%s'\n",
                 path.c_str());
    ROSE_ABORT();
  }
}

void ensureVirtualClangDriver(const std::filesystem::path &path) {
  if (!path.is_absolute()) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[clang-driver-path]: configured driver "
                 "path is not absolute: '%s'\n",
                 path.c_str());
    ROSE_ABORT();
  }
  if (std::filesystem::is_symlink(path) ||
      (std::filesystem::exists(path) &&
       !std::filesystem::is_regular_file(path))) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[clang-driver-path]: configured driver "
                 "path is not a regular file: '%s'\n",
                 path.c_str());
    ROSE_ABORT();
  }
  if (!std::filesystem::exists(path)) {
    writeFile(path, "REX in-process Clang driver contract\n");
  }

  std::error_code error;
  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add, error);
  if (error) {
    std::fprintf(stderr,
                 "REX_WASM_INVARIANT[clang-driver-path]: cannot make '%s' "
                 "executable: %s\n",
                 path.c_str(), error.message().c_str());
    ROSE_ABORT();
  }
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

std::string sanitizeFileName(const std::string &requested) {
  std::string name = std::filesystem::path(requested).filename().string();
  if (name.empty() || name == "." || name == "..") {
    name = "input.c";
  }

  for (char &ch : name) {
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
                    ch == '.';
    if (!ok) {
      ch = '_';
    }
  }

  InputLanguage language;
  if (!classifyInputLanguage(name, language)) {
    name += ".c";
  }

  return name;
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
    files.push_back({name, readFile(entry.path())});
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
  if (!std::filesystem::exists("/rex/bin/rex-wasm")) {
    writeFile("/rex/bin/rex-wasm", "");
  }
  ensureVirtualClangDriver(REX_WASM_CLANG_C_DRIVER);
  ensureVirtualClangDriver(REX_WASM_CLANG_CXX_DRIVER);
  assertVirtualClangResourceContract();
}

std::vector<std::string> buildArgs(const std::string &mode,
                                   const std::string &inputName) {
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

  InputLanguage language;
  const bool classified = classifyInputLanguage(inputName, language);
  ROSE_ASSERT(classified);

  if (isCxxLanguage(language)) {
    args.push_back("-std=c++17");
  } else {
    args.push_back("-std=c11");
  }

  if (mode == "omp_ast") {
    args.push_back("--rex-omp-ast-only");
  } else if (mode == "omp_lowering") {
    args.push_back("--rex-omp-lowering");
  }

  args.push_back("-c");
  args.push_back(inputName);
  return args;
}

int runRexDriver(const std::vector<std::string> &args) {
  SgProject *project = frontend(args);
  int frontendStatus = frontendExitStatus(project);
  if (frontendStatus != 0) {
    return frontendStatus;
  }
  return backend(project);
}

std::string runWithCapturedLog(const std::vector<std::string> &args,
                               int &exitCode) {
  const int savedStdout = dup(STDOUT_FILENO);
  const int savedStderr = dup(STDERR_FILENO);
  FILE *logFile = std::freopen("rex.log", "w", stdout);
  if (logFile == nullptr) {
    exitCode = 127;
    return std::string("error: failed to open log: ") + std::strerror(errno);
  }
  dup2(STDOUT_FILENO, STDERR_FILENO);

  exitCode = runRexDriver(args);

  std::fflush(stdout);
  std::fflush(stderr);
  if (savedStdout >= 0) {
    dup2(savedStdout, STDOUT_FILENO);
    close(savedStdout);
  }
  if (savedStderr >= 0) {
    dup2(savedStderr, STDERR_FILENO);
    close(savedStderr);
  }

  return readFile("rex.log");
}

std::string runRex(const std::string &source, const std::string &filename,
                   const std::string &mode) {
  if (mode != "plain" && mode != "omp_ast" && mode != "omp_lowering") {
    return resultJson(false, mode, 2, {},
                      "error: unsupported mode '" + mode + "'");
  }

  ensureRuntimeRoots();

  static int runCounter = 0;
  const std::filesystem::path runDir =
      std::filesystem::path("/work") / ("run-" + std::to_string(++runCounter));
  std::filesystem::remove_all(runDir);
  std::filesystem::create_directories(runDir);

  const std::string inputName = sanitizeFileName(filename);
  writeFile(runDir / inputName, source);

  const std::filesystem::path oldCwd = std::filesystem::current_path();
  std::filesystem::current_path(runDir);

  int exitCode = 0;
  const std::string log =
      runWithCapturedLog(buildArgs(mode, inputName), exitCode);
  std::vector<GeneratedFile> files = collectGeneratedFiles(runDir, inputName);

  std::filesystem::current_path(oldCwd);

  const bool ok = exitCode == 0;
  return resultJson(ok, mode, exitCode, files, log);
}

} // namespace

EMSCRIPTEN_BINDINGS(rex_wasm_module) {
  emscripten::function("runRex", &runRex);
}
