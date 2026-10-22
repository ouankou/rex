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
  size_t storage_uses = 0;
  size_t constant_reads = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgNonrealRefExp)) {
    auto *reference = isSgNonrealRefExp(node);
    SgTemplateVariableDeclaration *variable =
        reference->get_resolved_variable_declaration();
    if (variable == nullptr) {
      continue;
    }
    ROSE_ASSERT(variable->get_variables().size() == 1);
    ROSE_ASSERT(variable->get_variables().front() != nullptr);
    if (variable->get_variables().front()->get_name() !=
        "rex_reference_storage_use") {
      continue;
    }
    if (reference->get_is_non_odr_use()) {
      ++constant_reads;
    } else {
      ++storage_uses;
    }
    auto *copy = SageInterface::deepCopy(reference);
    ROSE_ASSERT(copy->get_is_non_odr_use() == reference->get_is_non_odr_use());
    SageInterface::deleteAST(copy);
  }
  ROSE_ASSERT(storage_uses == 2 && constant_reads == 2);
  size_t semantic_partials = 0;
  SgTemplateVariableDeclaration *semantic_partial = nullptr;
  for (auto *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    auto *variable = isSgTemplateVariableDeclaration(node);
    auto *name = variable->get_variables().front();
    if (name->get_name() != "rex_member_partial" ||
        variable->get_specialization() !=
            SgDeclarationStatement::e_partial_specialization ||
        !isSgTemplateInstantiationDefn(name->get_scope()))
      continue;
    auto *auxiliary = isSgAuxiliaryDeclarationList(variable->get_parent());
    ROSE_ASSERT(auxiliary != nullptr &&
                auxiliary->get_parent() == name->get_scope());
    ROSE_ASSERT(variable->get_is_constexpr());
    ROSE_ASSERT(
        variable->get_declarationModifier().get_storageModifier().isStatic());
    ROSE_ASSERT(name->get_initializer() == nullptr);
    ROSE_ASSERT(name->get_scope()->find_symbol_from_declaration(name) !=
                nullptr);
    semantic_partial = variable;
    ++semantic_partials;
  }
  ROSE_ASSERT(semantic_partials == 1);
  size_t partial_instantiations = 0;
  for (auto *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    auto *variable = isSgTemplateVariableDeclaration(node);
    if (variable->get_specializedTemplateDeclaration() != semantic_partial)
      continue;
    ROSE_ASSERT(variable->get_variables().size() == 1);
    auto *name = variable->get_variables().front();
    ROSE_ASSERT(name->get_scope() ==
                semantic_partial->get_variables().front()->get_scope());
    ROSE_ASSERT(variable->get_is_constexpr());
    auto *initializer = name->get_initializer();
    ROSE_ASSERT(initializer != nullptr && initializer->get_parent() == name);
    ROSE_ASSERT(initializer->get_file_info()->isFrontendSpecific());
    auto sizes = NodeQuery::querySubTree(initializer, V_SgSizeOfOp);
    ROSE_ASSERT(sizes.size() == 1);
    ROSE_ASSERT(isSgTypeInt(isSgSizeOfOp(sizes.front())
                                ->get_operand_type()
                                ->stripTypedefsAndModifiers()));
    ROSE_ASSERT(name->get_scope()->find_symbol_from_declaration(name) !=
                nullptr);
    ++partial_instantiations;
  }
  ROSE_ASSERT(partial_instantiations == 1);
  size_t standard_thread_storage = 0;
  size_t gnu_thread_storage = 0;
  for (auto *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    auto *variable = isSgTemplateVariableDeclaration(node);
    const auto &name = variable->get_variables().front()->get_name();
    const bool gnu_storage = variable->get_declarationModifier()
                                 .get_storageModifier()
                                 .get_thread_local_storage();
    if (name == "rex_template_thread_storage") {
      ROSE_ASSERT(variable->get_is_thread_local() && !gnu_storage);
      ++standard_thread_storage;
    } else if (name == "rex_template_gnu_thread_storage") {
      ROSE_ASSERT(gnu_storage && !variable->get_is_thread_local());
      ++gnu_thread_storage;
    }
  }
  ROSE_ASSERT(standard_thread_storage == 5 && gnu_thread_storage == 5);
  project->skipfinalCompileStep(true);
  return backend(project);
}
