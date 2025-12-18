#include "nodeQuery.h"
#include "sage3basic.h"
#include "sageInterface.h"

#include <string>

namespace {
SgTemplateClassDeclaration *findTemplateClass(SgProject *project,
                                              const std::string &name) {
  auto decls = NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration);
  for (SgNode *n : decls) {
    auto *decl = isSgTemplateClassDeclaration(n);
    if (decl != nullptr && decl->get_name().getString() == name) {
      return decl;
    }
  }
  return nullptr;
}

SgInitializedName *findInitializedName(SgNode *root, const std::string &name) {
  auto decls = NodeQuery::querySubTree(root, V_SgInitializedName);
  for (SgNode *n : decls) {
    auto *init = isSgInitializedName(n);
    if (init != nullptr && init->get_name().getString() == name) {
      return init;
    }
  }
  return nullptr;
}

SgType *stripForTypeCheck(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  return type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgTemplateClassDeclaration *use = findTemplateClass(project, "Use");
  ROSE_ASSERT(use != nullptr);
  ROSE_ASSERT(use->get_definition() != nullptr);

  SgInitializedName *value =
      findInitializedName(use->get_definition(), "value");
  ROSE_ASSERT(value != nullptr);

  SgType *value_type = stripForTypeCheck(value->get_type());
  ROSE_ASSERT(isSgNonrealType(value_type) != nullptr);

  SgTemplateClassDeclaration *a = findTemplateClass(project, "A");
  ROSE_ASSERT(a != nullptr);
  ROSE_ASSERT(a->get_definition() != nullptr);

  bool found_nonreal_other = false;
  auto typedefs =
      NodeQuery::querySubTree(a->get_definition(), V_SgTypedefDeclaration);
  for (SgNode *n : typedefs) {
    auto *decl = isSgTypedefDeclaration(n);
    if (decl == nullptr) {
      continue;
    }
    if (decl->get_name().getString() != "other") {
      continue;
    }

    SgType *other_type = stripForTypeCheck(decl->get_base_type());
    SgNonrealType *nr_other = isSgNonrealType(other_type);
    if (nr_other == nullptr) {
      continue;
    }

    SgNonrealDecl *nr_decl = isSgNonrealDecl(nr_other->get_declaration());
    ROSE_ASSERT(nr_decl != nullptr);
    if (!nr_decl->get_tpl_args().empty()) {
      found_nonreal_other = true;
      break;
    }
  }
  ROSE_ASSERT(found_nonreal_other);

  return backend(project);
}
