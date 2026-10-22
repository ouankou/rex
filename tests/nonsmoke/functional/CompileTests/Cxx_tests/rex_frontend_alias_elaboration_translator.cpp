#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <map>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ASSERT_not_null(sourceFile);
  SgGlobal *global = sourceFile->get_globalScope();
  ASSERT_not_null(global);
  for (const char *name :
       {"RexAliasRedeclaredStruct", "RexAliasForwardClass"}) {
    SgClassDeclaration *forward = nullptr;
    for (SgDeclarationStatement *statement : global->get_declarations()) {
      SgClassDeclaration *declaration = isSgClassDeclaration(statement);
      if (declaration != nullptr && declaration->get_name() == name) {
        ROSE_ASSERT(forward == nullptr);
        forward = declaration;
      }
    }
    ASSERT_not_null(forward);
    ROSE_ASSERT(forward->isForward());
    ROSE_ASSERT(forward->get_isAutonomousDeclaration());
    ROSE_ASSERT(forward->get_parent() == global);
    ROSE_ASSERT(forward->get_scope() == global);
    ROSE_ASSERT(!forward->get_file_info()->isCompilerGenerated());
    SgClassDeclaration *definition =
        isSgClassDeclaration(forward->get_definingDeclaration());
    ASSERT_not_null(definition);
    SgVariableDeclaration *owner =
        isSgVariableDeclaration(definition->get_parent());
    ASSERT_not_null(owner);
    ROSE_ASSERT(owner->get_baseTypeDefiningDeclaration() == definition);
    ROSE_ASSERT(!definition->get_isAutonomousDeclaration());
    ROSE_ASSERT(definition->get_type() == forward->get_type());
    ASSERT_not_null(definition->get_definition());
    ROSE_ASSERT(definition->get_definition()->get_declaration() == definition);
    ROSE_ASSERT(definition->get_definition()->get_parent() == definition);
    ROSE_ASSERT(std::count(global->get_declarations().begin(),
                           global->get_declarations().end(), definition) == 0);
  }
  for (const char *name : {"RexAliasRedeclaredEnum", "RexAliasForwardEnum"}) {
    SgEnumDeclaration *forward = nullptr;
    for (SgDeclarationStatement *statement : global->get_declarations()) {
      SgEnumDeclaration *declaration = isSgEnumDeclaration(statement);
      if (declaration != nullptr && declaration->get_name() == name) {
        ROSE_ASSERT(forward == nullptr);
        forward = declaration;
      }
    }
    ASSERT_not_null(forward);
    ROSE_ASSERT(forward->isForward());
    ROSE_ASSERT(forward->get_isAutonomousDeclaration());
    ROSE_ASSERT(forward->get_parent() == global);
    ROSE_ASSERT(forward->get_scope() == global);
    ROSE_ASSERT(!forward->get_file_info()->isCompilerGenerated());
    SgEnumDeclaration *definition =
        isSgEnumDeclaration(forward->get_definingDeclaration());
    ASSERT_not_null(definition);
    SgVariableDeclaration *owner =
        isSgVariableDeclaration(definition->get_parent());
    ASSERT_not_null(owner);
    ROSE_ASSERT(owner->get_baseTypeDefiningDeclaration() == definition);
    ROSE_ASSERT(!definition->get_isAutonomousDeclaration());
    ROSE_ASSERT(definition->get_type() == forward->get_type());
    ROSE_ASSERT(std::count(global->get_declarations().begin(),
                           global->get_declarations().end(), definition) == 0);
  }

  const std::map<std::string, bool> expected = {
      {"RexExplicitStructAlias", true}, {"RexPlainStructAlias", false},
      {"RexExplicitClassAlias", true},  {"RexPlainClassAlias", false},
      {"RexExplicitEnumAlias", true},   {"RexPlainEnumAlias", false},
  };
  std::map<std::string, SgTypedefDeclaration *> aliases;
  for (SgNode *node : RoseAst(project)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr) {
      continue;
    }
    const std::string name = declaration->get_name().getString();
    if (expected.count(name) != 0) {
      ROSE_ASSERT(aliases.emplace(name, declaration).second);
    }
  }

  ROSE_ASSERT(aliases.size() == expected.size());
  for (const auto &entry : expected) {
    SgTypedefDeclaration *declaration = aliases.at(entry.first);
    ROSE_ASSERT(declaration->get_base_type() != nullptr);
    ROSE_ASSERT(declaration->get_type_elaboration_required_for_base_type() ==
                entry.second);
  }

  size_t source_type_arguments = 0;
  size_t elaborated_type_arguments = 0;
  size_t unelaborated_type_arguments = 0;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateArgument *argument = isSgTemplateArgument(node);
    if (argument == nullptr ||
        argument->get_argumentType() != SgTemplateArgument::type_argument ||
        argument->get_sourceSpelledType() == nullptr) {
      continue;
    }
    ++source_type_arguments;
    ROSE_ASSERT(argument->get_source_type_qualification_present());
    ROSE_ASSERT(!argument->get_source_type_global_qualification());
    ROSE_ASSERT(argument->get_source_type_qualification_tokens().empty());
    if (argument->get_type_elaboration_required()) {
      ++elaborated_type_arguments;
    } else {
      ++unelaborated_type_arguments;
    }
  }
  ROSE_ASSERT(source_type_arguments >= 2);
  ROSE_ASSERT(elaborated_type_arguments >= 1);
  ROSE_ASSERT(unelaborated_type_arguments >= 1);

  AstTests::runAllTests(project);
  return backend(project);
}
