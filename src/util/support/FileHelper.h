#ifndef ROSE_FileHelper_H
#define ROSE_FileHelper_H

// Prefer Rose/FileSystem.h for new callers using std::filesystem::path.

#include "Rose/FileSystem.h"

#include <filesystem>

#include <limits>

#include <string>

class FileHelper {
public:
  // This is initialized in src/frontend/SageIII/sage_support/sage_support.C,
  // not FileHelper.C
  static const std::string pathDelimiter;

  static void ensureParentFolderExists(const std::string &path) {
    ensureFolderExists(getParentFolder(path));
  }

  static void ensureFolderExists(const std::string &folder) {
    std::filesystem::path fsPath(folder);
    create_directories(fsPath);
  }

  static std::string concatenatePaths(const std::string &path1,
                                      const std::string &path2) {
    if (path1.size() == 0) {
      return path2;
    }
    if (path2.size() == 0) {
      return path1;
    }
    return path1 + pathDelimiter + path2;
  }

  static std::string getParentFolder(const std::string &aPath) {
    std::filesystem::path fsPath(aPath);
    return fsPath.parent_path().string();
  }

  static std::string getFileName(const std::string &aPath) {
    return Rose::FileSystem::toString(std::filesystem::path(aPath).filename());
  }

  static bool isAbsolutePath(const std::string &path) {
    return path.compare(0, 1, pathDelimiter) == 0;
  }

  // Expects both paths to be absolute.
  static bool areEquivalentPaths(const std::string &path1,
                                 const std::string &path2) {
    // Note: Do not use std::filesystem::equivalent since the compared paths
    // might not exist, which will cause an error.
    return normalizePath(path1).compare(normalizePath(path2)) == 0;
  }

  // Assumes that both arguments are absolute and normalized.
  // Argument toPath can be either a folder or a file.
  static std::string getRelativePath(const std::string &fromFolder,
                                     const std::string &toPath) {
    return Rose::FileSystem::toString(
        Rose::FileSystem::makeRelative(toPath, fromFolder));
  }

  static bool fileExists(const std::string &fullFileName) {
    return std::filesystem::exists(fullFileName);
  }

  static bool isNotEmptyFolder(const std::string &fullFolderName) {
    return std::filesystem::exists(fullFolderName) &&
           !std::filesystem::is_empty(fullFolderName);
  }

  static std::string normalizePath(const std::string &aPath) {
    // Generated output paths may not exist. Resolve their existing prefix,
    // while exposing real errors such as symlink loops and denied access.
    return std::filesystem::weakly_canonical(aPath).string();
  }

  static std::string normalizePathIfPossible(const std::string &path) {
    if (path.empty()) {
      return path;
    }
    if (!fileExists(path)) {
      return path;
    }
    return normalizePath(path);
  }

  // Include spellings are relative lexical paths. Resolving them against the
  // process working directory loses the parent components needed to construct
  // an output include search path.
  static std::string getTextualPart(const std::string &path) {
    const auto normalized = std::filesystem::path(path).lexically_normal();
    auto component = normalized.begin();
    while (component != normalized.end() &&
           (*component == "." || *component == "..")) {
      ++component;
    }
    std::filesystem::path result;
    for (; component != normalized.end(); ++component) {
      result /= *component;
    }
    return result.string();
  }

  static int countUpsToParentFolder(const std::string &path) {
    const auto normalized = std::filesystem::path(path).lexically_normal();
    int count = 0;
    for (const auto &component : normalized) {
      if (component != "..") {
        break;
      }
      ROSE_ASSERT(count < std::numeric_limits<int>::max());
      ++count;
    }
    return count;
  }
};

#endif
