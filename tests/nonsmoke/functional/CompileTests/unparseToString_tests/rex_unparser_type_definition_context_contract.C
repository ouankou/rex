#include "rose.h"

#include "unparser.h"

#include <iostream>
#include <string>

namespace {

struct EmbeddedClassTypedef {
  SgClassDeclaration *definition = nullptr;
  SgTypedefDeclaration *owner = nullptr;
};

struct EmbeddedEnumTypedef {
  SgEnumDeclaration *definition = nullptr;
  SgTypedefDeclaration *owner = nullptr;
};

EmbeddedClassTypedef buildEmbeddedClassTypedef(const std::string &tag_name,
                                               const std::string &alias_name,
                                               SgGlobal *scope) {
  EmbeddedClassTypedef result;
  result.definition = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::embeddedDeclaratorChild(), tag_name,
      scope);
  result.owner = SageBuilder::buildTypedefDeclarationWithEmbeddedTag(
      SageBuilder::typedef_declaration_ownership::sourceLexical(),
      SgTypedefDeclaration::e_typedef, alias_name,
      result.definition->get_type(), scope, result.definition);
  ROSE_ASSERT(result.definition->get_parent() == result.owner);
  ROSE_ASSERT(result.owner->get_baseTypeDefiningDeclaration() ==
              result.definition);
  return result;
}

EmbeddedEnumTypedef buildEmbeddedEnumTypedef(const std::string &tag_name,
                                             const std::string &alias_name,
                                             SgGlobal *scope) {
  EmbeddedEnumTypedef result;
  result.definition = SageBuilder::buildEnumDeclaration(
      SageBuilder::declaration_ownership::embeddedDeclaratorChild(), tag_name,
      false, scope);
  result.owner = SageBuilder::buildTypedefDeclarationWithEmbeddedTag(
      SageBuilder::typedef_declaration_ownership::sourceLexical(),
      SgTypedefDeclaration::e_typedef, alias_name,
      result.definition->get_type(), scope, result.definition);
  ROSE_ASSERT(result.definition->get_parent() == result.owner);
  ROSE_ASSERT(result.owner->get_baseTypeDefiningDeclaration() ==
              result.definition);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    return 2;
  }
  const std::string mode = argv[1];
  if (mode != "unrelated-class-context" && mode != "unrelated-enum-context") {
    return 3;
  }

  SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(
      "rex_unparser_type_definition_context_contract.cpp");
  ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
  source->set_Cxx_only(true);
  source->set_outputLanguage(SgFile::e_Cxx_language);
  SgGlobal *scope = source->get_globalScope();

  SgNamedType *target_type = nullptr;
  SgTypedefDeclaration *unrelated_owner = nullptr;
  if (mode == "unrelated-class-context") {
    EmbeddedClassTypedef target = buildEmbeddedClassTypedef(
        "RexTargetClass", "RexTargetClassAlias", scope);
    EmbeddedClassTypedef unrelated = buildEmbeddedClassTypedef(
        "RexUnrelatedClass", "RexUnrelatedClassAlias", scope);
    target_type = target.definition->get_type();
    unrelated_owner = unrelated.owner;
  } else {
    EmbeddedEnumTypedef target =
        buildEmbeddedEnumTypedef("RexTargetEnum", "RexTargetEnumAlias", scope);
    EmbeddedEnumTypedef unrelated = buildEmbeddedEnumTypedef(
        "RexUnrelatedEnum", "RexUnrelatedEnumAlias", scope);
    target_type = target.definition->get_type();
    unrelated_owner = unrelated.owner;
  }
  ROSE_ASSERT(target_type != nullptr && unrelated_owner != nullptr);

  // The hard contract must run before the type unparser writes even a keyword.
  std::cout << "REX_TYPE_DEFINITION_OUTPUT_SENTINEL\n" << std::flush;
  Unparser_Opt options;
  Unparser unparser(
      &std::cout, "rex_unparser_type_definition_context_contract.cpp", options);
  unparser.currentFile = source;
  SgUnparse_Info info;
  info.set_current_source_file(source);
  info.set_language(SgFile::e_Cxx_language);
  info.set_declstatement_ptr(unrelated_owner);
  info.set_isTypeFirstPart();
  unparser.u_type->unparseType(target_type, info);
  return 99;
}
