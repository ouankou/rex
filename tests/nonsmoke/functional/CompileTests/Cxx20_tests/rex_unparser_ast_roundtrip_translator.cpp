#include "rose.h"

#include <cstdlib>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgFloatVal *old_value = nullptr;
  bool found_two_argument_initializer_list = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr) {
      continue;
    }

    if (initialized_name->get_name() == "pair") {
      SgConstructorInitializer *constructor =
          isSgConstructorInitializer(initialized_name->get_initializer());
      ROSE_ASSERT(constructor != nullptr);
      ROSE_ASSERT(constructor->get_declaration() != nullptr);
      ROSE_ASSERT(constructor->get_declaration()->get_name() ==
                  "initializer_list");
      ROSE_ASSERT(constructor->get_args() != nullptr);
      ROSE_ASSERT(constructor->get_args()->get_expressions().size() == 2);
      SgFunctionDeclaration *initializer_list = constructor->get_declaration();
      SgFunctionDeclaration *first_initializer_list = isSgFunctionDeclaration(
          initializer_list->get_firstNondefiningDeclaration());
      SgFunctionDeclaration *defining_initializer_list =
          isSgFunctionDeclaration(initializer_list->get_definingDeclaration());
      ROSE_ASSERT(first_initializer_list != nullptr);
      ROSE_ASSERT(defining_initializer_list != nullptr);
      ROSE_ASSERT(initializer_list->get_type() ==
                  first_initializer_list->get_type());
      ROSE_ASSERT(defining_initializer_list->get_type() ==
                  first_initializer_list->get_type());
      found_two_argument_initializer_list = true;
      continue;
    }

    if (initialized_name->get_name() != "builder_value") {
      continue;
    }
    SgAssignInitializer *initializer =
        isSgAssignInitializer(initialized_name->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    old_value = isSgFloatVal(initializer->get_operand());
    ROSE_ASSERT(old_value != nullptr);
  }
  ROSE_ASSERT(found_two_argument_initializer_list);
  ROSE_ASSERT(old_value != nullptr);

  bool found_union_designator_in_call = false;
  bool found_braced_union_variable_initializer = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr ||
        initialized_name->get_name() != "choice") {
      continue;
    }
    SgConstructorInitializer *initializer =
        isSgConstructorInitializer(initialized_name->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    ROSE_ASSERT(initializer->get_is_braced_initialized());
    found_braced_union_variable_initializer = true;
  }
  ROSE_ASSERT(found_braced_union_variable_initializer);

  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgDesignatedInitializer)) {
    SgDesignatedInitializer *initializer = isSgDesignatedInitializer(node);
    if (initializer == nullptr) {
      continue;
    }
    SgExprListExp *designators = initializer->get_designatorList();
    SgInitializer *member = initializer->get_memberInit();
    ROSE_ASSERT(designators != nullptr);
    ROSE_ASSERT(!designators->get_expressions().empty());
    ROSE_ASSERT(designators->get_parent() == initializer);
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_parent() == initializer);
    for (SgExpression *entry : designators->get_expressions()) {
      ROSE_ASSERT(isSgDesignator(entry) != nullptr);
      ROSE_ASSERT(entry->get_parent() == designators);
    }
    if (SageInterface::getEnclosingNode<SgFunctionCallExp>(initializer) !=
        nullptr) {
      found_union_designator_in_call = true;
    }
  }
  ROSE_ASSERT(found_union_designator_in_call);

  bool found_exact_field_designator_payload = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgDesignator)) {
    SgDesignator *designator = isSgDesignator(node);
    if (designator == nullptr ||
        designator->get_kind() != SgDesignator::e_designator_field) {
      continue;
    }
    SgVarRefExp *field = isSgVarRefExp(designator->get_first_expression());
    ROSE_ASSERT(field != nullptr);
    ROSE_ASSERT(field->get_parent() == designator);
    ROSE_ASSERT(designator->get_startOfConstruct() != nullptr);
    const int expected_physical_file_id =
        designator->get_startOfConstruct()->get_physical_file_id();
    ROSE_ASSERT(expected_physical_file_id >= 0);
    for (Sg_File_Info *position :
         {field->get_file_info(), field->get_startOfConstruct(),
          field->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == field);
      ROSE_ASSERT(!position->isShared());
      ROSE_ASSERT(!position->isCompilerGenerated());
      ROSE_ASSERT(!position->isFrontendSpecific());
      ROSE_ASSERT(!position->isTransformation());
      ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(position->get_physical_file_id() ==
                  expected_physical_file_id);
    }
    ROSE_ASSERT(field->get_startOfConstruct()->get_physical_line() ==
                designator->get_startOfConstruct()->get_physical_line());
    ROSE_ASSERT(field->get_startOfConstruct()->get_col() >
                designator->get_startOfConstruct()->get_col());
    found_exact_field_designator_payload = true;
  }
  ROSE_ASSERT(found_exact_field_designator_payload);

  bool found_structural_catch_all = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCatchOptionStmt)) {
    SgCatchOptionStmt *catch_statement = isSgCatchOptionStmt(node);
    if (catch_statement != nullptr &&
        catch_statement->get_condition() == nullptr) {
      found_structural_catch_all = true;
    }
  }
  ROSE_ASSERT(found_structural_catch_all);

  bool found_semantic_nonreal_template_name = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgNonrealDecl)) {
    SgNonrealDecl *declaration = isSgNonrealDecl(node);
    if (declaration == nullptr || declaration->get_nonreal_template_role() !=
                                      SgNonrealDecl::e_nonreal_template_id) {
      continue;
    }
    const std::string semantic_name =
        declaration->get_semantic_name().getString();
    ROSE_ASSERT(semantic_name.find('<') != std::string::npos);
    ROSE_ASSERT(semantic_name.rfind('>') != std::string::npos);
    found_semantic_nonreal_template_name = true;
  }
  ROSE_ASSERT(found_semantic_nonreal_template_name);

  SgInitializedName *unnamed_template_parameter = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name() != "UnnamedParameter") {
      continue;
    }
    for (SgTemplateParameter *parameter :
         declaration->get_templateParameters()) {
      if (parameter == nullptr || parameter->get_parameterType() !=
                                      SgTemplateParameter::nontype_parameter) {
        continue;
      }
      SgInitializedName *initialized_name = parameter->get_initializedName();
      if (initialized_name == nullptr) {
        continue;
      }
      // The frontend must preserve the exact empty source spelling.  A
      // generated semantic identity must never leak into this source-facing
      // initialized name or require correction by the unparser.
      ROSE_ASSERT(initialized_name->get_name().is_null());
      unnamed_template_parameter = initialized_name;
      break;
    }
    if (unnamed_template_parameter != nullptr) {
      break;
    }
  }
  ROSE_ASSERT(unnamed_template_parameter != nullptr);

  bool found_member_pointer_template_argument = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr ||
        initialized_name->get_name() != "member_pointer_type_argument") {
      continue;
    }

    SgClassType *semantic_class = isSgClassType(initialized_name->get_type());
    SgClassDeclaration *semantic_declaration = isSgClassDeclaration(
        semantic_class != nullptr ? semantic_class->get_declaration()
                                  : nullptr);
    SgTemplateInstantiationDecl *semantic_instantiation =
        isSgTemplateInstantiationDecl(semantic_declaration);
    if (semantic_instantiation == nullptr && semantic_declaration != nullptr) {
      semantic_instantiation = isSgTemplateInstantiationDecl(
          semantic_declaration->get_firstNondefiningDeclaration());
    }
    ROSE_ASSERT(semantic_instantiation != nullptr);
    ROSE_ASSERT(semantic_instantiation->get_templateArguments().size() == 1);

    SgTemplateArgument *semantic_argument =
        semantic_instantiation->get_templateArguments().front();
    SgPointerMemberType *semantic_type =
        semantic_argument != nullptr
            ? isSgPointerMemberType(semantic_argument->get_type())
            : nullptr;
    ROSE_ASSERT(semantic_type != nullptr);
    ROSE_ASSERT(semantic_argument->get_sourceSpelledType() == nullptr);
    ROSE_ASSERT(SgPointerMemberType::isCanonicalSemanticType(semantic_type));
    ROSE_ASSERT(SageBuilder::buildPointerMemberType(
                    semantic_type->get_base_type(),
                    semantic_type->get_class_type()) == semantic_type);

    SgNonrealType *written_type =
        isSgNonrealType(initialized_name->get_cxx_source_type());
    SgNonrealDecl *written_declaration = isSgNonrealDecl(
        written_type != nullptr ? written_type->get_declaration() : nullptr);
    ROSE_ASSERT(written_declaration != nullptr);
    ROSE_ASSERT(written_declaration->get_tpl_args().size() == 1);
    SgTemplateArgument *written_argument =
        written_declaration->get_tpl_args().front();
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(semantic_argument,
                                                           written_argument));
    ROSE_ASSERT(written_argument->get_type() == semantic_type);

    SgPointerMemberType *source_type =
        isSgPointerMemberType(written_argument->get_sourceSpelledType());
    ROSE_ASSERT(source_type != nullptr);
    ROSE_ASSERT(source_type != semantic_type);
    ROSE_ASSERT(!SgPointerMemberType::isCanonicalSemanticType(source_type));

    const SgTemplateArgumentPtrList &deduced_arguments =
        semantic_instantiation->get_deducedTemplateArguments();
    if (!deduced_arguments.empty()) {
      ROSE_ASSERT(deduced_arguments.size() == 1);
      ROSE_ASSERT(SageInterface::templateArgumentEquivalence(
          semantic_argument, deduced_arguments.front()));
      ROSE_ASSERT(deduced_arguments.front()->get_type() == semantic_type);
    }
    found_member_pointer_template_argument = true;
  }
  ROSE_ASSERT(found_member_pointer_template_argument);

  SgType *first_member_pointer_type = nullptr;
  SgType *second_member_pointer_type = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initialized_name = isSgInitializedName(node);
    if (initialized_name == nullptr) {
      continue;
    }
    if (initialized_name->get_name() == "member_one") {
      first_member_pointer_type = initialized_name->get_type();
    } else if (initialized_name->get_name() == "member_two") {
      second_member_pointer_type = initialized_name->get_type();
    }
  }
  ROSE_ASSERT(first_member_pointer_type != nullptr);
  ROSE_ASSERT(second_member_pointer_type == first_member_pointer_type);
  ROSE_ASSERT(SgPointerMemberType::isCanonicalSemanticType(
      isSgPointerMemberType(first_member_pointer_type)));

  bool found_lambda_closure = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLambdaExp)) {
    SgLambdaExp *lambda = isSgLambdaExp(node);
    SgClassDeclaration *closure =
        lambda != nullptr ? lambda->get_lambda_closure_class() : nullptr;
    SgClassDefinition *definition =
        closure != nullptr ? closure->get_definition() : nullptr;
    ROSE_ASSERT(lambda != nullptr);
    ROSE_ASSERT(closure != nullptr);
    ROSE_ASSERT(closure->get_parent() == lambda);
    ROSE_ASSERT(lambda->get_type() == closure->get_type());
    ROSE_ASSERT(isSgClassType(lambda->get_type()) != nullptr);
    ROSE_ASSERT(definition != nullptr);
    ROSE_ASSERT(definition->get_parent() == closure);
    ROSE_ASSERT(definition->get_construction_physical_output_owner() ==
                nullptr);

    SgFunctionDeclaration *lambda_function = lambda->get_lambda_function();
    SgFunctionDefinition *lambda_definition =
        lambda_function != nullptr ? lambda_function->get_definition()
                                   : nullptr;
    ROSE_ASSERT(lambda_function != nullptr);
    for (Sg_File_Info *position : {lambda_function->get_file_info(),
                                   lambda_function->get_startOfConstruct(),
                                   lambda_function->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->isCompilerGenerated());
      ROSE_ASSERT(position->isFrontendSpecific());
    }
    SgBasicBlock *lambda_body =
        lambda_definition != nullptr ? lambda_definition->get_body() : nullptr;
    SgAuxiliaryDeclarationList *capture_declarations =
        lambda_body != nullptr ? lambda_body->get_auxiliary_declarations()
                               : nullptr;
    ROSE_ASSERT(lambda_body != nullptr);
    ROSE_ASSERT(capture_declarations != nullptr);
    ROSE_ASSERT(capture_declarations->get_parent() == lambda_body);
    size_t captured_declaration_count = 0;
    SgInitializedName *captured_name = nullptr;
    for (SgDeclarationStatement *declaration :
         capture_declarations->get_declarations()) {
      SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
      if (variable == nullptr || variable->get_variables().size() != 1 ||
          variable->get_variables().front()->get_name() != "captured") {
        continue;
      }
      ROSE_ASSERT(variable->get_parent() == capture_declarations);
      ROSE_ASSERT(variable->get_scope() == lambda_body);
      ROSE_ASSERT(variable->get_variables().front()->get_scope() ==
                  lambda_body);
      ROSE_ASSERT(!lambda_body->statementExistsInScope(variable));
      captured_name = variable->get_variables().front();
      ++captured_declaration_count;
    }
    ROSE_ASSERT(captured_declaration_count == 1);
    ROSE_ASSERT(captured_name != nullptr);

    SgLambdaCaptureList *capture_list = lambda->get_lambda_capture_list();
    ROSE_ASSERT(capture_list != nullptr);
    ROSE_ASSERT(capture_list->get_parent() == lambda);
    ROSE_ASSERT(capture_list->get_capture_list().size() == 1);
    ROSE_ASSERT(capture_list->get_startOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture_list->get_startOfConstruct()->get_col() == 29);
    ROSE_ASSERT(capture_list->get_endOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture_list->get_endOfConstruct()->get_col() == 43);

    SgLambdaCapture *capture = capture_list->get_capture_list().front();
    ROSE_ASSERT(capture != nullptr);
    ROSE_ASSERT(capture->get_parent() == capture_list);
    ROSE_ASSERT(!capture->get_capture_by_reference());
    ROSE_ASSERT(!capture->get_implicit());
    ROSE_ASSERT(!capture->get_pack_expansion());
    ROSE_ASSERT(capture->get_closure_variable() == nullptr);
    ROSE_ASSERT(capture->get_startOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture->get_startOfConstruct()->get_col() == 30);
    ROSE_ASSERT(capture->get_endOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture->get_endOfConstruct()->get_col() == 42);

    SgVarRefExp *capture_reference =
        isSgVarRefExp(capture->get_capture_variable());
    ROSE_ASSERT(capture_reference != nullptr);
    ROSE_ASSERT(capture_reference->get_parent() == capture);
    ROSE_ASSERT(capture_reference->get_symbol() != nullptr);
    ROSE_ASSERT(capture_reference->get_symbol()->get_declaration() ==
                captured_name);
    ROSE_ASSERT(lambda_body->find_symbol_from_declaration(captured_name) ==
                capture_reference->get_symbol());
    ROSE_ASSERT(capture_reference->get_startOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture_reference->get_startOfConstruct()->get_col() == 30);
    ROSE_ASSERT(capture_reference->get_endOfConstruct()->get_line() == 119);
    ROSE_ASSERT(capture_reference->get_endOfConstruct()->get_col() == 37);

    SgIntVal *capture_initializer =
        isSgIntVal(capture->get_source_closure_variable());
    ROSE_ASSERT(capture_initializer != nullptr);
    ROSE_ASSERT(capture_initializer->get_parent() == capture);
    ROSE_ASSERT(capture_initializer->get_value() == 11);
    ROSE_ASSERT(capture_initializer->get_valueString() == "11");
    ROSE_ASSERT(capture_initializer->get_literal_spelling_form() ==
                SgValueExp::e_literal_source_spelled);

    auto require_exact_source = [](SgLocatedNode *located, int start_line,
                                   int start_column, int end_line,
                                   int end_column) {
      ROSE_ASSERT(located != nullptr);
      Sg_File_Info *file_info = located->get_file_info();
      Sg_File_Info *start = located->get_startOfConstruct();
      Sg_File_Info *end = located->get_endOfConstruct();
      for (Sg_File_Info *position : {file_info, start, end}) {
        ROSE_ASSERT(position != nullptr);
        ROSE_ASSERT(position->get_parent() == located);
        ROSE_ASSERT(!position->isCompilerGenerated());
        ROSE_ASSERT(!position->isFrontendSpecific());
        ROSE_ASSERT(!position->isTransformation());
        ROSE_ASSERT(position->isOutputInCodeGeneration());
        ROSE_ASSERT(position->get_physical_file_id() >= 0);
        ROSE_ASSERT(position->get_filenameString().find(
                        "rex_unparser_semantic_roundtrip.cpp") !=
                    std::string::npos);
      }
      ROSE_ASSERT(start->get_line() == start_line);
      ROSE_ASSERT(start->get_col() == start_column);
      ROSE_ASSERT(end->get_line() == end_line);
      ROSE_ASSERT(end->get_col() == end_column);
      ROSE_ASSERT(file_info->get_line() == start_line);
      ROSE_ASSERT(file_info->get_col() == start_column);
      ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
      ROSE_ASSERT(file_info->get_physical_file_id() ==
                  start->get_physical_file_id());
    };
    require_exact_source(capture_list, 119, 29, 119, 43);
    require_exact_source(capture, 119, 30, 119, 42);
    require_exact_source(capture_reference, 119, 30, 119, 37);
    require_exact_source(capture_initializer, 119, 41, 119, 42);
    found_lambda_closure = true;
  }
  ROSE_ASSERT(found_lambda_closure);

  bool found_inline_owned_declaration = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == "InlineOwned" &&
        !declaration->get_isAutonomousDeclaration()) {
      ROSE_ASSERT(declaration->getAttribute(
                      "rose:inline_type_definition_emitted") == nullptr);
      found_inline_owned_declaration = true;
    }
  }
  ROSE_ASSERT(found_inline_owned_declaration);

  bool found_alias_base_source_type = false;
  bool found_dependent_base_source_type = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDefinition)) {
    SgClassDefinition *definition = isSgClassDefinition(node);
    ROSE_ASSERT(definition != nullptr);
    SgClassDeclaration *declaration = definition->get_declaration();
    if (declaration == nullptr) {
      continue;
    }
    if (declaration->get_name() == "Derived") {
      ROSE_ASSERT(definition->get_inheritances().size() == 1);
      SgBaseClass *base = definition->get_inheritances().front();
      ROSE_ASSERT(base != nullptr);
      SgTypedefType *source_type = isSgTypedefType(base->get_source_type());
      ROSE_ASSERT(source_type != nullptr);
      ROSE_ASSERT(source_type->get_name() == "Alias");
      ROSE_ASSERT(base->get_base_class() != nullptr);
      ROSE_ASSERT(base->get_base_class()->get_name() == "Base");
      found_alias_base_source_type = true;
    } else if (declaration->get_name() == "Dependent") {
      ROSE_ASSERT(definition->get_inheritances().size() == 1);
      SgBaseClass *base = definition->get_inheritances().front();
      ROSE_ASSERT(base != nullptr);
      SgTemplateType *source_type = isSgTemplateType(base->get_source_type());
      ROSE_ASSERT(source_type != nullptr);
      ROSE_ASSERT(source_type->get_name() == "T");
      ROSE_ASSERT(isSgNonrealBaseClass(base) != nullptr);
      found_dependent_base_source_type = true;
    }
  }
  ROSE_ASSERT(found_alias_base_source_type);
  ROSE_ASSERT(found_dependent_base_source_type);

  SgFloatVal *builder_value = SageBuilder::buildFloatVal(6.5f);
  ROSE_ASSERT(builder_value != nullptr);
  ROSE_ASSERT(builder_value->get_valueString().empty());
  SageInterface::replaceExpression(old_value, builder_value, false);

  const bool corrupt_operator_metadata =
      std::getenv("REX_TEST_MISSING_OPERATOR_METADATA") != nullptr;
  bool found_operator_metadata = false;
  auto clear_operator_metadata = [](SgFunctionDeclaration *declaration) {
    if (declaration != nullptr) {
      declaration->get_specialFunctionModifier().unsetOperator();
    }
  };
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    SgMemberFunctionDeclaration *declaration =
        isSgMemberFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == "operator*") {
      SgFunctionDeclaration *defining =
          isSgFunctionDeclaration(declaration->get_definingDeclaration());
      SgFunctionDeclaration *first = isSgFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration());
      if (corrupt_operator_metadata) {
        clear_operator_metadata(declaration);
        clear_operator_metadata(defining);
        clear_operator_metadata(first);
      } else {
        ROSE_ASSERT(declaration->get_specialFunctionModifier().isOperator());
        ROSE_ASSERT(defining != nullptr);
        ROSE_ASSERT(defining->get_specialFunctionModifier().isOperator());
        ROSE_ASSERT(first != nullptr);
        ROSE_ASSERT(first->get_specialFunctionModifier().isOperator());
      }
      found_operator_metadata = true;
    }
  }
  ROSE_ASSERT(found_operator_metadata);

  if (corrupt_operator_metadata) {
    bool found_operator_call = false;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
      SgFunctionCallExp *call = isSgFunctionCallExp(node);
      SgFunctionDeclaration *declaration =
          call != nullptr ? call->getAssociatedFunctionDeclaration() : nullptr;
      if (call != nullptr && call->get_uses_operator_syntax() &&
          declaration != nullptr && declaration->get_name() == "operator*") {
        found_operator_call = true;
        (void)call->unparseToString();
        ROSE_ABORT();
      }
    }
    ROSE_ASSERT(found_operator_call);
  }

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source_file != nullptr);
  const std::string preview = globalUnparseToString(source_file);
  ROSE_ASSERT(!preview.empty());
  ROSE_ASSERT(unnamed_template_parameter->get_name().getString().empty());
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    ROSE_ASSERT(declaration == nullptr ||
                declaration->getAttribute(
                    "rose:inline_type_definition_emitted") == nullptr);
  }

  return backend(project);
}
