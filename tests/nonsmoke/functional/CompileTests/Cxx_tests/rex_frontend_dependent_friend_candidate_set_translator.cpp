#include "nodeQuery.h"
#include "rose.h"

#include <unordered_set>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  size_t unresolved_friend_count = 0;
  const Rose_STL_Container<SgNode *> declarations =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);
  for (SgNode *node : declarations) {
    SgTemplateInstantiationFunctionDecl *declaration =
        isSgTemplateInstantiationFunctionDecl(node);
    ROSE_ASSERT(declaration != nullptr);

    const SgDeclarationStatementPtrList &candidates =
        declaration->get_dependentTemplateCandidates();
    if (candidates.empty()) {
      ROSE_ASSERT(declaration->get_templateDeclaration() != nullptr);
      ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() ==
                  declaration->get_templateDeclaration());
      continue;
    }

    ++unresolved_friend_count;
    ROSE_ASSERT(declaration->get_templateName() == "select");
    ROSE_ASSERT(declaration->get_declarationModifier().isFriend());
    ROSE_ASSERT(declaration->get_templateDeclaration() == nullptr);
    ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() == nullptr);
    ROSE_ASSERT(candidates.size() == 2);

    std::unordered_set<SgTemplateFunctionDeclaration *> exact_candidates;
    std::unordered_set<SgFunctionType *> exact_types;
    for (SgDeclarationStatement *candidate : candidates) {
      SgTemplateFunctionDeclaration *template_declaration =
          isSgTemplateFunctionDeclaration(candidate);
      ROSE_ASSERT(template_declaration != nullptr);
      ROSE_ASSERT(template_declaration->get_name() ==
                  declaration->get_templateName());
      ROSE_ASSERT(template_declaration->get_scope() ==
                  declaration->get_scope());
      ROSE_ASSERT(template_declaration->get_firstNondefiningDeclaration() ==
                  template_declaration);
      ROSE_ASSERT(template_declaration->get_symbol_from_symbol_table() !=
                  nullptr);
      ROSE_ASSERT(exact_candidates.insert(template_declaration).second);
      ROSE_ASSERT(exact_types.insert(template_declaration->get_type()).second);
    }
  }

  ROSE_ASSERT(unresolved_friend_count == 1);
  return backend(project);
}
