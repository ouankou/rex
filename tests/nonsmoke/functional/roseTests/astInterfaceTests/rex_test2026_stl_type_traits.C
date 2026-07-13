#include "rose.h"

#include <algorithm>
#include <set>
#include <string>

static bool hasTemplateInstantiation(SgProject *project,
                                     const std::string &name) {
  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl);
  for (SgNode *node : nodes) {
    SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node);
    if (decl != nullptr && decl->get_templateName().getString() == name) {
      return true;
    }
  }
  return false;
}

static bool hasTemplateInstantiationTypedef(SgProject *project,
                                            const std::string &name) {
  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(
      project, V_SgTemplateInstantiationTypedefDeclaration);
  for (SgNode *node : nodes) {
    SgTemplateInstantiationTypedefDeclaration *decl =
        isSgTemplateInstantiationTypedefDeclaration(node);
    if (decl != nullptr && decl->get_templateName().getString() == name) {
      SgTemplateTypedefDeclaration *source_template =
          decl->get_templateDeclaration();
      ROSE_ASSERT(source_template != nullptr);
      ROSE_ASSERT(decl->get_specializedTemplateDeclaration() ==
                  source_template);
      ROSE_ASSERT(source_template->get_firstNondefiningDeclaration() !=
                  nullptr);

      SgScopeStatement *scope = decl->get_scope();
      ROSE_ASSERT(scope != nullptr);
      SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(decl->get_parent());
      ROSE_ASSERT(auxiliary != nullptr);
      ROSE_ASSERT(auxiliary->get_parent() == scope);
      ROSE_ASSERT(scope->get_auxiliary_declarations() == auxiliary);
      ROSE_ASSERT(!scope->statementExistsInScope(decl));
      ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                             auxiliary->get_declarations().end(), decl) == 1);
      return true;
    }
  }
  return false;
}

static bool hasSourceSpelledAliasTemplateUse(SgProject *project,
                                             const std::string &name) {
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgNonrealDecl)) {
    SgNonrealDecl *declaration = isSgNonrealDecl(node);
    if (declaration == nullptr || declaration->get_name() != name ||
        declaration->get_nonreal_template_role() !=
            SgNonrealDecl::e_nonreal_template_id) {
      continue;
    }

    SgTemplateTypedefDeclaration *source_template =
        isSgTemplateTypedefDeclaration(declaration->get_templateDeclaration());
    if (source_template == nullptr || source_template->get_name() != name) {
      continue;
    }

    SgNonrealType *source_type = isSgNonrealType(declaration->get_type());
    ROSE_ASSERT(source_type != nullptr);
    ROSE_ASSERT(source_type->get_declaration() == declaration);
    ROSE_ASSERT(!declaration->get_tpl_args().empty());

    SgDeclarationScope *scope = isSgDeclarationScope(declaration->get_parent());
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(declaration->get_scope() == scope);
    ROSE_ASSERT(std::count(scope->get_declarations().begin(),
                           scope->get_declarations().end(), declaration) == 1);
    ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() == declaration);
    ROSE_ASSERT(declaration->get_definingDeclaration() == nullptr);
    return true;
  }
  return false;
}

static void requireDistinctScopedAliasTemplates(SgProject *project) {
  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(
      project, V_SgTemplateInstantiationTypedefDeclaration);
  std::set<SgTemplateTypedefDeclaration *> source_templates;
  std::set<SgScopeStatement *> source_scopes;
  size_t scoped_instantiations = 0;
  for (SgNode *node : nodes) {
    SgTemplateInstantiationTypedefDeclaration *declaration =
        isSgTemplateInstantiationTypedefDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_templateName() != "scoped_alias") {
      continue;
    }
    ++scoped_instantiations;
    SgTemplateTypedefDeclaration *source_template =
        declaration->get_templateDeclaration();
    ROSE_ASSERT(source_template != nullptr);
    ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() ==
                source_template);
    ROSE_ASSERT(source_template->get_firstNondefiningDeclaration() != nullptr);
    ROSE_ASSERT(source_template->get_scope() != nullptr);
    source_templates.insert(source_template);
    source_scopes.insert(source_template->get_scope());
  }
  ROSE_ASSERT(scoped_instantiations == 2);
  ROSE_ASSERT(source_templates.size() == 2);
  ROSE_ASSERT(source_scopes.size() == 2);
}

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  ROSE_ASSERT(hasTemplateInstantiation(project, "integral_constant"));
  ROSE_ASSERT(hasTemplateInstantiation(project, "remove_all_pointers"));

  ROSE_ASSERT(hasTemplateInstantiationTypedef(project, "add_pointer_t"));
  ROSE_ASSERT(hasSourceSpelledAliasTemplateUse(project, "remove_reference_t"));
  requireDistinctScopedAliasTemplates(project);

  Rose_STL_Container<SgNode *> traits =
      NodeQuery::querySubTree(project, V_SgTypeTraitBuiltinOperator);
  ROSE_ASSERT(!traits.empty());
  size_t type_operand_count = 0;
  for (SgNode *node : traits) {
    SgTypeTraitBuiltinOperator *trait = isSgTypeTraitBuiltinOperator(node);
    ROSE_ASSERT(trait != nullptr);
    SgType *result_type = trait->get_expression_type();
    ROSE_ASSERT(result_type != nullptr);
    ROSE_ASSERT(isSgTypeUnknown(result_type) == nullptr);
    ROSE_ASSERT(isSgTypeDefault(result_type) == nullptr);
    ROSE_ASSERT(trait->get_type() == result_type);
    ROSE_ASSERT(!trait->get_args().empty());
    for (SgNode *argument : trait->get_args()) {
      SgExpression *argument_expression = isSgExpression(argument);
      ROSE_ASSERT(argument_expression != nullptr);
      ROSE_ASSERT(argument_expression->get_parent() == trait);
      ROSE_ASSERT(isSgType(argument) == nullptr);
      if (SgTypeExpression *type_operand =
              isSgTypeExpression(argument_expression)) {
        SgType *represented_type = type_operand->get_represented_type();
        ROSE_ASSERT(represented_type != nullptr);
        ROSE_ASSERT(isSgTypeUnknown(represented_type) == nullptr);
        ROSE_ASSERT(isSgTypeDefault(represented_type) == nullptr);
        ++type_operand_count;
      }
    }
  }
  ROSE_ASSERT(type_operand_count != 0);

  return backend(project);
}
