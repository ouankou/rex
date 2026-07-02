#include "astJson/sageAstJson.h"
#include "fixupTypes.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  std::string dir;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  const char prefix[] = "--rex-ast-json-dir=";
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], prefix, sizeof(prefix) - 1) == 0) {
      dir = argv[i] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[i]);
  }

  argc = static_cast<int>(filtered.size());
  for (int i = 0; i < argc; ++i) {
    argv[i] = filtered[i];
  }
  argv[argc] = nullptr;
  return dir;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

SgClassDeclaration *findDetachedForwardDeclaration(SgNode *root) {
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    SgClassDeclaration *decl = isSgClassDeclaration(node);
    ROSE_ASSERT(decl != nullptr);
    if (decl->get_name() != "RexTest2026DetachedType") {
      continue;
    }
    if (decl->get_definingDeclaration() != decl &&
        decl->get_definingDeclaration() != nullptr) {
      return decl;
    }
  }
  return nullptr;
}

} // namespace

int main(int argc, char **argv) {
  const std::string json_dir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!json_dir.empty());
  std::filesystem::remove_all(json_dir);
  std::filesystem::create_directories(json_dir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *old_file = firstSourceFile(project);
  SgClassDeclaration *old_forward = findDetachedForwardDeclaration(old_file);
  ROSE_ASSERT(old_forward != nullptr);

  SgSourceFile *new_file = Rose::AstJson::roundTripSourceFile(
      old_file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(new_file != nullptr);
  ROSE_ASSERT(new_file != old_file);
  ROSE_ASSERT(old_file->get_parent() == nullptr);
  ROSE_ASSERT(new_file->get_parent() != nullptr);

  SgClassType *sentinel_type = new SgClassType(old_forward);
  old_forward->set_type(sentinel_type);

  resetTypesInAST();

  ROSE_ASSERT(old_forward->get_type() == sentinel_type);
  ROSE_ASSERT(firstSourceFile(project) == new_file);
  return 0;
}
