#include "nodeQuery.h"
#include "rose.h"

#include <string>

namespace {
struct ExpectedType {
  const char *declaration_name;
  const char *spelling;
  SgTypeTargetBuiltin::target_family_enum family;
};

const ExpectedType *expectedType(const std::string &name) {
  static const ExpectedType expected[] = {
      {"rex_aarch64_vector_type", "__SVFloat32_t",
       SgTypeTargetBuiltin::e_target_builtin_aarch64},
      {"rex_aarch64_scalar_type", "__mfp8",
       SgTypeTargetBuiltin::e_target_builtin_aarch64},
      {"rex_riscv_vector_type", "__rvv_int8mf8_t",
       SgTypeTargetBuiltin::e_target_builtin_riscv},
  };
  for (const ExpectedType &type : expected) {
    if (name == type.declaration_name)
      return &type;
  }
  return nullptr;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  std::size_t matched = 0;
  SgTypeTargetBuiltin::target_family_enum observed_family =
      SgTypeTargetBuiltin::e_target_builtin_hlsl;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    const ExpectedType *expected =
        expectedType(declaration->get_name().getString());
    if (expected == nullptr)
      continue;

    SgType *written_type = declaration->get_base_type();
    ROSE_ASSERT(written_type != nullptr);
    SgTypeTargetBuiltin *type = isSgTypeTargetBuiltin(written_type->stripType(
        SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE));
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(type->get_spelling().getString() == expected->spelling);
    ROSE_ASSERT(type->get_target_family() == expected->family);
    ROSE_ASSERT(SageInterface::getTypeName(type) == expected->spelling);
    ROSE_ASSERT(SageInterface::isDefaultConstructible(type));
    ROSE_ASSERT(SageInterface::isCopyConstructible(type));
    ROSE_ASSERT(SageInterface::isAssignable(type));
    if (matched != 0)
      ROSE_ASSERT(observed_family == type->get_target_family());
    observed_family = type->get_target_family();
    ++matched;
  }

  if (observed_family == SgTypeTargetBuiltin::e_target_builtin_aarch64)
    ROSE_ASSERT(matched == 2);
  else {
    ROSE_ASSERT(observed_family == SgTypeTargetBuiltin::e_target_builtin_riscv);
    ROSE_ASSERT(matched == 1);
  }
  return 0;
}
