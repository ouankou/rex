#include "astJson/sageAstJson.h"
#include "rose.h"

#include <stdexcept>

namespace {

SgSourceFile *firstSourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    throw std::runtime_error(
        "suppressed-header inline-tag test requires one source file");
  }
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  if (file == nullptr) {
    throw std::runtime_error(
        "suppressed-header inline-tag test input is not a source file");
  }
  return file;
}

void verifySuppressedHeaderInlineTag() {
  SgInitializedName *nestedValue = nullptr;
  VariantVector initializedNameVariants;
  initializedNameVariants.push_back(V_SgInitializedName);
  for (SgNode *node : NodeQuery::queryMemoryPool(initializedNameVariants)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate == nullptr ||
        candidate->get_name() != "rex_suppressed_nested_value") {
      continue;
    }
    if (nestedValue != nullptr) {
      throw std::runtime_error(
          "suppressed-header inline-tag field is not unique");
    }
    nestedValue = candidate;
  }
  if (nestedValue == nullptr) {
    throw std::runtime_error(
        "suppressed-header inline-tag field was not translated");
  }

  SgVariableDeclaration *field =
      isSgVariableDeclaration(nestedValue->get_declaration());
  SgClassType *nestedType =
      isSgClassType(nestedValue->get_type()->findBaseType());
  SgClassDeclaration *nestedDeclaration =
      nestedType != nullptr
          ? isSgClassDeclaration(nestedType->get_declaration())
          : nullptr;
  SgClassDeclaration *nestedDefinition =
      nestedDeclaration != nullptr
          ? isSgClassDeclaration(nestedDeclaration->get_definingDeclaration())
          : nullptr;
  if (field == nullptr || nestedDefinition == nullptr ||
      nestedDefinition->get_definition() == nullptr ||
      nestedDefinition->get_parent() != field ||
      field->get_baseTypeDefiningDeclaration() != nestedDefinition ||
      nestedDefinition->get_scope() != nestedValue->get_scope() ||
      nestedDefinition->get_isAutonomousDeclaration()) {
    throw std::runtime_error(
        "suppressed-header inline tag lost its exact declarator owner");
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  if (project == nullptr) {
    throw std::runtime_error(
        "suppressed-header inline-tag frontend returned no project");
  }
  project->skipfinalCompileStep(true);

  SgSourceFile *file = firstSourceFile(project);
  verifySuppressedHeaderInlineTag();

  file = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  if (file == nullptr) {
    throw std::runtime_error(
        "suppressed-header inline-tag AST JSON round trip returned no file");
  }
  verifySuppressedHeaderInlineTag();
  AstTests::runAllTests(project);

  return backend(project);
}
