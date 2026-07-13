/*
Regression for getDependentDecls sorting with compiler-generated/hidden decls.
*/
#include "rose.h"

#include <vector>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  SgFunctionDeclaration *func = SageInterface::findMain(project);
  ROSE_ASSERT(func != NULL);
  SgBasicBlock *body = func->get_definition()->get_body();
  ROSE_ASSERT(body != NULL);
  SgStatement *stmt = SageInterface::getFirstStatement(body);
  ROSE_ASSERT(stmt != NULL);
  ROSE_ASSERT(isSgForStatement(stmt) != NULL);

  std::vector<SgDeclarationStatement *> decls =
      SageInterface::getDependentDeclarations(stmt);

  SgFilePtrList &files = project->get_fileList();
  ROSE_ASSERT(!files.empty());
  SgSourceFile *source_file = isSgSourceFile(files.front());
  ROSE_ASSERT(source_file != NULL);
  SgGlobal *global_scope = source_file->get_globalScope();
  ROSE_ASSERT(global_scope != NULL);

  SgEmptyDeclaration *dummy_decl = SageBuilder::buildEmptyDeclaration(
      SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement);
  ROSE_ASSERT(dummy_decl != NULL);
  ROSE_ASSERT(dummy_decl->get_parent() == NULL);
  ROSE_ASSERT(!dummy_decl->hasExplicitScope());
  if (Sg_File_Info *info = dummy_decl->get_file_info()) {
    info->setCompilerGenerated();
    info->unsetOutputInCodeGeneration();
  }
  decls.push_back(dummy_decl);

  std::vector<SgDeclarationStatement *> sorted =
      SageInterface::sortSgNodeListBasedOnAppearanceOrderInSource(decls);

  ROSE_ASSERT(sorted.size() == decls.size());
  ROSE_ASSERT(!sorted.empty());
  ROSE_ASSERT(sorted.back() == dummy_decl);

  Sg_File_Info *last_info = NULL;
  for (SgDeclarationStatement *decl : sorted) {
    if (decl == dummy_decl) {
      break;
    }
    if (decl == NULL) {
      continue;
    }
    Sg_File_Info *info = decl->get_file_info();
    if (info == NULL || info->isCompilerGenerated()) {
      continue;
    }
    if (last_info != NULL) {
      ROSE_ASSERT((*last_info) <= (*info));
    }
    last_info = info;
  }

  AstTests::runAllTests(project);

  return backend(project);
}
