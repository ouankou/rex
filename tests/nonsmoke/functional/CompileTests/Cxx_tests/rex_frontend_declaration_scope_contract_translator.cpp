#include "rose.h"

#include <vector>

namespace {
class FunctionCollector : public AstSimpleProcessing {
public:
  std::vector<SgFunctionDeclaration *> functions;
  std::vector<SgClassDeclaration *> classes;
  std::vector<SgTemplateClassDeclaration *> templateClasses;
  std::vector<SgTemplateInstantiationDecl *> templateInstantiations;
  std::vector<SgVariableDeclaration *> variables;

  void visit(SgNode *node) override {
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(node)) {
      functions.push_back(function);
    }
    if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(node)) {
      classes.push_back(classDeclaration);
    }
    if (SgTemplateClassDeclaration *templateClass =
            isSgTemplateClassDeclaration(node)) {
      templateClasses.push_back(templateClass);
    }
    if (SgTemplateInstantiationDecl *instantiation =
            isSgTemplateInstantiationDecl(node)) {
      templateInstantiations.push_back(instantiation);
    }
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(node)) {
      variables.push_back(variable);
    }
  }
};

bool isClassLikeScope(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != nullptr ||
         isSgTemplateClassDefinition(scope) != nullptr ||
         isSgTemplateInstantiationDefn(scope) != nullptr;
}

bool isFromMainFile(SgLocatedNode *node, SgSourceFile *sourceFile) {
  Sg_File_Info *info = node != nullptr ? node->get_file_info() : nullptr;
  return info != nullptr && info->isSameFile(sourceFile);
}

SgSourceFile *findMainFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      if (!sourceFile->get_isHeaderFile()) {
        return sourceFile;
      }
    }
  }
  return nullptr;
}

std::vector<SgFunctionDeclaration *> findFunctions(FunctionCollector &collector,
                                                   SgSourceFile *sourceFile,
                                                   const SgName &name) {
  std::vector<SgFunctionDeclaration *> result;
  for (SgFunctionDeclaration *function : collector.functions) {
    SgName functionName;
    if (SgTemplateInstantiationFunctionDecl *instantiation =
            isSgTemplateInstantiationFunctionDecl(function)) {
      functionName = instantiation->get_templateName();
    } else if (function != nullptr) {
      functionName = function->get_name();
    }
    if (function != nullptr && functionName == name &&
        isFromMainFile(function, sourceFile)) {
      result.push_back(function);
    }
  }
  return result;
}

std::vector<SgTemplateClassDeclaration *>
findTemplateClasses(FunctionCollector &collector, SgSourceFile *sourceFile,
                    const SgName &name) {
  std::vector<SgTemplateClassDeclaration *> result;
  for (SgTemplateClassDeclaration *templateClass : collector.templateClasses) {
    if (templateClass != nullptr && templateClass->get_templateName() == name &&
        isFromMainFile(templateClass, sourceFile)) {
      result.push_back(templateClass);
    }
  }
  return result;
}

SgInitializedName *findVariable(FunctionCollector &collector,
                                const SgName &name) {
  SgInitializedName *result = nullptr;
  for (SgVariableDeclaration *declaration : collector.variables) {
    for (SgInitializedName *variable : declaration->get_variables()) {
      if (variable != nullptr && variable->get_name() == name) {
        ROSE_ASSERT(result == nullptr);
        result = variable;
      }
    }
  }
  return result;
}

SgTemplateInstantiationDecl *
templateInstantiationForVariable(FunctionCollector &collector,
                                 const SgName &name) {
  SgInitializedName *variable = findVariable(collector, name);
  ROSE_ASSERT(variable != nullptr);
  SgClassType *type = isSgClassType(variable->get_type()->findBaseType());
  ROSE_ASSERT(type != nullptr);
  SgTemplateInstantiationDecl *declaration =
      isSgTemplateInstantiationDecl(type->get_declaration());
  ROSE_ASSERT(declaration != nullptr);
  return declaration;
}

SgTemplateClassDeclaration *
exactTemplateClassIdentity(SgDeclarationStatement *declaration) {
  if (SgNonrealDecl *written = isSgNonrealDecl(declaration)) {
    declaration = written->get_templateDeclaration();
  }
  return isSgTemplateClassDeclaration(declaration);
}

SgTemplateClassDeclaration *
canonicalTemplateClass(SgTemplateClassDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
          declaration->get_firstNondefiningDeclaration())) {
    return first;
  }
  return declaration;
}

std::string templateNamespaceName(SgTemplateClassDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgNamespaceDefinitionStatement *scope =
      isSgNamespaceDefinitionStatement(declaration->get_scope());
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(scope->get_namespaceDeclaration() != nullptr);
  return scope->get_namespaceDeclaration()->get_name().getString();
}

std::vector<SgFunctionSymbol *>
findDirectFunctionSymbols(SgScopeStatement *scope, const SgName &name) {
  std::vector<SgFunctionSymbol *> result;
  ROSE_ASSERT(scope != nullptr);
  SgSymbolTable *table = scope->get_symbol_table();
  ROSE_ASSERT(table != nullptr);
  ROSE_ASSERT(table->get_parent() == scope);
  rose_hash_multimap *symbols = table->get_table();
  ROSE_ASSERT(symbols != nullptr);
  auto range = symbols->equal_range(name);
  for (auto iterator = range.first; iterator != range.second; ++iterator) {
    if (SgFunctionSymbol *symbol = isSgFunctionSymbol(iterator->second)) {
      result.push_back(symbol);
    }
  }
  return result;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = findMainFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  FunctionCollector collector;
  collector.traverse(sourceFile, preorder);

  std::vector<SgFunctionDeclaration *> globalTargets =
      findFunctions(collector, sourceFile, "rex_global_target");
  SgFunctionDeclaration *globalForward = nullptr;
  SgFunctionDeclaration *globalDefinition = nullptr;
  SgFunctionDeclaration *namespaceForward = nullptr;
  SgFunctionDeclaration *lexicalFriend = nullptr;
  for (SgFunctionDeclaration *function : globalTargets) {
    SgScopeStatement *parent = isSgScopeStatement(function->get_parent());
    if (function->get_declarationModifier().isFriend()) {
      ROSE_ASSERT(lexicalFriend == nullptr);
      lexicalFriend = function;
    } else if (function->get_definition() != nullptr) {
      ROSE_ASSERT(globalDefinition == nullptr);
      globalDefinition = function;
    } else if (isSgGlobal(parent) != nullptr) {
      ROSE_ASSERT(globalForward == nullptr);
      globalForward = function;
    } else if (isSgNamespaceDefinitionStatement(parent) != nullptr) {
      ROSE_ASSERT(namespaceForward == nullptr);
      namespaceForward = function;
    }
  }

  ROSE_ASSERT(globalTargets.size() == 4);
  ROSE_ASSERT(globalForward != nullptr);
  ROSE_ASSERT(globalDefinition != nullptr);
  ROSE_ASSERT(namespaceForward != nullptr);
  ROSE_ASSERT(lexicalFriend != nullptr);
  ROSE_ASSERT(globalForward->get_parent() == global);
  ROSE_ASSERT(globalForward->get_scope() == global);
  ROSE_ASSERT(globalDefinition->get_parent() == global);
  ROSE_ASSERT(globalDefinition->get_scope() == global);
  ROSE_ASSERT(globalDefinition->get_firstNondefiningDeclaration() ==
              globalForward);
  ROSE_ASSERT(globalForward->get_definingDeclaration() == globalDefinition);
  ROSE_ASSERT(isSgNamespaceDefinitionStatement(
                  namespaceForward->get_parent()) != nullptr);
  ROSE_ASSERT(isSgNamespaceDefinitionStatement(namespaceForward->get_scope()) !=
              nullptr);
  ROSE_ASSERT(namespaceForward->get_firstNondefiningDeclaration() ==
              namespaceForward);
  ROSE_ASSERT(namespaceForward->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(
      isClassLikeScope(isSgScopeStatement(lexicalFriend->get_parent())));
  ROSE_ASSERT(lexicalFriend->get_scope() == global);
  ROSE_ASSERT(lexicalFriend->get_firstNondefiningDeclaration() ==
              globalForward);
  ROSE_ASSERT(lexicalFriend->get_definingDeclaration() == globalDefinition);

  std::vector<SgFunctionSymbol *> globalTargetSymbols =
      findDirectFunctionSymbols(global, "rex_global_target");
  ROSE_ASSERT(globalTargetSymbols.size() == 1);
  SgFunctionSymbol *globalTargetSymbol = globalTargetSymbols.front();
  ROSE_ASSERT(globalTargetSymbol->get_declaration() == globalForward);
  ROSE_ASSERT(globalTargetSymbol->get_symbol_basis() == globalForward);
  ROSE_ASSERT(globalTargetSymbol->get_parent() == global->get_symbol_table());
  ROSE_ASSERT(globalTargetSymbol->get_scope() == global);
  ROSE_ASSERT(
      findDirectFunctionSymbols(isSgScopeStatement(lexicalFriend->get_parent()),
                                "rex_global_target")
          .empty());

  SgNamespaceDefinitionStatement *namespaceSymbolScope =
      isSgNamespaceDefinitionStatement(namespaceForward->get_scope());
  ROSE_ASSERT(namespaceSymbolScope != nullptr);
  namespaceSymbolScope = namespaceSymbolScope->get_global_definition();
  ROSE_ASSERT(namespaceSymbolScope != nullptr);
  std::vector<SgFunctionSymbol *> namespaceTargetSymbols =
      findDirectFunctionSymbols(namespaceSymbolScope, "rex_global_target");
  ROSE_ASSERT(namespaceTargetSymbols.size() == 1);
  ROSE_ASSERT(namespaceTargetSymbols.front()->get_declaration() ==
              namespaceForward);
  ROSE_ASSERT(namespaceTargetSymbols.front()->get_symbol_basis() ==
              namespaceForward);
  ROSE_ASSERT(namespaceTargetSymbols.front()->get_parent() ==
              namespaceSymbolScope->get_symbol_table());

  std::vector<SgFunctionDeclaration *> memberTemplates =
      findFunctions(collector, sourceFile, "rex_member");
  SgTemplateMemberFunctionDeclaration *memberForward = nullptr;
  SgTemplateMemberFunctionDeclaration *memberDefinition = nullptr;
  for (SgFunctionDeclaration *function : memberTemplates) {
    SgTemplateMemberFunctionDeclaration *member =
        isSgTemplateMemberFunctionDeclaration(function);
    if (member == nullptr) {
      continue;
    }
    if (member->get_definition() != nullptr) {
      ROSE_ASSERT(memberDefinition == nullptr);
      memberDefinition = member;
    } else {
      ROSE_ASSERT(memberForward == nullptr);
      memberForward = member;
    }
  }
  ROSE_ASSERT(memberForward != nullptr);
  ROSE_ASSERT(memberDefinition != nullptr);
  ROSE_ASSERT(isClassLikeScope(memberForward->get_scope()));
  ROSE_ASSERT(memberDefinition->get_scope() == memberForward->get_scope());
  ROSE_ASSERT(
      isClassLikeScope(isSgScopeStatement(memberForward->get_parent())));
  ROSE_ASSERT(memberDefinition->get_parent() == global);
  ROSE_ASSERT(memberDefinition->get_firstNondefiningDeclaration() ==
              memberForward);
  ROSE_ASSERT(memberForward->get_definingDeclaration() == memberDefinition);

  std::vector<SgFunctionDeclaration *> friendTemplates =
      findFunctions(collector, sourceFile, "rex_friend_template");
  SgTemplateFunctionDeclaration *friendTemplateForward = nullptr;
  SgTemplateFunctionDeclaration *friendTemplateLexical = nullptr;
  for (SgFunctionDeclaration *function : friendTemplates) {
    SgTemplateFunctionDeclaration *friendTemplate =
        isSgTemplateFunctionDeclaration(function);
    if (friendTemplate == nullptr) {
      continue;
    }
    if (friendTemplate->get_declarationModifier().isFriend()) {
      ROSE_ASSERT(friendTemplateLexical == nullptr);
      friendTemplateLexical = friendTemplate;
    } else {
      ROSE_ASSERT(friendTemplateForward == nullptr);
      friendTemplateForward = friendTemplate;
    }
  }
  ROSE_ASSERT(friendTemplates.size() == 2);
  ROSE_ASSERT(friendTemplateForward != nullptr);
  ROSE_ASSERT(friendTemplateLexical != nullptr);
  ROSE_ASSERT(friendTemplateForward->get_parent() == global);
  ROSE_ASSERT(friendTemplateForward->get_scope() == global);
  ROSE_ASSERT(isClassLikeScope(
      isSgScopeStatement(friendTemplateLexical->get_parent())));
  ROSE_ASSERT(friendTemplateLexical->get_scope() == global);
  ROSE_ASSERT(friendTemplateLexical->get_firstNondefiningDeclaration() ==
              friendTemplateForward);
  ROSE_ASSERT(friendTemplateForward->get_templateParameters().size() == 1);
  ROSE_ASSERT(friendTemplateLexical->get_templateParameters().size() == 1);
  SgTemplateParameter *globalParameter =
      friendTemplateForward->get_templateParameters().front();
  SgTemplateParameter *friendParameter =
      friendTemplateLexical->get_templateParameters().front();
  ROSE_ASSERT(globalParameter != friendParameter);
  ROSE_ASSERT(globalParameter->get_parent() == friendTemplateForward);
  ROSE_ASSERT(friendParameter->get_parent() == friendTemplateLexical);

  std::vector<SgFunctionDeclaration *> friendSpecializations =
      findFunctions(collector, sourceFile, "rex_friend_specialization");
  SgTemplateFunctionDeclaration *friendSpecializationTemplate = nullptr;
  SgTemplateInstantiationFunctionDecl *friendSpecializationLexical = nullptr;
  for (SgFunctionDeclaration *function : friendSpecializations) {
    if (SgTemplateFunctionDeclaration *functionTemplate =
            isSgTemplateFunctionDeclaration(function)) {
      ROSE_ASSERT(friendSpecializationTemplate == nullptr);
      friendSpecializationTemplate = functionTemplate;
    } else if (SgTemplateInstantiationFunctionDecl *instantiation =
                   isSgTemplateInstantiationFunctionDecl(function)) {
      ROSE_ASSERT(friendSpecializationLexical == nullptr);
      friendSpecializationLexical = instantiation;
    }
  }
  ROSE_ASSERT(friendSpecializations.size() == 2);
  ROSE_ASSERT(friendSpecializationTemplate != nullptr);
  ROSE_ASSERT(friendSpecializationLexical != nullptr);
  ROSE_ASSERT(friendSpecializationTemplate->get_parent() == global);
  ROSE_ASSERT(friendSpecializationTemplate->get_scope() == global);
  ROSE_ASSERT(
      friendSpecializationLexical->get_declarationModifier().isFriend());
  ROSE_ASSERT(isClassLikeScope(
      isSgScopeStatement(friendSpecializationLexical->get_parent())));
  ROSE_ASSERT(friendSpecializationLexical->get_scope() == global);
  ROSE_ASSERT(friendSpecializationLexical->get_firstNondefiningDeclaration() ==
              friendSpecializationLexical);
  ROSE_ASSERT(friendSpecializationLexical->get_definingDeclaration() ==
              nullptr);
  ROSE_ASSERT(friendSpecializationLexical->get_templateDeclaration() ==
              friendSpecializationTemplate);
  ROSE_ASSERT(global->find_symbol_from_declaration(
                  friendSpecializationLexical) == nullptr);
  ROSE_ASSERT(friendSpecializationLexical->get_parameterList() != nullptr);
  ROSE_ASSERT(friendSpecializationLexical->get_parameterList()->get_parent() ==
              friendSpecializationLexical);
  ROSE_ASSERT(
      friendSpecializationLexical->get_parameterList()->get_args().size() == 1);
  SgInitializedName *friendSpecializationParameter =
      friendSpecializationLexical->get_parameterList()->get_args().front();
  ROSE_ASSERT(friendSpecializationParameter->get_parent() ==
              friendSpecializationLexical->get_parameterList());
  ROSE_ASSERT(friendSpecializationParameter->get_declptr() ==
              friendSpecializationLexical);

  SgClassDeclaration *nestedFriendCanonical = nullptr;
  SgClassDeclaration *nestedFriendLexical = nullptr;
  for (SgClassDeclaration *declaration : collector.classes) {
    if (declaration == nullptr ||
        declaration->get_name() != "RexNestedFriend" ||
        !isFromMainFile(declaration, sourceFile)) {
      continue;
    }
    if (declaration->get_declarationModifier().isFriend()) {
      ROSE_ASSERT(nestedFriendLexical == nullptr);
      nestedFriendLexical = declaration;
    } else {
      ROSE_ASSERT(nestedFriendCanonical == nullptr);
      nestedFriendCanonical = declaration;
    }
  }
  ROSE_ASSERT(nestedFriendCanonical != nullptr);
  ROSE_ASSERT(nestedFriendLexical != nullptr);
  SgScopeStatement *nestedFriendScope = nestedFriendCanonical->get_scope();
  ROSE_ASSERT(isClassLikeScope(nestedFriendScope));
  ROSE_ASSERT(nestedFriendCanonical->get_parent() == nestedFriendScope);
  ROSE_ASSERT(nestedFriendLexical->get_parent() == nestedFriendScope);
  ROSE_ASSERT(nestedFriendLexical->get_scope() == nestedFriendScope);
  ROSE_ASSERT(nestedFriendLexical->get_firstNondefiningDeclaration() ==
              nestedFriendCanonical);
  ROSE_ASSERT(nestedFriendCanonical->get_declarationModifier()
                  .get_accessModifier()
                  .isPublic());
  ROSE_ASSERT(nestedFriendLexical->get_declarationModifier()
                  .get_accessModifier()
                  .isNotApplicable());

  std::vector<SgTemplateClassDeclaration *> laterOnlyTemplates =
      findTemplateClasses(collector, sourceFile, "LaterOnly");
  ROSE_ASSERT(laterOnlyTemplates.size() == 1);
  SgTemplateClassDeclaration *laterOnly = laterOnlyTemplates.front();
  ROSE_ASSERT(laterOnly->get_definition() != nullptr);
  SgNamespaceDefinitionStatement *laterOnlyNamespace =
      isSgNamespaceDefinitionStatement(laterOnly->get_parent());
  ROSE_ASSERT(laterOnlyNamespace != nullptr);
  ROSE_ASSERT(laterOnlyNamespace->get_global_definition() != nullptr);
  ROSE_ASSERT(laterOnly->get_scope() ==
              laterOnlyNamespace->get_global_definition());
  ROSE_ASSERT(laterOnly->get_scope() != laterOnly->get_parent());

  std::vector<SgTemplateClassDeclaration *> acrossTemplates =
      findTemplateClasses(collector, sourceFile, "Across");
  SgTemplateClassDeclaration *acrossForward = nullptr;
  SgTemplateClassDeclaration *acrossDefinition = nullptr;
  for (SgTemplateClassDeclaration *declaration : acrossTemplates) {
    if (declaration->get_definition() != nullptr) {
      ROSE_ASSERT(acrossDefinition == nullptr);
      acrossDefinition = declaration;
    } else {
      ROSE_ASSERT(acrossForward == nullptr);
      acrossForward = declaration;
    }
  }
  ROSE_ASSERT(acrossTemplates.size() == 2);
  ROSE_ASSERT(acrossForward != nullptr);
  ROSE_ASSERT(acrossDefinition != nullptr);
  SgNamespaceDefinitionStatement *forwardNamespace =
      isSgNamespaceDefinitionStatement(acrossForward->get_parent());
  SgNamespaceDefinitionStatement *definitionNamespace =
      isSgNamespaceDefinitionStatement(acrossDefinition->get_parent());
  ROSE_ASSERT(forwardNamespace != nullptr);
  ROSE_ASSERT(definitionNamespace != nullptr);
  ROSE_ASSERT(forwardNamespace != definitionNamespace);
  ROSE_ASSERT(forwardNamespace->get_global_definition() ==
              definitionNamespace->get_global_definition());
  ROSE_ASSERT(acrossForward->get_scope() ==
              forwardNamespace->get_global_definition());
  ROSE_ASSERT(acrossDefinition->get_scope() ==
              definitionNamespace->get_global_definition());
  ROSE_ASSERT(acrossForward->get_parent() != acrossForward->get_scope());
  ROSE_ASSERT(acrossDefinition->get_parent() != acrossDefinition->get_scope());
  ROSE_ASSERT(acrossForward->get_firstNondefiningDeclaration() ==
              acrossForward);
  ROSE_ASSERT(acrossForward->get_definingDeclaration() == acrossDefinition);
  ROSE_ASSERT(acrossDefinition->get_firstNondefiningDeclaration() ==
              acrossForward);
  ROSE_ASSERT(acrossDefinition->get_definingDeclaration() == acrossDefinition);
  ROSE_ASSERT(acrossForward->get_type() == acrossDefinition->get_type());
  SgTemplateClassSymbol *acrossSymbol =
      isSgTemplateClassSymbol(acrossForward->get_symbol_from_symbol_table());
  ROSE_ASSERT(acrossSymbol != nullptr);
  ROSE_ASSERT(acrossDefinition->get_symbol_from_symbol_table() == acrossSymbol);
  ROSE_ASSERT(acrossSymbol->get_declaration() == acrossForward);
  ROSE_ASSERT(acrossSymbol->get_parent() ==
              forwardNamespace->get_global_definition()->get_symbol_table());

  std::vector<SgTemplateClassDeclaration *> friendClasses =
      findTemplateClasses(collector, sourceFile, "RexFriendClass");
  SgTemplateClassDeclaration *friendClassForward = nullptr;
  SgTemplateClassDeclaration *friendClassLexical = nullptr;
  for (SgTemplateClassDeclaration *declaration : friendClasses) {
    if (declaration->get_declarationModifier().isFriend()) {
      ROSE_ASSERT(friendClassLexical == nullptr);
      friendClassLexical = declaration;
    } else {
      ROSE_ASSERT(friendClassForward == nullptr);
      friendClassForward = declaration;
    }
  }
  ROSE_ASSERT(friendClasses.size() == 2);
  ROSE_ASSERT(friendClassForward != nullptr);
  ROSE_ASSERT(friendClassLexical != nullptr);
  ROSE_ASSERT(friendClassForward->get_parent() == global);
  ROSE_ASSERT(friendClassForward->get_scope() == global);
  ROSE_ASSERT(
      isClassLikeScope(isSgScopeStatement(friendClassLexical->get_parent())));
  ROSE_ASSERT(friendClassLexical->get_scope() == global);
  ROSE_ASSERT(friendClassLexical->get_firstNondefiningDeclaration() ==
              friendClassForward);
  ROSE_ASSERT(friendClassForward->get_templateParameters().size() == 1);
  ROSE_ASSERT(friendClassLexical->get_templateParameters().size() == 1);
  ROSE_ASSERT(friendClassForward->get_templateParameters().front() !=
              friendClassLexical->get_templateParameters().front());
  ROSE_ASSERT(
      friendClassForward->get_templateParameters().front()->get_parent() ==
      friendClassForward);
  ROSE_ASSERT(
      friendClassLexical->get_templateParameters().front()->get_parent() ==
      friendClassLexical);

  std::vector<SgTemplateClassDeclaration *> nestedClasses =
      findTemplateClasses(collector, sourceFile, "Nested");
  SgTemplateClassDeclaration *nestedForward = nullptr;
  SgTemplateClassDeclaration *nestedDefinition = nullptr;
  for (SgTemplateClassDeclaration *declaration : nestedClasses) {
    if (declaration->get_definition() != nullptr) {
      ROSE_ASSERT(nestedDefinition == nullptr);
      nestedDefinition = declaration;
    } else {
      ROSE_ASSERT(nestedForward == nullptr);
      nestedForward = declaration;
    }
  }
  ROSE_ASSERT(nestedClasses.size() == 2);
  ROSE_ASSERT(nestedForward != nullptr);
  ROSE_ASSERT(nestedDefinition != nullptr);
  ROSE_ASSERT(isClassLikeScope(nestedForward->get_scope()));
  ROSE_ASSERT(nestedDefinition->get_scope() == nestedForward->get_scope());
  ROSE_ASSERT(
      isClassLikeScope(isSgScopeStatement(nestedForward->get_parent())));
  ROSE_ASSERT(nestedDefinition->get_parent() == global);
  ROSE_ASSERT(nestedDefinition->get_firstNondefiningDeclaration() ==
              nestedForward);
  ROSE_ASSERT(nestedForward->get_definingDeclaration() == nestedDefinition);
  ROSE_ASSERT(nestedDefinition->get_symbol_from_symbol_table() ==
              nestedForward->get_symbol_from_symbol_table());

  SgTemplateInstantiationDecl *allocatorInstantiation =
      templateInstantiationForVariable(collector,
                                       "rex_exact_allocator_identity");
  SgTemplateClassDeclaration *allocatorIdentity =
      canonicalTemplateClass(allocatorInstantiation->get_templateDeclaration());
  ROSE_ASSERT(allocatorIdentity->get_templateName() == "allocator");
  ROSE_ASSERT(allocatorIdentity->get_file_info() != nullptr);
  ROSE_ASSERT(!allocatorIdentity->get_file_info()->isCompilerGenerated());

  SgTemplateInstantiationDecl *templateArgumentInstantiation =
      templateInstantiationForVariable(collector,
                                       "rex_exact_template_argument_identity");
  ROSE_ASSERT(templateArgumentInstantiation->get_templateArguments().size() ==
              1);
  SgTemplateArgument *allocatorArgument =
      templateArgumentInstantiation->get_templateArguments().front();
  ROSE_ASSERT(allocatorArgument != nullptr);
  ROSE_ASSERT(allocatorArgument->get_argumentType() ==
              SgTemplateArgument::template_template_argument);
  ROSE_ASSERT(canonicalTemplateClass(exactTemplateClassIdentity(
                  allocatorArgument->get_templateDeclaration())) ==
              allocatorIdentity);

  SgTemplateClassDeclaration *leftTarget = nullptr;
  SgTemplateClassDeclaration *rightTarget = nullptr;
  for (SgTemplateClassDeclaration *declaration :
       findTemplateClasses(collector, sourceFile, "Target")) {
    SgTemplateClassDeclaration *canonical = canonicalTemplateClass(declaration);
    const std::string owner = templateNamespaceName(canonical);
    if (owner == "RexFriendIdentityLeft") {
      if (leftTarget != nullptr) {
        ROSE_ASSERT(leftTarget == canonical);
      }
      leftTarget = canonical;
    } else if (owner == "RexFriendIdentityRight") {
      if (rightTarget != nullptr) {
        ROSE_ASSERT(rightTarget == canonical);
      }
      rightTarget = canonical;
    }
  }
  ROSE_ASSERT(leftTarget != nullptr);
  ROSE_ASSERT(rightTarget != nullptr);
  ROSE_ASSERT(leftTarget != rightTarget);

  SgTemplateInstantiationDecl *qualifiedFriend = nullptr;
  for (SgTemplateInstantiationDecl *instantiation :
       collector.templateInstantiations) {
    if (instantiation != nullptr &&
        instantiation->get_declarationModifier().isFriend() &&
        instantiation->get_templateName() == "Target") {
      ROSE_ASSERT(qualifiedFriend == nullptr);
      qualifiedFriend = instantiation;
    }
  }
  ROSE_ASSERT(qualifiedFriend != nullptr);
  ROSE_ASSERT(canonicalTemplateClass(
                  qualifiedFriend->get_templateDeclaration()) == leftTarget);

  return backend(project);
}
