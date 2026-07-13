#include "rose.h"

#include <algorithm>
#include <string>

namespace {

SgSourceFile *mainSourceFile(SgProject *project) {
  SgSourceFile *result = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source_file = isSgSourceFile(file)) {
      if (Rose::utility_stripPathFromFileName(source_file->getFileName()) ==
          "rex_frontend_generated_typedef_dependency.c") {
        ROSE_ASSERT(result == nullptr);
        result = source_file;
      }
    }
  }
  return result;
}

SgTypedefDeclaration *findSemanticTypedef(SgGlobal *global,
                                          const SgName &name) {
  SgAuxiliaryDeclarationList *auxiliary =
      global != nullptr ? global->get_auxiliary_declarations() : nullptr;
  if (auxiliary == nullptr) {
    return nullptr;
  }
  for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
    if (SgTypedefDeclaration *typedef_declaration =
            isSgTypedefDeclaration(declaration)) {
      if (typedef_declaration->get_name() == name) {
        return typedef_declaration;
      }
    }
  }
  return nullptr;
}

SgClassDeclaration *findGlobalClassDefinition(SgGlobal *global,
                                              const SgName &name) {
  SgDeclarationStatementPtrList candidates = global->get_declarations();
  if (SgAuxiliaryDeclarationList *auxiliary =
          global->get_auxiliary_declarations()) {
    candidates.insert(candidates.end(), auxiliary->get_declarations().begin(),
                      auxiliary->get_declarations().end());
  }
  for (SgDeclarationStatement *declaration : candidates) {
    if (SgClassDeclaration *class_declaration =
            isSgClassDeclaration(declaration)) {
      if (class_declaration->get_name() == name &&
          class_declaration->get_definition() != nullptr &&
          class_declaration->get_definingDeclaration() == class_declaration) {
        return class_declaration;
      }
    }
  }
  return nullptr;
}

SgFunctionDeclaration *findGlobalFunction(SgGlobal *global,
                                          const SgName &name) {
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    if (function != nullptr && function->get_name() == name &&
        function->get_definition() != nullptr) {
      return function;
    }
  }
  return nullptr;
}

SgInitializedName *findSemanticField(SgClassDefinition *definition,
                                     const SgName &name) {
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_members().empty());
  SgAuxiliaryDeclarationList *auxiliary =
      definition->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == definition);
  for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
    if (variable == nullptr) {
      continue;
    }
    for (SgInitializedName *initialized_name : variable->get_variables()) {
      if (initialized_name != nullptr && initialized_name->get_name() == name) {
        ROSE_ASSERT(
            SageInterface::hasExactSemanticAuxiliaryOwnership(variable));
        ROSE_ASSERT(initialized_name->get_parent() == variable);
        ROSE_ASSERT(initialized_name->get_scope() == definition);
        return initialized_name;
      }
    }
  }
  return nullptr;
}

void requireGeneratedDependencyProvenance(SgTypedefDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  for (Sg_File_Info *file_info :
       {declaration->get_file_info(), declaration->get_startOfConstruct(),
        declaration->get_endOfConstruct()}) {
    ROSE_ASSERT(file_info != nullptr);
    ROSE_ASSERT(file_info->get_parent() == declaration);
    ROSE_ASSERT(file_info->isCompilerGenerated());
    ROSE_ASSERT(file_info->isFrontendSpecific());
    ROSE_ASSERT(!file_info->isTransformation());
    ROSE_ASSERT(!file_info->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(file_info->isOutputInCodeGeneration());
    ROSE_ASSERT(file_info->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(file_info->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(file_info->get_source_sequence_number() == 0);
  }
}

void requireSemanticDependency(SgGlobal *global,
                               SgTypedefDeclaration *dependency) {
  ROSE_ASSERT(dependency != nullptr);
  SgAuxiliaryDeclarationList *auxiliary = global->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == global);
  ROSE_ASSERT(dependency->get_parent() == auxiliary);
  ROSE_ASSERT(dependency->get_scope() == global);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(), dependency) == 0);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(), dependency) == 1);
  requireGeneratedDependencyProvenance(dependency);
  ROSE_ASSERT(!dependency->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(dependency->get_firstNondefiningDeclaration() == dependency);
  ROSE_ASSERT(dependency->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(dependency->get_type() != nullptr);
  ROSE_ASSERT(dependency->get_type()->get_declaration() == dependency);

  SgTypedefSymbol *symbol =
      global->lookup_typedef_symbol(dependency->get_name());
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == dependency);
  ROSE_ASSERT(dependency->get_symbol_from_symbol_table() == symbol);
  ROSE_ASSERT(global->find_symbol_from_declaration(dependency) == symbol);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *source_file = mainSourceFile(project);
  ROSE_ASSERT(source_file != nullptr);
  SgGlobal *global = source_file->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgTypedefDeclaration *field_dependency =
      findSemanticTypedef(global, "rex_hidden_field_word_t");
  SgTypedefDeclaration *signature_dependency =
      findSemanticTypedef(global, "rex_hidden_signature_word_t");
  SgTypedefDeclaration *local_dependency =
      findSemanticTypedef(global, "rex_hidden_local_word_t");
  SgClassDeclaration *record =
      findGlobalClassDefinition(global, "rex_hidden_socket_address");
  requireSemanticDependency(global, field_dependency);
  requireSemanticDependency(global, signature_dependency);
  requireSemanticDependency(global, local_dependency);
  ROSE_ASSERT(record != nullptr);
  ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(record));
  ROSE_ASSERT(
      !field_dependency->get_translation_unit_source_order().has_value());

  SgInitializedName *field =
      findSemanticField(record->get_definition(), "family");
  ROSE_ASSERT(field != nullptr);
  SgTypedefType *field_type = isSgTypedefType(field->get_type());
  ROSE_ASSERT(field_type != nullptr);
  ROSE_ASSERT(field_type == field_dependency->get_type());
  ROSE_ASSERT(field_type->get_declaration() == field_dependency);

  SgFunctionDeclaration *identity =
      findGlobalFunction(global, "rex_hidden_identity");
  ROSE_ASSERT(identity != nullptr);
  ROSE_ASSERT(identity->get_type()->get_return_type() ==
              signature_dependency->get_type());
  SgFunctionParameterList *identity_parameters = identity->get_parameterList();
  ROSE_ASSERT(identity_parameters != nullptr);
  ROSE_ASSERT(identity_parameters->get_args().size() == 1);
  SgFunctionParameterTypeList *semantic_parameter_types =
      identity->get_type()->get_argument_list();
  ROSE_ASSERT(semantic_parameter_types != nullptr);
  ROSE_ASSERT(semantic_parameter_types->get_arguments().size() == 1);
  ROSE_ASSERT(identity_parameters->get_args().front()->get_type() ==
              semantic_parameter_types->get_arguments().front());
  ROSE_ASSERT(isSgTypeUnsignedShort(
                  identity_parameters->get_args().front()->get_type()) !=
              nullptr);
  SgFunctionParameterList *identity_parameter_syntax =
      identity->get_parameterList_syntax();
  ROSE_ASSERT(identity_parameter_syntax != nullptr);
  ROSE_ASSERT(identity_parameter_syntax != identity_parameters);
  ROSE_ASSERT(identity_parameter_syntax->get_parent() == identity);
  ROSE_ASSERT(identity_parameter_syntax->get_args().size() == 1);
  ROSE_ASSERT(identity_parameter_syntax->get_args().front()->get_type() ==
              signature_dependency->get_type());

  SgFunctionDeclaration *local_user =
      findGlobalFunction(global, "rex_use_hidden_local");
  ROSE_ASSERT(local_user != nullptr);
  SgBasicBlock *local_body = local_user->get_definition()->get_body();
  ROSE_ASSERT(local_body != nullptr);
  SgInitializedName *local_value = nullptr;
  for (SgNode *candidate :
       NodeQuery::querySubTree(local_body, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(candidate);
    if (initialized_name != nullptr &&
        initialized_name->get_name() == "value") {
      ROSE_ASSERT(local_value == nullptr);
      local_value = initialized_name;
    }
  }
  ROSE_ASSERT(local_value != nullptr);
  ROSE_ASSERT(local_value->get_type() == local_dependency->get_type());

  return backend(project);
}
