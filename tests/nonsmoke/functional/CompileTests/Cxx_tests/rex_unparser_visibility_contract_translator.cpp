#include "RoseAst.h"
#include "rose.h"

#include <map>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  using Visibility = SgDeclarationModifier::gnu_declaration_visibility_enum;
  struct ExpectedVisibility {
    Visibility declaration = SgDeclarationModifier::e_unspecified_visibility;
    Visibility type = SgDeclarationModifier::e_unspecified_visibility;
  };
  const std::map<std::string, ExpectedVisibility> expected = {
      {"rex_visibility_hidden", {SgDeclarationModifier::e_hidden_visibility}},
      {"rex_visibility_protected",
       {SgDeclarationModifier::e_protected_visibility}},
      {"rex_visibility_default", {SgDeclarationModifier::e_default_visibility}},
      {"rex_visibility_internal",
       {SgDeclarationModifier::e_internal_visibility}},
      {"rex_visibility_hidden_variable",
       {SgDeclarationModifier::e_hidden_visibility}},
      {"rex_visibility_macro_hidden",
       {SgDeclarationModifier::e_hidden_visibility}},
      {"rex_visibility_macro_default",
       {SgDeclarationModifier::e_default_visibility}},
      {"rex_visibility_macro_internal",
       {SgDeclarationModifier::e_internal_visibility}},
      {"RexVisibilityHiddenType", {SgDeclarationModifier::e_hidden_visibility}},
      {"RexTypeVisibilityHidden",
       {SgDeclarationModifier::e_unspecified_visibility,
        SgDeclarationModifier::e_hidden_visibility}},
      {"RexTypeVisibilityInternal",
       {SgDeclarationModifier::e_unspecified_visibility,
        SgDeclarationModifier::e_internal_visibility}},
      {"RexTypeVisibilityMacroProtected",
       {SgDeclarationModifier::e_unspecified_visibility,
        SgDeclarationModifier::e_protected_visibility}}};
  std::map<std::string, size_t> seen;

  for (SgNode *node : RoseAst(project)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr) {
      continue;
    }
    std::string name;
    if (SgFunctionDeclaration *function =
            isSgFunctionDeclaration(declaration)) {
      name = function->get_name().getString();
    } else if (SgClassDeclaration *record = isSgClassDeclaration(declaration)) {
      name = record->get_name().getString();
    } else if (SgVariableDeclaration *variable =
                   isSgVariableDeclaration(declaration)) {
      if (variable->get_variables().size() == 1 &&
          variable->get_variables().front() != nullptr) {
        name = variable->get_variables().front()->get_name().getString();
      }
    }
    const auto expected_visibility = expected.find(name);
    if (expected_visibility == expected.end()) {
      continue;
    }
    ROSE_ASSERT(
        declaration->get_declarationModifier().get_gnu_attribute_visibility() ==
        expected_visibility->second.declaration);
    ROSE_ASSERT(
        declaration->get_declarationModifier().get_gnu_type_visibility() ==
        expected_visibility->second.type);
    ++seen[name];
  }

  for (const auto &[name, visibility] : expected) {
    static_cast<void>(visibility);
    ROSE_ASSERT(seen[name] != 0);
  }
  return backend(project);
}
