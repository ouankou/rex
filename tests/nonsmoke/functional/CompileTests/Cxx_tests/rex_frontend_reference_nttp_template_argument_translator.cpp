#include "RoseAst.h"
#include "rose.h"

namespace {
SgInitializedName *referencedVariable(SgExpression *expression) {
  SgVarRefExp *reference = isSgVarRefExp(expression);
  SgVariableSymbol *symbol =
      reference != nullptr ? reference->get_symbol() : nullptr;
  return symbol != nullptr ? symbol->get_declaration() : nullptr;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  int matching_references = 0;
  for (SgNode *node : RoseAst(project)) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    SgFunctionDeclaration *resolved =
        reference != nullptr ? reference->get_resolved_function_declaration()
                             : nullptr;
    SgTemplateInstantiationFunctionDecl *instantiation =
        isSgTemplateInstantiationFunctionDecl(resolved);
    if (instantiation == nullptr ||
        instantiation->get_templateName() != "rex_reference_nttp_value") {
      continue;
    }

    SgNonrealSymbol *symbol = reference->get_symbol();
    SgNonrealDecl *identity =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    ROSE_ASSERT(identity != nullptr);
    ROSE_ASSERT(identity->get_nonreal_template_role() ==
                SgNonrealDecl::e_nonreal_template_id);

    SgTemplateArgumentPtrList &written = reference->get_templateArguments();
    SgTemplateArgumentPtrList &semantic = identity->get_tpl_args();
    ROSE_ASSERT(written.size() == 1);
    ROSE_ASSERT(semantic.size() == 1);
    ROSE_ASSERT(written.front() != nullptr);
    ROSE_ASSERT(semantic.front() != nullptr);
    ROSE_ASSERT(written.front() != semantic.front());
    ROSE_ASSERT(written.front()->get_parent() == reference);
    ROSE_ASSERT(semantic.front()->get_parent() == identity);
    ROSE_ASSERT(written.front()->get_explicitlySpecified());
    ROSE_ASSERT(semantic.front()->get_explicitlySpecified());
    ROSE_ASSERT(written.front()->get_argumentType() ==
                SgTemplateArgument::nontype_argument);
    ROSE_ASSERT(semantic.front()->get_argumentType() ==
                SgTemplateArgument::nontype_argument);

    SgExpression *written_expression = written.front()->get_expression();
    SgExpression *semantic_expression = semantic.front()->get_expression();
    ROSE_ASSERT(written_expression != nullptr);
    ROSE_ASSERT(semantic_expression != nullptr);
    ROSE_ASSERT(written_expression != semantic_expression);
    ROSE_ASSERT(written_expression->get_parent() == written.front());
    ROSE_ASSERT(semantic_expression->get_parent() == semantic.front());
    ROSE_ASSERT(isSgReferenceType(written.front()->get_type()) == nullptr);
    SgReferenceType *semantic_reference =
        isSgReferenceType(semantic.front()->get_type());
    ROSE_ASSERT(semantic_reference != nullptr);
    ROSE_ASSERT(isSgTypeInt(semantic_reference->get_base_type()
                                ->stripTypedefsAndModifiers()) != nullptr);

    SgInitializedName *written_variable =
        referencedVariable(written_expression);
    SgInitializedName *semantic_variable =
        referencedVariable(semantic_expression);
    ROSE_ASSERT(written_variable != nullptr);
    ROSE_ASSERT(written_variable == semantic_variable);
    ROSE_ASSERT(written_variable->get_name() == "rex_reference_nttp_object");
    ++matching_references;
  }

  ROSE_ASSERT(matching_references == 1);
  return backend(project);
}
