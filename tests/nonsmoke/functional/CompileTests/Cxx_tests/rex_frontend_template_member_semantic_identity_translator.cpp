#include "rose.h"
#include "sageInterface.h"

#include <cstddef>

namespace {

SgTemplateMemberFunctionDeclaration *
canonicalSourceTemplate(SgTemplateMemberFunctionDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgTemplateMemberFunctionDeclaration *canonical =
      isSgTemplateMemberFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  return canonical;
}

struct ReferenceIdentity {
  SgTemplateMemberFunctionDeclaration *source = nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic = nullptr;
  SgFunctionCallExp *call = nullptr;
};

ReferenceIdentity verifyReference(SgTemplateMemberFunctionRefExp *reference) {
  ROSE_ASSERT(reference != nullptr);
  SgTemplateMemberFunctionSymbol *source_symbol = reference->get_symbol();
  SgTemplateMemberFunctionDeclaration *source =
      source_symbol != nullptr ? isSgTemplateMemberFunctionDeclaration(
                                     source_symbol->get_declaration())
                               : nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic =
      isSgTemplateInstantiationMemberFunctionDecl(
          reference->get_semantic_member_function_declaration());
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(semantic != nullptr);
  ROSE_ASSERT(static_cast<SgMemberFunctionDeclaration *>(semantic) !=
              static_cast<SgMemberFunctionDeclaration *>(source));
  ROSE_ASSERT(reference->getAssociatedMemberFunctionDeclaration() == semantic);
  ROSE_ASSERT(reference->get_type() == semantic->get_type());
  ROSE_ASSERT(canonicalSourceTemplate(semantic->get_templateDeclaration()) ==
              canonicalSourceTemplate(source));

  SgExpression *access = isSgExpression(reference->get_parent());
  ROSE_ASSERT(isSgDotExp(access) != nullptr || isSgArrowExp(access) != nullptr);
  SgFunctionCallExp *call = isSgFunctionCallExp(access->get_parent());
  ROSE_ASSERT(call != nullptr);
  ROSE_ASSERT(call->get_function() == access);
  ROSE_ASSERT(call->getAssociatedFunctionDeclaration() == semantic);
  return {source, semantic, call};
}

void verifyCopiedCall(const ReferenceIdentity &original) {
  SgFunctionCallExp *copy =
      isSgFunctionCallExp(SageInterface::copyExpression(original.call));
  ROSE_ASSERT(copy != nullptr);
  auto copied_references =
      NodeQuery::querySubTree(copy, V_SgTemplateMemberFunctionRefExp);
  ROSE_ASSERT(copied_references.size() == 1);
  SgTemplateMemberFunctionRefExp *copied_reference =
      isSgTemplateMemberFunctionRefExp(copied_references.front());
  ReferenceIdentity copied = verifyReference(copied_reference);
  ROSE_ASSERT(copied.source == original.source);
  ROSE_ASSERT(copied.semantic == original.semantic);
  ROSE_ASSERT(copied.call == copy);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  auto references =
      NodeQuery::querySubTree(project, V_SgTemplateMemberFunctionRefExp);
  ROSE_ASSERT(references.size() == 2);

  std::size_t explicit_calls = 0;
  std::size_t operator_calls = 0;
  SgTemplateMemberFunctionDeclaration *source = nullptr;
  SgTemplateInstantiationMemberFunctionDecl *semantic = nullptr;
  for (SgNode *node : references) {
    ReferenceIdentity identity =
        verifyReference(isSgTemplateMemberFunctionRefExp(node));
    if (identity.call->get_uses_operator_syntax()) {
      ++operator_calls;
    } else {
      ++explicit_calls;
    }
    if (source == nullptr) {
      source = identity.source;
      semantic = identity.semantic;
    } else {
      ROSE_ASSERT(canonicalSourceTemplate(identity.source) ==
                  canonicalSourceTemplate(source));
      ROSE_ASSERT(identity.semantic == semantic);
    }
    verifyCopiedCall(identity);
  }
  ROSE_ASSERT(explicit_calls == 1);
  ROSE_ASSERT(operator_calls == 1);

  AstTests::runAllTests(project);
  return backend(project);
}
