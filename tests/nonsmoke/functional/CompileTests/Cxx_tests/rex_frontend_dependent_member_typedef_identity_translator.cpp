#include "RoseAst.h"
#include "rose.h"

#include <algorithm>

namespace {

void validateDependentMemberTypedefIdentity(SgProject *project) {
  SgTypedefDeclaration *typedef_declaration = nullptr;
  SgInitializedName *callback_parameter = nullptr;
  size_t matching_nonreal_declarations = 0;

  for (SgNode *node : RoseAst(project)) {
    if (SgTypedefDeclaration *candidate = isSgTypedefDeclaration(node);
        candidate != nullptr &&
        candidate->get_name() == "RexDependentCallback" &&
        candidate->get_file_info() != nullptr &&
        !candidate->get_file_info()->isCompilerGenerated()) {
      ROSE_ASSERT(typedef_declaration == nullptr);
      typedef_declaration = candidate;
    }
    if (SgInitializedName *candidate = isSgInitializedName(node);
        candidate != nullptr && candidate->get_name() == "callback" &&
        isSgFunctionParameterList(candidate->get_parent()) != nullptr &&
        candidate->get_file_info() != nullptr &&
        !candidate->get_file_info()->isCompilerGenerated()) {
      ROSE_ASSERT(callback_parameter == nullptr);
      callback_parameter = candidate;
    }
    if (SgNonrealDecl *candidate = isSgNonrealDecl(node);
        candidate != nullptr &&
        candidate->get_name() == "RexDependentCallback") {
      ++matching_nonreal_declarations;
    }
  }

  ROSE_ASSERT(typedef_declaration != nullptr);
  ROSE_ASSERT(callback_parameter != nullptr);
  ROSE_ASSERT(matching_nonreal_declarations == 0);

  SgTypedefType *parameter_type =
      isSgTypedefType(callback_parameter->get_type());
  ROSE_ASSERT(parameter_type != nullptr);
  ROSE_ASSERT(parameter_type->get_declaration() == typedef_declaration);
  ROSE_ASSERT(typedef_declaration->get_type() == parameter_type);
  ROSE_ASSERT(callback_parameter->get_declptr() != nullptr);
  ROSE_ASSERT(callback_parameter->get_scope() != nullptr);
}

void validateSemanticInstantiationPopulation(SgProject *project) {
  SgTemplateInstantiationDefn *definition = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateInstantiationDefn *candidate =
        isSgTemplateInstantiationDefn(node);
    SgTemplateInstantiationDecl *declaration =
        candidate != nullptr
            ? isSgTemplateInstantiationDecl(candidate->get_declaration())
            : nullptr;
    if (declaration == nullptr ||
        declaration->get_templateName() != "RexDependentTypedefOwner") {
      continue;
    }
    ROSE_ASSERT(definition == nullptr);
    definition = candidate;
  }

  ROSE_ASSERT(definition != nullptr);
  SgAuxiliaryDeclarationList *auxiliary =
      definition->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == definition);

  auto exactSemanticVariable = [&](const SgName &name) {
    SgVariableDeclaration *result = nullptr;
    for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
      SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
      if (variable == nullptr || variable->get_variables().size() != 1 ||
          variable->get_variables().front()->get_name() != name) {
        continue;
      }
      ROSE_ASSERT(result == nullptr);
      result = variable;
    }
    ROSE_ASSERT(result != nullptr);
    ROSE_ASSERT(result->get_parent() == auxiliary);
    ROSE_ASSERT(result->get_scope() == definition);
    ROSE_ASSERT(!result->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(std::count(definition->get_members().begin(),
                           definition->get_members().end(), result) == 0);
    Sg_File_Info *source = result->get_file_info();
    ROSE_ASSERT(source != nullptr);
    ROSE_ASSERT(source->isCompilerGenerated());
    ROSE_ASSERT(source->isFrontendSpecific());
    return result;
  };

  SgVariableDeclaration *callback = exactSemanticVariable("callback_");
  SgVariableDeclaration *object = exactSemanticVariable("object_");
  ROSE_ASSERT(callback != object);

  struct SemanticMethodFamily {
    SgTemplateInstantiationMemberFunctionDecl *canonical = nullptr;
    SgTemplateInstantiationMemberFunctionDecl *defining = nullptr;
    size_t declarations = 0;
  } constructorFamily, invokeFamily;
  for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
    SgTemplateInstantiationMemberFunctionDecl *function =
        isSgTemplateInstantiationMemberFunctionDecl(declaration);
    if (function == nullptr) {
      continue;
    }
    SemanticMethodFamily *family = nullptr;
    if (function->get_name() == "RexDependentTypedefOwner") {
      family = &constructorFamily;
    } else if (function->get_name() == "invoke") {
      family = &invokeFamily;
    } else {
      continue;
    }
    ++family->declarations;
    if (function->get_definition() != nullptr) {
      ROSE_ASSERT(family->defining == nullptr);
      family->defining = function;
    } else {
      ROSE_ASSERT(family->canonical == nullptr);
      family->canonical = function;
    }
    ROSE_ASSERT(function->get_parent() == auxiliary);
    ROSE_ASSERT(function->get_scope() == definition);
    ROSE_ASSERT(!function->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(std::count(definition->get_members().begin(),
                           definition->get_members().end(), function) == 0);
    Sg_File_Info *source = function->get_file_info();
    ROSE_ASSERT(source != nullptr);
    ROSE_ASSERT(source->isCompilerGenerated());
    ROSE_ASSERT(source->isFrontendSpecific());
  }
  auto validateMethodFamily = [](const SemanticMethodFamily &family) {
    ROSE_ASSERT(family.declarations == 2);
    ROSE_ASSERT(family.canonical != nullptr);
    ROSE_ASSERT(family.defining != nullptr);
    ROSE_ASSERT(family.canonical != family.defining);
    ROSE_ASSERT(family.canonical->get_firstNondefiningDeclaration() ==
                family.canonical);
    ROSE_ASSERT(family.canonical->get_definingDeclaration() == family.defining);
    ROSE_ASSERT(family.defining->get_firstNondefiningDeclaration() ==
                family.canonical);
    ROSE_ASSERT(family.defining->get_definingDeclaration() == family.defining);
  };
  validateMethodFamily(constructorFamily);
  validateMethodFamily(invokeFamily);

  SgInitializedName *objectName = object->get_variables().front();
  size_t objectReferences = 0;
  for (SgNode *node : RoseAst(definition)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    SgVariableSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    if (symbol != nullptr && symbol->get_declaration() == objectName) {
      ++objectReferences;
    }
  }
  ROSE_ASSERT(objectReferences > 0);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  validateDependentMemberTypedefIdentity(project);
  validateSemanticInstantiationPopulation(project);
  return backend(project);
}
