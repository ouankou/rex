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

  unsigned variadicFriends = 0;
  unsigned ordinaryFunctions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name() == "rex_variadic_plain" &&
        isFromMainFile(function, sourceFile)) {
      ROSE_ASSERT(!function->get_type()->get_has_ellipses());
      ROSE_ASSERT(function->get_parameterList()->get_args().size() == 1);
      ROSE_ASSERT(function->get_type()->get_arguments().size() == 1);
      ++ordinaryFunctions;
    }
    if (!function->get_declarationModifier().isFriend() ||
        !isFromMainFile(function, sourceFile))
      continue;
    if (isSgMemberFunctionDeclaration(function) != nullptr) {
      ROSE_ASSERT(!function->get_declarationModifier()
                       .get_storageModifier()
                       .isStatic());
      auto *first = function->get_firstNondefiningDeclaration();
      ROSE_ASSERT(first != nullptr);
      ROSE_ASSERT(
          first->get_declarationModifier().get_storageModifier().isStatic());
    }
    ROSE_ASSERT(function->get_type()->get_has_ellipses());
    const auto &parameters = function->get_parameterList()->get_args();
    const auto &arguments = function->get_type()->get_arguments();
    ROSE_ASSERT(parameters.size() == 2 && arguments.size() == 2);
    ROSE_ASSERT(isSgTypeEllipse(parameters.back()->get_type()) != nullptr &&
                isSgTypeEllipse(arguments.back()) != nullptr);
    ROSE_ASSERT(parameters.back()->get_parent() ==
                function->get_parameterList());
    ++variadicFriends;
  }
  ROSE_ASSERT(variadicFriends == 4);
  ROSE_ASSERT(ordinaryFunctions == 1);

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

  size_t emptyPackFriends = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    auto *declaration = isSgTemplateInstantiationDecl(node);
    if (declaration->get_templateName() != "empty_pack" ||
        !declaration->get_declarationModifier().isFriend())
      continue;
    ++emptyPackFriends;
    ROSE_ASSERT(declaration->get_templateArguments().empty());
    ROSE_ASSERT(declaration->get_templateDeclaration() != nullptr);
    ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() ==
                declaration->get_templateDeclaration());
    auto *canonical = isSgTemplateInstantiationDecl(
        declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr && canonical != declaration);
    ROSE_ASSERT(canonical->get_templateArguments().empty());
    ROSE_ASSERT(canonical->get_type() == declaration->get_type());
    ROSE_ASSERT(canonical->get_templateDeclaration() ==
                declaration->get_templateDeclaration());
  }
  ROSE_ASSERT(emptyPackFriends == 1);

  size_t defaultedFriends = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    auto *declaration = isSgTemplateInstantiationDecl(node);
    if (declaration->get_templateName() != "defaulted" ||
        !declaration->get_declarationModifier().isFriend())
      continue;
    ++defaultedFriends;
    ROSE_ASSERT(declaration->get_templateArguments().empty());
    ROSE_ASSERT(declaration->get_semanticTemplateArguments().size() == 1);
    ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() ==
                declaration->get_templateDeclaration());
    auto *canonical = isSgTemplateInstantiationDecl(
        declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr && canonical != declaration);
    ROSE_ASSERT(canonical->get_templateArguments().empty());
    ROSE_ASSERT(SageInterface::templateArgumentListEquivalence(
        declaration->get_semanticTemplateArguments(),
        canonical->get_semanticTemplateArguments()));
    SgClassSymbol *symbol =
        declaration->get_scope()->get_symbol_table()->find_class_instantiation(
            declaration->get_templateDeclaration(),
            declaration->get_semanticTemplateArguments());
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbol->get_declaration() == canonical);
  }
  ROSE_ASSERT(defaultedFriends == 1);
  return backend(project);
}
