#include "Rose/FileSystem.h"

#include <cerrno>

#include <chrono>

#include <filesystem>

#include <fstream>

#include <random>

#include <regex>

#include <set>

#include <system_error>

namespace Rose {
namespace FileSystem {

const char *tempNamePattern = "rose-%%%%%%%-%%%%%%%";

bool baseNameMatches::operator()(const Path &path) {
  return std::regex_match(path.filename().string(), re_);
}

bool isExisting(const Path &path) { return std::filesystem::exists(path); }

bool isFile(const Path &path) { return std::filesystem::is_regular_file(path); }

bool isDirectory(const Path &path) {
  return std::filesystem::is_directory(path);
}

bool isSymbolicLink(const Path &path) {
  return std::filesystem::is_symlink(path);
}

bool isNotSymbolicLink(const Path &path) {
  return !std::filesystem::is_symlink(path);
}

Path createTemporaryDirectory() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<unsigned long long> dis;

  const Path tempBase = std::filesystem::temp_directory_path();
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string dirNameStr =
        "rose-" + std::to_string(now) + "-" + std::to_string(dis(gen));
    const Path dirName = tempBase / dirNameStr;

    std::error_code ec;
    if (std::filesystem::create_directory(dirName, ec)) {
      return dirName;
    }
    if (ec) {
      throw std::filesystem::filesystem_error(
          "failed to create temporary directory", dirName, ec);
    }
  }

  throw std::filesystem::filesystem_error(
      "failed to create unique temporary directory", tempBase,
      std::make_error_code(std::errc::file_exists));
}

Path makeNormal(const Path &path) {
  std::vector<Path> components;
  for (std::filesystem::path::const_iterator i = path.begin(); i != path.end();
       ++i) {
    if (0 == i->string().compare("..") && !components.empty()) {
      components.pop_back();
    } else if (0 != i->string().compare(".")) {
      components.push_back(*i);
    }
  }
  Path result;
  for (const Path &component : components)
    result /= component;
  return result;
}

Path makeAbsolute(const Path &path, const Path &root) {
  return makeNormal(path.is_absolute() ? path : absolute(root / path));
}

Path makeRelative(const Path &path_, const Path &root_) {
  Path path = makeAbsolute(path_);
  Path root = makeAbsolute(root_);

  std::filesystem::path::const_iterator rootIter = root.begin();
  std::filesystem::path::const_iterator pathIter = path.begin();

  // Skip past common prefix
  while (rootIter != root.end() && pathIter != path.end() &&
         *rootIter == *pathIter) {
    ++rootIter;
    ++pathIter;
  }

  // Return value must back out of remaining A components
  Path retval;
  while (rootIter != root.end()) {
    if (*rootIter++ != ".")
      retval /= "..";
  }

  // Append path components
  while (pathIter != path.end())
    retval /= *pathIter++;
  return retval;
}

std::vector<Path> findNames(const Path &root) {
  return findNames(root, isExisting);
}

std::vector<Path> findNamesRecursively(const Path &root) {
  return findNamesRecursively(root, isExisting, isDirectory);
}

void copyFile(const Path &src, const Path &dst) {
  // Use stream I/O here for portability across toolchains and filesystem
  // implementations. Use path::string rather than path::native in order to
  // support Filesystem version 2.
  std::ifstream in(src.string().c_str(), std::ios::binary);
  std::ofstream out(dst.string().c_str(), std::ios::binary);
  out << in.rdbuf();
  if (in.fail()) {
    throw std::filesystem::filesystem_error(
        "read failed", src, std::error_code(errno, std::system_category()));
  }
  if (out.fail()) {
    throw std::filesystem::filesystem_error(
        "write failed", dst, std::error_code(errno, std::system_category()));
  }
}

// Copies files to dstDir so that their name relative to dstDir is the same as
// their name relative to root
void copyFiles(const std::vector<Path> &fileNames, const Path &root,
               const Path &dstDir) {
  std::set<Path> dirs;
  for (const Path &fileName : fileNames) {
    Path dirName = dstDir / makeRelative(fileName.parent_path(), root);
    if (dirs.insert(dirName).second)
      std::filesystem::create_directories(dirName);
    Path outputName = dirName / fileName.filename();
    copyFile(fileName, outputName);
  }
}

std::vector<Path> findRoseFilesRecursively(const Path &root) {
  return findNamesRecursively(root, baseNameMatches(std::regex("rose_.*")),
                              isDirectory);
}

// Don't use this if you can help it!
std::string toString(const Path &path) { return path.generic_string(); }

} // namespace FileSystem
} // namespace Rose
