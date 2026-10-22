#include "rose.h"

#include <map>
#include <memory>

namespace {
using TypeNames = std::map<SgClassType *, std::string>;

TypeNames requireNominalIdentities(SgProject *project) {
  TypeNames identities;
  std::map<std::string, SgClassType *> names;
  for (auto *node :
       NodeQuery::querySubTree(project, V_SgDerivedTypeStatement)) {
    auto *declaration = isSgDerivedTypeStatement(node);
    auto *type = declaration->get_type();
    ROSE_ASSERT(type != nullptr);
    auto name = declaration->get_mangled_name().getString();
    ROSE_ASSERT(!name.empty());
    auto typeEntry = identities.emplace(type, name);
    ROSE_ASSERT(typeEntry.second || typeEntry.first->second == name);
    auto nameEntry = names.emplace(name, type);
    ROSE_ASSERT(nameEntry.second || nameEntry.first->second == type);
  }
  ROSE_ASSERT(identities.size() >= 2);
  return identities;
}
} // namespace

int main(int argc, char **argv) {
  for (int iteration = 0; iteration != 2; ++iteration) {
    std::unique_ptr<SgProject> project(frontend(argc, argv));
    AstTests::runAllTests(project.get());
    const auto original = requireNominalIdentities(project.get());
    std::unique_ptr<SgProject> copy(
        isSgProject(SageInterface::deepCopy(project.get())));
    ROSE_ASSERT(copy != nullptr && copy.get() != project.get());
    AstTests::runAllTests(copy.get());
    const auto copied = requireNominalIdentities(copy.get());
    ROSE_ASSERT(requireNominalIdentities(project.get()) == original);
    ROSE_ASSERT(copied.size() == original.size());
    for (const auto &entry : copied)
      ROSE_ASSERT(original.count(entry.first) == 0);
    if (iteration == 1) {
      const int status = backend(project.get());
      if (status != 0)
        return status;
    }
    SgFileList *copiedFiles = copy->get_fileList_ptr();
    SageInterface::deleteAST(copy.release(),
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(!SgNode::isLiveNode(copiedFiles));
    AstTests::runAllTests(project.get());
    ROSE_ASSERT(requireNominalIdentities(project.get()) == original);
    SgFileList *originalFiles = project->get_fileList_ptr();
    SageInterface::deleteAST(project.release(),
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(!SgNode::isLiveNode(originalFiles));
  }
  return 0;
}
