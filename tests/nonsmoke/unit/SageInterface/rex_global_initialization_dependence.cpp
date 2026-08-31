#include "rose.h"

#include "AstInterface_ROSE.h"
#include "AstUtilInterface.h"
#include "RoseAst.h"
#include "dependence_analysis.h"
#include "dependence_table.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool requireDependenceTokenBoundaries() {
  const std::vector<std::string> operator_names = {
      "operator[]",        "operator()",      "operator new",
      "operator new[]",    "operator delete", "operator delete[]",
      "operator co_await", "operator+",       "operator-",
      "operator*",         "operator/",       "operator%",
      "operator^",         "operator&",       "operator|",
      "operator~",         "operator!",       "operator=",
      "operator<",         "operator>",       "operator+=",
      "operator-=",        "operator*=",      "operator/=",
      "operator%=",        "operator^=",      "operator&=",
      "operator|=",        "operator<<",      "operator>>",
      "operator>>=",       "operator<<=",     "operator==",
      "operator!=",        "operator<=",      "operator>=",
      "operator<=>",       "operator&&",      "operator||",
      "operator++",        "operator--",      "operator,",
      "operator->*",       "operator->",      "operator bool",
      "operator\"\"_tag",  "rex::operator[]", "rex::operator=="};

  std::ostringstream serialized;
  for (const std::string &name : operator_names) {
    serialized << AstUtilInterface::DependenceEntry(name, "source", "call",
                                                    "exact")
               << '\n';
  }
  serialized << AstUtilInterface::DependenceEntry("cooperator", "source",
                                                  "read", "exact")
             << '\n'
             << AstUtilInterface::DependenceEntry("target", "cooperator",
                                                  "read", "exact")
             << '\n';

  std::istringstream input(serialized.str());
  AstUtilInterface::SelectDependences dependences;
  dependences.CollectFromFile(input);

  const std::vector<AstUtilInterface::DependenceEntry> &entries =
      dependences.get_result();
  const auto contains = [&entries](const std::string &first,
                                   const std::string &second,
                                   const std::string &type) {
    for (const AstUtilInterface::DependenceEntry &entry : entries) {
      if (entry.first_entry() == first && entry.second_entry() == second &&
          entry.type_entry() == type && entry.attr_entry() == "exact")
        return true;
    }
    return false;
  };
  bool complete = entries.size() == operator_names.size() + 2 &&
                  contains("cooperator", "source", "read") &&
                  contains("target", "cooperator", "read");
  for (const std::string &name : operator_names)
    complete = complete && contains(name, "source", "call");
  if (complete)
    return true;

  std::cerr << "dependence-table operator and identifier boundaries did not "
               "round-trip exactly"
            << std::endl;
  return false;
}

bool requireInitializationInput(const std::string &dependences,
                                const std::string &target,
                                const std::string &input) {
  const std::string expected = target + " : [ read ] " + input + " = init ;";
  if (dependences.find(expected) != std::string::npos)
    return true;
  std::cerr << "missing global initializer input dependence '" << expected
            << "':\n"
            << dependences;
  return false;
}

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

SgVariableDeclaration *findVariableDeclaration(SgProject *project,
                                               const std::string &name) {
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(*node);
    if (declaration != nullptr && declaration->get_variables().size() == 1 &&
        declaration->get_variables().front()->get_name().getString() == name)
      return declaration;
  }
  std::cerr << "declaration '" << name << "' was not found" << std::endl;
  return nullptr;
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

bool requireExactDeclarationEffects(SgVariableDeclaration *declaration) {
  ASSERT_not_null(declaration);
  ASSERT_require(declaration->get_variables().size() == 1);
  SgInitializedName *target = declaration->get_variables().front();
  ASSERT_not_null(target);

  AstInterface::AstNodeList variables;
  AstInterface::AstNodeList initializers;
  if (!AstInterface::IsVariableDecl(declaration, &variables, &initializers) ||
      variables.size() != 1 || initializers.size() != 1 ||
      AstNodePtrImpl(variables.front()).get_ptr() != target) {
    std::cerr << "declaration list for '" << target->get_name().getString()
              << "' contains aggregate designators or lost its exact target"
              << std::endl;
    return false;
  }

  bool saw_target_declaration = false;
  bool saw_spurious_declaration = false;
  std::function<bool(const AstNodePtr &, const AstNodePtr &,
                     AstUtilInterface::OperatorSideEffect)>
      collect = [target, &saw_target_declaration, &saw_spurious_declaration](
                    const AstNodePtr &node, const AstNodePtr &,
                    AstUtilInterface::OperatorSideEffect relation) {
        if (relation != AstUtilInterface::OperatorSideEffect::Decl)
          return true;
        if (AstNodePtrImpl(node).get_ptr() == target) {
          saw_target_declaration = true;
        } else {
          saw_spurious_declaration = true;
        }
        return true;
      };
  AstUtilInterface::ComputeAstSideEffects(declaration, &collect, nullptr);
  if (!saw_target_declaration || saw_spurious_declaration) {
    std::cerr << "declaration effects for '" << target->get_name().getString()
              << "' did not contain exactly the initialized object"
              << std::endl;
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (!requireDependenceTokenBoundaries())
    return 1;

  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);

  SgTemplateVariableDeclaration *template_variable =
      findTemplateVariableDeclaration(project, "rex_template_variable");
  if (template_variable == nullptr ||
      !requireExactDeclarationEffects(template_variable))
    return 1;

  SgVariableDeclaration *designated_pair =
      findVariableDeclaration(project, "designated_pair");
  if (designated_pair == nullptr ||
      !requireExactDeclarationEffects(designated_pair))
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
  if (!requireInitializationInput(dependences, "target", "source") ||
      !requireInitializationInput(dependences, "target", "delta") ||
      !requireInitializationInput(dependences, "positional_pair", "source") ||
      !requireInitializationInput(dependences, "positional_pair", "delta") ||
      !requireInitializationInput(dependences, "designated_pair", "source") ||
      !requireInitializationInput(dependences, "designated_pair", "delta") ||
      !requireInitializationInput(dependences, "rex_nested::nested_target",
                                  "source") ||
      !requireInitializationInput(dependences, "rex_nested::nested_target",
                                  "delta") ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::local") ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::function_static")) {
    return 1;
  }

  return 0;
}
