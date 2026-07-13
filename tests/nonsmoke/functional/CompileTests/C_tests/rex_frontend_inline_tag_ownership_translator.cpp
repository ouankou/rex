#include "nameQualificationSupport.h"
#include "rose.h"

#include <algorithm>
#include <string>

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

SgTypedefDeclaration *findGlobalTypedef(SgGlobal *global, const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(declaration)) {
      if (typedefDeclaration->get_name() == name) {
        return typedefDeclaration;
      }
    }
  }
  return nullptr;
}

SgClassDeclaration *definingClassDeclaration(SgDeclarationStatement *node) {
  SgClassDeclaration *declaration = isSgClassDeclaration(node);
  if (declaration == nullptr) {
    return nullptr;
  }
  if (SgClassDeclaration *defining =
          isSgClassDeclaration(declaration->get_definingDeclaration())) {
    return defining;
  }
  return declaration->get_definition() != nullptr ? declaration : nullptr;
}

SgClassDeclaration *definingTypedefClass(SgTypedefDeclaration *declaration) {
  return declaration != nullptr
             ? definingClassDeclaration(declaration->get_declaration())
             : nullptr;
}

SgClassDeclaration *findGlobalClass(SgGlobal *global, const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration)) {
      if (classDeclaration->get_name() == name) {
        return definingClassDeclaration(classDeclaration);
      }
    }
  }
  return nullptr;
}

SgEnumDeclaration *findGlobalEnum(SgGlobal *global, const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration)) {
      if (enumDeclaration->get_name() == name) {
        if (SgEnumDeclaration *defining = isSgEnumDeclaration(
                enumDeclaration->get_definingDeclaration())) {
          return defining;
        }
        return enumDeclaration;
      }
    }
  }
  return nullptr;
}

SgFunctionDeclaration *findGlobalFunction(SgGlobal *global,
                                          const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgFunctionDeclaration *function =
            isSgFunctionDeclaration(declaration)) {
      if (function->get_name() == name) {
        return function;
      }
    }
  }
  return nullptr;
}

SgVariableDeclaration *findMemberVariable(SgClassDefinition *definition,
                                          const SgName &name) {
  for (SgDeclarationStatement *member : definition->get_members()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(member);
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

void assertDeclaratorOwnedDefinition(SgClassDeclaration *declaration,
                                     SgDeclarationStatement *sourceOwner,
                                     SgScopeStatement *semanticScope) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_definition() != nullptr);
  ROSE_ASSERT(declaration->get_definingDeclaration() == declaration);
  ROSE_ASSERT(declaration->get_parent() == sourceOwner);
  ROSE_ASSERT(declaration->get_scope() == semanticScope);
  ROSE_ASSERT(!declaration->get_isAutonomousDeclaration());
  ROSE_ASSERT(declaration->isOutputInCodeGeneration());
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(sourceOwner)) {
    ROSE_ASSERT(variable->get_baseTypeDefiningDeclaration() == declaration);
    const SgNodePtrList successors =
        variable->get_traversalSuccessorContainer();
    ROSE_ASSERT(std::count(successors.begin(), successors.end(), declaration) ==
                1);
  }
}

void assertTypedefAliasField(SgClassDefinition *definition, const SgName &name,
                             SgTypedefDeclaration *typedefDeclaration,
                             bool expectArray) {
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(typedefDeclaration != nullptr);
  SgVariableDeclaration *variable = findMemberVariable(definition, name);
  ROSE_ASSERT(variable != nullptr);
  ROSE_ASSERT(variable->get_baseTypeDefiningDeclaration() == nullptr);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  ROSE_ASSERT(!initializedName->get_type_elaboration_required());
  ROSE_ASSERT(!initializedName->get_type_elaboration_required_for_type());

  SgType *writtenType = initializedName->get_type();
  if (expectArray) {
    SgArrayType *arrayType = isSgArrayType(writtenType);
    ROSE_ASSERT(arrayType != nullptr);
    writtenType = arrayType->get_base_type();
  }
  ROSE_ASSERT(writtenType == typedefDeclaration->get_type());
}
} // namespace

int main(int argc, char **argv) {
  bool corruptParameterTagOwner = false;
  if (argc > 1 && std::string(argv[1]) == "--corrupt-parameter-tag-owner") {
    corruptParameterTagOwner = true;
    for (int index = 1; index + 1 < argc; ++index) {
      argv[index] = argv[index + 1];
    }
    --argc;
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = findMainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgTypedefDeclaration *namedTypedef =
      findGlobalTypedef(global, "NamedRecordPointer");
  SgTypedefDeclaration *anonymousTypedef =
      findGlobalTypedef(global, "AnonymousRecord");
  SgTypedefDeclaration *fieldAliasTypedef =
      findGlobalTypedef(global, "RexTypedefAliasRecord");
  ROSE_ASSERT(namedTypedef != nullptr);
  ROSE_ASSERT(anonymousTypedef != nullptr);
  ROSE_ASSERT(fieldAliasTypedef != nullptr);
  ROSE_ASSERT(namedTypedef->get_typedefBaseTypeContainsDefiningDeclaration());
  ROSE_ASSERT(
      anonymousTypedef->get_typedefBaseTypeContainsDefiningDeclaration());
  ROSE_ASSERT(
      fieldAliasTypedef->get_typedefBaseTypeContainsDefiningDeclaration());

  SgClassDeclaration *namedDefinition = definingTypedefClass(namedTypedef);
  SgClassDeclaration *anonymousDefinition =
      definingTypedefClass(anonymousTypedef);
  SgClassDeclaration *fieldAliasDefinition =
      definingTypedefClass(fieldAliasTypedef);
  assertDeclaratorOwnedDefinition(namedDefinition, namedTypedef, global);
  assertDeclaratorOwnedDefinition(anonymousDefinition, anonymousTypedef,
                                  global);
  assertDeclaratorOwnedDefinition(fieldAliasDefinition, fieldAliasTypedef,
                                  global);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         namedDefinition) == 0);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         anonymousDefinition) == 0);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         fieldAliasDefinition) == 0);

  SgClassDeclaration *fieldAliasOwner =
      findGlobalClass(global, "RexTypedefAliasFieldOwner");
  ROSE_ASSERT(fieldAliasOwner != nullptr);
  SgClassDefinition *fieldAliasOwnerDefinition =
      fieldAliasOwner->get_definition();
  ROSE_ASSERT(fieldAliasOwnerDefinition != nullptr);
  assertTypedefAliasField(fieldAliasOwnerDefinition, "rex_direct_alias",
                          fieldAliasTypedef, false);
  assertTypedefAliasField(fieldAliasOwnerDefinition, "rex_alias_array",
                          fieldAliasTypedef, true);

  SgClassDeclaration *holderDeclaration =
      findGlobalClass(global, "RecordHolder");
  ROSE_ASSERT(holderDeclaration != nullptr);
  ROSE_ASSERT(holderDeclaration->get_parent() == global);
  ROSE_ASSERT(holderDeclaration->get_scope() == global);
  ROSE_ASSERT(holderDeclaration->get_isAutonomousDeclaration());
  SgClassDefinition *holderDefinition = holderDeclaration->get_definition();
  ROSE_ASSERT(holderDefinition != nullptr);

  SgVariableDeclaration *nestedField =
      findMemberVariable(holderDefinition, "nested");
  ROSE_ASSERT(nestedField != nullptr);
  ROSE_ASSERT(nestedField->get_variables().size() == 1);
  SgInitializedName *nestedName = nestedField->get_variables().front();
  ROSE_ASSERT(nestedName != nullptr);
  SgClassType *nestedType =
      isSgClassType(nestedName->get_type()->findBaseType());
  ROSE_ASSERT(nestedType != nullptr);
  SgClassDeclaration *nestedDefinition =
      definingClassDeclaration(nestedType->get_declaration());
  assertDeclaratorOwnedDefinition(nestedDefinition, nestedField, global);
  ROSE_ASSERT(std::count(holderDefinition->get_members().begin(),
                         holderDefinition->get_members().end(),
                         nestedDefinition) == 0);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         nestedDefinition) == 0);

  SgClassDeclaration *standaloneRecord =
      findGlobalClass(global, "StandaloneRecord");
  ROSE_ASSERT(standaloneRecord != nullptr);
  ROSE_ASSERT(standaloneRecord->get_definition() != nullptr);
  ROSE_ASSERT(standaloneRecord->get_parent() == global);
  ROSE_ASSERT(standaloneRecord->get_scope() == global);
  ROSE_ASSERT(standaloneRecord->get_isAutonomousDeclaration());
  ROSE_ASSERT(standaloneRecord->isOutputInCodeGeneration());

  SgEnumDeclaration *standaloneEnum = findGlobalEnum(global, "StandaloneEnum");
  ROSE_ASSERT(standaloneEnum != nullptr);
  ROSE_ASSERT(standaloneEnum->get_parent() == global);
  ROSE_ASSERT(standaloneEnum->get_scope() == global);
  ROSE_ASSERT(standaloneEnum->get_isAutonomousDeclaration());
  ROSE_ASSERT(standaloneEnum->isOutputInCodeGeneration());

  SgFunctionDeclaration *incompleteEnumReturn =
      findGlobalFunction(global, "rex_incomplete_enum_return");
  ROSE_ASSERT(incompleteEnumReturn != nullptr);
  SgEnumType *incompleteEnumType = isSgEnumType(
      incompleteEnumReturn->get_orig_return_type()->findBaseType());
  ROSE_ASSERT(incompleteEnumType != nullptr);
  SgEnumDeclaration *incompleteEnum =
      isSgEnumDeclaration(incompleteEnumType->get_declaration());
  ROSE_ASSERT(incompleteEnum != nullptr);
  ROSE_ASSERT(incompleteEnum->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(incompleteEnum->get_firstNondefiningDeclaration() ==
              incompleteEnum);
  SgDeclarationScope *returnTypeOwner =
      isSgDeclarationScope(incompleteEnum->get_parent());
  ROSE_ASSERT(returnTypeOwner != nullptr);
  ROSE_ASSERT(returnTypeOwner->get_parent() == incompleteEnumReturn);
  ROSE_ASSERT(incompleteEnum->get_scope() == global);
  ROSE_ASSERT(!incompleteEnum->get_isAutonomousDeclaration());
  ROSE_ASSERT(incompleteEnum->isOutputInCodeGeneration());
  ROSE_ASSERT(std::count(returnTypeOwner->get_declarations().begin(),
                         returnTypeOwner->get_declarations().end(),
                         incompleteEnum) == 1);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         incompleteEnum) == 0);

  SgFunctionDeclaration *parameterTagFunction =
      findGlobalFunction(global, "rex_parameter_owned_tag");
  ROSE_ASSERT(parameterTagFunction != nullptr);
  SgFunctionParameterList *parameterList =
      parameterTagFunction->get_parameterList();
  ROSE_ASSERT(parameterList != nullptr);
  ROSE_ASSERT(parameterList->get_args().size() == 1);
  SgInitializedName *parameter = parameterList->get_args().front();
  ROSE_ASSERT(parameter != nullptr);
  SgClassType *parameterType =
      isSgClassType(parameter->get_type()->findBaseType());
  ROSE_ASSERT(parameterType != nullptr);
  SgClassDeclaration *parameterDefinition =
      definingClassDeclaration(parameterType->get_declaration());
  ROSE_ASSERT(parameterDefinition != nullptr);
  SgDeclarationScope *parameterTagOwner =
      isSgDeclarationScope(parameterDefinition->get_parent());
  ROSE_ASSERT(parameterTagOwner != nullptr);
  ROSE_ASSERT(SageBuilder::getDeclarationScopeOwner(parameterTagOwner) ==
              parameterTagFunction);
  ROSE_ASSERT(parameterDefinition->get_scope() == parameterTagOwner);
  ROSE_ASSERT(!parameterDefinition->get_isAutonomousDeclaration());
  ROSE_ASSERT(parameterDefinition->isOutputInCodeGeneration());
  ROSE_ASSERT(std::count(parameterTagOwner->get_declarations().begin(),
                         parameterTagOwner->get_declarations().end(),
                         parameterDefinition) == 1);
  SgClassDeclaration *parameterFirst = isSgClassDeclaration(
      parameterDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(parameterFirst != nullptr);
  ROSE_ASSERT(parameterFirst != parameterDefinition);
  ROSE_ASSERT(parameterFirst->get_scope() == parameterTagOwner);
  SgAuxiliaryDeclarationList *parameterFirstOwner =
      isSgAuxiliaryDeclarationList(parameterFirst->get_parent());
  ROSE_ASSERT(parameterFirstOwner != nullptr);
  ROSE_ASSERT(parameterFirstOwner->get_parent() == parameterTagOwner);
  ROSE_ASSERT(std::count(parameterFirstOwner->get_declarations().begin(),
                         parameterFirstOwner->get_declarations().end(),
                         parameterFirst) == 1);
  ROSE_ASSERT(std::count(parameterTagOwner->get_declarations().begin(),
                         parameterTagOwner->get_declarations().end(),
                         parameterFirst) == 0);

  if (corruptParameterTagOwner) {
    // Keep the declaration reachable through the exact signature-scope list
    // while corrupting only its structural parent.  Name qualification must
    // reject the obsolete parameter-list-parent shape instead of recovering a
    // nearby function scope and silently skipping referencedNameSet.
    parameterDefinition->set_parent(parameterList);
    SgUnorderedNodeSet referencedNames;
    NameQualificationContext qualifications;
    generateNameQualificationSupport(sourceFile, referencedNames,
                                     qualifications);
    return 1;
  }

  SgUnorderedNodeSet referencedNames;
  NameQualificationContext qualifications;
  generateNameQualificationSupport(sourceFile, referencedNames, qualifications);
  SgDeclarationStatement *namedReferencedDeclaration =
      namedDefinition->get_firstNondefiningDeclaration();
  SgDeclarationStatement *parameterReferencedDeclaration =
      parameterDefinition->get_firstNondefiningDeclaration();
  ROSE_ASSERT(namedReferencedDeclaration != nullptr);
  ROSE_ASSERT(parameterReferencedDeclaration != nullptr);
  ROSE_ASSERT(referencedNames.find(namedReferencedDeclaration) !=
              referencedNames.end());
  ROSE_ASSERT(referencedNames.find(parameterReferencedDeclaration) !=
              referencedNames.end());

  SgClassDeclaration *nestedFirst =
      isSgClassDeclaration(nestedDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(nestedFirst != nullptr);
  ROSE_ASSERT(nestedFirst->get_scope() == global);
  if (nestedFirst != nestedDefinition) {
    SgAuxiliaryDeclarationList *auxiliaryOwner =
        isSgAuxiliaryDeclarationList(nestedFirst->get_parent());
    ROSE_ASSERT(auxiliaryOwner != nullptr);
    ROSE_ASSERT(auxiliaryOwner->get_parent() == global);
    ROSE_ASSERT(nestedFirst->isCompilerGenerated());
    ROSE_ASSERT(nestedFirst->isOutputInCodeGeneration());
    ROSE_ASSERT(std::count(auxiliaryOwner->get_declarations().begin(),
                           auxiliaryOwner->get_declarations().end(),
                           nestedFirst) == 1);
    ROSE_ASSERT(std::count(global->get_declarations().begin(),
                           global->get_declarations().end(), nestedFirst) == 0);
  }

  return backend(project);
}
