#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<decltype(std::declval<SgFunctionDeclaration &>()
                                          .get_sourceSpelledTemplateHeaders()),
                             SgTemplateParameterListPtrList &>);
static_assert(std::is_same_v<decltype(std::declval<SgVariableDeclaration &>()
                                          .get_sourceSpelledTemplateHeaders()),
                             SgTemplateParameterListPtrList &>);
static_assert(std::is_same_v<decltype(std::declval<SgClassDeclaration &>()
                                          .get_sourceSpelledTemplateHeaders()),
                             SgTemplateParameterListPtrList &>);

namespace {

bool hasOneExactEmptyOuterHeader(
    SgTemplateInstantiationMemberFunctionDecl *declaration) {
  if (declaration->get_sourceSpelledTemplateHeaders().size() != 1) {
    return false;
  }
  SgTemplateParameterList *header =
      declaration->get_sourceSpelledTemplateHeaders().front();
  return header != nullptr && header->get_parent() == declaration &&
         header->get_args().empty();
}

SgTemplateInstantiationMemberFunctionDecl *
findSpecialization(SgProject *project, const std::string &name,
                   bool isFunctionTemplate, bool requiresEnclosingHeader) {
  for (SgNode *node : RoseAst(project)) {
    SgTemplateInstantiationMemberFunctionDecl *declaration =
        isSgTemplateInstantiationMemberFunctionDecl(node);
    if (declaration == nullptr ||
        declaration->get_templateName().getString() != name ||
        declaration->get_specialization() !=
            SgDeclarationStatement::e_specialization ||
        (declaration->get_templateDeclaration() != nullptr) !=
            isFunctionTemplate) {
      continue;
    }
    if (requiresEnclosingHeader != hasOneExactEmptyOuterHeader(declaration)) {
      continue;
    }
    if (!requiresEnclosingHeader &&
        !declaration->get_sourceSpelledTemplateHeaders().empty()) {
      continue;
    }
    return declaration;
  }
  return nullptr;
}

SgInitializedName *findBaseInitializer(SgProject *project) {
  for (SgNode *node : RoseAst(project)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName == nullptr ||
        initializedName->get_preinitialization() !=
            SgInitializedName::e_nonvirtual_base_class) {
      continue;
    }
    SgCtorInitializerList *list =
        isSgCtorInitializerList(initializedName->get_parent());
    SgMemberFunctionDeclaration *constructor =
        list != nullptr ? isSgMemberFunctionDeclaration(list->get_parent())
                        : nullptr;
    SgClassDefinition *classScope =
        constructor != nullptr ? isSgClassDefinition(constructor->get_scope())
                               : nullptr;
    if (constructor != nullptr && classScope != nullptr &&
        constructor->get_specialFunctionModifier().isConstructor() &&
        constructor->get_CtorInitializerList() == list &&
        initializedName->get_scope() == classScope) {
      return initializedName;
    }
  }
  return nullptr;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateInstantiationMemberFunctionDecl *ordinary =
      findSpecialization(project, "ordinary", false, true);
  SgTemplateInstantiationMemberFunctionDecl *templated =
      findSpecialization(project, "templated", true, true);
  SgInitializedName *baseInitializer = findBaseInitializer(project);
  ROSE_ASSERT(ordinary != nullptr);
  ROSE_ASSERT(templated != nullptr);
  ROSE_ASSERT(baseInitializer != nullptr);

  if (std::getenv("REX_TEST_MALFORMED_TEMPLATE_HEADER") != nullptr) {
    ordinary->get_sourceSpelledTemplateHeaders().clear();
  } else if (std::getenv("REX_TEST_MALFORMED_ENCLOSING_TEMPLATE_HEADER") !=
             nullptr) {
    templated->get_sourceSpelledTemplateHeaders().clear();
  } else if (std::getenv("REX_TEST_MALFORMED_CTOR_OWNER") != nullptr) {
    baseInitializer->set_scope(nullptr);
  } else {
    AstTests::runAllTests(project);
  }

  return backend(project);
}
