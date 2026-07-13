#include "rose.h"

#include <set>
#include <string>

namespace {

void verifyParameterRedeclarationChains(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  bool sawParameterChain = false;
  bool sawImportedPointerParameter = false;

  std::vector<SgNode *> roots{project};
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source != nullptr &&
        source->get_frontendExternalFileList() != nullptr) {
      roots.push_back(source->get_frontendExternalFileList());
    }
  }

  std::set<SgInitializedName *> declarations;
  for (SgNode *root : roots) {
    for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
      declarations.insert(isSgInitializedName(node));
    }
  }
  for (SgInitializedName *declaration : declarations) {
    ROSE_ASSERT(declaration != nullptr);
    std::set<SgInitializedName *> chain;
    for (SgInitializedName *current = declaration; current != nullptr;
         current = current->get_prev_decl_item()) {
      if (!chain.insert(current).second) {
        fprintf(stderr,
                "REX_TEST_INVARIANT[variable-redeclaration-chain]: "
                "declaration=%p name=%s contains a cycle at %p\n",
                static_cast<void *>(declaration), declaration->get_name().str(),
                static_cast<void *>(current));
        ROSE_ABORT();
      }
    }

    if (declaration->get_prev_decl_item() == nullptr) {
      continue;
    }
    sawParameterChain = true;
    if (declaration->get_name() == "fptr") {
      sawImportedPointerParameter =
          sawImportedPointerParameter ||
          declaration->search_for_symbol_from_symbol_table() != nullptr;
    }
  }

  ROSE_ASSERT(sawParameterChain);
  ROSE_ASSERT(sawImportedPointerParameter);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--cycle") {
    SgInitializedName *first = SageBuilder::buildInitializedName_nfi(
        SgName("rex_cycle"), SageBuilder::buildIntType(), nullptr);
    SgInitializedName *second = SageBuilder::buildInitializedName_nfi(
        SgName("rex_cycle"), SageBuilder::buildIntType(), nullptr);
    ROSE_ASSERT(first != nullptr);
    ROSE_ASSERT(second != nullptr);
    first->set_prev_decl_item(second);
    second->set_prev_decl_item(first);
    first->search_for_symbol_from_symbol_table();
    ROSE_ABORT();
  }

  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);
  verifyParameterRedeclarationChains(project);
  AstTests::runAllTests(project);
  return 0;
}
