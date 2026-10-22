#include "rose.h"

#include <algorithm>

namespace {
SgSourceFile *findMainFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source == nullptr || source->get_isHeaderFile()) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = source;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireExactSourceSurface(SgDeclarationStatement *surface) {
  ROSE_ASSERT(surface != nullptr);
  for (Sg_File_Info *position :
       {surface->get_file_info(), surface->get_startOfConstruct(),
        surface->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == surface);
    ROSE_ASSERT(!position->isCompilerGenerated());
    ROSE_ASSERT(!position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
  }
  ROSE_ASSERT(surface->get_translation_unit_source_order().has_value());
}

SgClassDeclaration *findNamedClass(SgNode *root, const SgName &name,
                                   bool requireDefinition) {
  SgClassDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgDeclarationStatement)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    if (declaration == nullptr || declaration->get_name() != name ||
        (declaration->get_definition() != nullptr) != requireDefinition) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgTypedefDeclaration *findNamedTypedef(SgNode *root, const SgName &name) {
  SgTypedefDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgDeclarationStatement)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr || declaration->get_name() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgVariableDeclaration *findField(SgNode *root, const SgName &name) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_variables().size() != 1 ||
        declaration->get_variables().front() == nullptr ||
        declaration->get_variables().front()->get_name() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *source = findMainFile(project);
  SgGlobal *global = source->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgClassDeclaration *owner =
      findNamedClass(source, "rex_field_tag_owner", true);
  SgClassDefinition *ownerDefinition = owner->get_definition();
  ROSE_ASSERT(ownerDefinition != nullptr);
  ROSE_ASSERT(owner->get_parent() == global);
  ROSE_ASSERT(owner->get_scope() == global);

  SgVariableDeclaration *field = findField(ownerDefinition, "value");
  ROSE_ASSERT(field->get_parent() == ownerDefinition);
  ROSE_ASSERT(field->get_scope() == ownerDefinition);
  ROSE_ASSERT(field->get_variables().size() == 1);
  SgInitializedName *initializedName = field->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  ROSE_ASSERT(initializedName->get_parent() == field);

  ROSE_ASSERT(field->get_baseTypeDefiningDeclaration() == nullptr);
  SgClassDeclaration *introduction =
      isSgClassDeclaration(field->get_baseTypeNondefiningDeclaration());
  ROSE_ASSERT(introduction != nullptr);
  ROSE_ASSERT(introduction->get_name() == "rex_field_tag");
  ROSE_ASSERT(introduction->get_parent() == field);
  ROSE_ASSERT(introduction->get_scope() == global);
  ROSE_ASSERT(!introduction->get_isAutonomousDeclaration());
  ROSE_ASSERT(introduction->get_definition() == nullptr);
  ROSE_ASSERT(introduction->get_firstNondefiningDeclaration() == introduction);
  requireExactSourceSurface(introduction);

  SgTypedefDeclaration *typedefDeclaration =
      findNamedTypedef(source, "rex_field_tag_definition");
  ROSE_ASSERT(typedefDeclaration->get_parent() == global);
  ROSE_ASSERT(typedefDeclaration->get_scope() == global);
  ROSE_ASSERT(
      typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration());
  SgClassDeclaration *definition =
      isSgClassDeclaration(typedefDeclaration->get_declaration());
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_name() == "rex_field_tag");
  ROSE_ASSERT(definition->get_parent() == typedefDeclaration);
  ROSE_ASSERT(definition->get_scope() == global);
  ROSE_ASSERT(!definition->get_isAutonomousDeclaration());
  ROSE_ASSERT(definition->get_definition() != nullptr);
  ROSE_ASSERT(definition->get_definition()->get_parent() == definition);
  ROSE_ASSERT(definition->get_firstNondefiningDeclaration() == introduction);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  ROSE_ASSERT(introduction->get_definingDeclaration() == definition);
  requireExactSourceSurface(definition);
  ROSE_ASSERT(*introduction->get_translation_unit_source_order() <
              *definition->get_translation_unit_source_order());

  SgClassType *fieldType =
      isSgClassType(initializedName->get_type()->findBaseType());
  ROSE_ASSERT(fieldType != nullptr);
  ROSE_ASSERT(fieldType->get_declaration() == introduction);

  const SgNodePtrList successors = field->get_traversalSuccessorContainer();
  auto introductionPosition =
      std::find(successors.begin(), successors.end(), introduction);
  auto namePosition =
      std::find(successors.begin(), successors.end(), initializedName);
  ROSE_ASSERT(introductionPosition != successors.end());
  ROSE_ASSERT(namePosition != successors.end());
  ROSE_ASSERT(std::count(successors.begin(), successors.end(), introduction) ==
              1);
  ROSE_ASSERT(introductionPosition < namePosition);

  SgVariableDeclaration *callback = findField(ownerDefinition, "callback");
  SgInitializedName *callbackName = callback->get_variables().front();
  ROSE_ASSERT(callbackName->get_type_elaboration_required());
  ROSE_ASSERT(callbackName->get_type_elaboration_required_for_type());
  SgDeclarationScope *callbackScope = callback->get_source_declarator_scope();
  ROSE_ASSERT(callbackScope != nullptr &&
              callbackScope->get_parent() == callback);
  ROSE_ASSERT(callbackScope->get_declarations().size() == 2);
  const char *tagNames[] = {"rex_field_return_tag", "rex_field_parameter_tag"};
  for (size_t index = 0; index < 2; ++index) {
    auto *tag = isSgClassDeclaration(callbackScope->get_declarations()[index]);
    ROSE_ASSERT(tag != nullptr && tag->get_name() == tagNames[index]);
    ROSE_ASSERT(tag->get_parent() == callbackScope &&
                tag->get_scope() == global);
    ROSE_ASSERT(!tag->get_isAutonomousDeclaration());
    requireExactSourceSurface(tag);
  }
  auto *callbackType = isSgFunctionType(callbackName->get_type()->stripType());
  ROSE_ASSERT(callbackType != nullptr);
  auto *returnType =
      isSgClassType(callbackType->get_return_type()->findBaseType());
  auto *parameterType =
      isSgClassType(callbackType->get_arguments().front()->findBaseType());
  ROSE_ASSERT(returnType != nullptr && parameterType != nullptr);
  ROSE_ASSERT(returnType->get_declaration() ==
              callbackScope->get_declarations()[0]);
  ROSE_ASSERT(parameterType->get_declaration() ==
              callbackScope->get_declarations()[1]);

  SgVariableDeclaration *variableCallback =
      findField(global, "rex_variable_callback");
  SgInitializedName *variableName = variableCallback->get_variables().front();
  ROSE_ASSERT(variableName->get_type_elaboration_required());
  ROSE_ASSERT(variableName->get_type_elaboration_required_for_type());
  SgFunctionSymbol *parameterCallback =
      global->lookup_function_symbol("rex_parameter_callback");
  ROSE_ASSERT(parameterCallback != nullptr);
  SgFunctionDeclaration *parameterFunction =
      parameterCallback->get_declaration();
  SgFunctionParameterList *callbackParameters =
      parameterFunction->get_parameterList_syntax();
  ROSE_ASSERT(callbackParameters != nullptr);
  ROSE_ASSERT(callbackParameters->get_parent() == parameterFunction);
  ROSE_ASSERT(callbackParameters->get_args().size() == 1);
  SgInitializedName *callbackParameter = callbackParameters->get_args().front();
  ROSE_ASSERT(callbackParameter->get_type_elaboration_required());
  ROSE_ASSERT(callbackParameter->get_type_elaboration_required_for_type());

  const SgDeclarationStatementPtrList &globalDeclarations =
      global->get_declarations();
  ROSE_ASSERT(std::count(globalDeclarations.begin(), globalDeclarations.end(),
                         introduction) == 0);
  ROSE_ASSERT(std::count(globalDeclarations.begin(), globalDeclarations.end(),
                         definition) == 0);
  ROSE_ASSERT(std::count(globalDeclarations.begin(), globalDeclarations.end(),
                         owner) == 1);
  ROSE_ASSERT(std::count(globalDeclarations.begin(), globalDeclarations.end(),
                         typedefDeclaration) == 1);

  if (SgAuxiliaryDeclarationList *auxiliary =
          global->get_auxiliary_declarations()) {
    const SgDeclarationStatementPtrList &declarations =
        auxiliary->get_declarations();
    ROSE_ASSERT(std::count(declarations.begin(), declarations.end(),
                           introduction) == 0);
    ROSE_ASSERT(
        std::count(declarations.begin(), declarations.end(), definition) == 0);
  }

  return backend(project);
}
