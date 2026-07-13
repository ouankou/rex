#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <string>

namespace {
bool hasSuffix(const SgStringList &paths, const std::string &suffix) {
  return std::count_if(paths.begin(), paths.end(),
                       [&](const std::string &path) {
                         return path.size() >= suffix.size() &&
                                path.compare(path.size() - suffix.size(),
                                             suffix.size(), suffix) == 0;
                       }) == 1;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgSourceFile *source = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source != nullptr);

  const SgStringList &includes = source->get_frontendIncludeOwnershipPathList();
  const SgStringList &systems =
      source->get_frontendSystemIncludeOwnershipPathList();
  const SgStringList &externals =
      source->get_frontendExternalOwnershipPathList();
  ROSE_ASSERT(hasSuffix(externals, "test2020_45_module.pcm"));
  ROSE_ASSERT(!hasSuffix(includes, "test2020_45_module.pcm"));
  ROSE_ASSERT(!hasSuffix(systems, "test2020_45_module.pcm"));

  std::size_t helloCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function == nullptr || function->get_name() != "hello") {
      continue;
    }
    ++helloCount;
    SgAuxiliaryDeclarationList *owner =
        isSgAuxiliaryDeclarationList(function->get_parent());
    ROSE_ASSERT(owner != nullptr &&
                owner->get_parent() == function->get_scope());
  }
  ROSE_ASSERT(helloCount == 1);
  return 0;
}
