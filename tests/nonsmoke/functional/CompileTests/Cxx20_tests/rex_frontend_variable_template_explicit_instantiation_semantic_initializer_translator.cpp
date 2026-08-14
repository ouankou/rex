#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t primary_source_range_count = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    SgTemplateVariableDeclaration *variable =
        isSgTemplateVariableDeclaration(node);
    if (variable == nullptr || variable->get_variables().size() != 1 ||
        isSgGlobal(variable->get_parent()) == nullptr) {
      continue;
    }
    SgInitializedName *name = variable->get_variables().front();
    if (name == nullptr ||
        name->get_name() != "rex_explicit_instantiation_semantic_initializer") {
      continue;
    }

    Sg_File_Info *start = variable->get_startOfConstruct();
    Sg_File_Info *end = variable->get_endOfConstruct();
    ROSE_ASSERT(start != nullptr);
    ROSE_ASSERT(end != nullptr);
    ROSE_ASSERT(start->get_filenameString().find(
                    "rex_frontend_variable_template_explicit_instantiation_"
                    "semantic_initializer.hpp") != std::string::npos);
    ROSE_ASSERT(start->get_line() == 7);
    ROSE_ASSERT(start->get_col() == 1);
    ROSE_ASSERT(end->get_line() == 11);
    ROSE_ASSERT(end->get_col() == 6);
    ++primary_source_range_count;
  }

  size_t declaration_count = 0;
  size_t definition_count = 0;
  size_t lexical_scope_count = 0;
  size_t member_instantiation_count = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationDirectiveStatement)) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    ROSE_ASSERT(directive != nullptr);
    SgTemplateVariableDeclaration *variable =
        isSgTemplateVariableDeclaration(directive->get_declaration());
    if (variable == nullptr || variable->get_variables().size() != 1) {
      continue;
    }

    ROSE_ASSERT(variable->get_parent() == directive);
    SgInitializedName *name = variable->get_variables().front();
    ROSE_ASSERT(name != nullptr);
    ROSE_ASSERT(name->get_parent() == variable);
    if (name->get_name() == "rex_explicit_instantiation_lexical_scope") {
      ROSE_ASSERT(isSgGlobal(directive->get_parent()) != nullptr);
      ROSE_ASSERT(isSgGlobal(directive->get_scope()) != nullptr);
      SgNamespaceDefinitionStatement *semantic_scope =
          isSgNamespaceDefinitionStatement(name->get_scope());
      ROSE_ASSERT(semantic_scope != nullptr);
      ROSE_ASSERT(semantic_scope->get_namespaceDeclaration() != nullptr);
      ROSE_ASSERT(semantic_scope->get_namespaceDeclaration()->get_name() ==
                  "rex_explicit_instantiation_owner");
      ROSE_ASSERT(variable->get_scope() == directive->get_scope());
      ROSE_ASSERT(directive->get_scope() != name->get_scope());
      ROSE_ASSERT(variable->get_source_name_qualification_present());
      ROSE_ASSERT(!variable->get_source_name_global_qualification());
      ROSE_ASSERT(variable->get_source_name_qualification_tokens().size() == 1);
      ROSE_ASSERT(variable->get_source_name_qualification_tokens().front() ==
                  "rex_explicit_instantiation_owner::");
      ++lexical_scope_count;
      continue;
    }
    if (name->get_name() == "rex_explicit_instantiation_member_value") {
      ROSE_ASSERT(isSgGlobal(directive->get_parent()) != nullptr);
      ROSE_ASSERT(isSgGlobal(directive->get_scope()) != nullptr);
      ROSE_ASSERT(variable->get_scope() == directive->get_scope());
      ROSE_ASSERT(directive->get_scope() != name->get_scope());
      ROSE_ASSERT(isSgTemplateInstantiationDefn(name->get_scope()) != nullptr);
      ROSE_ASSERT(variable->get_source_name_qualification_present());
      ROSE_ASSERT(!variable->get_source_name_qualification_tokens().empty());
      ++member_instantiation_count;
      continue;
    }
    if (name->get_name() != "rex_explicit_instantiation_semantic_initializer") {
      continue;
    }
    ROSE_ASSERT(directive->get_scope() == variable->get_scope());
    SgInitializer *initializer = name->get_initializer();
    if (directive->get_do_not_instantiate()) {
      ROSE_ASSERT(initializer == nullptr);
      ++declaration_count;
      continue;
    }

    ROSE_ASSERT(initializer != nullptr);
    ROSE_ASSERT(initializer->get_parent() == name);
    ROSE_ASSERT(initializer->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(initializer->get_file_info()->isFrontendSpecific());
    ROSE_ASSERT(
        !NodeQuery::querySubTree(initializer, V_SgRequiresExpr).empty());
    ROSE_ASSERT(
        !NodeQuery::querySubTree(initializer, V_SgNestedRequirement).empty());

    ++definition_count;
  }

  ROSE_ASSERT(declaration_count == 1);
  ROSE_ASSERT(definition_count == 1);
  ROSE_ASSERT(lexical_scope_count == 1);
  ROSE_ASSERT(member_instantiation_count == 1);
  ROSE_ASSERT(primary_source_range_count == 1);
  project->skipfinalCompileStep(true);
  return backend(project);
}
