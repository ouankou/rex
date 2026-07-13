#include "rose.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

static_assert(!std::is_default_constructible_v<SgInitializedName>);
static_assert(!std::is_copy_constructible_v<SgInitializedName>);
static_assert(!std::is_copy_assignable_v<SgInitializedName>);
static_assert(!std::is_constructible_v<SgInitializedName, SgName, SgType *,
                                       SgInitializer *>);
static_assert(
    !std::is_constructible_v<
        SgInitializedName, Sg_File_Info *, SgName, SgType *, SgInitializer *,
        SgDeclarationStatement *, SgScopeStatement *, SgInitializedName *>);

namespace {

constexpr const char *kSourceFilename = "rex_namespace_builder_order.cpp";

void assertCompleteInitializedNameDefaults() {
  SgInitializedName *name = SageBuilder::buildInitializedName_nfi(
      SgName("rex_schema_defaults"), SageBuilder::buildIntType(), nullptr);
  ROSE_ASSERT(name != nullptr);
  ROSE_ASSERT(name->get_fortran_source_type() == nullptr);
  ROSE_ASSERT(name->get_cray_pointer_pointee() == nullptr);
  ROSE_ASSERT(!name->get_is_predefined_identifier());
  ROSE_ASSERT(name->get_generated_variable_role() ==
              SgInitializedName::e_generated_variable_none);
  ROSE_ASSERT(name->get_enum_constant_source_ownership() ==
              SgInitializedName::e_enum_constant_source_unclassified);
  ROSE_ASSERT(name->get_fortran_type_spec() ==
              SgInitializedName::e_fortran_type_spec_default);
  ROSE_ASSERT(name->get_fortran_procedure_interface().is_null());
  ROSE_ASSERT(name->get_fortran_separate_shape_declaration() == nullptr);
  ROSE_ASSERT(name->get_fortran_separate_pointer_declaration() == nullptr);
  ROSE_ASSERT(!name->get_source_type_qualification_present());
  ROSE_ASSERT(!name->get_source_type_global_qualification());
  ROSE_ASSERT(name->get_source_type_qualification_tokens().empty());
  ROSE_ASSERT(!name->get_source_name_qualification_present());
  ROSE_ASSERT(!name->get_source_name_global_qualification());
  ROSE_ASSERT(name->get_source_name_qualification_tokens().empty());
}

SgNamespaceSourceFragment *makeNamespaceFragment(
    SgNamespaceSourceFragment::namespace_source_fragment_kind_enum kind,
    int line, int startColumn, int endColumn) {
  SgNamespaceSourceFragment *fragment = new SgNamespaceSourceFragment(
      kind,
      SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
  Sg_File_Info *start = new Sg_File_Info(kSourceFilename, line, startColumn);
  Sg_File_Info *end = new Sg_File_Info(kSourceFilename, line, endColumn);
  fragment->set_startOfConstruct(start);
  fragment->set_endOfConstruct(end);
  start->set_parent(fragment);
  end->set_parent(fragment);
  return fragment;
}

void publishNamespaceTokenOccurrences(SgNamespaceSourceFragment *opening,
                                      SgNamespaceSourceFragment *closing,
                                      unsigned int sourceOrder) {
  ROSE_ASSERT(opening != nullptr);
  ROSE_ASSERT(closing != nullptr);
  ROSE_ASSERT(sourceOrder <= std::numeric_limits<unsigned int>::max() - 2);
  opening->get_startOfConstruct()->set_source_sequence_number(sourceOrder);
  opening->get_endOfConstruct()->set_source_sequence_number(sourceOrder + 1);
  closing->get_startOfConstruct()->set_source_sequence_number(sourceOrder + 2);
  closing->get_endOfConstruct()->set_source_sequence_number(sourceOrder + 2);
}

SgNamespaceDeclarationStatement *buildSourceNamespace(const SgName &name,
                                                      SgGlobal *global,
                                                      unsigned int sourceOrder,
                                                      int line) {
  SgNamespaceSourceFragment *opening = makeNamespaceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_opening, line, 1,
      12);
  SgNamespaceSourceFragment *closing = makeNamespaceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_closing, line, 20,
      20);
  publishNamespaceTokenOccurrences(opening, closing, sourceOrder);
  return SageBuilder::buildNamespaceDeclaration_nfi(
      name, false, global, SageBuilder::e_namespace_declaration_source_lexical,
      nullptr, opening, closing, sourceOrder);
}

SgTemplateParameterPtrList buildTemplateTypeParameters(const SgName &name) {
  SgTemplateType *parameterType = SageBuilder::buildTemplateType(name);
  ROSE_ASSERT(parameterType != nullptr);
  return {SageBuilder::buildTemplateParameter(
      SgTemplateParameter::type_parameter, parameterType,
      SgTemplateParameter::keyword_class)};
}

void assertExactSemanticProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_file_info() == node->get_startOfConstruct());
  ROSE_ASSERT(node->get_startOfConstruct() != node->get_endOfConstruct());
  for (Sg_File_Info *position :
       {node->get_startOfConstruct(), node->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  }
}

void assertNullSourceProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_file_info() == nullptr);
  ROSE_ASSERT(node->get_startOfConstruct() == nullptr);
  ROSE_ASSERT(node->get_endOfConstruct() == nullptr);
}

void installExactPhysicalSourceProvenance(SgLocatedNode *node, int line) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(line > 0);
  assertNullSourceProvenance(node);
  Sg_File_Info *start = new Sg_File_Info(kSourceFilename, line, 1);
  Sg_File_Info *end = new Sg_File_Info(kSourceFilename, line, 20);
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  start->set_parent(node);
  end->set_parent(node);
  if (isSgStatement(node) != nullptr) {
    start->setOutputInCodeGeneration();
    end->setOutputInCodeGeneration();
  }
  ROSE_ASSERT(node->get_file_info() == start);
  ROSE_ASSERT(start->get_physical_file_id() >= 0);
  ROSE_ASSERT(end->get_physical_file_id() == start->get_physical_file_id());
}

void assertDetachedTransformationProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_parent() == nullptr ||
              isSgDeclarationGroupStatement(node->get_parent()) != nullptr);
  for (Sg_File_Info *position :
       {node->get_startOfConstruct(), node->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(position->isTransformation());
  }
}

void assertExactPublishedProvenance(SgLocatedNode *node, SgLocatedNode *owner) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(owner != nullptr);
  Sg_File_Info *ownerPosition = owner->get_file_info();
  ROSE_ASSERT(ownerPosition != nullptr);
  for (Sg_File_Info *position :
       {node->get_startOfConstruct(), node->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == node);
    // Physical publication is orthogonal to semantic origin.  Every caller of
    // this helper builds a transformation surface, so publication must retain
    // that exact origin while adding only the concrete output owner.
    ROSE_ASSERT(position->isTransformation());
    ROSE_ASSERT(!position->isCompilerGenerated());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_physical_file_id() ==
                ownerPosition->get_physical_file_id());
    ROSE_ASSERT(position->get_physical_filename() ==
                ownerPosition->get_physical_filename());
  }
}

} // namespace

int main(int argc, char **argv) {
  assertCompleteInitializedNameDefaults();
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "--check-binary-nfi-source-provenance") {
    SgMinusOp *negated = SageBuilder::buildMinusOp_nfi(
        SageBuilder::buildIntVal_nfi(3), SageBuilder::buildIntType());
    assertNullSourceProvenance(negated);
    ROSE_ASSERT(negated->get_operatorPosition() == nullptr);

    SgIntVal *lhs = SageBuilder::buildIntVal_nfi(1);
    SgIntVal *rhs = SageBuilder::buildIntVal_nfi(2);
    SgAddOp *sum =
        SageBuilder::buildAddOp_nfi(lhs, rhs, SageBuilder::buildIntType());
    ROSE_ASSERT(sum != nullptr);
    ROSE_ASSERT(sum->get_lhs_operand_i() == lhs);
    ROSE_ASSERT(sum->get_rhs_operand_i() == rhs);
    ROSE_ASSERT(lhs->get_parent() == sum);
    ROSE_ASSERT(rhs->get_parent() == sum);
    assertNullSourceProvenance(sum);
    ROSE_ASSERT(sum->get_operatorPosition() == nullptr);
    return 0;
  }
  if (mode == "--reject-preclassified-nfi-source") {
    SgNullExpression *expression = new SgNullExpression();
    ROSE_ASSERT(expression != nullptr);
    SageInterface::setOneSourcePositionForTransformation(expression);
    SageBuilder::requireFreshNfiExpressionSourceState(
        expression, "preclassified NFI death fixture");
    return 1;
  }
  if (mode == "--reject-frontend-semantic-physical-hybrid") {
    SgNullStatement *statement = new SgNullStatement();
    ROSE_ASSERT(statement != nullptr);
    auto makeHybridPosition = [statement]() {
      Sg_File_Info *position =
          new Sg_File_Info("rex_frontend_semantic_physical_hybrid.cpp", 1, 1);
      position->setCompilerGenerated();
      position->setFrontendSpecific();
      position->setOutputInCodeGeneration();
      position->set_parent(statement);
      return position;
    };
    statement->set_startOfConstruct(makeHybridPosition());
    statement->set_endOfConstruct(makeHybridPosition());
    checkPhysicalSourcePosition(statement);
    return 1;
  }
  if (mode == "--reject-null-function-parameter-list") {
    SageBuilder::buildFunctionParameterTypeList(
        static_cast<SgFunctionParameterList *>(nullptr));
    return 1;
  }
  if (mode == "--reject-null-nfi-function-parameter-type-list") {
    SageBuilder::buildFunctionParameterList_nfi(
        static_cast<SgFunctionParameterTypeList *>(nullptr));
    return 1;
  }
  if (mode == "--reject-missing-template-identity") {
    SgTemplateArgumentPtrList arguments;
    const SgName instantiationName("rex_box<>");
    SageBuilder::buildNondefiningClassDeclaration_nfi(
        SageBuilder::declaration_ownership::sourceLexical(), "rex_box",
        SgClassDeclaration::e_class, nullptr, nullptr, &arguments,
        &instantiationName);
    return 1;
  }
  if (mode == "--reject-missing-template-scope") {
    SgTemplateClassDeclaration *primary = new SgTemplateClassDeclaration(
        "rex_box", SgClassDeclaration::e_class, nullptr, nullptr);
    SgTemplateArgumentPtrList arguments;
    const SgName instantiationName("rex_box<>");
    SageBuilder::buildNondefiningClassDeclaration_nfi(
        SageBuilder::declaration_ownership::sourceLexical(), "rex_box",
        SgClassDeclaration::e_class, nullptr, primary, &arguments,
        &instantiationName);
    return 1;
  }
  if (mode == "--reject-null-function-source-group") {
    SageBuilder::function_declaration_ownership::sourceGroupMember(nullptr);
    return 1;
  }
  if (mode == "--reject-null-typedef-source-group") {
    SageBuilder::typedef_declaration_ownership::sourceGroupMember(nullptr);
    return 1;
  }
  if (mode == "--reject-null-source-lexical-owner") {
    SageBuilder::declaration_ownership::sourceLexicalIn(nullptr);
    return 1;
  }
  if (mode == "--reject-null-variable-scope") {
    SageBuilder::buildVariableDeclaration_nfi(
        "rex_missing_scope", SageBuilder::buildIntType(), nullptr, nullptr);
    return 1;
  }

  // Namespace construction publishes symbols through both the translation
  // unit global scope and the project's cross-file global scope.  A bare
  // SgGlobal has neither an owning translation unit nor a project and is not a
  // valid symbol-publication fixture.
  SgProject *project = new SgProject();
  ROSE_ASSERT(project != nullptr);
  project->get_fileList().clear();
  project->set_compileOnly(true);
  project->get_originalCommandLineArgumentList() = {"c++", "-c"};
  SgSourceFile *source =
      SageBuilder::buildGeneratedSourceFile(kSourceFilename, project);
  ROSE_ASSERT(source != nullptr);
  SgGlobal *global = source->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  ROSE_ASSERT(SageInterface::getProject(global) == project);

  if (mode == "--reject-standalone-parameter-list-copy") {
    SageInterface::deepCopyNode(SageBuilder::buildFunctionParameterList());
    return 1;
  }

  if (mode == "--check-generated-prototype-parameter-ownership") {
    SgFunctionParameterList *sourceParameters =
        SageBuilder::buildFunctionParameterList();
    SgInitializedName *sourceParameter = SageBuilder::buildInitializedName(
        SgName("rex_value"), SageBuilder::buildIntType());
    sourceParameter->set_is_parameter_pack(true);
    sourceParameter->set_needs_definitions(true);
    sourceParameter->set_source_type_qualification_present(true);
    sourceParameter->set_source_type_global_qualification(true);
    sourceParameter->get_source_type_qualification_tokens() = {"rex", "type"};
    SageInterface::appendArg(sourceParameters, sourceParameter);
    SgFunctionDeclaration *sourceFunction =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_generated_prototype_source"),
            SageBuilder::buildVoidType(), sourceParameters, global);
    ROSE_ASSERT(sourceFunction != nullptr);
    ROSE_ASSERT(sourceParameter->get_declptr() == sourceFunction);

    SgNamespaceDefinitionStatement *targetScope =
        SageBuilder::buildNamespaceDeclaration_nfi(
            "rex_generated_prototype_target", false, global,
            SageBuilder::e_namespace_declaration_canonical_generated_lexical,
            nullptr, nullptr, nullptr, std::nullopt)
            ->get_definition();
    ROSE_ASSERT(targetScope != nullptr);
    SgFunctionDeclaration *prototype =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            sourceFunction, targetScope);
    ROSE_ASSERT(prototype != nullptr);
    ROSE_ASSERT(prototype != sourceFunction);
    ROSE_ASSERT(prototype->get_parent() == targetScope);
    ROSE_ASSERT(prototype->get_scope() == targetScope);

    SgFunctionParameterList *copiedParameters = prototype->get_parameterList();
    ROSE_ASSERT(copiedParameters != nullptr);
    ROSE_ASSERT(copiedParameters != sourceParameters);
    ROSE_ASSERT(copiedParameters->get_parent() == prototype);
    ROSE_ASSERT(copiedParameters->get_args().size() == 1);
    SgInitializedName *copiedParameter = copiedParameters->get_args().front();
    ROSE_ASSERT(copiedParameter != nullptr);
    ROSE_ASSERT(copiedParameter != sourceParameter);
    ROSE_ASSERT(copiedParameter->get_parent() == copiedParameters);
    ROSE_ASSERT(copiedParameter->get_declptr() == prototype);
    ROSE_ASSERT(copiedParameter->get_scope() ==
                prototype->get_functionParameterScope());
    ROSE_ASSERT(copiedParameter->get_initializer() == nullptr);
    ROSE_ASSERT(copiedParameter->get_name() == sourceParameter->get_name());
    ROSE_ASSERT(copiedParameter->get_type() == sourceParameter->get_type());
    ROSE_ASSERT(copiedParameter->get_is_parameter_pack());
    ROSE_ASSERT(copiedParameter->get_needs_definitions());
    ROSE_ASSERT(copiedParameter->get_source_type_qualification_present());
    ROSE_ASSERT(copiedParameter->get_source_type_global_qualification());
    ROSE_ASSERT(copiedParameter->get_source_type_qualification_tokens() ==
                sourceParameter->get_source_type_qualification_tokens());
    for (SgLocatedNode *node :
         {static_cast<SgLocatedNode *>(prototype),
          static_cast<SgLocatedNode *>(copiedParameters),
          static_cast<SgLocatedNode *>(copiedParameter)}) {
      for (Sg_File_Info *position :
           {node->get_startOfConstruct(), node->get_endOfConstruct()}) {
        ROSE_ASSERT(position != nullptr);
        ROSE_ASSERT(position->get_parent() == node);
        ROSE_ASSERT(position->isTransformation());
      }
    }
    return 0;
  }

  if (mode == "--check-pending-enclosing-tag-reference") {
    SgClassDeclaration *pending = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
        "rex_pending_enclosing_tag", global);
    ROSE_ASSERT(pending != nullptr);
    SgClassDefinition *memberScope = pending->get_definition();
    ROSE_ASSERT(memberScope != nullptr);
    ROSE_ASSERT(pending->get_parent() == nullptr);
    ROSE_ASSERT(pending->get_scope() == global);
    ROSE_ASSERT(memberScope->get_parent() == pending);

    SgVariableDeclaration *member = SageBuilder::buildVariableDeclaration_nfi(
        "rex_pending_enclosing_member", pending->get_type(), nullptr,
        memberScope);
    SgTypedefDeclaration *alias = SageBuilder::buildTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        SgTypedefDeclaration::e_typedef, "rex_pending_enclosing_alias",
        pending->get_type(), memberScope);
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(alias != nullptr);
    ROSE_ASSERT(member->get_baseTypeDefiningDeclaration() == nullptr);
    ROSE_ASSERT(alias->get_declaration() != pending);
    ROSE_ASSERT(!alias->get_typedefBaseTypeContainsDefiningDeclaration());
    ROSE_ASSERT(alias->get_parent() == memberScope);
    ROSE_ASSERT(alias->get_scope() == memberScope);
    ROSE_ASSERT(pending->get_parent() == nullptr);
    return 0;
  }

  if (mode == "--check-reopened-namespace-function-ownership") {
    SgNamespaceDefinitionStatement *canonicalNamespace =
        buildSourceNamespace("rex_function_namespace", global, 10, 10)
            ->get_definition();
    SgNamespaceDefinitionStatement *reopenedNamespace =
        buildSourceNamespace("rex_function_namespace", global, 20, 20)
            ->get_definition();

    // Reopened namespaces split lexical source ownership from semantic symbol
    // ownership. Every function redeclaration keeps the canonical namespace as
    // its scope while its parent and child-list edge identify the exact source
    // fragment that contains its syntax.
    const SgName functionName("rex_reopened_function");
    SgFunctionDeclaration *canonicalFunction =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexicalIn(
                canonicalNamespace),
            functionName, SageBuilder::buildIntType(),
            SageBuilder::buildFunctionParameterList(), canonicalNamespace);
    ROSE_ASSERT(canonicalFunction != nullptr);
    SgFunctionDeclaration *reopenedFunction =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::
                sourceLexicalRedeclarationIn(reopenedNamespace,
                                             canonicalFunction),
            functionName, SageBuilder::buildIntType(),
            SageBuilder::buildFunctionParameterList(), canonicalNamespace);
    ROSE_ASSERT(reopenedFunction != nullptr);
    SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(
        canonicalNamespace->find_symbol_from_declaration(canonicalFunction));
    ROSE_ASSERT(functionSymbol != nullptr);
    ROSE_ASSERT(canonicalFunction->get_parent() == canonicalNamespace);
    ROSE_ASSERT(reopenedFunction->get_parent() == reopenedNamespace);
    ROSE_ASSERT(canonicalFunction->get_scope() == canonicalNamespace);
    ROSE_ASSERT(reopenedFunction->get_scope() == canonicalNamespace);
    ROSE_ASSERT(canonicalFunction->get_firstNondefiningDeclaration() ==
                canonicalFunction);
    ROSE_ASSERT(reopenedFunction->get_firstNondefiningDeclaration() ==
                canonicalFunction);
    ROSE_ASSERT(reopenedFunction->get_type() == canonicalFunction->get_type());
    ROSE_ASSERT(functionSymbol->get_declaration() == canonicalFunction);
    ROSE_ASSERT(functionSymbol->get_parent() ==
                canonicalNamespace->get_symbol_table());
    ROSE_ASSERT(std::count(canonicalNamespace->get_declarations().begin(),
                           canonicalNamespace->get_declarations().end(),
                           canonicalFunction) == 1);
    ROSE_ASSERT(std::count(reopenedNamespace->get_declarations().begin(),
                           reopenedNamespace->get_declarations().end(),
                           reopenedFunction) == 1);
    ROSE_ASSERT(!reopenedNamespace->get_symbol_table()->exists(functionSymbol));
    return 0;
  }

  auto buildPendingCanonicalProcedure = [&]() {
    SgFunctionParameterList *parameters =
        SageBuilder::buildFunctionParameterList_nfi();
    SgInitializedName *argument = SageBuilder::buildInitializedName_nfi(
        "argument", SageBuilder::buildIntType(), nullptr);
    SageInterface::appendArg(parameters, argument);
    return SageBuilder::buildNondefiningProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::
            semanticAuxiliaryPendingExactSource(),
        SgName("rex_pending_exact_canonical"), SageBuilder::buildVoidType(),
        parameters, SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::
            e_fortran_procedure_source_form_semantic_only,
        global);
  };
  if (mode == "--check-pending-exact-semantic-function-source-boundary") {
    const SageBuilder::SourcePositionClassification savedMode =
        SageBuilder::getSourcePositionClassificationMode();
    SageBuilder::setSourcePositionClassificationMode(
        SageBuilder::e_sourcePositionFrontendConstruction);
    SgProcedureHeaderStatement *canonical = buildPendingCanonicalProcedure();
    SageBuilder::setSourcePositionClassificationMode(savedMode);
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(canonical->get_parent() ==
                global->get_auxiliary_declarations());
    assertNullSourceProvenance(canonical);
    SgFunctionParameterList *parameters = canonical->get_parameterList();
    ROSE_ASSERT(parameters != nullptr);
    ROSE_ASSERT(parameters->get_parent() == canonical);
    ROSE_ASSERT(parameters->get_args().size() == 1);
    assertNullSourceProvenance(parameters);
    assertNullSourceProvenance(parameters->get_args().front());
    return 0;
  }
  if (mode == "--reject-pending-exact-semantic-function-outside-frontend") {
    buildPendingCanonicalProcedure();
    return 1;
  }

  if (mode == "--reject-pending-exact-source-without-provenance") {
    SgModuleStatement *module = SageBuilder::buildModuleStatement(
        SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
        "rex_missing_pending_exact_source", global);
    ROSE_ASSERT(module != nullptr);
    ROSE_ASSERT(module->get_parent() == nullptr);
    SageBuilder::publishExactSourceLexicalDeclaration(module, global);
    return 1;
  }
  if (mode == "--reject-duplicate-pending-exact-source-publication") {
    SgModuleStatement *module = SageBuilder::buildModuleStatement(
        SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
        "rex_duplicate_pending_exact_source", global);
    ROSE_ASSERT(module != nullptr);
    SgClassDefinition *definition = module->get_definition();
    ROSE_ASSERT(definition != nullptr);
    installExactPhysicalSourceProvenance(module, 40);
    installExactPhysicalSourceProvenance(definition, 40);
    SageBuilder::publishExactSourceLexicalDeclaration(module, global);
    SageBuilder::publishExactSourceLexicalDeclaration(module, global);
    return 1;
  }

  if (mode == "--check-direct-nfi-source-provenance") {
    SgSubscriptExpression *subscript =
        SageBuilder::buildSubscriptExpression_nfi(
            SageBuilder::buildIntVal_nfi(1), SageBuilder::buildIntVal_nfi(4),
            SageBuilder::buildIntVal_nfi(1));
    assertNullSourceProvenance(subscript);
    ROSE_ASSERT(subscript->get_operatorPosition() == nullptr);

    SgAssignInitializer *initializer = SageBuilder::buildAssignInitializer_nfi(
        SageBuilder::buildIntVal_nfi(7), SageBuilder::buildIntType());
    assertNullSourceProvenance(initializer);
    ROSE_ASSERT(initializer->get_operatorPosition() == nullptr);

    SgLabelStatement *label = SageBuilder::buildLabelStatement_nfi(
        "rex_label", SageBuilder::buildNullStatement_nfi(), nullptr);
    assertNullSourceProvenance(label);

    SgStatementPtrList initializers{
        SageBuilder::buildExprStatement_nfi(SageBuilder::buildIntVal_nfi(0))};
    SgForInitStatement *forInit =
        SageBuilder::buildForInitStatement_nfi(initializers);
    assertNullSourceProvenance(forInit);

    SgTemplateVariableDeclaration *variable =
        SageBuilder::buildTemplateVariableDeclaration_nfi(
            "rex_template_variable", SageBuilder::buildIntType(), nullptr,
            global,
            SageBuilder::template_variable_entity_kind::primary_template,
            nullptr);
    assertNullSourceProvenance(variable);
    SgInitializedName *name = variable->get_variables().front();
    assertNullSourceProvenance(name);
    assertNullSourceProvenance(name->get_declptr());

    SgExprListExp *arguments = SageBuilder::buildExprListExp_nfi(
        std::vector<SgExpression *>{SageBuilder::buildIntVal_nfi(11)});
    SgFunctionCallExp *namedCall = SageBuilder::buildFunctionCallExp_nfi(
        SgName("rex_named_nfi_call"), SageBuilder::buildIntType(), arguments,
        global);
    ROSE_ASSERT(namedCall != nullptr);
    assertNullSourceProvenance(namedCall);
    ROSE_ASSERT(namedCall->get_operatorPosition() == nullptr);
    SgFunctionRefExp *namedReference =
        isSgFunctionRefExp(namedCall->get_function());
    ROSE_ASSERT(namedReference != nullptr);
    ROSE_ASSERT(namedReference->get_parent() == namedCall);
    assertNullSourceProvenance(namedReference);
    ROSE_ASSERT(namedReference->get_operatorPosition() == nullptr);
    SgFunctionDeclaration *namedDeclaration =
        namedReference->get_symbol()->get_declaration();
    ROSE_ASSERT(namedDeclaration != nullptr);
    ROSE_ASSERT(namedDeclaration->get_parent() ==
                global->get_auxiliary_declarations());
    assertExactSemanticProvenance(namedDeclaration);
    SgFunctionParameterList *namedParameters =
        namedDeclaration->get_parameterList();
    ROSE_ASSERT(namedParameters != nullptr);
    ROSE_ASSERT(namedParameters->get_parent() == namedDeclaration);
    ROSE_ASSERT(namedParameters->get_args().size() == 1);
    assertExactSemanticProvenance(namedParameters);
    assertExactSemanticProvenance(namedParameters->get_args().front());

    SgModuleStatement *module = SageBuilder::buildModuleStatement(
        SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
        "rex_pending_exact_module", global);
    ROSE_ASSERT(module != nullptr);
    ROSE_ASSERT(module->get_parent() == nullptr);
    ROSE_ASSERT(module->get_scope() == global);
    assertNullSourceProvenance(module);
    SgClassDefinition *moduleDefinition = module->get_definition();
    ROSE_ASSERT(moduleDefinition != nullptr);
    ROSE_ASSERT(moduleDefinition->get_parent() == module);
    assertNullSourceProvenance(moduleDefinition);
    ROSE_ASSERT(std::count(global->get_declarations().begin(),
                           global->get_declarations().end(), module) == 0);
    installExactPhysicalSourceProvenance(module, 30);
    installExactPhysicalSourceProvenance(moduleDefinition, 30);
    SageBuilder::publishExactSourceLexicalDeclaration(module, global);
    ROSE_ASSERT(module->get_parent() == global);
    ROSE_ASSERT(std::count(global->get_declarations().begin(),
                           global->get_declarations().end(), module) == 1);
    return 0;
  }

  if (mode == "--reject-template-unrelated-namespace-scope") {
    SgNamespaceDefinitionStatement *canonical =
        buildSourceNamespace("rex_template_semantic", global, 10, 10)
            ->get_definition();
    SgNamespaceDefinitionStatement *unrelated =
        buildSourceNamespace("rex_template_lexical", global, 20, 20)
            ->get_definition();
    SgTemplateParameterPtrList parameters =
        buildTemplateTypeParameters("FragmentParameter");
    SgTemplateArgumentPtrList arguments;
    const SgName name("rex_fragment_template");
    SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
        SageBuilder::declaration_ownership::sourceLexicalIn(unrelated), name,
        SgClassDeclaration::e_class,
        SageBuilder::template_class_declaration_scopes::fromExactSemanticScope(
            canonical),
        &parameters, &arguments, &name);
    return 1;
  }
  if (mode == "--reject-template-definition-unrelated-scope") {
    SgNamespaceDefinitionStatement *firstScope =
        buildSourceNamespace("rex_template_first", global, 10, 10)
            ->get_definition();
    SgNamespaceDefinitionStatement *unrelatedScope =
        buildSourceNamespace("rex_template_unrelated", global, 20, 20)
            ->get_definition();
    SgTemplateParameterPtrList firstParameters =
        buildTemplateTypeParameters("FirstParameter");
    SgTemplateParameterPtrList definingParameters =
        buildTemplateTypeParameters("DefiningParameter");
    SgTemplateArgumentPtrList arguments;
    const SgName name("rex_unrelated_template");
    SgTemplateClassDeclaration *canonical =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            SageBuilder::declaration_ownership::sourceLexicalIn(firstScope),
            name, SgClassDeclaration::e_class,
            SageBuilder::template_class_declaration_scopes::
                fromExactSemanticScope(firstScope),
            &firstParameters, &arguments, &name);
    SageBuilder::buildTemplateClassDeclaration_nfi(
        SageBuilder::declaration_ownership::sourceLexicalIn(unrelatedScope),
        name, SgClassDeclaration::e_class,
        SageBuilder::template_class_declaration_scopes::fromExactSemanticScope(
            unrelatedScope),
        canonical, &firstParameters, &definingParameters, &arguments, &name);
    return 1;
  }
  if (mode == "--reject-template-definition-missing-symbol") {
    SgTemplateParameterPtrList firstParameters =
        buildTemplateTypeParameters("FirstParameter");
    SgTemplateParameterPtrList definingParameters =
        buildTemplateTypeParameters("DefiningParameter");
    SgTemplateArgumentPtrList arguments;
    const SgName name("rex_missing_symbol_template");
    SgTemplateClassDeclaration *canonical =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            SageBuilder::declaration_ownership::semanticAuxiliary(), name,
            SgClassDeclaration::e_class,
            SageBuilder::template_class_declaration_scopes::
                fromExactSemanticScope(global),
            &firstParameters, &arguments, &name);
    SgSymbol *symbol = canonical->get_symbol_from_symbol_table();
    ROSE_ASSERT(symbol != nullptr);
    global->remove_symbol(symbol);
    SageBuilder::buildTemplateClassDeclaration_nfi(
        SageBuilder::declaration_ownership::semanticAuxiliary(), name,
        SgClassDeclaration::e_class,
        SageBuilder::template_class_declaration_scopes::fromExactSemanticScope(
            global),
        canonical, &firstParameters, &definingParameters, &arguments, &name);
    return 1;
  }

  if (mode == "--check-semantic-function-source-provenance") {
    SgFunctionParameterList *parameters =
        SageBuilder::buildFunctionParameterList_nfi();
    SgInitializedName *parameter = SageBuilder::buildInitializedName_nfi(
        SgName("semantic_parameter"), SageBuilder::buildIntType(), nullptr);
    SageInterface::appendArg(parameters, parameter);
    SgFunctionDeclaration *declaration =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            SgName("rex_semantic_function_provenance"),
            SageBuilder::buildVoidType(), parameters, global);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parent() ==
                global->get_auxiliary_declarations());
    ROSE_ASSERT(declaration->get_parameterList() == parameters);
    ROSE_ASSERT(parameters->get_parent() == declaration);
    ROSE_ASSERT(parameter->get_parent() == parameters);
    assertExactSemanticProvenance(declaration);
    assertExactSemanticProvenance(parameters);
    assertExactSemanticProvenance(parameter);

    SgClassDeclaration *owner = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "rex_semantic_member_provenance_owner", global);
    ROSE_ASSERT(owner != nullptr);
    SgClassDefinition *classScope = owner->get_definition();
    ROSE_ASSERT(classScope != nullptr);
    SgFunctionParameterList *memberParameters =
        SageBuilder::buildFunctionParameterList_nfi();
    SgMemberFunctionDeclaration *member =
        SageBuilder::buildNondefiningMemberFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            SgName("rex_semantic_member_provenance"),
            SageBuilder::buildVoidType(), memberParameters, classScope);
    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_parent() ==
                classScope->get_auxiliary_declarations());
    ROSE_ASSERT(member->get_scope() == classScope);
    ROSE_ASSERT(member->get_parameterList() == memberParameters);
    ROSE_ASSERT(memberParameters->get_parent() == member);
    assertExactSemanticProvenance(member);
    assertExactSemanticProvenance(memberParameters);

    SgCtorInitializerList *ctorInitializers = member->get_CtorInitializerList();
    ROSE_ASSERT(ctorInitializers != nullptr);
    ROSE_ASSERT(ctorInitializers->get_parent() == member);
    ROSE_ASSERT(ctorInitializers->get_ctors().empty());
    ROSE_ASSERT(ctorInitializers->get_definingDeclaration() ==
                ctorInitializers);
    ROSE_ASSERT(ctorInitializers->get_firstNondefiningDeclaration() ==
                ctorInitializers);
    assertExactSemanticProvenance(ctorInitializers);
    return 0;
  }

  if (mode == "--check-frontend-member-ctor-list-publication-boundary") {
    SgClassDeclaration *owner = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "rex_frontend_member_ctor_list_owner", global);
    ROSE_ASSERT(owner != nullptr);
    SgClassDefinition *classScope = owner->get_definition();
    ROSE_ASSERT(classScope != nullptr);

    const SageBuilder::SourcePositionClassification savedMode =
        SageBuilder::getSourcePositionClassificationMode();
    SageBuilder::setSourcePositionClassificationMode(
        SageBuilder::e_sourcePositionFrontendConstruction);
    SgMemberFunctionDeclaration *member =
        SageBuilder::buildNondefiningMemberFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_frontend_member_pending_ctor_list"),
            SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList_nfi(), classScope);
    SageBuilder::setSourcePositionClassificationMode(savedMode);

    ROSE_ASSERT(member != nullptr);
    ROSE_ASSERT(member->get_parent() == classScope);
    ROSE_ASSERT(member->get_scope() == classScope);
    ROSE_ASSERT(member->get_file_info() != nullptr);
    SgCtorInitializerList *ctorInitializers = member->get_CtorInitializerList();
    ROSE_ASSERT(ctorInitializers != nullptr);
    ROSE_ASSERT(ctorInitializers->get_parent() == member);
    ROSE_ASSERT(ctorInitializers->get_ctors().empty());
    ROSE_ASSERT(ctorInitializers->get_definingDeclaration() ==
                ctorInitializers);
    ROSE_ASSERT(ctorInitializers->get_firstNondefiningDeclaration() ==
                ctorInitializers);
    ROSE_ASSERT(
        !ctorInitializers->get_translation_unit_source_order().has_value());
    assertNullSourceProvenance(ctorInitializers);
    return 0;
  }

  auto buildProcedureParameters = [](const char *name, SgType *type) {
    SgFunctionParameterList *parameters =
        SageBuilder::buildFunctionParameterList_nfi();
    if (name != nullptr) {
      SageInterface::appendArg(
          parameters,
          SageBuilder::buildInitializedName_nfi(SgName(name), type, nullptr));
    }
    return parameters;
  };
  auto buildCanonicalProcedure = [&](const char *name,
                                     SgFunctionParameterList *parameters) {
    return SageBuilder::buildNondefiningProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        SgName(name), SageBuilder::buildVoidType(), parameters,
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::
            e_fortran_procedure_source_form_semantic_only,
        global);
  };

  if (mode == "--reject-procedure-parameter-arity") {
    SgProcedureHeaderStatement *canonical = buildCanonicalProcedure(
        "rex_parameter_arity", buildProcedureParameters(nullptr, nullptr));
    SageBuilder::buildProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        "rex_parameter_arity", SageBuilder::buildVoidType(),
        buildProcedureParameters("argument", SageBuilder::buildIntType()),
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
        global, canonical);
    return 1;
  }
  if (mode == "--reject-procedure-parameter-name") {
    SageBuilder::buildProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("rex_parameter_name"), SageBuilder::buildVoidType(),
        buildProcedureParameters("source_name", SageBuilder::buildIntType()),
        buildProcedureParameters("canonical_name", SageBuilder::buildIntType()),
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
        global);
    return 1;
  }
  if (mode == "--reject-exact-procedure-parameter-type") {
    SgProcedureHeaderStatement *canonical = buildCanonicalProcedure(
        "rex_parameter_type",
        buildProcedureParameters("argument", SageBuilder::buildFloatType()));
    SgFunctionDefinition *exactDefinition =
        new SgFunctionDefinition(SageBuilder::buildBasicBlock_nfi());
    SageBuilder::buildProcedureHeaderStatementFromExactDefinition(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        exactDefinition, "rex_parameter_type", SageBuilder::buildVoidType(),
        buildProcedureParameters("argument", SageBuilder::buildIntType()),
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
        SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
        global, canonical);
    return 1;
  }

  if (mode == "--reject-auxiliary-output-role") {
    SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        SgName("rex_invalid_auxiliary_output"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList_nfi(), global);
    SgAuxiliaryDeclarationList *container =
        global->get_auxiliary_declarations();
    ROSE_ASSERT(container != nullptr);
    container->setOutputInCodeGeneration();
    container->validate_semantic_non_output_role();
    return 1;
  }

  if (mode == "--reject-transformation-semantic-function-parameters") {
    SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        SgName("rex_invalid_semantic_parameters"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), global);
    return 1;
  }

  if (mode == "--reject-semantic-typedef-lexical-publication") {
    SgTypedefDeclaration *semantic = SageBuilder::buildTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
        SgTypedefDeclaration::e_typedef, "rex_semantic_typedef",
        SageBuilder::buildIntType(), global);
    assertExactSemanticProvenance(semantic);
    ROSE_ASSERT(SageBuilder::detachAuxiliaryDeclaration(global, semantic));
    SageInterface::appendStatement(semantic, global);
    return 1;
  }
  if (mode == "--reject-unknown-typedef-source-form") {
    SageBuilder::buildTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        SgTypedefDeclaration::e_unknown, "rex_unknown_typedef_source_form",
        SageBuilder::buildIntType(), global);
    return 1;
  }
  if (mode == "--reject-unknown-template-typedef-source-form") {
    SageBuilder::buildTemplateTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        SgTypedefDeclaration::e_unknown,
        "rex_unknown_template_typedef_source_form", SageBuilder::buildIntType(),
        global);
    return 1;
  }
  if (mode == "--reject-unknown-template-typedef-instantiation-source-form") {
    SgTemplateTypedefDeclaration *templateDeclaration =
        SageBuilder::buildTemplateTypedefDeclaration_nfi(
            SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
            SgTypedefDeclaration::e_using, "rex_alias",
            SageBuilder::buildIntType(), global);
    SgName aliasName("rex_alias");
    SgName instantiationName("rex_alias<>");
    SgTemplateArgumentPtrList arguments;
    SageBuilder::buildTemplateInstantiationTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
        SgTypedefDeclaration::e_unknown, aliasName, SageBuilder::buildIntType(),
        global, templateDeclaration, arguments, instantiationName);
    return 1;
  }
  if (mode == "--reject-semantic-function-lexical-publication") {
    SgFunctionDeclaration *semantic =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            SgName("rex_semantic_function"), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList_nfi(), global);
    assertExactSemanticProvenance(semantic);
    ROSE_ASSERT(SageBuilder::detachAuxiliaryDeclaration(global, semantic));
    SageInterface::appendStatement(semantic, global);
    return 1;
  }
  if (mode == "--reject-auxiliary-basic-block-append") {
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_auxiliary_basic_block_owner"),
            SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), global);
    ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
    SgBasicBlock *block = function->get_definition()->get_body();
    ROSE_ASSERT(block != nullptr);
    SgTypedefDeclaration *semantic = SageBuilder::buildTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
        SgTypedefDeclaration::e_typedef, "rex_auxiliary_typedef",
        SageBuilder::buildIntType(), block);
    block->append_statement(semantic);
    return 1;
  }

  SgDeclarationScope *detachedScope = SageBuilder::buildDeclarationScope();
  SageBuilder::attachSemanticDeclarationScope(global, detachedScope);
  ROSE_ASSERT(global->get_auxiliary_declaration_scopes() != nullptr);
  assertExactSemanticProvenance(detachedScope);
  assertExactSemanticProvenance(global->get_auxiliary_declaration_scopes());
  ROSE_ASSERT(global->get_auxiliary_declaration_scopes()->get_scopes().size() ==
              1);
  SageBuilder::detachDeclarationScope(global, detachedScope);
  ROSE_ASSERT(detachedScope->get_parent() == nullptr);
  ROSE_ASSERT(global->get_auxiliary_declaration_scopes() == nullptr);
  delete detachedScope;

  if (mode == "--reject-derived-forward-lexical") {
    SageBuilder::buildNondefiningDerivedTypeStatement(
        SageBuilder::declaration_ownership::sourceLexical(),
        "rex_invalid_lexical_forward", global);
    return 1;
  }
  if (mode == "--reject-duplicate-derived-forward") {
    SageBuilder::buildNondefiningDerivedTypeStatement(
        SageBuilder::declaration_ownership::semanticAuxiliary(),
        "rex_duplicate_derived_forward", global);
    SageBuilder::buildNondefiningDerivedTypeStatement(
        SageBuilder::declaration_ownership::semanticAuxiliary(),
        "rex_duplicate_derived_forward", global);
    return 1;
  }
  if (mode == "--reject-duplicate-derived-definition") {
    SageBuilder::buildNondefiningDerivedTypeStatement(
        SageBuilder::declaration_ownership::semanticAuxiliary(),
        "rex_duplicate_derived_definition", global);
    SageBuilder::buildDerivedTypeStatement(
        SageBuilder::declaration_ownership::sourceLexical(),
        "rex_duplicate_derived_definition", global);
    SageBuilder::buildDerivedTypeStatement(
        SageBuilder::declaration_ownership::sourceLexical(),
        "rex_duplicate_derived_definition", global);
    return 1;
  }

  if (mode == "--reject-ordinary-variable-for-embedded-tag") {
    SgClassDeclaration *pending = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
        "rex_pending_variable_tag", global);
    SageBuilder::buildVariableDeclaration("rex_pending_variable",
                                          pending->get_type(), nullptr, global);
    return 1;
  }
  if (mode == "--reject-ordinary-typedef-for-embedded-tag") {
    SgClassDeclaration *pending = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
        "rex_pending_typedef_tag", global);
    SageBuilder::buildTypedefDeclaration_nfi(
        SageBuilder::typedef_declaration_ownership::sourceLexical(),
        SgTypedefDeclaration::e_typedef, "rex_pending_typedef",
        pending->get_type(), global);
    return 1;
  }
  if (mode == "--reject-mismatched-embedded-tag-type") {
    SgClassDeclaration *pending = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
        "rex_mismatched_embedded_tag", global);
    SageBuilder::buildVariableDeclarationWithEmbeddedTag(
        "rex_mismatched_variable", SageBuilder::buildIntType(), nullptr, global,
        nullptr, pending);
    return 1;
  }

  SgFunctionDeclaration *lexicalFunction =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          SgName("rex_lexical_function_owner"), SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global);
  ROSE_ASSERT(lexicalFunction != nullptr);
  ROSE_ASSERT(lexicalFunction->get_parent() == global);
  ROSE_ASSERT(lexicalFunction->get_scope() == global);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         lexicalFunction) == 1);
  assertExactPublishedProvenance(lexicalFunction, global);

  SgDerivedTypeStatement *derivedForward =
      SageBuilder::buildNondefiningDerivedTypeStatement(
          SageBuilder::declaration_ownership::semanticAuxiliary(),
          "rex_typed_derived_owner", global);
  SgDerivedTypeStatement *derivedDefinition =
      SageBuilder::buildDerivedTypeStatement(
          SageBuilder::declaration_ownership::sourceLexical(),
          "rex_typed_derived_owner", global);
  ROSE_ASSERT(derivedForward != nullptr);
  ROSE_ASSERT(derivedDefinition != nullptr);
  ROSE_ASSERT(derivedForward != derivedDefinition);
  ROSE_ASSERT(derivedForward->get_parent() ==
              global->get_auxiliary_declarations());
  ROSE_ASSERT(derivedForward->get_firstNondefiningDeclaration() ==
              derivedForward);
  ROSE_ASSERT(derivedForward->get_definingDeclaration() == derivedDefinition);
  ROSE_ASSERT(derivedDefinition->get_parent() == global);
  ROSE_ASSERT(derivedDefinition->get_scope() == global);
  ROSE_ASSERT(derivedDefinition->get_firstNondefiningDeclaration() ==
              derivedForward);
  ROSE_ASSERT(derivedDefinition->get_definingDeclaration() ==
              derivedDefinition);
  ROSE_ASSERT(derivedDefinition->get_type() == derivedForward->get_type());
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         derivedDefinition) == 1);

  SgClassDeclaration *host = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(),
      "rex_friend_lexical_host", global);
  assertExactPublishedProvenance(host, global);
  SgClassDeclaration *friendDeclaration =
      SageBuilder::buildNondefiningClassDeclaration_nfi(
          SageBuilder::declaration_ownership::sourceLexicalIn(
              host->get_definition()),
          "rex_distinct_semantic_friend", SgClassDeclaration::e_class, global,
          nullptr, nullptr, nullptr);
  ROSE_ASSERT(friendDeclaration->get_parent() == host->get_definition());
  ROSE_ASSERT(friendDeclaration->get_scope() == global);
  ROSE_ASSERT(
      host->get_definition()->statementExistsInScope(friendDeclaration));
  ROSE_ASSERT(!global->statementExistsInScope(friendDeclaration));
  assertExactPublishedProvenance(friendDeclaration, host->get_definition());
  assertExactSemanticProvenance(
      isSgLocatedNode(host->get_firstNondefiningDeclaration()));

  SgClassDeclaration *embeddedClass = SageBuilder::buildStructDeclaration(
      SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
      "rex_embedded_variable_tag", global);
  assertDetachedTransformationProvenance(embeddedClass);
  SgVariableDeclaration *embeddedVariable =
      SageBuilder::buildVariableDeclarationWithEmbeddedTag(
          "rex_embedded_variable", embeddedClass->get_type(), nullptr, global,
          nullptr, embeddedClass);
  SageInterface::appendStatement(embeddedVariable, global);
  ROSE_ASSERT(embeddedClass->get_parent() == embeddedVariable);
  ROSE_ASSERT(!embeddedClass->get_isAutonomousDeclaration());
  ROSE_ASSERT(embeddedVariable->get_baseTypeDefiningDeclaration() ==
              embeddedClass);
  assertExactPublishedProvenance(embeddedClass, global);

  SgEnumDeclaration *embeddedEnum = SageBuilder::buildEnumDeclaration(
      SageBuilder::declaration_ownership::embeddedDeclaratorChild(),
      "rex_embedded_typedef_tag", false, global);
  assertDetachedTransformationProvenance(embeddedEnum);
  SgTypedefDeclaration *embeddedTypedef =
      SageBuilder::buildTypedefDeclarationWithEmbeddedTag(
          SageBuilder::typedef_declaration_ownership::sourceLexical(),
          SgTypedefDeclaration::e_typedef, "rex_embedded_typedef",
          embeddedEnum->get_type(), global, embeddedEnum);
  ROSE_ASSERT(embeddedEnum->get_parent() == embeddedTypedef);
  ROSE_ASSERT(!embeddedEnum->get_isAutonomousDeclaration());
  ROSE_ASSERT(embeddedTypedef->get_declaration() == embeddedEnum);
  assertExactPublishedProvenance(embeddedTypedef, global);
  assertExactPublishedProvenance(embeddedEnum, global);

  SgEnumDeclaration *lexicalEnum = SageBuilder::buildEnumDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(),
      "rex_lexical_enum_owner", false, global);
  ROSE_ASSERT(lexicalEnum->get_parent() == global);
  assertExactPublishedProvenance(lexicalEnum, global);
  assertExactSemanticProvenance(
      isSgLocatedNode(lexicalEnum->get_firstNondefiningDeclaration()));

  SgEnumDeclaration *lexicalEnumForward =
      SageBuilder::buildNondefiningEnumDeclaration_nfi(
          "rex_lexical_enum_forward_owner", false, global,
          SageBuilder::declaration_ownership::sourceLexical(), nullptr);
  ROSE_ASSERT(lexicalEnumForward->get_parent() == global);
  assertExactPublishedProvenance(lexicalEnumForward, global);

  SgFunctionParameterList *semanticParameters =
      SageBuilder::buildFunctionParameterList_nfi();
  SgInitializedName *semanticParameter = SageBuilder::buildInitializedName_nfi(
      SgName("semantic_parameter"), SageBuilder::buildIntType(), nullptr);
  SageInterface::appendArg(semanticParameters, semanticParameter);
  SgFunctionDeclaration *semanticFunction =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::semanticAuxiliary(),
          SgName("rex_semantic_function_owner"), SageBuilder::buildVoidType(),
          semanticParameters, global);
  SgAuxiliaryDeclarationList *auxiliary = global->get_auxiliary_declarations();
  ROSE_ASSERT(semanticFunction != nullptr);
  ROSE_ASSERT(auxiliary != nullptr);
  auxiliary->validate_semantic_non_output_role();
  ROSE_ASSERT(!auxiliary->get_isModified());
  ROSE_ASSERT(auxiliary->get_file_info() == auxiliary->get_startOfConstruct());
  for (Sg_File_Info *position :
       {auxiliary->get_startOfConstruct(), auxiliary->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == auxiliary);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isOutputInCodeGeneration());
  }
  ROSE_ASSERT(auxiliary->get_parent() == global);
  ROSE_ASSERT(semanticFunction->get_parent() == auxiliary);
  ROSE_ASSERT(semanticFunction->get_scope() == global);
  ROSE_ASSERT(semanticFunction->get_parameterList() == semanticParameters);
  ROSE_ASSERT(semanticParameters->get_parent() == semanticFunction);
  ROSE_ASSERT(semanticParameter->get_parent() == semanticParameters);
  assertExactSemanticProvenance(semanticFunction);
  assertExactSemanticProvenance(semanticParameters);
  assertExactSemanticProvenance(semanticParameter);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(),
                         semanticFunction) == 1);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         semanticFunction) == 0);
  ROSE_ASSERT(
      SageBuilder::detachAuxiliaryDeclaration(global, semanticFunction));
  ROSE_ASSERT(semanticFunction->get_parent() == nullptr);
  SageBuilder::attachAuxiliaryDeclaration(global, semanticFunction);
  ROSE_ASSERT(semanticFunction->get_parent() == auxiliary);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(),
                         semanticFunction) == 1);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         semanticFunction) == 0);

  SgTypedefDeclaration *lexicalTypedef =
      SageBuilder::buildTypedefDeclaration_nfi(
          SageBuilder::typedef_declaration_ownership::sourceLexical(),
          SgTypedefDeclaration::e_typedef, "rex_lexical_typedef_owner",
          SageBuilder::buildIntType(), global);
  ROSE_ASSERT(lexicalTypedef != nullptr);
  ROSE_ASSERT(lexicalTypedef->get_parent() == global);
  ROSE_ASSERT(lexicalTypedef->get_scope() == global);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         lexicalTypedef) == 1);
  assertExactPublishedProvenance(lexicalTypedef, global);

  SgTemplateTypedefDeclaration *lexicalTemplateTypedef =
      SageBuilder::buildTemplateTypedefDeclaration_nfi(
          SageBuilder::typedef_declaration_ownership::sourceLexical(),
          SgTypedefDeclaration::e_using, "rex_lexical_template_typedef_owner",
          SageBuilder::buildIntType(), global);
  ROSE_ASSERT(lexicalTemplateTypedef->get_parent() == global);
  ROSE_ASSERT(lexicalTemplateTypedef->get_typedef_type() ==
              SgTypedefDeclaration::e_using);
  assertExactPublishedProvenance(lexicalTemplateTypedef, global);

  SgTemplateTypedefDeclaration *semanticTemplateTypedef =
      SageBuilder::buildTemplateTypedefDeclaration_nfi(
          SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
          SgTypedefDeclaration::e_using, "rex_semantic_template_typedef_owner",
          SageBuilder::buildIntType(), global);
  ROSE_ASSERT(semanticTemplateTypedef->get_parent() == auxiliary);
  ROSE_ASSERT(semanticTemplateTypedef->get_typedef_type() ==
              SgTypedefDeclaration::e_using);
  assertExactSemanticProvenance(semanticTemplateTypedef);

  SgTypedefDeclaration *semanticTypedef = SageBuilder::buildTypedefDeclaration(
      SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
      SgTypedefDeclaration::e_typedef, "rex_semantic_typedef_owner",
      SageBuilder::buildIntType(), global);
  ROSE_ASSERT(semanticTypedef != nullptr);
  ROSE_ASSERT(semanticTypedef->get_parent() == auxiliary);
  ROSE_ASSERT(semanticTypedef->get_scope() == global);
  ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                         auxiliary->get_declarations().end(),
                         semanticTypedef) == 1);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         semanticTypedef) == 0);
  assertExactSemanticProvenance(semanticTypedef);

  auto countDirectScopeEdges = [](SgScopeStatement *scope, SgNode *node) {
    std::size_t count = 0;
    for (const auto &successor : scope->returnDataMemberPointers()) {
      if (successor.first == node) {
        ++count;
      }
    }
    return count;
  };

  if (mode == "--reject-implicit-variable-redeclaration") {
    SageBuilder::buildVariableDeclaration_nfi("rex_implicit_redeclaration",
                                              SageBuilder::buildIntType(),
                                              nullptr, global);
    SageBuilder::buildVariableDeclaration_nfi("rex_implicit_redeclaration",
                                              SageBuilder::buildIntType(),
                                              nullptr, global);
    return 1;
  }
  if (mode == "--reject-mismatched-variable-redeclaration") {
    SgVariableDeclaration *prior = SageBuilder::buildVariableDeclaration_nfi(
        "rex_prior_identity", SageBuilder::buildIntType(), nullptr, global);
    SageBuilder::buildVariableRedeclaration_nfi(
        "rex_wrong_identity", SageBuilder::buildIntType(), nullptr, global,
        prior->get_variables().front());
    return 1;
  }
  if (mode == "--reject-borrowed-variable-initializer") {
    SgAssignInitializer *ownedInitializer =
        SageBuilder::buildAssignInitializer_nfi(SageBuilder::buildIntVal_nfi(1),
                                                SageBuilder::buildIntType());
    SageBuilder::buildVariableDeclaration_nfi("rex_initializer_owner",
                                              SageBuilder::buildIntType(),
                                              ownedInitializer, global);
    SageBuilder::buildVariableDeclaration_nfi("rex_initializer_borrower",
                                              SageBuilder::buildIntType(),
                                              ownedInitializer, global);
    return 1;
  }
  if (mode == "--reject-parent-only-auxiliary-declaration") {
    SgVariableDeclaration *malformed =
        SageBuilder::buildVariableDeclaration_nfi("rex_parent_only_auxiliary",
                                                  SageBuilder::buildIntType(),
                                                  nullptr, global);
    malformed->set_parent(global);
    SageBuilder::attachAuxiliaryDeclaration(global, malformed);
    return 1;
  }
  if (mode == "--reject-declaration-scope-transfer") {
    SgDeclarationScope *owned = SageBuilder::buildDeclarationScope();
    SageBuilder::attachSemanticDeclarationScope(global, owned);
    SageBuilder::adoptFunctionDeclaratorScope(lexicalFunction, owned);
    return 1;
  }

  SgVariableDeclaration *firstVariable =
      SageBuilder::buildVariableDeclaration_nfi("rex_explicit_redeclaration",
                                                SageBuilder::buildIntType(),
                                                nullptr, global);
  SgInitializedName *firstName = firstVariable->get_variables().front();
  SgVariableDeclaration *secondVariable =
      SageBuilder::buildVariableRedeclaration_nfi("rex_explicit_redeclaration",
                                                  SageBuilder::buildIntType(),
                                                  nullptr, global, firstName);
  SgInitializedName *secondName = secondVariable->get_variables().front();
  ROSE_ASSERT(firstVariable != secondVariable);
  ROSE_ASSERT(firstName != secondName);
  ROSE_ASSERT(firstVariable->get_parent() == nullptr);
  ROSE_ASSERT(secondVariable->get_parent() == nullptr);
  ROSE_ASSERT(firstName->get_parent() == firstVariable);
  ROSE_ASSERT(secondName->get_parent() == secondVariable);
  ROSE_ASSERT(firstName->get_scope() == global);
  ROSE_ASSERT(secondName->get_scope() == global);
  ROSE_ASSERT(secondName->get_prev_decl_item() == firstName);
  ROSE_ASSERT(global->lookup_variable_symbol("rex_explicit_redeclaration")
                  ->get_declaration() == firstName);
  ROSE_ASSERT(countDirectScopeEdges(global, firstVariable) == 0);
  ROSE_ASSERT(countDirectScopeEdges(global, secondVariable) == 0);

  SgDeclarationGroupStatement *sourceGroup = new SgDeclarationGroupStatement();
  ROSE_ASSERT(sourceGroup != nullptr);
  SageInterface::setOneSourcePositionForTransformation(sourceGroup);
  sourceGroup->set_scope(global);
  sourceGroup->set_source_terminator(
      SgDeclarationGroupStatement::e_source_terminator_file_semicolon);

  SgFunctionDeclaration *groupFunction =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceGroupMember(
              sourceGroup),
          SgName("rex_group_function_owner"), SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global);
  SgTypedefDeclaration *groupTypedef = SageBuilder::buildTypedefDeclaration(
      SageBuilder::typedef_declaration_ownership::sourceGroupMember(
          sourceGroup),
      SgTypedefDeclaration::e_typedef, "rex_group_typedef_owner",
      SageBuilder::buildIntType(), global);
  const SgDeclarationStatementPtrList &groupMembers =
      sourceGroup->get_declarations();
  ROSE_ASSERT(groupMembers.size() == 2);
  ROSE_ASSERT(groupMembers[0] == groupFunction);
  ROSE_ASSERT(groupMembers[1] == groupTypedef);
  ROSE_ASSERT(groupFunction->get_parent() == sourceGroup);
  ROSE_ASSERT(groupTypedef->get_parent() == sourceGroup);
  ROSE_ASSERT(sourceGroup->get_parent() == nullptr);
  ROSE_ASSERT(!global->statementExistsInScope(groupFunction));
  ROSE_ASSERT(!global->statementExistsInScope(groupTypedef));
  ROSE_ASSERT(countDirectScopeEdges(global, groupFunction) == 0);
  ROSE_ASSERT(countDirectScopeEdges(global, groupTypedef) == 0);
  ROSE_ASSERT(countDirectScopeEdges(global, sourceGroup) == 0);
  assertDetachedTransformationProvenance(groupFunction);
  assertDetachedTransformationProvenance(groupTypedef);
  sourceGroup->validate();

  SageInterface::appendStatement(sourceGroup, global);
  ROSE_ASSERT(sourceGroup->get_parent() == global);
  ROSE_ASSERT(global->statementExistsInScope(sourceGroup));
  ROSE_ASSERT(countDirectScopeEdges(global, sourceGroup) == 1);
  ROSE_ASSERT(countDirectScopeEdges(global, groupFunction) == 0);
  ROSE_ASSERT(countDirectScopeEdges(global, groupTypedef) == 0);
  assertExactPublishedProvenance(groupFunction, global);
  assertExactPublishedProvenance(groupTypedef, global);
  sourceGroup->validate();

  SgFunctionParameterList *emptyParameters =
      SageBuilder::buildFunctionParameterList();
  SgFunctionParameterTypeList *emptyParameterTypes =
      SageBuilder::buildFunctionParameterTypeList(emptyParameters);
  ROSE_ASSERT(emptyParameterTypes != nullptr);
  ROSE_ASSERT(emptyParameterTypes->get_arguments().empty());
  if (mode == "--reject-missing-namespace-scope") {
    SageBuilder::buildNamespaceDeclaration_nfi(
        "rex_namespace", false, nullptr,
        SageBuilder::e_namespace_declaration_canonical_generated_lexical,
        nullptr, nullptr, nullptr, std::nullopt);
    return 1;
  }
  if (mode == "--reject-missing-source-fragments") {
    SageBuilder::buildNamespaceDeclaration_nfi(
        "rex_namespace", false, global,
        SageBuilder::e_namespace_declaration_source_lexical, nullptr, nullptr,
        nullptr, 1);
    return 1;
  }
  if (mode == "--reject-missing-source-order") {
    SgNamespaceSourceFragment *opening = makeNamespaceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_opening, 10, 1,
        12);
    SgNamespaceSourceFragment *closing = makeNamespaceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_closing, 10, 20,
        20);
    publishNamespaceTokenOccurrences(opening, closing, 10);
    SageBuilder::buildNamespaceDeclaration_nfi(
        "rex_namespace", false, global,
        SageBuilder::e_namespace_declaration_source_lexical, nullptr, opening,
        closing, std::nullopt);
    return 1;
  }
  if (mode == "--reject-reinitialized-declaration-source-order") {
    SgVariableDeclaration *declaration =
        SageBuilder::buildVariableDeclaration_nfi(
            "rex_ordered", SageBuilder::buildIntType(), nullptr, global);
    declaration->initialize_translation_unit_source_order(10);
    declaration->initialize_translation_unit_source_order(20);
    return 1;
  }
  if (mode == "--reject-generated-source-fragments") {
    SgNamespaceSourceFragment *opening = new SgNamespaceSourceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_opening,
        SgNamespaceSourceFragment::
            e_namespace_source_fragment_canonical_generated);
    SgNamespaceSourceFragment *closing = new SgNamespaceSourceFragment(
        SgNamespaceSourceFragment::e_namespace_source_fragment_closing,
        SgNamespaceSourceFragment::
            e_namespace_source_fragment_canonical_generated);
    SageInterface::setOneSourcePositionForTransformation(opening);
    SageInterface::setOneSourcePositionForTransformation(closing);
    SageBuilder::buildNamespaceDeclaration_nfi(
        "rex_namespace", false, global,
        SageBuilder::e_namespace_declaration_source_lexical, nullptr, opening,
        closing, 1);
    return 1;
  }
  if (mode == "--reject-ambiguous-namespace-order") {
    buildSourceNamespace("rex_first_order", global, 10, 10);
    buildSourceNamespace("rex_duplicate_order", global, 10, 20);
    return 1;
  }
  if (mode == "--reject-missing-mixed-declaration-order") {
    SgVariableDeclaration *variable = SageBuilder::buildVariableDeclaration_nfi(
        "rex_missing_order", SageBuilder::buildIntType(), nullptr, global);
    Sg_File_Info *file = new Sg_File_Info(kSourceFilename, 10, 1);
    Sg_File_Info *start = new Sg_File_Info(kSourceFilename, 10, 1);
    Sg_File_Info *end = new Sg_File_Info(kSourceFilename, 10, 17);
    variable->set_file_info(file);
    variable->set_startOfConstruct(start);
    variable->set_endOfConstruct(end);
    file->set_parent(variable);
    start->set_parent(variable);
    end->set_parent(variable);
    variable->set_parent(global);
    global->get_declarations().push_back(variable);
    buildSourceNamespace("rex_after_missing_order", global, 20, 20);
    return 1;
  }

  SgNamespaceDeclarationStatement *namespaceDeclaration =
      SageBuilder::buildNamespaceDeclaration_nfi(
          "rex_namespace", false, global,
          SageBuilder::e_namespace_declaration_canonical_generated_lexical,
          nullptr, nullptr, nullptr, std::nullopt);
  ROSE_ASSERT(namespaceDeclaration != nullptr);
  ROSE_ASSERT(namespaceDeclaration->get_parent() == global);
  ROSE_ASSERT(namespaceDeclaration->get_scope() == global);
  ROSE_ASSERT(namespaceDeclaration->get_definition()->get_parent() ==
              namespaceDeclaration);
  ROSE_ASSERT(std::count(global->get_declarations().begin(),
                         global->get_declarations().end(),
                         namespaceDeclaration) == 1);
  namespaceDeclaration->validate_source_fragments();

  SgTemplateClassDeclaration *primary = new SgTemplateClassDeclaration(
      "rex_box", SgClassDeclaration::e_class, nullptr, nullptr);
  SgTemplateArgumentPtrList arguments;
  const SgName instantiationName("rex_box<>");
  SgClassDeclaration *built = SageBuilder::buildNondefiningClassDeclaration_nfi(
      SageBuilder::declaration_ownership::sourceLexical(), "rex_box",
      SgClassDeclaration::e_class, global, primary, &arguments,
      &instantiationName);
  SgTemplateInstantiationDecl *instantiation =
      isSgTemplateInstantiationDecl(built);
  ROSE_ASSERT(instantiation != nullptr);
  ROSE_ASSERT(instantiation->get_templateDeclaration() == primary);

  SgNamespaceDeclarationStatement *first =
      buildSourceNamespace("rex_reordered", global, 10, 10);
  SgNamespaceDeclarationStatement *last =
      buildSourceNamespace("rex_reordered", global, 30, 30);

  SgVariableDeclaration *between = SageBuilder::buildVariableDeclaration_nfi(
      "rex_between_reopenings", SageBuilder::buildIntType(), nullptr, global);
  Sg_File_Info *betweenFile = new Sg_File_Info(kSourceFilename, 20, 1);
  Sg_File_Info *betweenStart = new Sg_File_Info(kSourceFilename, 20, 1);
  Sg_File_Info *betweenEnd = new Sg_File_Info(kSourceFilename, 20, 10);
  between->set_file_info(betweenFile);
  between->set_startOfConstruct(betweenStart);
  between->set_endOfConstruct(betweenEnd);
  betweenFile->set_parent(between);
  betweenStart->set_parent(between);
  betweenEnd->set_parent(between);
  betweenStart->set_source_sequence_number(20);
  between->set_parent(global);
  between->initialize_translation_unit_source_order(20);
  SgDeclarationStatementPtrList &declarations = global->get_declarations();
  SgDeclarationStatementPtrList::iterator lastPosition =
      std::find(declarations.begin(), declarations.end(), last);
  ROSE_ASSERT(lastPosition != declarations.end());
  declarations.insert(lastPosition, between);

  SgNamespaceDeclarationStatement *middle =
      buildSourceNamespace("rex_reordered", global, 15, 15);
  SgNamespaceDefinitionStatement *firstDefinition = first->get_definition();
  SgNamespaceDefinitionStatement *middleDefinition = middle->get_definition();
  SgNamespaceDefinitionStatement *lastDefinition = last->get_definition();
  ROSE_ASSERT(firstDefinition->get_previousNamespaceDefinition() == nullptr);
  ROSE_ASSERT(firstDefinition->get_nextNamespaceDefinition() ==
              middleDefinition);
  ROSE_ASSERT(middleDefinition->get_previousNamespaceDefinition() ==
              firstDefinition);
  ROSE_ASSERT(middleDefinition->get_nextNamespaceDefinition() ==
              lastDefinition);
  ROSE_ASSERT(lastDefinition->get_previousNamespaceDefinition() ==
              middleDefinition);
  ROSE_ASSERT(lastDefinition->get_nextNamespaceDefinition() == nullptr);
  ROSE_ASSERT(firstDefinition->get_global_definition() == firstDefinition);
  ROSE_ASSERT(middleDefinition->get_global_definition() == firstDefinition);
  ROSE_ASSERT(lastDefinition->get_global_definition() == firstDefinition);
  ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(middle->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(last->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(
      global->lookup_namespace_symbol("rex_reordered")->get_declaration() ==
      first);

  SgTemplateParameterPtrList forwardTemplateParameters =
      buildTemplateTypeParameters("ForwardParameter");
  SgTemplateParameterPtrList definingTemplateParameters =
      buildTemplateTypeParameters("DefiningParameter");
  SgTemplateArgumentPtrList templateArguments;
  const SgName reopenedTemplateName("rex_reopened_template");
  SgTemplateClassDeclaration *templateForward =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          SageBuilder::declaration_ownership::sourceLexicalIn(middleDefinition),
          reopenedTemplateName, SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(middleDefinition),
          &forwardTemplateParameters, &templateArguments,
          &reopenedTemplateName);
  SgTemplateClassDeclaration *templateDefinition =
      SageBuilder::buildTemplateClassDeclaration_nfi(
          SageBuilder::declaration_ownership::sourceLexicalIn(lastDefinition),
          reopenedTemplateName, SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(lastDefinition),
          templateForward, &forwardTemplateParameters,
          &definingTemplateParameters, &templateArguments,
          &reopenedTemplateName);
  SgTemplateClassSymbol *templateSymbol =
      isSgTemplateClassSymbol(firstDefinition->lookup_template_class_symbol(
          reopenedTemplateName, &forwardTemplateParameters,
          &templateArguments));
  ROSE_ASSERT(templateForward != nullptr);
  ROSE_ASSERT(templateDefinition != nullptr);
  ROSE_ASSERT(templateSymbol != nullptr);
  ROSE_ASSERT(templateForward->get_scope() == middleDefinition);
  ROSE_ASSERT(templateForward->get_parent() == middleDefinition);
  ROSE_ASSERT(templateDefinition->get_scope() == lastDefinition);
  ROSE_ASSERT(templateDefinition->get_parent() == lastDefinition);
  ROSE_ASSERT(templateForward->get_firstNondefiningDeclaration() ==
              templateForward);
  ROSE_ASSERT(templateForward->get_definingDeclaration() == templateDefinition);
  ROSE_ASSERT(templateDefinition->get_firstNondefiningDeclaration() ==
              templateForward);
  ROSE_ASSERT(templateDefinition->get_definingDeclaration() ==
              templateDefinition);
  ROSE_ASSERT(templateDefinition->get_type() == templateForward->get_type());
  ROSE_ASSERT(templateSymbol->get_declaration() == templateForward);
  ROSE_ASSERT(templateSymbol->get_parent() ==
              firstDefinition->get_symbol_table());
  ROSE_ASSERT(middleDefinition->lookup_template_class_symbol(
                  reopenedTemplateName, &forwardTemplateParameters,
                  &templateArguments) == templateSymbol);
  ROSE_ASSERT(lastDefinition->lookup_template_class_symbol(
                  reopenedTemplateName, &forwardTemplateParameters,
                  &templateArguments) == templateSymbol);
  ROSE_ASSERT(forwardTemplateParameters.front()->get_parent() ==
              templateForward);
  ROSE_ASSERT(definingTemplateParameters.front()->get_parent() ==
              templateDefinition);
  ROSE_ASSERT(templateDefinition->get_definition()->get_parent() ==
              templateDefinition);

  // A later source declaration in a reopened namespace is a distinct lexical
  // surface, not a second symbol basis.  This is the shape used by standard
  // library forward declarations such as std::tuple.
  SgTemplateParameterPtrList canonicalRedeclarationParameters =
      buildTemplateTypeParameters("RedeclaredParameter");
  SgTemplateParameterPtrList reopenedRedeclarationParameters =
      buildTemplateTypeParameters("RedeclaredParameter");
  SgTemplateArgumentPtrList redeclarationArguments;
  const SgName redeclaredTemplateName("rex_redeclared_template");
  SgTemplateClassDeclaration *canonicalRedeclaration =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          SageBuilder::declaration_ownership::sourceLexicalIn(firstDefinition),
          redeclaredTemplateName, SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(firstDefinition),
          &canonicalRedeclarationParameters, &redeclarationArguments,
          &redeclaredTemplateName);
  SgTemplateClassDeclaration *reopenedRedeclaration =
      SageBuilder::buildNondefiningTemplateClassRedeclaration_nfi(
          SageBuilder::declaration_ownership::sourceLexicalIn(middleDefinition),
          redeclaredTemplateName, SgClassDeclaration::e_class,
          SageBuilder::template_class_declaration_scopes::
              fromExactSemanticScope(middleDefinition),
          canonicalRedeclaration, &reopenedRedeclarationParameters,
          &redeclarationArguments, &redeclaredTemplateName);
  SgTemplateClassSymbol *redeclaredTemplateSymbol =
      firstDefinition->get_symbol_table()->find_template_class(
          redeclaredTemplateName, &canonicalRedeclarationParameters,
          &redeclarationArguments);
  ROSE_ASSERT(canonicalRedeclaration != nullptr);
  ROSE_ASSERT(reopenedRedeclaration != nullptr);
  ROSE_ASSERT(redeclaredTemplateSymbol != nullptr);
  ROSE_ASSERT(redeclaredTemplateSymbol->get_declaration() ==
              canonicalRedeclaration);
  ROSE_ASSERT(redeclaredTemplateSymbol->get_parent() ==
              firstDefinition->get_symbol_table());
  ROSE_ASSERT(reopenedRedeclaration->get_firstNondefiningDeclaration() ==
              canonicalRedeclaration);
  ROSE_ASSERT(reopenedRedeclaration->get_type() ==
              canonicalRedeclaration->get_type());
  ROSE_ASSERT(reopenedRedeclaration->get_symbol_from_symbol_table() ==
              redeclaredTemplateSymbol);
  ROSE_ASSERT(middleDefinition->get_symbol_table()->find_template_class(
                  redeclaredTemplateName, &reopenedRedeclarationParameters,
                  &redeclarationArguments) == nullptr);
  ROSE_ASSERT(middleDefinition->lookup_template_class_symbol(
                  redeclaredTemplateName, &reopenedRedeclarationParameters,
                  &redeclarationArguments) == redeclaredTemplateSymbol);

  const SgDeclarationStatementPtrList::iterator firstPosition =
      std::find(declarations.begin(), declarations.end(), first);
  const SgDeclarationStatementPtrList::iterator middlePosition =
      std::find(declarations.begin(), declarations.end(), middle);
  const SgDeclarationStatementPtrList::iterator betweenPosition =
      std::find(declarations.begin(), declarations.end(), between);
  lastPosition = std::find(declarations.begin(), declarations.end(), last);
  ROSE_ASSERT(firstPosition != declarations.end());
  ROSE_ASSERT(middlePosition != declarations.end());
  ROSE_ASSERT(betweenPosition != declarations.end());
  ROSE_ASSERT(lastPosition != declarations.end());
  ROSE_ASSERT(std::distance(declarations.begin(), firstPosition) <
              std::distance(declarations.begin(), middlePosition));
  ROSE_ASSERT(std::distance(declarations.begin(), middlePosition) <
              std::distance(declarations.begin(), betweenPosition));
  ROSE_ASSERT(std::distance(declarations.begin(), betweenPosition) <
              std::distance(declarations.begin(), lastPosition));

  SgNamespaceDeclarationStatement *constructedFirst =
      buildSourceNamespace("rex_rerooted", global, 40, 40);
  SgVariableDeclaration *priorMember =
      SageBuilder::buildVariableDeclaration_nfi(
          "rex_prior_member", SageBuilder::buildIntType(), nullptr,
          constructedFirst->get_definition());
  SgVariableSymbol *priorMemberSymbol =
      constructedFirst->get_definition()->lookup_variable_symbol(
          "rex_prior_member");
  ROSE_ASSERT(priorMember != nullptr);
  ROSE_ASSERT(priorMemberSymbol != nullptr);
  SgNamespaceDeclarationStatement *sourceFirst =
      buildSourceNamespace("rex_rerooted", global, 35, 35);
  SgNamespaceDefinitionStatement *sourceFirstDefinition =
      sourceFirst->get_definition();
  SgNamespaceDefinitionStatement *constructedFirstDefinition =
      constructedFirst->get_definition();
  ROSE_ASSERT(sourceFirstDefinition->get_previousNamespaceDefinition() ==
              nullptr);
  ROSE_ASSERT(sourceFirstDefinition->get_nextNamespaceDefinition() ==
              constructedFirstDefinition);
  ROSE_ASSERT(constructedFirstDefinition->get_previousNamespaceDefinition() ==
              sourceFirstDefinition);
  ROSE_ASSERT(constructedFirstDefinition->get_nextNamespaceDefinition() ==
              nullptr);
  ROSE_ASSERT(sourceFirstDefinition->get_global_definition() ==
              sourceFirstDefinition);
  ROSE_ASSERT(constructedFirstDefinition->get_global_definition() ==
              sourceFirstDefinition);
  ROSE_ASSERT(sourceFirst->get_firstNondefiningDeclaration() == sourceFirst);
  ROSE_ASSERT(constructedFirst->get_firstNondefiningDeclaration() ==
              sourceFirst);
  ROSE_ASSERT(
      global->lookup_namespace_symbol("rex_rerooted")->get_declaration() ==
      sourceFirst);
  ROSE_ASSERT(sourceFirstDefinition->lookup_variable_symbol(
                  "rex_prior_member") == priorMemberSymbol);
  ROSE_ASSERT(constructedFirstDefinition->lookup_variable_symbol(
                  "rex_prior_member") == priorMemberSymbol);
  ROSE_ASSERT(priorMemberSymbol->get_parent() ==
              sourceFirstDefinition->get_symbol_table());
  const SgDeclarationStatementPtrList::iterator sourceFirstPosition =
      std::find(declarations.begin(), declarations.end(), sourceFirst);
  const SgDeclarationStatementPtrList::iterator constructedFirstPosition =
      std::find(declarations.begin(), declarations.end(), constructedFirst);
  ROSE_ASSERT(sourceFirstPosition != declarations.end());
  ROSE_ASSERT(constructedFirstPosition != declarations.end());
  ROSE_ASSERT(std::distance(declarations.begin(), sourceFirstPosition) <
              std::distance(declarations.begin(), constructedFirstPosition));
  return 0;
}
