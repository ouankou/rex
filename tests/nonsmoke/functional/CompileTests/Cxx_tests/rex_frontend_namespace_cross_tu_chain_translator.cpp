#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

bool endsWith(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

struct TranslationUnitDeclarations {
  SgGlobal *global = nullptr;
  SgNamespaceDeclarationStatement *namespace_declaration = nullptr;
  SgClassDeclaration *class_declaration = nullptr;
  SgEnumDeclaration *enum_declaration = nullptr;
  std::vector<SgFunctionDeclaration *> functions;
};

TranslationUnitDeclarations collectTranslationUnit(SgSourceFile *source_file) {
  ROSE_ASSERT(source_file != nullptr);
  TranslationUnitDeclarations result;
  result.global = source_file->get_globalScope();
  ROSE_ASSERT(result.global != nullptr);

  for (SgDeclarationStatement *declaration :
       result.global->get_declarations()) {
    SgNamespaceDeclarationStatement *namespace_declaration =
        isSgNamespaceDeclarationStatement(declaration);
    if (namespace_declaration == nullptr ||
        namespace_declaration->get_name().getString() !=
            "rex_cross_tu_namespace") {
      continue;
    }
    ROSE_ASSERT(result.namespace_declaration == nullptr);
    result.namespace_declaration = namespace_declaration;
  }
  ROSE_ASSERT(result.namespace_declaration != nullptr);

  SgNamespaceDefinitionStatement *definition =
      result.namespace_declaration->get_definition();
  ROSE_ASSERT(definition != nullptr);
  for (SgDeclarationStatement *declaration : definition->get_declarations()) {
    SgClassDeclaration *class_declaration = isSgClassDeclaration(declaration);
    if (class_declaration != nullptr &&
        class_declaration->get_name().getString() == "rex_cross_tu_record" &&
        class_declaration->get_definition() != nullptr) {
      ROSE_ASSERT(result.class_declaration == nullptr);
      result.class_declaration = class_declaration;
    }
    SgEnumDeclaration *enum_declaration = isSgEnumDeclaration(declaration);
    if (enum_declaration != nullptr &&
        enum_declaration->get_name().getString() == "rex_cross_tu_enum" &&
        !enum_declaration->isForward()) {
      ROSE_ASSERT(result.enum_declaration == nullptr);
      result.enum_declaration = enum_declaration;
    }
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    if (function != nullptr &&
        function->get_name().getString() == "rex_cross_tu_function") {
      result.functions.push_back(function);
    }
  }
  ROSE_ASSERT(result.class_declaration != nullptr);
  ROSE_ASSERT(result.enum_declaration != nullptr);
  SgNamespaceDefinitionStatement *semantic_definition =
      definition->get_global_definition();
  ROSE_ASSERT(semantic_definition != nullptr);
  ROSE_ASSERT(result.enum_declaration->get_parent() == definition);
  ROSE_ASSERT(result.enum_declaration->get_scope() == semantic_definition);
  ROSE_ASSERT(result.class_declaration->get_parent() == definition);
  ROSE_ASSERT(result.class_declaration->get_scope() == semantic_definition);
  ROSE_ASSERT(result.class_declaration->get_type() != nullptr);
  ROSE_ASSERT(semantic_definition->get_type_table()->lookup_type(
                  result.class_declaration->get_type()->get_mangled()) ==
              result.class_declaration->get_type());
  ROSE_ASSERT(result.enum_declaration->get_type() != nullptr);
  ROSE_ASSERT(semantic_definition->get_type_table()->lookup_type(
                  result.enum_declaration->get_type()->get_mangled()) ==
              result.enum_declaration->get_type());
  std::sort(
      result.functions.begin(), result.functions.end(),
      [](const SgFunctionDeclaration *lhs, const SgFunctionDeclaration *rhs) {
        return *lhs->get_translation_unit_source_order() <
               *rhs->get_translation_unit_source_order();
      });

  for (SgFunctionDeclaration *function : result.functions) {
    ROSE_ASSERT(function->get_parent() == definition);
    ROSE_ASSERT(function->get_scope() == semantic_definition);
    ROSE_ASSERT(SageInterface::getGlobalScope(function) == result.global);
    ROSE_ASSERT(function->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(function->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(
        function->get_startOfConstruct()->get_source_sequence_number() ==
        *function->get_translation_unit_source_order());
  }
  return result;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 2);

  TranslationUnitDeclarations first;
  TranslationUnitDeclarations second;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    ROSE_ASSERT(source_file != nullptr);
    if (endsWith(source_file->getFileName(),
                 "rex_frontend_namespace_cross_tu_chain_a.cpp")) {
      first = collectTranslationUnit(source_file);
    } else if (endsWith(source_file->getFileName(),
                        "rex_frontend_namespace_cross_tu_chain_b.cpp")) {
      second = collectTranslationUnit(source_file);
    } else {
      ROSE_ABORT();
    }
  }

  ROSE_ASSERT(first.global != nullptr && second.global != nullptr);
  ROSE_ASSERT(first.global != second.global);
  ROSE_ASSERT(first.namespace_declaration != second.namespace_declaration);
  ROSE_ASSERT(first.class_declaration != second.class_declaration);
  ROSE_ASSERT(first.enum_declaration != second.enum_declaration);
  ROSE_ASSERT(first.class_declaration->get_type() !=
              second.class_declaration->get_type());
  ROSE_ASSERT(first.enum_declaration->get_type() !=
              second.enum_declaration->get_type());
  SgClassDeclaration *first_canonical_class = isSgClassDeclaration(
      first.class_declaration->get_firstNondefiningDeclaration());
  SgClassDeclaration *second_canonical_class = isSgClassDeclaration(
      second.class_declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first_canonical_class != nullptr);
  ROSE_ASSERT(second_canonical_class != nullptr);
  ROSE_ASSERT(first_canonical_class != second_canonical_class);
  ROSE_ASSERT(first_canonical_class != first.class_declaration);
  ROSE_ASSERT(second_canonical_class != second.class_declaration);
  ROSE_ASSERT(first_canonical_class->get_firstNondefiningDeclaration() ==
              first_canonical_class);
  ROSE_ASSERT(second_canonical_class->get_firstNondefiningDeclaration() ==
              second_canonical_class);
  ROSE_ASSERT(first.class_declaration->get_definingDeclaration() ==
              first.class_declaration);
  ROSE_ASSERT(second.class_declaration->get_definingDeclaration() ==
              second.class_declaration);
  ROSE_ASSERT(first_canonical_class->get_definingDeclaration() ==
              first.class_declaration);
  ROSE_ASSERT(second_canonical_class->get_definingDeclaration() ==
              second.class_declaration);
  ROSE_ASSERT(
      isSgClassType(first.class_declaration->get_type())->get_declaration() ==
      first_canonical_class);
  ROSE_ASSERT(
      isSgClassType(second.class_declaration->get_type())->get_declaration() ==
      second_canonical_class);
  ROSE_ASSERT(SageInterface::getGlobalScope(first_canonical_class) ==
              first.global);
  ROSE_ASSERT(SageInterface::getGlobalScope(second_canonical_class) ==
              second.global);
  SgEnumDeclaration *first_canonical_enum = isSgEnumDeclaration(
      first.enum_declaration->get_firstNondefiningDeclaration());
  SgEnumDeclaration *second_canonical_enum = isSgEnumDeclaration(
      second.enum_declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first_canonical_enum != nullptr);
  ROSE_ASSERT(second_canonical_enum != nullptr);
  ROSE_ASSERT(first_canonical_enum != second_canonical_enum);
  ROSE_ASSERT(first_canonical_enum != first.enum_declaration);
  ROSE_ASSERT(second_canonical_enum != second.enum_declaration);
  ROSE_ASSERT(first_canonical_enum->get_firstNondefiningDeclaration() ==
              first_canonical_enum);
  ROSE_ASSERT(second_canonical_enum->get_firstNondefiningDeclaration() ==
              second_canonical_enum);
  ROSE_ASSERT(first.enum_declaration->get_definingDeclaration() ==
              first.enum_declaration);
  ROSE_ASSERT(second.enum_declaration->get_definingDeclaration() ==
              second.enum_declaration);
  ROSE_ASSERT(first_canonical_enum->get_definingDeclaration() ==
              first.enum_declaration);
  ROSE_ASSERT(second_canonical_enum->get_definingDeclaration() ==
              second.enum_declaration);
  ROSE_ASSERT(
      isSgEnumType(first.enum_declaration->get_type())->get_declaration() ==
      first_canonical_enum);
  ROSE_ASSERT(
      isSgEnumType(second.enum_declaration->get_type())->get_declaration() ==
      second_canonical_enum);
  ROSE_ASSERT(SageInterface::getGlobalScope(first_canonical_enum) ==
              first.global);
  ROSE_ASSERT(SageInterface::getGlobalScope(second_canonical_enum) ==
              second.global);
  ROSE_ASSERT(first.namespace_declaration->get_firstNondefiningDeclaration() ==
              first.namespace_declaration);
  ROSE_ASSERT(second.namespace_declaration->get_firstNondefiningDeclaration() ==
              second.namespace_declaration);
  SgNamespaceDefinitionStatement *first_namespace_definition =
      first.namespace_declaration->get_definition();
  SgNamespaceDefinitionStatement *second_namespace_definition =
      second.namespace_declaration->get_definition();
  ROSE_ASSERT(first_namespace_definition != nullptr);
  ROSE_ASSERT(second_namespace_definition != nullptr);
  ROSE_ASSERT(first_namespace_definition != second_namespace_definition);
  ROSE_ASSERT(first_namespace_definition->get_global_definition() ==
              first_namespace_definition);
  ROSE_ASSERT(second_namespace_definition->get_global_definition() ==
              second_namespace_definition);
  ROSE_ASSERT(SageInterface::getGlobalScope(first.namespace_declaration) ==
              first.global);
  ROSE_ASSERT(SageInterface::getGlobalScope(second.namespace_declaration) ==
              second.global);
  ROSE_ASSERT(first.functions.size() == 1);
  ROSE_ASSERT(second.functions.size() == 2);
  ROSE_ASSERT(
      first.namespace_declaration->get_translation_unit_source_order() ==
      second.namespace_declaration->get_translation_unit_source_order());
  ROSE_ASSERT(first.functions[0]->get_translation_unit_source_order() ==
              second.functions[0]->get_translation_unit_source_order());

  SgFunctionDeclaration *first_declaration = first.functions[0];
  SgFunctionDeclaration *second_declaration = second.functions[0];
  SgFunctionDeclaration *definition = second.functions[1];
  ROSE_ASSERT(definition->get_definition() != nullptr);
  ROSE_ASSERT(first_declaration->get_firstNondefiningDeclaration() ==
              first_declaration);
  ROSE_ASSERT(first_declaration->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(second_declaration->get_firstNondefiningDeclaration() ==
              second_declaration);
  ROSE_ASSERT(definition->get_firstNondefiningDeclaration() ==
              second_declaration);
  ROSE_ASSERT(second_declaration->get_definingDeclaration() == definition);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);

  AstTests::runAllTests(project);
  return 0;
}
