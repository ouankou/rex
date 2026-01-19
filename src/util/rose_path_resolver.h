#ifndef ROSE_PATH_RESOLVER_H
#define ROSE_PATH_RESOLVER_H

#include "rosedll.h"

#include <string>

struct RosePathRoots {
  std::string compiler_header_root;
  std::string builtin_header_root;
  std::string rose_include_root;
  std::string build_root;
  std::string install_prefix;
  bool in_install_tree = false;
};

ROSE_UTIL_API RosePathRoots resolveRosePaths(const char *argv0);

ROSE_UTIL_API bool rosePathIsWithinTree(const std::string &root,
                                        const std::string &candidate);

#endif
