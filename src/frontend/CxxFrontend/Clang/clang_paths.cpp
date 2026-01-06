#include "clang_paths.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ROSE_ASSERT.h"
#include "rose_config.h"
#include "rose_paths.h"

#ifdef HAVE_DLADDR
#include <dlfcn.h>
#endif

namespace {
using std::filesystem::path;

bool dir_exists(const path &dir) {
  std::error_code ec;
  return std::filesystem::exists(dir, ec) &&
         std::filesystem::is_directory(dir, ec);
}

bool file_exists(const path &file) {
  std::error_code ec;
  return std::filesystem::exists(file, ec) &&
         std::filesystem::is_regular_file(file, ec);
}

std::string with_trailing_slash(const path &dir) {
  std::string value = dir.string();
  if (!value.empty() && value.back() != '/') {
    value.push_back('/');
  }
  return value;
}

std::vector<std::string> split_path_env(const char *value) {
  std::vector<std::string> parts;
  if (!value || *value == '\0') {
    return parts;
  }
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ':')) {
    if (item.empty()) {
      item = ".";
    }
    parts.push_back(item);
  }
  return parts;
}

std::optional<path> resolve_executable_path(const char *argv0) {
  if (!argv0 || *argv0 == '\0') {
    return std::nullopt;
  }

  path arg_path(argv0);
  std::error_code ec;
  if (arg_path.has_parent_path()) {
    path abs_path = std::filesystem::absolute(arg_path, ec);
    if (ec) {
      return std::nullopt;
    }
    path canonical = std::filesystem::weakly_canonical(abs_path, ec);
    if (!ec) {
      std::error_code exists_ec;
      if (std::filesystem::exists(canonical, exists_ec) && !exists_ec) {
        return canonical;
      }
    }

    ec.clear();
    if (std::filesystem::exists(abs_path, ec) && !ec) {
      return abs_path;
    }
    return std::nullopt;
  }

  const char *path_env = std::getenv("PATH");
  for (const auto &dir : split_path_env(path_env)) {
    path candidate = path(dir) / arg_path;
    if (std::filesystem::exists(candidate, ec) &&
        std::filesystem::is_regular_file(candidate, ec)) {
      path canonical = std::filesystem::weakly_canonical(candidate, ec);
      if (!ec && std::filesystem::exists(canonical, ec)) {
        return canonical;
      }
      return candidate;
    }
  }
  return std::nullopt;
}

std::optional<path> prefix_from_executable(const char *argv0) {
  auto exe_path = resolve_executable_path(argv0);
  if (!exe_path) {
    return std::nullopt;
  }
  path bin_dir = exe_path->parent_path();
  if (bin_dir.empty() || !bin_dir.has_parent_path()) {
    return std::nullopt;
  }
  return bin_dir.parent_path();
}

std::optional<path> prefix_from_shared_library() {
#ifdef HAVE_DLADDR
  Dl_info info;
  info.dli_fname = "";
  if (dladdr(reinterpret_cast<void *>(&resolveRoseClangPaths), &info) == 0) {
    return std::nullopt;
  }
  if (!info.dli_fname || *info.dli_fname == '\0') {
    return std::nullopt;
  }
  path lib_path(info.dli_fname);
  std::error_code ec;
  path abs_path = std::filesystem::absolute(lib_path, ec);
  if (!ec) {
    lib_path = abs_path;
  }

  ec.clear();
  path canonical = std::filesystem::weakly_canonical(lib_path, ec);
  if (!ec) {
    std::error_code exists_ec;
    if (std::filesystem::exists(canonical, exists_ec) && !exists_ec) {
      lib_path = canonical;
    }
  }
  path lib_dir = lib_path.parent_path();
  if (lib_dir.empty() || !lib_dir.has_parent_path()) {
    return std::nullopt;
  }
  return lib_dir.parent_path();
#else
  return std::nullopt;
#endif
}

path resolve_install_path(const path &prefix, const std::string &suffix) {
  path suffix_path(suffix);
  if (suffix_path.is_absolute()) {
    return suffix_path;
  }
  return prefix / suffix_path;
}

bool looks_like_rose_install(const path &prefix) {
  const path rose_root = resolve_install_path(prefix, ROSE_INSTALL_INCLUDE_DIR);
  if (!dir_exists(rose_root)) {
    return false;
  }

  const path compiler_root =
      resolve_install_path(prefix, ROSE_INSTALL_CLANG_INCLUDE_DIR);
  if (!dir_exists(compiler_root)) {
    return false;
  }

  const path builtin_root = rose_root / "clang";
  if (!dir_exists(builtin_root)) {
    return false;
  }

  if (!file_exists(rose_root / "rose.h")) {
    return false;
  }
  if (!file_exists(builtin_root / "clang-builtin-c.h")) {
    return false;
  }

  return true;
}

std::optional<path> find_install_prefix(const std::vector<path> &candidates) {
  for (const auto &candidate : candidates) {
    if (looks_like_rose_install(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::optional<path> find_build_root(const std::vector<path> &candidates) {
  for (const auto &candidate : candidates) {
    if (dir_exists(candidate / "include-staging")) {
      return candidate;
    }
  }
  return std::nullopt;
}

RoseClangPathRoots make_build_tree_roots(const path &build_root) {
  RoseClangPathRoots roots;
  roots.in_install_tree = false;
  const path compiler_root = build_root / "include-staging";
  roots.compiler_header_root = with_trailing_slash(compiler_root);
  roots.builtin_header_root = with_trailing_slash(compiler_root / "clang");
  roots.rose_include_root = with_trailing_slash(build_root / "include");
  return roots;
}
} // namespace

RoseClangPathRoots resolveRoseClangPaths(const char *argv0) {
  std::vector<path> override_candidates;
  const char *rose_home = std::getenv("ROSE_HOME");
  if (rose_home && *rose_home) {
    override_candidates.emplace_back(rose_home);
  }

  const bool force_build_tree = std::getenv("ROSE_IN_BUILD_TREE") != nullptr;
  auto argv_prefix = prefix_from_executable(argv0);
  auto library_prefix = prefix_from_shared_library();

  std::vector<path> build_candidates = override_candidates;
  if (library_prefix) {
    build_candidates.push_back(*library_prefix);
  }
  if (argv_prefix) {
    build_candidates.push_back(*argv_prefix);
  }
  auto build_root = find_build_root(build_candidates);

  if (!build_root) {
    path build_staging(ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR);
    if (dir_exists(build_staging)) {
      build_root = build_staging.parent_path();
    }
  }

  if (build_root) {
    return make_build_tree_roots(*build_root);
  }

  if (!force_build_tree) {
    std::vector<path> install_candidates = override_candidates;
    if (library_prefix) {
      install_candidates.push_back(*library_prefix);
    }
    if (argv_prefix) {
      install_candidates.push_back(*argv_prefix);
    }
    if (!ROSE_INSTALL_PREFIX.empty()) {
      install_candidates.emplace_back(ROSE_INSTALL_PREFIX);
    }

    auto install_prefix = find_install_prefix(install_candidates);
    if (install_prefix) {
      RoseClangPathRoots roots;
      roots.in_install_tree = true;
      const path rose_root =
          resolve_install_path(*install_prefix, ROSE_INSTALL_INCLUDE_DIR);
      const path compiler_root =
          resolve_install_path(*install_prefix, ROSE_INSTALL_CLANG_INCLUDE_DIR);
      const path builtin_root = rose_root / "clang";
      roots.compiler_header_root = with_trailing_slash(compiler_root);
      roots.builtin_header_root = with_trailing_slash(builtin_root);
      roots.rose_include_root = with_trailing_slash(rose_root);
      return roots;
    }
  }

  ROSE_ASSERT(!"Unable to resolve ROSE build/install roots for Clang headers.");
  return RoseClangPathRoots();
}
