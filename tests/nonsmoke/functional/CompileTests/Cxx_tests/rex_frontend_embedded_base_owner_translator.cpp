#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace {

SgType *declaratorBaseType(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  std::unordered_set<SgType *> visited;
  while (visited.insert(type).second) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      type = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      type = reference->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
    } else if (SgTypedefType *typedefType = isSgTypedefType(type)) {
      type = typedefType->get_base_type();
    } else {
      return type;
    }
    ROSE_ASSERT(type != nullptr);
  }
  ROSE_ABORT();
}

SgVariableDeclaration *findVariable(SgProject *project,
                                    const std::string &name) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName == nullptr ||
        initializedName->get_name().getString() != name) {
      continue;
    }
    SgVariableDeclaration *candidate =
        isSgVariableDeclaration(initializedName->get_parent());
    if (candidate == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireEmbeddedBase(SgProject *project, const std::string &name,
                         bool expectClass) {
  SgVariableDeclaration *variable = findVariable(project, name);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  ROSE_ASSERT(initializedName->get_parent() == variable);

  SgDeclarationStatement *definition =
      variable->get_baseTypeDefiningDeclaration();
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_parent() == variable);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  ROSE_ASSERT(isSgAuxiliaryDeclarationList(definition->get_parent()) ==
              nullptr);

  SgNamedType *baseType =
      isSgNamedType(declaratorBaseType(initializedName->get_type()));
  ROSE_ASSERT(baseType != nullptr);
  ROSE_ASSERT(baseType->get_declaration() != nullptr);
  ROSE_ASSERT(baseType->get_declaration()->get_definingDeclaration() ==
              definition);

  if (expectClass) {
    SgClassDeclaration *classDefinition = isSgClassDeclaration(definition);
    ROSE_ASSERT(classDefinition != nullptr);
    ROSE_ASSERT(classDefinition->get_definition() != nullptr);
    ROSE_ASSERT(classDefinition->get_definition()->get_parent() ==
                classDefinition);
    ROSE_ASSERT(!classDefinition->get_isAutonomousDeclaration());
  } else {
    SgEnumDeclaration *enumDefinition = isSgEnumDeclaration(definition);
    ROSE_ASSERT(enumDefinition != nullptr);
    ROSE_ASSERT(!enumDefinition->get_isAutonomousDeclaration());
  }

  SgScopeStatement *semanticScope = definition->get_scope();
  ROSE_ASSERT(semanticScope != nullptr);
  const SgStatementPtrList statements = semanticScope->generateStatementList();
  ROSE_ASSERT(std::count(statements.begin(), statements.end(), definition) ==
              0);
}

void requireStandaloneBase(SgProject *project, const std::string &name) {
  SgVariableDeclaration *variable = findVariable(project, name);
  ROSE_ASSERT(variable->get_baseTypeDefiningDeclaration() == nullptr);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgNamedType *baseType = isSgNamedType(
      declaratorBaseType(variable->get_variables().front()->get_type()));
  ROSE_ASSERT(baseType != nullptr);
  SgDeclarationStatement *definition =
      baseType->get_declaration()->get_definingDeclaration();
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_parent() == definition->get_scope());
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  requireEmbeddedBase(project, "rex_embedded_global_class", true);
  requireEmbeddedBase(project, "rex_embedded_global_enum", false);
  requireEmbeddedBase(project, "rex_embedded_named_global", true);
  requireEmbeddedBase(project, "rex_embedded_field_class", true);
  requireEmbeddedBase(project, "rex_embedded_field_enum", false);
  requireEmbeddedBase(project, "rex_embedded_named_field", true);
  requireStandaloneBase(project, "rex_standalone_global");
  requireStandaloneBase(project, "rex_standalone_field");

  AstTests::runAllTests(project);
  return backend(project);
}
