#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void applyTypesAndSymbols(const AstFileRecord &ast, const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;

    restoreQualifiedNameState(node, p, nodes);

    if (SgScopeStatement *scope = isSgScopeStatement(node)) {
      scope->setCaseInsensitive(
          p.boolOr("case_insensitive", scope->isCaseInsensitive()));
    }

    if (SgInitializedName *name = isSgInitializedName(node)) {
      name->set_name_qualification_length(
          static_cast<int>(p.at("name_qualification_length").asInt()));
      name->set_type_elaboration_required(
          p.at("type_elaboration_required").asBool());
      name->set_global_qualification_required(
          p.at("global_qualification_required").asBool());
      name->set_name_qualification_length_for_type(
          static_cast<int>(p.at("name_qualification_length_for_type").asInt()));
      name->set_type_elaboration_required_for_type(
          p.at("type_elaboration_required_for_type").asBool());
      name->set_global_qualification_required_for_type(
          p.at("global_qualification_required_for_type").asBool());
    }

    if (SgTemplateInstantiationTypedefDeclaration *tmpl =
            isSgTemplateInstantiationTypedefDeclaration(node)) {
      tmpl->set_templateName(SgName(
          p.stringOr("template_name", tmpl->get_templateName().getString())));
      tmpl->set_templateHeader(SgName(p.stringOr(
          "template_header", tmpl->get_templateHeader().getString())));
      tmpl->set_nameResetFromMangledForm(
          p.boolOr("name_reset_from_mangled_form",
                   tmpl->get_nameResetFromMangledForm()));
      if (const JsonValue *template_arguments = p.find("template_arguments")) {
        tmpl->get_templateArguments() =
            templateArgumentListFromJson(*template_arguments, nodes, tmpl);
      }
      if (const JsonValue *deduced_template_arguments =
              p.find("deduced_template_arguments")) {
        tmpl->get_deducedTemplateArguments() = templateArgumentListFromJson(
            *deduced_template_arguments, nodes, tmpl);
      }
    }
    if (SgTemplateInstantiationFunctionDecl *tmpl =
            isSgTemplateInstantiationFunctionDecl(node)) {
      tmpl->set_templateName(SgName(
          p.stringOr("template_name", tmpl->get_templateName().getString())));
      tmpl->set_nameResetFromMangledForm(
          p.boolOr("name_reset_from_mangled_form",
                   tmpl->get_nameResetFromMangledForm()));
      tmpl->set_template_argument_list_is_explicit(
          p.boolOr("template_argument_list_is_explicit",
                   tmpl->get_template_argument_list_is_explicit()));
      if (const JsonValue *template_arguments = p.find("template_arguments")) {
        tmpl->get_templateArguments() =
            templateArgumentListFromJson(*template_arguments, nodes, tmpl);
      }
      if (const JsonValue *deduced_template_arguments =
              p.find("deduced_template_arguments")) {
        tmpl->get_deducedTemplateArguments() = templateArgumentListFromJson(
            *deduced_template_arguments, nodes, tmpl);
      }
    }
    if (SgTemplateInstantiationMemberFunctionDecl *tmpl =
            isSgTemplateInstantiationMemberFunctionDecl(node)) {
      tmpl->set_templateName(SgName(
          p.stringOr("template_name", tmpl->get_templateName().getString())));
      tmpl->set_nameResetFromMangledForm(
          p.boolOr("name_reset_from_mangled_form",
                   tmpl->get_nameResetFromMangledForm()));
      tmpl->set_template_argument_list_is_explicit(
          p.boolOr("template_argument_list_is_explicit",
                   tmpl->get_template_argument_list_is_explicit()));
      if (const JsonValue *template_arguments = p.find("template_arguments")) {
        tmpl->get_templateArguments() =
            templateArgumentListFromJson(*template_arguments, nodes, tmpl);
      }
      if (const JsonValue *deduced_template_arguments =
              p.find("deduced_template_arguments")) {
        tmpl->get_deducedTemplateArguments() = templateArgumentListFromJson(
            *deduced_template_arguments, nodes, tmpl);
      }
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      decl->set_decl_attributes(
          static_cast<unsigned int>(p.at("decl_attributes").asInt()));
      decl->set_linkage(p.at("linkage").asString());
      decl->get_declarationModifier().set_modifierVector(bitVectorFromJson(
          p.at("declaration_modifier_vector"), "declaration_modifier_vector"));
      decl->get_declarationModifier().get_typeModifier().set_modifierVector(
          bitVectorFromJson(p.at("declaration_type_modifier_vector"),
                            "declaration_type_modifier_vector"));
      decl->get_declarationModifier().get_storageModifier().set_modifier(
          static_cast<SgStorageModifier::storage_modifier_enum>(
              p.at("declaration_storage_modifier").asInt()));
      decl->get_declarationModifier().get_accessModifier().set_modifier(
          static_cast<SgAccessModifier::access_modifier_enum>(
              p.at("declaration_access_modifier").asInt()));
      decl->get_declarationModifier().get_accessModifier().set_is_explicit(
          p.boolOr("declaration_access_is_explicit", false));
      decl->set_nameOnly(p.at("name_only").asBool());
      decl->set_forward(p.at("forward").asBool());
      decl->set_externBrace(p.at("extern_brace").asBool());
      decl->set_skipElaborateType(p.at("skip_elaborate_type").asBool());
      decl->set_binding_label(p.at("binding_label").asString());
      decl->set_unparse_template_ast(p.at("unparse_template_ast").asBool());
      decl->get_declarationModifier().set_gnu_attribute_section_name(
          p.at("declaration_gnu_attribute_section_name").asString());
      decl->get_declarationModifier().set_gnu_attribute_visability(
          static_cast<SgDeclarationModifier::gnu_declaration_visability_enum>(
              p.at("declaration_gnu_attribute_visability").asInt()));
    }

    if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
      decl->get_functionModifier().set_modifierVector(bitVectorFromJson(
          p.at("function_modifier_vector"), "function_modifier_vector"));
      decl->get_specialFunctionModifier().set_modifierVector(
          bitVectorFromJson(p.at("special_function_modifier_vector"),
                            "special_function_modifier_vector"));
      decl->set_named_in_end_statement(p.at("named_in_end_statement").asBool());
      decl->set_asm_name(p.at("asm_name").asString());
      decl->set_oldStyleDefinition(p.at("old_style_definition").asBool());
      decl->set_specialization(
          static_cast<SgDeclarationStatement::template_specialization_enum>(
              p.at("specialization").asInt()));
      decl->set_requiresNameQualificationOnReturnType(
          p.at("requires_name_qualification_on_return_type").asBool());
      decl->set_gnu_extension_section(p.at("gnu_extension_section").asString());
      decl->set_gnu_extension_alias(p.at("gnu_extension_alias").asString());
      decl->set_gnu_extension_visability(
          static_cast<
              SgDeclarationStatement::gnu_extension_visability_attribute_enum>(
              p.at("gnu_extension_visability").asInt()));
      decl->set_name_qualification_length(
          static_cast<int>(p.at("name_qualification_length").asInt()));
      decl->set_type_elaboration_required(
          p.at("type_elaboration_required").asBool());
      decl->set_global_qualification_required(
          p.at("global_qualification_required").asBool());
      decl->set_name_qualification_length_for_return_type(static_cast<int>(
          p.at("name_qualification_length_for_return_type").asInt()));
      decl->set_type_elaboration_required_for_return_type(
          p.at("type_elaboration_required_for_return_type").asBool());
      decl->set_global_qualification_required_for_return_type(
          p.at("global_qualification_required_for_return_type").asBool());
      decl->set_prototypeIsWithoutParameters(
          p.at("prototype_is_without_parameters").asBool());
      decl->set_gnu_regparm_attribute(
          static_cast<int>(p.at("gnu_regparm_attribute").asInt()));
      decl->set_type_syntax_is_available(
          p.at("type_syntax_is_available").asBool());
      decl->set_using_C11_Noreturn_keyword(
          p.at("using_c11_noreturn_keyword").asBool());
      decl->set_is_constexpr(p.at("is_constexpr").asBool());
      decl->set_using_new_function_return_type_syntax(
          p.at("using_new_function_return_type_syntax").asBool());
      decl->set_is_deduction_guide(p.at("is_deduction_guide").asBool());
      decl->set_marked_as_frontend_normalization(
          p.at("marked_as_frontend_normalization").asBool());
      decl->set_is_implicit_function(p.at("is_implicit_function").asBool());
      if (SgProcedureHeaderStatement *procedure =
              isSgProcedureHeaderStatement(decl)) {
        procedure->set_subprogram_kind(
            static_cast<SgProcedureHeaderStatement::subprogram_kind_enum>(
                p.at("subprogram_kind").asInt()));
      }
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      auto restore_declaration_qualification = [&](auto *qualified_decl) {
        qualified_decl->set_name_qualification_length(
            static_cast<int>(p.at("name_qualification_length").asInt()));
        qualified_decl->set_type_elaboration_required(
            p.at("type_elaboration_required").asBool());
        qualified_decl->set_global_qualification_required(
            p.at("global_qualification_required").asBool());
      };
      if (isSgFunctionDeclaration(decl) == nullptr) {
        if (SgVariableDeclaration *qualified = isSgVariableDeclaration(decl)) {
          restore_declaration_qualification(qualified);
        } else if (SgEnumDeclaration *qualified = isSgEnumDeclaration(decl)) {
          restore_declaration_qualification(qualified);
        } else if (SgTypedefDeclaration *qualified =
                       isSgTypedefDeclaration(decl)) {
          restore_declaration_qualification(qualified);
        } else if (SgUsingDirectiveStatement *qualified =
                       isSgUsingDirectiveStatement(decl)) {
          restore_declaration_qualification(qualified);
        } else if (SgUsingDeclarationStatement *qualified =
                       isSgUsingDeclarationStatement(decl)) {
          restore_declaration_qualification(qualified);
        } else if (SgClassDeclaration *qualified = isSgClassDeclaration(decl)) {
          restore_declaration_qualification(qualified);
        }
      }
      if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
        variable->set_requiresGlobalNameQualificationOnType(
            p.at("requires_global_name_qualification_on_type").asBool());
      }
    }

    if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
      decl->set_isUnNamed(p.boolOr("is_unnamed", decl->get_isUnNamed()));
      decl->set_isAutonomousDeclaration(p.boolOr(
          "is_autonomous_declaration", decl->get_isAutonomousDeclaration()));
      if (SgTemplateClassDeclaration *tmpl =
              isSgTemplateClassDeclaration(decl)) {
        std::string template_name =
            p.stringOr("template_name", tmpl->get_templateName().getString());
        if (template_name.empty()) {
          template_name = decl->get_name().getString();
        }
        tmpl->set_templateName(SgName(template_name));
      }
      if (SgTemplateInstantiationDecl *tmpl =
              isSgTemplateInstantiationDecl(decl)) {
        std::string template_name =
            p.stringOr("template_name", tmpl->get_templateName().getString());
        if (template_name.empty()) {
          template_name = decl->get_name().getString();
          const size_t args = template_name.find('<');
          if (args != std::string::npos) {
            template_name = trim(template_name.substr(0, args));
          }
        }
        tmpl->set_templateName(SgName(template_name));
        tmpl->set_templateHeader(SgName(p.stringOr(
            "template_header", tmpl->get_templateHeader().getString())));
        tmpl->set_nameResetFromMangledForm(
            p.boolOr("name_reset_from_mangled_form",
                     tmpl->get_nameResetFromMangledForm()));
        tmpl->set_constraintSatisfactionEvaluated(
            p.boolOr("constraint_satisfaction_evaluated",
                     tmpl->get_constraintSatisfactionEvaluated()));
        tmpl->set_constraintSatisfactionSatisfied(
            p.boolOr("constraint_satisfaction_satisfied",
                     tmpl->get_constraintSatisfactionSatisfied()));
        tmpl->set_constraintSatisfactionContainsErrors(
            p.boolOr("constraint_satisfaction_contains_errors",
                     tmpl->get_constraintSatisfactionContainsErrors()));
        tmpl->set_constraintSatisfactionSubstitutionFailure(
            p.boolOr("constraint_satisfaction_substitution_failure",
                     tmpl->get_constraintSatisfactionSubstitutionFailure()));
        tmpl->set_constraintSatisfactionSummary(
            p.stringOr("constraint_satisfaction_summary",
                       tmpl->get_constraintSatisfactionSummary()));
        tmpl->set_sfinaeEvaluated(
            p.boolOr("sfinae_evaluated", tmpl->get_sfinaeEvaluated()));
        tmpl->set_sfinaeSubstitutionFailure(
            p.boolOr("sfinae_substitution_failure",
                     tmpl->get_sfinaeSubstitutionFailure()));
        tmpl->set_sfinaeSummary(
            p.stringOr("sfinae_summary", tmpl->get_sfinaeSummary()));
        if (const JsonValue *template_arguments =
                p.find("template_arguments")) {
          tmpl->get_templateArguments() =
              templateArgumentListFromJson(*template_arguments, nodes, tmpl);
        }
        if (const JsonValue *deduced_template_arguments =
                p.find("deduced_template_arguments")) {
          tmpl->get_deducedTemplateArguments() = templateArgumentListFromJson(
              *deduced_template_arguments, nodes, tmpl);
        }
      }
      if (SgClassDeclaration *first_nondef =
              isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
        if (first_nondef != decl) {
          decl->set_type(ensureClassTypeForDeclaration(first_nondef));
        }
      }
      ensureClassTypeForDeclaration(decl);
    } else if (SgBaseClass *base = isSgBaseClass(node)) {
      base->set_isDirectBaseClass(
          p.boolOr("is_direct_base_class", base->get_isDirectBaseClass()));
      base->set_pack_expansion(
          p.boolOr("pack_expansion", base->get_pack_expansion()));
      if (base->get_baseClassModifier() != nullptr) {
        base->get_baseClassModifier()->set_modifier(
            static_cast<SgBaseClassModifier::baseclass_modifier_enum>(
                p.intOr("base_class_modifier",
                        base->get_baseClassModifier()->get_modifier())));
        base->get_baseClassModifier()->get_accessModifier().set_modifier(
            static_cast<SgAccessModifier::access_modifier_enum>(p.intOr(
                "base_class_access_modifier", base->get_baseClassModifier()
                                                  ->get_accessModifier()
                                                  .get_modifier())));
        base->get_baseClassModifier()->get_accessModifier().set_is_explicit(
            p.boolOr("base_class_access_is_explicit",
                     base->get_baseClassModifier()
                         ->get_accessModifier()
                         .get_is_explicit()));
      }
      base->set_name_qualification_length(static_cast<int>(p.intOr(
          "name_qualification_length", base->get_name_qualification_length())));
      base->set_type_elaboration_required(p.boolOr(
          "type_elaboration_required", base->get_type_elaboration_required()));
      base->set_global_qualification_required(
          p.boolOr("global_qualification_required",
                   base->get_global_qualification_required()));
    } else if (SgNamespaceDeclarationStatement *decl =
                   isSgNamespaceDeclarationStatement(node)) {
      decl->set_isUnnamedNamespace(
          p.boolOr("is_unnamed_namespace", decl->get_isUnnamedNamespace()));
      decl->set_isInlinedNamespace(
          p.boolOr("is_inlined_namespace", decl->get_isInlinedNamespace()));
    } else if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
      stmt->set_implicit_none(
          p.boolOr("implicit_none", stmt->get_implicit_none()));
      stmt->set_implicit_spec(
          static_cast<SgImplicitStatement::implicit_spec_enum>(
              p.intOr("implicit_spec", stmt->get_implicit_spec())));
    } else if (SgFortranIncludeLine *stmt = isSgFortranIncludeLine(node)) {
      stmt->set_filename(p.stringOr("filename", stmt->get_filename()));
    } else if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
      stmt->set_label(
          SgName(p.stringOr("label", stmt->get_label().getString())));
      stmt->set_gnu_extension_unused(
          p.boolOr("gnu_extension_unused", stmt->get_gnu_extension_unused()));
    } else if (SgIfStmt *stmt = isSgIfStmt(node)) {
      stmt->set_string_label(
          p.stringOr("string_label", stmt->get_string_label()));
      stmt->set_has_end_statement(
          p.boolOr("has_end_statement", stmt->get_has_end_statement()));
      stmt->set_use_then_keyword(
          p.boolOr("use_then_keyword", stmt->get_use_then_keyword()));
      stmt->set_is_else_if_statement(
          p.boolOr("is_else_if_statement", stmt->get_is_else_if_statement()));
    } else if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
      stmt->set_string_label(
          p.stringOr("string_label", stmt->get_string_label()));
      stmt->set_has_end_statement(
          p.boolOr("has_end_statement", stmt->get_has_end_statement()));
    } else if (SgFortranDo *stmt = isSgFortranDo(node)) {
      stmt->set_string_label(
          p.stringOr("string_label", stmt->get_string_label()));
      stmt->set_old_style(p.boolOr("old_style", stmt->get_old_style()));
      stmt->set_has_end_statement(
          p.boolOr("has_end_statement", stmt->get_has_end_statement()));
    } else if (SgIOStatement *stmt = isSgIOStatement(node)) {
      stmt->set_io_statement(static_cast<SgIOStatement::io_statement_enum>(
          p.intOr("io_statement", stmt->get_io_statement())));
    } else if (SgAttributeSpecificationStatement *stmt =
                   isSgAttributeSpecificationStatement(node)) {
      stmt->set_attribute_kind(
          static_cast<SgAttributeSpecificationStatement::attribute_spec_enum>(
              p.intOr("attribute_kind", stmt->get_attribute_kind())));
      stmt->set_intent(static_cast<int>(p.intOr("intent", stmt->get_intent())));
      if (const JsonValue *names = p.find("name_list")) {
        stmt->get_name_list() = stringListFromJson(*names, "name_list");
      }
    } else if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
      stmt->set_name(SgName(p.stringOr("name", stmt->get_name().getString())));
      stmt->set_generic_spec(
          static_cast<SgInterfaceStatement::generic_spec_enum>(
              p.intOr("generic_spec", stmt->get_generic_spec())));
    } else if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
      body->set_function_name(SgName(
          p.stringOr("function_name", body->get_function_name().getString())));
      body->set_use_function_name(
          p.boolOr("use_function_name", body->get_use_function_name()));
    } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
      decl->set_template_parameter_position(
          static_cast<int>(p.intOr("template_parameter_position",
                                   decl->get_template_parameter_position())));
      decl->set_template_parameter_depth(static_cast<int>(p.intOr(
          "template_parameter_depth", decl->get_template_parameter_depth())));
      decl->set_is_class_member(
          p.boolOr("is_class_member", decl->get_is_class_member()));
      decl->set_is_template_param(
          p.boolOr("is_template_param", decl->get_is_template_param()));
      decl->set_is_template_template_param(
          p.boolOr("is_template_template_param",
                   decl->get_is_template_template_param()));
      decl->set_has_template_keyword(
          p.boolOr("has_template_keyword", decl->get_has_template_keyword()));
      decl->set_has_global_qualifier(
          p.boolOr("has_global_qualifier", decl->get_has_global_qualifier()));
      decl->set_suppress_typename(
          p.boolOr("suppress_typename", decl->get_suppress_typename()));
      decl->set_is_nonreal_template(
          p.boolOr("is_nonreal_template", decl->get_is_nonreal_template()));
      decl->set_is_concept(p.boolOr("is_concept", decl->get_is_concept()));
      decl->set_is_nonreal_function(
          p.boolOr("is_nonreal_function", decl->get_is_nonreal_function()));
      if (decl->get_type() == nullptr) {
        decl->set_type(new SgNonrealType(decl));
      }
      if (const JsonValue *type = p.find("type")) {
        decl->get_type()->set_autonomous_declaration(
            type->boolOr("autonomous_declaration",
                         decl->get_type()->get_autonomous_declaration()));
        attachJsonTypeText(decl->get_type(), *type);
      }
    } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
      decl->set_embedded(p.boolOr("embedded", decl->get_embedded()));
      decl->set_isUnNamed(p.boolOr("is_unnamed", decl->get_isUnNamed()));
      decl->set_isAutonomousDeclaration(p.boolOr(
          "is_autonomous_declaration", decl->get_isAutonomousDeclaration()));
      decl->set_isScopedEnum(
          p.boolOr("is_scoped_enum", decl->get_isScopedEnum()));
      if (const JsonValue *field_type = p.find("field_type")) {
        decl->set_field_type(field_type->boolOr("present", false)
                                 ? typeFromJson(*field_type, nodes)
                                 : nullptr);
      }
      if (decl->get_type() == nullptr) {
        decl->set_type(SgEnumType::createType(decl));
      }
    } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
      decl->set_typedefBaseTypeContainsDefiningDeclaration(
          p.boolOr("typedef_base_type_contains_defining_declaration", false));
      decl->set_isAutonomousDeclaration(p.boolOr(
          "is_autonomous_declaration", decl->get_isAutonomousDeclaration()));
      if (const JsonValue *type = p.find("base_type")) {
        if (uint64_t target = singleEdgeTarget(record, "declaration")) {
          SgDeclarationStatement *base_decl =
              nodeByIdAs<SgDeclarationStatement>(nodes, target);
          decl->set_base_type(typeFromJson(*type, nodes));
          decl->set_declaration(base_decl);
          if (base_decl->get_parent() == nullptr) {
            base_decl->set_parent(decl);
          }
        } else {
          decl->set_base_type(typeFromJson(*type, nodes));
        }
      }
      if (decl->get_type() == nullptr) {
        decl->set_type(new SgTypedefType(decl, nullptr));
      }
    } else if (SgInitializedName *name = isSgInitializedName(node)) {
      if (const JsonValue *type = p.find("type")) {
        name->set_typeptr(typeFromJson(*type, nodes));
      }
      name->get_storageModifier().set_modifier(
          static_cast<SgStorageModifier::storage_modifier_enum>(
              p.intOr("storage_modifier", SgStorageModifier::e_default)));
      const std::string section = p.stringOr("gnu_attribute_section_name");
      if (!section.empty()) {
        name->set_gnu_attribute_section_name(section);
      }
    } else if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
      argument->set_argumentType(
          static_cast<SgTemplateArgument::template_argument_enum>(p.intOr(
              "argument_type", SgTemplateArgument::argument_undefined)));
      argument->set_isArrayBoundUnknownType(
          p.boolOr("is_array_bound_unknown_type", false));
      if (const JsonValue *type = p.find("type")) {
        argument->set_type(nullableTypeFromJson(*type, nodes));
      }
      if (const JsonValue *expr = p.find("expression")) {
        SgExpression *expression = expressionFromRef(*expr, nodes);
        argument->set_expression(expression);
        if (expression != nullptr) {
          expression->set_parent(argument);
        }
      }
      if (uint64_t target =
              static_cast<uint64_t>(p.intOr("template_declaration", 0))) {
        argument->set_templateDeclaration(
            nodeByIdAs<SgDeclarationStatement>(nodes, target));
      }
      if (uint64_t target =
              static_cast<uint64_t>(p.intOr("initialized_name", 0))) {
        argument->set_initializedName(
            nodeByIdAs<SgInitializedName>(nodes, target));
      }
      argument->set_explicitlySpecified(p.boolOr("explicitly_specified", true));
      argument->set_is_pack_element(p.boolOr("is_pack_element", false));
    } else if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
      parameter->set_parameterType(
          static_cast<SgTemplateParameter::template_parameter_enum>(p.intOr(
              "parameter_type", SgTemplateParameter::parameter_undefined)));
      if (const JsonValue *type = p.find("type")) {
        parameter->set_type(nullableTypeFromJson(*type, nodes));
      }
      if (const JsonValue *type = p.find("default_type_parameter")) {
        parameter->set_defaultTypeParameter(nullableTypeFromJson(*type, nodes));
      }
      if (const JsonValue *expr = p.find("expression")) {
        SgExpression *expression = expressionFromRef(*expr, nodes);
        parameter->set_expression(expression);
        if (expression != nullptr) {
          expression->set_parent(parameter);
        }
      }
      if (const JsonValue *expr = p.find("type_constraint")) {
        SgExpression *expression = expressionFromRef(*expr, nodes);
        parameter->set_typeConstraint(expression);
        if (expression != nullptr) {
          expression->set_parent(parameter);
        }
      }
      if (const JsonValue *expr = p.find("default_expression_parameter")) {
        SgExpression *expression = expressionFromRef(*expr, nodes);
        parameter->set_defaultExpressionParameter(expression);
        if (expression != nullptr) {
          expression->set_parent(parameter);
        }
      }
      if (uint64_t target =
              static_cast<uint64_t>(p.intOr("template_declaration", 0))) {
        parameter->set_templateDeclaration(
            nodeByIdAs<SgDeclarationStatement>(nodes, target));
      }
      if (uint64_t target = static_cast<uint64_t>(
              p.intOr("default_template_declaration_parameter", 0))) {
        parameter->set_defaultTemplateDeclarationParameter(
            nodeByIdAs<SgDeclarationStatement>(nodes, target));
      }
      if (uint64_t target =
              static_cast<uint64_t>(p.intOr("initialized_name", 0))) {
        parameter->set_initializedName(
            nodeByIdAs<SgInitializedName>(nodes, target));
      }
      parameter->set_templateParameterKeyword(
          static_cast<SgTemplateParameter::template_parameter_keyword_enum>(
              p.intOr("template_parameter_keyword",
                      SgTemplateParameter::keyword_unspecified)));
      parameter->set_isAbbreviatedFunctionTemplateParameter(
          p.boolOr("is_abbreviated_function_template_parameter", false));
      parameter->set_is_parameter_pack(p.boolOr("is_parameter_pack", false));
    } else if (SgExpression *expr = isSgExpression(node)) {
      if (const JsonValue *type = p.find("type")) {
        SgType *restored_type = typeFromJson(*type, nodes);
        if (SgCastExp *cast = isSgCastExp(expr)) {
          cast->set_type(restored_type);
        } else if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
          type_expr->set_type(restored_type);
        } else if (SgAggregateInitializer *init =
                       isSgAggregateInitializer(expr)) {
          init->set_expression_type(restored_type);
        } else if (SgCompoundInitializer *init =
                       isSgCompoundInitializer(expr)) {
          init->set_expression_type(restored_type);
        } else if (SgConstructorInitializer *init =
                       isSgConstructorInitializer(expr)) {
          init->set_expression_type(restored_type);
        }
      }
      expr->set_lvalue(p.boolOr("lvalue", expr->get_lvalue()));
      expr->set_need_paren(p.boolOr("need_paren", expr->get_need_paren()));
      expr->set_global_qualified_name(
          p.boolOr("global_qualified_name", expr->get_global_qualified_name()));
      restoreExpressionQualificationFields(expr, p);
      if (SgNewExp *new_expr = isSgNewExp(expr)) {
        if (const JsonValue *type = p.find("specified_type")) {
          new_expr->set_specified_type(typeFromJson(*type, nodes));
        }
        if (const JsonValue *type = p.find("type")) {
          installNewExpressionResultType(new_expr, typeFromJson(*type, nodes),
                                         *type);
        }
        new_expr->set_need_global_specifier(static_cast<short>(p.intOr(
            "need_global_specifier", new_expr->get_need_global_specifier())));
        new_expr->set_type_id_is_parenthesized(
            p.boolOr("type_id_is_parenthesized",
                     new_expr->get_type_id_is_parenthesized()));
      }
      if (SgDeleteExp *delete_expr = isSgDeleteExp(expr)) {
        delete_expr->set_is_array(static_cast<short>(
            p.intOr("is_array", delete_expr->get_is_array())));
        delete_expr->set_need_global_specifier(static_cast<short>(
            p.intOr("need_global_specifier",
                    delete_expr->get_need_global_specifier())));
      }
      if (SgEnumVal *value = isSgEnumVal(expr)) {
        if (uint64_t target =
                static_cast<uint64_t>(p.intOr("declaration", 0))) {
          value->set_declaration(nodeByIdAs<SgEnumDeclaration>(nodes, target));
        }
        value->set_requiresNameQualification(
            p.boolOr("requires_name_qualification",
                     value->get_requiresNameQualification()));
      } else if (SgTemplateParameterVal *value =
                     isSgTemplateParameterVal(expr)) {
        value->set_template_parameter_position(static_cast<int>(
            p.intOr("template_parameter_position",
                    value->get_template_parameter_position())));
        value->set_valueString(
            p.stringOr("value_string", value->get_valueString()));
        if (const JsonValue *type = p.find("type")) {
          value->set_valueType(typeFromJson(*type, nodes));
        }
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;

    if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
      if (decl->get_parameterList() == nullptr) {
        decl->set_parameterList(new SgFunctionParameterList());
        decl->get_parameterList()->set_parent(decl);
      }
      if (const JsonValue *function_type = p.find("function_type")) {
        if (SgFunctionType *restored_type =
                isSgFunctionType(typeFromJson(*function_type, nodes))) {
          decl->set_type(restored_type);
          continue;
        }
      }
      SgType *return_type = SageBuilder::buildIntType();
      if (const JsonValue *type = p.find("return_type")) {
        return_type = typeFromJson(*type, nodes);
      }
      decl->set_type(SageBuilder::buildFunctionType(return_type,
                                                    decl->get_parameterList()));
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      if (decl->get_scope() == nullptr) {
        if (SgScopeStatement *scope = nearestScope(decl)) {
          decl->set_scope(scope);
        }
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgDeclarationStatement *decl =
        isSgDeclarationStatement(nodeById(nodes, record.id));
    if (decl == nullptr) {
      continue;
    }

    SgDeclarationStatement *peers[] = {decl->get_definingDeclaration(),
                                       decl->get_firstNondefiningDeclaration()};
    for (SgDeclarationStatement *peer : peers) {
      if (peer == nullptr || peer == decl) {
        continue;
      }
      if (peer->get_scope() == nullptr && decl->get_scope() != nullptr) {
        peer->set_scope(decl->get_scope());
      }
      if (peer->get_parent() == nullptr && decl->get_parent() != nullptr) {
        peer->set_parent(decl->get_parent());
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (name->get_scope() == nullptr) {
        SgScopeStatement *scope = nullptr;
        if (SgDeclarationStatement *decl = name->get_declptr()) {
          scope = decl->get_scope();
        }
        if (scope == nullptr) {
          scope = nearestScope(name);
        }
        if (scope != nullptr) {
          name->set_scope(scope);
        }
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
      SgScopeStatement *scope = decl->get_scope();
      if (scope != nullptr) {
        for (SgInitializedName *name : decl->get_variables()) {
          if (name != nullptr && name->get_scope() == nullptr) {
            name->set_scope(scope);
          }
        }
      }
    } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
      SgScopeStatement *scope = decl->get_scope();
      if (scope != nullptr) {
        for (SgInitializedName *name : decl->get_enumerators()) {
          if (name != nullptr && name->get_scope() == nullptr) {
            name->set_scope(scope);
          }
        }
      }
    }
  }

  restoreRecordedScopeEdges(ast, nodes);
  restoreSerializedSymbolTables(ast, nodes);

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;
    if (SgVarRefExp *ref = isSgVarRefExp(node)) {
      if (const JsonValue *symbol_json = p.find("symbol")) {
        ref->set_symbol(
            isSgVariableSymbol(symbolFromJson(*symbol_json, nodes)));
      } else {
        const uint64_t decl_id =
            static_cast<uint64_t>(p.intOr("symbol_declaration", 0));
        if (decl_id != 0) {
          SgInitializedName *decl =
              isSgInitializedName(nodeById(nodes, decl_id));
          if (decl != nullptr) {
            ref->set_symbol(new SgVariableSymbol(decl));
          }
        }
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgVarRefExp symbol");
      }
      normalizeAnonymousDataMemberReference(ref);
    } else if (SgLabelRefExp *ref = isSgLabelRefExp(node)) {
      if (const JsonValue *symbol_json = p.find("symbol")) {
        ref->set_symbol(isSgLabelSymbol(symbolFromJson(*symbol_json, nodes)));
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgLabelRefExp symbol");
      }
    } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
      SgMemberFunctionSymbol *symbol = nullptr;
      if (const JsonValue *symbol_json = p.find("symbol")) {
        symbol = isSgMemberFunctionSymbol(symbolFromJson(*symbol_json, nodes));
        attachExternalSymbolBasisToScope(symbol, nearestScope(ref));
      } else {
        const uint64_t decl_id =
            static_cast<uint64_t>(p.intOr("symbol_declaration", 0));
        if (decl_id != 0) {
          SgMemberFunctionDeclaration *decl =
              isSgMemberFunctionDeclaration(nodeById(nodes, decl_id));
          if (decl != nullptr) {
            symbol = new SgMemberFunctionSymbol(decl);
          }
        }
      }
      ref->set_symbol_i(symbol);
      ref->set_virtual_call(static_cast<int>(p.intOr("virtual_call", 0)));
      ref->set_need_qualifier(
          static_cast<int>(p.intOr("need_qualifier", true)));
      if (ref->get_symbol_i() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgMemberFunctionRefExp symbol");
      }
    } else if (SgThisExp *expr = isSgThisExp(node)) {
      const uint64_t class_decl_id =
          static_cast<uint64_t>(p.intOr("class_symbol_declaration", 0));
      if (class_decl_id != 0) {
        expr->set_class_symbol(classSymbolForDeclaration(
            isSgClassDeclaration(nodeById(nodes, class_decl_id))));
      }
      const uint64_t nonreal_decl_id =
          static_cast<uint64_t>(p.intOr("nonreal_symbol_declaration", 0));
      if (nonreal_decl_id != 0) {
        expr->set_nonreal_symbol(nonrealSymbolForDeclaration(
            isSgNonrealDecl(nodeById(nodes, nonreal_decl_id))));
      }
      expr->set_pobj_this(static_cast<int>(p.intOr("pobj_this", 0)));
      if (expr->get_class_symbol() == nullptr &&
          expr->get_nonreal_symbol() == nullptr) {
        if (const JsonValue *type_json = p.find("type")) {
          if (SgPointerType *pointer =
                  isSgPointerType(typeFromJson(*type_json, nodes))) {
            if (SgClassType *class_type =
                    isSgClassType(pointer->get_base_type())) {
              expr->set_class_symbol(classSymbolForDeclaration(
                  isSgClassDeclaration(class_type->get_declaration())));
            } else if (SgNonrealType *nonreal_type =
                           isSgNonrealType(pointer->get_base_type())) {
              expr->set_nonreal_symbol(nonrealSymbolForDeclaration(
                  isSgNonrealDecl(nonreal_type->get_declaration())));
            }
          }
        }
      }
      if (expr->get_class_symbol() == nullptr &&
          expr->get_nonreal_symbol() == nullptr) {
        throw std::runtime_error("AST JSON failed to resolve SgThisExp symbol");
      }
    } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
      const uint64_t decl_id =
          static_cast<uint64_t>(p.intOr("symbol_declaration", 0));
      if (decl_id != 0) {
        ref->set_symbol(nonrealSymbolForDeclaration(
            isSgNonrealDecl(nodeById(nodes, decl_id))));
      }
      if (ref->get_symbol() == nullptr) {
        if (const JsonValue *type_json = p.find("type")) {
          if (SgNonrealType *nonreal_type =
                  isSgNonrealType(typeFromJson(*type_json, nodes))) {
            ref->set_symbol(nonrealSymbolForDeclaration(
                isSgNonrealDecl(nonreal_type->get_declaration())));
          }
        }
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgNonrealRefExp symbol");
      }
      if (const JsonValue *template_arguments = p.find("template_arguments")) {
        ref->get_templateArguments() =
            templateArgumentListFromJson(*template_arguments, nodes, ref);
      }
      ref->set_constraintSatisfactionEvaluated(
          p.boolOr("constraint_satisfaction_evaluated",
                   ref->get_constraintSatisfactionEvaluated()));
      ref->set_constraintSatisfactionSatisfied(
          p.boolOr("constraint_satisfaction_satisfied",
                   ref->get_constraintSatisfactionSatisfied()));
      ref->set_constraintSatisfactionContainsErrors(
          p.boolOr("constraint_satisfaction_contains_errors",
                   ref->get_constraintSatisfactionContainsErrors()));
      ref->set_constraintSatisfactionSubstitutionFailure(
          p.boolOr("constraint_satisfaction_substitution_failure",
                   ref->get_constraintSatisfactionSubstitutionFailure()));
      ref->set_constraintSatisfactionSummary(
          p.stringOr("constraint_satisfaction_summary",
                     ref->get_constraintSatisfactionSummary()));
      ref->set_sfinaeEvaluated(
          p.boolOr("sfinae_evaluated", ref->get_sfinaeEvaluated()));
      ref->set_sfinaeSubstitutionFailure(p.boolOr(
          "sfinae_substitution_failure", ref->get_sfinaeSubstitutionFailure()));
      ref->set_sfinaeSummary(
          p.stringOr("sfinae_summary", ref->get_sfinaeSummary()));
    } else if (SgTemplateFunctionRefExp *ref =
                   isSgTemplateFunctionRefExp(node)) {
      if (const JsonValue *symbol_json = p.find("symbol")) {
        SgTemplateFunctionSymbol *symbol =
            isSgTemplateFunctionSymbol(symbolFromJson(*symbol_json, nodes));
        attachExternalSymbolBasisToScope(symbol, nearestScope(ref));
        ref->set_symbol(symbol);
      } else {
        const uint64_t decl_id =
            static_cast<uint64_t>(p.intOr("symbol_declaration", 0));
        if (decl_id != 0) {
          SgFunctionDeclaration *decl =
              isSgFunctionDeclaration(nodeById(nodes, decl_id));
          if (decl != nullptr) {
            ref->set_symbol(new SgTemplateFunctionSymbol(decl));
          }
        }
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgTemplateFunctionRefExp symbol");
      }
    } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
      const uint64_t decl_id =
          static_cast<uint64_t>(p.intOr("symbol_declaration", 0));
      SgFunctionDeclaration *external_decl = nullptr;
      if (const JsonValue *symbol_json = p.find("symbol")) {
        SgFunctionSymbol *symbol =
            isSgFunctionSymbol(symbolFromJson(*symbol_json, nodes));
        attachExternalSymbolBasisToScope(symbol, nearestScope(ref));
        ref->set_symbol(symbol);
      } else if (decl_id != 0) {
        SgFunctionDeclaration *decl =
            isSgFunctionDeclaration(nodeById(nodes, decl_id));
        if (decl != nullptr) {
          ref->set_symbol(new SgFunctionSymbol(decl));
        }
      } else if (const JsonValue *external = p.find("external_function")) {
        external_decl = externalFunctionFromJson(*external, nodes);
        if (external_decl != nullptr) {
          external_decl->set_scope(nearestScope(ref));
          ref->set_symbol(new SgFunctionSymbol(external_decl));
        }
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgFunctionRefExp symbol");
      }
    }
  }

  applyOmpAuxiliaryState(ast, nodes);
}

void rebuildConstructorOnlyNodes(const AstFileRecord &ast, NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgTemplateInstantiationDefn") {
      const uint64_t parent = singleEdgeTarget(record, "parent");
      if (parent == 0) {
        throw std::runtime_error(
            "AST JSON SgTemplateInstantiationDefn is missing parent edge");
      }
      SgTemplateInstantiationDecl *decl =
          nodeByIdAs<SgTemplateInstantiationDecl>(nodes, parent);
      SgTemplateInstantiationDefn *def = new SgTemplateInstantiationDefn(decl);
      def->set_parent(decl);
      decl->set_definition(def);
      nodes[record.id] = def;
      continue;
    }

    const JsonValue *type = record.properties.find("type");
    if (type == nullptr) {
      continue;
    }
    if (isJsonBinaryOpKind(record.kind)) {
      nodes[record.id] =
          buildBinaryOpForKind(record.kind, typeFromJson(*type, nodes));
      continue;
    }
    if (isJsonUnaryOpKind(record.kind)) {
      nodes[record.id] = buildUnaryOpForKind(
          record.kind, typeFromJson(*type, nodes), record.properties);
      continue;
    }
    if (record.kind == "SgFunctionCallExp") {
      nodes[record.id] = new SgFunctionCallExp(
          static_cast<SgExpression *>(nullptr),
          static_cast<SgExprListExp *>(nullptr), typeFromJson(*type, nodes));
      continue;
    }
    if (record.kind == "SgConditionalExp") {
      nodes[record.id] = new SgConditionalExp(
          static_cast<SgExpression *>(nullptr),
          static_cast<SgExpression *>(nullptr),
          static_cast<SgExpression *>(nullptr), typeFromJson(*type, nodes));
      continue;
    }
    if (record.kind == "SgNewExp") {
      const JsonValue *specified_type_json =
          record.properties.find("specified_type");
      SgType *specified_type = specified_type_json != nullptr
                                   ? typeFromJson(*specified_type_json, nodes)
                                   : SageBuilder::buildUnknownType();
      SgNewExp *new_expr =
          new SgNewExp(specified_type, static_cast<SgExprListExp *>(nullptr),
                       static_cast<SgConstructorInitializer *>(nullptr),
                       static_cast<SgExpression *>(nullptr),
                       static_cast<short>(
                           record.properties.intOr("need_global_specifier", 0)),
                       static_cast<SgFunctionDeclaration *>(nullptr));
      installNewExpressionResultType(new_expr, typeFromJson(*type, nodes),
                                     *type);
      new_expr->set_type_id_is_parenthesized(
          record.properties.boolOr("type_id_is_parenthesized", false));
      nodes[record.id] = new_expr;
      continue;
    }
    if (record.kind == "SgConstructorInitializer") {
      SgConstructorInitializer *replacement = new SgConstructorInitializer(
          static_cast<SgMemberFunctionDeclaration *>(nullptr),
          static_cast<SgExprListExp *>(nullptr), typeFromJson(*type, nodes),
          record.properties.boolOr("need_name", false),
          record.properties.boolOr("need_qualifier", false),
          record.properties.boolOr("need_parenthesis_after_name", false),
          record.properties.boolOr("associated_class_unknown", false));
      nodes[record.id] = replacement;
      continue;
    }
    if (record.kind == "SgAssignInitializer") {
      SgAssignInitializer *replacement = new SgAssignInitializer(
          static_cast<SgExpression *>(nullptr), typeFromJson(*type, nodes));
      nodes[record.id] = replacement;
    }
  }
}

void restoreDeclarationIdentityEdges(const AstFileRecord &ast,
                                     const NodeMap &nodes) {
  auto attach_external_peer = [](SgDeclarationStatement *owner,
                                 SgDeclarationStatement *peer) {
    if (owner == nullptr || peer == nullptr) {
      return;
    }
    if (peer->get_scope() == nullptr) {
      peer->set_scope(owner->get_scope());
    }
    if (peer->get_parent() == nullptr) {
      peer->set_parent(owner->get_parent());
    }
  };

  for (const NodeRecord &record : ast.nodes) {
    SgDeclarationStatement *decl =
        isSgDeclarationStatement(nodeById(nodes, record.id));
    if (decl == nullptr) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      decl->set_parent(nodeById(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      decl->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "firstNondefiningDeclaration")) {
      decl->set_firstNondefiningDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (SgDeclarationStatement *external =
                   externalDeclarationReferenceFromJson(
                       record.properties.find(
                           "external_first_nondefining_declaration"),
                       nodes, record.kind + ".firstNondefiningDeclaration")) {
      attach_external_peer(decl, external);
      decl->set_firstNondefiningDeclaration(external);
    }
    if (uint64_t target = singleEdgeTarget(record, "definingDeclaration")) {
      decl->set_definingDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (SgDeclarationStatement *external =
                   externalDeclarationReferenceFromJson(
                       record.properties.find("external_defining_declaration"),
                       nodes, record.kind + ".definingDeclaration")) {
      attach_external_peer(decl, external);
      decl->set_definingDeclaration(external);
    }
  }
}

void restoreRecordedParentEdges(const AstFileRecord &ast,
                                const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      nodeById(nodes, record.id)->set_parent(nodeById(nodes, target));
    }
  }
}

void restoreRecordedScopeEdges(const AstFileRecord &ast, const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        name->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
      }
    }
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        decl->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
      }
    }
    if (SgLabelStatement *label = isSgLabelStatement(node)) {
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        label->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
      }
    }
  }
}

SgSourceFile *reconstructSourceFile(const AstFileRecord &ast,
                                    SgSourceFile *old_file) {
  SgProject *project = owningProject(old_file);
  ROSE_ASSERT(project != nullptr);
  DeserializationProjectGuard project_guard(project);
  restoreFileIdMapFromMetadata(ast.metadata);

  NodeMap nodes;
  nodes.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    if (requiresDelayedRebuild(record)) {
      continue;
    }
    SgNode *node = createNodeFromRecord(record, project, ast.metadata);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }

  restoreAvailableSourcePositionsAndScopes(ast, nodes);
  rebuildConstructorOnlyNodes(ast, nodes);
  restoreDeclarationIdentityEdges(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    attachJsonNodeId(nodeById(nodes, record.id), record.id);
  }
  for (const NodeRecord &record : ast.nodes) {
    requireRestoredKind(nodeById(nodes, record.id), record);
  }

  for (const NodeRecord &record : ast.nodes) {
    linkNodeEdges(record, nodes);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (SgClassDefinition *def =
            isSgClassDefinition(nodeById(nodes, record.id))) {
      SgNode *parent = def->get_parent();
      if (parent == nullptr || isSgClassDeclaration(parent) == nullptr) {
        std::ostringstream message;
        message << "AST JSON reconstructed " << record.kind << " id "
                << record.id << " without owning SgClassDeclaration";
        throw std::runtime_error(message.str());
      }
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    setNodeSourcePosition(node, record);
    setNodeFlags(node, record);
    attachPreprocessingInfo(node, record);
    attachAstAttributes(node, record);
    requireRestoredKind(nodeById(nodes, record.id), record);
  }
  applyTypesAndSymbols(ast, nodes);
  restoreRecordedScopeEdges(ast, nodes);
  restoreRecordedParentEdges(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    if (SgClassDeclaration *decl =
            isSgClassDeclaration(nodeById(nodes, record.id))) {
      if (SgClassDefinition *def = decl->get_definition()) {
        SgNode *parent = def->get_parent();
        if (parent == nullptr || isSgClassDeclaration(parent) == nullptr) {
          std::ostringstream message;
          message << "AST JSON reconstructed " << record.kind << " id "
                  << record.id
                  << " with a definition lacking an owning declaration";
          throw std::runtime_error(message.str());
        }
      }
    }
  }

  SgSourceFile *file = isSgSourceFile(nodeById(nodes, ast.root_id));
  ROSE_ASSERT(file != nullptr);
  restoreFileIdMapFromMetadata(ast.metadata);
  return file;
}

} // namespace AstJson
} // namespace Rose
