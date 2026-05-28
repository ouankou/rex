#ifndef ROSE_CLANG_PATHS_H
#define ROSE_CLANG_PATHS_H

#include <string>

struct RoseClangPathRoots {
  std::string compiler_header_root;
  std::string builtin_header_root;
  std::string rose_include_root;
  bool in_install_tree = false;
};

RoseClangPathRoots resolveRoseClangPaths(const char *argv0);

#endif
