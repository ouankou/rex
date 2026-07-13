#include "rose.h"

#include <vector>

namespace {

bool isFromMainFile(SgLocatedNode *node, SgSourceFile *sourceFile) {
  Sg_File_Info *info = node != nullptr ? node->get_file_info() : nullptr;
  return info != nullptr && info->isSameFile(sourceFile);
}

bool isClassLikeScope(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != nullptr ||
         isSgTemplateClassDefinition(scope) != nullptr ||
         isSgTemplateInstantiationDefn(scope) != nullptr;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(sourceFile != nullptr);

  SgTemplateClassDeclaration *canonical = nullptr;
  SgTemplateClassDeclaration *definition = nullptr;
  SgTemplateClassDeclaration *lexicalFriend = nullptr;
  std::vector<SgTemplateClassDeclaration *> declarations;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration == nullptr || declaration->get_templateName() != "vector" ||
        !isFromMainFile(declaration, sourceFile)) {
      continue;
    }
    declarations.push_back(declaration);
    if (declaration->get_declarationModifier().isFriend()) {
      ROSE_ASSERT(lexicalFriend == nullptr);
      lexicalFriend = declaration;
    } else if (declaration->get_definition() != nullptr) {
      ROSE_ASSERT(definition == nullptr);
      definition = declaration;
    }
  }

  ROSE_ASSERT(declarations.size() == 2);
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(lexicalFriend != nullptr);
  canonical = isSgTemplateClassDeclaration(
      definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical != definition);
  ROSE_ASSERT(canonical != lexicalFriend);
  ROSE_ASSERT(definition != lexicalFriend);

  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == definition);
  ROSE_ASSERT(definition->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  ROSE_ASSERT(lexicalFriend->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(lexicalFriend->get_definingDeclaration() == definition);

  ROSE_ASSERT(canonical->get_type() == definition->get_type());
  ROSE_ASSERT(canonical->get_type() == lexicalFriend->get_type());
  SgClassType *canonicalType = isSgClassType(canonical->get_type());
  ROSE_ASSERT(canonicalType != nullptr);
  ROSE_ASSERT(canonicalType->get_declaration() == canonical);

  ROSE_ASSERT(canonical->get_scope() == definition->get_scope());
  ROSE_ASSERT(canonical->get_scope() == lexicalFriend->get_scope());
  SgAuxiliaryDeclarationList *auxiliaryDeclarations =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  ROSE_ASSERT(auxiliaryDeclarations != nullptr);
  ROSE_ASSERT(auxiliaryDeclarations->get_parent() == canonical->get_scope());
  ROSE_ASSERT(canonical->get_scope()->get_auxiliary_declarations() ==
              auxiliaryDeclarations);
  ROSE_ASSERT(!canonical->get_scope()->statementExistsInScope(canonical));
  ROSE_ASSERT(
      isClassLikeScope(isSgScopeStatement(lexicalFriend->get_parent())));
  ROSE_ASSERT(lexicalFriend->get_parent() != lexicalFriend->get_scope());

  const SgTemplateParameterPtrList &definitionParameters =
      definition->get_templateParameters();
  const SgTemplateParameterPtrList &friendParameters =
      lexicalFriend->get_templateParameters();
  ROSE_ASSERT(definitionParameters.size() == 2);
  ROSE_ASSERT(friendParameters.size() == 2);
  for (size_t index = 0; index < definitionParameters.size(); ++index) {
    ROSE_ASSERT(definitionParameters[index] != friendParameters[index]);
    ROSE_ASSERT(definitionParameters[index]->get_parent() == definition);
    ROSE_ASSERT(friendParameters[index]->get_parent() == lexicalFriend);
  }

  return backend(project);
}
