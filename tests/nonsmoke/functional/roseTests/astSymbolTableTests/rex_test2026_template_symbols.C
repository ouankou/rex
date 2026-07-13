#include "AstInterface.h"
#include "rose.h"

#include <algorithm>
#include <set>
#include <string>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> tmpl_classes =
      NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration);
  bool saw_partial = false;
  for (SgNode *node : tmpl_classes) {
    SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node);
    if (decl != nullptr &&
        !decl->get_templateSpecializationArguments().empty()) {
      saw_partial = true;
      break;
    }
  }
  ROSE_ASSERT(saw_partial);

  Rose_STL_Container<SgNode *> instantiations =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl);
  ROSE_ASSERT(!instantiations.empty());
  for (SgNode *node : instantiations) {
    SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node);
    ROSE_ASSERT(decl != nullptr);
    ROSE_ASSERT(decl->get_symbol_from_symbol_table() != nullptr);
    SgScopeStatement *scope = decl->get_scope();
    ROSE_ASSERT(scope != nullptr);
    Sg_File_Info *info = decl->get_file_info();
    const bool sourceWritten = info != nullptr && info->get_line() > 0 &&
                               !info->isCompilerGenerated() &&
                               !info->isSourcePositionUnavailableInFrontend();
    if (sourceWritten) {
      ROSE_ASSERT(decl->get_parent() == scope);
      ROSE_ASSERT(scope->statementExistsInScope(decl));
    } else {
      SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(decl->get_parent());
      ROSE_ASSERT(auxiliary != nullptr);
      ROSE_ASSERT(auxiliary->get_parent() == scope);
      ROSE_ASSERT(scope->get_auxiliary_declarations() == auxiliary);
      ROSE_ASSERT(!scope->statementExistsInScope(decl));
      ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                             auxiliary->get_declarations().end(), decl) == 1);
      // The container is a structural semantic owner, never an output
      // surface. The declaration retains its independent output role below.
      ROSE_ASSERT(!auxiliary->isOutputInCodeGeneration());
    }
    ROSE_ASSERT(decl->isOutputInCodeGeneration());
  }

  Rose_STL_Container<SgNode *> tmpl_vars =
      NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration);
  ROSE_ASSERT(!tmpl_vars.empty());
  bool saw_value_of_specialization = false;
  bool saw_templ_value_source_contract = false;
  bool saw_templ_default_source_contract = false;
  bool saw_default_value_source_contract = false;
  for (SgNode *node : tmpl_vars) {
    SgTemplateVariableDeclaration *decl = isSgTemplateVariableDeclaration(node);
    if (decl == nullptr) {
      continue;
    }
    SgInitializedName *init_name = SageInterface::getFirstInitializedName(decl);
    ROSE_ASSERT(init_name != nullptr);
    ROSE_ASSERT(init_name->search_for_symbol_from_symbol_table() != nullptr);
    Sg_File_Info *decl_info = decl->get_file_info();
    const bool source_written =
        decl_info != nullptr && decl_info->get_line() > 0 &&
        !decl_info->isCompilerGenerated() &&
        !decl_info->isSourcePositionUnavailableInFrontend();
    const std::string variable_name = init_name->get_name().getString();
    if (source_written &&
        (variable_name == "templ_value" || variable_name == "templ_default" ||
         variable_name == "default_value")) {
      ROSE_ASSERT(decl->get_is_constexpr());
      ROSE_ASSERT(!SageInterface::isConstType(init_name->get_type()));
      SgAssignInitializer *assignment =
          isSgAssignInitializer(init_name->get_initializer());
      ROSE_ASSERT(assignment != nullptr);
      SgConstructorInitializer *constructor =
          isSgConstructorInitializer(assignment->get_operand_i());
      ROSE_ASSERT(constructor != nullptr);
      ROSE_ASSERT(constructor->get_parent() == assignment);
      ROSE_ASSERT(constructor->get_is_braced_initialized());

      const bool is_static =
          decl->get_declarationModifier().get_storageModifier().isStatic();
      if (variable_name == "templ_value") {
        ROSE_ASSERT(!saw_templ_value_source_contract);
        ROSE_ASSERT(is_static);
        saw_templ_value_source_contract = true;
      } else if (variable_name == "templ_default") {
        ROSE_ASSERT(!saw_templ_default_source_contract);
        ROSE_ASSERT(is_static);
        saw_templ_default_source_contract = true;
      } else {
        ROSE_ASSERT(!saw_default_value_source_contract);
        ROSE_ASSERT(!is_static);
        saw_default_value_source_contract = true;
      }
    }
    if (init_name->get_name() == "value_of" &&
        decl->get_specialization() ==
            SgDeclarationStatement::e_specialization) {
      ROSE_ASSERT(!saw_value_of_specialization);
      saw_value_of_specialization = true;
      const SgTemplateArgumentPtrList &arguments =
          decl->get_templateSpecializationArguments();
      ROSE_ASSERT(arguments.size() == 1);
      ROSE_ASSERT(arguments.front() != nullptr);
      ROSE_ASSERT(arguments.front()->get_explicitlySpecified());
      ROSE_ASSERT(arguments.front()->get_parent() == decl);
      ROSE_ASSERT(decl->get_specializedTemplateDeclaration() != nullptr);
    }
  }
  ROSE_ASSERT(saw_value_of_specialization);
  ROSE_ASSERT(saw_templ_value_source_contract);
  ROSE_ASSERT(saw_templ_default_source_contract);
  ROSE_ASSERT(saw_default_value_source_contract);

  Rose_STL_Container<SgNode *> nonreal_refs =
      NodeQuery::querySubTree(project, V_SgNonrealRefExp);
  size_t explicit_value_of_reference_count = 0;
  for (SgNode *node : nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_name() != "value_of" ||
        !reference->get_explicit_template_argument_list()) {
      continue;
    }

    ++explicit_value_of_reference_count;
    ROSE_ASSERT(reference->get_resolved_function_declaration() == nullptr);
    SgTemplateVariableDeclaration *resolved =
        reference->get_resolved_variable_declaration();
    ROSE_ASSERT(resolved != nullptr);
    ROSE_ASSERT(resolved->get_specialization() ==
                SgDeclarationStatement::e_specialization);
    ROSE_ASSERT(resolved->get_variables().size() == 1);
    SgInitializedName *resolved_name = resolved->get_variables().front();
    ROSE_ASSERT(resolved_name != nullptr);
    ROSE_ASSERT(resolved_name->get_name() == "value_of");
    ROSE_ASSERT(resolved_name->get_parent() == resolved);
    ROSE_ASSERT(resolved_name->get_type() != nullptr);
    ROSE_ASSERT(reference->get_type() == resolved_name->get_type());
    ROSE_ASSERT(SageInterface::convertRefToInitializedName(reference) ==
                resolved_name);
    ROSE_ASSERT(isSgTypeInt(reference->get_type()->stripType(
                    SgType::STRIP_MODIFIER_TYPE)) != nullptr);
    ROSE_ASSERT(reference->get_templateArguments().size() == 1);
    SgTemplateArgument *argument = reference->get_templateArguments().front();
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == reference);
    ROSE_ASSERT(argument->get_explicitlySpecified());
  }
  ROSE_ASSERT(explicit_value_of_reference_count == 1);

  size_t empty_default_value_reference_count = 0;
  for (SgNode *node : nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_name() != "default_value" ||
        !reference->get_explicit_template_argument_list()) {
      continue;
    }
    ++empty_default_value_reference_count;
    ROSE_ASSERT(reference->get_templateArguments().empty());
    SgInitializedName *resolved_name =
        SageInterface::requireResolvedVariableTemplateReference(
            reference, "template-symbol empty default argument regression");
    ROSE_ASSERT(resolved_name->get_name() == "default_value");
    ROSE_ASSERT(isSgTypeLong(resolved_name->get_type()->stripType(
                    SgType::STRIP_MODIFIER_TYPE)) != nullptr);
  }
  ROSE_ASSERT(empty_default_value_reference_count == 1);

  size_t explicit_member_value_reference_count = 0;
  bool saw_dot_template_variable = false;
  bool saw_arrow_template_variable = false;
  for (SgNode *node : nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_name() != "templ_value" ||
        !reference->get_explicit_template_argument_list()) {
      continue;
    }

    ++explicit_member_value_reference_count;
    ROSE_ASSERT(reference->get_resolved_function_declaration() == nullptr);
    SgTemplateVariableDeclaration *resolved =
        reference->get_resolved_variable_declaration();
    ROSE_ASSERT(resolved != nullptr);
    ROSE_ASSERT(resolved->get_variables().size() == 1);
    SgInitializedName *resolved_name = resolved->get_variables().front();
    ROSE_ASSERT(resolved_name != nullptr);
    ROSE_ASSERT(resolved_name->get_name() == "templ_value");
    ROSE_ASSERT(resolved_name->get_parent() == resolved);
    ROSE_ASSERT(reference->get_type() == resolved_name->get_type());
    ROSE_ASSERT(SageInterface::convertRefToInitializedName(reference) ==
                resolved_name);
    ROSE_ASSERT(reference->get_templateArguments().size() == 1);
    SgTemplateArgument *argument = reference->get_templateArguments().front();
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == reference);
    ROSE_ASSERT(argument->get_explicitlySpecified());

    SgBinaryOp *member_access = isSgBinaryOp(reference->get_parent());
    ROSE_ASSERT(member_access != nullptr);
    ROSE_ASSERT(isSgDotExp(member_access) != nullptr ||
                isSgArrowExp(member_access) != nullptr);
    if (isSgDotExp(member_access) != nullptr) {
      ROSE_ASSERT(isSgVarRefExp(member_access->get_lhs_operand()) != nullptr);
      ROSE_ASSERT(isSgTypeShort(reference->get_type()->stripType(
                      SgType::STRIP_MODIFIER_TYPE)) != nullptr);
    } else {
      SgCastExp *base_cast = isSgCastExp(member_access->get_lhs_operand());
      ROSE_ASSERT(base_cast != nullptr);
      ROSE_ASSERT(base_cast->cast_type() == SgCastExp::e_implicit_cast);
      ROSE_ASSERT(base_cast->get_file_info() != nullptr);
      ROSE_ASSERT(base_cast->get_file_info()->isImplicitCast());
      ROSE_ASSERT(base_cast->get_operand() != nullptr);
      ROSE_ASSERT(base_cast->get_operand()->get_parent() == base_cast);
      ROSE_ASSERT(base_cast->get_type() !=
                  base_cast->get_operand()->get_type());
      SgPointerType *converted_pointer = isSgPointerType(base_cast->get_type());
      SgPointerType *dependent_pointer =
          isSgPointerType(base_cast->get_operand()->get_type());
      ROSE_ASSERT(converted_pointer != nullptr);
      ROSE_ASSERT(dependent_pointer != nullptr);
      ROSE_ASSERT(isSgClassType(converted_pointer->get_base_type()) != nullptr);
      ROSE_ASSERT(isSgNonrealType(dependent_pointer->get_base_type()) !=
                  nullptr);
      ROSE_ASSERT(isSgTypeLong(reference->get_type()->stripType(
                      SgType::STRIP_MODIFIER_TYPE)) != nullptr);
    }
    std::string member_name;
    AstNodeType member_type;
    AstNodePtr member_scope;
    ROSE_ASSERT(AstInterface::IsVarRef(AstNodePtr(member_access), &member_type,
                                       &member_name, &member_scope));
    ROSE_ASSERT(member_name.find("templ_value") != std::string::npos);
    ROSE_ASSERT(member_type.get_ptr() == resolved_name->get_type());
    ROSE_ASSERT(member_scope != AST_NULL);
    saw_dot_template_variable =
        saw_dot_template_variable || isSgDotExp(member_access) != nullptr;
    saw_arrow_template_variable =
        saw_arrow_template_variable || isSgArrowExp(member_access) != nullptr;
  }
  ROSE_ASSERT(explicit_member_value_reference_count == 2);
  ROSE_ASSERT(saw_dot_template_variable);
  ROSE_ASSERT(saw_arrow_template_variable);

  size_t empty_member_default_reference_count = 0;
  for (SgNode *node : nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_name() != "templ_default" ||
        !reference->get_explicit_template_argument_list()) {
      continue;
    }
    ++empty_member_default_reference_count;
    ROSE_ASSERT(reference->get_templateArguments().empty());
    SgInitializedName *resolved_name =
        SageInterface::requireResolvedVariableTemplateReference(
            reference, "template-symbol empty member default regression");
    ROSE_ASSERT(resolved_name->get_name() == "templ_default");
    SgBinaryOp *member_access = isSgBinaryOp(reference->get_parent());
    ROSE_ASSERT(isSgDotExp(member_access) != nullptr);
    std::string member_name;
    ROSE_ASSERT(AstInterface::IsVarRef(AstNodePtr(member_access), nullptr,
                                       &member_name));
    ROSE_ASSERT(member_name.find("templ_default") != std::string::npos);
  }
  ROSE_ASSERT(empty_member_default_reference_count == 1);

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(project, V_SgVarRefExp);
  bool saw_field = false;
  bool saw_converted_field = false;
  for (SgNode *node : var_refs) {
    SgVarRefExp *var_ref = isSgVarRefExp(node);
    if (var_ref == nullptr) {
      continue;
    }
    Sg_File_Info *info = var_ref->get_file_info();
    if (info == nullptr || !info->isOutputInCodeGeneration()) {
      continue;
    }
    SgVariableSymbol *symbol = var_ref->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    if (symbol->get_name() == "templ_field") {
      saw_field = true;
      SgArrowExp *member_access = isSgArrowExp(var_ref->get_parent());
      if (member_access != nullptr) {
        SgCastExp *base_cast = isSgCastExp(member_access->get_lhs_operand());
        ROSE_ASSERT(base_cast != nullptr);
        ROSE_ASSERT(base_cast->cast_type() == SgCastExp::e_implicit_cast);
        ROSE_ASSERT(base_cast->get_file_info() != nullptr);
        ROSE_ASSERT(base_cast->get_file_info()->isImplicitCast());
        ROSE_ASSERT(base_cast->get_operand() != nullptr);
        ROSE_ASSERT(base_cast->get_operand()->get_parent() == base_cast);
        ROSE_ASSERT(base_cast->get_type() !=
                    base_cast->get_operand()->get_type());
        AstNodeType field_type;
        std::string field_name;
        AstNodePtr field_scope;
        ROSE_ASSERT(AstInterface::IsVarRef(
            AstNodePtr(member_access), &field_type, &field_name, &field_scope));
        ROSE_ASSERT(field_name.find("templ_field") != std::string::npos);
        ROSE_ASSERT(field_type.get_ptr() == symbol->get_type());
        ROSE_ASSERT(field_scope != AST_NULL);
        saw_converted_field = true;
      }
    }
  }
  ROSE_ASSERT(saw_field);
  ROSE_ASSERT(saw_converted_field);

  bool saw_nested_syntax_expression_list = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgExprListExp)) {
    SgExprListExp *expression_list = isSgExprListExp(node);
    ROSE_ASSERT(expression_list != nullptr);
    ROSE_ASSERT(!expression_list->has_semantic_value_type());
    if (isSgExprListExp(expression_list->get_parent()) != nullptr) {
      saw_nested_syntax_expression_list = true;
    }
  }
  // The constructor syntax for a variable-template initializer contains a
  // nested expression-list container.  It has no standalone value type; only
  // its semantic child expressions may be queried for one by the unparser.
  ROSE_ASSERT(saw_nested_syntax_expression_list);

  SgTemplateClassDeclaration *holder_definition = nullptr;
  for (SgNode *node : tmpl_classes) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == "Holder" &&
        declaration->get_definingDeclaration() == declaration) {
      ROSE_ASSERT(holder_definition == nullptr);
      holder_definition = declaration;
    }
  }
  ROSE_ASSERT(holder_definition != nullptr);

  SgFunctionDeclaration *use_field_definition = nullptr;
  Rose_STL_Container<SgNode *> functions =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  for (SgNode *node : functions) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == "use_field" &&
        declaration->get_definingDeclaration() == declaration) {
      ROSE_ASSERT(use_field_definition == nullptr);
      use_field_definition = declaration;
    }
  }
  ROSE_ASSERT(use_field_definition != nullptr);

  SgGlobal *global = isSgGlobal(holder_definition->get_parent());
  ROSE_ASSERT(global != nullptr);
  ROSE_ASSERT(use_field_definition->get_parent() == global);
  const SgDeclarationStatementPtrList &declarations =
      global->get_declarations();
  const auto holder_position =
      std::find(declarations.begin(), declarations.end(), holder_definition);
  const auto use_field_position =
      std::find(declarations.begin(), declarations.end(), use_field_definition);
  ROSE_ASSERT(holder_position != declarations.end());
  ROSE_ASSERT(use_field_position != declarations.end());
  ROSE_ASSERT(holder_position < use_field_position);

  SgFunctionDeclaration *use_field_canonical = isSgFunctionDeclaration(
      use_field_definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(use_field_canonical != nullptr);
  ROSE_ASSERT(use_field_canonical != use_field_definition);
  SgAuxiliaryDeclarationList *use_field_auxiliary =
      isSgAuxiliaryDeclarationList(use_field_canonical->get_parent());
  ROSE_ASSERT(use_field_auxiliary != nullptr);
  ROSE_ASSERT(use_field_auxiliary->get_parent() == global);
  ROSE_ASSERT(global->get_auxiliary_declarations() == use_field_auxiliary);
  ROSE_ASSERT(!global->statementExistsInScope(use_field_canonical));
  ROSE_ASSERT(std::count(use_field_auxiliary->get_declarations().begin(),
                         use_field_auxiliary->get_declarations().end(),
                         use_field_canonical) == 1);
  ROSE_ASSERT(!use_field_auxiliary->isOutputInCodeGeneration());

  SgFunctionParameterList *canonical_parameters =
      use_field_canonical->get_parameterList();
  ROSE_ASSERT(canonical_parameters != nullptr);
  ROSE_ASSERT(canonical_parameters->get_args().size() == 1);
  SgInitializedName *canonical_parameter =
      canonical_parameters->get_args().front();
  ROSE_ASSERT(canonical_parameter != nullptr);
  SgType *parameter_type = canonical_parameter->get_type()->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE);
  SgClassType *holder_type = isSgClassType(parameter_type);
  ROSE_ASSERT(holder_type != nullptr);
  SgTemplateInstantiationDecl *holder_instantiation =
      isSgTemplateInstantiationDecl(holder_type->get_declaration());
  ROSE_ASSERT(holder_instantiation != nullptr);
  SgTemplateClassDeclaration *holder_canonical = isSgTemplateClassDeclaration(
      holder_definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(holder_canonical != nullptr);
  ROSE_ASSERT(holder_canonical != holder_definition);
  ROSE_ASSERT(holder_canonical->get_definingDeclaration() == holder_definition);
  ROSE_ASSERT(holder_instantiation->get_templateDeclaration() ==
              holder_canonical);

  std::set<SgTemplateVariableDeclaration *> original_resolutions;
  std::set<SgFunctionDeclaration *> original_function_resolutions;
  std::set<SgNonrealDecl *> original_spelling_declarations;
  std::set<SgDeclarationScope *> original_spelling_scopes;
  size_t original_resolved_reference_count = 0;
  size_t original_resolved_function_reference_count = 0;
  for (SgNode *node : nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference != nullptr &&
        reference->get_resolved_variable_declaration() != nullptr) {
      ++original_resolved_reference_count;
      original_resolutions.insert(
          reference->get_resolved_variable_declaration());
      SgNonrealDecl *spelling = reference->get_symbol()->get_declaration();
      ROSE_ASSERT(spelling != nullptr);
      SgDeclarationScope *spelling_scope =
          isSgDeclarationScope(spelling->get_scope());
      ROSE_ASSERT(spelling_scope != nullptr);
      ROSE_ASSERT(spelling->get_parent() == spelling_scope);
      ROSE_ASSERT(spelling_scope->find_symbol_from_declaration(spelling) ==
                  reference->get_symbol());
      original_spelling_declarations.insert(spelling);
      original_spelling_scopes.insert(spelling_scope);
    }
    if (reference != nullptr &&
        reference->get_resolved_function_declaration() != nullptr) {
      ++original_resolved_function_reference_count;
      original_function_resolutions.insert(
          reference->get_resolved_function_declaration());
      SgNonrealDecl *spelling = reference->get_symbol()->get_declaration();
      ROSE_ASSERT(spelling != nullptr);
      SgDeclarationScope *spelling_scope =
          isSgDeclarationScope(spelling->get_scope());
      ROSE_ASSERT(spelling_scope != nullptr);
      ROSE_ASSERT(spelling->get_parent() == spelling_scope);
      ROSE_ASSERT(spelling_scope->find_symbol_from_declaration(spelling) ==
                  reference->get_symbol());
      original_spelling_declarations.insert(spelling);
      original_spelling_scopes.insert(spelling_scope);
    }
  }
  SgProject *project_copy = isSgProject(SageInterface::deepCopy(project));
  ROSE_ASSERT(project_copy != nullptr);
  Rose_STL_Container<SgNode *> copied_nonreal_refs =
      NodeQuery::querySubTree(project_copy, V_SgNonrealRefExp);
  size_t copied_variable_template_references = 0;
  size_t copied_function_template_references = 0;
  for (SgNode *node : copied_nonreal_refs) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(node);
    if (reference == nullptr) {
      continue;
    }
    if (reference->get_resolved_variable_declaration() != nullptr ||
        reference->get_resolved_function_declaration() != nullptr) {
      ROSE_ASSERT(reference->get_symbol() != nullptr);
      SgNonrealDecl *copied_spelling =
          reference->get_symbol()->get_declaration();
      ROSE_ASSERT(copied_spelling != nullptr);
      SgDeclarationScope *copied_spelling_scope =
          isSgDeclarationScope(copied_spelling->get_scope());
      ROSE_ASSERT(copied_spelling_scope != nullptr);
      ROSE_ASSERT(copied_spelling->get_parent() == copied_spelling_scope);
      ROSE_ASSERT(copied_spelling_scope->find_symbol_from_declaration(
                      copied_spelling) == reference->get_symbol());
      ROSE_ASSERT(original_spelling_declarations.count(copied_spelling) == 0);
      ROSE_ASSERT(original_spelling_scopes.count(copied_spelling_scope) == 0);
    }
    if (reference->get_resolved_variable_declaration() != nullptr) {
      ++copied_variable_template_references;
      SgInitializedName *copied_name =
          SageInterface::requireResolvedVariableTemplateReference(
              reference, "template-symbol deep-copy regression");
      SgTemplateVariableDeclaration *copied_resolution =
          reference->get_resolved_variable_declaration();
      ROSE_ASSERT(copied_name->get_parent() == copied_resolution);
      ROSE_ASSERT(original_resolutions.count(copied_resolution) == 0);
      ROSE_ASSERT(reference->get_symbol()
                      ->get_declaration()
                      ->get_templateDeclaration() == copied_resolution);
    }
    if (SgFunctionDeclaration *copied_function =
            reference->get_resolved_function_declaration()) {
      ++copied_function_template_references;
      ROSE_ASSERT(original_function_resolutions.count(copied_function) == 0);
    }
  }
  ROSE_ASSERT(copied_variable_template_references ==
              original_resolved_reference_count);
  ROSE_ASSERT(copied_function_template_references ==
              original_resolved_function_reference_count);
  ROSE_ASSERT(original_resolved_function_reference_count == 1);
  AstTests::runAllTests(project_copy);

  AstTests::runAllTests(project);
  return backend(project);
}
