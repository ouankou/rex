#include "rose.h"

#include <algorithm>
#include <vector>

namespace {
SgSourceFile *findMainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      if (!sourceFile->get_isHeaderFile()) {
        return sourceFile;
      }
    }
  }
  return nullptr;
}

SgVariableDeclaration *findGlobalVariable(SgGlobal *global,
                                          const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
    if (variable == nullptr) {
      continue;
    }
    for (SgInitializedName *initializedName : variable->get_variables()) {
      if (initializedName != nullptr && initializedName->get_name() == name) {
        return variable;
      }
    }
  }
  return nullptr;
}

SgFunctionDeclaration *findGlobalFunction(SgGlobal *global,
                                          const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    if (function != nullptr && function->get_name() == name) {
      return function;
    }
  }
  return nullptr;
}

std::vector<SgClassDeclaration *> findGlobalTags(SgGlobal *global,
                                                 const SgName &name) {
  std::vector<SgClassDeclaration *> result;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
    if (classDeclaration != nullptr && classDeclaration->get_name() == name) {
      result.push_back(classDeclaration);
    }
  }
  return result;
}

SgClassDeclaration *classDeclarationForType(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  SgClassType *classType = isSgClassType(type->findBaseType());
  return classType != nullptr
             ? isSgClassDeclaration(classType->get_declaration())
             : nullptr;
}

void assertDeclaratorTagSourceSurface(SgClassDeclaration *tag,
                                      SgNode *structuralOwner,
                                      SgGlobal *semanticScope) {
  ROSE_ASSERT(tag != nullptr);
  ROSE_ASSERT(tag->get_parent() == structuralOwner);
  ROSE_ASSERT(tag->get_scope() == semanticScope);
  ROSE_ASSERT(!tag->get_isAutonomousDeclaration());
  ROSE_ASSERT(tag->isOutputInCodeGeneration());
  ROSE_ASSERT(tag->get_firstNondefiningDeclaration() == tag);
  ROSE_ASSERT(tag->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(tag->get_definition() == nullptr);
  ROSE_ASSERT(std::find(semanticScope->get_declarations().begin(),
                        semanticScope->get_declarations().end(),
                        tag) == semanticScope->get_declarations().end());
}

void assertStandaloneRedeclaration(SgClassDeclaration *tag,
                                   SgClassDeclaration *first, SgGlobal *scope) {
  ROSE_ASSERT(tag != nullptr);
  ROSE_ASSERT(tag != first);
  ROSE_ASSERT(tag->get_parent() == scope);
  ROSE_ASSERT(tag->get_scope() == scope);
  ROSE_ASSERT(tag->get_isAutonomousDeclaration());
  ROSE_ASSERT(tag->isOutputInCodeGeneration());
  ROSE_ASSERT(tag->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(tag->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(tag->get_definition() == nullptr);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = findMainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgVariableDeclaration *object = findGlobalVariable(global, "rex_tag_object");
  ROSE_ASSERT(object != nullptr);
  ROSE_ASSERT(object->get_variables().size() == 1);
  SgClassDeclaration *variableTag =
      classDeclarationForType(object->get_variables().front()->get_type());
  ROSE_ASSERT(variableTag != nullptr);
  ROSE_ASSERT(object->get_baseTypeNondefiningDeclaration() == variableTag);
  const SgNodePtrList variableSuccessors =
      object->get_traversalSuccessorContainer();
  const auto tagPosition = std::find(variableSuccessors.begin(),
                                     variableSuccessors.end(), variableTag);
  const auto namePosition =
      std::find(variableSuccessors.begin(), variableSuccessors.end(),
                object->get_variables().front());
  ROSE_ASSERT(tagPosition != variableSuccessors.end());
  ROSE_ASSERT(namePosition != variableSuccessors.end());
  ROSE_ASSERT(std::count(variableSuccessors.begin(), variableSuccessors.end(),
                         variableTag) == 1);
  ROSE_ASSERT(tagPosition < namePosition);
  assertDeclaratorTagSourceSurface(variableTag, object, global);

  std::vector<SgClassDeclaration *> variableTagRedeclarations =
      findGlobalTags(global, "rex_tag_redeclaration");
  ROSE_ASSERT(variableTagRedeclarations.size() == 1);
  assertStandaloneRedeclaration(variableTagRedeclarations.front(), variableTag,
                                global);

  SgFunctionDeclaration *function =
      findGlobalFunction(global, "rex_function_tag_identity");
  ROSE_ASSERT(function != nullptr);
  SgClassDeclaration *functionTag =
      classDeclarationForType(function->get_type()->get_return_type());
  SgDeclarationScope *functionScope = function->get_function_declarator_scope();
  ROSE_ASSERT(functionScope != nullptr);
  assertDeclaratorTagSourceSurface(functionTag, functionScope, global);

  SgFunctionParameterList *parameters = function->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_args().size() == 1);
  ROSE_ASSERT(classDeclarationForType(
                  parameters->get_args().front()->get_type()) == functionTag);

  std::vector<SgClassDeclaration *> functionTagRedeclarations =
      findGlobalTags(global, "rex_function_tag");
  ROSE_ASSERT(functionTagRedeclarations.size() == 1);
  assertStandaloneRedeclaration(functionTagRedeclarations.front(), functionTag,
                                global);

  return backend(project);
}
