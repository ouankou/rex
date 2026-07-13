#include "rose.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-signature-contract]: " << reason
            << "\n";
  std::exit(1);
}

void validateSignature(SgFunctionDeclaration *definition,
                       const std::string &mode) {
  SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
      definition != nullptr ? definition->get_firstNondefiningDeclaration()
                            : nullptr);
  SgFunctionParameterList *definitionParameters =
      definition != nullptr ? definition->get_parameterList() : nullptr;
  SgFunctionParameterList *canonicalParameters =
      canonical != nullptr ? canonical->get_parameterList() : nullptr;
  SgFunctionType *definitionType =
      definition != nullptr ? definition->get_type() : nullptr;

  if (definition == nullptr || definition->get_definition() == nullptr ||
      definition->get_definingDeclaration() != definition ||
      canonical == nullptr || canonical == definition ||
      canonical->get_firstNondefiningDeclaration() != canonical ||
      canonical->get_definingDeclaration() != definition ||
      definitionParameters == nullptr ||
      definitionParameters->get_parent() != definition ||
      canonicalParameters == nullptr ||
      canonicalParameters == definitionParameters ||
      canonicalParameters->get_parent() != canonical ||
      definitionType == nullptr || canonical->get_type() != definitionType) {
    fail("defining and canonical declarations are not one exact typed family");
  }

  const SgInitializedNamePtrList &definitionArguments =
      definitionParameters->get_args();
  const SgInitializedNamePtrList &canonicalArguments =
      canonicalParameters->get_args();
  const SgTypePtrList &typeArguments = definitionType->get_arguments();
  if (definitionArguments.size() != canonicalArguments.size() ||
      definitionArguments.size() != typeArguments.size()) {
    fail("parameter-list and function-type arities differ");
  }
  if ((mode == "wrapper" && definitionArguments.size() != 1) ||
      (mode == "direct" && definitionArguments.size() < 2)) {
    fail("outlined signature has the wrong mode-specific arity");
  }

  for (size_t index = 0; index < definitionArguments.size(); ++index) {
    SgInitializedName *definitionArgument = definitionArguments[index];
    SgInitializedName *canonicalArgument = canonicalArguments[index];
    if (definitionArgument == nullptr || canonicalArgument == nullptr ||
        definitionArgument == canonicalArgument ||
        definitionArgument->get_parent() != definitionParameters ||
        canonicalArgument->get_parent() != canonicalParameters ||
        definitionArgument->get_name() != canonicalArgument->get_name() ||
        definitionArgument->get_type() != canonicalArgument->get_type() ||
        definitionArgument->get_type() != typeArguments[index]) {
      fail("parameter differs across declaration and function-type surfaces");
    }
  }
}

} // namespace

int main(int argc, char *argv[]) {
  std::vector<std::string> arguments(argv, argv + argc);
  std::string mode;
  for (auto argument = arguments.begin(); argument != arguments.end();) {
    const std::string prefix = "--rex-signature-mode=";
    if (argument->compare(0, prefix.size(), prefix) == 0) {
      mode = argument->substr(prefix.size());
      argument = arguments.erase(argument);
    } else {
      ++argument;
    }
  }
  if (mode != "direct" && mode != "wrapper") {
    fail("expected --rex-signature-mode=direct or wrapper");
  }

  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr) {
    fail("frontend returned a null project");
  }
  AstTests::runAllTests(project);

  std::set<SgFunctionDeclaration *> originalDefinitions;
  RoseAst originalAst(project);
  for (RoseAst::iterator node = originalAst.begin(); node != originalAst.end();
       ++node) {
    SgFunctionDeclaration *definition = isSgFunctionDeclaration(*node);
    if (definition != nullptr && definition->get_definition() != nullptr) {
      originalDefinitions.insert(definition);
    }
  }

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);

  size_t outlinedDefinitions = 0;
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgFunctionDeclaration *definition = isSgFunctionDeclaration(*node);
    if (definition == nullptr || definition->get_definition() == nullptr ||
        originalDefinitions.count(definition) != 0) {
      continue;
    }
    validateSignature(definition, mode);
    ++outlinedDefinitions;
  }
  if (outlinedDefinitions != 1) {
    fail("expected exactly one outlined defining declaration");
  }
  return 0;
}
