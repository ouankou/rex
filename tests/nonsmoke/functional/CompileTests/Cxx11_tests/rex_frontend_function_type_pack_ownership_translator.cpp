#include "rose.h"

namespace {
  bool hasDeclarationOwnedPackSyntax(SgType *type) {
    if (type == nullptr) {
      return false;
    }
    if (SgTemplateType *template_type = isSgTemplateType(type)) {
      return template_type->get_packed();
    }
    if (SgModifierType *modifier = isSgModifierType(type)) {
      return hasDeclarationOwnedPackSyntax(modifier->get_base_type());
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      return hasDeclarationOwnedPackSyntax(pointer->get_base_type());
    }
    if (SgReferenceType *reference = isSgReferenceType(type)) {
      return hasDeclarationOwnedPackSyntax(reference->get_base_type());
    }
    if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
      return hasDeclarationOwnedPackSyntax(reference->get_base_type());
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      return hasDeclarationOwnedPackSyntax(array->get_base_type());
    }
    return false;
  }
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  project->skipfinalCompileStep(true);

  size_t pack_expansion_positions = 0;
  VariantVector function_type_variants(V_SgFunctionType);
  function_type_variants.push_back(V_SgMemberFunctionType);
  for (SgNode *node : NodeQuery::queryMemoryPool(function_type_variants)) {
    SgFunctionType *function_type = isSgFunctionType(node);
    ROSE_ASSERT(function_type != nullptr);
    SgFunctionParameterTypeList *arguments = function_type->get_argument_list();
    ROSE_ASSERT(arguments != nullptr);
    arguments->validate_argument_qualification_use_sites(
        "rex-function-type-pack-ownership");
    const SgTypePtrList &argument_types = arguments->get_arguments();
    const SgFunctionTypeArgumentPtrList &positions =
        arguments->get_argument_qualification_use_sites();
    ROSE_ASSERT(argument_types.size() == positions.size());
    for (size_t index = 0; index < positions.size(); ++index) {
      SgFunctionTypeArgument *position = positions[index];
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == arguments);
      ROSE_ASSERT(position->get_type() == argument_types[index]);
      if (!position->get_is_pack_expansion()) {
        continue;
      }
      // The use-site position owns this ellipsis.  The referenced template
      // parameter type is a declaration identity and must not emit another.
      ROSE_ASSERT(!hasDeclarationOwnedPackSyntax(argument_types[index]));
      ++pack_expansion_positions;
    }
  }
  ROSE_ASSERT(pack_expansion_positions >= 2);

  size_t unqualified_recursive_aliases = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != "next_type") {
      continue;
    }
    ROSE_ASSERT(declaration->get_typedef_type() ==
                SgTypedefDeclaration::e_using);
    ROSE_ASSERT(declaration->get_source_base_type_qualification_present());
    ROSE_ASSERT(!declaration->get_source_base_type_global_qualification());
    ROSE_ASSERT(
        declaration->get_source_base_type_qualification_tokens().empty());
    ROSE_ASSERT(declaration->get_name_qualification_length_for_base_type() ==
                0);
    ROSE_ASSERT(
        !declaration->get_global_qualification_required_for_base_type());
    ++unqualified_recursive_aliases;
  }
  ROSE_ASSERT(unqualified_recursive_aliases == 1);

  return backend(project);
}
