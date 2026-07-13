#include "nodeQuery.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t exact_pointer_count = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != "pointer") {
      continue;
    }

    SgPointerMemberType *source_type =
        isSgPointerMemberType(declaration->get_base_type());
    ROSE_ASSERT(source_type != nullptr);
    if (!source_type->get_source_class_type_is_unqualified_injected_name()) {
      ROSE_ASSERT(SgPointerMemberType::isCanonicalSemanticType(source_type));
      continue;
    }
    ROSE_ASSERT(!SgPointerMemberType::isCanonicalSemanticType(source_type));

    SgNonrealType *source_owner =
        isSgNonrealType(source_type->get_class_type());
    ROSE_ASSERT(source_owner != nullptr);
    ROSE_ASSERT(source_owner->get_name() ==
                "rex_injected_member_pointer_owner");
    SgNonrealDecl *source_owner_declaration =
        isSgNonrealDecl(source_owner->get_declaration());
    ROSE_ASSERT(source_owner_declaration != nullptr);
    ROSE_ASSERT(source_owner_declaration->get_tpl_args().empty());

    SgPointerMemberType *semantic_type = SgPointerMemberType::createType(
        source_type->get_base_type(), source_type->get_class_type());
    ROSE_ASSERT(semantic_type != nullptr);
    ROSE_ASSERT(SgPointerMemberType::isCanonicalSemanticType(semantic_type));
    ROSE_ASSERT(
        !semantic_type->get_source_class_type_is_unqualified_injected_name());
    ++exact_pointer_count;
  }

  ROSE_ASSERT(exact_pointer_count == 1);
  return backend(project);
}
