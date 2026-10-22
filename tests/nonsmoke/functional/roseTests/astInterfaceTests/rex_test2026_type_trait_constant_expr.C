#include "nodeQuery.h"

#include "rose.h"

namespace {
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

  ROSE_ASSERT(project->get_fileList().size() == 1);
  SgSourceFile *source_file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source_file != NULL);
  ROSE_ASSERT(!source_file->get_isHeaderFile());

  SgStaticAssertionDeclaration *static_assert_decl =
      findStaticAssert(source_file);
  ROSE_ASSERT(static_assert_decl != NULL);

  SgTypeTraitBuiltinOperator *condition =
      isSgTypeTraitBuiltinOperator(static_assert_decl->get_condition());
  ROSE_ASSERT(condition != NULL);
  ROSE_ASSERT(condition->get_parent() == static_assert_decl);
  ROSE_ASSERT(condition->get_builtin_operator_kind() ==
              SgTypeTraitBuiltinOperator::e_type_trait_builtin);
  ROSE_ASSERT(condition->get_name() == "__is_integral");
  ROSE_ASSERT(isSgTypeBool(condition->get_type()) != nullptr);
  ROSE_ASSERT(condition->get_args().size() == 1);
  SgTypeExpression *operand = isSgTypeExpression(condition->get_args().front());
  ROSE_ASSERT(operand != nullptr && operand->get_parent() == condition);
  ROSE_ASSERT(isSgTypeInt(operand->get_represented_type()) != nullptr);

  Rose_STL_Container<SgNode *> static_assert_traits = NodeQuery::querySubTree(
      static_assert_decl->get_condition(), V_SgTypeTraitBuiltinOperator);
  ROSE_ASSERT(static_assert_traits.size() == 1);
  ROSE_ASSERT(static_assert_traits.front() == condition);

  return backend(project);
}
