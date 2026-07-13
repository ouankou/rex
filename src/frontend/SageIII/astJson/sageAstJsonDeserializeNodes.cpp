#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <limits>

namespace Rose {
namespace AstJson {

namespace {

thread_local bool externalFunctionDeserializationIdentityActive = false;
struct ExternalFunctionDeserializationIdentity {
  const JsonValue *serialized_declaration = nullptr;
  SgFunctionDeclaration *declaration = nullptr;
};
thread_local std::unordered_map<
    uint64_t, std::vector<ExternalFunctionDeserializationIdentity>>
    externalFunctionDeserializationIdentities;
thread_local std::unordered_map<SgSourceFile *,
                                std::vector<SgFunctionDeclaration *>>
    externalFunctionDeclarationsBySource;

std::size_t requiredSizeFromJson(const JsonValue &object,
                                 const std::string &field,
                                 const std::string &context) {
  const int64_t raw = object.requiredInt(field);
  if (raw < 0 ||
      static_cast<uint64_t>(raw) > std::numeric_limits<std::size_t>::max()) {
    throw std::runtime_error("AST JSON " + context + " " + field +
                             " is out of range");
  }
  return static_cast<std::size_t>(raw);
}

SgOmpClause::omp_directive_kind_list
ompDirectiveKindsFromJson(const JsonValue &properties,
                          const std::string &context) {
  const JsonValue &values = properties.at("directive_kinds");
  if (values.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + context +
                             " directive_kinds is not an array");
  }
  if (values.array.empty()) {
    throw std::runtime_error("AST JSON " + context +
                             " has an empty directive_kinds list");
  }

  SgOmpClause::omp_directive_kind_list result;
  std::set<SgOmpClause::omp_directive_kind_enum> seen;
  result.reserve(values.array.size());
  for (const JsonValue &entry : values.array) {
    const int64_t value = entry.asInt();
    const auto kind = static_cast<SgOmpClause::omp_directive_kind_enum>(value);
    switch (value) {
    case SgOmpClause::e_omp_directive_kind_parallel:
    case SgOmpClause::e_omp_directive_kind_for:
    case SgOmpClause::e_omp_directive_kind_do:
    case SgOmpClause::e_omp_directive_kind_simd:
    case SgOmpClause::e_omp_directive_kind_target:
    case SgOmpClause::e_omp_directive_kind_teams:
    case SgOmpClause::e_omp_directive_kind_distribute:
    case SgOmpClause::e_omp_directive_kind_task:
    case SgOmpClause::e_omp_directive_kind_taskloop:
    case SgOmpClause::e_omp_directive_kind_sections:
    case SgOmpClause::e_omp_directive_kind_section:
    case SgOmpClause::e_omp_directive_kind_single:
    case SgOmpClause::e_omp_directive_kind_master:
    case SgOmpClause::e_omp_directive_kind_masked:
    case SgOmpClause::e_omp_directive_kind_critical:
    case SgOmpClause::e_omp_directive_kind_barrier:
    case SgOmpClause::e_omp_directive_kind_taskwait:
    case SgOmpClause::e_omp_directive_kind_taskgroup:
    case SgOmpClause::e_omp_directive_kind_atomic:
    case SgOmpClause::e_omp_directive_kind_flush:
    case SgOmpClause::e_omp_directive_kind_ordered:
    case SgOmpClause::e_omp_directive_kind_scan:
    case SgOmpClause::e_omp_directive_kind_scope:
    case SgOmpClause::e_omp_directive_kind_loop:
    case SgOmpClause::e_omp_directive_kind_workshare:
    case SgOmpClause::e_omp_directive_kind_cancel:
    case SgOmpClause::e_omp_directive_kind_metadirective:
      break;
    case SgOmpClause::e_omp_directive_kind_unknown:
    default:
      throw std::runtime_error("AST JSON " + context +
                               " has an invalid directive_kinds entry");
    }
    if (!seen.insert(kind).second) {
      throw std::runtime_error("AST JSON " + context +
                               " has a duplicate directive_kinds entry");
    }
    result.push_back(kind);
  }
  return result;
}

SgFunctionCallExp::source_operator_surface_enum
sourceOperatorSurfaceFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("source_operator_surface");
  if (value < SgFunctionCallExp::e_no_operator_surface ||
      value > SgFunctionCallExp::e_user_defined_literal_surface) {
    throw std::runtime_error(
        "AST JSON SgFunctionCallExp has an invalid source operator surface");
  }
  return static_cast<SgFunctionCallExp::source_operator_surface_enum>(value);
}

SgFunctionCallExp::source_operator_callee_form_enum
sourceOperatorCalleeFormFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("source_operator_callee_form");
  if (value < SgFunctionCallExp::e_no_operator_callee_form ||
      value > SgFunctionCallExp::e_nonmember_operator_callee) {
    throw std::runtime_error(
        "AST JSON SgFunctionCallExp has an invalid source operator callee "
        "form");
  }
  return static_cast<SgFunctionCallExp::source_operator_callee_form_enum>(
      value);
}

SgUnsignedCharList
sourceOperatorOperandRolesFromJson(const JsonValue &properties) {
  const JsonValue &roles = properties.at("source_operator_operand_roles");
  if (roles.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON SgFunctionCallExp source_operator_operand_roles is not an "
        "array");
  }
  SgUnsignedCharList result;
  result.reserve(roles.array.size());
  for (const JsonValue &entry : roles.array) {
    const int64_t value = entry.asInt();
    if (value != SgFunctionCallExp::e_source_operator_operand &&
        value != SgFunctionCallExp::e_semantic_operator_operand) {
      throw std::runtime_error(
          "AST JSON SgFunctionCallExp has an invalid source operator operand "
          "role");
    }
    result.push_back(static_cast<unsigned char>(value));
  }
  return result;
}

SgUnsignedCharList udlSuffixRolesFromJson(const JsonValue &properties) {
  const JsonValue &roles =
      properties.at("source_user_defined_literal_suffix_roles");
  if (roles.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON SgFunctionCallExp UDL suffix roles is not an array");
  }
  SgUnsignedCharList result;
  result.reserve(roles.array.size());
  for (const JsonValue &entry : roles.array) {
    const int64_t value = entry.asInt();
    if (value !=
            SgFunctionCallExp::e_user_defined_literal_token_without_suffix &&
        value != SgFunctionCallExp::e_user_defined_literal_token_with_suffix) {
      throw std::runtime_error(
          "AST JSON SgFunctionCallExp has an invalid UDL suffix role");
    }
    result.push_back(static_cast<unsigned char>(value));
  }
  return result;
}

SgReturnStmt::return_keyword_kind_enum
returnKeywordKindFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("return_keyword_kind");
  switch (value) {
  case SgReturnStmt::e_return_keyword_return:
    return SgReturnStmt::e_return_keyword_return;
  case SgReturnStmt::e_return_keyword_co_return:
    return SgReturnStmt::e_return_keyword_co_return;
  default:
    throw std::runtime_error(
        "AST JSON SgReturnStmt has an invalid return keyword kind");
  }
}

SgAwaitExpression::coroutine_keyword_kind_enum
coroutineKeywordKindFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("coroutine_keyword_kind");
  switch (value) {
  case SgAwaitExpression::e_coroutine_keyword_co_await:
    return SgAwaitExpression::e_coroutine_keyword_co_await;
  case SgAwaitExpression::e_coroutine_keyword_co_yield:
    return SgAwaitExpression::e_coroutine_keyword_co_yield;
  case SgAwaitExpression::e_coroutine_keyword_unspecified:
  default:
    throw std::runtime_error(
        "AST JSON SgAwaitExpression has an invalid coroutine keyword kind");
  }
}

SgOmpClause::omp_at_kind_enum ompAtKindFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("kind");
  switch (value) {
  case SgOmpClause::e_omp_at_compilation:
    return SgOmpClause::e_omp_at_compilation;
  case SgOmpClause::e_omp_at_execution:
    return SgOmpClause::e_omp_at_execution;
  default:
    throw std::runtime_error("AST JSON SgOmpAtClause has an invalid kind");
  }
}

SgOmpClause::omp_severity_kind_enum
ompSeverityKindFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("kind");
  switch (value) {
  case SgOmpClause::e_omp_severity_fatal:
    return SgOmpClause::e_omp_severity_fatal;
  case SgOmpClause::e_omp_severity_warning:
    return SgOmpClause::e_omp_severity_warning;
  default:
    throw std::runtime_error(
        "AST JSON SgOmpSeverityClause has an invalid kind");
  }
}

SgOmpClause::omp_doacross_kind_enum
ompDoacrossKindFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("kind");
  switch (value) {
  case SgOmpClause::e_omp_doacross_source:
    return SgOmpClause::e_omp_doacross_source;
  case SgOmpClause::e_omp_doacross_sink:
    return SgOmpClause::e_omp_doacross_sink;
  default:
    throw std::runtime_error(
        "AST JSON SgOmpDoacrossClause has an invalid kind");
  }
}

SgOmpClause::omp_when_context_kind_enum
ompDeviceTypeKindFromJson(const JsonValue &properties,
                          const std::string &node_kind) {
  const int64_t value = properties.requiredInt("device_type_kind");
  switch (value) {
  case SgOmpClause::e_omp_when_context_kind_unknown:
    return SgOmpClause::e_omp_when_context_kind_unknown;
  case SgOmpClause::e_omp_when_context_kind_host:
    return SgOmpClause::e_omp_when_context_kind_host;
  case SgOmpClause::e_omp_when_context_kind_nohost:
    return SgOmpClause::e_omp_when_context_kind_nohost;
  case SgOmpClause::e_omp_when_context_kind_any:
    return SgOmpClause::e_omp_when_context_kind_any;
  default:
    throw std::runtime_error("AST JSON " + node_kind +
                             " has an invalid device_type kind");
  }
}

SgProgramHeaderStatement::program_statement_kind_enum
programStatementKindFromJson(const JsonValue &properties) {
  return requiredEnum<SgProgramHeaderStatement::program_statement_kind_enum>(
      properties, "program_statement_kind", "SgProgramHeaderStatement",
      {SgProgramHeaderStatement::e_implicit_program_statement,
       SgProgramHeaderStatement::e_explicit_program_statement});
}

SgProcedureHeaderStatement::subprogram_kind_enum
subprogramKindFromJson(const JsonValue &properties,
                       const std::string &context) {
  return requiredEnum<SgProcedureHeaderStatement::subprogram_kind_enum>(
      properties, "subprogram_kind", context,
      {SgProcedureHeaderStatement::e_function_subprogram_kind,
       SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
       SgProcedureHeaderStatement::e_block_data_subprogram_kind});
}

SgProcedureHeaderStatement::block_data_name_kind_enum
blockDataNameKindFromJson(const JsonValue &properties,
                          const std::string &context) {
  return requiredEnum<SgProcedureHeaderStatement::block_data_name_kind_enum>(
      properties, "block_data_name_kind", context,
      {SgProcedureHeaderStatement::e_unknown_block_data_name_kind,
       SgProcedureHeaderStatement::e_unnamed_block_data,
       SgProcedureHeaderStatement::e_named_block_data});
}

SgProcedureHeaderStatement::fortran_procedure_source_form_enum
procedureSourceFormFromJson(const JsonValue &properties,
                            const std::string &context) {
  return requiredEnum<
      SgProcedureHeaderStatement::fortran_procedure_source_form_enum>(
      properties, "fortran_procedure_source_form", context,
      {SgProcedureHeaderStatement::
           e_fortran_procedure_source_form_semantic_only,
       SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
       SgProcedureHeaderStatement::
           e_fortran_procedure_source_form_compiler_module_header,
       SgProcedureHeaderStatement::
           e_fortran_procedure_source_form_type_declaration,
       SgProcedureHeaderStatement::
           e_fortran_procedure_source_form_type_external});
}

SgProcedureHeaderStatement::fortran_result_type_spec_enum
procedureResultTypeSpecFromJson(const JsonValue &properties,
                                const std::string &context) {
  return requiredEnum<
      SgProcedureHeaderStatement::fortran_result_type_spec_enum>(
      properties, "fortran_result_type_spec", context,
      {SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown,
       SgProcedureHeaderStatement::e_fortran_result_type_spec_intrinsic,
       SgProcedureHeaderStatement::e_fortran_result_type_spec_type,
       SgProcedureHeaderStatement::e_fortran_result_type_spec_class,
       SgProcedureHeaderStatement::e_fortran_result_type_spec_type_star,
       SgProcedureHeaderStatement::e_fortran_result_type_spec_class_star});
}

} // namespace

ExternalFunctionDeserializationIdentityGuard::
    ExternalFunctionDeserializationIdentityGuard() {
  if (externalFunctionDeserializationIdentityActive ||
      !externalFunctionDeserializationIdentities.empty() ||
      !externalFunctionDeclarationsBySource.empty()) {
    throw std::runtime_error(
        "AST JSON external-function deserialization graph is already active");
  }
  externalFunctionDeserializationIdentityActive = true;
}

ExternalFunctionDeserializationIdentityGuard::
    ~ExternalFunctionDeserializationIdentityGuard() {
  externalFunctionDeserializationIdentities.clear();
  externalFunctionDeclarationsBySource.clear();
  externalFunctionDeserializationIdentityActive = false;
}

void restoreFunctionCallSourceMetadata(SgFunctionCallExp *call,
                                       const JsonValue &properties) {
  if (call == nullptr) {
    throw std::runtime_error(
        "AST JSON function-call source metadata has no call owner");
  }
  call->set_uses_operator_syntax(
      properties.requiredBool("uses_operator_syntax"));
  call->set_source_syntax(requiredEnum<SgFunctionCallExp::source_syntax_enum>(
      properties, "source_syntax", "SgFunctionCallExp",
      {SgFunctionCallExp::e_source_function_call,
       SgFunctionCallExp::e_implicit_conversion}));
  const SgFunctionCallExp::source_operator_surface_enum
      source_operator_surface = sourceOperatorSurfaceFromJson(properties);
  const SgFunctionCallExp::source_operator_callee_form_enum
      source_operator_callee_form =
          sourceOperatorCalleeFormFromJson(properties);
  const SgUnsignedCharList source_operator_operand_roles =
      sourceOperatorOperandRolesFromJson(properties);
  const std::string source_user_defined_literal_suffix =
      properties.requiredString("source_user_defined_literal_suffix");
  const SgUnsignedCharList source_user_defined_literal_suffix_roles =
      udlSuffixRolesFromJson(properties);
  const bool user_defined_literal =
      source_operator_surface ==
      SgFunctionCallExp::e_user_defined_literal_surface;
  const bool uses_operator_syntax =
      properties.requiredBool("uses_operator_syntax");
  const bool has_operator_surface =
      source_operator_surface != SgFunctionCallExp::e_no_operator_surface;
  if (has_operator_surface != uses_operator_syntax ||
      (!has_operator_surface &&
       (source_operator_callee_form !=
            SgFunctionCallExp::e_no_operator_callee_form ||
        !source_operator_operand_roles.empty())) ||
      (has_operator_surface &&
       source_operator_callee_form ==
           SgFunctionCallExp::e_no_operator_callee_form) ||
      (call->get_source_syntax() == SgFunctionCallExp::e_implicit_conversion &&
       has_operator_surface) ||
      user_defined_literal != !source_user_defined_literal_suffix.empty() ||
      (user_defined_literal &&
       (source_operator_callee_form !=
            SgFunctionCallExp::e_nonmember_operator_callee ||
        std::any_of(source_operator_operand_roles.begin(),
                    source_operator_operand_roles.end(),
                    [](unsigned char role) {
                      return role !=
                             SgFunctionCallExp::e_semantic_operator_operand;
                    }) ||
        source_user_defined_literal_suffix_roles.empty() ||
        std::none_of(source_user_defined_literal_suffix_roles.begin(),
                     source_user_defined_literal_suffix_roles.end(),
                     [](unsigned char role) {
                       return role ==
                              SgFunctionCallExp::
                                  e_user_defined_literal_token_with_suffix;
                     }))) ||
      (!user_defined_literal &&
       !source_user_defined_literal_suffix_roles.empty())) {
    throw std::runtime_error(
        "AST JSON SgFunctionCallExp has inconsistent operator source metadata");
  }
  call->set_source_operator_surface(source_operator_surface);
  call->set_source_operator_callee_form(source_operator_callee_form);
  call->set_source_operator_operand_roles(source_operator_operand_roles);
  call->set_source_user_defined_literal_suffix(
      SgName(source_user_defined_literal_suffix));
  call->set_source_user_defined_literal_suffix_roles(
      source_user_defined_literal_suffix_roles);
}

SgNode *createNodeFromRecordImpl(const NodeRecord &record, SgProject *project,
                                 const JsonValue &metadata) {
  const JsonValue &p = record.properties;
  const std::string &kind = record.kind;
  if (kind == "SgSourceFile") {
    std::vector<std::string> args = commandLineFromMetadata(metadata);
    SgSourceFile *file = new SgSourceFile(args, project);
    file->set_sourceFileNameWithPath(
        p.requiredString("source_filename_with_path"));
    file->set_sourceFileNameWithoutPath(
        p.requiredString("source_filename_without_path"));
    const JsonValue &include_ownership_paths =
        metadata.at("frontend_include_ownership_paths");
    if (include_ownership_paths.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON frontend_include_ownership_paths is not an array");
    }
    std::unordered_set<std::string> unique_include_ownership_paths;
    for (const JsonValue &path_value : include_ownership_paths.array) {
      const std::string path = path_value.asString();
      if (path.empty()) {
        throw std::runtime_error(
            "AST JSON frontend include ownership path is empty");
      }
      if (!unique_include_ownership_paths.insert(path).second) {
        throw std::runtime_error(
            "AST JSON frontend include ownership path is duplicated: " + path);
      }
      file->get_frontendIncludeOwnershipPathList().push_back(path);
    }
    const JsonValue &system_include_ownership_paths =
        metadata.at("frontend_system_include_ownership_paths");
    if (system_include_ownership_paths.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON frontend_system_include_ownership_paths is not an array");
    }
    std::unordered_set<std::string> unique_system_include_ownership_paths;
    for (const JsonValue &path_value : system_include_ownership_paths.array) {
      const std::string path = path_value.asString();
      if (unique_include_ownership_paths.find(path) ==
          unique_include_ownership_paths.end()) {
        throw std::runtime_error(
            "AST JSON system include ownership path is not in the complete "
            "include ownership list: " +
            path);
      }
      if (!unique_system_include_ownership_paths.insert(path).second) {
        throw std::runtime_error(
            "AST JSON system include ownership path is duplicated: " + path);
      }
      file->get_frontendSystemIncludeOwnershipPathList().push_back(path);
    }
    const JsonValue &external_ownership_paths =
        metadata.at("frontend_external_ownership_paths");
    if (external_ownership_paths.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON frontend_external_ownership_paths is not an array");
    }
    std::unordered_set<std::string> unique_external_ownership_paths;
    for (const JsonValue &path_value : external_ownership_paths.array) {
      const std::string path = path_value.asString();
      if (path.empty()) {
        throw std::runtime_error(
            "AST JSON frontend external ownership path is empty");
      }
      if (!unique_external_ownership_paths.insert(path).second) {
        throw std::runtime_error(
            "AST JSON frontend external ownership path is duplicated: " + path);
      }
      file->get_frontendExternalOwnershipPathList().push_back(path);
    }
    file->set_unparse_output_filename(
        p.requiredString("unparse_output_filename"));
    file->set_skip_unparse(p.requiredBool("skip_unparse"));
    file->set_isGeneratedSource(p.requiredBool("is_generated_source"));
    file->set_C_only(p.requiredBool("C_only"));
    file->set_Cxx_only(p.requiredBool("Cxx_only"));
    file->set_Fortran_only(p.requiredBool("Fortran_only"));
    file->set_CoArrayFortran_only(p.requiredBool("CoArrayFortran_only"));
    file->set_Cuda_only(p.requiredBool("Cuda_only"));
    file->set_OpenCL_only(p.requiredBool("OpenCL_only"));
    file->set_requires_C_preprocessor(
        p.requiredBool("requires_C_preprocessor"));
    file->set_inputFormat(fileOutputFormatFromJson(p, "input_format"));
    file->set_outputFormat(fileOutputFormatFromJson(p, "output_format"));
    file->set_backendCompileFormat(
        fileOutputFormatFromJson(p, "backend_compile_format"));
    file->set_fortran_implicit_none(p.requiredBool("fortran_implicit_none"));
    file->set_inputLanguage(fileLanguageFromJson(p, "input_language"));
    file->set_outputLanguage(fileLanguageFromJson(p, "output_language"));
    file->set_strict_language_handling(
        p.requiredBool("strict_language_handling"));
    file->set_sourceFileUsesCppFileExtension(
        p.requiredBool("source_uses_cpp_extension"));
    file->set_sourceFileUsesFortranFileExtension(
        p.requiredBool("source_uses_fortran_extension"));
    file->set_sourceFileUsesFortran77FileExtension(
        p.requiredBool("source_uses_fortran77_extension"));
    file->set_sourceFileUsesFortran90FileExtension(
        p.requiredBool("source_uses_fortran90_extension"));
    file->set_sourceFileUsesFortran95FileExtension(
        p.requiredBool("source_uses_fortran95_extension"));
    file->set_sourceFileUsesFortran2003FileExtension(
        p.requiredBool("source_uses_fortran2003_extension"));
    file->set_sourceFileUsesFortran2008FileExtension(
        p.requiredBool("source_uses_fortran2008_extension"));
    file->set_sourceFileUsesCoArrayFortranFileExtension(
        p.requiredBool("source_uses_coarray_fortran_extension"));
    file->set_sourceFileTypeIsUnknown(
        p.requiredBool("source_file_type_is_unknown"));
    file->set_experimental_flang_frontend(
        p.requiredBool("experimental_flang_frontend"));
    file->set_openmp(metadata.requiredBool("openmp"));
    file->set_openmp_parse_only(metadata.requiredBool("openmp_parse_only"));
    file->set_openmp_ast_only(metadata.requiredBool("openmp_ast_only"));
    file->set_openmp_analyzing(metadata.requiredBool("openmp_analyzing"));
    file->set_openmp_lowering(metadata.requiredBool("openmp_lowering"));
    file->set_openmp_processed(metadata.requiredBool("openmp_processed"));
    file->set_openacc(metadata.requiredBool("openacc"));
    file->set_skipfinalCompileStep(
        metadata.requiredBool("skipfinalCompileStep"));
    file->set_unparse_tokens(metadata.requiredBool("unparse_tokens"));
    return file;
  }
  if (kind == "SgGlobal") {
    return new SgGlobal();
  }
  if (kind == "SgBasicBlock") {
    return new SgBasicBlock(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgInitializedName") {
    return new SgInitializedName(nullptr, SgName(p.requiredString("name")),
                                 SageBuilder::buildUnknownType());
  }
  if (kind == "SgVariableDeclaration") {
    return new SgVariableDeclaration();
  }
  if (kind == "SgVariableDefinition") {
    return new SgVariableDefinition(static_cast<Sg_File_Info *>(nullptr),
                                    static_cast<SgInitializedName *>(nullptr),
                                    static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgTemplateVariableDeclaration") {
    return new SgTemplateVariableDeclaration();
  }
  if (kind == "SgProgramHeaderStatement") {
    const std::string name = p.requiredString("name");
    SgFunctionType *function_type = nullptr;
    if (const JsonValue *type = p.find("function_type")) {
      function_type = isSgFunctionType(earlyTypeFromJson(*type));
    }
    const auto statementKind = programStatementKindFromJson(p);
    const bool namedInEndStatement = p.requiredBool("named_in_end_statement");
    const std::string endStatementName = p.requiredString("end_statement_name");
    validateFortranProgramNameMetadata(name, statementKind, namedInEndStatement,
                                       endStatementName);
    auto *decl =
        new SgProgramHeaderStatement(SgName(name), function_type, nullptr);
    decl->set_program_statement_kind(statementKind);
    decl->set_named_in_end_statement(namedInEndStatement);
    decl->set_end_statement_name(SgName(endStatementName));
    const std::string anonymous_symbol_key =
        p.requiredString("fortran_anonymous_program_unit_symbol_key");
    SageInterface::isFortranProgramUnitWithoutSourceName(decl);
    if (!anonymous_symbol_key.empty()) {
      throw std::runtime_error(
          "AST JSON SgProgramHeaderStatement exposes an implementation-only "
          "anonymous program-unit symbol key");
    }
    return decl;
  }
  if (kind == "SgProcedureHeaderStatement") {
    const std::string name = p.at("name").asString();
    SgFunctionType *function_type = nullptr;
    if (const JsonValue *type = p.find("function_type")) {
      function_type = isSgFunctionType(earlyTypeFromJson(*type));
    }
    SgProcedureHeaderStatement *decl =
        new SgProcedureHeaderStatement(SgName(name), function_type, nullptr);
    decl->set_subprogram_kind(
        subprogramKindFromJson(p, "SgProcedureHeaderStatement"));
    decl->set_block_data_name_kind(
        blockDataNameKindFromJson(p, "SgProcedureHeaderStatement"));
    decl->set_fortran_procedure_source_form(
        procedureSourceFormFromJson(p, "SgProcedureHeaderStatement"));
    decl->set_fortran_result_type_spec(
        procedureResultTypeSpecFromJson(p, "SgProcedureHeaderStatement"));
    decl->set_named_in_end_statement(p.requiredBool("named_in_end_statement"));
    const std::string anonymous_symbol_key =
        p.requiredString("fortran_anonymous_program_unit_symbol_key");
    SageInterface::isFortranProgramUnitWithoutSourceName(decl);
    if (!anonymous_symbol_key.empty()) {
      throw std::runtime_error(
          "AST JSON SgProcedureHeaderStatement exposes an implementation-only "
          "anonymous program-unit symbol key");
    }
    return decl;
  }
  if (kind == "SgContainsStatement") {
    return new SgContainsStatement();
  }
  if (kind == "SgFortranContinueStmt") {
    return new SgFortranContinueStmt();
  }
  if (kind == "SgProcessControlStatement") {
    SgProcessControlStatement *stmt =
        new SgProcessControlStatement(static_cast<SgExpression *>(nullptr));
    stmt->set_control_kind(
        requiredEnum<SgProcessControlStatement::control_enum>(
            p, "control_kind", "SgProcessControlStatement",
            {SgProcessControlStatement::e_abort,
             SgProcessControlStatement::e_stop,
             SgProcessControlStatement::e_error_stop,
             SgProcessControlStatement::e_exit,
             SgProcessControlStatement::e_fail_image,
             SgProcessControlStatement::e_pause}));
    return stmt;
  }
  if (kind == "SgCommonBlock") {
    return new SgCommonBlock(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgCommonBlockObject") {
    SgCommonBlockObject *object =
        new SgCommonBlockObject(static_cast<Sg_File_Info *>(nullptr));
    object->set_block_name(p.requiredString("block_name"));
    return object;
  }
  if (kind == "SgFortranIncludeLine") {
    return new SgFortranIncludeLine(p.requiredString("filename"));
  }
  if (kind == "SgFunctionDeclaration") {
    return new SgFunctionDeclaration(SgName(p.requiredString("name")), nullptr,
                                     nullptr);
  }
  if (kind == "SgTemplateInstantiationFunctionDecl") {
    SgTemplateArgumentPtrList arguments;
    return new SgTemplateInstantiationFunctionDecl(
        SgName(p.at("name").asString()), nullptr, nullptr, nullptr, arguments);
  }
  if (kind == "SgTemplateFunctionDeclaration") {
    return new SgTemplateFunctionDeclaration(SgName(p.requiredString("name")),
                                             nullptr, nullptr);
  }
  if (kind == "SgFunctionParameterList") {
    return new SgFunctionParameterList();
  }
  if (kind == "SgFunctionDefinition") {
    return new SgFunctionDefinition(static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgTemplateFunctionDefinition") {
    return new SgTemplateFunctionDefinition(
        static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgTypedefDeclaration") {
    return new SgTypedefDeclaration(SgName(p.requiredString("name")),
                                    SageBuilder::buildIntType(), nullptr,
                                    nullptr, nullptr);
  }
  if (kind == "SgTemplateTypedefDeclaration") {
    SgType *base_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("base_type")) {
      base_type = earlyTypeFromJson(*type);
    }
    return new SgTemplateTypedefDeclaration(
        SgName(p.requiredString("name")), base_type, nullptr, nullptr, nullptr);
  }
  if (kind == "SgTemplateInstantiationTypedefDeclaration") {
    SgTemplateArgumentPtrList arguments;
    SgType *base_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("base_type")) {
      base_type = earlyTypeFromJson(*type);
    }
    return new SgTemplateInstantiationTypedefDeclaration(
        SgName(p.at("name").asString()), base_type, nullptr, nullptr, nullptr,
        nullptr, arguments);
  }
  if (kind == "SgModuleStatement") {
    return new SgModuleStatement(SgName(p.requiredString("name")),
                                 requiredClassType(p, "SgModuleStatement"),
                                 nullptr, nullptr);
  }
  if (kind == "SgDerivedTypeStatement") {
    return new SgDerivedTypeStatement(
        SgName(p.requiredString("name")),
        requiredClassType(p, "SgDerivedTypeStatement"), nullptr, nullptr);
  }
  if (kind == "SgClassDeclaration") {
    return new SgClassDeclaration(SgName(p.requiredString("name")),
                                  requiredClassType(p, "SgClassDeclaration"),
                                  nullptr, nullptr);
  }
  if (kind == "SgClassDefinition") {
    return new SgClassDefinition();
  }
  if (kind == "SgTemplateInstantiationDefn") {
    throw std::runtime_error(
        "AST JSON SgTemplateInstantiationDefn requires delayed construction");
  }
  if (kind == "SgBaseClass") {
    return new SgBaseClass(nullptr, p.requiredBool("is_direct_base_class"));
  }
  if (kind == "SgExpBaseClass") {
    return new SgExpBaseClass(nullptr, p.requiredBool("is_direct_base_class"),
                              nullptr);
  }
  if (kind == "SgNonrealBaseClass") {
    return new SgNonrealBaseClass(
        nullptr, p.requiredBool("is_direct_base_class"), nullptr);
  }
  if (kind == "SgDeclarationScope") {
    return new SgDeclarationScope();
  }
  if (kind == "SgFunctionParameterScope") {
    return new SgFunctionParameterScope();
  }
  if (kind == "SgTemplateClassDefinition") {
    return new SgTemplateClassDefinition();
  }
  if (kind == "SgNamespaceDeclarationStatement") {
    return new SgNamespaceDeclarationStatement(
        SgName(p.requiredString("name")), nullptr,
        p.requiredBool("is_unnamed_namespace"));
  }
  if (kind == "SgNamespaceDefinitionStatement") {
    SgNamespaceDefinitionStatement *def = new SgNamespaceDefinitionStatement(
        static_cast<SgNamespaceDeclarationStatement *>(nullptr));
    def->set_isUnionOfReentrantNamespaceDefinitions(
        p.requiredBool("is_union_of_reentrant_namespace_definitions"));
    return def;
  }
  if (kind == "SgEnumDeclaration") {
    return new SgEnumDeclaration(SgName(p.requiredString("name")), nullptr);
  }
  if (kind == "SgCtorInitializerList") {
    return new SgCtorInitializerList();
  }
  if (kind == "SgEmptyDeclaration") {
    const auto role =
        requiredEnum<SgEmptyDeclaration::empty_declaration_role_enum>(
            p, "empty_declaration_lexical_role", "SgEmptyDeclaration",
            {SgEmptyDeclaration::e_empty_declaration_source_semicolon,
             SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor,
             SgEmptyDeclaration::
                 e_empty_declaration_zero_width_source_replacement});
    return new SgEmptyDeclaration(role);
  }
  if (kind == "SgUsingDirectiveStatement") {
    return new SgUsingDirectiveStatement(
        static_cast<SgNamespaceDeclarationStatement *>(nullptr));
  }
  if (kind == "SgUsingDeclarationStatement") {
    SgUsingDeclarationStatement *statement = new SgUsingDeclarationStatement(
        static_cast<SgDeclarationStatement *>(nullptr),
        static_cast<SgInitializedName *>(nullptr));
    statement->set_source_terminal_name(
        SgName(p.requiredString("source_terminal_name")));
    statement->set_is_inheriting_constructor(
        p.requiredBool("is_inheriting_constructor"));
    return statement;
  }
  if (kind == "SgUseStatement") {
    return new SgUseStatement(SgName(p.requiredString("name")),
                              p.requiredBool("only_option"),
                              p.requiredString("module_nature"));
  }
  if (kind == "SgRenamePair") {
    return new SgRenamePair(SgName(p.requiredString("local_name")),
                            SgName(p.requiredString("use_name")));
  }
  if (kind == "SgToken") {
    return new SgToken(
        p.requiredString("lexeme_string"),
        static_cast<unsigned int>(p.requiredInt("classification_code")));
  }
  if (kind == "SgImplicitStatement") {
    SgImplicitStatement *stmt =
        new SgImplicitStatement(p.requiredBool("implicit_none"));
    stmt->set_implicit_spec(
        requiredEnum<SgImplicitStatement::implicit_spec_enum>(
            p, "implicit_spec", "SgImplicitStatement",
            {SgImplicitStatement::e_has_implicit_spec_list,
             SgImplicitStatement::e_none, SgImplicitStatement::e_none_external,
             SgImplicitStatement::e_none_type,
             SgImplicitStatement::e_none_external_and_type}));
    return stmt;
  }
  if (kind == "SgMemberFunctionDeclaration") {
    return new SgMemberFunctionDeclaration(SgName(p.requiredString("name")),
                                           nullptr, nullptr);
  }
  if (kind == "SgTemplateInstantiationMemberFunctionDecl") {
    SgTemplateArgumentPtrList arguments;
    return new SgTemplateInstantiationMemberFunctionDecl(
        SgName(p.at("name").asString()), nullptr, nullptr, nullptr, arguments);
  }
  if (kind == "SgTemplateMemberFunctionDeclaration") {
    return new SgTemplateMemberFunctionDeclaration(
        SgName(p.requiredString("name")), nullptr, nullptr);
  }
  if (kind == "SgTemplateClassDeclaration") {
    SgTemplateClassDeclaration *decl = new SgTemplateClassDeclaration(
        SgName(p.at("name").asString()),
        requiredClassType(p, "SgTemplateClassDeclaration"), nullptr, nullptr);
    const std::string template_name = p.at("template_name").asString();
    if (template_name.empty()) {
      throw std::runtime_error(
          "AST JSON SgTemplateClassDeclaration has an empty template_name");
    }
    decl->set_templateName(SgName(template_name));
    return decl;
  }
  if (kind == "SgTemplateInstantiationDecl") {
    SgTemplateArgumentPtrList arguments;
    const std::string name = p.at("name").asString();
    if (name.find('<') == std::string::npos ||
        name.rfind('>') == std::string::npos) {
      throw std::runtime_error(
          "AST JSON SgTemplateInstantiationDecl name is not a complete "
          "template-id");
    }
    SgTemplateInstantiationDecl *decl = new SgTemplateInstantiationDecl(
        SgName(name), requiredClassType(p, "SgTemplateInstantiationDecl"),
        nullptr, nullptr, nullptr, arguments, SgTemplateArgumentPtrList());
    const std::string template_name = p.at("template_name").asString();
    if (template_name.empty()) {
      throw std::runtime_error(
          "AST JSON SgTemplateInstantiationDecl has an empty template_name");
    }
    decl->set_templateName(SgName(template_name));
    return decl;
  }
  if (kind == "SgTemplateInstantiationDirectiveStatement") {
    return new SgTemplateInstantiationDirectiveStatement(
        static_cast<SgDeclarationStatement *>(nullptr));
  }
  if (kind == "SgNonrealDecl") {
    const std::string name = p.at("name").asString();
    if (name.empty()) {
      throw std::runtime_error("AST JSON SgNonrealDecl has an empty name");
    }
    return new SgNonrealDecl(SgName(name));
  }
  if (kind == "SgTypeExpression") {
    return new SgTypeExpression(earlyTypeFromJson(p.at("represented_type")));
  }
  if (kind == "SgAsteriskShapeExp") {
    return new SgAsteriskShapeExp();
  }
  if (kind == "SgColonShapeExp") {
    return new SgColonShapeExp();
  }
  if (kind == "SgExprStatement") {
    return new SgExprStatement(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgReturnStmt") {
    SgReturnStmt *return_statement =
        new SgReturnStmt(static_cast<SgExpression *>(nullptr));
    return_statement->set_return_keyword_kind(returnKeywordKindFromJson(p));
    return return_statement;
  }
  if (kind == "SgBreakStmt") {
    return new SgBreakStmt();
  }
  if (kind == "SgContinueStmt") {
    return new SgContinueStmt();
  }
  if (kind == "SgGotoStatement") {
    return new SgGotoStatement(static_cast<SgLabelStatement *>(nullptr));
  }
  if (kind == "SgForStatement") {
    return new SgForStatement(static_cast<SgStatement *>(nullptr),
                              static_cast<SgExpression *>(nullptr),
                              static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgFortranDo") {
    return new SgFortranDo(static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgBasicBlock *>(nullptr));
  }
  if (kind == "SgFortranNonblockedDo") {
    return new SgFortranNonblockedDo(static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgBasicBlock *>(nullptr),
                                     static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgImpliedDo") {
    return new SgImpliedDo(static_cast<Sg_File_Info *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExpression *>(nullptr),
                           static_cast<SgExprListExp *>(nullptr),
                           static_cast<SgScopeStatement *>(nullptr));
  }
  if (kind == "SgAllocateStatement") {
    return new SgAllocateStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgNullifyStatement") {
    return new SgNullifyStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgDeallocateStatement") {
    return new SgDeallocateStatement(static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgForInitStatement") {
    return new SgForInitStatement();
  }
  if (kind == "SgIfStmt") {
    return new SgIfStmt(static_cast<SgStatement *>(nullptr),
                        static_cast<SgStatement *>(nullptr),
                        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgWhileStmt") {
    return new SgWhileStmt(static_cast<SgStatement *>(nullptr),
                           static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgDoWhileStmt") {
    return new SgDoWhileStmt(static_cast<SgStatement *>(nullptr),
                             static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgSwitchStatement") {
    return new SgSwitchStatement(static_cast<SgStatement *>(nullptr),
                                 static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgCaseOptionStmt") {
    return new SgCaseOptionStmt(static_cast<SgExpression *>(nullptr),
                                static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgDefaultOptionStmt") {
    return new SgDefaultOptionStmt(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgTryStmt") {
    SgTryStmt *stmt = new SgTryStmt(static_cast<SgStatement *>(nullptr));
    stmt->set_is_function_try_block(p.at("is_function_try_block").asBool());
    return stmt;
  }
  if (kind == "SgCatchStatementSeq") {
    return new SgCatchStatementSeq();
  }
  if (kind == "SgCatchOptionStmt") {
    return new SgCatchOptionStmt(static_cast<SgVariableDeclaration *>(nullptr),
                                 static_cast<SgStatement *>(nullptr),
                                 static_cast<SgTryStmt *>(nullptr));
  }
  if (kind == "SgNullStatement") {
    return new SgNullStatement();
  }
  if (kind == "SgAccessLabelStatement") {
    return new SgAccessLabelStatement(
        requiredEnum<SgAccessLabelStatement::access_label_kind_enum>(
            p, "access_label_kind", "SgAccessLabelStatement",
            {SgAccessLabelStatement::e_access_label_private,
             SgAccessLabelStatement::e_access_label_protected,
             SgAccessLabelStatement::e_access_label_public}));
  }
  if (kind == "SgDeclarationGroupStatement") {
    SgDeclarationGroupStatement *group = new SgDeclarationGroupStatement();
    group->set_source_terminator(
        requiredEnum<SgDeclarationGroupStatement::source_terminator_enum>(
            p, "declaration_group_source_terminator",
            "SgDeclarationGroupStatement",
            {SgDeclarationGroupStatement::e_source_terminator_file_semicolon,
             SgDeclarationGroupStatement::
                 e_source_terminator_macro_semicolon}));
    return group;
  }
  if (kind == "SgAttributedStatement") {
    // Its structural edges are restored after every node and parent identity
    // exists.  Keep the constructor invariant true with one owned placeholder
    // that linkNodeEdges replaces exactly once.
    return new SgAttributedStatement(new SgNullStatement());
  }
  if (kind == "SgNamespaceSourceFragment") {
    return new SgNamespaceSourceFragment(
        requiredEnum<
            SgNamespaceSourceFragment::namespace_source_fragment_kind_enum>(
            p, "namespace_source_fragment_kind", "SgNamespaceSourceFragment",
            {SgNamespaceSourceFragment::
                 e_namespace_source_fragment_opening_introducer,
             SgNamespaceSourceFragment::e_namespace_source_fragment_opening,
             SgNamespaceSourceFragment::e_namespace_source_fragment_closing}),
        requiredEnum<
            SgNamespaceSourceFragment::namespace_source_fragment_form_enum>(
            p, "namespace_source_fragment_form", "SgNamespaceSourceFragment",
            {SgNamespaceSourceFragment::
                 e_namespace_source_fragment_source_spelled,
             SgNamespaceSourceFragment::
                 e_namespace_source_fragment_canonical_generated}));
  }
  if (kind == "SgStatementAttribute") {
    const int64_t raw_integral =
        p.requiredInt("statement_attribute_integral_argument");
    if (raw_integral < 0) {
      throw std::runtime_error(
          "AST JSON SgStatementAttribute has invalid typed fields");
    }
    return new SgStatementAttribute(
        requiredEnum<SgStatementAttribute::statement_attribute_kind_enum>(
            p, "statement_attribute_kind", "SgStatementAttribute",
            {SgStatementAttribute::e_statement_attribute_fallthrough,
             SgStatementAttribute::e_statement_attribute_likely,
             SgStatementAttribute::e_statement_attribute_unlikely,
             SgStatementAttribute::e_statement_attribute_assume,
             SgStatementAttribute::e_statement_attribute_nomerge,
             SgStatementAttribute::e_statement_attribute_musttail,
             SgStatementAttribute::e_statement_attribute_always_inline,
             SgStatementAttribute::e_statement_attribute_opencl_unroll_hint,
             SgStatementAttribute::e_statement_attribute_loop_hint}),
        requiredEnum<SgStatementAttribute::statement_attribute_spelling_enum>(
            p, "statement_attribute_spelling", "SgStatementAttribute",
            {SgStatementAttribute::
                 e_statement_attribute_spelling_cxx11_unscoped,
             SgStatementAttribute::e_statement_attribute_spelling_c23_unscoped,
             SgStatementAttribute::e_statement_attribute_spelling_cxx11_clang,
             SgStatementAttribute::e_statement_attribute_spelling_c23_clang,
             SgStatementAttribute::e_statement_attribute_spelling_gnu,
             SgStatementAttribute::e_statement_attribute_spelling_cxx11_gnu,
             SgStatementAttribute::e_statement_attribute_spelling_c23_gnu,
             SgStatementAttribute::
                 e_statement_attribute_spelling_pragma_clang_loop,
             SgStatementAttribute::e_statement_attribute_spelling_pragma_unroll,
             SgStatementAttribute::
                 e_statement_attribute_spelling_pragma_nounroll,
             SgStatementAttribute::
                 e_statement_attribute_spelling_pragma_unroll_and_jam,
             SgStatementAttribute::
                 e_statement_attribute_spelling_pragma_nounroll_and_jam}),
        static_cast<SgExpression *>(nullptr),
        static_cast<unsigned long>(raw_integral),
        requiredEnum<SgStatementAttribute::loop_hint_option_enum>(
            p, "statement_attribute_loop_hint_option", "SgStatementAttribute",
            {SgStatementAttribute::e_loop_hint_option_none,
             SgStatementAttribute::e_loop_hint_option_vectorize,
             SgStatementAttribute::e_loop_hint_option_vectorize_width,
             SgStatementAttribute::e_loop_hint_option_interleave,
             SgStatementAttribute::e_loop_hint_option_interleave_count,
             SgStatementAttribute::e_loop_hint_option_unroll,
             SgStatementAttribute::e_loop_hint_option_unroll_count,
             SgStatementAttribute::e_loop_hint_option_unroll_and_jam,
             SgStatementAttribute::e_loop_hint_option_unroll_and_jam_count,
             SgStatementAttribute::e_loop_hint_option_pipeline_disabled,
             SgStatementAttribute::
                 e_loop_hint_option_pipeline_initiation_interval,
             SgStatementAttribute::e_loop_hint_option_distribute,
             SgStatementAttribute::e_loop_hint_option_vectorize_predicate}),
        requiredEnum<SgStatementAttribute::loop_hint_state_enum>(
            p, "statement_attribute_loop_hint_state", "SgStatementAttribute",
            {SgStatementAttribute::e_loop_hint_state_none,
             SgStatementAttribute::e_loop_hint_state_enable,
             SgStatementAttribute::e_loop_hint_state_disable,
             SgStatementAttribute::e_loop_hint_state_numeric,
             SgStatementAttribute::e_loop_hint_state_fixed_width,
             SgStatementAttribute::e_loop_hint_state_scalable_width,
             SgStatementAttribute::e_loop_hint_state_assume_safety,
             SgStatementAttribute::e_loop_hint_state_full}));
  }
  if (kind == "SgStatementAttributeList") {
    return new SgStatementAttributeList();
  }
  if (kind == "SgLabelStatement") {
    return new SgLabelStatement(SgName(p.requiredString("label")),
                                static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgPragma") {
    return new SgPragma(p.requiredString("name"), nullptr, nullptr);
  }
  if (kind == "SgPragmaDeclaration") {
    SgPragmaDeclaration *pragma =
        new SgPragmaDeclaration(static_cast<SgPragma *>(nullptr));
    pragma->set_fortran_directive_family(
        requiredEnum<SgPragmaDeclaration::fortran_directive_family_enum>(
            p, "fortran_directive_family", "SgPragmaDeclaration",
            {SgPragmaDeclaration::e_fortran_directive_none,
             SgPragmaDeclaration::e_fortran_directive_openmp,
             SgPragmaDeclaration::e_fortran_directive_ompx,
             SgPragmaDeclaration::e_fortran_directive_openacc,
             SgPragmaDeclaration::e_fortran_directive_cuda}));
    pragma->set_fortran_directive_group_id(
        p.requiredString("fortran_directive_group_id"));
    const int64_t memberIndex = p.requiredInt("fortran_directive_member_index");
    const int64_t memberCount = p.requiredInt("fortran_directive_member_count");
    if (memberIndex < 0 || memberCount < 0) {
      throw std::runtime_error(
          "AST JSON SgPragmaDeclaration has negative Fortran directive "
          "membership");
    }
    pragma->set_fortran_directive_member_index(
        static_cast<unsigned long>(memberIndex));
    pragma->set_fortran_directive_member_count(
        static_cast<unsigned long>(memberCount));
    pragma->set_fortran_directive_primary(
        p.requiredBool("fortran_directive_primary"));
    pragma->set_fortran_directive_raw_text(
        p.requiredString("fortran_directive_raw_text"));
    pragma->set_fortran_directive_logical_text(
        p.requiredString("fortran_directive_logical_text"));
    pragma->set_fortran_directive_semantic_text(
        p.requiredString("fortran_directive_semantic_text"));
    pragma->set_cxx_pragma_payload_kind(
        requiredEnum<SgPragmaDeclaration::cxx_pragma_payload_kind_enum>(
            p, "cxx_pragma_payload_kind", "SgPragmaDeclaration",
            {SgPragmaDeclaration::e_cxx_pragma_payload_none,
             SgPragmaDeclaration::e_cxx_pragma_source_spelled,
             SgPragmaDeclaration::e_cxx_pragma_source_file_only,
             SgPragmaDeclaration::e_cxx_pragma_generated_semantic}));
    pragma->set_cxx_source_text(p.requiredString("cxx_source_text"));
    pragma->set_cxx_top_level_macro_expansion(
        p.requiredBool("cxx_top_level_macro_expansion"));
    return pragma;
  }
  if (kind == "SgAttributeSpecificationStatement") {
    return new SgAttributeSpecificationStatement();
  }
  if (kind == "SgInterfaceStatement") {
    return new SgInterfaceStatement(
        SgName(p.requiredString("name")),
        requiredEnum<SgInterfaceStatement::generic_spec_enum>(
            p, "generic_spec", "SgInterfaceStatement",
            {SgInterfaceStatement::e_default_interface_type,
             SgInterfaceStatement::e_unnamed_interface_type,
             SgInterfaceStatement::e_named_interface_type,
             SgInterfaceStatement::e_operator_interface_type,
             SgInterfaceStatement::e_assignment_interface_type}));
  }
  if (kind == "SgInterfaceBody") {
    return new SgInterfaceBody(SgName(p.requiredString("function_name")),
                               static_cast<SgFunctionDeclaration *>(nullptr),
                               p.requiredBool("use_function_name"));
  }
  if (kind == "SgPrintStatement") {
    return new SgPrintStatement();
  }
  if (kind == "SgReadStatement") {
    return new SgReadStatement();
  }
  if (kind == "SgWriteStatement") {
    return new SgWriteStatement();
  }
  if (kind == "SgOpenStatement") {
    return new SgOpenStatement();
  }
  if (kind == "SgCloseStatement") {
    return new SgCloseStatement();
  }
  if (kind == "SgFlushStatement") {
    return new SgFlushStatement();
  }
  if (kind == "SgBackspaceStatement") {
    return new SgBackspaceStatement();
  }
  if (kind == "SgRewindStatement") {
    return new SgRewindStatement();
  }
  if (kind == "SgEndfileStatement") {
    return new SgEndfileStatement();
  }
  if (kind == "SgWaitStatement") {
    return new SgWaitStatement();
  }
  if (kind == "SgClinkageStartStatement" || kind == "SgClinkageEndStatement") {
    const std::string language = p.requiredString("language_specifier");
    if (language != "C" && language != "C++") {
      throw std::runtime_error("AST JSON " + kind +
                               " has invalid language specifier: " + language);
    }
    SgClinkageDeclarationStatement *linkage =
        kind == "SgClinkageStartStatement"
            ? static_cast<SgClinkageDeclarationStatement *>(
                  new SgClinkageStartStatement())
            : static_cast<SgClinkageDeclarationStatement *>(
                  new SgClinkageEndStatement());
    linkage->set_languageSpecifier(language);
    return linkage;
  }
  if (kind == "SgStaticAssertionDeclaration") {
    return new SgStaticAssertionDeclaration(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAwaitExpression") {
    SgAwaitExpression *await_expression = new SgAwaitExpression(
        static_cast<SgExpression *>(nullptr), SageBuilder::buildUnknownType());
    await_expression->set_coroutine_keyword_kind(
        coroutineKeywordKindFromJson(p));
    return await_expression;
  }
  if (kind == "SgFoldExpression") {
    const std::string operator_token = p.requiredString("operator_token");
    if (operator_token.empty()) {
      throw std::runtime_error(
          "AST JSON SgFoldExpression has an empty operator token");
    }
    return new SgFoldExpression(
        static_cast<SgExpression *>(nullptr), operator_token,
        p.requiredBool("is_left_associative"), SageBuilder::buildUnknownType());
  }
  if (kind == "SgMacroExpansionExp") {
    const std::string spelling = p.requiredString("spelling");
    if (spelling.empty()) {
      throw std::runtime_error(
          "AST JSON SgMacroExpansionExp has empty spelling");
    }
    return new SgMacroExpansionExp(spelling,
                                   static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgSourceLocationBuiltinExp") {
    return new SgSourceLocationBuiltinExp(
        requiredEnum<
            SgSourceLocationBuiltinExp::source_location_builtin_kind_enum>(
            p, "source_location_builtin_kind", "SgSourceLocationBuiltinExp",
            {SgSourceLocationBuiltinExp::e_function,
             SgSourceLocationBuiltinExp::e_function_signature,
             SgSourceLocationBuiltinExp::e_file,
             SgSourceLocationBuiltinExp::e_file_name,
             SgSourceLocationBuiltinExp::e_line,
             SgSourceLocationBuiltinExp::e_column,
             SgSourceLocationBuiltinExp::e_source_location}),
        SageBuilder::buildUnknownType());
  }
  if (kind == "SgFortranCommonBlockRefExp") {
    const std::string useName = p.requiredString("use_name");
    if (useName.empty()) {
      throw std::runtime_error(
          "AST JSON SgFortranCommonBlockRefExp has empty use_name");
    }
    return new SgFortranCommonBlockRefExp(
        SgName(useName), static_cast<SgCommonBlockObject *>(nullptr));
  }
  if (kind == "SgVarRefExp") {
    return new SgVarRefExp(static_cast<SgVariableSymbol *>(nullptr));
  }
  if (kind == "SgLabelRefExp") {
    return new SgLabelRefExp(static_cast<SgLabelSymbol *>(nullptr));
  }
  if (kind == "SgActualArgumentExpression") {
    SgActualArgumentExpression *actual = new SgActualArgumentExpression(
        SgName(p.requiredString("argument_name")), nullptr);
    return actual;
  }
  if (kind == "SgFunctionRefExp") {
    return new SgFunctionRefExp(static_cast<SgFunctionSymbol *>(nullptr),
                                static_cast<SgFunctionType *>(nullptr));
  }
  if (kind == "SgTemplateFunctionRefExp") {
    return new SgTemplateFunctionRefExp(
        static_cast<SgTemplateFunctionSymbol *>(nullptr));
  }
  if (kind == "SgMemberFunctionRefExp") {
    return new SgMemberFunctionRefExp(
        static_cast<SgMemberFunctionSymbol *>(nullptr),
        static_cast<int>(p.requiredInt("virtual_call")),
        isSgFunctionType(earlyTypeFromProperties(p)),
        static_cast<int>(p.requiredInt("need_qualifier")));
  }
  if (kind == "SgTemplateMemberFunctionRefExp") {
    return new SgTemplateMemberFunctionRefExp(
        static_cast<SgTemplateMemberFunctionSymbol *>(nullptr),
        static_cast<int>(p.requiredInt("virtual_call")),
        static_cast<int>(p.requiredInt("need_qualifier")));
  }
  if (kind == "SgExprListExp") {
    return new SgExprListExp();
  }
  if (kind == "SgRequiresExpr") {
    return new SgRequiresExpr(static_cast<Sg_File_Info *>(nullptr),
                              static_cast<SgFunctionParameterList *>(nullptr),
                              static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgSimpleRequirement") {
    return new SgSimpleRequirement(static_cast<Sg_File_Info *>(nullptr),
                                   static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgTypeRequirement") {
    return new SgTypeRequirement(static_cast<Sg_File_Info *>(nullptr),
                                 static_cast<SgType *>(nullptr));
  }
  if (kind == "SgCompoundRequirement") {
    return new SgCompoundRequirement(static_cast<Sg_File_Info *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     p.requiredBool("noexcept_required"),
                                     static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgNestedRequirement") {
    return new SgNestedRequirement(static_cast<Sg_File_Info *>(nullptr),
                                   static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgFunctionCallExp") {
    return new SgFunctionCallExp(static_cast<SgExpression *>(nullptr),
                                 static_cast<SgExprListExp *>(nullptr),
                                 SageBuilder::buildUnknownType());
  }
  if (kind == "SgAggregateInitializer") {
    return new SgAggregateInitializer(
        static_cast<SgExprListExp *>(nullptr), SageBuilder::buildUnknownType(),
        requiredEnum<
            SgAggregateInitializer::aggregate_initializer_source_form_enum>(
            p, "aggregate_source_form", "SgAggregateInitializer",
            {SgAggregateInitializer::e_aggregate_initializer_source_braced,
             SgAggregateInitializer::e_aggregate_initializer_source_unbraced,
             SgAggregateInitializer::
                 e_aggregate_initializer_source_typed_braced,
             SgAggregateInitializer::
                 e_aggregate_initializer_source_compound_literal,
             SgAggregateInitializer::e_aggregate_initializer_source_fortran,
             SgAggregateInitializer::
                 e_aggregate_initializer_source_fortran_structure}));
  }
  if (kind == "SgDesignator") {
    return new SgDesignator(
        requiredEnum<SgDesignator::designator_kind_enum>(
            p, "designator_kind", "SgDesignator",
            {SgDesignator::e_designator_field, SgDesignator::e_designator_array,
             SgDesignator::e_designator_array_range}),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgBoolValExp") {
    return new SgBoolValExp(static_cast<int>(p.requiredInt("value")));
  }
  if (kind == "SgShortVal") {
    return new SgShortVal(static_cast<short>(p.requiredInt("value")),
                          p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedShortVal") {
    return new SgUnsignedShortVal(
        static_cast<unsigned short>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgIntVal") {
    return new SgIntVal(static_cast<int>(p.requiredInt("value")),
                        p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedIntVal") {
    return new SgUnsignedIntVal(
        static_cast<unsigned int>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgLongIntVal") {
    return new SgLongIntVal(static_cast<long>(p.requiredInt("value")),
                            p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedLongVal") {
    return new SgUnsignedLongVal(
        static_cast<unsigned long>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgLongLongIntVal") {
    return new SgLongLongIntVal(p.requiredInt("value"),
                                p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedLongLongIntVal") {
    return new SgUnsignedLongLongIntVal(
        static_cast<unsigned long long>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgCharVal") {
    return new SgCharVal(static_cast<char>(p.requiredInt("value")),
                         p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedCharVal") {
    return new SgUnsignedCharVal(
        static_cast<unsigned char>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgFloatVal") {
    const std::string value = p.requiredString("value");
    return new SgFloatVal(std::stof(value), value);
  }
  if (kind == "SgDoubleVal") {
    const std::string value = p.requiredString("value");
    return new SgDoubleVal(std::stod(value), value);
  }
  if (kind == "SgComplexVal") {
    SgType *precision_type = SageBuilder::buildUnknownType();
    if (const JsonValue *type = p.find("precision_type")) {
      precision_type = earlyTypeFromJson(*type);
    }
    return new SgComplexVal(static_cast<Sg_File_Info *>(nullptr),
                            static_cast<SgExpression *>(nullptr),
                            static_cast<SgExpression *>(nullptr),
                            precision_type, p.requiredString("value_string"));
  }
  if (kind == "SgStringVal") {
    SgStringVal *value = new SgStringVal(p.requiredString("value"));
    value->set_literal_encoding(
        requiredEnum<SgStringVal::string_literal_encoding_enum>(
            p, "literal_encoding", "SgStringVal",
            {SgStringVal::e_string_encoding_ordinary,
             SgStringVal::e_string_encoding_wide,
             SgStringVal::e_string_encoding_utf8,
             SgStringVal::e_string_encoding_utf16,
             SgStringVal::e_string_encoding_utf32}));
    value->set_cxx_unevaluated(p.requiredBool("cxx_unevaluated"));
    value->set_stringDelimiter(
        static_cast<char>(p.requiredInt("string_delimiter")));
    value->set_isRawString(p.requiredBool("is_raw_string"));
    value->set_raw_string_delimiter(p.requiredString("raw_string_delimiter"));
    value->set_raw_string_payload(p.requiredString("raw_string_payload"));
    return value;
  }
  if (kind == "SgAccCollapseClause") {
    return new SgAccCollapseClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccNumGangsClause") {
    return new SgAccNumGangsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccNumWorkersClause") {
    return new SgAccNumWorkersClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccVectorLengthClause") {
    return new SgAccVectorLengthClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccAsyncClause") {
    return new SgAccAsyncClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccIfClause") {
    return new SgAccIfClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccVectorClause") {
    return new SgAccVectorClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgAccCopyClause") {
    return new SgAccCopyClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccCopyinClause") {
    return new SgAccCopyinClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccCopyoutClause") {
    return new SgAccCopyoutClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccCreateClause") {
    return new SgAccCreateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccPresentClause") {
    return new SgAccPresentClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccPrivateClause") {
    return new SgAccPrivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccDeviceptrClause") {
    return new SgAccDeviceptrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccDeleteClause") {
    return new SgAccDeleteClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgAccReductionClause") {
    const int64_t reduction_operator = p.requiredInt("reduction_operator");
    if (reduction_operator < 0 || reduction_operator > 16) {
      throw std::runtime_error(
          "AST JSON SgAccReductionClause has an invalid reduction operator");
    }
    return new SgAccReductionClause(static_cast<SgExprListExp *>(nullptr),
                                    static_cast<int>(reduction_operator));
  }
  if (kind == "SgAccDefaultClause") {
    const int64_t default_kind = p.requiredInt("default_kind");
    if (default_kind < 0 || default_kind > 1) {
      throw std::runtime_error(
          "AST JSON SgAccDefaultClause has an invalid default kind");
    }
    return new SgAccDefaultClause(static_cast<int>(default_kind));
  }
  if (kind == "SgAccGangClause") {
    return new SgAccGangClause();
  }
  if (kind == "SgAccSeqClause") {
    return new SgAccSeqClause();
  }
  if (kind == "SgAccUpdateClause") {
    return new SgAccUpdateClause();
  }
  if (kind == "SgAccReadClause") {
    return new SgAccReadClause();
  }
  if (kind == "SgAccWriteClause") {
    return new SgAccWriteClause();
  }
  if (kind == "SgAccCaptureClause") {
    return new SgAccCaptureClause();
  }
  if (kind == "SgAccParallelStatement") {
    return new SgAccParallelStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAccParallelLoopStatement") {
    return new SgAccParallelLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAccDataStatement") {
    return new SgAccDataStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAccKernelsStatement") {
    return new SgAccKernelsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAccAtomicStatement") {
    return new SgAccAtomicStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAccEnterDataStatement") {
    return new SgAccEnterDataStatement();
  }
  if (kind == "SgAccExitDataStatement") {
    return new SgAccExitDataStatement();
  }
  if (kind == "SgAccRoutineStatement") {
    return new SgAccRoutineStatement(SgName(p.requiredString("routine_name")));
  }
  if (kind == "SgAccWaitStatement") {
    return new SgAccWaitStatement(static_cast<SgExprListExp *>(nullptr),
                                  static_cast<SgExpression *>(nullptr),
                                  p.requiredBool("queues"));
  }
  if (kind == "SgAccCacheStatement") {
    const int64_t modifier = p.requiredInt("modifier");
    if (modifier != 0 && modifier != 1) {
      throw std::runtime_error(
          "AST JSON SgAccCacheStatement has an invalid modifier");
    }
    return new SgAccCacheStatement(static_cast<SgExprListExp *>(nullptr),
                                   static_cast<int>(modifier));
  }
  if (kind == "SgOmpNameExpression") {
    return new SgOmpNameExpression(p.requiredString("spelling"));
  }
  if (kind == "SgOmpSourceExpression") {
    return new SgOmpSourceExpression(p.requiredString("spelling"));
  }
  if (kind == "SgOmpIteratorDefinition") {
    return new SgOmpIteratorDefinition(
        static_cast<SgTypeExpression *>(nullptr),
        static_cast<SgOmpNameExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpInductionItem") {
    return new SgOmpInductionItem(
        requiredEnum<SgOmpClause::omp_induction_item_kind_enum>(
            p, "kind", "SgOmpInductionItem",
            {SgOmpClause::e_omp_induction_item_step,
             SgOmpClause::e_omp_induction_item_binding,
             SgOmpClause::e_omp_induction_item_expression}),
        p.requiredString("label"), static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpApplyTransformation") {
    return new SgOmpApplyTransformation(
        requiredEnum<SgOmpClause::omp_apply_transform_kind_enum>(
            p, "kind", "SgOmpApplyTransformation",
            {SgOmpClause::e_omp_apply_transform_unroll,
             SgOmpClause::e_omp_apply_transform_unroll_partial,
             SgOmpClause::e_omp_apply_transform_unroll_full,
             SgOmpClause::e_omp_apply_transform_reverse,
             SgOmpClause::e_omp_apply_transform_interchange,
             SgOmpClause::e_omp_apply_transform_nothing,
             SgOmpClause::e_omp_apply_transform_tile_sizes,
             SgOmpClause::e_omp_apply_transform_nested_apply,
             SgOmpClause::e_omp_apply_transform_named}),
        requiredEnum<SgOmpClause::omp_clause_separator_enum>(
            p, "separator", "SgOmpApplyTransformation",
            {SgOmpClause::e_omp_clause_separator_none,
             SgOmpClause::e_omp_clause_separator_comma,
             SgOmpClause::e_omp_clause_separator_space}),
        p.requiredString("transformation_name"),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgOmpApplyClause *>(nullptr));
  }
  if (kind == "SgOmpInitModifier") {
    return new SgOmpInitModifier(
        requiredEnum<SgOmpClause::omp_init_modifier_kind_enum>(
            p, "kind", "SgOmpInitModifier",
            {SgOmpClause::e_omp_init_modifier_depobj,
             SgOmpClause::e_omp_init_modifier_interop,
             SgOmpClause::e_omp_init_modifier_prefer_type,
             SgOmpClause::e_omp_init_modifier_depinfo_in,
             SgOmpClause::e_omp_init_modifier_depinfo_out,
             SgOmpClause::e_omp_init_modifier_depinfo_inout,
             SgOmpClause::e_omp_init_modifier_depinfo_inoutset,
             SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset,
             SgOmpClause::e_omp_init_modifier_target,
             SgOmpClause::e_omp_init_modifier_targetsync}),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpInitModifierList") {
    return new SgOmpInitModifierList();
  }
  if (kind == "SgOmpAppendArgsOperation") {
    return new SgOmpAppendArgsOperation(
        static_cast<SgOmpInitModifierList *>(nullptr));
  }
  if (kind == "SgOmpMapDistDataPolicy") {
    return new SgOmpMapDistDataPolicy(
        requiredEnum<SgOmpClause::omp_map_dist_data_enum>(
            p, "policy", "SgOmpMapDistDataPolicy",
            {SgOmpClause::e_omp_map_dist_data_duplicate,
             SgOmpClause::e_omp_map_dist_data_block,
             SgOmpClause::e_omp_map_dist_data_cyclic}),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpMapItem") {
    return new SgOmpMapItem(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpClauseList") {
    return new SgOmpClauseList();
  }
  if (kind == "SgEnumVal") {
    return new SgEnumVal(p.requiredInt("value"),
                         static_cast<SgEnumDeclaration *>(nullptr),
                         SgName(p.requiredString("name")));
  }
  if (kind == "SgTemplateParameterVal") {
    SgTemplateParameterVal *value = new SgTemplateParameterVal(
        static_cast<int>(p.requiredInt("template_parameter_position")),
        p.requiredString("value_string"));
    value->set_valueType(SageBuilder::buildUnknownType());
    return value;
  }
  if (kind == "SgThisExp") {
    return new SgThisExp(static_cast<SgClassSymbol *>(nullptr),
                         static_cast<SgNonrealSymbol *>(nullptr),
                         static_cast<int>(p.requiredInt("pobj_this")),
                         SageBuilder::buildUnknownType());
  }
  if (kind == "SgNonrealRefExp") {
    return new SgNonrealRefExp(static_cast<SgNonrealSymbol *>(nullptr));
  }
  if (kind == "SgPseudoDestructorRefExp") {
    throw std::runtime_error(
        "AST JSON SgPseudoDestructorRefExp requires delayed construction");
  }
  if (kind == "SgNullExpression") {
    SgNullExpression *result = new SgNullExpression();
    result->set_role(requiredEnum<SgNullExpression::null_expression_role_enum>(
        p, "role", "SgNullExpression",
        {SgNullExpression::e_null_expression_syntactic_absence}));
    return result;
  }
  if (kind == "SgNullptrValExp") {
    return new SgNullptrValExp();
  }
  if (kind == "SgCastExp") {
    const SgCastExp::cast_type_enum cast_type =
        requiredEnum<SgCastExp::cast_type_enum>(
            p, "cast_type", "SgCastExp",
            {SgCastExp::e_C_style_cast, SgCastExp::e_const_cast,
             SgCastExp::e_static_cast, SgCastExp::e_dynamic_cast,
             SgCastExp::e_reinterpret_cast, SgCastExp::e_implicit_cast,
             SgCastExp::e_builtin_bit_cast, SgCastExp::e_functional_cast,
             SgCastExp::e_functional_list_cast});
    const SgCastExp::semantic_conversion_kind_enum conversion_kind =
        requiredClosedEnumRange<SgCastExp::semantic_conversion_kind_enum>(
            p, "semantic_conversion_kind", "SgCastExp",
            SgCastExp::e_semantic_conversion_unclassified,
            SgCastExp::e_semantic_conversion_last);
    const SgCastExp::value_category_enum value_category =
        requiredEnum<SgCastExp::value_category_enum>(
            p, "value_category", "SgCastExp",
            {SgCastExp::e_value_category_lvalue,
             SgCastExp::e_value_category_xvalue,
             SgCastExp::e_value_category_prvalue});
    const JsonValue &base_path = p.at("conversion_base_path");
    if (base_path.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON SgCastExp conversion_base_path is not an array");
    }
    return new SgCastExp(static_cast<SgExpression *>(nullptr),
                         SageBuilder::buildUnknownType(), cast_type,
                         conversion_kind, value_category);
  }
  if (kind == "SgConstructorInitializer") {
    throw std::runtime_error(
        "AST JSON SgConstructorInitializer requires delayed construction");
  }
  if (kind == "SgSizeOfOp") {
    return new SgSizeOfOp(static_cast<SgExpression *>(nullptr), nullptr,
                          SageBuilder::buildUnknownType());
  }
  if (kind == "SgAlignOfOp") {
    return new SgAlignOfOp(static_cast<SgExpression *>(nullptr), nullptr,
                           SageBuilder::buildUnknownType());
  }
  if (kind == "SgNoexceptOp") {
    return new SgNoexceptOp(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgConditionalExp") {
    SgConditionalExp *result = new SgConditionalExp(
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr), SageBuilder::buildUnknownType());
    result->set_operator_kind(
        requiredEnum<SgConditionalExp::conditional_operator_kind_enum>(
            p, "conditional_operator_kind", "SgConditionalExp",
            {SgConditionalExp::e_conditional_operator_standard,
             SgConditionalExp::e_conditional_operator_gnu_binary}));
    return result;
  }
  if (kind == "SgStatementExpression") {
    return new SgStatementExpression(static_cast<SgStatement *>(nullptr),
                                     SageBuilder::buildUnknownType());
  }
  if (kind == "SgNewExp") {
    SgNewExp *expr = new SgNewExp(
        SageBuilder::buildUnknownType(), static_cast<SgExprListExp *>(nullptr),
        static_cast<SgConstructorInitializer *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<short>(p.requiredInt("need_global_specifier")),
        static_cast<SgFunctionDeclaration *>(nullptr));
    expr->set_type_id_is_parenthesized(
        p.requiredBool("type_id_is_parenthesized"));
    return expr;
  }
  if (kind == "SgDeleteExp") {
    return new SgDeleteExp(
        static_cast<SgExpression *>(nullptr),
        static_cast<short>(p.requiredInt("is_array")),
        static_cast<short>(p.requiredInt("need_global_specifier")),
        static_cast<SgFunctionDeclaration *>(nullptr));
  }
  if (kind == "SgPackExpansionExpr") {
    return new SgPackExpansionExpr(static_cast<SgExpression *>(nullptr),
                                   SageBuilder::buildUnknownType());
  }
  if (kind == "SgTypeTraitBuiltinOperator") {
    return new SgTypeTraitBuiltinOperator(
        SgName(p.requiredString("name")),
        requiredEnum<SgTypeTraitBuiltinOperator::builtin_operator_kind_enum>(
            p, "builtin_operator_kind", "SgTypeTraitBuiltinOperator",
            {SgTypeTraitBuiltinOperator::e_type_trait_builtin,
             SgTypeTraitBuiltinOperator::e_offsetof_builtin,
             SgTypeTraitBuiltinOperator::e_convert_vector_builtin}),
        SageBuilder::buildUnknownType());
  }
  if (kind == "SgLambdaExp") {
    SgLambdaExp *lambda =
        new SgLambdaExp(static_cast<SgLambdaCaptureList *>(nullptr),
                        static_cast<SgClassDeclaration *>(nullptr),
                        static_cast<SgFunctionDeclaration *>(nullptr));
    lambda->set_is_mutable(p.at("is_mutable").asBool());
    lambda->set_capture_default(p.at("capture_default").asBool());
    lambda->set_default_is_by_reference(
        p.at("default_is_by_reference").asBool());
    lambda->set_explicit_return_type(p.at("explicit_return_type").asBool());
    lambda->set_has_parameter_decl(p.at("has_parameter_decl").asBool());
    lambda->set_is_device(p.at("is_device").asBool());
    if (!lambda->get_capture_default() &&
        lambda->get_default_is_by_reference()) {
      throw std::runtime_error(
          "AST JSON lambda has reference default without capture default");
    }
    return lambda;
  }
  if (kind == "SgLambdaCaptureList") {
    return new SgLambdaCaptureList();
  }
  if (kind == "SgDeclarationScopeList") {
    return new SgDeclarationScopeList();
  }
  if (kind == "SgAuxiliaryDeclarationList") {
    return new SgAuxiliaryDeclarationList();
  }
  if (kind == "SgTemplateParameterList") {
    return new SgTemplateParameterList();
  }
  if (kind == "SgLambdaCapture") {
    return new SgLambdaCapture(static_cast<SgExpression *>(nullptr),
                               static_cast<SgExpression *>(nullptr),
                               static_cast<SgExpression *>(nullptr),
                               p.at("capture_by_reference").asBool(),
                               p.at("implicit").asBool(),
                               p.at("pack_expansion").asBool());
  }

  if (SgBinaryOp *binary =
          buildBinaryOpForKind(kind, SageBuilder::buildUnknownType())) {
    return binary;
  }

  if (SgUnaryOp *unary =
          buildUnaryOpForKind(kind, SageBuilder::buildUnknownType(), p)) {
    return unary;
  }
  if (kind == "SgSubscriptExpression") {
    return new SgSubscriptExpression(static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr),
                                     static_cast<SgExpression *>(nullptr));
  }

  if (kind == "SgTemplateArgument") {
    return new SgTemplateArgument(
        requiredTemplateArgumentType(p, "SgTemplateArgument"),
        p.requiredBool("is_array_bound_unknown_type"), nullptr, nullptr,
        nullptr, p.requiredBool("explicitly_specified"));
  }
  if (kind == "SgTemplateDeclaration") {
    SgTemplateDeclaration *declaration =
        new SgTemplateDeclaration(SgName(p.requiredString("name")));
    declaration->set_template_kind(
        requiredEnum<SgTemplateDeclaration::template_type_enum>(
            p, "template_kind", "SgTemplateDeclaration",
            {SgTemplateDeclaration::e_template_none,
             SgTemplateDeclaration::e_template_class,
             SgTemplateDeclaration::e_template_m_class,
             SgTemplateDeclaration::e_template_function,
             SgTemplateDeclaration::e_template_m_function,
             SgTemplateDeclaration::e_template_m_data,
             SgTemplateDeclaration::e_template_variable}));
    return declaration;
  }
  if (kind == "SgTemplateParameter") {
    SgTemplateParameter *parameter = new SgTemplateParameter(
        SgTemplateParameter::parameter_undefined, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr);
    parameter->set_parameterType(
        requiredTemplateParameterType(p, "SgTemplateParameter"));
    if (parameter->get_type() != nullptr) {
      throw std::runtime_error(
          "AST JSON SgTemplateParameter acquired a synthetic canonical type "
          "before exact type reconstruction");
    }
    return parameter;
  }

  if (kind == "SgOmpMapClause") {
    SgOmpMapClause *clause = new SgOmpMapClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_map_operator_enum>(
            p, "operation", "SgOmpMapClause",
            {SgOmpClause::e_omp_map_unknown, SgOmpClause::e_omp_map_alloc,
             SgOmpClause::e_omp_map_to, SgOmpClause::e_omp_map_from,
             SgOmpClause::e_omp_map_tofrom, SgOmpClause::e_omp_map_storage,
             SgOmpClause::e_omp_map_release, SgOmpClause::e_omp_map_delete,
             SgOmpClause::e_omp_map_present, SgOmpClause::e_omp_map_self}));
    auto modifier = [&](const char *field) {
      return requiredEnum<SgOmpClause::omp_map_modifier_enum>(
          p, field, "SgOmpMapClause",
          {SgOmpClause::e_omp_map_modifier_unspecified,
           SgOmpClause::e_omp_map_modifier_always,
           SgOmpClause::e_omp_map_modifier_close,
           SgOmpClause::e_omp_map_modifier_present,
           SgOmpClause::e_omp_map_modifier_self,
           SgOmpClause::e_omp_map_modifier_mapper,
           SgOmpClause::e_omp_map_modifier_iterator});
    };
    clause->set_modifier1(modifier("modifier1"));
    clause->set_modifier2(modifier("modifier2"));
    clause->set_modifier3(modifier("modifier3"));
    return clause;
  }
  if (kind == "SgOmpDependClause") {
    return new SgOmpDependClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_depend_modifier_enum>(
            p, "depend_modifier", "SgOmpDependClause",
            {SgOmpClause::e_omp_depend_modifier_unspecified,
             SgOmpClause::e_omp_depend_modifier_iterator}),
        requiredEnum<SgOmpClause::omp_dependence_type_enum>(
            p, "dependence_type", "SgOmpDependClause",
            {SgOmpClause::e_omp_depend_in, SgOmpClause::e_omp_depend_out,
             SgOmpClause::e_omp_depend_inout,
             SgOmpClause::e_omp_depend_inoutset,
             SgOmpClause::e_omp_depend_mutexinoutset,
             SgOmpClause::e_omp_depend_depobj, SgOmpClause::e_omp_depend_source,
             SgOmpClause::e_omp_depend_sink}));
  }
  if (kind == "SgOmpAffinityClause") {
    return new SgOmpAffinityClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_affinity_modifier_enum>(
            p, "affinity_modifier", "SgOmpAffinityClause",
            {SgOmpClause::e_omp_affinity_modifier_unspecified,
             SgOmpClause::e_omp_affinity_modifier_iterator}));
  }
  if (kind == "SgOmpToClause") {
    SgOmpToClause *clause =
        new SgOmpToClause(static_cast<SgExprListExp *>(nullptr),
                          requiredEnum<SgOmpClause::omp_to_kind_enum>(
                              p, "kind", "SgOmpToClause",
                              {SgOmpClause::e_omp_to_kind_unknown,
                               SgOmpClause::e_omp_to_kind_mapper,
                               SgOmpClause::e_omp_to_kind_iterator,
                               SgOmpClause::e_omp_to_kind_present}));
    clause->set_declare_target_extended_list(
        p.requiredBool("declare_target_extended_list"));
    return clause;
  }
  if (kind == "SgOmpFromClause") {
    return new SgOmpFromClause(static_cast<SgExprListExp *>(nullptr),
                               requiredEnum<SgOmpClause::omp_from_kind_enum>(
                                   p, "kind", "SgOmpFromClause",
                                   {SgOmpClause::e_omp_from_kind_unknown,
                                    SgOmpClause::e_omp_from_kind_mapper,
                                    SgOmpClause::e_omp_from_kind_iterator,
                                    SgOmpClause::e_omp_from_kind_present}));
  }
  if (kind == "SgOmpLinkClause") {
    return new SgOmpLinkClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpEnterClause") {
    return new SgOmpEnterClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpLocalClause") {
    return new SgOmpLocalClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpSelfMapsClause") {
    return new SgOmpSelfMapsClause();
  }
  if (kind == "SgOmpIndirectClause") {
    return new SgOmpIndirectClause();
  }
  if (kind == "SgOmpNoOpenmpClause") {
    return new SgOmpNoOpenmpClause();
  }
  if (kind == "SgOmpNoOpenmpRoutinesClause") {
    return new SgOmpNoOpenmpRoutinesClause();
  }
  if (kind == "SgOmpNoParallelismClause") {
    return new SgOmpNoParallelismClause();
  }
  if (kind == "SgOmpAtClause") {
    return new SgOmpAtClause(ompAtKindFromJson(p));
  }
  if (kind == "SgOmpSeverityClause") {
    return new SgOmpSeverityClause(ompSeverityKindFromJson(p));
  }
  if (kind == "SgOmpDoacrossClause") {
    return new SgOmpDoacrossClause(ompDoacrossKindFromJson(p),
                                   static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpOtherwiseClause") {
    return new SgOmpOtherwiseClause(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpInductionClause") {
    return new SgOmpInductionClause();
  }
  if (kind == "SgOmpApplyClause") {
    return new SgOmpApplyClause(p.requiredString("label"));
  }
  if (kind == "SgOmpInitClause") {
    return new SgOmpInitClause(static_cast<SgOmpInitModifierList *>(nullptr),
                               static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpAlignClause") {
    return new SgOmpAlignClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpMessageClause") {
    return new SgOmpMessageClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpGraphIdClause") {
    return new SgOmpGraphIdClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpGraphResetClause") {
    return new SgOmpGraphResetClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpTransparentClause") {
    return new SgOmpTransparentClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpThreadsetClause") {
    return new SgOmpThreadsetClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSafesyncClause") {
    return new SgOmpSafesyncClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpLooprangeClause") {
    return new SgOmpLooprangeClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNoOpenmpConstructsClause") {
    return new SgOmpNoOpenmpConstructsClause(
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpHoldsClause") {
    return new SgOmpHoldsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpUseClause") {
    return new SgOmpUseClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpAbsentClause") {
    return new SgOmpAbsentClause(
        ompDirectiveKindsFromJson(p, "SgOmpAbsentClause"));
  }
  if (kind == "SgOmpContainsClause") {
    return new SgOmpContainsClause(
        ompDirectiveKindsFromJson(p, "SgOmpContainsClause"));
  }
  if (kind == "SgOmpDefaultClause") {
    return new SgOmpDefaultClause(
        requiredEnum<SgOmpClause::omp_default_option_enum>(
            p, "data_sharing", "SgOmpDefaultClause",
            {SgOmpClause::e_omp_default_variant,
             SgOmpClause::e_omp_default_none, SgOmpClause::e_omp_default_shared,
             SgOmpClause::e_omp_default_private,
             SgOmpClause::e_omp_default_firstprivate}),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpProcBindClause") {
    return new SgOmpProcBindClause(
        requiredEnum<SgOmpClause::omp_proc_bind_policy_enum>(
            p, "policy", "SgOmpProcBindClause",
            {SgOmpClause::e_omp_proc_bind_policy_master,
             SgOmpClause::e_omp_proc_bind_policy_close,
             SgOmpClause::e_omp_proc_bind_policy_spread}));
  }
  if (kind == "SgOmpNowaitClause") {
    return new SgOmpNowaitClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpOrderedClause") {
    return new SgOmpOrderedClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpCollapseClause") {
    return new SgOmpCollapseClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpIfClause") {
    return new SgOmpIfClause(
        static_cast<SgExpression *>(nullptr),
        requiredEnum<SgOmpClause::omp_if_modifier_enum>(
            p, "modifier", "SgOmpIfClause",
            {SgOmpClause::e_omp_if_modifier_unknown,
             SgOmpClause::e_omp_if_parallel, SgOmpClause::e_omp_if_simd,
             SgOmpClause::e_omp_if_target, SgOmpClause::e_omp_if_cancel,
             SgOmpClause::e_omp_if_taskloop, SgOmpClause::e_omp_if_target_data,
             SgOmpClause::e_omp_if_target_enter_data,
             SgOmpClause::e_omp_if_target_exit_data, SgOmpClause::e_omp_if_task,
             SgOmpClause::e_omp_if_target_update}));
  }
  if (kind == "SgOmpNumThreadsClause") {
    return new SgOmpNumThreadsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNumTeamsClause") {
    return new SgOmpNumTeamsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSafelenClause") {
    return new SgOmpSafelenClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSimdlenClause") {
    return new SgOmpSimdlenClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPrivateClause") {
    return new SgOmpPrivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpFirstprivateClause") {
    SgOmpFirstprivateClause *clause =
        new SgOmpFirstprivateClause(static_cast<SgExprListExp *>(nullptr));
    clause->set_saved(p.requiredBool("saved"));
    return clause;
  }
  if (kind == "SgOmpCopyinClause") {
    return new SgOmpCopyinClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpLastprivateClause") {
    return new SgOmpLastprivateClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_lastprivate_modifier_enum>(
            p, "modifier", "SgOmpLastprivateClause",
            {SgOmpClause::e_omp_lastprivate_modifier_unspecified,
             SgOmpClause::e_omp_lastprivate_conditional}));
  }
  if (kind == "SgOmpReductionClause") {
    return new SgOmpReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_reduction_modifier_enum>(
            p, "modifier", "SgOmpReductionClause",
            {SgOmpClause::e_omp_reduction_modifier_unknown,
             SgOmpClause::e_omp_reduction_inscan,
             SgOmpClause::e_omp_reduction_task,
             SgOmpClause::e_omp_reduction_default,
             SgOmpClause::e_omp_reduction_original_private}),
        requiredEnum<SgOmpClause::omp_reduction_identifier_enum>(
            p, "identifier", "SgOmpReductionClause",
            {SgOmpClause::e_omp_reduction_plus,
             SgOmpClause::e_omp_reduction_mul,
             SgOmpClause::e_omp_reduction_minus,
             SgOmpClause::e_omp_reduction_bitand,
             SgOmpClause::e_omp_reduction_bitor,
             SgOmpClause::e_omp_reduction_bitxor,
             SgOmpClause::e_omp_reduction_logand,
             SgOmpClause::e_omp_reduction_logor,
             SgOmpClause::e_omp_reduction_and, SgOmpClause::e_omp_reduction_or,
             SgOmpClause::e_omp_reduction_eqv,
             SgOmpClause::e_omp_reduction_neqv,
             SgOmpClause::e_omp_reduction_max, SgOmpClause::e_omp_reduction_min,
             SgOmpClause::e_omp_reduction_iand,
             SgOmpClause::e_omp_reduction_ior,
             SgOmpClause::e_omp_reduction_ieor,
             SgOmpClause::e_omp_reduction_user_defined_identifier}),
        static_cast<SgOmpNameExpression *>(nullptr));
  }
  if (kind == "SgOmpLinearClause") {
    return new SgOmpLinearClause(
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        requiredEnum<SgOmpClause::omp_linear_modifier_enum>(
            p, "modifier", "SgOmpLinearClause",
            {SgOmpClause::e_omp_linear_modifier_unspecified,
             SgOmpClause::e_omp_linear_modifier_ref,
             SgOmpClause::e_omp_linear_modifier_val,
             SgOmpClause::e_omp_linear_modifier_uval}));
  }
  if (kind == "SgOmpAlignedClause") {
    return new SgOmpAlignedClause(static_cast<SgExprListExp *>(nullptr),
                                  static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpSharedClause") {
    return new SgOmpSharedClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpScheduleClause") {
    return new SgOmpScheduleClause(
        requiredEnum<SgOmpClause::omp_schedule_modifier_enum>(
            p, "modifier", "SgOmpScheduleClause",
            {SgOmpClause::e_omp_schedule_modifier_unspecified,
             SgOmpClause::e_omp_schedule_modifier_monotonic,
             SgOmpClause::e_omp_schedule_modifier_nonmonotonic,
             SgOmpClause::e_omp_schedule_modifier_simd}),
        requiredEnum<SgOmpClause::omp_schedule_modifier_enum>(
            p, "modifier1", "SgOmpScheduleClause",
            {SgOmpClause::e_omp_schedule_modifier_unspecified,
             SgOmpClause::e_omp_schedule_modifier_monotonic,
             SgOmpClause::e_omp_schedule_modifier_nonmonotonic,
             SgOmpClause::e_omp_schedule_modifier_simd}),
        requiredEnum<SgOmpClause::omp_schedule_kind_enum>(
            p, "kind", "SgOmpScheduleClause",
            {SgOmpClause::e_omp_schedule_kind_static,
             SgOmpClause::e_omp_schedule_kind_dynamic,
             SgOmpClause::e_omp_schedule_kind_guided,
             SgOmpClause::e_omp_schedule_kind_auto,
             SgOmpClause::e_omp_schedule_kind_runtime}),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpDistScheduleClause") {
    return new SgOmpDistScheduleClause(
        requiredEnum<SgOmpClause::omp_dist_schedule_kind_enum>(
            p, "kind", "SgOmpDistScheduleClause",
            {SgOmpClause::e_omp_dist_schedule_kind_static}),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpOrderClause") {
    return new SgOmpOrderClause(
        requiredEnum<SgOmpClause::omp_order_kind_enum>(
            p, "kind", "SgOmpOrderClause",
            {SgOmpClause::e_omp_order_kind_concurrent}),
        requiredEnum<SgOmpClause::omp_order_modifier_enum>(
            p, "modifier", "SgOmpOrderClause",
            {SgOmpClause::e_omp_order_modifier_unspecified,
             SgOmpClause::e_omp_order_modifier_reproducible,
             SgOmpClause::e_omp_order_modifier_unconstrained}));
  }
  if (kind == "SgOmpAtomicDefaultMemOrderClause") {
    return new SgOmpAtomicDefaultMemOrderClause(
        requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
            p, "kind", "SgOmpAtomicDefaultMemOrderClause",
            {SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed}));
  }
  if (kind == "SgOmpDefaultmapClause") {
    return new SgOmpDefaultmapClause(
        requiredEnum<SgOmpClause::omp_defaultmap_behavior_enum>(
            p, "behavior", "SgOmpDefaultmapClause",
            {SgOmpClause::e_omp_defaultmap_behavior_alloc,
             SgOmpClause::e_omp_defaultmap_behavior_to,
             SgOmpClause::e_omp_defaultmap_behavior_from,
             SgOmpClause::e_omp_defaultmap_behavior_tofrom,
             SgOmpClause::e_omp_defaultmap_behavior_firstprivate,
             SgOmpClause::e_omp_defaultmap_behavior_none,
             SgOmpClause::e_omp_defaultmap_behavior_default,
             SgOmpClause::e_omp_defaultmap_behavior_present}),
        requiredEnum<SgOmpClause::omp_defaultmap_category_enum>(
            p, "category", "SgOmpDefaultmapClause",
            {SgOmpClause::e_omp_defaultmap_category_unspecified,
             SgOmpClause::e_omp_defaultmap_category_scalar,
             SgOmpClause::e_omp_defaultmap_category_aggregate,
             SgOmpClause::e_omp_defaultmap_category_pointer,
             SgOmpClause::e_omp_defaultmap_category_allocatable}));
  }
  if (kind == "SgOmpBindClause") {
    return new SgOmpBindClause(requiredEnum<SgOmpClause::omp_bind_binding_enum>(
        p, "binding", "SgOmpBindClause",
        {SgOmpClause::e_omp_bind_binding_teams,
         SgOmpClause::e_omp_bind_binding_parallel,
         SgOmpClause::e_omp_bind_binding_thread}));
  }
  if (kind == "SgOmpParallelClause") {
    return new SgOmpParallelClause();
  }
  if (kind == "SgOmpSectionsClause") {
    return new SgOmpSectionsClause();
  }
  if (kind == "SgOmpForClause") {
    return new SgOmpForClause();
  }
  if (kind == "SgOmpTaskgroupClause") {
    return new SgOmpTaskgroupClause();
  }
  if (kind == "SgOmpFullClause") {
    return new SgOmpFullClause();
  }
  if (kind == "SgOmpInbranchClause") {
    return new SgOmpInbranchClause();
  }
  if (kind == "SgOmpNocontextClause") {
    return new SgOmpNocontextClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpFilterClause") {
    return new SgOmpFilterClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNovariantsClause") {
    return new SgOmpNovariantsClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPartialClause") {
    return new SgOmpPartialClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpBeginClause") {
    return new SgOmpBeginClause();
  }
  if (kind == "SgOmpEndClause") {
    return new SgOmpEndClause();
  }
  if (kind == "SgOmpReadClause") {
    return new SgOmpReadClause();
  }
  if (kind == "SgOmpWriteClause") {
    return new SgOmpWriteClause();
  }
  if (kind == "SgOmpUpdateClause") {
    return new SgOmpUpdateClause();
  }
  if (kind == "SgOmpCaptureClause") {
    return new SgOmpCaptureClause();
  }
  if (kind == "SgOmpCompareClause") {
    return new SgOmpCompareClause();
  }
  if (kind == "SgOmpSeqCstClause") {
    return new SgOmpSeqCstClause();
  }
  if (kind == "SgOmpAcqRelClause") {
    return new SgOmpAcqRelClause();
  }
  if (kind == "SgOmpReleaseClause") {
    return new SgOmpReleaseClause();
  }
  if (kind == "SgOmpAcquireClause") {
    return new SgOmpAcquireClause();
  }
  if (kind == "SgOmpRelaxedClause") {
    return new SgOmpRelaxedClause();
  }
  if (kind == "SgOmpFailClause") {
    return new SgOmpFailClause(
        requiredEnum<SgOmpClause::omp_fail_memory_order_kind_enum>(
            p, "memory_order", "SgOmpFailClause",
            {SgOmpClause::e_omp_fail_memory_order_kind_seq_cst,
             SgOmpClause::e_omp_fail_memory_order_kind_acquire,
             SgOmpClause::e_omp_fail_memory_order_kind_relaxed}));
  }
  if (kind == "SgOmpCopyprivateClause") {
    return new SgOmpCopyprivateClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpAllocateClause") {
    SgOmpAllocateClause *clause = new SgOmpAllocateClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_allocate_modifier_enum>(
            p, "modifier", "SgOmpAllocateClause",
            {SgOmpClause::e_omp_allocate_modifier_unknown,
             SgOmpClause::e_omp_allocate_default_mem_alloc,
             SgOmpClause::e_omp_allocate_large_cap_mem_alloc,
             SgOmpClause::e_omp_allocate_const_mem_alloc,
             SgOmpClause::e_omp_allocate_high_bw_mem_alloc,
             SgOmpClause::e_omp_allocate_low_lat_mem_alloc,
             SgOmpClause::e_omp_allocate_cgroup_mem_alloc,
             SgOmpClause::e_omp_allocate_pteam_mem_alloc,
             SgOmpClause::e_omp_allocate_thread_mem_alloc,
             SgOmpClause::e_omp_allocate_user_defined_modifier}),
        static_cast<SgExpression *>(nullptr));
    clause->set_uses_allocator_modifier_syntax(
        p.requiredBool("uses_allocator_modifier_syntax"));
    return clause;
  }
  if (kind == "SgOmpAllocatorClause") {
    return new SgOmpAllocatorClause(
        requiredEnum<SgOmpClause::omp_allocator_modifier_enum>(
            p, "modifier", "SgOmpAllocatorClause",
            {SgOmpClause::e_omp_allocator_modifier_unknown,
             SgOmpClause::e_omp_allocator_default_mem_alloc,
             SgOmpClause::e_omp_allocator_large_cap_mem_alloc,
             SgOmpClause::e_omp_allocator_const_mem_alloc,
             SgOmpClause::e_omp_allocator_high_bw_mem_alloc,
             SgOmpClause::e_omp_allocator_low_lat_mem_alloc,
             SgOmpClause::e_omp_allocator_cgroup_mem_alloc,
             SgOmpClause::e_omp_allocator_pteam_mem_alloc,
             SgOmpClause::e_omp_allocator_thread_mem_alloc,
             SgOmpClause::e_omp_allocator_user_defined_modifier}),
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpAdjustArgsClause") {
    return new SgOmpAdjustArgsClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_adjust_args_modifier_enum>(
            p, "modifier", "SgOmpAdjustArgsClause",
            {SgOmpClause::e_omp_adjust_args_modifier_need_device_addr,
             SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr,
             SgOmpClause::e_omp_adjust_args_modifier_nothing}));
  }
  if (kind == "SgOmpAppendArgsClause") {
    return new SgOmpAppendArgsClause();
  }
  if (kind == "SgOmpInReductionClause") {
    return new SgOmpInReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_in_reduction_identifier_enum>(
            p, "identifier", "SgOmpInReductionClause",
            {SgOmpClause::e_omp_in_reduction_identifier_plus,
             SgOmpClause::e_omp_in_reduction_identifier_mul,
             SgOmpClause::e_omp_in_reduction_identifier_minus,
             SgOmpClause::e_omp_in_reduction_identifier_bitand,
             SgOmpClause::e_omp_in_reduction_identifier_bitor,
             SgOmpClause::e_omp_in_reduction_identifier_bitxor,
             SgOmpClause::e_omp_in_reduction_identifier_logand,
             SgOmpClause::e_omp_in_reduction_identifier_logor,
             SgOmpClause::e_omp_in_reduction_identifier_and,
             SgOmpClause::e_omp_in_reduction_identifier_or,
             SgOmpClause::e_omp_in_reduction_identifier_eqv,
             SgOmpClause::e_omp_in_reduction_identifier_neqv,
             SgOmpClause::e_omp_in_reduction_identifier_max,
             SgOmpClause::e_omp_in_reduction_identifier_min,
             SgOmpClause::e_omp_in_reduction_identifier_iand,
             SgOmpClause::e_omp_in_reduction_identifier_ior,
             SgOmpClause::e_omp_in_reduction_identifier_ieor,
             SgOmpClause::e_omp_in_reduction_user_defined_identifier}),
        static_cast<SgOmpNameExpression *>(nullptr));
  }
  if (kind == "SgOmpTaskReductionClause") {
    return new SgOmpTaskReductionClause(
        static_cast<SgExprListExp *>(nullptr),
        requiredEnum<SgOmpClause::omp_task_reduction_identifier_enum>(
            p, "identifier", "SgOmpTaskReductionClause",
            {SgOmpClause::e_omp_task_reduction_identifier_plus,
             SgOmpClause::e_omp_task_reduction_identifier_mul,
             SgOmpClause::e_omp_task_reduction_identifier_minus,
             SgOmpClause::e_omp_task_reduction_identifier_bitand,
             SgOmpClause::e_omp_task_reduction_identifier_bitor,
             SgOmpClause::e_omp_task_reduction_identifier_bitxor,
             SgOmpClause::e_omp_task_reduction_identifier_logand,
             SgOmpClause::e_omp_task_reduction_identifier_logor,
             SgOmpClause::e_omp_task_reduction_identifier_and,
             SgOmpClause::e_omp_task_reduction_identifier_or,
             SgOmpClause::e_omp_task_reduction_identifier_eqv,
             SgOmpClause::e_omp_task_reduction_identifier_neqv,
             SgOmpClause::e_omp_task_reduction_identifier_max,
             SgOmpClause::e_omp_task_reduction_identifier_min,
             SgOmpClause::e_omp_task_reduction_identifier_iand,
             SgOmpClause::e_omp_task_reduction_identifier_ior,
             SgOmpClause::e_omp_task_reduction_identifier_ieor,
             SgOmpClause::e_omp_task_reduction_user_defined_identifier}),
        static_cast<SgOmpNameExpression *>(nullptr));
  }
  if (kind == "SgOmpUniformClause") {
    return new SgOmpUniformClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpHintClause") {
    return new SgOmpHintClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNogroupClause") {
    return new SgOmpNogroupClause();
  }
  if (kind == "SgOmpUntiedClause") {
    return new SgOmpUntiedClause();
  }
  if (kind == "SgOmpMergeableClause") {
    return new SgOmpMergeableClause();
  }
  if (kind == "SgOmpReverseOffloadClause") {
    return new SgOmpReverseOffloadClause();
  }
  if (kind == "SgOmpUnifiedAddressClause") {
    return new SgOmpUnifiedAddressClause();
  }
  if (kind == "SgOmpUnifiedSharedMemoryClause") {
    return new SgOmpUnifiedSharedMemoryClause();
  }
  if (kind == "SgOmpDynamicAllocatorsClause") {
    return new SgOmpDynamicAllocatorsClause();
  }
  if (kind == "SgOmpDepobjUpdateClause") {
    return new SgOmpDepobjUpdateClause(
        requiredEnum<SgOmpClause::omp_depobj_modifier_enum>(
            p, "modifier", "SgOmpDepobjUpdateClause",
            {SgOmpClause::e_omp_depobj_modifier_in,
             SgOmpClause::e_omp_depobj_modifier_out,
             SgOmpClause::e_omp_depobj_modifier_inout,
             SgOmpClause::e_omp_depobj_modifier_mutexinoutset,
             SgOmpClause::e_omp_depobj_modifier_depobj,
             SgOmpClause::e_omp_depobj_modifier_sink,
             SgOmpClause::e_omp_depobj_modifier_source}));
  }
  if (kind == "SgOmpDestroyClause") {
    return new SgOmpDestroyClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpThreadLimitClause") {
    return new SgOmpThreadLimitClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpUsesAllocatorsClause") {
    return new SgOmpUsesAllocatorsClause();
  }
  if (kind == "SgOmpUsesAllocatorsDefination") {
    SgOmpUsesAllocatorsDefination *definition =
        new SgOmpUsesAllocatorsDefination();
    definition->set_user_defined_allocator(nullptr);
    definition->set_allocator_traits_array(nullptr);
    return definition;
  }
  if (kind == "SgOmpContextSelectorSet") {
    return new SgOmpContextSelectorSet(
        requiredEnum<SgOmpClause::omp_context_selector_set_kind_enum>(
            p, "set_kind", "SgOmpContextSelectorSet",
            {SgOmpClause::e_omp_context_selector_set_user,
             SgOmpClause::e_omp_context_selector_set_construct,
             SgOmpClause::e_omp_context_selector_set_device,
             SgOmpClause::e_omp_context_selector_set_target_device,
             SgOmpClause::e_omp_context_selector_set_implementation},
            "set kind"));
  }
  if (kind == "SgOmpContextSelectorProperty") {
    SgOmpContextSelectorProperty *property = new SgOmpContextSelectorProperty();
    property->set_context_kind(
        requiredEnum<SgOmpClause::omp_when_context_kind_enum>(
            p, "context_kind", "SgOmpContextSelectorProperty",
            {SgOmpClause::e_omp_when_context_kind_unknown,
             SgOmpClause::e_omp_when_context_kind_host,
             SgOmpClause::e_omp_when_context_kind_nohost,
             SgOmpClause::e_omp_when_context_kind_any,
             SgOmpClause::e_omp_when_context_kind_cpu,
             SgOmpClause::e_omp_when_context_kind_gpu,
             SgOmpClause::e_omp_when_context_kind_fpga},
            "context kind"));
    property->set_context_vendor(
        requiredEnum<SgOmpClause::omp_when_context_vendor_enum>(
            p, "context_vendor", "SgOmpContextSelectorProperty",
            {SgOmpClause::e_omp_when_context_vendor_unspecified,
             SgOmpClause::e_omp_when_context_vendor_amd,
             SgOmpClause::e_omp_when_context_vendor_arm,
             SgOmpClause::e_omp_when_context_vendor_bsc,
             SgOmpClause::e_omp_when_context_vendor_cray,
             SgOmpClause::e_omp_when_context_vendor_fujitsu,
             SgOmpClause::e_omp_when_context_vendor_gnu,
             SgOmpClause::e_omp_when_context_vendor_ibm,
             SgOmpClause::e_omp_when_context_vendor_intel,
             SgOmpClause::e_omp_when_context_vendor_llvm,
             SgOmpClause::e_omp_when_context_vendor_nvidia,
             SgOmpClause::e_omp_when_context_vendor_pgi,
             SgOmpClause::e_omp_when_context_vendor_ti,
             SgOmpClause::e_omp_when_context_vendor_user,
             SgOmpClause::e_omp_when_context_vendor_unknown}));
    property->set_atomic_default_mem_order(
        requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
            p, "atomic_default_mem_order", "SgOmpContextSelectorProperty",
            {SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed},
            "atomic_default_mem_order kind"));
    property->set_requires_kind(
        requiredEnum<SgOmpClause::omp_requires_property_kind_enum>(
            p, "requires_kind", "SgOmpContextSelectorProperty",
            {SgOmpClause::e_omp_requires_property_unspecified,
             SgOmpClause::e_omp_requires_property_reverse_offload,
             SgOmpClause::e_omp_requires_property_unified_address,
             SgOmpClause::e_omp_requires_property_unified_shared_memory,
             SgOmpClause::e_omp_requires_property_dynamic_allocators,
             SgOmpClause::e_omp_requires_property_self_maps,
             SgOmpClause::e_omp_requires_property_device_safesync,
             SgOmpClause::e_omp_requires_property_atomic_default_mem_order,
             SgOmpClause::e_omp_requires_property_implementation_defined},
            "requires kind"));
    property->set_requires_atomic_default_mem_order(
        requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
            p, "requires_atomic_default_mem_order",
            "SgOmpContextSelectorProperty",
            {SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed},
            "requires atomic_default_mem_order kind"));
    property->set_requires_extension(
        SgName(p.requiredString("requires_extension")));
    return property;
  }
  if (kind == "SgOmpContextSelector") {
    SgOmpContextSelector *selector = new SgOmpContextSelector(
        requiredEnum<SgOmpClause::omp_context_trait_selector_kind_enum>(
            p, "selector_kind", "SgOmpContextSelector",
            {SgOmpClause::e_omp_context_trait_condition,
             SgOmpClause::e_omp_context_trait_construct,
             SgOmpClause::e_omp_context_trait_kind,
             SgOmpClause::e_omp_context_trait_arch,
             SgOmpClause::e_omp_context_trait_isa,
             SgOmpClause::e_omp_context_trait_device_num,
             SgOmpClause::e_omp_context_trait_uid,
             SgOmpClause::e_omp_context_trait_vendor,
             SgOmpClause::e_omp_context_trait_extension,
             SgOmpClause::e_omp_context_trait_requires,
             SgOmpClause::e_omp_context_trait_atomic_default_mem_order,
             SgOmpClause::e_omp_context_trait_implementation_user}));
    selector->set_implementation_defined_name(
        SgName(p.requiredString("implementation_defined_name")));
    return selector;
  }
  if (kind == "SgOmpWhenClause") {
    return new SgOmpWhenClause(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMatchClause") {
    return new SgOmpMatchClause();
  }
  if (kind == "SgOmpNontemporalClause") {
    return new SgOmpNontemporalClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpIsDevicePtrClause") {
    return new SgOmpIsDevicePtrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpUseDevicePtrClause") {
    return new SgOmpUseDevicePtrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpUseDeviceAddrClause") {
    return new SgOmpUseDeviceAddrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpHasDeviceAddrClause") {
    return new SgOmpHasDeviceAddrClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpDetachClause") {
    return new SgOmpDetachClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNumTasksClause") {
    return new SgOmpNumTasksClause(
        static_cast<SgExpression *>(nullptr),
        requiredEnum<SgOmpClause::omp_num_tasks_modifier_enum>(
            p, "modifier", "SgOmpNumTasksClause",
            {SgOmpClause::e_omp_num_tasks_modifier_unspecified,
             SgOmpClause::e_omp_num_tasks_modifier_strict}));
  }
  if (kind == "SgOmpGrainsizeClause") {
    return new SgOmpGrainsizeClause(
        static_cast<SgExpression *>(nullptr),
        requiredEnum<SgOmpClause::omp_grainsize_modifier_enum>(
            p, "modifier", "SgOmpGrainsizeClause",
            {SgOmpClause::e_omp_grainsize_modifier_unspecified,
             SgOmpClause::e_omp_grainsize_modifier_strict}));
  }
  if (kind == "SgOmpSizesClause") {
    return new SgOmpSizesClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpPriorityClause") {
    return new SgOmpPriorityClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpFinalClause") {
    return new SgOmpFinalClause(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpNotinbranchClause") {
    return new SgOmpNotinbranchClause();
  }
  if (kind == "SgOmpExclusiveClause") {
    return new SgOmpExclusiveClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpInclusiveClause") {
    return new SgOmpInclusiveClause(static_cast<SgExprListExp *>(nullptr));
  }
  if (kind == "SgOmpExtImplementationDefinedRequirementClause") {
    return new SgOmpExtImplementationDefinedRequirementClause(
        static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpDeviceClause") {
    return new SgOmpDeviceClause(
        static_cast<SgExpression *>(nullptr),
        requiredEnum<SgOmpClause::omp_device_modifier_enum>(
            p, "modifier", "SgOmpDeviceClause",
            {SgOmpClause::e_omp_device_modifier_unspecified,
             SgOmpClause::e_omp_device_modifier_ancestor,
             SgOmpClause::e_omp_device_modifier_device_num}));
  }
  if (kind == "SgOmpRequiresStatement") {
    return new SgOmpRequiresStatement();
  }
  if (kind == "SgOmpBarrierStatement") {
    return new SgOmpBarrierStatement();
  }
  if (kind == "SgOmpNothingStatement") {
    return new SgOmpNothingStatement();
  }
  if (kind == "SgOmpTaskwaitStatement") {
    return new SgOmpTaskwaitStatement();
  }
  if (kind == "SgOmpOrderedDependStatement") {
    return new SgOmpOrderedDependStatement();
  }
  if (kind == "SgOmpErrorStatement") {
    return new SgOmpErrorStatement();
  }
  if (kind == "SgOmpInteropStatement") {
    return new SgOmpInteropStatement();
  }
  if (kind == "SgOmpScanStatement") {
    return new SgOmpScanStatement();
  }
  if (kind == "SgOmpFlushStatement") {
    return new SgOmpFlushStatement();
  }
  if (kind == "SgOmpDeclareSimdStatement") {
    throw std::runtime_error(
        "AST JSON SgOmpDeclareSimdStatement requires delayed exact-target "
        "construction");
  }
  if (kind == "SgOmpDeclareMapperStatement") {
    SgOmpDeclareMapperStatement *stmt = new SgOmpDeclareMapperStatement();
    const int64_t identifierValue = p.requiredInt("identifier");
    if (identifierValue <
            SgOmpClause::e_omp_declare_mapper_identifier_unspecified ||
        identifierValue > SgOmpClause::e_omp_declare_mapper_identifier_user) {
      throw std::runtime_error(
          "AST JSON declare mapper identifier kind is invalid");
    }
    stmt->set_identifier(
        static_cast<SgOmpClause::omp_declare_mapper_identifier_enum>(
            identifierValue));
    stmt->set_identifier_is_explicit(p.requiredBool("identifier_is_explicit"));
    return stmt;
  }
  if (kind == "SgOmpDeclareTargetStatement") {
    SgOmpDeclareTargetStatement *stmt = new SgOmpDeclareTargetStatement();
    stmt->set_device_type_kind(
        ompDeviceTypeKindFromJson(p, "SgOmpDeclareTargetStatement"));
    stmt->set_use_underscore_spelling(
        p.requiredBool("use_underscore_spelling"));
    return stmt;
  }
  if (kind == "SgOmpBeginDeclareTargetStatement") {
    SgOmpBeginDeclareTargetStatement *stmt =
        new SgOmpBeginDeclareTargetStatement();
    stmt->set_use_underscore_spelling(
        p.requiredBool("use_underscore_spelling"));
    return stmt;
  }
  if (kind == "SgOmpEndDeclareTargetStatement") {
    SgOmpEndDeclareTargetStatement *stmt = new SgOmpEndDeclareTargetStatement();
    stmt->set_use_underscore_spelling(
        p.requiredBool("use_underscore_spelling"));
    return stmt;
  }
  if (kind == "SgOmpAssumesStatement") {
    return new SgOmpAssumesStatement();
  }
  if (kind == "SgOmpBeginAssumesStatement") {
    return new SgOmpBeginAssumesStatement();
  }
  if (kind == "SgOmpEndAssumesStatement") {
    return new SgOmpEndAssumesStatement();
  }
  if (kind == "SgOmpEndAssumeStatement") {
    return new SgOmpEndAssumeStatement();
  }
  if (kind == "SgOmpGroupprivateStatement") {
    SgOmpGroupprivateStatement *stmt =
        new SgOmpGroupprivateStatement(static_cast<SgExprListExp *>(nullptr));
    stmt->set_device_type_kind(
        ompDeviceTypeKindFromJson(p, "SgOmpGroupprivateStatement"));
    return stmt;
  }
  if (kind == "SgOmpDeclareVariantStatement") {
    throw std::runtime_error(
        "AST JSON SgOmpDeclareVariantStatement requires delayed exact-target "
        "construction");
  }
  if (kind == "SgOmpBeginDeclareVariantStatement") {
    return new SgOmpBeginDeclareVariantStatement();
  }
  if (kind == "SgOmpEndDeclareVariantStatement") {
    return new SgOmpEndDeclareVariantStatement();
  }
  if (kind == "SgOmpThreadprivateStatement") {
    return new SgOmpThreadprivateStatement();
  }
  if (kind == "SgOmpAllocateStatement") {
    return new SgOmpAllocateStatement();
  }
  if (kind == "SgOmpTargetUpdateStatement") {
    return new SgOmpTargetUpdateStatement();
  }
  if (kind == "SgOmpTargetEnterDataStatement") {
    return new SgOmpTargetEnterDataStatement();
  }
  if (kind == "SgOmpTargetExitDataStatement") {
    return new SgOmpTargetExitDataStatement();
  }
  if (kind == "SgOmpTargetStatement") {
    return new SgOmpTargetStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetDataStatement") {
    return new SgOmpTargetDataStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetDataCompositeStatement") {
    return new SgOmpTargetDataCompositeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpScopeStatement") {
    return new SgOmpScopeStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMaskedStatement") {
    return new SgOmpParallelMaskedStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpAssumeStatement") {
    return new SgOmpAssumeStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskgraphStatement") {
    return new SgOmpTaskgraphStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpFuseStatement") {
    return new SgOmpFuseStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpInterchangeStatement") {
    return new SgOmpInterchangeStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpReverseStatement") {
    return new SgOmpReverseStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelStatement") {
    return new SgOmpParallelStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpForStatement") {
    return new SgOmpForStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDoStatement") {
    return new SgOmpDoStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpForSimdStatement") {
    return new SgOmpForSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpLoopStatement") {
    return new SgOmpLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSimdStatement") {
    return new SgOmpSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSingleStatement") {
    return new SgOmpSingleStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskStatement") {
    return new SgOmpTaskStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterStatement") {
    return new SgOmpMasterStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMaskedStatement") {
    return new SgOmpMaskedStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMaskedTaskloopStatement") {
    return new SgOmpMaskedTaskloopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMaskedTaskloopSimdStatement") {
    return new SgOmpMaskedTaskloopSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterStatement") {
    return new SgOmpParallelMasterStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterTaskloopStatement") {
    return new SgOmpMasterTaskloopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpMasterTaskloopSimdStatement") {
    return new SgOmpMasterTaskloopSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterTaskloopStatement") {
    return new SgOmpParallelMasterTaskloopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelMasterTaskloopSimdStatement") {
    return new SgOmpParallelMasterTaskloopSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSectionStatement") {
    return new SgOmpSectionStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpSectionsStatement") {
    return new SgOmpSectionsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpCriticalStatement") {
    return new SgOmpCriticalStatement(static_cast<SgStatement *>(nullptr),
                                      SgName(p.requiredString("name")));
  }
  if (kind == "SgOmpAtomicStatement") {
    return new SgOmpAtomicStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpOrderedStatement") {
    return new SgOmpOrderedStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpParallelLoopStatement") {
    return new SgOmpParallelLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskgroupStatement") {
    return new SgOmpTaskgroupStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskloopStatement") {
    return new SgOmpTaskloopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskloopSimdStatement") {
    return new SgOmpTaskloopSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTaskyieldStatement") {
    return new SgOmpTaskyieldStatement();
  }
  if (kind == "SgOmpCancelStatement") {
    return new SgOmpCancelStatement();
  }
  if (kind == "SgOmpCancellationPointStatement") {
    return new SgOmpCancellationPointStatement();
  }
  if (kind == "SgOmpDepobjStatement") {
    return new SgOmpDepobjStatement(static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpMetadirectiveStatement") {
    return new SgOmpMetadirectiveStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDispatchStatement") {
    return new SgOmpDispatchStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeStatement") {
    return new SgOmpDistributeStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpWorkdistributeStatement") {
    return new SgOmpWorkdistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeSimdStatement") {
    return new SgOmpDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeParallelForStatement") {
    return new SgOmpDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpDistributeParallelForSimdStatement") {
    return new SgOmpDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelStatement") {
    return new SgOmpTargetParallelStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelForStatement") {
    return new SgOmpTargetParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelForSimdStatement") {
    return new SgOmpTargetParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetParallelLoopStatement") {
    return new SgOmpTargetParallelLoopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetSimdStatement") {
    return new SgOmpTargetSimdStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsStatement") {
    return new SgOmpTargetTeamsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsStatement") {
    return new SgOmpTeamsStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeStatement") {
    return new SgOmpTeamsDistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeSimdStatement") {
    return new SgOmpTeamsDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeParallelForStatement") {
    return new SgOmpTeamsDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsDistributeParallelForSimdStatement") {
    return new SgOmpTeamsDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTeamsLoopStatement") {
    return new SgOmpTeamsLoopStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpWorkshareStatement") {
    return new SgOmpWorkshareStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpUnrollStatement") {
    return new SgOmpUnrollStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTileStatement") {
    return new SgOmpTileStatement(static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeStatement") {
    return new SgOmpTargetTeamsDistributeStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeSimdStatement") {
    return new SgOmpTargetTeamsDistributeSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsLoopStatement") {
    return new SgOmpTargetTeamsLoopStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForStatement") {
    return new SgOmpTargetTeamsDistributeParallelForStatement(
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForSimdStatement") {
    return new SgOmpTargetTeamsDistributeParallelForSimdStatement(
        static_cast<SgStatement *>(nullptr));
  }

  throw std::runtime_error("AST JSON deserializer does not support Sage node " +
                           kind);
}

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const JsonValue &metadata) {
  SgNode *node = createNodeFromRecordImpl(record, project, metadata);
  if (SgStatement *statement = isSgStatement(node)) {
    const int64_t source_sequence_value =
        record.properties.requiredInt("source_sequence_value");
    if (source_sequence_value < std::numeric_limits<int>::min() ||
        source_sequence_value > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON SgStatement source sequence value is out of range");
    }
    statement->set_source_sequence_value(
        static_cast<int>(source_sequence_value));
    statement->set_directive_end_kind(
        requiredEnum<SgStatement::directive_end_kind_enum>(
            record.properties, "directive_end_kind", "SgStatement",
            {SgStatement::e_directive_end_not_applicable,
             SgStatement::e_directive_end_implicit,
             SgStatement::e_directive_end_explicit},
            "directive-end kind"));
    statement->set_omp_fortran_spelling(
        requiredEnum<SgStatement::omp_fortran_spelling_enum>(
            record.properties, "omp_fortran_spelling", "SgStatement",
            {SgStatement::e_omp_fortran_spelling_not_applicable,
             SgStatement::e_omp_fortran_spelling_do},
            "OpenMP Fortran spelling"));
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_enum_constant_source_ownership(
        requiredEnum<SgInitializedName::enum_constant_source_ownership_enum>(
            record.properties, "enum_constant_source_ownership",
            "SgInitializedName",
            {SgInitializedName::e_enum_constant_source_unclassified,
             SgInitializedName::e_enum_constant_source_body,
             SgInitializedName::e_enum_constant_source_external,
             SgInitializedName::e_enum_constant_semantic_only}));
    name->set_preinitialization(
        requiredPreinitialization(record.properties, "SgInitializedName"));
  }
  if (SgOmpClause *clause = isSgOmpClause(node)) {
    const JsonValue &source_order =
        record.properties.at("combined_source_order");
    if (source_order.kind != JsonValue::Kind::Null) {
      const int64_t value = source_order.asInt();
      if (value < 0 || static_cast<uint64_t>(value) >
                           std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "AST JSON OpenMP combined clause source order is out of range");
      }
      clause->initialize_combined_source_order(static_cast<std::size_t>(value));
    }
  }
  if (SgOmpBodyStatement *statement = isSgOmpBodyStatement(node)) {
    statement->set_source_form_is_combined(
        record.properties.requiredBool("source_form_is_combined"));
  }
  if (SgDeclarationStatement *declaration = isSgDeclarationStatement(node)) {
    restoreTranslationUnitSourceOrder(declaration, record.properties,
                                      "SgDeclarationStatement");
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(node)) {
    const auto ownership =
        requiredEnum<SgFunctionDeclaration::frontend_source_ownership_enum>(
            record.properties, "frontend_source_ownership",
            "SgFunctionDeclaration",
            {SgFunctionDeclaration::e_frontend_source_unclassified,
             SgFunctionDeclaration::e_frontend_source_main_file,
             SgFunctionDeclaration::e_frontend_source_application_header,
             SgFunctionDeclaration::e_frontend_source_system_header,
             SgFunctionDeclaration::e_frontend_source_external,
             SgFunctionDeclaration::e_frontend_source_support,
             SgFunctionDeclaration::e_frontend_source_pseudo_file,
             SgFunctionDeclaration::e_frontend_source_implicit});
    if (ownership != SgFunctionDeclaration::e_frontend_source_unclassified) {
      function->initialize_frontend_source_ownership(ownership);
    }
    const auto origin =
        requiredEnum<SgFunctionDeclaration::frontend_declaration_origin_enum>(
            record.properties, "frontend_declaration_origin",
            "SgFunctionDeclaration",
            {SgFunctionDeclaration::e_frontend_declaration_unclassified,
             SgFunctionDeclaration::e_frontend_declaration_explicit,
             SgFunctionDeclaration::e_frontend_declaration_implicit});
    if (origin != SgFunctionDeclaration::e_frontend_declaration_unclassified) {
      function->initialize_frontend_declaration_origin(origin);
    }
  }
  return node;
}

std::vector<EdgeRecord> edgesFor(const NodeRecord &record,
                                 const std::string &field) {
  std::vector<EdgeRecord> result;
  for (const EdgeRecord &edge : record.edges) {
    if (edge.field == field) {
      result.push_back(edge);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const EdgeRecord &a, const EdgeRecord &b) {
              return a.index < b.index;
            });
  return result;
}

uint64_t singleEdgeTarget(const NodeRecord &record, const std::string &field) {
  std::vector<EdgeRecord> edges = edgesFor(record, field);
  if (edges.size() > 1) {
    throw std::runtime_error("AST JSON node " + std::to_string(record.id) +
                             " has duplicate singleton edge '" + field + "'");
  }
  return edges.empty() ? 0 : edges.front().target;
}

uint64_t requiredSingleEdgeTarget(const NodeRecord &record,
                                  const std::string &field) {
  const uint64_t target = singleEdgeTarget(record, field);
  if (target == 0) {
    throw std::runtime_error("AST JSON node " + std::to_string(record.id) +
                             " is missing required edge '" + field + "'");
  }
  return target;
}

SgBitVector bitVectorFromJson(const JsonValue &json,
                              const std::string &field_name) {
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + field_name +
                             " field is not a bool array");
  }
  SgBitVector bits;
  bits.reserve(json.array.size());
  for (const JsonValue &value : json.array) {
    if (value.kind != JsonValue::Kind::Bool) {
      throw std::runtime_error("AST JSON " + field_name +
                               " contains a non-bool value");
    }
    bits.push_back(value.asBool());
  }
  return bits;
}

SgModuleStatement *externalModuleFromJson(const JsonValue &json) {
  if (!json.requiredBool("present")) {
    return nullptr;
  }
  if (requiredTranslationUnitSourceOrder(json, "external_module").has_value()) {
    throw std::runtime_error(
        "AST JSON external_module must be semantic-only and unordered");
  }
  const std::string name = json.at("name").asString();
  if (name.empty()) {
    throw std::runtime_error("AST JSON external_module is missing module name");
  }
  SgModuleStatement *module = new SgModuleStatement(
      SgName(name), requiredClassType(json, "external_module"), nullptr,
      nullptr);
  installTransformationSourcePosition(module);
  markAstJsonExternalModule(module, json.requiredString("source_file"));
  return module;
}

const std::vector<SgFunctionDeclaration *> &
externalFunctionDeclarationsForSource(SgSourceFile *source) {
  if (source == nullptr) {
    throw std::runtime_error(
        "AST JSON external function search has no source file");
  }
  auto found = externalFunctionDeclarationsBySource.find(source);
  if (found != externalFunctionDeclarationsBySource.end()) {
    return found->second;
  }

  std::vector<SgFunctionDeclaration *> declarations;
  std::unordered_set<SgFunctionDeclaration *> seen;
  auto add_candidate = [&](SgFunctionDeclaration *declaration) {
    if (declaration != nullptr && seen.insert(declaration).second) {
      declarations.push_back(declaration);
    }
  };
  RoseAst ast(source);
  for (RoseAst::iterator node = ast.begin().withoutNullValues();
       node != ast.end(); ++node) {
    add_candidate(isSgFunctionDeclaration(*node));
    SgScopeStatement *scope = isSgScopeStatement(*node);
    SgSymbolTable *table =
        scope != nullptr ? scope->get_symbol_table() : nullptr;
    if (table == nullptr || table->get_table() == nullptr) {
      continue;
    }
    for (const auto &entry : *table->get_table()) {
      add_candidate(isSgFunctionDeclaration(
          const_cast<SgNode *>(symbolBasis(entry.second))));
      if (SgRenameSymbol *rename = isSgRenameSymbol(entry.second)) {
        add_candidate(isSgFunctionDeclaration(
            const_cast<SgNode *>(symbolBasis(rename->get_original_symbol()))));
      }
    }
  }
  auto inserted = externalFunctionDeclarationsBySource.emplace(
      source, std::move(declarations));
  if (!inserted.second) {
    throw std::runtime_error(
        "AST JSON external function source cache insertion failed");
  }
  return inserted.first->second;
}

bool externalFunctionLocationMatches(SgFunctionDeclaration *candidate,
                                     const JsonValue &json) {
  Sg_File_Info *candidate_start =
      candidate != nullptr ? candidate->get_startOfConstruct() : nullptr;
  Sg_File_Info *candidate_end =
      candidate != nullptr ? candidate->get_endOfConstruct() : nullptr;
  const JsonValue &location = json.at("location");
  const JsonValue &start = location.at("start");
  const JsonValue &end = location.at("end");
  if (candidate_start == nullptr || candidate_end == nullptr ||
      !start.requiredBool("present") || !end.requiredBool("present")) {
    return false;
  }
  return sourceFileMatchesExternalRecord(
             SageInterface::getEnclosingSourceFile(candidate, true),
             json.requiredString("source_file")) &&
         sameAstJsonPath(candidate_start->get_filenameString(),
                         start.requiredString("filename")) &&
         candidate_start->get_line() == start.requiredInt("line") &&
         candidate_start->get_col() == start.requiredInt("column") &&
         sameAstJsonPath(candidate_end->get_filenameString(),
                         end.requiredString("filename")) &&
         candidate_end->get_line() == end.requiredInt("line") &&
         candidate_end->get_col() == end.requiredInt("column");
}

bool externalFunctionTypeMatches(SgFunctionDeclaration *candidate,
                                 SgFunctionType *function_type) {
  SgFunctionType *candidate_type =
      candidate != nullptr ? candidate->get_type() : nullptr;
  if (candidate_type == nullptr || function_type == nullptr) {
    return false;
  }
  return candidate_type == function_type ||
         candidate_type->get_mangled() == function_type->get_mangled();
}

bool externalFunctionCandidateMatches(SgFunctionDeclaration *candidate,
                                      SgFunctionType *function_type,
                                      const JsonValue &json) {
  const std::string omp_declare_variant_source_name =
      json.requiredString("omp_declare_variant_source_name");
  const std::optional<unsigned int> omp_declare_variant_region_ordinal =
      requiredOmpDeclareVariantRegionOrdinal(json, json.requiredString("name"),
                                             "external function candidate");
  if (candidate == nullptr || function_type == nullptr ||
      candidate->sage_class_name() != json.requiredString("kind") ||
      candidate->get_name().getString() != json.requiredString("name") ||
      candidate->get_omp_declare_variant_source_name().getString() !=
          omp_declare_variant_source_name ||
      candidate->get_omp_declare_variant_region_ordinal() !=
          omp_declare_variant_region_ordinal ||
      !externalFunctionLocationMatches(candidate, json) ||
      !externalFunctionTypeMatches(candidate, function_type) ||
      candidate->get_source_name_parenthesized_for_macro() !=
          json.requiredBool("source_name_parenthesized_for_macro") ||
      candidate->get_source_declarator_uses_wrapped_function_type() !=
          json.requiredBool("source_declarator_uses_wrapped_function_type") ||
      candidate->get_type_syntax_is_available() !=
          json.requiredBool("type_syntax_is_available")) {
    return false;
  }
  SgFunctionParameterList *parameters = candidate->get_parameterList();
  const JsonValue &serialized_parameters = json.at("parameters");
  if (parameters == nullptr ||
      serialized_parameters.kind != JsonValue::Kind::Array ||
      parameters->get_args().size() != serialized_parameters.array.size()) {
    return false;
  }
  if (SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(candidate)) {
    return procedure->get_subprogram_kind() ==
               subprogramKindFromJson(json, "external function candidate") &&
           procedure->get_block_data_name_kind() ==
               blockDataNameKindFromJson(json, "external function candidate") &&
           procedure->get_fortran_procedure_source_form() ==
               procedureSourceFormFromJson(json,
                                           "external function candidate") &&
           procedure->get_fortran_result_type_spec() ==
               procedureResultTypeSpecFromJson(json,
                                               "external function candidate");
  }
  return true;
}

SgFunctionDeclaration *
loadedExternalFunctionFromJson(const JsonValue &json,
                               SgFunctionType *function_type) {
  const std::string source_file = json.requiredString("source_file");
  const JsonValue &serialized_start = json.at("location").at("start");
  const bool names_loaded_source =
      serialized_start.requiredBool("present") &&
      (sameAstJsonPath(serialized_start.requiredString("filename"),
                       source_file) ||
       sameAstJsonPath(serialized_start.requiredString("physical_filename"),
                       source_file));
  if (!names_loaded_source) {
    return nullptr;
  }
  bool saw_source_file = false;
  SgFunctionDeclaration *matched = nullptr;
  std::ostringstream rejected_candidates;
  for (SgSourceFile *source : currentDeserializationSourceFiles()) {
    if (!sourceFileMatchesExternalRecord(source, source_file)) {
      continue;
    }
    saw_source_file = true;
    for (SgFunctionDeclaration *candidate :
         externalFunctionDeclarationsForSource(source)) {
      if (!externalFunctionCandidateMatches(candidate, function_type, json)) {
        if (candidate != nullptr &&
            candidate->sage_class_name() == json.requiredString("kind") &&
            candidate->get_name().getString() == json.requiredString("name")) {
          Sg_File_Info *start = candidate->get_startOfConstruct();
          Sg_File_Info *end = candidate->get_endOfConstruct();
          rejected_candidates
              << " candidate=" << candidate << " type=" << candidate->get_type()
              << " type-equivalent="
              << (SageInterface::isEquivalentType(candidate->get_type(),
                                                  function_type)
                      ? 1
                      : 0)
              << " semantic-type-identity="
              << (externalFunctionTypeMatches(candidate, function_type) ? 1 : 0)
              << " source-file="
              << SageInterface::getEnclosingSourceFile(candidate, true)
              << " start="
              << (start != nullptr ? start->get_filenameString() : "<null>")
              << ":" << (start != nullptr ? start->get_line() : -1) << ":"
              << (start != nullptr ? start->get_col() : -1) << " end="
              << (end != nullptr ? end->get_filenameString() : "<null>") << ":"
              << (end != nullptr ? end->get_line() : -1) << ":"
              << (end != nullptr ? end->get_col() : -1) << " source-form="
              << (isSgProcedureHeaderStatement(candidate) != nullptr
                      ? static_cast<int>(
                            isSgProcedureHeaderStatement(candidate)
                                ->get_fortran_procedure_source_form())
                      : -1)
              << " parameter-count="
              << (candidate->get_parameterList() != nullptr
                      ? candidate->get_parameterList()->get_args().size()
                      : 0)
              << " wrapped="
              << (candidate->get_source_declarator_uses_wrapped_function_type()
                      ? 1
                      : 0)
              << " syntax-available="
              << (candidate->get_type_syntax_is_available() ? 1 : 0);
        }
        continue;
      }
      if (matched != nullptr && matched != candidate) {
        throw std::runtime_error(
            "AST JSON external function is ambiguous in loaded source file " +
            source_file + ": " + json.requiredString("kind") + " " +
            json.requiredString("name"));
      }
      matched = candidate;
    }
  }
  if (saw_source_file && matched == nullptr) {
    throw std::runtime_error(
        "AST JSON external function was not found exactly in loaded source "
        "file " +
        source_file + ": " + json.requiredString("kind") + " " +
        json.requiredString("name") + rejected_candidates.str());
  }
  if (matched != nullptr) {
    SgFunctionDeclaration *canonical =
        isSgFunctionDeclaration(matched->get_firstNondefiningDeclaration());
    SgScopeStatement *scope =
        canonical != nullptr ? canonical->get_scope() : nullptr;
    SgFunctionSymbol *symbol =
        canonical != nullptr && scope != nullptr
            ? isSgFunctionSymbol(scope->find_symbol_from_declaration(canonical))
            : nullptr;
    if (canonical == nullptr || scope == nullptr || symbol == nullptr ||
        symbol->get_declaration() != canonical ||
        symbol->get_parent() != scope->get_symbol_table() ||
        !scope->get_symbol_table()->exists(symbol)) {
      throw std::runtime_error(
          "AST JSON loaded external function has no exact canonical symbol: " +
          json.requiredString("name"));
    }
  }
  return matched;
}

SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes) {
  if (!json.requiredBool("present")) {
    return nullptr;
  }
  if (!externalFunctionDeserializationIdentityActive) {
    throw std::runtime_error(
        "AST JSON external function was reconstructed outside its exact "
        "identity transaction");
  }
  if (requiredTranslationUnitSourceOrder(json, "external_function")
          .has_value()) {
    throw std::runtime_error(
        "AST JSON external_function must be semantic-only and unordered");
  }
  const std::string name = json.at("name").asString();
  const std::string source_file = json.at("source_file").asString();
  if (source_file.empty()) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " requires a non-empty source_file");
  }
  const uint64_t identity_hash = exactJsonValueHash(json);
  std::vector<ExternalFunctionDeserializationIdentity> &identity_bucket =
      externalFunctionDeserializationIdentities[identity_hash];
  for (const ExternalFunctionDeserializationIdentity &identity :
       identity_bucket) {
    if (identity.serialized_declaration == nullptr ||
        identity.declaration == nullptr) {
      throw std::runtime_error(
          "AST JSON external function identity cache is malformed");
    }
    if (identity.serialized_declaration->exactlyEquals(json)) {
      return identity.declaration;
    }
  }
  SgFunctionType *function_type =
      semanticFunctionTypeFromJson(json.at("function_type"), nodes);
  if (function_type == nullptr) {
    throw std::runtime_error(
        "AST JSON external_function function_type is not a SgFunctionType");
  }
  if (SgFunctionDeclaration *loaded =
          loadedExternalFunctionFromJson(json, function_type)) {
    identity_bucket.push_back({&json, loaded});
    return loaded;
  }
  SgFunctionType *function_type_syntax = isSgFunctionType(
      nullableTypeFromJson(json.at("function_type_syntax"), nodes));

  SgFunctionDeclaration *decl = nullptr;
  const std::string kind = json.at("kind").asString();
  if (kind == "SgProcedureHeaderStatement") {
    SgProcedureHeaderStatement *procedure =
        new SgProcedureHeaderStatement(SgName(name), function_type, nullptr);
    procedure->set_subprogram_kind(
        subprogramKindFromJson(json, "external function"));
    procedure->set_block_data_name_kind(
        blockDataNameKindFromJson(json, "external function"));
    procedure->set_fortran_procedure_source_form(
        procedureSourceFormFromJson(json, "external function"));
    procedure->set_fortran_result_type_spec(
        procedureResultTypeSpecFromJson(json, "external function"));
    decl = procedure;
  } else if (kind == "SgFunctionDeclaration") {
    if (name.empty()) {
      throw std::runtime_error("AST JSON external_function is missing name");
    }
    decl = new SgFunctionDeclaration(SgName(name), function_type, nullptr);
  } else {
    throw std::runtime_error(
        "AST JSON external_function has unsupported declaration kind: " + kind);
  }
  const std::string anonymous_symbol_key =
      json.requiredString("fortran_anonymous_program_unit_symbol_key");
  const bool requires_anonymous_symbol_key =
      isSgProcedureHeaderStatement(decl) != nullptr &&
      SageInterface::isFortranProgramUnitWithoutSourceName(decl);
  if (requires_anonymous_symbol_key != !anonymous_symbol_key.empty()) {
    throw std::runtime_error(
        "AST JSON external function anonymous program-unit symbol key "
        "disagrees with its typed source identity");
  }
  if (!anonymous_symbol_key.empty()) {
    decl->initialize_fortran_anonymous_program_unit_symbol_key(
        SgName(anonymous_symbol_key));
  }
  const std::string omp_declare_variant_source_name =
      json.requiredString("omp_declare_variant_source_name");
  const std::optional<unsigned int> omp_declare_variant_region_ordinal =
      requiredOmpDeclareVariantRegionOrdinal(json, decl->get_name().getString(),
                                             "external function " +
                                                 decl->get_name().getString());
  decl->set_omp_declare_variant_source_name(
      SgName(omp_declare_variant_source_name));
  decl->set_omp_declare_variant_region_ordinal(
      omp_declare_variant_region_ordinal);
  decl->set_source_name_parenthesized_for_macro(
      json.requiredBool("source_name_parenthesized_for_macro"));
  decl->set_source_declarator_uses_wrapped_function_type(
      json.requiredBool("source_declarator_uses_wrapped_function_type"));
  decl->set_type_syntax(function_type_syntax);
  decl->set_type_syntax_is_available(
      json.requiredBool("type_syntax_is_available"));
  if (decl->get_type_syntax_is_available() !=
      (decl->get_type_syntax() != nullptr)) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has inconsistent function type syntax state");
  }
  if (function_type_syntax != nullptr) {
    if (function_type_syntax == function_type ||
        function_type_syntax->get_parent() != nullptr) {
      throw std::runtime_error("AST JSON external_function " + name +
                               " does not own one distinct source function "
                               "type syntax node");
    }
    function_type_syntax->set_parent(decl);
  }

  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no location");
  }
  restoreNodeSourcePositionFromJson(decl, *location,
                                    "external_function " + name);
  SgFunctionParameterList *parameter_list = decl->get_parameterList();
  if (parameter_list == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " constructor did not create a parameterList");
  }
  const JsonValue *parameter_list_location =
      json.find("parameter_list_location");
  if (parameter_list_location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no parameter_list_location");
  }
  restoreNodeSourcePositionFromJson(parameter_list, *parameter_list_location,
                                    "external_function parameterList " + name);
  parameter_list->set_parent(decl);
  const JsonValue &parameters = json.at("parameters");
  if (parameters.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " parameters is not an array");
  }
  for (const JsonValue &parameter_json : parameters.array) {
    SgInitializedName *parameter = externalInitializedNameFromJson(
        parameter_json, nodes, decl, nullptr, "external_function " + name);
    parameter_list->append_arg(parameter);
  }
  if (json.requiredBool("parameter_list_syntax_aliases_parameter_list")) {
    decl->set_parameterList_syntax(parameter_list);
  }
  const JsonValue *function_parameter_scope =
      json.find("function_parameter_scope");
  if (function_parameter_scope == nullptr) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has no function_parameter_scope");
  }
  SgFunctionParameterScope *parameter_scope =
      externalFunctionParameterScopeFromJson(*function_parameter_scope, nodes,
                                             parameter_list->get_args(), name);
  if (parameter_scope != nullptr) {
    parameter_scope->set_parent(decl);
    decl->set_functionParameterScope(parameter_scope);
  }
  decl->set_firstNondefiningDeclaration(decl);
  decl->set_definingDeclaration(nullptr);
  if (isSgProcedureHeaderStatement(decl) != nullptr) {
    SageInterface::isFortranProgramUnitWithoutSourceName(decl);
  }
  if (decl->get_source_declarator_uses_wrapped_function_type()) {
    decl->validate_source_declarator_form();
  }
  SgGlobal *external_global = new SgGlobal();
  installTransformationSourcePosition(external_global);
  decl->set_scope(external_global);
  SageBuilder::attachAuxiliaryDeclaration(external_global, decl);
  markAstJsonExternalFunction(decl, source_file);
  const SgName symbol_key =
      isSgProcedureHeaderStatement(decl) != nullptr
          ? SageInterface::getFortranProgramUnitSymbolTableKey(decl)
          : decl->get_name();
  SgFunctionSymbol *canonical_symbol = new SgFunctionSymbol(decl);
  // This global is an isolated, non-output semantic root, not a project file.
  // Publish into its exact table without the project-wide alias side effect of
  // SgScopeStatement::insert_symbol.
  external_global->get_symbol_table()->insert(symbol_key, canonical_symbol);
  if (external_global->find_symbol_from_declaration(decl) != canonical_symbol ||
      canonical_symbol->get_parent() != external_global->get_symbol_table() ||
      !external_global->get_symbol_table()->exists(canonical_symbol)) {
    throw std::runtime_error("AST JSON external function " + name +
                             " has no exact canonical symbol publication");
  }
  identity_bucket.push_back({&json, decl});
  return decl;
}

SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue *json, const NodeMap &nodes, const std::string &context) {
  if (json == nullptr || !json->requiredBool("present")) {
    return nullptr;
  }
  const std::string kind = json->at("kind").asString();
  if (const JsonValue *external = json->find("external_function")) {
    SgFunctionDeclaration *decl = externalFunctionFromJson(*external, nodes);
    if (decl == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_function reconstructed as null");
    }
    if (kind != decl->sage_class_name()) {
      throw std::runtime_error("AST JSON " + context +
                               " kind disagrees with external_function");
    }
    return decl;
  }
  if (const JsonValue *external = json->find("external_module")) {
    SgModuleStatement *module = externalModuleFromJson(*external);
    if (module == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_module reconstructed as null");
    }
    if (kind != module->sage_class_name()) {
      throw std::runtime_error("AST JSON " + context +
                               " kind disagrees with external_module");
    }
    return module;
  }
  if (const JsonValue *external = json->find("external_class")) {
    SgClassDeclaration *decl = externalClassDeclarationFromJson(*external);
    if (decl == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " external_class reconstructed as null");
    }
    if (kind != decl->sage_class_name()) {
      throw std::runtime_error("AST JSON " + context +
                               " kind disagrees with external_class");
    }
    return decl;
  }
  throw std::runtime_error(
      "AST JSON " + context +
      " has unsupported external declaration kind: " + kind);
}

template <typename ListT, typename NodeT>
void appendEdgeList(ListT &list, const NodeRecord &record,
                    const std::string &field, const NodeMap &nodes,
                    SgNode *parent, bool owns_children = true) {
  for (const EdgeRecord &edge : edgesFor(record, field)) {
    NodeT *child = nodeByIdAs<NodeT>(nodes, edge.target);
    list.push_back(child);
    if (owns_children) {
      child->set_parent(parent);
      if (SgDeclarationStatement *decl = isSgDeclarationStatement(child)) {
        if (decl->get_scope() == nullptr) {
          decl->set_scope(isSgScopeStatement(parent));
        }
      }
    }
  }
}

template <typename StatementT>
void linkRequiredOmpClauseList(StatementT *statement, const NodeRecord &record,
                               const NodeMap &nodes) {
  if (!edgesFor(record, "clauses").empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP statement has flattened 'clauses' edges");
  }
  const uint64_t target = requiredSingleEdgeTarget(record, "clause_list");
  SgOmpClauseList *serialized_list = nodeByIdAs<SgOmpClauseList>(nodes, target);
  if (serialized_list->get_parent() != statement) {
    throw std::runtime_error(
        "AST JSON OpenMP clause-list parent edge does not identify its "
        "owning statement");
  }
  serialized_list->set_parent(nullptr);
  statement->replace_clause_list(serialized_list);
}

template <typename StatementT>
void linkRequiredOmpVariableList(StatementT *statement,
                                 const NodeRecord &record,
                                 const NodeMap &nodes) {
  if (!edgesFor(record, "variables").empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP statement has flattened 'variables' edges");
  }
  const uint64_t target = requiredSingleEdgeTarget(record, "variable_list");
  SgExprListExp *serialized_list = nodeByIdAs<SgExprListExp>(nodes, target);
  if (serialized_list->get_parent() != statement) {
    throw std::runtime_error(
        "AST JSON OpenMP variable-list parent edge does not identify its "
        "owning statement");
  }
  serialized_list->set_parent(nullptr);
  statement->replace_variable_list(serialized_list);
}

void restoreTokenStreamContainer(SgSourceFile *file) {
  if (file == nullptr ||
      file->get_preprocessorDirectivesAndCommentsList() != nullptr) {
    throw std::runtime_error(
        "AST JSON token-stream restoration requires a fresh source file");
  }
  const std::string filename = file->getFileName();
  if (filename.empty()) {
    throw std::runtime_error(
        "AST JSON token-stream restoration requires a source filename");
  }

  std::unique_ptr<LexTokenStreamType> stream(new LexTokenStreamType());
  for (SgToken *token : file->get_token_list()) {
    if (token == nullptr || token->get_startOfConstruct() == nullptr ||
        token->get_endOfConstruct() == nullptr) {
      throw std::runtime_error(
          "AST JSON token stream contains a token without source extent");
    }
    std::unique_ptr<stream_element> element(new stream_element());
    element->p_tok_elem = new token_element();
    element->p_tok_elem->token_lexeme = token->get_lexeme_string();
    element->p_tok_elem->token_id =
        static_cast<int>(token->get_classification_code());
    element->p_preprocessingInfo = nullptr;
    element->beginning_fpi.line_num = token->get_startOfConstruct()->get_line();
    element->beginning_fpi.column_num =
        token->get_startOfConstruct()->get_col();
    element->ending_fpi.line_num = token->get_endOfConstruct()->get_line();
    element->ending_fpi.column_num = token->get_endOfConstruct()->get_col();
    stream->push_back(element.release());
  }

  std::unique_ptr<ROSEAttributesList> attributes(new ROSEAttributesList());
  attributes->setFileName(filename);
  attributes->set_rawTokenStream(stream.release());
  std::unique_ptr<ROSEAttributesListContainer> container(
      new ROSEAttributesListContainer());
  container->addList(filename, attributes.release());
  file->set_preprocessorDirectivesAndCommentsList(container.release());
}

void setNodeSourcePosition(SgNode *node, const NodeRecord &record) {
  const JsonValue *start = record.location.find("start");
  const JsonValue *end = record.location.find("end");
  Sg_File_Info *start_info =
      start != nullptr ? buildFileInfo(*start, node) : nullptr;
  Sg_File_Info *end_info = end != nullptr ? buildFileInfo(*end, node) : nullptr;

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_info);
    located->set_endOfConstruct(end_info);
    if (SgExpression *expression = isSgExpression(node)) {
      const JsonValue *operator_position = record.location.find("operator");
      if (operator_position == nullptr) {
        throw std::runtime_error("AST JSON " + record.kind +
                                 " location has no operator field");
      }
      expression->set_operatorPosition(
          buildFileInfo(*operator_position, expression));
    }
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_info);
    pragma->set_endOfConstruct(end_info);
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_info);
    name->set_endOfConstruct(end_info);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_info);
  }
}

void setNodeFlags(SgNode *node, const NodeRecord &record) {
  node->set_containsTransformation(
      record.flags.requiredBool("contains_transformation"));
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_containsTransformationToSurroundingWhitespace(
        record.flags.requiredBool(
            "contains_transformation_to_surrounding_whitespace"));
    located->set_source_range_ends_in_macro_expansion(
        record.flags.requiredBool("source_range_ends_in_macro_expansion"));
    located->set_source_range_is_macro_expansion_fragment(
        record.flags.requiredBool("source_range_is_macro_expansion_fragment"));
  }
}

void restoreAvailableSourcePositionsAndScopes(const AstFileRecord &ast,
                                              const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    setNodeSourcePosition(found->second, record);
    setNodeFlags(found->second, record);
    if (SgBasicBlock *block = isSgBasicBlock(found->second)) {
      const uint64_t semantic_scope_id =
          singleEdgeTarget(record, "statement_expression_semantic_scope");
      SgScopeStatement *semantic_scope =
          semantic_scope_id != 0
              ? nodeByIdAs<SgScopeStatement>(nodes, semantic_scope_id)
              : nullptr;
      if (block->get_statement_expression_construction_scope() != nullptr ||
          block->get_statement_expression_semantic_scope() != nullptr ||
          block->get_implied_do_construction_scope() != nullptr ||
          block->get_implied_do_semantic_scope() != nullptr ||
          block->get_forall_construction_scope() != nullptr) {
        throw std::runtime_error(
            "AST JSON fresh SgBasicBlock already has typed expression "
            "semantic state");
      }
      block->set_statement_expression_semantic_scope(semantic_scope);
      const uint64_t implied_semantic_scope_id =
          singleEdgeTarget(record, "implied_do_semantic_scope");
      SgScopeStatement *implied_semantic_scope =
          implied_semantic_scope_id != 0
              ? nodeByIdAs<SgScopeStatement>(nodes, implied_semantic_scope_id)
              : nullptr;
      if (semantic_scope != nullptr && implied_semantic_scope != nullptr) {
        throw std::runtime_error(
            "AST JSON SgBasicBlock has multiple typed expression semantic "
            "roles");
      }
      block->set_implied_do_semantic_scope(implied_semantic_scope);
    }
  }

  std::unordered_map<uint64_t, uint64_t> recorded_parents;
  std::unordered_map<uint64_t, uint64_t> nonreal_scope_owners;
  std::unordered_map<uint64_t, uint64_t> function_scope_owners;
  for (const NodeRecord &record : ast.nodes) {
    if (const uint64_t parent_id = singleEdgeTarget(record, "parent")) {
      recorded_parents.emplace(record.id, parent_id);
    }
    const uint64_t scope_id = singleEdgeTarget(record, "nonreal_decl_scope");
    if (scope_id != 0 &&
        !nonreal_scope_owners.emplace(scope_id, record.id).second) {
      throw std::runtime_error(
          "AST JSON nonreal declaration scope has multiple typed owners");
    }
    const uint64_t function_scope_id =
        singleEdgeTarget(record, "function_declarator_scope");
    if (function_scope_id != 0 &&
        !function_scope_owners.emplace(function_scope_id, record.id).second) {
      throw std::runtime_error(
          "AST JSON function declarator scope has multiple typed owners");
    }
  }
  for (const auto &[scope_id, owner_id] : nonreal_scope_owners) {
    auto scope_node = nodes.find(scope_id);
    auto owner_node = nodes.find(owner_id);
    if (scope_node == nodes.end() || owner_node == nodes.end()) {
      continue;
    }
    auto recorded_parent = recorded_parents.find(scope_id);
    if (recorded_parent == recorded_parents.end() ||
        recorded_parent->second != owner_id) {
      throw std::runtime_error(
          "AST JSON nonreal declaration scope parent disagrees with its "
          "typed owner");
    }
    SgDeclarationStatement *owner =
        isSgDeclarationStatement(owner_node->second);
    SgDeclarationScope *scope = isSgDeclarationScope(scope_node->second);
    if (owner == nullptr || scope == nullptr) {
      throw std::runtime_error(
          "AST JSON nonreal declaration scope has invalid typed endpoints");
    }
    SageBuilder::setNonrealDeclarationScope(owner, scope);
  }
  for (const auto &[scope_id, owner_id] : function_scope_owners) {
    auto scope_node = nodes.find(scope_id);
    auto owner_node = nodes.find(owner_id);
    if (scope_node == nodes.end() || owner_node == nodes.end()) {
      continue;
    }
    auto recorded_parent = recorded_parents.find(scope_id);
    if (recorded_parent == recorded_parents.end() ||
        recorded_parent->second != owner_id) {
      throw std::runtime_error(
          "AST JSON function declarator scope parent disagrees with its "
          "typed owner");
    }
    SgFunctionDeclaration *owner = isSgFunctionDeclaration(owner_node->second);
    SgDeclarationScope *scope = isSgDeclarationScope(scope_node->second);
    if (owner == nullptr || scope == nullptr) {
      throw std::runtime_error(
          "AST JSON function declarator scope has invalid typed endpoints");
    }
    SageBuilder::adoptFunctionDeclaratorScope(owner, scope);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      auto parent = nodes.find(target);
      if (parent != nodes.end()) {
        found->second->set_parent(parent->second);
      }
    }
  }

  // A requires-expression parameter list derives its declaration scope from
  // the separately owned local parameter-scope edge.  Publish that typed
  // declarative region before restoring any SgDeclarationStatement::scope
  // edge: set_scope validates the declaration's current structural scope and
  // must never observe a half-linked requires expression.
  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    if (SgRequiresExpr *requires_expr = isSgRequiresExpr(found->second)) {
      const uint64_t parameter_list_id =
          singleEdgeTarget(record, "local_parameter_list");
      const uint64_t parameter_scope_id =
          singleEdgeTarget(record, "local_parameter_scope");
      if ((parameter_list_id == 0) != (parameter_scope_id == 0)) {
        throw std::runtime_error(
            "AST JSON SgRequiresExpr has mismatched local parameter "
            "list/scope construction edges");
      }
      if (parameter_list_id != 0) {
        SgFunctionParameterList *parameters =
            nodeByIdAs<SgFunctionParameterList>(nodes, parameter_list_id);
        SgFunctionParameterScope *scope =
            nodeByIdAs<SgFunctionParameterScope>(nodes, parameter_scope_id);
        requires_expr->set_local_parameter_list(parameters);
        parameters->set_parent(requires_expr);
        requires_expr->set_local_parameter_scope(scope);
        scope->set_parent(requires_expr);
      }
    }
    if (SgFunctionParameterScope *scope =
            isSgFunctionParameterScope(found->second)) {
      if (uint64_t target =
              singleEdgeTarget(record, "completed_semantic_scope")) {
        scope->set_completed_semantic_scope(
            nodeByIdAs<SgScopeStatement>(nodes, target));
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    SgNode *node = found->second;
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (uint64_t target = singleEdgeTarget(record, "declptr")) {
        auto declaration = nodes.find(target);
        SgDeclarationStatement *exact_declaration =
            declaration != nodes.end()
                ? isSgDeclarationStatement(declaration->second)
                : nullptr;
        if (exact_declaration == nullptr ||
            (name->get_declptr() != nullptr &&
             name->get_declptr() != exact_declaration)) {
          throw std::runtime_error(
              "AST JSON initialized-name has no exact construction-time "
              "declaration owner");
        }
        name->set_declptr(exact_declaration);
      }
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        auto scope = nodes.find(target);
        if (scope != nodes.end()) {
          name->set_scope(isSgScopeStatement(scope->second));
        }
      }
    }
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      const auto restore_specialization = [&](auto *specializable,
                                              const char *context) {
        specializable->set_specialization(
            requiredEnum<SgDeclarationStatement::template_specialization_enum>(
                record.properties, "specialization", context,
                {SgDeclarationStatement::e_no_specialization,
                 SgDeclarationStatement::e_partial_specialization,
                 SgDeclarationStatement::e_specialization}));
      };
      if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
        restore_specialization(variable, "SgVariableDeclaration");
      } else if (SgClassDeclaration *class_declaration =
                     isSgClassDeclaration(decl)) {
        restore_specialization(class_declaration, "SgClassDeclaration");
      } else if (SgFunctionDeclaration *function =
                     isSgFunctionDeclaration(decl)) {
        restore_specialization(function, "SgFunctionDeclaration");
      }
      if (uint64_t target = singleEdgeTarget(record, "scope")) {
        auto scope = nodes.find(target);
        if (scope != nodes.end()) {
          decl->set_scope(isSgScopeStatement(scope->second));
        }
      }
    }
  }
}

void attachPreprocessingInfo(SgNode *node, const NodeRecord &record) {
  if (record.preprocessing.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON node preprocessing payload is not an array");
  }
  SgLocatedNode *located = isSgLocatedNode(node);
  if (located == nullptr) {
    if (!record.preprocessing.array.empty()) {
      throw std::runtime_error(
          "AST JSON non-located node owns preprocessing entries");
    }
    return;
  }
  auto make_info = [](const JsonValue &entry) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error("AST JSON preprocessing entry is not an object");
    }
    std::unique_ptr<Sg_File_Info> file_info(
        buildFileInfo(entry.at("file_info"), SgTypeDefault::createType()));
    if (file_info == nullptr) {
      throw std::runtime_error(
          "AST JSON preprocessing entry requires present file_info");
    }
    const PreprocessingInfo::DirectiveType directive =
        requiredEnum<PreprocessingInfo::DirectiveType>(
            entry, "directive", "preprocessing entry",
            {PreprocessingInfo::C_StyleComment,
             PreprocessingInfo::CplusplusStyleComment,
             PreprocessingInfo::FortranStyleComment,
             PreprocessingInfo::F90StyleComment,
             PreprocessingInfo::CpreprocessorBlankLine,
             PreprocessingInfo::CpreprocessorIncludeDeclaration,
             PreprocessingInfo::CpreprocessorIncludeNextDeclaration,
             PreprocessingInfo::CpreprocessorDefineDeclaration,
             PreprocessingInfo::CpreprocessorUndefDeclaration,
             PreprocessingInfo::CpreprocessorIfdefDeclaration,
             PreprocessingInfo::CpreprocessorIfndefDeclaration,
             PreprocessingInfo::CpreprocessorIfDeclaration,
             PreprocessingInfo::CpreprocessorDeadIfDeclaration,
             PreprocessingInfo::CpreprocessorElseDeclaration,
             PreprocessingInfo::CpreprocessorElifDeclaration,
             PreprocessingInfo::CpreprocessorEndifDeclaration,
             PreprocessingInfo::CpreprocessorLineDeclaration,
             PreprocessingInfo::CpreprocessorPragmaDeclaration,
             PreprocessingInfo::CpreprocessorErrorDeclaration,
             PreprocessingInfo::CpreprocessorWarningDeclaration,
             PreprocessingInfo::CpreprocessorEmptyDeclaration,
             PreprocessingInfo::CSkippedToken,
             PreprocessingInfo::CMacroCall,
             PreprocessingInfo::CMacroCallStatement,
             PreprocessingInfo::CpreprocessorIdentDeclaration,
             PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker,
             PreprocessingInfo::CpreprocessorEnd_ifDeclaration});
    std::unique_ptr<PreprocessingInfo> info(new PreprocessingInfo(
        directive, entry.requiredString("text"),
        file_info->get_filenameString(), file_info->get_line(),
        file_info->get_col(), static_cast<int>(entry.requiredInt("lines")),
        requiredEnum<PreprocessingInfo::RelativePositionType>(
            entry, "relative", "preprocessing entry",
            {PreprocessingInfo::before, PreprocessingInfo::after,
             PreprocessingInfo::inside, PreprocessingInfo::before_syntax,
             PreprocessingInfo::after_syntax})));
    std::unique_ptr<Sg_File_Info> constructor_file_info(info->get_file_info());
    info->set_file_info(file_info.release());
    if (entry.requiredBool("transformation")) {
      info->setAsTransformation();
    } else {
      info->unsetAsTransformation();
    }
    info->setOutputPlacement(
        requiredEnum<PreprocessingInfo::OutputPlacementType>(
            entry, "output_placement", "preprocessing entry",
            {PreprocessingInfo::source_position,
             PreprocessingInfo::attached_output_boundary,
             PreprocessingInfo::attached_output_trailing_line}));
    return info;
  };
  if (located->getAttachedPreprocessingInfo() != nullptr) {
    located->clearAttachedPreprocessingInfo();
  }
  for (const JsonValue &entry : record.preprocessing.array) {
    std::unique_ptr<PreprocessingInfo> info = make_info(entry);
    located->attachPreprocessingInfo(
        info.get(), info->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::back);
    info.release();
  }
  located->validateAttachedPreprocessingInfoOwnership();
}

void attachAstAttributes(SgNode *node, const NodeRecord &record) {
  if (node == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot attach attributes to a null node");
  }
  const JsonValue *attributes = record.properties.find("attributes");
  if (attributes == nullptr) {
    throw std::runtime_error("AST JSON node is missing required attributes");
  }
  if (attributes->kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON node attributes are not an array");
  }
  std::unordered_set<std::string> names;
  for (const JsonValue &entry : attributes->array) {
    const std::string name = entry.requiredString("name");
    const std::string type = entry.requiredString("type");
    if (name.empty()) {
      throw std::runtime_error("AST JSON attribute has an empty name");
    }
    if (!names.insert(name).second || node->attributeExists(name)) {
      throw std::runtime_error("AST JSON attribute name is duplicated: " +
                               name);
    }
    if (type == "AstIntAttribute") {
      const int64_t value = entry.requiredInt("value");
      if (value < std::numeric_limits<int>::min() ||
          value > std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "AST JSON integer attribute is out of range: " + name);
      }
      node->setAttribute(name, new AstIntAttribute(static_cast<int>(value)));
    } else if (type == "AstStringAttribute") {
      node->setAttribute(name, new AstValueAttribute<std::string>(
                                   entry.requiredString("value")));
    } else {
      throw std::runtime_error("AST JSON attribute type is unsupported: " +
                               type);
    }
  }
}

void restoreSharedFunctionParameterScopeBindings(
    SgFunctionDeclaration *function) {
  if (function == nullptr || function->get_parameterList() == nullptr ||
      function->get_functionParameterScope() == nullptr) {
    return;
  }
  SgFunctionParameterScope *parameter_scope =
      function->get_functionParameterScope();
  SgFunctionDeclaration *scope_owner =
      isSgFunctionDeclaration(parameter_scope->get_parent());
  if (scope_owner == nullptr || scope_owner == function ||
      (scope_owner != function->get_firstNondefiningDeclaration() &&
       scope_owner != function->get_definingDeclaration())) {
    return;
  }
  for (SgInitializedName *parameter :
       function->get_parameterList()->get_args()) {
    if (parameter == nullptr || (parameter->get_scope() != nullptr &&
                                 parameter->get_scope() != parameter_scope)) {
      throw std::runtime_error(
          "AST JSON shared function parameter scope has a conflicting "
          "parameter binding");
    }
    parameter->set_scope(parameter_scope);
    SgVariableSymbol *symbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    if (symbol == nullptr || symbol->get_declaration() != parameter) {
      throw std::runtime_error(
          "AST JSON shared function parameter scope has no exact restored "
          "parameter symbol");
    }
  }
}

void linkNodeEdges(const NodeRecord &record, const NodeMap &nodes) {
  SgNode *node = nodeById(nodes, record.id);
  if (uint64_t target = singleEdgeTarget(record, "parent")) {
    node->set_parent(nodeById(nodes, target));
  }
  if (SgRequiresExpr *requires_expr = isSgRequiresExpr(node)) {
    SgFunctionParameterList *parameters = nullptr;
    if (uint64_t target = singleEdgeTarget(record, "local_parameter_list")) {
      parameters = nodeByIdAs<SgFunctionParameterList>(nodes, target);
      requires_expr->set_local_parameter_list(parameters);
      parameters->set_parent(requires_expr);
    }
    SgFunctionParameterScope *parameter_scope = nullptr;
    if (uint64_t target = singleEdgeTarget(record, "local_parameter_scope")) {
      parameter_scope = nodeByIdAs<SgFunctionParameterScope>(nodes, target);
      requires_expr->set_local_parameter_scope(parameter_scope);
      parameter_scope->set_parent(requires_expr);
    }
    if ((parameters == nullptr) != (parameter_scope == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgRequiresExpr has mismatched local parameter list/scope");
    }
    const uint64_t target = requiredSingleEdgeTarget(record, "requirements");
    SgExprListExp *requirements = nodeByIdAs<SgExprListExp>(nodes, target);
    requires_expr->set_requirements(requirements);
    requirements->set_parent(requires_expr);
  }
  if (SgSimpleRequirement *requirement = isSgSimpleRequirement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
    requirement->set_expression(expression);
    expression->set_parent(requirement);
  }
  if (SgTypeRequirement *requirement = isSgTypeRequirement(node)) {
    SgType *required_type =
        typeFromJson(record.properties.at("required_type"), nodes);
    if (required_type == nullptr) {
      throw std::runtime_error(
          "AST JSON SgTypeRequirement has a null required type");
    }
    requirement->set_required_type(required_type);
  }
  if (SgCompoundRequirement *requirement = isSgCompoundRequirement(node)) {
    const uint64_t expression_target =
        requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression =
        nodeByIdAs<SgExpression>(nodes, expression_target);
    requirement->set_expression(expression);
    expression->set_parent(requirement);
    if (uint64_t target = singleEdgeTarget(record, "type_constraint")) {
      SgExpression *constraint = nodeByIdAs<SgExpression>(nodes, target);
      requirement->set_type_constraint(constraint);
      constraint->set_parent(requirement);
    }
  }
  if (SgNestedRequirement *requirement = isSgNestedRequirement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "constraint");
    SgExpression *constraint = nodeByIdAs<SgExpression>(nodes, target);
    requirement->set_constraint(constraint);
    constraint->set_parent(requirement);
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "numeric_label")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_numeric_label(label);
      label->set_parent(stmt);
    }
  }
  if (SgScopeStatement *scope = isSgScopeStatement(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "auxiliary_declaration_scopes")) {
      SgDeclarationScopeList *container =
          nodeByIdAs<SgDeclarationScopeList>(nodes, target);
      scope->set_auxiliary_declaration_scopes(container);
      container->set_parent(scope);
    }
    if (uint64_t target = singleEdgeTarget(record, "auxiliary_declarations")) {
      SgAuxiliaryDeclarationList *container =
          nodeByIdAs<SgAuxiliaryDeclarationList>(nodes, target);
      scope->set_auxiliary_declarations(container);
      container->set_parent(scope);
    }
  }
  if (SgDeclarationScopeList *container = isSgDeclarationScopeList(node)) {
    appendEdgeList<SgDeclarationScopePtrList, SgDeclarationScope>(
        container->get_scopes(), record, "scopes", nodes, container);
    if (container->get_scopes().empty()) {
      throw std::runtime_error(
          "AST JSON SgDeclarationScopeList must not be empty");
    }
  }
  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        container->get_declarations(), record, "declarations", nodes, container,
        false);
    if (container->get_declarations().empty()) {
      throw std::runtime_error(
          "AST JSON SgAuxiliaryDeclarationList must not be empty");
    }
  }
  if (SgOmpClauseList *container = isSgOmpClauseList(node)) {
    for (const EdgeRecord &edge : edgesFor(record, "clauses")) {
      container->append_clause(nodeByIdAs<SgOmpClause>(nodes, edge.target));
    }
    return;
  }
  if (SgTemplateParameterList *container = isSgTemplateParameterList(node)) {
    SgTemplateParameterPtrList parameters;
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        parameters, record, "args", nodes, container);
    if (parameters.empty()) {
      throw std::runtime_error(
          "AST JSON SgTemplateParameterList must not be empty");
    }
    container->set_args(parameters);
    container->set_source_header_separator(
        requiredEnum<SgTemplateParameterList::source_header_separator_enum>(
            record.properties, "source_header_separator",
            "SgTemplateParameterList",
            {SgTemplateParameterList::e_source_header_separator_space,
             SgTemplateParameterList::e_source_header_separator_newline}));
  }

  if (SgSourceFile *file = isSgSourceFile(node)) {
    SgType *target_size_type =
        nullableTypeFromJson(record.properties.at("target_size_type"), nodes);
    file->set_target_size_type(target_size_type);
    if (target_size_type != nullptr &&
        SageInterface::requireTargetSizeType(file) != target_size_type) {
      throw std::runtime_error(
          "AST JSON SgSourceFile did not retain its exact target size_t type");
    }
    const uint64_t target = singleEdgeTarget(record, "globalScope");
    if (target != 0) {
      SgGlobal *global = nodeByIdAs<SgGlobal>(nodes, target);
      file->set_globalScope(global);
      global->set_parent(file);
    }
    appendEdgeList<SgTokenPtrList, SgToken>(file->get_token_list(), record,
                                            "token_list", nodes, file);
    restoreTokenStreamContainer(file);
    return;
  }
  if (SgGlobal *global = isSgGlobal(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        global->get_declarations(), record, "declarations", nodes, global);
    return;
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        def->get_declarations(), record, "declarations", nodes, def,
        !def->get_isUnionOfReentrantNamespaceDefinitions());
  }
  if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        scope->get_declarations(), record, "declarations", nodes, scope);
  }
  if (SgFunctionParameterScope *scope = isSgFunctionParameterScope(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "completed_semantic_scope")) {
      scope->set_completed_semantic_scope(
          nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (singleEdgeTarget(record, "construction_physical_output_owner") != 0 ||
        singleEdgeTarget(record, "construction_semantic_scope") != 0) {
      throw std::runtime_error(
          "AST JSON attached SgFunctionParameterScope retained an incomplete "
          "construction transaction");
    }
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        scope->get_declarations(), record, "declarations", nodes, scope);
  }
  if (SgBasicBlock *block = isSgBasicBlock(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        block->get_statements(), record, "statements", nodes, block);
    return;
  }
  if (SgFunctionParameterList *params = isSgFunctionParameterList(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        params->get_args(), record, "args", nodes, params);
    if (SgFunctionDeclaration *function =
            isSgFunctionDeclaration(params->get_parent())) {
      restoreSharedFunctionParameterScopeBindings(function);
    } else if (SgRequiresExpr *requires_expr =
                   isSgRequiresExpr(params->get_parent())) {
      SgFunctionParameterScope *scope =
          requires_expr->get_local_parameter_scope();
      if (requires_expr->get_local_parameter_list() != params ||
          scope == nullptr || scope->get_parent() != requires_expr) {
        throw std::runtime_error(
            "AST JSON SgRequiresExpr has no exact restored local parameter "
            "declarative region");
      }
      for (SgInitializedName *parameter : params->get_args()) {
        if (parameter == nullptr || parameter->get_parent() != params ||
            parameter->get_scope() != scope) {
          throw std::runtime_error(
              "AST JSON requires-expression parameter has a conflicting "
              "semantic scope");
        }
        if (!parameter->get_name().getString().empty()) {
          SgVariableSymbol *symbol =
              isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
          if (symbol == nullptr || symbol->get_declaration() != parameter ||
              scope->find_symbol_from_declaration(parameter) != symbol) {
            throw std::runtime_error(
                "AST JSON requires-expression parameter has no exact restored "
                "symbol");
          }
        }
      }
    }
  }
  if (SgExprListExp *exprs = isSgExprListExp(node)) {
    const std::vector<EdgeRecord> expression_edges =
        edgesFor(record, "expressions");
    if (!exprs->get_expressions().empty()) {
      if (exprs->get_expressions().size() != expression_edges.size()) {
        throw std::runtime_error(
            "AST JSON construction-populated expression list disagrees with "
            "its exact serialized edge count");
      }
      for (size_t index = 0; index < expression_edges.size(); ++index) {
        SgExpression *expected =
            nodeByIdAs<SgExpression>(nodes, expression_edges[index].target);
        if (exprs->get_expressions()[index] != expected ||
            expected->get_parent() != exprs) {
          throw std::runtime_error(
              "AST JSON construction-populated expression list disagrees "
              "with its exact serialized child ownership");
        }
      }
      return;
    }
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        exprs->get_expressions(), record, "expressions", nodes, exprs);
    return;
  }
  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(node)) {
    const uint64_t target = singleEdgeTarget(record, "expanded_expression");
    if (target == 0) {
      throw std::runtime_error(
          "AST JSON SgMacroExpansionExp has no expanded_expression edge");
    }
    SgExpression *expanded = nodeByIdAs<SgExpression>(nodes, target);
    macro->set_expanded_expression(expanded);
    expanded->set_parent(macro);
  }
  if (SgDesignator *designator = isSgDesignator(node)) {
    const uint64_t first_target =
        requiredSingleEdgeTarget(record, "first_expression");
    SgExpression *first = nodeByIdAs<SgExpression>(nodes, first_target);
    designator->set_first_expression(first);
    first->set_parent(designator);
    if (uint64_t second_target =
            singleEdgeTarget(record, "second_expression")) {
      SgExpression *second = nodeByIdAs<SgExpression>(nodes, second_target);
      designator->set_second_expression(second);
      second->set_parent(designator);
    }
    designator->validate_designator();
  }
  if (SgFortranCommonBlockRefExp *common = isSgFortranCommonBlockRefExp(node)) {
    const uint64_t target = singleEdgeTarget(record, "common_block");
    if (target == 0) {
      throw std::runtime_error(
          "AST JSON SgFortranCommonBlockRefExp has no common_block edge");
    }
    common->set_common_block(nodeByIdAs<SgCommonBlockObject>(nodes, target));
    SageInterface::validateFortranCommonBlockRef(common);
  }
  if (SgClassDefinition *def = isSgClassDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgClassDeclaration *decl = nodeByIdAs<SgClassDeclaration>(nodes, target);
      def->set_declaration(decl);
      if (decl->get_definition() == nullptr) {
        decl->set_definition(def);
      }
    }
    appendEdgeList<SgBaseClassPtrList, SgBaseClass>(
        def->get_inheritances(), record, "inheritances", nodes, def);
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        def->get_members(), record, "members", nodes, def);
    return;
  }
  if (SgBaseClass *base = isSgBaseClass(node)) {
    uint64_t target = singleEdgeTarget(record, "base_class");
    if (target == 0) {
      target =
          static_cast<uint64_t>(record.properties.requiredInt("base_class"));
    }
    if (target != 0) {
      base->set_base_class(nodeByIdAs<SgClassDeclaration>(nodes, target));
    }
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      if (uint64_t expr_target = singleEdgeTarget(record, "base_class_exp")) {
        SgExpression *expr = nodeByIdAs<SgExpression>(nodes, expr_target);
        expr_base->set_base_class_exp(expr);
        expr->set_parent(expr_base);
      } else if (const JsonValue *expr_json =
                     record.properties.find("base_class_exp")) {
        if (SgExpression *expr = expressionFromRef(*expr_json, nodes)) {
          expr_base->set_base_class_exp(expr);
          expr->set_parent(expr_base);
        }
      }
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      uint64_t nonreal_target = singleEdgeTarget(record, "base_class_nonreal");
      if (nonreal_target == 0) {
        nonreal_target = static_cast<uint64_t>(
            record.properties.requiredInt("base_class_nonreal"));
      }
      if (nonreal_target != 0) {
        nonreal_base->set_base_class_nonreal(
            nodeByIdAs<SgNonrealDecl>(nodes, nonreal_target));
      }
    }
    return;
  }
  if (SgCtorInitializerList *ctors = isSgCtorInitializerList(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        ctors->get_ctors(), record, "ctors", nodes, ctors);
  }
  if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        decl->get_variables(), record, "variables", nodes, decl);
    appendEdgeList<SgTemplateParameterListPtrList, SgTemplateParameterList>(
        decl->get_sourceSpelledTemplateHeaders(), record,
        "source_spelled_template_headers", nodes, decl);
    SgType *source_owner_type = nullableTypeFromJson(
        record.properties.at("source_spelled_template_owner_type"), nodes);
    SgNamedType *named_source_owner = isSgNamedType(source_owner_type);
    SgDeclarationStatement *source_owner_decl =
        named_source_owner != nullptr ? named_source_owner->get_declaration()
                                      : nullptr;
    const bool exact_dependent_owner =
        isSgNonrealType(named_source_owner) != nullptr &&
        isSgNonrealDecl(source_owner_decl) != nullptr;
    const bool exact_concrete_owner =
        isSgClassType(named_source_owner) != nullptr &&
        isSgTemplateInstantiationDecl(source_owner_decl) != nullptr;
    if (source_owner_type != nullptr && !exact_dependent_owner &&
        !exact_concrete_owner) {
      throw std::runtime_error(
          "AST JSON variable source template owner type is not an exact "
          "dependent or concrete template owner");
    }
    decl->set_sourceSpelledTemplateOwnerType(named_source_owner);
    if (uint64_t target =
            singleEdgeTarget(record, "baseTypeNondefiningDeclaration")) {
      SgDeclarationStatement *base_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      base_decl->set_parent(decl);
      decl->set_baseTypeNondefiningDeclaration(base_decl);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "baseTypeDefiningDeclaration")) {
      SgDeclarationStatement *base_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      base_decl->set_parent(decl);
      decl->set_baseTypeDefiningDeclaration(base_decl);
    }
  }
  if (SgVariableDefinition *def = isSgVariableDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "vardefn")) {
      SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, target);
      def->set_vardefn(name);
      name->set_definition(def);
      def->set_parent(name);
    }
    if (uint64_t target = singleEdgeTarget(record, "bitfield")) {
      SgExpression *bitfield = nodeByIdAs<SgExpression>(nodes, target);
      def->set_bitfield(bitfield);
      bitfield->set_parent(def);
    }
  }
  if (SgCommonBlock *stmt = isSgCommonBlock(node)) {
    appendEdgeList<SgCommonBlockObjectPtrList, SgCommonBlockObject>(
        stmt->get_block_list(), record, "block_list", nodes, stmt);
  }
  if (SgCommonBlockObject *object = isSgCommonBlockObject(node)) {
    object->set_block_name(record.properties.requiredString("block_name"));
    if (uint64_t target = singleEdgeTarget(record, "canonical_common_block")) {
      object->set_canonical_common_block(
          nodeByIdAs<SgCommonBlockObject>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "variable_reference_list")) {
      SgExprListExp *list = nodeByIdAs<SgExprListExp>(nodes, target);
      object->set_variable_reference_list(list);
      list->set_parent(object);
    }
  }
  if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    for (const EdgeRecord &edge : edgesFor(record, "enumerators")) {
      SgInitializedName *enumerator =
          nodeByIdAs<SgInitializedName>(nodes, edge.target);
      if (enumerator->get_declptr() != decl) {
        throw std::runtime_error(
            "AST JSON enum constant declaration owner disagrees with its "
            "enumerator edge");
      }
      decl->append_enumerator(enumerator);
    }
  }
  if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgForInitStatement *init = isSgForInitStatement(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        init->get_init_stmt(), record, "init_stmt", nodes, init);
  }
  if (SgAccExpressionClause *clause = isSgAccExpressionClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    const bool expression_is_optional = isSgAccAsyncClause(clause) != nullptr ||
                                        isSgAccVectorClause(clause) != nullptr;
    if (target == 0 && !expression_is_optional) {
      throw std::runtime_error("AST JSON " + record.kind +
                               " has no required expression edge");
    }
    if (target != 0) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_expression(expression);
      expression->set_parent(clause);
    }
  }
  if (SgAccVariablesClause *clause = isSgAccVariablesClause(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "variables");
    SgExprListExp *variables = nodeByIdAs<SgExprListExp>(nodes, target);
    clause->set_variables(variables);
    variables->set_parent(clause);
  }
  if (SgAccBodyStatement *stmt = isSgAccBodyStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "body");
    SgStatement *body = nodeByIdAs<SgStatement>(nodes, target);
    stmt->set_body(body);
    body->set_parent(stmt);
  }
  if (SgAccClauseBodyStatement *stmt = isSgAccClauseBodyStatement(node)) {
    appendEdgeList<SgAccClausePtrList, SgAccClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  } else if (SgAccClauseStatement *stmt = isSgAccClauseStatement(node)) {
    appendEdgeList<SgAccClausePtrList, SgAccClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgAccWaitStatement *stmt = isSgAccWaitStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "wait_list")) {
      SgExprListExp *wait_list = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_wait_list(wait_list);
      wait_list->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "devnum")) {
      SgExpression *devnum = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_devnum(devnum);
      devnum->set_parent(stmt);
    }
    if (stmt->get_queues() && stmt->get_wait_list() == nullptr) {
      throw std::runtime_error(
          "AST JSON SgAccWaitStatement queues syntax has no wait list");
    }
  }
  if (SgAccCacheStatement *stmt = isSgAccCacheStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "variables");
    SgExprListExp *variables = nodeByIdAs<SgExprListExp>(nodes, target);
    stmt->set_variables(variables);
    variables->set_parent(stmt);
  }
  if (SgOmpClauseStatement *stmt = isSgOmpClauseStatement(node)) {
    linkRequiredOmpClauseList(stmt, record, nodes);
  } else if (SgOmpClauseBodyStatement *stmt =
                 isSgOmpClauseBodyStatement(node)) {
    linkRequiredOmpClauseList(stmt, record, nodes);
  }
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "function_ref");
    SgExpression *functionRef = nodeByIdAs<SgExpression>(nodes, target);
    const int64_t ordinal =
        record.properties.requiredInt("semantic_variant_ordinal");
    if (ordinal < 0 || stmt->get_function_ref() != functionRef ||
        functionRef->get_parent() != stmt ||
        stmt->get_function_ref_is_explicit() !=
            record.properties.requiredBool("function_ref_is_explicit") ||
        stmt->get_semantic_variant_ordinal() !=
            static_cast<std::size_t>(ordinal)) {
      throw std::runtime_error(
          "AST JSON declare simd exact target changed after construction");
    }
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    const uint64_t baseTarget =
        requiredSingleEdgeTarget(record, "base_function_ref");
    SgExpression *baseFunctionRef = nodeByIdAs<SgExpression>(nodes, baseTarget);
    const uint64_t variantTarget =
        requiredSingleEdgeTarget(record, "variant_function_ref");
    SgExpression *variantFunctionRef =
        nodeByIdAs<SgExpression>(nodes, variantTarget);
    const int64_t ordinal =
        record.properties.requiredInt("semantic_variant_ordinal");
    if (ordinal < 0 || stmt->get_base_function_ref() != baseFunctionRef ||
        baseFunctionRef->get_parent() != stmt ||
        stmt->get_variant_function_ref() != variantFunctionRef ||
        variantFunctionRef->get_parent() != stmt ||
        stmt->get_base_function_ref_is_explicit() !=
            record.properties.requiredBool("base_function_ref_is_explicit") ||
        stmt->get_semantic_variant_ordinal() !=
            static_cast<std::size_t>(ordinal)) {
      throw std::runtime_error(
          "AST JSON declare variant exact targets changed after construction");
    }
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpBeginDeclareVariantStatement *stmt =
          isSgOmpBeginDeclareVariantStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpDeclareTargetStatement *stmt = isSgOmpDeclareTargetStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    appendEdgeList<SgStatementPtrList, SgStatement>(
        stmt->get_statements(), record, "statements", nodes, stmt);
  }
  if (SgOmpAssumesStatement *stmt = isSgOmpAssumesStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpBeginAssumesStatement *stmt = isSgOmpBeginAssumesStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpGroupprivateStatement *stmt = isSgOmpGroupprivateStatement(node)) {
    linkRequiredOmpClauseList(stmt, record, nodes);
    if (uint64_t target = singleEdgeTarget(record, "variables")) {
      SgExprListExp *variables = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_variables(variables);
      variables->set_parent(stmt);
    }
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_user_defined_identifier(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "mapper_type")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_mapper_type(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "mapper_variable")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_mapper_variable(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgOmpRequiresStatement *stmt = isSgOmpRequiresStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgOmpTaskwaitStatement *stmt = isSgOmpTaskwaitStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgIOStatement *stmt = isSgIOStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "io_stmt_list")) {
      SgExprListExp *expr = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_io_stmt_list(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "unit")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_unit(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "iostat")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_iostat(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "err")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_err(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "iomsg")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_iomsg(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      stmt->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "statement")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_statement(child);
      child->set_parent(stmt);
    }
  }
  if (SgPrintStatement *stmt = isSgPrintStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgWriteStatement *stmt = isSgWriteStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgReadStatement *stmt = isSgReadStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgAttributeSpecificationStatement *stmt =
          isSgAttributeSpecificationStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parameter_list")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_parameter_list(exprs);
      exprs->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "bind_list")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_bind_list(exprs);
      exprs->set_parent(stmt);
    }
  }
  if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    appendEdgeList<SgInterfaceBodyPtrList, SgInterfaceBody>(
        stmt->get_interface_body_list(), record, "interface_body_list", nodes,
        stmt);
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(label);
      label->set_parent(stmt);
    }
  }
  if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    uint64_t target = singleEdgeTarget(record, "functionDeclaration");
    if (target == 0) {
      target = static_cast<uint64_t>(
          record.properties.requiredInt("function_declaration"));
    }
    if (target != 0) {
      body->set_functionDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }

  auto set_statement = [&](const std::string &field,
                           void (SgStatement::*setter)(SgStatement *)) {
    const uint64_t target = singleEdgeTarget(record, field);
    if (target != 0) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      (isSgStatement(node)->*setter)(child);
      child->set_parent(node);
    }
  };
  (void)set_statement;

  if (SgInitializedName *name = isSgInitializedName(node)) {
    const uint64_t shape_property_target = static_cast<uint64_t>(
        record.properties.requiredInt("fortran_cray_pointer_pointee_shape"));
    const uint64_t shape_edge_target =
        singleEdgeTarget(record, "fortran_cray_pointer_pointee_shape");
    if (shape_property_target != shape_edge_target) {
      throw std::runtime_error(
          "AST JSON initialized-name Cray pointee shape property does not "
          "match its exact owned edge");
    }
    if (shape_edge_target != 0) {
      SgExprListExp *shape =
          nodeByIdAs<SgExprListExp>(nodes, shape_edge_target);
      name->set_fortran_cray_pointer_pointee_shape(shape);
      shape->set_parent(name);
    }
    if (uint64_t target = singleEdgeTarget(record, "initptr")) {
      SgInitializer *init = nodeByIdAs<SgInitializer>(nodes, target);
      name->set_initializer(init);
      if (name->get_initializer() != init || init->get_parent() != name) {
        throw std::runtime_error(
            "AST JSON initialized-name failed exact initializer ownership "
            "publication");
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "declptr")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      if (name->get_declptr() != nullptr &&
          name->get_declptr() != declaration) {
        throw std::runtime_error(
            "AST JSON initialized-name declaration owner changed during "
            "edge linking");
      }
      name->set_declptr(declaration);
    }
    if (uint64_t target = singleEdgeTarget(record, "variable_definition")) {
      SgVariableDefinition *definition =
          nodeByIdAs<SgVariableDefinition>(nodes, target);
      if (name->get_definition() != nullptr &&
          name->get_definition() != definition) {
        throw std::runtime_error(
            "AST JSON initialized-name variable definition changed during "
            "edge linking");
      }
      name->set_definition(definition);
    }
    if (uint64_t target = singleEdgeTarget(record, "prev_decl_item")) {
      name->set_prev_decl_item(nodeByIdAs<SgInitializedName>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      name->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
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
    }
    if (uint64_t target = singleEdgeTarget(record, "definingDeclaration")) {
      decl->set_definingDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "declarationScope")) {
      decl->set_declarationScope(nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "nonreal_decl_scope")) {
      SageBuilder::setNonrealDeclarationScope(
          decl, nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "function_declarator_scope")) {
      SageBuilder::adoptFunctionDeclaratorScope(
          decl, nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "template_instantiation_pattern")) {
      SgFunctionDeclaration *pattern =
          nodeByIdAs<SgFunctionDeclaration>(nodes, target);
      if (pattern == decl) {
        throw std::runtime_error(
            "AST JSON function instantiation pattern points to itself");
      }
      decl->set_templateInstantiationPattern(pattern);
    } else {
      decl->set_templateInstantiationPattern(nullptr);
    }
  }
  auto link_result_name = [&](auto *decl) {
    if (uint64_t target = singleEdgeTarget(record, "result_name")) {
      SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, target);
      decl->set_result_name(name);
      if (name->get_parent() == nullptr) {
        name->set_parent(decl);
      }
      if (name->get_scope() == nullptr) {
        name->set_scope(decl->get_scope() != nullptr ? decl->get_scope()
                                                     : nearestScope(decl));
      }
    }
  };
  if (SgProcedureHeaderStatement *decl = isSgProcedureHeaderStatement(node)) {
    link_result_name(decl);
  }
  if (SgEntryStatement *decl = isSgEntryStatement(node)) {
    link_result_name(decl);
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_tpl_params(), record, "tpl_params", nodes, decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_tpl_args(), record, "tpl_args", nodes, decl);
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "conceptConstraint")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_conceptConstraint(expr);
      expr->set_parent(decl);
    }
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgDeclarationStatement *base_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      decl->set_declaration(base_decl);
      if (base_decl->get_parent() == nullptr) {
        base_decl->set_parent(decl);
      }
    }
  }
  if (SgTemplateInstantiationDirectiveStatement *decl =
          isSgTemplateInstantiationDirectiveStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      SgDeclarationStatement *instantiated_decl =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      decl->set_declaration(instantiated_decl);
      instantiated_decl->set_parent(decl);
    }
  }
  if (SgUsingDirectiveStatement *decl = isSgUsingDirectiveStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "namespaceDeclaration")) {
      decl->set_namespaceDeclaration(
          nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.requiredInt("namespace_declaration"))) {
      decl->set_namespaceDeclaration(
          nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, target));
    }
  }
  if (SgUsingDeclarationStatement *decl = isSgUsingDeclarationStatement(node)) {
    uint64_t target = singleEdgeTarget(record, "declaration");
    if (target == 0) {
      target =
          static_cast<uint64_t>(record.properties.requiredInt("declaration"));
    }
    if (target != 0) {
      decl->set_declaration(nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    target = singleEdgeTarget(record, "initializedName");
    if (target == 0) {
      target = static_cast<uint64_t>(
          record.properties.requiredInt("initialized_name"));
    }
    if (target != 0) {
      decl->set_initializedName(nodeByIdAs<SgInitializedName>(nodes, target));
    }
  }
  if (SgUseStatement *stmt = isSgUseStatement(node)) {
    appendEdgeList<SgRenamePairPtrList, SgRenamePair>(
        stmt->get_rename_list(), record, "rename_list", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "module")) {
      stmt->set_module(nodeByIdAs<SgModuleStatement>(nodes, target));
    } else if (const JsonValue *external =
                   record.properties.find("external_module")) {
      stmt->set_module(externalModuleFromJson(*external));
    }
  }
  auto set_template_requires_clause = [&](auto *decl) {
    if (uint64_t target = singleEdgeTarget(record, "requiresClause")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_requiresClause(expr);
      expr->set_parent(decl);
    }
  };
  if (SgTemplateDeclaration *decl = isSgTemplateDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateVariableDeclaration *decl =
          isSgTemplateVariableDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateTypedefDeclaration *decl =
          isSgTemplateTypedefDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node)) {
    auto require_published_list = [&](const auto &published,
                                      const std::string &field) {
      const std::vector<EdgeRecord> &edges = edgesFor(record, field);
      if (published.size() != edges.size()) {
        throw std::runtime_error("AST JSON template class " + field +
                                 " was not published exactly once");
      }
      for (size_t index = 0; index < edges.size(); ++index) {
        if (published[index] != nodeById(nodes, edges[index].target) ||
            published[index]->get_parent() != decl) {
          throw std::runtime_error("AST JSON template class " + field +
                                   " identity changed after early "
                                   "publication");
        }
      }
    };
    require_published_list(decl->get_templateParameters(),
                           "templateParameters");
    require_published_list(decl->get_templateSpecializationArguments(),
                           "templateSpecializationArguments");
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgTemplateClassDeclaration>(nodes, target));
    }
    set_template_requires_clause(decl);
  }
  if (SgTemplateFunctionDeclaration *decl =
          isSgTemplateFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateMemberFunctionDeclaration *decl =
          isSgTemplateMemberFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_templateParameters(), record, "templateParameters", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateSpecializationArguments(), record,
        "templateSpecializationArguments", nodes, decl);
    set_template_requires_clause(decl);
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    auto set_expression =
        [&](const std::string &field,
            void (SgTemplateParameter::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
            (parameter->*setter)(expr);
            expr->set_parent(parameter);
          } else if (const JsonValue *value = record.properties.find(field)) {
            if (SgExpression *expr = expressionFromRef(*value, nodes)) {
              (parameter->*setter)(expr);
              expr->set_parent(parameter);
            }
          }
        };
    set_expression("expression", &SgTemplateParameter::set_expression);
    set_expression("typeConstraint", &SgTemplateParameter::set_typeConstraint);
    set_expression("type_constraint", &SgTemplateParameter::set_typeConstraint);
    set_expression("sourceTypeConstraint",
                   &SgTemplateParameter::set_sourceTypeConstraint);
    set_expression("source_type_constraint",
                   &SgTemplateParameter::set_sourceTypeConstraint);
    set_expression("defaultExpressionParameter",
                   &SgTemplateParameter::set_defaultExpressionParameter);
    set_expression("default_expression_parameter",
                   &SgTemplateParameter::set_defaultExpressionParameter);
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      parameter->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.requiredInt("template_declaration"))) {
      parameter->set_templateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "defaultTemplateDeclarationParameter")) {
      parameter->set_defaultTemplateDeclarationParameter(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (uint64_t target =
                   static_cast<uint64_t>(record.properties.requiredInt(
                       "default_template_declaration_parameter"))) {
      parameter->set_defaultTemplateDeclarationParameter(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "initializedName")) {
      parameter->set_initializedName(
          nodeByIdAs<SgInitializedName>(nodes, target));
    } else if (uint64_t target = static_cast<uint64_t>(
                   record.properties.requiredInt("initialized_name"))) {
      parameter->set_initializedName(
          nodeByIdAs<SgInitializedName>(nodes, target));
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterListPtrList, SgTemplateParameterList>(
        decl->get_sourceSpelledTemplateHeaders(), record,
        "source_spelled_template_headers", nodes, decl);
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    appendEdgeList<SgTemplateParameterListPtrList, SgTemplateParameterList>(
        decl->get_sourceSpelledTemplateHeaders(), record,
        "source_spelled_template_headers", nodes, decl);
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgClassDefinition *def = nodeByIdAs<SgClassDefinition>(nodes, target);
      decl->set_definition(def);
      def->set_declaration(decl);
      def->set_parent(decl);
    }
    if (SgModuleStatement *module = isSgModuleStatement(decl)) {
      if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
        SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
        module->set_end_numeric_label(label);
        label->set_parent(module);
      }
    }
    if (SgDerivedTypeStatement *derived = isSgDerivedTypeStatement(decl)) {
      if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
        SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
        derived->set_end_numeric_label(label);
        label->set_parent(derived);
      }
    }
  }
  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_semanticTemplateArguments(), record,
        "semanticTemplateArguments", nodes, decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateClassDeclaration>(nodes, target));
    }
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateTypedefDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgTemplateInstantiationFunctionDecl *decl =
          isSgTemplateInstantiationFunctionDecl(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateFunctionDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        decl->get_dependentTemplateCandidates(), record,
        "dependentTemplateCandidates", nodes, decl,
        /*owns_children=*/false);
  }
  if (SgTemplateInstantiationMemberFunctionDecl *decl =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    if (uint64_t target = singleEdgeTarget(record, "templateDeclaration")) {
      decl->set_templateDeclaration(
          nodeByIdAs<SgTemplateMemberFunctionDeclaration>(nodes, target));
    }
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes,
        decl);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    if (uint64_t target =
            singleEdgeTarget(record, "specializedTemplateDeclaration")) {
      decl->set_specializedTemplateDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgNamespaceDeclarationStatement *decl =
          isSgNamespaceDeclarationStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "definition");
    SgNamespaceDefinitionStatement *def =
        nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, target);
    if (decl->get_definition() != def || def->get_parent() != decl ||
        def->get_namespaceDeclaration() != decl) {
      throw std::runtime_error(
          "AST JSON namespace edge linking disagrees with its restored "
          "canonical identity");
    }
    const uint64_t introducer_target =
        singleEdgeTarget(record, "opening_introducer_source_fragment");
    const uint64_t opening_target =
        singleEdgeTarget(record, "opening_source_fragment");
    const uint64_t closing_target =
        singleEdgeTarget(record, "closing_source_fragment");
    if ((opening_target == 0) != (closing_target == 0)) {
      throw std::runtime_error(
          "AST JSON namespace has an incomplete source-fragment pair");
    }
    if (introducer_target != 0 && opening_target == 0) {
      throw std::runtime_error(
          "AST JSON namespace has an introducer without a complete "
          "source-fragment pair");
    }
    if (opening_target != 0) {
      SgNamespaceSourceFragment *introducer =
          introducer_target != 0
              ? nodeByIdAs<SgNamespaceSourceFragment>(nodes, introducer_target)
              : nullptr;
      SgNamespaceSourceFragment *opening =
          nodeByIdAs<SgNamespaceSourceFragment>(nodes, opening_target);
      SgNamespaceSourceFragment *closing =
          nodeByIdAs<SgNamespaceSourceFragment>(nodes, closing_target);
      if ((introducer != nullptr && introducer->get_parent() != decl) ||
          opening->get_parent() != decl || closing->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON namespace source fragment has a different owner");
      }
      if (introducer != nullptr) {
        introducer->set_parent(nullptr);
      }
      opening->set_parent(nullptr);
      closing->set_parent(nullptr);
      decl->attach_source_fragments(introducer, opening, closing);
    }
    decl->validate_source_fragments();
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    const uint64_t declaration_target =
        requiredSingleEdgeTarget(record, "namespaceDeclaration");
    const uint64_t global_target =
        requiredSingleEdgeTarget(record, "global_definition");
    const uint64_t previous_target =
        singleEdgeTarget(record, "previousNamespaceDefinition");
    const uint64_t next_target =
        singleEdgeTarget(record, "nextNamespaceDefinition");
    if (def->get_namespaceDeclaration() !=
            nodeByIdAs<SgNamespaceDeclarationStatement>(nodes,
                                                        declaration_target) ||
        def->get_global_definition() !=
            nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, global_target) ||
        def->get_previousNamespaceDefinition() !=
            (previous_target != 0 ? nodeByIdAs<SgNamespaceDefinitionStatement>(
                                        nodes, previous_target)
                                  : nullptr) ||
        def->get_nextNamespaceDefinition() !=
            (next_target != 0 ? nodeByIdAs<SgNamespaceDefinitionStatement>(
                                    nodes, next_target)
                              : nullptr)) {
      throw std::runtime_error(
          "AST JSON namespace definition edge linking disagrees with its "
          "restored canonical chain");
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "cuda_launch_bounds_expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_cuda_launch_bounds_expression(expression);
      expression->set_parent(decl);
    }
    if (uint64_t target = singleEdgeTarget(record, "parameterList")) {
      SgFunctionParameterList *params =
          nodeByIdAs<SgFunctionParameterList>(nodes, target);
      decl->set_parameterList(params);
      if (params->get_parent() == nullptr || params->get_parent() == decl) {
        params->set_parent(decl);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "parameterList_syntax")) {
      decl->set_parameterList_syntax(
          nodeByIdAs<SgFunctionParameterList>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgFunctionDefinition *def =
          nodeByIdAs<SgFunctionDefinition>(nodes, target);
      decl->set_definition(def);
      def->set_parent(decl);
      def->set_declaration(decl);
    }
    if (uint64_t target = singleEdgeTarget(record, "functionParameterScope")) {
      decl->set_functionParameterScope(
          nodeByIdAs<SgFunctionParameterScope>(nodes, target));
    } else if (const JsonValue *external_scope = record.properties.find(
                   "external_function_parameter_scope")) {
      if (!external_scope->requiredBool("present")) {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " external_function_parameter_scope is present but false");
      }
      const std::string source = record.properties.requiredString(
          "external_function_parameter_scope_source");
      SgFunctionDeclaration *peer = nullptr;
      if (source == "firstNondefiningDeclaration") {
        peer = isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration());
      } else if (source == "definingDeclaration") {
        peer = isSgFunctionDeclaration(decl->get_definingDeclaration());
      } else {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " external_function_parameter_scope has unsupported source: " +
            source);
      }
      if (peer == nullptr || peer->get_functionParameterScope() == nullptr) {
        throw std::runtime_error(
            "AST JSON " + record.kind +
            " cannot restore external functionParameterScope from " + source);
      }
      decl->set_functionParameterScope(peer->get_functionParameterScope());
    }
    restoreSharedFunctionParameterScopeBindings(decl);
  }
  if (SgMemberFunctionDeclaration *decl = isSgMemberFunctionDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "CtorInitializerList")) {
      SgCtorInitializerList *ctors =
          nodeByIdAs<SgCtorInitializerList>(nodes, target);
      decl->set_CtorInitializerList(ctors);
      ctors->set_parent(decl);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "associatedClassDeclaration")) {
      decl->set_associatedClassDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    }
  }
  if (SgFunctionDefinition *def = isSgFunctionDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *body = nodeByIdAs<SgBasicBlock>(nodes, target);
      def->set_body(body);
      body->set_parent(def);
    }
  }
  if (SgPragmaDeclaration *decl = isSgPragmaDeclaration(node)) {
    if (uint64_t target = singleEdgeTarget(record, "pragma")) {
      SgPragma *pragma = nodeByIdAs<SgPragma>(nodes, target);
      decl->set_pragma(pragma);
    }
  }
  if (SgPragma *pragma = isSgPragma(node)) {
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      pragma->set_parent(nodeById(nodes, target));
    }
  }
  if (SgExprStatement *stmt = isSgExprStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_expression(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgOmpDepobjStatement *stmt = isSgOmpDepobjStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "depobj")) {
      SgExpression *depobj = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_depobj(depobj);
      depobj->set_parent(stmt);
    }
  }
  if (SgReturnStmt *stmt = isSgReturnStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_expression(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgBreakStmt *stmt = isSgBreakStmt(node)) {
    stmt->set_do_string_label(
        record.properties.requiredString("do_string_label"));
  }
  if (SgContinueStmt *stmt = isSgContinueStmt(node)) {
    stmt->set_do_string_label(
        record.properties.requiredString("do_string_label"));
  }
  if (SgProcessControlStatement *stmt = isSgProcessControlStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "code")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_code(expr);
      expr->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "quiet")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_quiet(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgAllocateStatement *stmt = isSgAllocateStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expr_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_expr_list(child);
      child->set_parent(stmt);
    }
    auto set_expression =
        [&](const std::string &field,
            void (SgAllocateStatement::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
            (stmt->*setter)(child);
            child->set_parent(stmt);
          }
        };
    set_expression("stat_expression",
                   &SgAllocateStatement::set_stat_expression);
    set_expression("errmsg_expression",
                   &SgAllocateStatement::set_errmsg_expression);
    set_expression("source_expression",
                   &SgAllocateStatement::set_source_expression);
    set_expression("mold_expression",
                   &SgAllocateStatement::set_mold_expression);
    set_expression("stream_expression",
                   &SgAllocateStatement::set_stream_expression);
    set_expression("pinned_expression",
                   &SgAllocateStatement::set_pinned_expression);
  }
  if (SgDeallocateStatement *stmt = isSgDeallocateStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expr_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_expr_list(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "stat_expression")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_stat_expression(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "errmsg_expression")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_errmsg_expression(child);
      child->set_parent(stmt);
    }
  }
  if (SgNullifyStatement *stmt = isSgNullifyStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "pointer_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      stmt->set_pointer_list(child);
      child->set_parent(stmt);
    }
  }
  if (SgBasicBlock *stmt = isSgBasicBlock(node)) {
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "conditional")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_conditional(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "true_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_true_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "false_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_false_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "else_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_else_numeric_label(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgForStatement *stmt = isSgForStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "for_init_stmt")) {
      SgForInitStatement *child = nodeByIdAs<SgForInitStatement>(nodes, target);
      stmt->set_for_init_stmt(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "test")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_test(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_increment(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "loop_body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_loop_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgGotoStatement *stmt = isSgGotoStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "label")) {
      stmt->set_label(nodeByIdAs<SgLabelStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "label_expression")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_label_expression(label);
      label->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "selector_expression")) {
      SgExpression *selector = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_selector_expression(selector);
      selector->set_parent(stmt);
    }
  }
  if (SgFortranDo *stmt = isSgFortranDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initialization")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_initialization(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "bound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_bound(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_increment(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *child = nodeByIdAs<SgBasicBlock>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgFortranNonblockedDo *stmt = isSgFortranNonblockedDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "end_statement")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_end_statement(child);
      child->set_parent(stmt);
    }
  }
  if (SgImpliedDo *expr = isSgImpliedDo(node)) {
    if (uint64_t target = singleEdgeTarget(record, "do_var_initialization")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_do_var_initialization(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "last_val")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_last_val(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "increment")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_increment(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "object_list")) {
      SgExprListExp *child = nodeByIdAs<SgExprListExp>(nodes, target);
      expr->set_object_list(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "implied_do_scope")) {
      SgBasicBlock *scope = nodeByIdAs<SgBasicBlock>(nodes, target);
      if (scope->get_implied_do_semantic_scope() == nullptr ||
          scope->get_implied_do_construction_scope() != nullptr ||
          (scope->get_parent() != nullptr && scope->get_parent() != expr)) {
        throw std::runtime_error(
            "AST JSON SgImpliedDo scope has no exact semantic owner");
      }
      expr->set_implied_do_scope(scope);
      scope->set_parent(expr);
      if (scope->get_scope() != scope->get_implied_do_semantic_scope()) {
        throw std::runtime_error(
            "AST JSON SgImpliedDo scope lost its exact outer semantic scope");
      }
    }
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_condition(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "end_numeric_label")) {
      SgLabelRefExp *child = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_end_numeric_label(child);
      child->set_parent(stmt);
    }
  }
  if (SgDoWhileStmt *stmt = isSgDoWhileStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_condition(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgSwitchStatement *stmt = isSgSwitchStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "item_selector")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_item_selector(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *child = nodeByIdAs<SgBasicBlock>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgCaseOptionStmt *stmt = isSgCaseOptionStmt(node)) {
    stmt->set_case_construct_name(
        record.properties.requiredString("case_construct_name"));
    if (uint64_t target = singleEdgeTarget(record, "key")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_key(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "key_range_end")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_key_range_end(child);
      child->set_parent(stmt);
    }
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgDefaultOptionStmt *stmt = isSgDefaultOptionStmt(node)) {
    stmt->set_default_construct_name(
        record.properties.requiredString("default_construct_name"));
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(child);
      child->set_parent(stmt);
    }
  }
  if (SgLambdaExp *lambda = isSgLambdaExp(node)) {
    const uint64_t captures_target =
        singleEdgeTarget(record, "lambda_capture_list");
    const uint64_t closure_target =
        singleEdgeTarget(record, "lambda_closure_class");
    const uint64_t function_target =
        singleEdgeTarget(record, "lambda_function");
    if (captures_target == 0 || closure_target == 0 || function_target == 0) {
      throw std::runtime_error(
          "AST JSON SgLambdaExp requires capture list, closure, and function");
    }
    SgLambdaCaptureList *captures =
        nodeByIdAs<SgLambdaCaptureList>(nodes, captures_target);
    lambda->set_lambda_capture_list(captures);
    captures->set_parent(lambda);
    lambda->set_lambda_closure_class(
        nodeByIdAs<SgClassDeclaration>(nodes, closure_target));
    lambda->set_lambda_function(
        nodeByIdAs<SgFunctionDeclaration>(nodes, function_target));
  }
  if (SgLambdaCaptureList *captures = isSgLambdaCaptureList(node)) {
    appendEdgeList<SgLambdaCapturePtrList, SgLambdaCapture>(
        captures->get_capture_list(), record, "capture_list", nodes, captures);
  }
  if (SgLambdaCapture *capture = isSgLambdaCapture(node)) {
    const uint64_t variable_target =
        singleEdgeTarget(record, "capture_variable");
    if (variable_target == 0) {
      throw std::runtime_error(
          "AST JSON SgLambdaCapture requires capture_variable");
    }
    SgExpression *variable = nodeByIdAs<SgExpression>(nodes, variable_target);
    capture->set_capture_variable(variable);
    variable->set_parent(capture);
    if (uint64_t target = singleEdgeTarget(record, "source_closure_variable")) {
      SgExpression *source = nodeByIdAs<SgExpression>(nodes, target);
      capture->set_source_closure_variable(source);
      source->set_parent(capture);
    }
    if (uint64_t target = singleEdgeTarget(record, "closure_variable")) {
      SgExpression *closure = nodeByIdAs<SgExpression>(nodes, target);
      capture->set_closure_variable(closure);
      closure->set_parent(capture);
    }
  }
  if (SgTryStmt *stmt = isSgTryStmt(node)) {
    const uint64_t body_target = singleEdgeTarget(record, "body");
    const uint64_t catches_target =
        singleEdgeTarget(record, "catch_statement_seq_root");
    if (body_target == 0 || catches_target == 0) {
      throw std::runtime_error(
          "AST JSON SgTryStmt requires body and catch_statement_seq_root");
    }
    SgStatement *body = nodeByIdAs<SgStatement>(nodes, body_target);
    stmt->set_body(body);
    body->set_parent(stmt);

    SgCatchStatementSeq *catches =
        nodeByIdAs<SgCatchStatementSeq>(nodes, catches_target);
    SgCatchStatementSeq *constructor_catches =
        stmt->get_catch_statement_seq_root();
    if (constructor_catches != nullptr && constructor_catches != catches) {
      constructor_catches->set_parent(nullptr);
      delete constructor_catches;
    }
    stmt->set_catch_statement_seq_root(catches);
    catches->set_parent(stmt);
  }
  if (SgCatchStatementSeq *catches = isSgCatchStatementSeq(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        catches->get_catch_statement_seq(), record, "catch_statement_seq",
        nodes, catches);
    if (catches->get_catch_statement_seq().empty()) {
      throw std::runtime_error(
          "AST JSON SgCatchStatementSeq must contain a handler");
    }
  }
  if (SgCatchOptionStmt *handler = isSgCatchOptionStmt(node)) {
    if (uint64_t target = singleEdgeTarget(record, "condition")) {
      SgVariableDeclaration *condition =
          nodeByIdAs<SgVariableDeclaration>(nodes, target);
      handler->set_condition(condition);
      condition->set_parent(handler);
    }
    const uint64_t body_target = singleEdgeTarget(record, "body");
    const uint64_t try_target = singleEdgeTarget(record, "trystmt");
    if (body_target == 0 || try_target == 0) {
      throw std::runtime_error(
          "AST JSON SgCatchOptionStmt requires body and trystmt");
    }
    SgStatement *body = nodeByIdAs<SgStatement>(nodes, body_target);
    handler->set_body(body);
    body->set_parent(handler);
    handler->set_trystmt(nodeByIdAs<SgTryStmt>(nodes, try_target));
  }
  if (SgOmpBodyStatement *stmt = isSgOmpBodyStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgStatement *body = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_body(body);
      body->set_parent(stmt);
    }
  }
  if (SgStaticAssertionDeclaration *decl =
          isSgStaticAssertionDeclaration(node)) {
    const uint64_t condition_target =
        requiredSingleEdgeTarget(record, "condition");
    SgExpression *condition = nodeByIdAs<SgExpression>(nodes, condition_target);
    decl->set_condition(condition);
    condition->set_parent(decl);
    if (uint64_t target = singleEdgeTarget(record, "message")) {
      SgExpression *message = nodeByIdAs<SgExpression>(nodes, target);
      decl->set_message(message);
      message->set_parent(decl);
    }
  }
  if (SgOmpExecStatement *stmt = isSgOmpExecStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "omp_parent")) {
      stmt->set_omp_parent(nodeByIdAs<SgStatement>(nodes, target));
    }
    for (const EdgeRecord &edge : edgesFor(record, "omp_children")) {
      stmt->get_omp_children().push_back(
          nodeByIdAs<SgStatement>(nodes, edge.target));
    }
  }
  if (SgAssignInitializer *init = isSgAssignInitializer(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "operand_i");
    SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
    if (init->get_operand_i() != expr || expr->get_parent() != init) {
      throw std::runtime_error(
          "AST JSON SgAssignInitializer lost its construction-time operand");
    }
  }
  if (SgAggregateInitializer *init = isSgAggregateInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "initializers")) {
      SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
      init->set_initializers(exprs);
      exprs->set_parent(init);
    }
  }
  if (SgDesignatedInitializer *init = isSgDesignatedInitializer(node)) {
    const uint64_t designators_target =
        requiredSingleEdgeTarget(record, "designatorList");
    const uint64_t member_target =
        requiredSingleEdgeTarget(record, "memberInit");
    SgExprListExp *designators =
        nodeByIdAs<SgExprListExp>(nodes, designators_target);
    SgInitializer *member = nodeByIdAs<SgInitializer>(nodes, member_target);
    if (init->get_designatorList() != designators ||
        designators->get_parent() != init || init->get_memberInit() != member ||
        member->get_parent() != init) {
      throw std::runtime_error(
          "AST JSON SgDesignatedInitializer lost its construction-time "
          "children");
    }
  }
  if (SgBracedInitializer *init = isSgBracedInitializer(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "initializers");
    SgExprListExp *exprs = nodeByIdAs<SgExprListExp>(nodes, target);
    if (init->get_initializers() != exprs || exprs->get_parent() != init) {
      throw std::runtime_error(
          "AST JSON SgBracedInitializer lost its construction-time list");
    }
  }
  if (SgFunctionCallExp *call = isSgFunctionCallExp(node)) {
    restoreFunctionCallSourceMetadata(call, record.properties);
    if (uint64_t target = singleEdgeTarget(record, "function")) {
      SgExpression *function = nodeByIdAs<SgExpression>(nodes, target);
      call->set_function(function);
      function->set_parent(call);
    }
    if (uint64_t target = singleEdgeTarget(record, "args")) {
      SgExprListExp *args = nodeByIdAs<SgExprListExp>(nodes, target);
      call->set_args(args);
      args->set_parent(call);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "source_user_defined_literal_operands")) {
      SgExprListExp *lexical = nodeByIdAs<SgExprListExp>(nodes, target);
      call->set_source_user_defined_literal_operands(lexical);
      lexical->set_parent(call);
    }
  }
  if (SgActualArgumentExpression *actual = isSgActualArgumentExpression(node)) {
    actual->set_argument_name(
        SgName(record.properties.requiredString("argument_name")));
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      if (SgExpression *old = actual->get_expression()) {
        if (old != expr) {
          old->set_parent(nullptr);
        }
      }
      actual->set_expression(expr);
      expr->set_parent(actual);
    }
  }
  if (SgAwaitExpression *await_expression = isSgAwaitExpression(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "value");
    SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
    if (operand->get_parent() != nullptr &&
        operand->get_parent() != await_expression) {
      throw std::runtime_error(
          "AST JSON SgAwaitExpression operand has a foreign owner");
    }
    await_expression->set_value(operand);
    operand->set_parent(await_expression);
  }
  if (SgFoldExpression *fold = isSgFoldExpression(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "operands");
    SgExpression *operands = nodeByIdAs<SgExpression>(nodes, target);
    if (operands->get_parent() != nullptr && operands->get_parent() != fold) {
      throw std::runtime_error(
          "AST JSON SgFoldExpression operands have a foreign owner");
    }
    fold->set_operands(operands);
    operands->set_parent(fold);
  }
  if (SgExpression *expr = isSgExpression(node)) {
    if (uint64_t target = singleEdgeTarget(record, "originalExpressionTree")) {
      SgExpression *original = nodeByIdAs<SgExpression>(nodes, target);
      if (original == expr || (original->get_parent() != nullptr &&
                               original->get_parent() != expr)) {
        throw std::runtime_error(
            "AST JSON originalExpressionTree is not an exclusively owned "
            "source-expression edge");
      }
      expr->set_originalExpressionTree(original);
      original->set_parent(expr);
    }
  }
  if (SgTypeTraitBuiltinOperator *op = isSgTypeTraitBuiltinOperator(node)) {
    op->get_args().clear();
    appendEdgeList<SgExpressionPtrList, SgExpression>(op->get_args(), record,
                                                      "args", nodes, op);
    const SgExpressionPtrList &arguments = op->get_args();
    const std::string name = op->get_name().getString();
    auto is_type_operand = [](SgExpression *argument) {
      SgTypeExpression *type_expression = isSgTypeExpression(argument);
      SgType *represented_type = type_expression != nullptr
                                     ? type_expression->get_represented_type()
                                     : nullptr;
      return type_expression != nullptr && represented_type != nullptr &&
             isSgTypeUnknown(represented_type) == nullptr &&
             isSgTypeDefault(represented_type) == nullptr;
    };
    auto is_value_operand = [&](SgExpression *argument) {
      return argument != nullptr && !is_type_operand(argument);
    };
    const bool valid_type_trait =
        op->get_builtin_operator_kind() ==
            SgTypeTraitBuiltinOperator::e_type_trait_builtin &&
        !name.empty() && !arguments.empty() &&
        std::all_of(
            arguments.begin(), arguments.end(), [&](SgExpression *argument) {
              return is_type_operand(argument) || is_value_operand(argument);
            });
    const bool valid_offsetof =
        op->get_builtin_operator_kind() ==
            SgTypeTraitBuiltinOperator::e_offsetof_builtin &&
        name == "__builtin_offsetof" && arguments.size() == 2 &&
        is_type_operand(arguments[0]) && is_value_operand(arguments[1]);
    const bool valid_convert_vector =
        op->get_builtin_operator_kind() ==
            SgTypeTraitBuiltinOperator::e_convert_vector_builtin &&
        name == "__builtin_convertvector" && arguments.size() == 2 &&
        is_value_operand(arguments[0]) && is_type_operand(arguments[1]);
    if (!valid_type_trait && !valid_offsetof && !valid_convert_vector) {
      throw std::runtime_error(
          "AST JSON SgTypeTraitBuiltinOperator has invalid typed argument "
          "roles");
    }
  }
  if (SgUnaryOp *op = isSgUnaryOp(node)) {
    const uint64_t target = singleEdgeTarget(record, "operand_i");
    const bool rethrow = isSgThrowOp(op) != nullptr &&
                         isSgThrowOp(op)->get_throwKind() == SgThrowOp::rethrow;
    if (rethrow) {
      if (target != 0 || op->get_operand_i() != nullptr) {
        throw std::runtime_error(
            "AST JSON rethrow SgThrowOp must not own an operand");
      }
    } else {
      const uint64_t required_target =
          requiredSingleEdgeTarget(record, "operand_i");
      SgExpression *operand = nodeByIdAs<SgExpression>(nodes, required_target);
      if (operand->get_parent() != op) {
        throw std::runtime_error(
            "AST JSON unary operand does not name its exact owner");
      }
      op->set_operand_i(operand);
    }
  }
  if (SgThrowOp *throw_op = isSgThrowOp(node)) {
    const bool has_operand = throw_op->get_operand_i() != nullptr;
    if ((throw_op->get_throwKind() == SgThrowOp::throw_expression) !=
        has_operand) {
      throw std::runtime_error(
          "AST JSON SgThrowOp operand does not match throw_kind");
    }
  }
  if (SgNoexceptOp *op = isSgNoexceptOp(node)) {
    SgExpression *operand = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "operand_expr"));
    if (operand->get_parent() != op) {
      throw std::runtime_error(
          "AST JSON SgNoexceptOp operand does not name its exact owner");
    }
    op->set_operand_expr(operand);
  }
  if (SgBinaryOp *op = isSgBinaryOp(node)) {
    SgExpression *lhs = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "lhs_operand_i"));
    SgExpression *rhs = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "rhs_operand_i"));
    if (lhs == rhs || lhs->get_parent() != op || rhs->get_parent() != op) {
      throw std::runtime_error(
          "AST JSON binary operands do not have exclusive exact ownership");
    }
    op->set_lhs_operand_i(lhs);
    op->set_rhs_operand_i(rhs);
  }
  if (SgComplexVal *value = isSgComplexVal(node)) {
    if (const JsonValue *type = record.properties.find("precision_type")) {
      value->set_precisionType(typeFromJson(*type, nodes));
    }
    if (uint64_t target = singleEdgeTarget(record, "real_value")) {
      SgExpression *real = nodeByIdAs<SgExpression>(nodes, target);
      value->set_real_value(real);
      real->set_parent(value);
    } else if (const JsonValue *real_json =
                   record.properties.find("real_value")) {
      SgExpression *real = expressionFromRef(*real_json, nodes);
      value->set_real_value(real);
      if (real != nullptr) {
        real->set_parent(value);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "imaginary_value")) {
      SgExpression *imag = nodeByIdAs<SgExpression>(nodes, target);
      value->set_imaginary_value(imag);
      imag->set_parent(value);
    } else if (const JsonValue *imag_json =
                   record.properties.find("imaginary_value")) {
      SgExpression *imag = expressionFromRef(*imag_json, nodes);
      value->set_imaginary_value(imag);
      if (imag != nullptr) {
        imag->set_parent(value);
      }
    }
    value->set_valueString(record.properties.requiredString("value_string"));
  }
  if (SgSubscriptExpression *expr = isSgSubscriptExpression(node)) {
    if (uint64_t target = singleEdgeTarget(record, "lowerBound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_lowerBound(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "upperBound")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_upperBound(child);
      child->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "stride")) {
      SgExpression *child = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_stride(child);
      child->set_parent(expr);
    }
  }
  if (SgSizeOfOp *op = isSgSizeOfOp(node)) {
    op->set_is_objectless_nonstatic_data_member_reference(
        record.properties.requiredBool(
            "is_objectless_nonstatic_data_member_reference"));
    op->set_is_sizeof_pack(record.properties.requiredBool("is_sizeof_pack"));
    if (const JsonValue *type = record.properties.find("operand_type")) {
      if (type->requiredBool("present")) {
        op->set_operand_type(typeFromJson(*type, nodes));
      } else {
        op->set_operand_type(nullptr);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "operand_expr")) {
      SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
      op->set_operand_expr(operand);
      operand->set_parent(op);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "type_defining_declaration")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      op->set_type_defining_declaration(declaration);
      declaration->set_parent(op);
    }
    if ((op->get_operand_expr() == nullptr) ==
        (op->get_operand_type() == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgSizeOfOp must have exactly one operand");
    }
    if (op->get_type_defining_declaration() != nullptr &&
        (op->get_operand_type() == nullptr ||
         op->get_type_defining_declaration()->get_parent() != op)) {
      throw std::runtime_error(
          "AST JSON SgSizeOfOp has malformed inline type ownership");
    }
  }
  if (SgAlignOfOp *op = isSgAlignOfOp(node)) {
    const JsonValue &type = record.properties.at("operand_type");
    if (type.requiredBool("present")) {
      op->set_operand_type(typeFromJson(type, nodes));
    } else {
      op->set_operand_type(nullptr);
    }
    if (uint64_t target = singleEdgeTarget(record, "operand_expr")) {
      SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
      op->set_operand_expr(operand);
      operand->set_parent(op);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "type_defining_declaration")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      op->set_type_defining_declaration(declaration);
      declaration->set_parent(op);
    }
    if ((op->get_operand_expr() == nullptr) ==
        (op->get_operand_type() == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgAlignOfOp must have exactly one operand");
    }
    if (op->get_type_defining_declaration() != nullptr &&
        (op->get_operand_type() == nullptr ||
         op->get_type_defining_declaration()->get_parent() != op)) {
      throw std::runtime_error(
          "AST JSON SgAlignOfOp has malformed inline type ownership");
    }
  }
  if (SgCastExp *op = isSgCastExp(node)) {
    const JsonValue &base_path_json =
        record.properties.at("conversion_base_path");
    if (base_path_json.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON SgCastExp conversion_base_path is not an array");
    }
    SgTypePtrList base_path;
    for (const JsonValue &entry : base_path_json.array) {
      SgType *base_type = typeFromJson(entry, nodes);
      if (base_type == nullptr || isSgTypeUnknown(base_type) != nullptr ||
          isSgTypeDefault(base_type) != nullptr ||
          SageInterface::containsUnknownType(base_type)) {
        throw std::runtime_error(
            "AST JSON SgCastExp has an inexact conversion base path");
      }
      base_path.push_back(base_type);
    }
    op->set_conversion_base_path(base_path);
    if (uint64_t target =
            singleEdgeTarget(record, "type_defining_declaration")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      op->set_type_defining_declaration(declaration);
      declaration->set_parent(op);
    }
    if (op->get_type_defining_declaration() != nullptr &&
        op->get_type_defining_declaration()->get_parent() != op) {
      throw std::runtime_error(
          "AST JSON SgCastExp has malformed inline type ownership");
    }
  }
  if (SgPseudoDestructorRefExp *pseudo = isSgPseudoDestructorRefExp(node)) {
    SgType *object_type =
        typeFromJson(record.properties.at("object_type"), nodes);
    SgType *expression_type = typeFromJson(record.properties.at("type"), nodes);
    if (object_type == nullptr || expression_type == nullptr) {
      throw std::runtime_error(
          "AST JSON SgPseudoDestructorRefExp has incomplete type identity");
    }
    pseudo->set_object_type(object_type);
    pseudo->set_expression_type(expression_type);
  }
  if (SgConditionalExp *expr = isSgConditionalExp(node)) {
    SgExpression *condition = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "conditional_exp"));
    const uint64_t true_target = singleEdgeTarget(record, "true_exp");
    SgExpression *true_expression =
        true_target != 0 ? nodeByIdAs<SgExpression>(nodes, true_target)
                         : nullptr;
    SgExpression *false_expression = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "false_exp"));
    const bool standard = expr->get_operator_kind() ==
                          SgConditionalExp::e_conditional_operator_standard;
    const bool gnu_binary = expr->get_operator_kind() ==
                            SgConditionalExp::e_conditional_operator_gnu_binary;
    if ((!standard && !gnu_binary) ||
        standard != (true_expression != nullptr) ||
        condition == true_expression || condition == false_expression ||
        (true_expression != nullptr &&
         (true_expression == false_expression ||
          true_expression->get_parent() != expr)) ||
        condition->get_parent() != expr ||
        false_expression->get_parent() != expr) {
      throw std::runtime_error(
          "AST JSON conditional operands do not have exclusive exact "
          "ownership");
    }
    expr->set_conditional_exp(condition);
    expr->set_true_exp(true_expression);
    expr->set_false_exp(false_expression);
  }
  if (SgStatementExpression *expr = isSgStatementExpression(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "statement");
    SgBasicBlock *stmt = nodeByIdAs<SgBasicBlock>(nodes, target);
    SgScopeStatement *semantic_scope =
        stmt->get_statement_expression_semantic_scope();
    if ((stmt->get_parent() != nullptr && stmt->get_parent() != expr) ||
        semantic_scope == nullptr ||
        stmt->get_statement_expression_construction_scope() != nullptr) {
      throw std::runtime_error(
          "AST JSON SgStatementExpression body has a foreign owner or no "
          "semantic scope");
    }
    expr->set_statement(stmt);
    stmt->set_parent(expr);
    if (stmt->get_scope() != semantic_scope) {
      throw std::runtime_error(
          "AST JSON SgStatementExpression body lost its exact semantic "
          "scope");
    }
  }
  if (SgConstructorInitializer *init = isSgConstructorInitializer(node)) {
    if (uint64_t target = singleEdgeTarget(record, "declaration")) {
      init->set_declaration(
          nodeByIdAs<SgMemberFunctionDeclaration>(nodes, target));
    }
    SgExprListExp *args = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "args"));
    if (init->get_args() != args || args->get_parent() != init) {
      throw std::runtime_error(
          "AST JSON SgConstructorInitializer lost its construction-time args");
    }
  }
  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    clause->set_has_source_variables_override(
        record.properties.requiredBool("has_source_variables_override"));
    if (uint64_t target = singleEdgeTarget(record, "variables")) {
      SgExprListExp *vars = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_variables(vars);
      vars->set_parent(clause);
    }
    if (uint64_t target = singleEdgeTarget(record, "source_variables")) {
      SgExprListExp *vars = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_source_variables(vars);
      vars->set_parent(clause);
    }
    if (clause->get_has_source_variables_override() !=
        (clause->get_source_variables() != nullptr)) {
      throw std::runtime_error(
          "AST JSON OpenMP source-variable override discriminator and edge "
          "disagree");
    }
  }
  if (SgOmpExclusiveClause *clause = isSgOmpExclusiveClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variables")) {
      SgExprListExp *vars = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_variables(vars);
      vars->set_parent(clause);
    }
  }
  if (SgOmpExpressionClause *clause = isSgOmpExpressionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_expression(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpDefaultClause *clause = isSgOmpDefaultClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variant_directive")) {
      SgStatement *stmt = nodeByIdAs<SgStatement>(nodes, target);
      clause->set_variant_directive(stmt);
      stmt->set_parent(clause);
    }
  }
  if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    }
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         clause->get_user_defined_identifier()->get_spelling().empty())) {
      throw std::runtime_error(
          "AST JSON OpenMP reduction identifier kind and owned payload "
          "disagree");
    }
  }
  if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "step")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_step(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpAlignedClause *clause = isSgOmpAlignedClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "alignment")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_alignment(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpScheduleClause *clause = isSgOmpScheduleClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "chunk_size")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_chunk_size(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpDistScheduleClause *clause = isSgOmpDistScheduleClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "chunk_size")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_chunk_size(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpDestroyClause *clause = isSgOmpDestroyClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_expression(expression);
      expression->set_parent(clause);
    }
  }
  if (SgOmpDoacrossClause *clause = isSgOmpDoacrossClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expressions")) {
      SgExprListExp *expressions = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_expressions(expressions);
      expressions->set_parent(clause);
    }
  }
  if (SgOmpOtherwiseClause *clause = isSgOmpOtherwiseClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variant_directive")) {
      SgStatement *directive = nodeByIdAs<SgStatement>(nodes, target);
      clause->set_variant_directive(directive);
      directive->set_parent(clause);
    }
  }
  if (SgOmpInductionClause *clause = isSgOmpInductionClause(node)) {
    appendEdgeList<SgOmpInductionItemPtrList, SgOmpInductionItem>(
        clause->get_items(), record, "items", nodes, clause);
  }
  if (SgOmpApplyClause *clause = isSgOmpApplyClause(node)) {
    appendEdgeList<SgOmpApplyTransformationPtrList, SgOmpApplyTransformation>(
        clause->get_transformations(), record, "transformations", nodes,
        clause);
  }
  if (SgOmpInitModifierList *modifier_list = isSgOmpInitModifierList(node)) {
    appendEdgeList<SgOmpInitModifierPtrList, SgOmpInitModifier>(
        modifier_list->get_modifiers(), record, "modifiers", nodes,
        modifier_list);
  }
  if (SgOmpAppendArgsOperation *operation = isSgOmpAppendArgsOperation(node)) {
    const uint64_t modifier_list_target =
        requiredSingleEdgeTarget(record, "modifier_list");
    SgOmpInitModifierList *modifier_list =
        nodeByIdAs<SgOmpInitModifierList>(nodes, modifier_list_target);
    operation->set_modifier_list(modifier_list);
    modifier_list->set_parent(operation);
  }
  if (SgOmpInitClause *clause = isSgOmpInitClause(node)) {
    const uint64_t modifier_list_target =
        requiredSingleEdgeTarget(record, "modifier_list");
    SgOmpInitModifierList *modifier_list =
        nodeByIdAs<SgOmpInitModifierList>(nodes, modifier_list_target);
    clause->set_modifier_list(modifier_list);
    modifier_list->set_parent(clause);
    const uint64_t operand_target = requiredSingleEdgeTarget(record, "operand");
    SgExpression *operand = nodeByIdAs<SgExpression>(nodes, operand_target);
    clause->set_operand(operand);
    operand->set_parent(clause);
  }
  if (SgOmpInductionItem *item = isSgOmpInductionItem(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
    item->set_expression(expression);
    expression->set_parent(item);
  }
  if (SgOmpApplyTransformation *item = isSgOmpApplyTransformation(node)) {
    if (uint64_t target = singleEdgeTarget(record, "argument")) {
      SgExpression *argument = nodeByIdAs<SgExpression>(nodes, target);
      item->set_argument(argument);
      argument->set_parent(item);
    }
    if (uint64_t target = singleEdgeTarget(record, "nested_apply")) {
      SgOmpApplyClause *nested = nodeByIdAs<SgOmpApplyClause>(nodes, target);
      item->set_nested_apply(nested);
      nested->set_parent(item);
    }
  }
  if (SgOmpInitModifier *modifier = isSgOmpInitModifier(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      modifier->set_expression(expression);
      expression->set_parent(modifier);
    }
  }
  if (SgOmpMapDistDataPolicy *policy = isSgOmpMapDistDataPolicy(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      policy->set_expression(expression);
      expression->set_parent(policy);
    }
  }
  if (SgOmpMapItem *item = isSgOmpMapItem(node)) {
    const uint64_t expression_target =
        requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression =
        nodeByIdAs<SgExpression>(nodes, expression_target);
    item->set_expression(expression);
    expression->set_parent(item);
    appendEdgeList<SgOmpMapDistDataPolicyPtrList, SgOmpMapDistDataPolicy>(
        item->get_policies(), record, "policies", nodes, item);
  }
  if (SgOmpIteratorDefinition *definition = isSgOmpIteratorDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "iterator_type")) {
      SgTypeExpression *type = nodeByIdAs<SgTypeExpression>(nodes, target);
      definition->set_iterator_type(type);
      type->set_parent(definition);
    }
    const uint64_t name_target =
        requiredSingleEdgeTarget(record, "iterator_name");
    SgOmpNameExpression *name =
        nodeByIdAs<SgOmpNameExpression>(nodes, name_target);
    definition->set_iterator_name(name);
    name->set_parent(definition);
    const uint64_t begin_target = requiredSingleEdgeTarget(record, "begin");
    SgExpression *begin = nodeByIdAs<SgExpression>(nodes, begin_target);
    definition->set_begin(begin);
    begin->set_parent(definition);
    const uint64_t end_target = requiredSingleEdgeTarget(record, "end");
    SgExpression *end = nodeByIdAs<SgExpression>(nodes, end_target);
    definition->set_end(end);
    end->set_parent(definition);
    if (uint64_t target = singleEdgeTarget(record, "step")) {
      SgExpression *step = nodeByIdAs<SgExpression>(nodes, target);
      definition->set_step(step);
      step->set_parent(definition);
    }
    if (definition->get_iterator_name()->get_spelling().empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP iterator definition has an empty typed name");
    }
    std::set<SgExpression *> fields = {definition->get_iterator_name(),
                                       definition->get_begin(),
                                       definition->get_end()};
    const size_t required_field_count =
        3 + (definition->get_iterator_type() != nullptr) +
        (definition->get_step() != nullptr);
    if (definition->get_iterator_type() != nullptr) {
      fields.insert(definition->get_iterator_type());
    }
    if (definition->get_step() != nullptr) {
      fields.insert(definition->get_step());
    }
    if (fields.size() != required_field_count) {
      throw std::runtime_error(
          "AST JSON OpenMP iterator definition aliases one syntax node "
          "across roles");
    }
  }
  if (SgOmpFlushStatement *stmt = isSgOmpFlushStatement(node)) {
    linkRequiredOmpVariableList(stmt, record, nodes);
  }
  if (SgOmpThreadprivateStatement *stmt = isSgOmpThreadprivateStatement(node)) {
    appendEdgeList<SgExpressionPtrList, SgExpression>(
        stmt->get_variables(), record, "variables", nodes, stmt);
  }
  if (SgOmpAllocateStatement *stmt = isSgOmpAllocateStatement(node)) {
    linkRequiredOmpVariableList(stmt, record, nodes);
  }
  if (SgOmpExtImplementationDefinedRequirementClause *clause =
          isSgOmpExtImplementationDefinedRequirementClause(node)) {
    if (uint64_t target =
            singleEdgeTarget(record, "implementation_defined_requirement")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_implementation_defined_requirement(expr);
      expr->set_parent(clause);
    }
  }
  if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_modifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_modifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *modifier =
                   record.properties.find("user_defined_modifier")) {
      if (SgExpression *expr = expressionFromRef(*modifier, nodes)) {
        clause->set_user_defined_modifier(expr);
        expr->set_parent(clause);
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "alignment")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_alignment(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *alignment =
                   record.properties.find("alignment")) {
      if (SgExpression *expr = expressionFromRef(*alignment, nodes)) {
        clause->set_alignment(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_modifier")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      clause->set_user_defined_modifier(expr);
      expr->set_parent(clause);
    } else if (const JsonValue *modifier =
                   record.properties.find("user_defined_modifier")) {
      if (SgExpression *expr = expressionFromRef(*modifier, nodes)) {
        clause->set_user_defined_modifier(expr);
        expr->set_parent(clause);
      }
    }
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "arguments")) {
      SgExprListExp *arguments = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_arguments(arguments);
      arguments->set_parent(clause);
    }
  }
  if (SgOmpAppendArgsClause *clause = isSgOmpAppendArgsClause(node)) {
    appendEdgeList<SgOmpAppendArgsOperationPtrList, SgOmpAppendArgsOperation>(
        clause->get_interop_operations(), record, "interop_operations", nodes,
        clause);
    if (clause->get_interop_operations().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpAppendArgsClause has no interop operations");
    }
  }
  if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    }
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         clause->get_user_defined_identifier()->get_spelling().empty())) {
      throw std::runtime_error(
          "AST JSON OpenMP in_reduction identifier kind and owned payload "
          "disagree");
    }
  }
  if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_user_defined_identifier(expr);
      expr->set_parent(clause);
    }
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         clause->get_user_defined_identifier()->get_spelling().empty())) {
      throw std::runtime_error(
          "AST JSON OpenMP task_reduction identifier kind and owned payload "
          "disagree");
    }
  }
  if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause);
    const SgOmpClause::omp_map_modifier_enum modifiers[] = {
        clause->get_modifier1(), clause->get_modifier2(),
        clause->get_modifier3()};
    bool saw_unspecified_modifier = false;
    std::set<int> unique_modifiers;
    size_t mapper_modifier_count = 0;
    size_t iterator_modifier_count = 0;
    for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
      if (modifier == SgOmpClause::e_omp_map_modifier_unspecified) {
        saw_unspecified_modifier = true;
        continue;
      }
      if (saw_unspecified_modifier ||
          !unique_modifiers.insert(static_cast<int>(modifier)).second) {
        throw std::runtime_error(
            "AST JSON OpenMP map modifiers are not a unique contiguous "
            "sequence");
      }
      mapper_modifier_count +=
          modifier == SgOmpClause::e_omp_map_modifier_mapper ? 1 : 0;
      iterator_modifier_count +=
          modifier == SgOmpClause::e_omp_map_modifier_iterator ? 1 : 0;
    }
    if (mapper_modifier_count > 1 || iterator_modifier_count > 1 ||
        (mapper_modifier_count == 1) !=
            (clause->get_mapper_identifier() != nullptr) ||
        (clause->get_mapper_identifier() != nullptr &&
         (clause->get_mapper_identifier()->get_spelling().empty() ||
          clause->get_mapper_identifier()->get_parent() != clause)) ||
        (iterator_modifier_count == 1) !=
            !clause->get_iterator_definitions().empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP map modifiers and structural payloads disagree");
    }
  }
  if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause);
    if (uint64_t target = singleEdgeTarget(record, "sink_vectors")) {
      SgExprListExp *vectors = nodeByIdAs<SgExprListExp>(nodes, target);
      clause->set_sink_vectors(vectors);
      vectors->set_parent(clause);
    }
    const bool requires_iterator = clause->get_depend_modifier() ==
                                   SgOmpClause::e_omp_depend_modifier_iterator;
    if (requires_iterator != !clause->get_iterator_definitions().empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP depend modifier and owned iterator definitions "
          "disagree");
    }
    const bool requires_sink_vectors =
        clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
    if (requires_sink_vectors != (clause->get_sink_vectors() != nullptr)) {
      throw std::runtime_error(
          "AST JSON OpenMP depend kind and owned sink-vector edge disagree");
    }
  }
  if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause);
    const bool requires_iterator =
        clause->get_affinity_modifier() ==
        SgOmpClause::e_omp_affinity_modifier_iterator;
    if (requires_iterator != !clause->get_iterator_definitions().empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP affinity modifier and owned iterator definitions "
          "disagree");
    }
  }
  if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause);
    const bool requires_iterator =
        clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator;
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
    if (requires_iterator != !clause->get_iterator_definitions().empty() ||
        requires_mapper != (clause->get_mapper_identifier() != nullptr) ||
        (clause->get_mapper_identifier() != nullptr &&
         (clause->get_mapper_identifier()->get_spelling().empty() ||
          clause->get_mapper_identifier()->get_parent() != clause)) ||
        (clause->get_declare_target_extended_list() &&
         (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
          clause->get_mapper_identifier() != nullptr ||
          !clause->get_iterator_definitions().empty()))) {
      throw std::runtime_error(
          "AST JSON OpenMP to kind and structural payloads disagree");
    }
  }
  if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    if (uint64_t target = singleEdgeTarget(record, "mapper_identifier")) {
      SgOmpNameExpression *expr =
          nodeByIdAs<SgOmpNameExpression>(nodes, target);
      clause->set_mapper_identifier(expr);
      expr->set_parent(clause);
    }
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause);
    const bool requires_iterator =
        clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator;
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
    if (requires_iterator != !clause->get_iterator_definitions().empty() ||
        requires_mapper != (clause->get_mapper_identifier() != nullptr) ||
        (clause->get_mapper_identifier() != nullptr &&
         (clause->get_mapper_identifier()->get_spelling().empty() ||
          clause->get_mapper_identifier()->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP from kind and structural payloads disagree");
    }
  }
  if (SgOmpContextSelectorSet *set = isSgOmpContextSelectorSet(node)) {
    appendEdgeList<SgOmpContextSelectorPtrList, SgOmpContextSelector>(
        set->get_selectors(), record, "selectors", nodes, set);
    if (set->get_selectors().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpContextSelectorSet has no selectors");
    }
  }
  if (SgOmpContextSelectorProperty *property =
          isSgOmpContextSelectorProperty(node)) {
    if (uint64_t target = singleEdgeTarget(record, "expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      property->set_expression(expression);
      expression->set_parent(property);
    }
    if (uint64_t target = singleEdgeTarget(record, "requires_expression")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      property->set_requires_expression(expression);
      expression->set_parent(property);
    }
  }
  if (SgOmpContextSelector *selector = isSgOmpContextSelector(node)) {
    auto set_expression =
        [&](const std::string &field,
            void (SgOmpContextSelector::*setter)(SgExpression *)) {
          if (uint64_t target = singleEdgeTarget(record, field)) {
            SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
            (selector->*setter)(expression);
            expression->set_parent(selector);
          }
        };
    set_expression("score", &SgOmpContextSelector::set_score);
    appendEdgeList<SgOmpContextSelectorPropertyPtrList,
                   SgOmpContextSelectorProperty>(
        selector->get_properties(), record, "properties", nodes, selector);
    if (uint64_t target = singleEdgeTarget(record, "construct_directive")) {
      SgStatement *directive = nodeByIdAs<SgStatement>(nodes, target);
      selector->set_construct_directive(directive);
      directive->set_parent(selector);
    }
  }
  if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    appendEdgeList<SgOmpContextSelectorSetPtrList, SgOmpContextSelectorSet>(
        clause->get_context_selector_sets(), record, "context_selector_sets",
        nodes, clause);
    if (clause->get_context_selector_sets().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpWhenClause has no context selector sets");
    }
    if (uint64_t target = singleEdgeTarget(record, "variant_directive")) {
      SgStatement *stmt = nodeByIdAs<SgStatement>(nodes, target);
      clause->set_variant_directive(stmt);
      stmt->set_parent(clause);
    }
  }
  if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    appendEdgeList<SgOmpContextSelectorSetPtrList, SgOmpContextSelectorSet>(
        clause->get_context_selector_sets(), record, "context_selector_sets",
        nodes, clause);
    if (clause->get_context_selector_sets().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpMatchClause has no context selector sets");
    }
  }
  if (SgOmpUsesAllocatorsDefination *definition =
          isSgOmpUsesAllocatorsDefination(node)) {
    if (uint64_t target = singleEdgeTarget(record, "user_defined_allocator")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      definition->set_user_defined_allocator(expr);
      expr->set_parent(definition);
    }
    if (uint64_t target = singleEdgeTarget(record, "allocator_traits_array")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      definition->set_allocator_traits_array(expr);
      expr->set_parent(definition);
    }
  }
  if (SgNewExp *expr = isSgNewExp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "placement_args")) {
      SgExprListExp *args = nodeByIdAs<SgExprListExp>(nodes, target);
      expr->set_placement_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "constructor_args")) {
      SgConstructorInitializer *args =
          nodeByIdAs<SgConstructorInitializer>(nodes, target);
      expr->set_constructor_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "builtin_args")) {
      SgExpression *args = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_builtin_args(args);
      args->set_parent(expr);
    }
    if (uint64_t target = singleEdgeTarget(record, "newOperatorDeclaration")) {
      expr->set_newOperatorDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }
  if (SgDeleteExp *expr = isSgDeleteExp(node)) {
    if (uint64_t target = singleEdgeTarget(record, "variable")) {
      SgExpression *variable = nodeByIdAs<SgExpression>(nodes, target);
      expr->set_variable(variable);
      variable->set_parent(expr);
    }
    if (uint64_t target =
            singleEdgeTarget(record, "deleteOperatorDeclaration")) {
      expr->set_deleteOperatorDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }
  if (SgPackExpansionExpr *expr = isSgPackExpansionExpr(node)) {
    const uint64_t target =
        requiredSingleEdgeTarget(record, "pattern_expression");
    SgExpression *pattern = nodeByIdAs<SgExpression>(nodes, target);
    if (pattern->get_parent() != nullptr && pattern->get_parent() != expr) {
      throw std::runtime_error(
          "AST JSON SgPackExpansionExpr pattern has a foreign owner");
    }
    expr->set_pattern_expression(pattern);
    pattern->set_parent(expr);
  }
  if (SgNonrealRefExp *expr = isSgNonrealRefExp(node)) {
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        expr->get_templateArguments(), record, "templateArguments", nodes,
        expr);
  }
}

void validateRestoredOmpOwnedClausePayloads(const AstFileRecord &ast,
                                            const NodeMap &nodes) {
  auto fail = [](SgNode *node, const std::string &detail) {
    throw std::runtime_error("AST JSON reconstructed malformed " +
                             std::string(node != nullptr
                                             ? node->sage_class_name()
                                             : "OpenMP owned payload") +
                             ": " + detail);
  };
  auto validate_name = [&](SgOmpClause *owner, SgOmpNameExpression *name,
                           bool required, const char *role) {
    if (required != (name != nullptr) ||
        (name != nullptr &&
         (name->get_spelling().empty() || name->get_parent() != owner))) {
      fail(owner, std::string(role) +
                      " discriminator and exact name ownership disagree");
    }
  };
  auto validate_iterators =
      [&](SgOmpClause *owner, const SgOmpIteratorDefinitionPtrList &definitions,
          bool required) {
        if (required != !definitions.empty()) {
          fail(owner,
               "iterator discriminator and definition cardinality disagree");
        }
        for (SgOmpIteratorDefinition *definition : definitions) {
          if (definition == nullptr || definition->get_parent() != owner ||
              std::count(definitions.begin(), definitions.end(), definition) !=
                  1 ||
              definition->get_iterator_name() == nullptr ||
              definition->get_iterator_name()->get_spelling().empty() ||
              definition->get_begin() == nullptr ||
              definition->get_end() == nullptr ||
              (definition->get_iterator_type() != nullptr &&
               definition->get_iterator_type()->get_parent() != definition) ||
              definition->get_iterator_name()->get_parent() != definition ||
              definition->get_begin()->get_parent() != definition ||
              definition->get_end()->get_parent() != definition ||
              (definition->get_step() != nullptr &&
               definition->get_step()->get_parent() != definition)) {
            fail(owner, "iterator definition has invalid structural ownership");
          }
          std::set<SgExpression *> fields = {definition->get_iterator_name(),
                                             definition->get_begin(),
                                             definition->get_end()};
          const size_t required_field_count =
              3 + (definition->get_iterator_type() != nullptr) +
              (definition->get_step() != nullptr);
          if (definition->get_iterator_type() != nullptr) {
            fields.insert(definition->get_iterator_type());
          }
          if (definition->get_step() != nullptr) {
            fields.insert(definition->get_step());
          }
          if (fields.size() != required_field_count) {
            fail(owner, "iterator definition aliases one node across roles");
          }
        }
      };

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgOmpInitClause *clause = isSgOmpInitClause(node)) {
      std::string detail;
      if (!Rose::OpenMP::Detail::validateInitClause(clause, &detail)) {
        fail(clause, detail);
      }
    }
    if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
      std::string detail;
      if (!Rose::OpenMP::Detail::validateAdjustArgsClause(clause, &detail)) {
        fail(clause, detail);
      }
    }
    if (SgOmpAppendArgsClause *clause = isSgOmpAppendArgsClause(node)) {
      std::string detail;
      if (!Rose::OpenMP::Detail::validateAppendArgsClause(clause, &detail)) {
        fail(clause, detail);
      }
    }
    if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
      validate_name(clause, clause->get_user_defined_identifier(),
                    clause->get_identifier() ==
                        SgOmpClause::e_omp_reduction_user_defined_identifier,
                    "reduction identifier");
    }
    if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
      validate_name(clause, clause->get_user_defined_identifier(),
                    clause->get_identifier() ==
                        SgOmpClause::e_omp_in_reduction_user_defined_identifier,
                    "in_reduction identifier");
    }
    if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
      validate_name(
          clause, clause->get_user_defined_identifier(),
          clause->get_identifier() ==
              SgOmpClause::e_omp_task_reduction_user_defined_identifier,
          "task_reduction identifier");
    }
    if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
      const SgOmpClause::omp_map_modifier_enum modifiers[] = {
          clause->get_modifier1(), clause->get_modifier2(),
          clause->get_modifier3()};
      bool saw_unspecified = false;
      std::set<int> unique_modifiers;
      size_t mapper_count = 0;
      size_t iterator_count = 0;
      for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
        if (modifier == SgOmpClause::e_omp_map_modifier_unspecified) {
          saw_unspecified = true;
          continue;
        }
        if (saw_unspecified ||
            !unique_modifiers.insert(static_cast<int>(modifier)).second) {
          fail(clause, "map modifiers are not a unique contiguous sequence");
        }
        mapper_count +=
            modifier == SgOmpClause::e_omp_map_modifier_mapper ? 1 : 0;
        iterator_count +=
            modifier == SgOmpClause::e_omp_map_modifier_iterator ? 1 : 0;
      }
      if (mapper_count > 1 || iterator_count > 1) {
        fail(clause, "map has duplicate mapper or iterator modifiers");
      }
      validate_name(clause, clause->get_mapper_identifier(), mapper_count == 1,
                    "map mapper");
      validate_iterators(clause, clause->get_iterator_definitions(),
                         iterator_count == 1);
    }
    if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
      const bool requires_iterator =
          clause->get_depend_modifier() ==
          SgOmpClause::e_omp_depend_modifier_iterator;
      validate_iterators(clause, clause->get_iterator_definitions(),
                         requires_iterator);
      const bool requires_sink =
          clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
      SgExprListExp *vectors = clause->get_sink_vectors();
      if (requires_sink != (vectors != nullptr) ||
          (vectors != nullptr && (vectors->get_parent() != clause ||
                                  vectors->get_expressions().empty()))) {
        fail(clause,
             "sink discriminator and nonempty vector ownership disagree");
      }
      if (vectors != nullptr) {
        const SgExpressionPtrList &expressions = vectors->get_expressions();
        for (SgExpression *expression : expressions) {
          if (expression == nullptr || expression->get_parent() != vectors ||
              std::count(expressions.begin(), expressions.end(), expression) !=
                  1) {
            fail(clause, "sink vector is null, aliased, or incorrectly owned");
          }
        }
      }
    }
    if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
      validate_iterators(clause, clause->get_iterator_definitions(),
                         clause->get_affinity_modifier() ==
                             SgOmpClause::e_omp_affinity_modifier_iterator);
    }
    if (SgOmpToClause *clause = isSgOmpToClause(node)) {
      validate_name(clause, clause->get_mapper_identifier(),
                    clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper,
                    "to mapper");
      validate_iterators(clause, clause->get_iterator_definitions(),
                         clause->get_kind() ==
                             SgOmpClause::e_omp_to_kind_iterator);
      if (clause->get_declare_target_extended_list() &&
          (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
           clause->get_mapper_identifier() != nullptr ||
           !clause->get_iterator_definitions().empty())) {
        fail(clause, "declare-target extended-list state is incompatible");
      }
    }
    if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
      validate_name(clause, clause->get_mapper_identifier(),
                    clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper,
                    "from mapper");
      validate_iterators(clause, clause->get_iterator_definitions(),
                         clause->get_kind() ==
                             SgOmpClause::e_omp_from_kind_iterator);
    }
    if (SgOmpIteratorDefinition *definition = isSgOmpIteratorDefinition(node)) {
      SgOmpClause *owner = isSgOmpClause(definition->get_parent());
      const SgOmpIteratorDefinitionPtrList *definitions = nullptr;
      if (SgOmpMapClause *clause = isSgOmpMapClause(owner)) {
        definitions = &clause->get_iterator_definitions();
      } else if (SgOmpDependClause *clause = isSgOmpDependClause(owner)) {
        definitions = &clause->get_iterator_definitions();
      } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(owner)) {
        definitions = &clause->get_iterator_definitions();
      } else if (SgOmpToClause *clause = isSgOmpToClause(owner)) {
        definitions = &clause->get_iterator_definitions();
      } else if (SgOmpFromClause *clause = isSgOmpFromClause(owner)) {
        definitions = &clause->get_iterator_definitions();
      }
      if (definitions == nullptr ||
          std::count(definitions->begin(), definitions->end(), definition) !=
              1) {
        fail(definition, "iterator definition has no exact owning clause");
      }
    }
  }
}

void linkDeclarationGroupEdges(const AstFileRecord &ast, const NodeMap &nodes) {
  // A group's record can precede its member records.  Link every member's
  // intrinsic edges first so append_declaration can validate the complete
  // typed declaration instead of observing a half-reconstructed shell.
  for (const NodeRecord &record : ast.nodes) {
    SgDeclarationGroupStatement *group =
        isSgDeclarationGroupStatement(nodeById(nodes, record.id));
    if (group == nullptr) {
      continue;
    }
    for (const EdgeRecord &edge : edgesFor(record, "declarations")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, edge.target);
      if (declaration->get_parent() != group) {
        throw std::runtime_error(
            "AST JSON declaration-group member has a different owner");
      }
      declaration->set_parent(nullptr);
      group->append_declaration(declaration);
    }
  }
}

void linkStatementAttributeEdges(const AstFileRecord &ast,
                                 const NodeMap &nodes) {
  // Restore payloads before containers: append_attribute validates each item,
  // and SgAttributedStatement validates the completed non-empty list.
  for (const NodeRecord &record : ast.nodes) {
    SgStatementAttribute *attribute =
        isSgStatementAttribute(nodeById(nodes, record.id));
    if (attribute == nullptr) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "expression_argument")) {
      SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
      if (expression->get_parent() != attribute) {
        throw std::runtime_error(
            "AST JSON statement attribute expression has a different owner");
      }
      expression->set_parent(nullptr);
      attribute->set_expression_argument(expression);
    }
    attribute->validate();
  }

  for (const NodeRecord &record : ast.nodes) {
    SgStatementAttributeList *attribute_list =
        isSgStatementAttributeList(nodeById(nodes, record.id));
    if (attribute_list == nullptr) {
      continue;
    }
    for (const EdgeRecord &edge : edgesFor(record, "attributes")) {
      SgStatementAttribute *attribute =
          nodeByIdAs<SgStatementAttribute>(nodes, edge.target);
      if (attribute->get_parent() != attribute_list) {
        throw std::runtime_error(
            "AST JSON statement attribute list item has a different owner");
      }
      attribute->set_parent(nullptr);
      attribute_list->append_attribute(attribute);
    }
    attribute_list->validate();
  }

  for (const NodeRecord &record : ast.nodes) {
    SgAttributedStatement *statement =
        isSgAttributedStatement(nodeById(nodes, record.id));
    if (statement == nullptr) {
      continue;
    }
    const uint64_t list_target =
        requiredSingleEdgeTarget(record, "attribute_list");
    SgStatementAttributeList *attribute_list =
        nodeByIdAs<SgStatementAttributeList>(nodes, list_target);
    if (attribute_list->get_parent() != statement) {
      throw std::runtime_error(
          "AST JSON attributed statement list has a different owner");
    }
    attribute_list->set_parent(nullptr);
    statement->replace_attribute_list(attribute_list);

    const uint64_t statement_target =
        requiredSingleEdgeTarget(record, "statement");
    SgStatement *wrapped = nodeByIdAs<SgStatement>(nodes, statement_target);
    if (wrapped->get_parent() != statement) {
      throw std::runtime_error(
          "AST JSON attributed statement child has a different owner");
    }
    wrapped->set_parent(nullptr);
    SgStatement *placeholder = statement->replace_statement(wrapped);
    delete placeholder;
    statement->validate();
  }
}

SgScopeStatement *nearestScope(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(current)) {
      return scope;
    }
  }
  return nullptr;
}

SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes);

} // namespace AstJson
} // namespace Rose
