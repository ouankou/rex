#include "RoseAst.h"
#include "rose.h"

#include <map>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

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
