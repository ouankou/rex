#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  constexpr char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      result = argv[index] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[index]);
  }

  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

void verifyCanonicalClassType(SgNode *root, VariantT variant,
                              const char *name) {
  SgClassDeclaration *canonical = nullptr;
  SgClassDeclaration *defining = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, variant)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != name) {
      continue;
    }

    SgClassDeclaration *first =
        isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr);
    if (canonical == nullptr) {
      canonical = first;
    }
    ROSE_ASSERT(first == canonical);
    if (declaration->get_definition() != nullptr) {
      ROSE_ASSERT(defining == nullptr || defining == declaration);
      defining = declaration;
    }
  }

  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_scope() != nullptr);
  ROSE_ASSERT(canonical->get_scope()->get_type_table() != nullptr);

  SgClassType *type = canonical->get_type();
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(type->get_declaration() == canonical);
  ROSE_ASSERT(canonical->get_scope()->get_type_table()->lookup_type(
                  type->get_mangled()) == type);

  if (defining != nullptr) {
    ROSE_ASSERT(canonical->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == canonical);
    ROSE_ASSERT(defining->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_scope() == canonical->get_scope());
    ROSE_ASSERT(defining->get_type() == type);
  }
}

void verifyCanonicalClassTypes(SgNode *root) {
  verifyCanonicalClassType(root, V_SgClassDeclaration,
                           "RexAstJsonCanonicalOwner");
  verifyCanonicalClassType(root, V_SgTemplateClassDeclaration,
                           "RexAstJsonCanonicalTemplate");
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

  SgSourceFile *file = firstSourceFile(project);
  verifyCanonicalClassTypes(file);
  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(file != nullptr);
  verifyCanonicalClassTypes(file);
  return 0;
}
