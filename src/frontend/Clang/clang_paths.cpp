#include "clang_paths.h"

#include "rose_path_resolver.h"

RoseClangPathRoots resolveRoseClangPaths(const char *argv0) {
  RosePathRoots roots = resolveRosePaths(argv0);
  RoseClangPathRoots clang_roots;
  clang_roots.compiler_header_root = roots.compiler_header_root;
  clang_roots.builtin_header_root = roots.builtin_header_root;
  clang_roots.rose_include_root = roots.rose_include_root;
  clang_roots.in_install_tree = roots.in_install_tree;
  return clang_roots;
}
