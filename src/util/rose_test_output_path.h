#ifndef ROSE_TEST_OUTPUT_PATH_H
#define ROSE_TEST_OUTPUT_PATH_H

#include "rose_paths.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace Rose {
namespace TestOutput {
namespace detail {
inline std::filesystem::path normalizePath(const std::filesystem::path &path) {
  if (path.empty()) {
    return path;
  }

  std::error_code ec;
  std::filesystem::path absolute_path = path;
  if (!absolute_path.is_absolute()) {
    absolute_path = std::filesystem::absolute(absolute_path, ec);
    if (ec) {
      return path.lexically_normal();
    }
  }

  std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(absolute_path, ec);
  if (!ec) {
    return canonical_path;
  }

  return absolute_path.lexically_normal();
}

inline bool isWithinTree(const std::filesystem::path &root,
                         const std::filesystem::path &candidate) {
  if (root.empty() || candidate.empty()) {
    return false;
  }

  std::filesystem::path normalized_root = normalizePath(root);
  std::filesystem::path normalized_candidate = normalizePath(candidate);

  auto root_it = normalized_root.begin();
  auto candidate_it = normalized_candidate.begin();
  for (; root_it != normalized_root.end() &&
         candidate_it != normalized_candidate.end();
       ++root_it, ++candidate_it) {
    if (*root_it != *candidate_it) {
      return false;
    }
  }

  return root_it == normalized_root.end();
}
} // namespace detail

inline std::string resolvePath(const std::string &filename,
                               const std::string &output_dir_override) {
  if (filename.empty()) {
    return filename;
  }

  std::string output_dir_storage = output_dir_override;
  if (output_dir_storage.empty()) {
    const char *output_dir = std::getenv("ROSE_TEST_OUTPUT_DIR");
    if (output_dir != nullptr && output_dir[0] != '\0') {
      output_dir_storage = output_dir;
    }
  }

  if (output_dir_storage.empty()) {
    return filename;
  }

  std::filesystem::path output_dir_path =
      detail::normalizePath(std::filesystem::path(output_dir_storage));
  std::error_code ec;
  std::filesystem::create_directories(output_dir_path, ec);
  if (ec) {
    return filename;
  }

  std::filesystem::path resolved_path(filename);
  if (!resolved_path.is_absolute()) {
    resolved_path = output_dir_path / resolved_path;
  } else {
    resolved_path = detail::normalizePath(resolved_path);

    if (!detail::isWithinTree(output_dir_path, resolved_path) &&
        !detail::isWithinTree(std::filesystem::path(ROSE_BUILD_TREE),
                              resolved_path) &&
        detail::isWithinTree(std::filesystem::path(ROSE_SOURCE_TREE),
                             resolved_path)) {
      std::filesystem::path relative_to_source =
          resolved_path.lexically_relative(
              detail::normalizePath(std::filesystem::path(ROSE_SOURCE_TREE)));
      if (!relative_to_source.empty()) {
        resolved_path = output_dir_path / relative_to_source;
      } else {
        resolved_path = output_dir_path / resolved_path.filename();
      }
    }
  }

  std::filesystem::path parent_path = resolved_path.parent_path();
  if (!parent_path.empty()) {
    ec.clear();
    std::filesystem::create_directories(parent_path, ec);
    if (ec) {
      return filename;
    }
  }

  return resolved_path.string();
}

inline std::string resolvePath(const std::string &filename) {
  return resolvePath(filename, std::string());
}
} // namespace TestOutput
} // namespace Rose

#endif
