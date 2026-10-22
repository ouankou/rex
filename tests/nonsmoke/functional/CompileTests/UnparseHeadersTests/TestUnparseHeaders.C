#include "rose.h"

#include "UnparseHeadersTransformVisitor.h"

#include <algorithm>
#include <set>
#include <vector>

int main(int argc, char *argv[]) {
  ROSE_ASSERT(argc > 1);

  SgProject *project = frontend(argc, argv);
  for (auto *file : project->get_fileList()) {
    auto *source = isSgSourceFile(file);
    ROSE_ASSERT(source != nullptr);
    const auto &includes = source->get_frontendIncludeOwnershipPathList();
    const auto &systems = source->get_frontendSystemIncludeOwnershipPathList();
    auto *root = source->get_associated_include_file();
    ROSE_ASSERT(root != nullptr);
    std::vector<SgIncludeFile *> pending{root};
    std::set<SgIncludeFile *> seen;
    while (!pending.empty()) {
      auto *include = pending.back();
      pending.pop_back();
      ROSE_ASSERT(include != nullptr && seen.insert(include).second);
      include->validateIncludeOwnership();
      if (include != root) {
        const std::string path = include->get_filename();
        const bool system =
            std::find(systems.begin(), systems.end(), path) != systems.end();
        const bool application =
            !system &&
            std::find(includes.begin(), includes.end(), path) != includes.end();
        ROSE_ASSERT(include->get_isSystemInclude() == system);
        ROSE_ASSERT(include->get_isApplicationFile() == application);
      }
      const auto &children = include->get_include_file_list();
      pending.insert(pending.end(), children.begin(), children.end());
    }
  }

  UnparseHeadersTransformVisitor transformVisitor;
  transformVisitor.traverse(project, preorder);

  return backend(project);
}
