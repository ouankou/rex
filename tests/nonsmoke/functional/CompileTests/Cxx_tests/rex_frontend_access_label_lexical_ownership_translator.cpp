#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

std::string variableName(SgDeclarationStatement *declaration) {
  SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
  if (variable == nullptr || variable->get_variables().size() != 1 ||
      variable->get_variables().front() == nullptr) {
    return {};
  }
  return variable->get_variables().front()->get_name().getString();
}

std::string accessName(const SgAccessModifier &access) {
  if (access.isPrivate()) {
    return "private";
  }
  if (access.isProtected()) {
    return "protected";
  }
  if (access.isPublic()) {
    return "public";
  }
  return "invalid";
}

std::string accessName(SgAccessLabelStatement::access_label_kind_enum label) {
  switch (label) {
  case SgAccessLabelStatement::e_access_label_private:
    return "private";
  case SgAccessLabelStatement::e_access_label_protected:
    return "protected";
  case SgAccessLabelStatement::e_access_label_public:
    return "public";
  }
  return "invalid";
}

SgClassDefinition *findClassDefinition(SgProject *project,
                                       const std::string &name) {
  for (SgNode *node : RoseAst(project)) {
    SgClassDefinition *definition = isSgClassDefinition(node);
    if (definition != nullptr && definition->get_declaration() != nullptr &&
        definition->get_declaration()->get_name().getString() == name) {
      return definition;
    }
  }
  return nullptr;
}

std::string declarationName(SgDeclarationStatement *declaration) {
  if (SgClassDeclaration *class_declaration =
          isSgClassDeclaration(declaration)) {
    return class_declaration->get_name().getString();
  }
  if (SgEnumDeclaration *enum_declaration = isSgEnumDeclaration(declaration)) {
    return enum_declaration->get_name().getString();
  }
  if (SgTemplateTypedefDeclaration *template_typedef =
          isSgTemplateTypedefDeclaration(declaration)) {
    return template_typedef->get_name().getString();
  }
  if (SgTypedefDeclaration *typedef_declaration =
          isSgTypedefDeclaration(declaration)) {
    return typedef_declaration->get_name().getString();
  }
  if (SgFunctionDeclaration *function_declaration =
          isSgFunctionDeclaration(declaration)) {
    return function_declaration->get_name().getString();
  }
  return variableName(declaration);
}

void requireNonmemberAccessIsNotApplicable(SgProject *project) {
  const std::set<std::string> expected_names = {
      "RexGlobalAccessEnum",
      "RexGlobalAccessTypedef",
      "RexGlobalAccessAlias",
      "RexGlobalAccessAliasTemplate",
      "rex_global_access_function",
      "rex_global_access_variable",
      "RexAccessBase",
      "RexAccessLabelClass",
      "RexAccessLabelStruct",
      "RexNamespaceAccessAlias",
      "rex_namespace_access_function",
      "rex_binding_first",
      "rex_binding_second",
      "rex_binding_access",
  };
  std::set<std::string> observed_names;
  for (SgNode *node : RoseAst(project)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr ||
        isSgClassDefinition(declaration->get_scope()) != nullptr) {
      continue;
    }
    const std::string name = declarationName(declaration);
    if (expected_names.count(name) == 0) {
      continue;
    }
    const SgAccessModifier &access =
        declaration->get_declarationModifier().get_accessModifier();
    if (!access.isNotApplicable()) {
      std::cerr << "REX_TEST_INVARIANT[nonmember-access]: declaration="
                << declaration << " type=" << declaration->class_name()
                << " name=" << name << " scope=" << declaration->get_scope()
                << " access=" << accessName(access) << std::endl;
      ROSE_ABORT();
    }
    ROSE_ASSERT(!access.get_is_explicit());
    observed_names.insert(name);
  }
  ROSE_ASSERT(observed_names == expected_names);
}

void requireLexicalContract(SgClassDefinition *definition,
                            const std::vector<std::string> &expected) {
  ROSE_ASSERT(definition != nullptr);
  std::vector<std::string> actual;
  for (SgDeclarationStatement *member : definition->get_members()) {
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_parent() == definition);
    ROSE_ASSERT(member->get_scope() == definition);

    if (SgAccessLabelStatement *label = isSgAccessLabelStatement(member)) {
      label->validate();
      actual.push_back("access:" + accessName(label->get_label_kind()));
      continue;
    }

    const SgAccessModifier &access =
        member->get_declarationModifier().get_accessModifier();
    if (SgUsingDeclarationStatement *using_declaration =
            isSgUsingDeclarationStatement(member)) {
      ROSE_ASSERT(!access.get_is_explicit());
      actual.push_back(
          "using:" + using_declaration->get_source_terminal_name().getString() +
          ":" + accessName(access));
      continue;
    }
    const std::string name = variableName(member);
    if (!name.empty()) {
      ROSE_ASSERT(!access.get_is_explicit());
      actual.push_back("variable:" + name + ":" + accessName(access));
    }
  }
  ROSE_ASSERT(actual == expected);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgClassDefinition *class_definition =
      findClassDefinition(project, "RexAccessLabelClass");
  SgClassDefinition *struct_definition =
      findClassDefinition(project, "RexAccessLabelStruct");
  requireLexicalContract(class_definition,
                         {"variable:implicit_private:private", "access:public",
                          "variable:explicit_public:public", "access:protected",
                          "using:rex_protected_using_target:protected",
                          "variable:explicit_protected:protected",
                          "access:private",
                          "variable:explicit_private:private"});
  requireLexicalContract(struct_definition,
                         {"variable:implicit_public:public", "access:private",
                          "variable:explicit_private:private", "access:public",
                          "variable:explicit_public:public"});
  requireNonmemberAccessIsNotApplicable(project);

  SgClassDeclaration *class_declaration = class_definition->get_declaration();
  ROSE_ASSERT(class_declaration != nullptr);
  ROSE_ASSERT(class_declaration->get_definition() == class_definition);
  SgTreeCopy class_copy_help;
  SgClassDeclaration *copied_class_declaration =
      isSgClassDeclaration(class_declaration->copy(class_copy_help));
  ROSE_ASSERT(copied_class_declaration != nullptr);
  SgClassDefinition *copied_class_definition =
      copied_class_declaration->get_definition();
  ROSE_ASSERT(copied_class_definition != nullptr);
  ROSE_ASSERT(copied_class_definition != class_definition);
  requireLexicalContract(copied_class_definition,
                         {"variable:implicit_private:private", "access:public",
                          "variable:explicit_public:public", "access:protected",
                          "using:rex_protected_using_target:protected",
                          "variable:explicit_protected:protected",
                          "access:private",
                          "variable:explicit_private:private"});

  if (std::getenv("REX_TEST_MALFORMED_CLASS_ACCESS") != nullptr) {
    for (SgDeclarationStatement *member : class_definition->get_members()) {
      if (variableName(member) == "explicit_private") {
        SgAccessModifier &access =
            member->get_declarationModifier().get_accessModifier();
        access.setPublic();
        access.set_is_explicit(false);
        break;
      }
    }
  } else {
    AstTests::runAllTests(project);
  }

  return backend(project);
}
