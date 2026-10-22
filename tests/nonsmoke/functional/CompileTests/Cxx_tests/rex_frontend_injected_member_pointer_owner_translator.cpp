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

  size_t exact_function_pointer_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *field = isSgInitializedName(node);
    if (field->get_name() != "callback" ||
        field->get_cxx_source_type() == nullptr)
      continue;
    auto *source = isSgPointerMemberType(field->get_cxx_source_type());
    auto *semantic = isSgPointerMemberType(field->get_type());
    ROSE_ASSERT(source != nullptr && semantic != nullptr);
    ROSE_ASSERT(source->get_source_class_type_is_unqualified_injected_name());
    ROSE_ASSERT(!SgPointerMemberType::isCanonicalSemanticType(source));
    ROSE_ASSERT(SgPointerMemberType::isCanonicalSemanticType(semantic));
    auto *source_function = isSgMemberFunctionType(source->get_base_type());
    auto *semantic_function = isSgMemberFunctionType(semantic->get_base_type());
    ROSE_ASSERT(source_function != nullptr && semantic_function != nullptr);
    ROSE_ASSERT(source_function->get_semantic_signature() == semantic_function);
    ROSE_ASSERT(source_function->get_class_type() ==
                semantic->get_class_type());
    ROSE_ASSERT(semantic_function->get_class_type() ==
                semantic->get_class_type());
    auto *source_owner = isSgNonrealType(source->get_class_type());
    ROSE_ASSERT(source_owner != nullptr);
    auto *source_owner_declaration =
        isSgNonrealDecl(source_owner->get_declaration());
    ROSE_ASSERT(source_owner_declaration != nullptr);
    ROSE_ASSERT(source_owner_declaration->get_tpl_args().empty());
    ROSE_ASSERT(source_owner != semantic->get_class_type());
    ROSE_ASSERT(
        SageInterface::cxxSourceTypeMatchesSemanticType(source, semantic));
    ++exact_function_pointer_count;
  }
  ROSE_ASSERT(exact_function_pointer_count == 1);
  return backend(project);
}
