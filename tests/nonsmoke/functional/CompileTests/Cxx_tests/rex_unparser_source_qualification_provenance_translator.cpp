#include "rose.h"

#include <initializer_list>
#include <string>

namespace {
bool hasTokens(const SgStringList &actual,
               std::initializer_list<const char *> expected) {
  if (actual.size() != expected.size()) {
    return false;
  }
  auto actual_it = actual.begin();
  auto expected_it = expected.begin();
  for (; actual_it != actual.end(); ++actual_it, ++expected_it) {
    if (*actual_it != *expected_it) {
      return false;
    }
  }
  return true;
}

SgPointerMemberType *pointerMemberType(SgType *type) {
  while (SgModifierType *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return isSgPointerMemberType(type);
}

SgTypedefType *typedefType(SgType *type) {
  while (SgModifierType *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return isSgTypedefType(type);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t macro_aliases = 0;
  size_t macro_delimiter_aliases = 0;
  size_t global_aliases = 0;
  size_t qualified_member_pointers = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    const std::string name = declaration->get_name().getString();
    if (name == "RexMacroPayload") {
      ROSE_ASSERT(declaration->get_source_base_type_qualification_present());
      ROSE_ASSERT(!declaration->get_source_base_type_global_qualification());
      ROSE_ASSERT(
          hasTokens(declaration->get_source_base_type_qualification_tokens(),
                    {"REX_SOURCE_QUALIFIER_ALIAS::"}));
      ++macro_aliases;
    } else if (name == "RexMacroDelimiterPayload") {
      ROSE_ASSERT(declaration->get_source_base_type_qualification_present());
      ROSE_ASSERT(!declaration->get_source_base_type_global_qualification());
      ROSE_ASSERT(
          hasTokens(declaration->get_source_base_type_qualification_tokens(),
                    {"REX_SOURCE_DELIMITER_QUALIFIER "}));
      ++macro_delimiter_aliases;
    } else if (name == "RexGlobalPayload") {
      ROSE_ASSERT(declaration->get_source_base_type_qualification_present());
      ROSE_ASSERT(declaration->get_source_base_type_global_qualification());
      ROSE_ASSERT(
          hasTokens(declaration->get_source_base_type_qualification_tokens(),
                    {"rex_source_qualification_provenance_target::"}));
      ++global_aliases;
    } else if (name == "RexQualifiedMemberPointer") {
      SgPointerMemberType *member_pointer =
          pointerMemberType(declaration->get_base_type());
      ROSE_ASSERT(member_pointer != nullptr);
      ROSE_ASSERT(member_pointer->get_source_base_type_qualification_present());
      ROSE_ASSERT(!member_pointer->get_source_base_type_global_qualification());
      ROSE_ASSERT(
          hasTokens(member_pointer->get_source_base_type_qualification_tokens(),
                    {"REX_SOURCE_QUALIFIER_ALIAS::"}));
      ++qualified_member_pointers;
    }
  }
  ROSE_ASSERT(macro_aliases == 1);
  ROSE_ASSERT(macro_delimiter_aliases == 1);
  ROSE_ASSERT(global_aliases == 1);
  ROSE_ASSERT(qualified_member_pointers == 1);

  size_t qualified_values = 0;
  size_t namespace_qualified_typedef_values = 0;
  size_t global_qualified_typedef_values = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name() == "rex_namespace_qualified_typedef_value" ||
        name->get_name() == "rex_global_qualified_typedef_value") {
      SgTypedefType *type = typedefType(name->get_type());
      SgTypedefDeclaration *declaration =
          type != nullptr ? isSgTypedefDeclaration(type->get_declaration())
                          : nullptr;
      ROSE_ASSERT(declaration != nullptr);
      ROSE_ASSERT(declaration->get_name() == "QualifiedPayloadAlias");
      ROSE_ASSERT(declaration->get_type() == type);
      ROSE_ASSERT(name->get_source_type_qualification_present());
      ROSE_ASSERT(hasTokens(name->get_source_type_qualification_tokens(),
                            {"rex_source_qualification_provenance_target::"}));
      if (name->get_name() == "rex_namespace_qualified_typedef_value") {
        ROSE_ASSERT(!name->get_source_type_global_qualification());
        ++namespace_qualified_typedef_values;
      } else {
        ROSE_ASSERT(name->get_source_type_global_qualification());
        ++global_qualified_typedef_values;
      }
      continue;
    }
    if (name->get_name() != "rex_source_qualified_value") {
      continue;
    }
    ROSE_ASSERT(name->get_source_type_qualification_present());
    ROSE_ASSERT(!name->get_source_type_global_qualification());
    ROSE_ASSERT(hasTokens(name->get_source_type_qualification_tokens(),
                          {"REX_SOURCE_QUALIFIER_ALIAS::"}));
    ROSE_ASSERT(name->get_source_name_qualification_present());
    ROSE_ASSERT(!name->get_source_name_global_qualification());
    ROSE_ASSERT(name->get_source_name_qualification_tokens().empty());
    ++qualified_values;
  }
  ROSE_ASSERT(qualified_values == 1);
  ROSE_ASSERT(namespace_qualified_typedef_values == 1);
  ROSE_ASSERT(global_qualified_typedef_values == 1);

  size_t qualified_using_declarations = 0;
  size_t pure_global_using_declarations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgUsingDeclarationStatement)) {
    SgUsingDeclarationStatement *declaration =
        isSgUsingDeclarationStatement(node);
    ROSE_ASSERT(declaration != nullptr);
    if (!declaration->get_source_name_qualification_present()) {
      continue;
    }
    if (declaration->get_source_terminal_name() ==
        "rex_source_global_using_value") {
      ROSE_ASSERT(declaration->get_source_name_global_qualification());
      ROSE_ASSERT(declaration->get_source_name_qualification_tokens().empty());
      ROSE_ASSERT(declaration->get_global_qualification_required());
      ROSE_ASSERT(declaration->get_name_qualification_length() == 0);
      SgVariableDeclaration *target =
          isSgVariableDeclaration(declaration->get_declaration());
      ROSE_ASSERT(target != nullptr);
      ROSE_ASSERT(target->get_variables().size() == 1);
      ROSE_ASSERT(target->get_variables().front()->get_name() ==
                  "rex_source_global_using_value");
      ++pure_global_using_declarations;
      continue;
    }
    if (!hasTokens(declaration->get_source_name_qualification_tokens(),
                   {"REX_SOURCE_QUALIFIER_ALIAS::"})) {
      continue;
    }
    ROSE_ASSERT(!declaration->get_source_name_global_qualification());
    ++qualified_using_declarations;
  }
  ROSE_ASSERT(qualified_using_declarations == 1);
  ROSE_ASSERT(pure_global_using_declarations == 1);

  size_t unqualified_forward_enums = 0;
  size_t qualified_enums = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgEnumDeclaration)) {
    SgEnumDeclaration *declaration = isSgEnumDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != "Mode" ||
        !declaration->get_source_name_qualification_present()) {
      continue;
    }
    ROSE_ASSERT(!declaration->get_source_name_global_qualification());
    if (declaration->get_source_name_qualification_tokens().empty()) {
      ROSE_ASSERT(declaration->isForward());
      ROSE_ASSERT(declaration->get_name_qualification_length() == 0);
      ROSE_ASSERT(!declaration->get_global_qualification_required());
      ++unqualified_forward_enums;
      continue;
    }
    ROSE_ASSERT(!declaration->isForward());
    ROSE_ASSERT(hasTokens(declaration->get_source_name_qualification_tokens(),
                          {"REX_SOURCE_QUALIFIER_ALIAS::"}));
    ++qualified_enums;
  }
  ROSE_ASSERT(unqualified_forward_enums == 1);
  ROSE_ASSERT(qualified_enums == 1);

  size_t qualified_functions = 0;
  size_t dependent_functions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    if (!declaration->get_source_name_qualification_present()) {
      continue;
    }
    if (declaration->get_name() == "function") {
      ROSE_ASSERT(!declaration->get_source_name_global_qualification());
      ROSE_ASSERT(hasTokens(declaration->get_source_name_qualification_tokens(),
                            {"REX_SOURCE_QUALIFIER_ALIAS::"}));
      ++qualified_functions;
    } else if (declaration->get_name() == "convert") {
      ROSE_ASSERT(!declaration->get_source_name_global_qualification());
      ROSE_ASSERT(
          hasTokens(declaration->get_source_name_qualification_tokens(),
                    {"RexSourceQualificationOuter<T>::", "Inner<U>::"}));
      ++dependent_functions;
    }
  }
  ROSE_ASSERT(qualified_functions == 1);
  ROSE_ASSERT(dependent_functions == 1);

  size_t macro_base_names = 0;
  size_t macro_base_types = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDefinition)) {
    SgClassDefinition *definition = isSgClassDefinition(node);
    ROSE_ASSERT(definition != nullptr);
    SgClassDeclaration *declaration = definition->get_declaration();
    if (declaration == nullptr || definition->get_inheritances().size() != 1) {
      continue;
    }
    SgBaseClass *base = definition->get_inheritances().front();
    ROSE_ASSERT(base != nullptr);
    if (declaration->get_name() == "RexSourceMacroBaseName") {
      ROSE_ASSERT(base->get_source_type_qualification_present());
      ROSE_ASSERT(hasTokens(base->get_source_type_qualification_tokens(),
                            {"REX_SOURCE_BASE_NAME "}));
      ROSE_ASSERT(base->get_source_type_qualification_owns_terminal_name());
      ROSE_ASSERT(
          !base->get_source_type_qualification_owns_template_arguments());
      ++macro_base_names;
    } else if (declaration->get_name() == "RexSourceMacroBaseType") {
      ROSE_ASSERT(base->get_source_type_qualification_present());
      ROSE_ASSERT(hasTokens(base->get_source_type_qualification_tokens(),
                            {"REX_SOURCE_BASE_TYPE(T) "}));
      ROSE_ASSERT(base->get_source_type_qualification_owns_terminal_name());
      ROSE_ASSERT(
          base->get_source_type_qualification_owns_template_arguments());
      ++macro_base_types;
    }
  }
  ROSE_ASSERT(macro_base_names == 1);
  ROSE_ASSERT(macro_base_types == 1);

  return backend(project);
}
