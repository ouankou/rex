#include "rose.h"

#include "AstInterface_ROSE.h"
#include "RoseAst.h"
#include "dependence_analysis.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool rejectInitializationTarget(const std::string &dependences,
                                const std::string &target) {
  const std::string unexpected = target + " : [ read ] ";
  if (dependences.find(unexpected) == std::string::npos)
    return true;
  std::cerr << "local initializer escaped into the whole-program dependence "
               "table as '"
            << unexpected << "':\n"
            << dependences;
  return false;
}

SgTemplateVariableDeclaration *
findTemplateVariableDeclaration(SgProject *project, const std::string &name) {
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgTemplateVariableDeclaration *declaration =
        isSgTemplateVariableDeclaration(*node);
    if (declaration != nullptr && declaration->get_variables().size() == 1 &&
        declaration->get_variables().front()->get_name().getString() == name)
      return declaration;
  }
  std::cerr << "template variable declaration '" << name << "' was not found"
            << std::endl;
  return nullptr;
}

bool requireDerivedVariableDeclaration(SgVariableDeclaration *declaration) {
  ASSERT_not_null(declaration);
  ASSERT_require(declaration->get_variables().size() == 1);
  SgInitializedName *target = declaration->get_variables().front();
  ASSERT_not_null(target);

  AstInterface::AstNodeList variables;
  AstInterface::AstNodeList initializers;
  if (!AstInterface::IsVariableDecl(declaration, &variables, &initializers) ||
      variables.size() != 1 || initializers.size() != 1 ||
      AstNodePtrImpl(variables.front()).get_ptr() != target) {
    std::cerr << "declaration '" << target->get_name().getString()
              << "' was not recognized through its variable declaration base"
              << std::endl;
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);

  SgTemplateVariableDeclaration *template_variable =
      findTemplateVariableDeclaration(project, "rex_template_variable");
  if (template_variable == nullptr ||
      !requireDerivedVariableDeclaration(template_variable))
    return 1;

  AstUtilInterface::WholeProgramDependenceAnalysis analysis;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    ASSERT_not_null(source_file);
    SgGlobal *global = source_file->get_globalScope();
    ASSERT_not_null(global);
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      ASSERT_not_null(declaration);
      analysis.ComputeDependences(declaration, global);
    }
  }

  std::ostringstream output;
  analysis.OutputDependences(output);
  const std::string dependences = output.str();
  if (dependences.find("init") == std::string::npos ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::local") ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::function_static")) {
    std::cerr << "global initializer dependence was not recorded:\n"
              << dependences;
    return 1;
  }

  return 0;
}
