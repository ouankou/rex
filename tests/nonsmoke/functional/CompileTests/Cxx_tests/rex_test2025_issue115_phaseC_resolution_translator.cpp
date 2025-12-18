#include "nodeQuery.h"
#include "sage3basic.h"
#include "sageInterface.h"

#include <string>

namespace {
std::string getInputFilename(int argc, char **argv) {
  for (int i = 0; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "-c") {
      return argv[i + 1];
    }
  }
  return "";
}

std::string getBasename(const std::string &path) {
  std::string::size_type pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

SgTemplateFunctionDeclaration *findTemplatePattern(SgProject *project,
                                                   const std::string &name) {
  auto decls =
      NodeQuery::querySubTree(project, V_SgTemplateFunctionDeclaration);
  for (SgNode *n : decls) {
    auto *decl = isSgTemplateFunctionDeclaration(n);
    if (decl != NULL && decl->get_name().getString() == name) {
      return decl;
    }
  }
  return NULL;
}

SgTemplateInstantiationFunctionDecl *
findDefiningInstantiation(SgProject *project,
                          const std::string &template_name) {
  auto decls =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);
  for (SgNode *n : decls) {
    auto *decl = isSgTemplateInstantiationFunctionDecl(n);
    if (decl == NULL) {
      continue;
    }
    if (decl->get_templateName().getString() != template_name) {
      continue;
    }
    if (decl->get_definition() == NULL) {
      continue;
    }
    return decl;
  }
  return NULL;
}

bool subtreeHasNonrealCallee(SgNode *root, const std::string &callee_name) {
  auto refs = NodeQuery::querySubTree(root, V_SgNonrealRefExp);
  for (SgNode *n : refs) {
    auto *nr = isSgNonrealRefExp(n);
    if (nr == NULL) {
      continue;
    }
    SgNonrealSymbol *sym = nr->get_symbol();
    if (sym != NULL && sym->get_name().getString() == callee_name) {
      return true;
    }
  }
  return false;
}

bool subtreeHasFunctionRef(SgNode *root, const std::string &callee_name) {
  auto refs = NodeQuery::querySubTree(root, V_SgFunctionRefExp);
  for (SgNode *n : refs) {
    auto *ref = isSgFunctionRefExp(n);
    if (ref == NULL) {
      continue;
    }
    SgFunctionDeclaration *decl = ref->getAssociatedFunctionDeclaration();
    if (decl != NULL && decl->get_name().getString() == callee_name) {
      return true;
    }
  }
  return false;
}

bool subtreeHasMemberFunctionRef(SgNode *root, const std::string &callee_name) {
  auto refs = NodeQuery::querySubTree(root, V_SgMemberFunctionRefExp);
  for (SgNode *n : refs) {
    auto *ref = isSgMemberFunctionRefExp(n);
    if (ref == NULL) {
      continue;
    }
    SgMemberFunctionDeclaration *decl =
        ref->getAssociatedMemberFunctionDeclaration();
    if (decl != NULL && decl->get_name().getString() == callee_name) {
      return true;
    }
  }
  return false;
}
} // namespace

int main(int argc, char **argv) {
  std::string input = getBasename(getInputFilename(argc, argv));

  std::string template_name;
  std::string expected_callee;
  bool expect_member = false;

  if (input == "rex_test2025_issue115_unresolved_lookup.cpp") {
    template_name = "f";
    expected_callee = "foo";
    expect_member = false;
  } else if (input == "rex_test2025_issue115_dependent_member.cpp") {
    template_name = "g";
    expected_callee = "begin";
    expect_member = true;
  } else if (input == "rex_test2025_issue115_qualified_unresolved_lookup.cpp") {
    template_name = "k";
    expected_callee = "h";
    expect_member = false;
  } else {
    std::cerr << "Unexpected input filename for Issue 115 Phase C test: "
              << input << std::endl;
    return 1;
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SgTemplateFunctionDeclaration *pattern =
      findTemplatePattern(project, template_name);
  ROSE_ASSERT(pattern != NULL);
  ROSE_ASSERT(pattern->get_definition() != NULL);
  ROSE_ASSERT(pattern->get_definition()->get_body() != NULL);

  ROSE_ASSERT(
      subtreeHasNonrealCallee(pattern->get_definition(), expected_callee));

  SgTemplateInstantiationFunctionDecl *inst =
      findDefiningInstantiation(project, template_name);
  ROSE_ASSERT(inst != NULL);
  ROSE_ASSERT(inst->get_definition() != NULL);
  ROSE_ASSERT(inst->get_definition()->get_body() != NULL);

  auto inst_nonreal =
      NodeQuery::querySubTree(inst->get_definition(), V_SgNonrealRefExp);
  ROSE_ASSERT(inst_nonreal.empty());

  if (expect_member) {
    ROSE_ASSERT(
        subtreeHasMemberFunctionRef(inst->get_definition(), expected_callee));
  } else {
    ROSE_ASSERT(subtreeHasFunctionRef(inst->get_definition(), expected_callee));
  }

  return backend(project);
}
