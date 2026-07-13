#include "rose.h"

#include <algorithm>
#include <string>

namespace {

bool isTargetSystemHeaderDeclaration(SgTemplateClassDeclaration *declaration) {
  if (declaration == nullptr ||
      declaration->get_name().getString() != "DefaultedWrapper") {
    return false;
  }
  Sg_File_Info *source = declaration->get_startOfConstruct();
  return source != nullptr &&
         source->get_filenameString().find(
             "rex_test2025_issue160_system_header_instantiation.h") !=
             std::string::npos;
}

bool isTargetSystemHeaderForwardDeclaration(
    SgTemplateClassDeclaration *declaration) {
  if (declaration == nullptr ||
      declaration->get_name().getString() != "ForwardOnly") {
    return false;
  }
  Sg_File_Info *source = declaration->get_startOfConstruct();
  return source != nullptr &&
         source->get_filenameString().find(
             "rex_test2025_issue160_system_header_instantiation.h") !=
             std::string::npos;
}

bool isDefaultStorageSemanticAlias(SgTypedefDeclaration *declaration) {
  if (declaration == nullptr || declaration->get_name().getString() != "type" ||
      declaration->get_typedef_type() != SgTypedefDeclaration::e_using) {
    return false;
  }
  SgClassDefinition *scope = isSgClassDefinition(declaration->get_scope());
  SgClassDeclaration *owner =
      scope != nullptr ? scope->get_declaration() : nullptr;
  if (owner == nullptr || owner->get_name().getString() != "DefaultStorage") {
    return false;
  }

  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  ROSE_ASSERT(container != nullptr);
  ROSE_ASSERT(container->get_parent() == scope);
  ROSE_ASSERT(scope->get_auxiliary_declarations() == container);
  ROSE_ASSERT(std::count(container->get_declarations().begin(),
                         container->get_declarations().end(),
                         declaration) == 1);
  ROSE_ASSERT(!scope->statementExistsInScope(declaration));
  for (Sg_File_Info *position :
       {declaration->get_file_info(), declaration->get_startOfConstruct(),
        declaration->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == declaration);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  }
  return true;
}

bool hasExactSemanticFunctionLocation(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  return node->get_file_info() == node->get_startOfConstruct() &&
         SageInterface::hasExactSemanticFrontendSourcePosition(
             node, node->get_startOfConstruct()) &&
         SageInterface::hasExactSemanticFrontendSourcePosition(
             node, node->get_endOfConstruct());
}

bool hasExactPhysicalSourceLocation(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  Sg_File_Info *primary = node->get_file_info();
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  auto hasExactSourceFileInfo = [node](Sg_File_Info *position) {
    return position != nullptr && position->get_parent() == node &&
           position->get_line() > 0 && position->get_col() > 0 &&
           position->get_physical_file_id() >= 0 &&
           !position->isCompilerGenerated() &&
           !position->isFrontendSpecific() &&
           !position->isSourcePositionUnavailableInFrontend();
  };

  return hasExactSourceFileInfo(primary) && hasExactSourceFileInfo(start) &&
         hasExactSourceFileInfo(end) &&
         primary->get_physical_file_id() == start->get_physical_file_id() &&
         primary->get_line() == start->get_line() &&
         primary->get_col() == start->get_col() &&
         start->get_physical_file_id() == end->get_physical_file_id();
}

bool hasExactAuxiliarySemanticPhysicalSourceLocation(
    SgTemplateClassDeclaration *declaration) {
  if (declaration == nullptr) {
    return false;
  }
  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgScopeStatement *semanticScope =
      auxiliary != nullptr ? isSgScopeStatement(auxiliary->get_parent())
                           : nullptr;
  if (semanticScope == nullptr || declaration->get_scope() != semanticScope ||
      semanticScope->get_auxiliary_declarations() != auxiliary ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) != 1 ||
      semanticScope->statementExistsInScope(declaration)) {
    return false;
  }

  for (Sg_File_Info *position :
       {declaration->get_file_info(), declaration->get_startOfConstruct(),
        declaration->get_endOfConstruct()}) {
    if (!SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                               position) ||
        position->get_file_id() < 0 || position->get_physical_file_id() < 0 ||
        position->get_raw_line() <= 0 || position->get_raw_col() <= 0 ||
        position->get_raw_filename().find(
            "rex_test2025_issue160_system_header_instantiation.h") ==
            std::string::npos) {
      return false;
    }
  }
  return true;
}

bool isScaleAndShiftRetainedDefinitionReference(
    SgTemplateFunctionRefExp *reference) {
  SgTemplateFunctionSymbol *symbol =
      reference != nullptr ? reference->get_symbol() : nullptr;
  SgTemplateFunctionDeclaration *sourceTemplate =
      symbol != nullptr
          ? isSgTemplateFunctionDeclaration(symbol->get_declaration())
          : nullptr;
  SgFunctionDeclaration *semanticFunction =
      reference != nullptr ? reference->getAssociatedFunctionDeclaration()
                           : nullptr;
  if (sourceTemplate == nullptr ||
      sourceTemplate->get_name().getString() != "scale_and_shift") {
    return false;
  }
  ROSE_ASSERT(semanticFunction != nullptr);
  ROSE_ASSERT(semanticFunction ==
              reference->get_semantic_function_declaration());
  SgTemplateInstantiationFunctionDecl *semanticInstantiation =
      isSgTemplateInstantiationFunctionDecl(semanticFunction);
  ROSE_ASSERT(semanticInstantiation != nullptr);
  ROSE_ASSERT(semanticInstantiation->get_templateName().getString() ==
              "scale_and_shift");
  SgCastExp *decay = isSgCastExp(reference->get_parent());
  SgPointerType *decayType =
      decay != nullptr
          ? isSgPointerType(decay->get_type()->stripTypedefsAndModifiers())
          : nullptr;
  ROSE_ASSERT(decay != nullptr);
  ROSE_ASSERT(decay->get_operand() == reference);
  ROSE_ASSERT(decay->get_cast_type() == SgCastExp::e_implicit_cast);
  ROSE_ASSERT(decay->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_FunctionToPointerDecay);
  ROSE_ASSERT(reference->get_type() == semanticFunction->get_type());
  ROSE_ASSERT(decayType != nullptr);
  ROSE_ASSERT(decayType->get_base_type() == reference->get_type());
  decay->validate_semantic_conversion();
  SgTemplateInstantiationDirectiveStatement *directive =
      isSgTemplateInstantiationDirectiveStatement(
          semanticFunction->get_parent());
  ROSE_ASSERT(directive != nullptr);
  ROSE_ASSERT(directive->get_declaration() == semanticFunction);
  ROSE_ASSERT(hasExactPhysicalSourceLocation(semanticFunction));
  ROSE_ASSERT(hasExactPhysicalSourceLocation(directive));

  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(sourceTemplate->get_parent());
  SgFunctionDeclaration *defining =
      isSgFunctionDeclaration(sourceTemplate->get_definingDeclaration());
  SgFunctionDefinition *definition =
      defining != nullptr ? defining->get_definition() : nullptr;
  SgBasicBlock *body = definition != nullptr ? definition->get_body() : nullptr;
  ROSE_ASSERT(container != nullptr);
  ROSE_ASSERT(container->get_parent() == sourceTemplate->get_scope());
  ROSE_ASSERT(sourceTemplate->get_firstNondefiningDeclaration() ==
              sourceTemplate);
  ROSE_ASSERT(defining != nullptr);
  ROSE_ASSERT(defining != sourceTemplate);
  ROSE_ASSERT(defining->get_parent() == container);
  ROSE_ASSERT(defining->get_scope() == sourceTemplate->get_scope());
  ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == sourceTemplate);
  ROSE_ASSERT(defining->get_definingDeclaration() == defining);
  ROSE_ASSERT(std::count(container->get_declarations().begin(),
                         container->get_declarations().end(),
                         sourceTemplate) == 1);
  ROSE_ASSERT(std::count(container->get_declarations().begin(),
                         container->get_declarations().end(), defining) == 1);
  ROSE_ASSERT(hasExactSemanticFunctionLocation(sourceTemplate));
  ROSE_ASSERT(hasExactSemanticFunctionLocation(defining));
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_declaration() == defining);
  ROSE_ASSERT(definition->get_parent() == defining);
  ROSE_ASSERT(hasExactSemanticFunctionLocation(definition));
  ROSE_ASSERT(body != nullptr);
  ROSE_ASSERT(body->get_parent() == definition);
  ROSE_ASSERT(hasExactPhysicalSourceLocation(body));
  ROSE_ASSERT(body->get_startOfConstruct()->get_filenameString().find(
                  "rex_test2025_issue160_system_header_instantiation.h") !=
              std::string::npos);

  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_symbol_basis() == sourceTemplate);
  ROSE_ASSERT(symbol->get_declaration() == sourceTemplate);
  return true;
}

bool isDefaultedWrapperExplicitInstantiation(
    SgTemplateInstantiationDirectiveStatement *directive,
    SgTemplateClassDeclaration *primaryTemplate) {
  if (directive == nullptr) {
    return false;
  }
  SgTemplateInstantiationDecl *declaration =
      isSgTemplateInstantiationDecl(directive->get_declaration());
  if (declaration == nullptr ||
      declaration->get_templateName().getString() != "DefaultedWrapper") {
    return false;
  }

  SgScopeStatement *lexicalScope = isSgScopeStatement(directive->get_parent());
  ROSE_ASSERT(lexicalScope != nullptr);
  ROSE_ASSERT(directive->get_scope() == lexicalScope);
  ROSE_ASSERT(declaration->get_parent() == directive);
  ROSE_ASSERT(lexicalScope->statementExistsInScope(directive));
  const SgDeclarationStatementPtrList &declarations =
      lexicalScope->getDeclarationList();
  ROSE_ASSERT(std::count(declarations.begin(), declarations.end(), directive) ==
              1);
  SgTemplateClassDeclaration *templateIdentity =
      isSgTemplateClassDeclaration(declaration->get_templateDeclaration());
  ROSE_ASSERT(templateIdentity != nullptr);
  ROSE_ASSERT(primaryTemplate != nullptr);
  SgDeclarationStatement *canonicalTemplate =
      templateIdentity->get_firstNondefiningDeclaration();
  SgDeclarationStatement *canonicalExpected =
      primaryTemplate->get_firstNondefiningDeclaration();
  ROSE_ASSERT(canonicalTemplate != nullptr);
  ROSE_ASSERT(canonicalExpected != nullptr);
  ROSE_ASSERT(canonicalTemplate == canonicalExpected);
  return true;
}

bool isFunctionExplicitInstantiation(
    SgTemplateInstantiationDirectiveStatement *directive,
    const std::string &expectedName,
    const std::string &expectedLexicalNamespace) {
  if (directive == nullptr) {
    return false;
  }
  SgFunctionDeclaration *declaration =
      isSgFunctionDeclaration(directive->get_declaration());
  SgTemplateInstantiationFunctionDecl *functionInstantiation =
      isSgTemplateInstantiationFunctionDecl(declaration);
  SgTemplateInstantiationMemberFunctionDecl *memberInstantiation =
      isSgTemplateInstantiationMemberFunctionDecl(declaration);
  if (declaration == nullptr ||
      (functionInstantiation == nullptr && memberInstantiation == nullptr)) {
    return false;
  }
  const SgName &templateName = functionInstantiation != nullptr
                                   ? functionInstantiation->get_templateName()
                                   : memberInstantiation->get_templateName();
  if (templateName.getString() != expectedName) {
    return false;
  }

  SgScopeStatement *lexicalScope = isSgScopeStatement(directive->get_parent());
  ROSE_ASSERT(lexicalScope != nullptr);
  ROSE_ASSERT(directive->get_scope() == lexicalScope);
  ROSE_ASSERT(declaration->get_parent() == directive);
  ROSE_ASSERT(lexicalScope->statementExistsInScope(directive));
  ROSE_ASSERT(declaration->get_frontend_source_ownership() ==
              SgFunctionDeclaration::e_frontend_source_main_file);
  ROSE_ASSERT(!SageInterface::insideSystemHeader(declaration));
  ROSE_ASSERT(!SageInterface::insideSystemHeader(directive));
  const SgDeclarationStatementPtrList &declarations =
      lexicalScope->getDeclarationList();
  ROSE_ASSERT(std::count(declarations.begin(), declarations.end(), directive) ==
              1);

  if (expectedLexicalNamespace.empty()) {
    ROSE_ASSERT(isSgGlobal(lexicalScope) != nullptr);
  } else {
    SgNamespaceDefinitionStatement *namespaceDefinition =
        isSgNamespaceDefinitionStatement(lexicalScope);
    ROSE_ASSERT(namespaceDefinition != nullptr);
    ROSE_ASSERT(namespaceDefinition->get_namespaceDeclaration() != nullptr);
    ROSE_ASSERT(namespaceDefinition->get_namespaceDeclaration()
                    ->get_name()
                    .getString() == expectedLexicalNamespace);
  }

  return true;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateClassDeclaration *target = nullptr;
  size_t sourceBackedForwardDeclarations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (isTargetSystemHeaderForwardDeclaration(declaration)) {
      ROSE_ASSERT(declaration->get_templateParameters().size() == 1);
      ROSE_ASSERT(declaration->get_definition() == nullptr);
      ROSE_ASSERT(declaration->isForward());
      ROSE_ASSERT(hasExactAuxiliarySemanticPhysicalSourceLocation(declaration));
      ++sourceBackedForwardDeclarations;
    }
    if (!isTargetSystemHeaderDeclaration(declaration)) {
      continue;
    }
    ROSE_ASSERT(declaration->get_templateParameters().size() == 2);
    SgTemplateParameter *storage_parameter =
        declaration->get_templateParameters()[1];
    ROSE_ASSERT(storage_parameter != nullptr);
    ROSE_ASSERT(storage_parameter->get_parameterType() ==
                SgTemplateParameter::type_parameter);
    ROSE_ASSERT(storage_parameter->get_defaultTypeParameter() != nullptr);
    ROSE_ASSERT(hasExactAuxiliarySemanticPhysicalSourceLocation(declaration));
    target = declaration;
  }
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(sourceBackedForwardDeclarations == 1);

  size_t defaultStorageSemanticAliases = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    defaultStorageSemanticAliases +=
        isDefaultStorageSemanticAlias(isSgTypedefDeclaration(node)) ? 1 : 0;
  }
  ROSE_ASSERT(defaultStorageSemanticAliases >= 1);

  size_t defaultedWrapperExplicitInstantiations = 0;
  size_t scaleAndShiftExplicitInstantiations = 0;
  size_t namespaceAdjustExplicitInstantiations = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationDirectiveStatement)) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    defaultedWrapperExplicitInstantiations +=
        isDefaultedWrapperExplicitInstantiation(directive, target) ? 1 : 0;
    scaleAndShiftExplicitInstantiations +=
        isFunctionExplicitInstantiation(directive, "scale_and_shift", "") ? 1
                                                                          : 0;
    namespaceAdjustExplicitInstantiations +=
        isFunctionExplicitInstantiation(directive, "namespace_adjust",
                                        "rex_test2025_issue160")
            ? 1
            : 0;
  }
  ROSE_ASSERT(defaultedWrapperExplicitInstantiations == 1);
  ROSE_ASSERT(scaleAndShiftExplicitInstantiations == 1);
  ROSE_ASSERT(namespaceAdjustExplicitInstantiations == 1);

  size_t retainedScaleAndShiftReferences = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateFunctionRefExp)) {
    retainedScaleAndShiftReferences +=
        isScaleAndShiftRetainedDefinitionReference(
            isSgTemplateFunctionRefExp(node))
            ? 1
            : 0;
  }
  ROSE_ASSERT(retainedScaleAndShiftReferences >= 1);

  return backend(project);
}
