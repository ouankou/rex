#include "nodeQuery.h"

#include "rose.h"

namespace {
SgSourceFile *findMainFile(SgProject *project) {
  SgSourceFile *first_source_file = NULL;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        return source;
      }
      if (first_source_file == NULL) {
        first_source_file = source;
      }
    }
  }

  return first_source_file;
}

bool isFromFile(SgLocatedNode *node, SgSourceFile *source_file) {
  Sg_File_Info *info = node != NULL ? node->get_file_info() : NULL;
  return info != NULL && info->isSameFile(source_file);
}

SgStaticAssertionDeclaration *findStaticAssert(SgSourceFile *source_file) {
  for (SgNode *node :
       NodeQuery::querySubTree(source_file, V_SgStaticAssertionDeclaration)) {
    SgStaticAssertionDeclaration *decl = isSgStaticAssertionDeclaration(node);
    if (decl != NULL && isFromFile(decl, source_file)) {
      return decl;
    }
  }

  return NULL;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *source_file = findMainFile(project);
  ROSE_ASSERT(source_file != NULL);

  SgStaticAssertionDeclaration *static_assert_decl =
      findStaticAssert(source_file);
  ROSE_ASSERT(static_assert_decl != NULL);

  SgBoolValExp *condition = isSgBoolValExp(static_assert_decl->get_condition());
  ROSE_ASSERT(condition != NULL);
  ROSE_ASSERT(condition->get_value());

  Rose_STL_Container<SgNode *> static_assert_traits = NodeQuery::querySubTree(
      static_assert_decl->get_condition(), V_SgTypeTraitBuiltinOperator);
  ROSE_ASSERT(static_assert_traits.empty());

  return 0;
}
