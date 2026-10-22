#include "AstNodes/Expression/BuiltinOperandValidation.h"
#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <limits>

namespace Rose {
namespace AstJson {

namespace {

thread_local bool externalFunctionDeserializationIdentityActive = false;
struct ExternalFunctionConstructionContext {
  uint64_t scope_id;
  std::optional<uint64_t> parent_id;
};
struct ExternalFunctionDeserializationIdentity {
  JsonValue serialized_declaration;
  SgFunctionDeclaration *declaration = nullptr;
  std::optional<ExternalFunctionConstructionContext> context;
  bool parameter_scope_symbols_pending = false;
};
thread_local std::unordered_map<
    uint64_t, std::vector<ExternalFunctionDeserializationIdentity>>
    externalFunctionDeserializationIdentities;
thread_local std::unordered_map<SgSourceFile *,
                                std::vector<SgFunctionDeclaration *>>
    externalFunctionDeclarationsBySource;

ExternalFunctionDeserializationIdentity *
externalFunctionIdentity(const JsonValue &json) {
  auto bucket =
      externalFunctionDeserializationIdentities.find(exactJsonValueHash(json));
  if (bucket != externalFunctionDeserializationIdentities.end())
    for (auto &identity : bucket->second)
      if (identity.serialized_declaration.exactlyEquals(json))
        return &identity;
  return nullptr;
}

template <typename Value>
Value requiredLiteralValue(const JsonValue &properties) {
  static_assert(std::numeric_limits<Value>::is_integer &&
                std::numeric_limits<Value>::digits <= 63);
  const int64_t value = properties.requiredInt("value");
  if (value < static_cast<int64_t>(std::numeric_limits<Value>::min()) ||
      value > static_cast<int64_t>(std::numeric_limits<Value>::max())) {
    throw std::runtime_error("AST JSON integer literal value is out of range");
  }
  return static_cast<Value>(value);
}

int sourceSequenceValueFromJson(const JsonValue &properties) {
  const int64_t value = properties.requiredInt("source_sequence_value");
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max())
    throw std::runtime_error(
        "AST JSON SgStatement source sequence value is out of range");
  return static_cast<int>(value);
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

SgFunctionDeclaration::constexpr_specifier_enum
functionConstexprSpecifierFromJson(const JsonValue &properties,
                                   const std::string &kind) {
  if (properties.find("is_constexpr") != nullptr)
    throw std::runtime_error("AST JSON function retains removed constexpr bit");
  const auto specifier =
      requiredEnum<SgFunctionDeclaration::constexpr_specifier_enum>(
          properties, "constexpr_specifier", kind,
          {SgFunctionDeclaration::e_constexpr_unspecified,
           SgFunctionDeclaration::e_constexpr,
           SgFunctionDeclaration::e_consteval});
  if (specifier != SgFunctionDeclaration::e_constexpr_unspecified &&
      (kind == "SgProgramHeaderStatement" ||
       kind == "SgProcedureHeaderStatement" || kind == "SgEntryStatement"))
    throw std::runtime_error("AST JSON Fortran function has a C++ specifier");
  return specifier;
}

SgIfStmt::if_statement_kind_enum
ifStatementKindFromJson(const JsonValue &properties) {
  const int64_t kind = properties.requiredInt("if_statement_kind");
  switch (kind) {
  case SgIfStmt::e_if_ordinary:
  case SgIfStmt::e_if_constexpr:
  case SgIfStmt::e_if_consteval:
  case SgIfStmt::e_if_not_consteval:
    return static_cast<SgIfStmt::if_statement_kind_enum>(kind);
  default:
    throw std::runtime_error("AST JSON IF has an invalid statement kind");
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

SgOmpClause::omp_default_option_enum
ompDefaultOptionFromJson(const JsonValue &properties) {
  return requiredEnum<SgOmpClause::omp_default_option_enum>(
      properties, "data_sharing", "SgOmpDefaultClause",
      {SgOmpClause::e_omp_default_variant, SgOmpClause::e_omp_default_none,
       SgOmpClause::e_omp_default_shared, SgOmpClause::e_omp_default_private,
       SgOmpClause::e_omp_default_firstprivate});
}

ReturnStatementFields
returnStatementFieldsFromJson(const JsonValue &properties) {
  const auto keyword = requiredEnum<SgReturnStmt::return_keyword_kind_enum>(
      properties, "return_keyword_kind", "SgReturnStmt",
      {SgReturnStmt::e_return_keyword_return,
       SgReturnStmt::e_return_keyword_co_return});
  const uint64_t candidate =
      properties.requiredNonnegativeNodeId("nrvo_candidate");
  if (candidate != 0 && keyword != SgReturnStmt::e_return_keyword_return)
    throw std::runtime_error("AST JSON return has inconsistent NRVO metadata");
  return {keyword, candidate};
}

ExternalFunctionDeserializationIdentityGuard::
    ExternalFunctionDeserializationIdentityGuard(const AstFileRecord &ast) {
  if (externalFunctionDeserializationIdentityActive ||
      !externalFunctionDeserializationIdentities.empty() ||
      !externalFunctionDeclarationsBySource.empty()) {
    throw std::runtime_error(
        "AST JSON external-function deserialization graph is already active");
  }
  decltype(externalFunctionDeserializationIdentities) identities;
  for (const auto &record : ast.nodes) {
    for (const char *field : {"external_first_nondefining_declaration",
                              "external_defining_declaration"}) {
      const auto *reference = record.properties.find(field);
      if (reference == nullptr || !reference->requiredBool("present"))
        continue;
      const auto *function = reference->find("external_function");
      if (function == nullptr)
        continue;
      if (reference->requiredString("kind") != function->requiredString("kind"))
        throw std::runtime_error("AST JSON external declaration kind disagrees "
                                 "with external_function");
      const std::string context = record.kind + "." + field;
      const auto scope_source = reference->requiredString("scope_source");
      const auto scope = reference->requiredNonnegativeNodeId("scope");
      uint64_t scope_id;
      if (scope_source == "node") {
        if (scope == 0)
          throw std::runtime_error("AST JSON " + context +
                                   " has no exact serialized scope");
        scope_id = scope;
      } else if (scope_source == "redeclaration_owner") {
        scope_id = singleEdgeTarget(record, "scope");
        if (scope != 0 || scope_id == 0)
          throw std::runtime_error("AST JSON " + context +
                                   " has an invalid owner-scope contract");
      } else {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid scope_source");
      }
      (void)ast.node(scope_id);
      const auto parent_source = reference->requiredString("parent_source");
      const auto parent = reference->requiredNonnegativeNodeId("parent");
      std::optional<uint64_t> parent_id;
      if (parent_source == "node") {
        if (parent == 0)
          throw std::runtime_error("AST JSON " + context +
                                   " has no exact serialized parent");
        parent_id = parent;
      } else if (parent_source == "redeclaration_owner_parent") {
        parent_id = singleEdgeTarget(record, "parent");
        if (parent != 0 || *parent_id == 0)
          throw std::runtime_error("AST JSON " + context +
                                   " has an invalid owner-parent contract");
      } else if (parent_source == "project") {
        if (parent != 0 || currentDeserializationProject == nullptr)
          throw std::runtime_error("AST JSON " + context +
                                   " has an invalid project-parent contract");
      } else {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid parent_source");
      }
      if (parent_id)
        (void)ast.node(*parent_id);
      auto &bucket = identities[exactJsonValueHash(*function)];
      auto existing =
          std::find_if(bucket.begin(), bucket.end(), [&](const auto &identity) {
            return identity.serialized_declaration.exactlyEquals(*function);
          });
      if (existing != bucket.end()) {
        if (existing->context->scope_id != scope_id ||
            existing->context->parent_id != parent_id)
          throw std::runtime_error(
              "AST JSON external function has conflicting construction owners");
      } else {
        bucket.push_back(
            {*function, nullptr,
             ExternalFunctionConstructionContext{scope_id, parent_id}});
      }
    }
  }
  externalFunctionDeserializationIdentities = std::move(identities);
  externalFunctionDeserializationIdentityActive = true;
}

ExternalFunctionDeserializationIdentityGuard::
    ~ExternalFunctionDeserializationIdentityGuard() {
  externalFunctionDeserializationIdentities.clear();
  externalFunctionDeclarationsBySource.clear();
  externalFunctionDeserializationIdentityActive = false;
}

bool constructExternalFunctionParameterScopeSymbols(const AstFileRecord &ast,
                                                    const NodeMap &nodes,
                                                    bool require_complete) {
  bool constructed = false;
  while (true) {
    std::vector<JsonValue> pending;
    for (const auto &[hash, identities] :
         externalFunctionDeserializationIdentities)
      for (const auto &identity : identities)
        if (identity.parameter_scope_symbols_pending)
          pending.push_back(identity.serialized_declaration);
    bool made_progress = false;
    for (const JsonValue &json : pending) {
      std::unordered_set<uint64_t> ids, symbols, types;
      const JsonValue &scope_json = json.at("function_parameter_scope");
      bool ready = true;
      if (scope_json.requiredBool("symbol_table_present")) {
        for (const JsonValue &entry : scope_json.at("symbol_table").array) {
          for (const char *field :
               {"alias_target", "original_symbol", "alias_target_binding",
                "original_symbol_binding", "alias_causal_nodes"})
            if (const auto *dependency = entry.find(field))
              collectTypeDependencyIds(*dependency, ids, symbols, types);
          for (const char *field :
               {"alias_target_binding", "original_symbol_binding"})
            if (const auto *binding = entry.find(field);
                binding != nullptr && binding->find("binding_local") == nullptr)
              ready &= exactSymbolBindingDependenciesReady(*binding, nodes);
          if (const auto *symbol = entry.find("symbol")) {
            const uint64_t basis =
                symbol->requiredNonnegativeNodeId("symbol_declaration");
            if (basis != 0)
              ids.insert(basis);
            else
              collectTypeDependencyIds(*symbol, ids, symbols, types);
          }
        }
      }
      for (uint64_t id : ids)
        ready &= declarationConstructionDependencyReady(
            ast, nodes, id, symbols.count(id) != 0, types.count(id) != 0);
      if (!ready) {
        if (require_complete)
          throw std::runtime_error("AST JSON external parameter-scope symbols "
                                   "have missing or cyclic dependencies");
        continue;
      }
      auto *identity = externalFunctionIdentity(json);
      ASSERT_not_null(identity);
      SgFunctionDeclaration *function = identity->declaration;
      ASSERT_not_null(function);
      constructExternalFunctionParameterScopeSymbolsFromJson(
          scope_json, nodes, function->get_functionParameterScope(),
          function->get_parameterList()->get_args(),
          json.requiredString("name"));
      externalFunctionIdentity(json)->parameter_scope_symbols_pending = false;
      constructed = made_progress = true;
    }
    if (!made_progress)
      return constructed;
  }
}

SgFunctionDeclaration *restoredExternalFunctionFromJson(const JsonValue &json) {
  if (!externalFunctionDeserializationIdentityActive)
    throw std::runtime_error("AST JSON external function identity lookup is "
                             "outside its transaction");
  auto *identity = externalFunctionIdentity(json);
  return identity != nullptr ? identity->declaration : nullptr;
}

void collectExternalFunctionConstructionDependencies(
    const JsonValue &json, std::unordered_set<uint64_t> &ids) {
  auto *identity = externalFunctionIdentity(json);
  if (identity != nullptr && identity->context) {
    ids.insert(identity->context->scope_id);
    if (identity->context->parent_id)
      ids.insert(*identity->context->parent_id);
  }
}

FunctionReturnTypeSourceFields
functionReturnTypeSourceFieldsFromJson(const JsonValue &properties) {
  FunctionReturnTypeSourceFields fields{
      properties.requiredBool("source_return_type_qualification_present"),
      properties.requiredBool("source_return_type_global_qualification"),
      stringListFromJson(
          properties.at("source_return_type_qualification_tokens"),
          "source_return_type_qualification_tokens"),
      requiredEnum<
          SgFunctionDeclaration::source_return_type_elaboration_kind_enum>(
          properties, "source_return_type_elaboration_kind",
          "SgFunctionDeclaration",
          {SgFunctionDeclaration::e_source_return_type_elaboration_unspecified,
           SgFunctionDeclaration::e_source_return_type_elaboration_none,
           SgFunctionDeclaration::e_source_return_type_elaboration_typename,
           SgFunctionDeclaration::e_source_return_type_elaboration_class,
           SgFunctionDeclaration::e_source_return_type_elaboration_struct,
           SgFunctionDeclaration::e_source_return_type_elaboration_union,
           SgFunctionDeclaration::e_source_return_type_elaboration_enum}),
      properties.requiredBool("type_elaboration_required_for_return_type"),
      properties.requiredNonnegativeInt(
          "name_qualification_length_for_return_type"),
      properties.requiredBool("requires_name_qualification_on_return_type"),
      properties.requiredBool("global_qualification_required_for_return_type")};
  if (!fields.qualified &&
      (fields.globally_qualified || !fields.qualification_tokens.empty())) {
    throw std::runtime_error(
        "AST JSON function return-type source qualifier payload is "
        "present without its presence bit");
  }
  if (fields.qualified &&
      fields.elaboration ==
          SgFunctionDeclaration::e_source_return_type_elaboration_unspecified) {
    throw std::runtime_error(
        "AST JSON source function return type has no exact elaboration kind");
  }
  if (fields.elaboration !=
          SgFunctionDeclaration::e_source_return_type_elaboration_unspecified &&
      fields.elaboration_required !=
          (fields.elaboration !=
           SgFunctionDeclaration::e_source_return_type_elaboration_none)) {
    throw std::runtime_error(
        "AST JSON exact function return-type elaboration disagrees with "
        "its typed boolean projection");
  }
  return fields;
}

FunctionCallSourceFields
functionCallSourceFieldsFromJson(const JsonValue &properties) {
  const bool uses_operator_syntax =
      properties.requiredBool("uses_operator_syntax");
  const auto source_syntax =
      requiredEnum<SgFunctionCallExp::source_syntax_enum>(
          properties, "source_syntax", "SgFunctionCallExp",
          {SgFunctionCallExp::e_source_function_call,
           SgFunctionCallExp::e_implicit_conversion,
           SgFunctionCallExp::e_implicit_destruction});
  const SgFunctionCallExp::source_operator_surface_enum
      source_operator_surface = sourceOperatorSurfaceFromJson(properties);
  const SgFunctionCallExp::source_operator_callee_form_enum
      source_operator_callee_form =
          sourceOperatorCalleeFormFromJson(properties);
  const SgUnsignedCharList source_operator_operand_roles =
      sourceOperatorOperandRolesFromJson(properties);
  const SgName source_user_defined_literal_suffix =
      properties.requiredName("source_user_defined_literal_suffix");
  const SgUnsignedCharList source_user_defined_literal_suffix_roles =
      udlSuffixRolesFromJson(properties);
  const bool user_defined_literal =
      source_operator_surface ==
      SgFunctionCallExp::e_user_defined_literal_surface;
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
      (source_syntax != SgFunctionCallExp::e_source_function_call &&
       has_operator_surface) ||
      user_defined_literal != !source_user_defined_literal_suffix.is_null() ||
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
  const bool literal_modified =
      user_defined_literal
          ? properties.requiredBool("source_user_defined_literal_is_modified")
          : false;
  return {uses_operator_syntax,
          source_syntax,
          source_operator_surface,
          source_operator_callee_form,
          source_operator_operand_roles,
          source_user_defined_literal_suffix,
          source_user_defined_literal_suffix_roles,
          literal_modified};
}

void applyFunctionCallSourceFields(SgFunctionCallExp *call,
                                   const FunctionCallSourceFields &fields) {
  if (call == nullptr)
    throw std::runtime_error(
        "AST JSON function-call source metadata has no call owner");
  call->set_uses_operator_syntax(fields.uses_operator_syntax);
  call->set_source_syntax(fields.syntax);
  call->set_source_operator_surface(fields.surface);
  call->set_source_operator_callee_form(fields.callee_form);
  call->set_source_operator_operand_roles(fields.operand_roles);
  call->set_source_user_defined_literal_suffix(fields.literal_suffix);
  call->set_source_user_defined_literal_suffix_roles(fields.suffix_roles);
  call->set_source_user_defined_literal_was_modified(fields.literal_modified);
}

void validateFunctionCallInputs(const FunctionCallSourceFields &fields,
                                SgExpression *function,
                                SgExprListExp *arguments, SgType *type,
                                SgCxxLiteralTokenList *literal_tokens,
                                size_t argument_count, size_t token_count) {
  if (function == nullptr || arguments == nullptr || function == arguments ||
      function->get_parent() != nullptr || arguments->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON call requires exclusive callee and argument ownership");
  if (SageInterface::containsUnknownType(type) ||
      isSgFunctionType(type) != nullptr ||
      isSgMemberFunctionType(type) != nullptr)
    throw std::runtime_error(
        "AST JSON call requires an exact non-callable result type");
  if (fields.syntax == SgFunctionCallExp::e_implicit_destruction) {
    auto *destructor = isSgMemberFunctionDeclaration(
        SgFunctionCallExp::getAssociatedFunctionDeclarationForCallee(function));
    if (destructor == nullptr ||
        !destructor->get_specialFunctionModifier().isDestructor() ||
        argument_count != 0 || isSgTypeVoid(type) == nullptr)
      throw std::runtime_error(
          "AST JSON implicit destruction requires an exact void destructor "
          "call without arguments");
  }
  if (fields.surface != SgFunctionCallExp::e_no_operator_surface &&
      fields.operand_roles.size() != argument_count)
    throw std::runtime_error(
        "AST JSON call operator roles differ from its arguments");
  const bool udl =
      fields.surface == SgFunctionCallExp::e_user_defined_literal_surface;
  if (udl != (literal_tokens != nullptr))
    throw std::runtime_error(
        "AST JSON call has inconsistent literal-token presence");
  if (literal_tokens != nullptr &&
      (literal_tokens->get_parent() != nullptr || token_count == 0 ||
       token_count != fields.suffix_roles.size()))
    throw std::runtime_error(
        "AST JSON call has invalid literal-token ownership or arity");
}

SgFunctionCallExp *
constructFunctionCall(const FunctionCallSourceFields &fields,
                      SgExpression *function, SgExprListExp *arguments,
                      SgType *type, SgCxxLiteralTokenList *literal_tokens) {
  validateFunctionCallInputs(
      fields, function, arguments, type, literal_tokens,
      arguments ? arguments->get_expressions().size() : 0,
      literal_tokens ? literal_tokens->get_tokens().size() : 0);
  if (!Rose::BuiltinOperands::haveExclusiveOwnership(
          arguments->get_expressions(), arguments))
    throw std::runtime_error(
        "AST JSON call requires exclusive argument children");
  if (std::any_of(arguments->get_expressions().begin(),
                  arguments->get_expressions().end(),
                  [](SgExpression *argument) {
                    return isSgCxxDefaultInitExp(argument) != nullptr;
                  }) &&
      !SgCxxDefaultInitExp::is_valid_binding(
          SgFunctionCallExp::getAssociatedFunctionDeclarationForCallee(
              function),
          arguments->get_expressions()))
    throw std::runtime_error(
        "AST JSON call default argument has no exact parameter binding");
  if (literal_tokens != nullptr) {
    std::unordered_set<SgCxxLiteralToken *> seen;
    for (SgCxxLiteralToken *token : literal_tokens->get_tokens()) {
      if (token == nullptr || token->get_parent() != literal_tokens ||
          !seen.insert(token).second)
        throw std::runtime_error(
            "AST JSON call requires exclusive literal tokens");
    }
  }
  auto *call = new SgFunctionCallExp(function, arguments, type);
  applyFunctionCallSourceFields(call, fields);
  if (literal_tokens != nullptr) {
    call->set_source_user_defined_literal_operands(literal_tokens);
    literal_tokens->set_parent(call);
  }
  return call;
}

SgName requiredTemplateName(const JsonValue &properties,
                            const std::string &kind) {
  const SgName name = properties.requiredName("template_name");
  if (name.is_null()) {
    throw std::runtime_error("AST JSON " + kind +
                             " has an empty template_name");
  }
  return name;
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

SgSourceLocationBuiltinExp::source_location_builtin_kind_enum
sourceLocationBuiltinKindFromJson(const JsonValue &p) {
  return requiredEnum<
      SgSourceLocationBuiltinExp::source_location_builtin_kind_enum>(
      p, "source_location_builtin_kind", "SgSourceLocationBuiltinExp",
      {SgSourceLocationBuiltinExp::e_function,
       SgSourceLocationBuiltinExp::e_function_signature,
       SgSourceLocationBuiltinExp::e_file,
       SgSourceLocationBuiltinExp::e_file_name,
       SgSourceLocationBuiltinExp::e_line, SgSourceLocationBuiltinExp::e_column,
       SgSourceLocationBuiltinExp::e_source_location});
}

SgSourceLocationBuiltinExp *
sourceLocationBuiltinFromProperties(const JsonValue &properties,
                                    const NodeMap &nodes) {
  const auto kind = sourceLocationBuiltinKindFromJson(properties);
  SgType *type = nullableTypeFromJson(properties.at("type"), nodes);
  if (SageInterface::containsUnknownType(type)) {
    throw std::runtime_error(
        "AST JSON SgSourceLocationBuiltinExp has no exact result type");
  }
  return new SgSourceLocationBuiltinExp(kind, type);
}

SgExpression *constructOperandRequirement(OperandRequirementKind kind,
                                          SgExpression *operand,
                                          SgExpression *type_constraint,
                                          bool noexcept_required) {
  if (operand == nullptr || operand->get_parent() != nullptr ||
      (type_constraint != nullptr &&
       (type_constraint == operand ||
        type_constraint->get_parent() != nullptr)) ||
      (kind != OperandRequirementKind::compound &&
       (type_constraint != nullptr || noexcept_required))) {
    throw std::runtime_error(
        "AST JSON requirement needs exact exclusive constructor-time operands");
  }
  SgExpression *result = nullptr;
  switch (kind) {
  case OperandRequirementKind::simple:
    result = new SgSimpleRequirement(nullptr, operand);
    break;
  case OperandRequirementKind::compound:
    result = new SgCompoundRequirement(nullptr, operand, noexcept_required,
                                       type_constraint);
    break;
  case OperandRequirementKind::nested:
    result = new SgNestedRequirement(nullptr, operand);
    break;
  }
  if (result == nullptr)
    throw std::runtime_error("AST JSON invalid operand requirement kind");
  return result;
}

SgTypeRequirement *typeRequirementFromProperties(const JsonValue &properties,
                                                 const NodeMap &nodes) {
  const bool qualification_present =
      properties.requiredBool("explicit_name_qualification_present");
  const bool global_qualification =
      properties.requiredBool("explicit_global_qualification");
  const SgStringList qualification_tokens = stringListFromJson(
      properties.at("explicit_name_qualification_tokens"),
      "SgTypeRequirement explicit_name_qualification_tokens");
  if ((!qualification_present &&
       (global_qualification || !qualification_tokens.empty())) ||
      std::any_of(qualification_tokens.begin(), qualification_tokens.end(),
                  [](const std::string &token) { return token.empty(); })) {
    throw std::runtime_error(
        "AST JSON SgTypeRequirement has contradictory source qualification");
  }
  SgType *type = nullableTypeFromJson(properties.at("required_type"), nodes);
  if (SageInterface::containsUnknownType(type)) {
    throw std::runtime_error(
        "AST JSON SgTypeRequirement has no exact required type");
  }
  auto requirement = std::make_unique<SgTypeRequirement>(nullptr, type);
  requirement->set_explicit_name_qualification_present(qualification_present);
  requirement->set_explicit_global_qualification(global_qualification);
  requirement->set_explicit_name_qualification_tokens(qualification_tokens);
  return requirement.release();
}

SgTypeIdOp *typeIdFromProperties(const JsonValue &properties,
                                 const NodeMap &nodes, SgExpression *operand,
                                 SgType *operand_type,
                                 bool operand_is_evaluated) {
  SgType *result_type = typeFromJson(properties.at("type"), nodes);
  auto *reference = isSgReferenceType(result_type);
  if (reference == nullptr || reference->get_base_type() == nullptr ||
      !SageInterface::isConstType(reference->get_base_type()) ||
      SageInterface::containsUnknownType(result_type))
    throw std::runtime_error(
        "AST JSON typeid requires its exact const reference result type");
  if ((operand == nullptr) == (operand_type == nullptr) ||
      (operand_is_evaluated && operand == nullptr) ||
      (operand != nullptr && operand->get_parent() != nullptr) ||
      (operand_type != nullptr &&
       SageInterface::containsUnknownType(operand_type)))
    throw std::runtime_error("AST JSON typeid requires one exclusive typed "
                             "operand and its evaluation role");
  return new SgTypeIdOp(operand, operand_type, result_type,
                        operand_is_evaluated);
}

SgVarArgOp *varArgFromProperties(const JsonValue &properties,
                                 const NodeMap &nodes, SgExpression *operand) {
  SgType *type = typeFromJson(properties.at("type"), nodes);
  if (type == nullptr || SageInterface::containsUnknownType(type) ||
      operand == nullptr || operand->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON va_arg requires an exclusive operand and exact type");
  return SageBuilder::buildVarArgOp_nfi(operand, type);
}

SgFunctionParameterRefExp *
functionParameterReferenceFromProperties(const JsonValue &properties,
                                         const NodeMap &nodes) {
  const int number = properties.requiredIntInRange(
      "parameter_number", 0, std::numeric_limits<int>::max());
  const int levels = properties.requiredIntInRange(
      "parameter_levels_up", 0, std::numeric_limits<int>::max());
  auto *type = typeFromJson(properties.at("type"), nodes);
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error("AST JSON function parameter reference requires "
                             "an exact semantic type");
  return new SgFunctionParameterRefExp(nullptr, number, levels, type);
}

SgNoexceptOp *noexceptFromProperties(const JsonValue &properties,
                                     const NodeMap &nodes,
                                     SgExpression *operand) {
  if (isSgTypeBool(nullableTypeFromJson(properties.at("type"), nodes)) ==
      nullptr) {
    throw std::runtime_error(
        "AST JSON SgNoexceptOp result type is not exact bool");
  }
  if (operand == nullptr || operand->get_parent() != nullptr) {
    throw std::runtime_error("AST JSON noexcept requires an exclusive operand");
  }
  auto *result = new SgNoexceptOp(operand);
  return result;
}

SgNullptrValExp *nullptrValueFromProperties(const JsonValue &p,
                                            const NodeMap &nodes) {
  const auto spelling = literalSpellingFormFromJson(p);
  SgType *type = nullableTypeFromJson(p.at("type"), nodes);
  if (SageInterface::containsUnknownType(type)) {
    throw std::runtime_error("AST JSON null literal requires an exact null or "
                             "pointer type and matching literal metadata");
  }
  const bool has_literal_type = p.requiredBool("has_literal_semantic_type");
  SgType *canonical =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if ((isSgTypeNullptr(canonical) == nullptr &&
       isSgPointerType(canonical) == nullptr &&
       isSgPointerMemberType(canonical) == nullptr) ||
      (!has_literal_type &&
       (type != canonical || isSgTypeNullptr(type) == nullptr))) {
    throw std::runtime_error("AST JSON null literal requires an exact null or "
                             "pointer type and matching literal metadata");
  }
  auto *value = new SgNullptrValExp();
  value->set_literal_spelling_form(spelling);
  if (has_literal_type)
    value->set_literal_type(type);
  return value;
}

SgEnumVal *enumValueFromProperties(const JsonValue &p, const NodeMap &nodes) {
  const auto spelling = literalSpellingFormFromJson(p);
  const int64_t id = p.requiredInt("declaration");
  if (id <= 0)
    throw std::runtime_error("AST JSON enum literal requires a declaration");
  auto *declaration =
      nodeByIdAs<SgEnumDeclaration>(nodes, static_cast<uint64_t>(id));
  SgType *type = nullableTypeFromJson(p.at("type"), nodes);
  if (isSgEnumType(type) == nullptr || type != declaration->get_type() ||
      !SageInterface::isExactTagTypeIdentity(type, declaration)) {
    throw std::runtime_error(
        "AST JSON enum literal type does not name its declaration");
  }
  const bool has_literal_type = p.requiredBool("has_literal_semantic_type");
  const bool requires_qualification =
      p.requiredBool("requires_name_qualification");
  const auto number = p.requiredInt("value");
  const SgName name = p.requiredName("name");
  auto *value = new SgEnumVal(number, declaration, name);
  value->set_literal_spelling_form(spelling);
  if (has_literal_type)
    value->set_literal_type(type);
  value->set_requiresNameQualification(requires_qualification);
  return value;
}

SgTemplateParameterVal *
templateParameterValueFromProperties(const JsonValue &p, const NodeMap &nodes) {
  const auto spelling = literalSpellingFormFromJson(p);
  const int64_t position = p.requiredInt("template_parameter_position");
  if (position < 0 || position > std::numeric_limits<int>::max()) {
    throw std::runtime_error(
        "AST JSON SgTemplateParameterVal has no exact position");
  }
  const uint64_t parameter_id =
      p.requiredNonnegativeNodeId("template_parameter");
  if (parameter_id == 0) {
    throw std::runtime_error(
        "AST JSON SgTemplateParameterVal requires a positive template "
        "parameter ID");
  }
  SgTemplateParameter *parameter =
      nodeByIdAs<SgTemplateParameter>(nodes, parameter_id);
  SgInitializedName *name = parameter->get_initializedName();
  SgType *type = parameter->get_type();
  if (parameter->get_parameterType() !=
          SgTemplateParameter::nontype_parameter ||
      name == nullptr || name->get_parent() != parameter ||
      name->get_type() != type || SageInterface::containsUnknownType(type) ||
      nullableTypeFromJson(p.at("type"), nodes) != type) {
    throw std::runtime_error(
        "AST JSON SgTemplateParameterVal has no exact typed parameter");
  }
  const bool has_literal_type = p.requiredBool("has_literal_semantic_type");
  auto *value =
      new SgTemplateParameterVal(static_cast<int>(position), parameter);
  value->set_literal_spelling_form(spelling);
  if (has_literal_type) {
    value->set_literal_type(type);
  }
  return value;
}

InitializedNameRoles initializedNameRolesFromJson(const JsonValue &properties) {
  InitializedNameRoles roles;
  roles.ownership =
      requiredEnum<SgInitializedName::enum_constant_source_ownership_enum>(
          properties, "enum_constant_source_ownership", "SgInitializedName",
          {SgInitializedName::e_enum_constant_source_unclassified,
           SgInitializedName::e_enum_constant_source_body,
           SgInitializedName::e_enum_constant_source_external,
           SgInitializedName::e_enum_constant_semantic_only});
  roles.enum_value =
      properties.at("enum_constant_value").kind == JsonValue::Kind::Null
          ? std::nullopt
          : std::optional<long long>(
                properties.requiredInt("enum_constant_value"));
  roles.preinitialization =
      requiredPreinitialization(properties, "SgInitializedName");
  (void)properties.requiredBool("is_parameter_pack");
  (void)properties.requiredBool("is_pack_element");
  (void)properties.requiredBool("deduction_guide_default_available");
  roles.ctor_source_order = requiredOptionalUnsignedInteger(
      properties, "ctor_initializer_source_order", "SgInitializedName");
  roles.ctor_field_target =
      properties.requiredNonnegativeNodeId("ctor_initializer_field_target");
  if ((roles.preinitialization == SgInitializedName::e_data_member) !=
          (roles.ctor_field_target != 0) ||
      (roles.preinitialization ==
           SgInitializedName::e_unknown_preinitialization &&
       roles.ctor_source_order.has_value()))
    throw std::runtime_error("AST JSON initialized-name has invalid "
                             "constructor target or source order");
  if (roles.enum_value.has_value() !=
      (roles.ownership !=
       SgInitializedName::e_enum_constant_source_unclassified)) {
    throw std::runtime_error(
        "AST JSON enum-constant value and source role disagree");
  }
  return roles;
}

static void validateNewConstructionTypes(
    SgType *specified, SgPointerType *result,
    SgNewExp::array_bound_source_form_enum source_form) {
  if (SageInterface::containsUnknownType(specified))
    throw std::runtime_error(
        "AST JSON new-expression requires an exact specified type");
  if (!SgNewExp::specifiedTypeMatchesResult(specified, result, source_form))
    throw std::runtime_error(
        "AST JSON new-expression requires its exact canonical result pointer");
}

SgNewExp::array_bound_source_form_enum
newArrayBoundSourceFormFromJson(const JsonValue &properties) {
  return requiredEnum<SgNewExp::array_bound_source_form_enum>(
      properties, "array_bound_source_form", "SgNewExp",
      {SgNewExp::e_no_array_bound, SgNewExp::e_written_array_bound,
       SgNewExp::e_inferred_array_bound, SgNewExp::e_type_array_bound});
}

NewConstructionFields newConstructionFieldsFromJson(const JsonValue &properties,
                                                    const NodeMap &nodes) {
  const auto global = properties.requiredInt("need_global_specifier");
  if (global != 0 && global != 1)
    throw std::runtime_error(
        "AST JSON new-expression global specifier must be zero or one");
  const bool parenthesized =
      properties.requiredBool("type_id_is_parenthesized");
  const auto implicit_arguments =
      properties.requiredInt("allocation_implicit_argument_count");
  if (implicit_arguments < 1 || implicit_arguments > 3)
    throw std::runtime_error(
        "AST JSON new-expression implicit argument count is out of range");
  const auto source_form = newArrayBoundSourceFormFromJson(properties);
  auto *specified =
      nullableTypeFromJson(properties.at("specified_type"), nodes);
  if (SageInterface::containsUnknownType(specified))
    throw std::runtime_error(
        "AST JSON new-expression requires an exact specified type");
  const JsonValue &type = properties.at("type");
  if (type.kind != JsonValue::Kind::Object ||
      type.requiredString("kind") != "SgPointerType")
    throw std::runtime_error("AST JSON SgNewExp type must be SgPointerType");
  auto *result = isSgPointerType(nullableTypeFromJson(type, nodes));
  validateNewConstructionTypes(specified, result, source_form);
  return {specified,
          result,
          static_cast<short>(global),
          parenthesized,
          static_cast<unsigned int>(implicit_arguments),
          source_form};
}

SgNewExp *constructNewExpression(
    const NewConstructionFields &fields, SgExpression *array_bound,
    SgExprListExp *placement, SgConstructorInitializer *constructor,
    SgExpression *builtin, SgFunctionDeclaration *operator_declaration,
    const SgExpressionPtrList &placement_elements) {
  if (constructor != nullptr && builtin != nullptr)
    throw std::runtime_error(
        "AST JSON new-expression has conflicting initializer operands");
  validateNewConstructionTypes(fields.specified_type, fields.result_type,
                               fields.array_bound_source_form);
  if ((array_bound == nullptr) !=
      (fields.array_bound_source_form == SgNewExp::e_no_array_bound))
    throw std::runtime_error(
        "AST JSON new-expression array bound and source form disagree");
  if (fields.array_bound_source_form == SgNewExp::e_inferred_array_bound ||
      fields.array_bound_source_form == SgNewExp::e_type_array_bound) {
    auto *value = isSgValueExp(array_bound);
    if (value == nullptr || !value->get_type()->isIntegerType() ||
        value->get_literal_spelling_form() !=
            SgValueExp::e_literal_canonical_generated ||
        value->get_file_info() == nullptr ||
        !value->get_file_info()->isCompilerGenerated())
      throw std::runtime_error(
          "AST JSON new-expression requires an exact generated integer bound");
  }
  if (fields.allocation_implicit_argument_count < 1 ||
      fields.allocation_implicit_argument_count > 3)
    throw std::runtime_error(
        "AST JSON new-expression implicit argument count is out of range");
  if (!SgCxxDefaultInitExp::is_valid_binding(
          operator_declaration, placement_elements,
          fields.allocation_implicit_argument_count))
    throw std::runtime_error("AST JSON new-expression default argument has no "
                             "exact parameter binding");
  std::unordered_set<SgExpression *> seen;
  for (SgExpression *child :
       {array_bound, static_cast<SgExpression *>(placement),
        static_cast<SgExpression *>(constructor), builtin}) {
    if (child != nullptr &&
        (child->get_parent() != nullptr || !seen.insert(child).second))
      throw std::runtime_error(
          "AST JSON new-expression operands require exclusive ownership");
  }
  if (constructor != nullptr && constructor->get_need_paren())
    throw std::runtime_error(
        "AST JSON new-expression constructor cannot own generic parentheses");
  if ((placement == nullptr && !placement_elements.empty()) ||
      (placement != nullptr &&
       placement->get_expressions() != placement_elements))
    throw std::runtime_error("AST JSON new-expression placement list differs "
                             "from construction inputs");
  for (auto *element : placement_elements) {
    if (element == nullptr || element->get_parent() != placement ||
        !seen.insert(element).second)
      throw std::runtime_error(
          "AST JSON new-expression placement child has conflicting ownership");
  }
  auto expression = std::make_unique<SgNewExp>(
      fields.specified_type, fields.result_type, array_bound,
      fields.array_bound_source_form, placement, constructor, builtin,
      fields.global_specifier, operator_declaration,
      fields.allocation_implicit_argument_count);
  expression->set_type_id_is_parenthesized(fields.type_id_is_parenthesized);
  return expression.release();
}

SgType *exactTypedefBaseType(const JsonValue &properties,
                             const NodeMap &nodes) {
  SgType *base = nullableTypeFromJson(properties.at("base_type"), nodes);
  if (SageInterface::containsUnknownType(base))
    throw std::runtime_error(
        "AST JSON typedef construction requires an exact base type");
  if (isTypedefOwnedSourceType(base) && base->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON typedef source type already has a lexical owner");
  return base;
}

SgTypedefDeclaration *
constructTypedefDeclaration(const SgName &name, SgType *base_type,
                            SgDeclarationStatement *base_declaration,
                            bool owns_base_declaration, VariantT kind,
                            SgTemplateTypedefDeclaration *template_declaration,
                            const SgTemplateArgumentPtrList &arguments,
                            const SgTemplateArgumentPtrList &deduced_arguments,
                            const TypedefMetadata &metadata) {
  if (SageInterface::containsUnknownType(base_type))
    throw std::runtime_error(
        "AST JSON typedef construction requires an exact base type");
  if (owns_base_declaration && (base_declaration == nullptr ||
                                base_declaration->get_parent() != nullptr))
    throw std::runtime_error(
        "AST JSON typedef base declaration has no exact lexical owner");
  if (kind != V_SgTemplateInstantiationTypedefDeclaration &&
      (!arguments.empty() || !deduced_arguments.empty()))
    throw std::runtime_error(
        "AST JSON ordinary typedef has instantiation arguments");
  std::unordered_set<SgTemplateArgument *> seen;
  for (const auto *list : {&arguments, &deduced_arguments})
    for (auto *argument : *list)
      if (argument == nullptr || argument->get_parent() != nullptr ||
          !seen.insert(argument).second)
        throw std::runtime_error("AST JSON typedef instantiation argument has "
                                 "conflicting ownership");
  // Both allocations stay in RAII ownership until every postcondition
  // holds: generated destructors never adopt child expressions or shared
  // types, so unwinding here cannot consume borrowed input.
  std::unique_ptr<SgTypedefDeclaration> declaration;
  switch (kind) {
  case V_SgTypedefDeclaration:
    if (template_declaration != nullptr)
      throw std::runtime_error("AST JSON ordinary typedef has a template");
    declaration = std::make_unique<SgTypedefDeclaration>(
        name, base_type, nullptr, base_declaration);
    break;
  case V_SgTemplateTypedefDeclaration:
    if (template_declaration != nullptr)
      throw std::runtime_error(
          "AST JSON template typedef has an instantiation target");
    declaration = std::make_unique<SgTemplateTypedefDeclaration>(
        name, base_type, nullptr, base_declaration);
    break;
  case V_SgTemplateInstantiationTypedefDeclaration: {
    if (template_declaration == nullptr)
      throw std::runtime_error(
          "AST JSON template typedef instantiation has no typed template "
          "declaration");
    auto instantiation =
        std::make_unique<SgTemplateInstantiationTypedefDeclaration>(
            name, base_type, nullptr, base_declaration, template_declaration,
            arguments);
    instantiation->set_specializedTemplateDeclaration(template_declaration);
    instantiation->get_deducedTemplateArguments() = deduced_arguments;
    declaration = std::move(instantiation);
    break;
  }
  default:
    throw std::runtime_error("AST JSON invalid typedef declaration kind");
  }
  declaration->set_typedef_type(metadata.kind);
  declaration->set_typedefBaseTypeContainsDefiningDeclaration(
      metadata.base_contains_defining_declaration);
  declaration->set_isAutonomousDeclaration(metadata.autonomous);
  declaration->set_type_elaboration_required_for_base_type(
      metadata.elaboration_required);
  declaration->set_source_base_type_global_qualification(
      metadata.global_qualification);
  declaration->get_source_base_type_qualification_tokens() =
      metadata.qualification_tokens;
  declaration->set_source_base_type_qualification_present(
      metadata.qualification_present);
  std::unique_ptr<SgTypedefType> wrapper =
      std::make_unique<SgTypedefType>(declaration.get());
  declaration->set_type(wrapper.get());
  if (declaration->get_base_type() != base_type ||
      declaration->get_declaration() != base_declaration ||
      declaration->get_type() != wrapper.get() ||
      wrapper->get_declaration() != declaration.get())
    throw std::runtime_error(
        "AST JSON typedef did not bind its exact base type");
  if (isTypedefOwnedSourceType(base_type))
    base_type->set_parent(declaration.get());
  if (owns_base_declaration)
    base_declaration->set_parent(declaration.get());
  for (auto *argument : seen)
    argument->set_parent(declaration.get());
  wrapper.release();
  return declaration.release();
}

SgType *exactInitializedNameType(const JsonValue &properties,
                                 const NodeMap &nodes) {
  SgType *type = nullableTypeFromJson(properties.at("type"), nodes);
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error(
        "AST JSON initialized-name construction requires an exact type");
  return type;
}

SgInitializedName *constructInitializedName(const SgName &name, SgType *type) {
  if (SageInterface::containsUnknownType(type) ||
      (type->has_source_prototype() && type->get_parent() != nullptr))
    throw std::runtime_error(
        "AST JSON initialized-name construction requires an exact type");
  std::unique_ptr<SgInitializedName> initialized_name =
      std::make_unique<SgInitializedName>(nullptr, name, type);
  if (initialized_name->get_typeptr() != type)
    throw std::runtime_error(
        "AST JSON initialized name did not bind its exact type");
  return initialized_name.release();
}

SgUnsignedCharList
aggregateSourceElementRolesFromJson(const JsonValue &properties) {
  const JsonValue &roles = properties.at("source_element_roles");
  if (roles.kind != JsonValue::Kind::Array)
    throw std::runtime_error(
        "AST JSON aggregate source roles are not an array");
  SgUnsignedCharList result;
  for (const JsonValue &entry : roles.array) {
    const int64_t value = entry.asInt();
    if (value < SgAggregateInitializer::e_source_element_ast ||
        value > SgAggregateInitializer::
                    e_source_element_include_expansion_after_owner_separator ||
        (result.empty() && value >=
                               SgAggregateInitializer::
                                   e_source_element_ast_after_owner_separator))
      throw std::runtime_error("AST JSON aggregate has an invalid source role");
    result.push_back(static_cast<unsigned char>(value));
  }
  return result;
}

AssignmentInitializerSourceFields
assignmentInitializerSourceFieldsFromJson(const JsonValue &properties) {
  return {requiredEnum<
              SgAssignInitializer::assignment_initializer_source_form_enum>(
              properties, "assignment_initializer_source_form",
              "SgAssignInitializer",
              {SgAssignInitializer::e_assignment_initializer_source_ast,
               SgAssignInitializer::
                   e_assignment_initializer_source_include_operand_expansion,
               SgAssignInitializer::
                   e_assignment_initializer_source_include_complete_expansion}),
          properties.requiredBool("is_braced_initialized")};
}

AggregateConstructionFields
aggregateConstructionFieldsFromJson(const JsonValue &properties,
                                    const NodeMap &nodes,
                                    SgUnsignedCharList source_roles) {
  const bool braced = properties.requiredBool("is_braced_initialized");
  const auto source_form = requiredEnum<
      SgAggregateInitializer::aggregate_initializer_source_form_enum>(
      properties, "aggregate_source_form", "SgAggregateInitializer",
      {SgAggregateInitializer::e_aggregate_initializer_source_braced,
       SgAggregateInitializer::e_aggregate_initializer_source_unbraced,
       SgAggregateInitializer::e_aggregate_initializer_source_typed_braced,
       SgAggregateInitializer::e_aggregate_initializer_source_compound_literal,
       SgAggregateInitializer::e_aggregate_initializer_source_fortran,
       SgAggregateInitializer::
           e_aggregate_initializer_source_fortran_structure});
  const bool has_explicit_type =
      properties.requiredBool("fortran_has_source_explicit_type");
  SgType *type = nullableTypeFromJson(properties.at("type"), nodes);
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error(
        "AST JSON aggregate requires an exact semantic type");
  AggregateConstructionFields fields{
      source_form,
      braced,
      type,
      has_explicit_type,
      nullableTypeFromJson(properties.at("fortran_source_explicit_type"),
                           nodes),
      exactBoundSymbolFromJson(
          properties.at("fortran_source_derived_type_symbol"), nodes),
      std::move(source_roles)};
  validateFortranAggregateConstruction(fields);
  return fields;
}

SgAlignmentSpecifier::spelling_enum
alignmentSpellingFromProperties(const JsonValue &properties, bool has_operand) {
  using Alignment = SgAlignmentSpecifier;
  const auto spelling = requiredEnum<Alignment::spelling_enum>(
      properties, "alignment_spelling", "SgAlignmentSpecifier",
      {Alignment::e_gnu, Alignment::e_cxx11_gnu, Alignment::e_c23_gnu,
       Alignment::e_declspec, Alignment::e_alignas, Alignment::e_Alignas});
  const bool pack = properties.requiredBool("alignment_pack_expansion");
  const bool gnu = spelling == Alignment::e_gnu ||
                   spelling == Alignment::e_cxx11_gnu ||
                   spelling == Alignment::e_c23_gnu;
  if ((!has_operand && !gnu) || (pack && spelling != Alignment::e_alignas))
    throw std::runtime_error(
        "AST JSON alignment has incompatible spelling and operand");
  return spelling;
}

AsmOperandConstructionFields
asmOperandFieldsFromJson(const JsonValue &properties) {
  AsmOperandConstructionFields fields;
  const JsonValue &p = properties.at("asm_constraint");
  auto &constraint = fields.constraint;
  constraint.spelling = p.requiredString("spelling");
  constraint.allows_memory = p.requiredBool("allows_memory");
  constraint.allows_register = p.requiredBool("allows_register");
  constraint.requires_immediate = p.requiredBool("requires_immediate");
  constraint.has_matching_input = p.requiredBool("has_matching_input");
  constraint.tied_output =
      p.requiredIntInRange("tied_output", -1, std::numeric_limits<int>::max());
  const JsonValue &range = p.at("immediate_range");
  if (range.kind != JsonValue::Kind::Null) {
    const int minimum =
        range.requiredIntInRange("minimum", std::numeric_limits<int>::min(),
                                 std::numeric_limits<int>::max());
    const int maximum =
        range.requiredIntInRange("maximum", std::numeric_limits<int>::min(),
                                 std::numeric_limits<int>::max());
    if (minimum > maximum)
      throw std::runtime_error("AST JSON asm immediate range is reversed");
    constraint.immediate_range = {minimum, maximum};
  }
  const JsonValue &values = p.at("immediate_values");
  if (values.kind != JsonValue::Kind::Array)
    throw std::runtime_error("AST JSON asm immediate values are not an array");
  for (const JsonValue &value : values.array) {
    const int64_t number = value.asInt();
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max() ||
        !constraint.immediate_values.insert(static_cast<int>(number)).second)
      throw std::runtime_error(
          "AST JSON asm immediate value is invalid or repeated");
  }
  const JsonValue &bounds = p.at("output_bounds");
  if (bounds.kind != JsonValue::Kind::Null) {
    const int64_t minimum = bounds.requiredInt("minimum");
    const int64_t maximum = bounds.requiredInt("maximum");
    if (minimum < 0 || maximum < 0 ||
        minimum > std::numeric_limits<unsigned>::max() ||
        maximum > std::numeric_limits<unsigned>::max())
      throw std::runtime_error(
          "AST JSON asm output bounds exceed unsigned range");
    constraint.output_bounds = {static_cast<unsigned>(minimum),
                                static_cast<unsigned>(maximum)};
  }
  fields.modifiers = static_cast<SgAsmOp::asm_operand_modifier_enum>(
      properties.requiredIntInRange("asm_modifiers", 1, 255));
  fields.name = properties.requiredString("name");
  const bool output = (fields.modifiers & SgAsmOp::e_output) != 0;
  if (constraint.spelling.empty() ||
      (fields.modifiers & SgAsmOp::e_modify) == 0 ||
      (output && constraint.tied_output != -1) ||
      (!output && (fields.modifiers & SgAsmOp::e_earlyclobber)) ||
      (!constraint.requires_immediate &&
       (constraint.immediate_range || !constraint.immediate_values.empty())))
    throw std::runtime_error(
        "AST JSON asm operand has inconsistent typed roles");
  return fields;
}

AsmStatementConstructionFields
asmStatementFieldsFromJson(const JsonValue &properties) {
  AsmStatementConstructionFields fields;
  fields.assembly = properties.requiredString("assembly_code");
  fields.extended = properties.requiredBool("gnu_extended_format");
  fields.is_volatile = properties.requiredBool("asm_volatile");
  const auto clobbers =
      stringListFromJson(properties.at("asm_clobbers"), "asm_clobbers");
  fields.clobbers.assign(clobbers.begin(), clobbers.end());
  if ((!fields.extended && !fields.clobbers.empty()) ||
      std::any_of(fields.clobbers.begin(), fields.clobbers.end(),
                  [](const std::string &name) { return name.empty(); }))
    throw std::runtime_error("AST JSON asm statement has invalid clobbers");
  return fields;
}

SgAsmOp *constructAsmOperand(const AsmOperandConstructionFields &fields,
                             SgExpression *expression) {
  if (expression == nullptr || expression->get_parent() != nullptr ||
      !expression->has_semantic_value_type() ||
      SageInterface::containsUnknownType(expression->get_type()))
    throw std::runtime_error(
        "AST JSON asm requires an exclusive typed expression");
  auto *operand = new SgAsmOp(fields.constraint, fields.modifiers, expression);
  expression->set_parent(operand);
  operand->set_name(fields.name);
  return operand;
}

SgAsmStmt *constructAsmStatement(const AsmStatementConstructionFields &fields,
                                 const SgExpressionPtrList &operands) {
  if (!fields.extended && !operands.empty())
    throw std::runtime_error("AST JSON simple asm has extended operands");
  std::unordered_set<SgExpression *> seen;
  size_t output_count = 0;
  bool has_input = false;
  for (auto *expression : operands) {
    auto *operand = isSgAsmOp(expression);
    if (operand == nullptr || operand->get_parent() != nullptr ||
        !seen.insert(operand).second)
      throw std::runtime_error(
          "AST JSON asm requires exclusive operand children");
    if (operand->get_isOutputOperand()) {
      if (has_input)
        throw std::runtime_error(
            "AST JSON asm output follows an input operand");
      ++output_count;
    } else {
      has_input = true;
      const int tied = operand->get_constraint().tied_output;
      if (tied != -1 && static_cast<size_t>(tied) >= output_count)
        throw std::runtime_error("AST JSON asm input names no output operand");
    }
  }
  auto *statement = new SgAsmStmt;
  statement->set_firstNondefiningDeclaration(statement);
  statement->set_definingDeclaration(statement);
  statement->set_assemblyCode(fields.assembly);
  statement->set_useGnuExtendedFormat(fields.extended);
  statement->set_isVolatile(fields.is_volatile);
  statement->get_clobberRegisterList() = fields.clobbers;
  statement->get_operands() = operands;
  for (auto *operand : operands)
    operand->set_parent(statement);
  return statement;
}

SgAlignmentSpecifier *
alignmentSpecifierFromProperties(const JsonValue &properties,
                                 SgExpression *operand) {
  using Alignment = SgAlignmentSpecifier;
  const auto spelling =
      alignmentSpellingFromProperties(properties, operand != nullptr);
  if (isSgTypeExpression(operand) != nullptr &&
      spelling != Alignment::e_alignas && spelling != Alignment::e_Alignas)
    throw std::runtime_error(
        "AST JSON alignment has incompatible spelling and type operand");
  if (operand != nullptr) {
    const auto *type_operand = isSgTypeExpression(operand);
    SgType *type = type_operand != nullptr
                       ? type_operand->get_represented_type()
                       : operand->get_type();
    if (operand->get_parent() != nullptr ||
        SageInterface::containsUnknownType(type))
      throw std::runtime_error(
          "AST JSON alignment requires an unowned typed operand");
  }
  return new Alignment(spelling, operand,
                       properties.requiredBool("alignment_pack_expansion"));
}

SgAggregateInitializer *
constructAggregateInitializer(const AggregateConstructionFields &fields,
                              SgExprListExp *initializers,
                              const SgExpressionPtrList &elements) {
  if (!fields.source_roles.empty() &&
      fields.source_roles.size() != elements.size())
    throw std::runtime_error(
        "AST JSON aggregate source role count differs from elements");
  if (initializers == nullptr || initializers->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON aggregate requires an exclusive initializer list");
  if (initializers->get_expressions() != elements)
    throw std::runtime_error(
        "AST JSON aggregate initializer list differs from construction inputs");
  std::unordered_set<SgExpression *> seen;
  for (auto *element : elements) {
    if (element == nullptr || element == initializers ||
        element->get_parent() != initializers || !seen.insert(element).second)
      throw std::runtime_error(
          "AST JSON aggregate child has conflicting ownership");
  }
  auto result = std::make_unique<SgAggregateInitializer>(
      initializers, fields.type, fields.source_form);
  result->set_is_braced_initialized(fields.braced);
  result->set_fortran_has_source_explicit_type(fields.has_explicit_type);
  result->set_fortran_source_explicit_type(fields.explicit_type);
  result->set_fortran_source_derived_type_symbol(fields.source_symbol);
  result->set_source_element_roles(fields.source_roles);
  return result.release();
}

SgChooseExpression::selection_kind_enum
chooseSelectionFromJson(const JsonValue &p) {
  return requiredEnum<SgChooseExpression::selection_kind_enum>(
      p, "selection_kind", "SgChooseExpression",
      {SgChooseExpression::e_choose_selection_dependent,
       SgChooseExpression::e_choose_selection_true,
       SgChooseExpression::e_choose_selection_false});
}

SgChooseExpression *constructChooseExpression(
    SgType *type, SgChooseExpression::selection_kind_enum selection,
    SgExpression *condition, SgExpression *true_expression,
    SgExpression *false_expression) {
  if (SageInterface::containsUnknownType(type) ||
      selection < SgChooseExpression::e_choose_selection_dependent ||
      selection > SgChooseExpression::e_choose_selection_false ||
      condition == nullptr || true_expression == nullptr ||
      false_expression == nullptr || condition == true_expression ||
      condition == false_expression || true_expression == false_expression ||
      condition->get_parent() != nullptr ||
      true_expression->get_parent() != nullptr ||
      false_expression->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON choose expression requires exclusive operands, exact type "
        "and typed selection");
  return new SgChooseExpression(condition, true_expression, false_expression,
                                type, selection);
}

SgConditionalExp::conditional_operator_kind_enum
conditionalOperatorKindFromJson(const JsonValue &p) {
  return requiredEnum<SgConditionalExp::conditional_operator_kind_enum>(
      p, "conditional_operator_kind", "SgConditionalExp",
      {SgConditionalExp::e_conditional_operator_standard,
       SgConditionalExp::e_conditional_operator_gnu_binary});
}

SgConditionalExp *
conditionalFromProperties(const JsonValue &p, const NodeMap &nodes,
                          SgConditionalExp::conditional_operator_kind_enum kind,
                          SgExpression *condition,
                          SgExpression *true_expression,
                          SgExpression *false_expression) {
  ROSE_ASSERT(kind == SgConditionalExp::e_conditional_operator_standard ||
              kind == SgConditionalExp::e_conditional_operator_gnu_binary);
  if ((kind == SgConditionalExp::e_conditional_operator_standard) !=
          (true_expression != nullptr) ||
      condition == nullptr || false_expression == nullptr)
    throw std::runtime_error("AST JSON conditional has invalid operand arity");
  if (condition == false_expression || condition == true_expression ||
      condition->get_parent() != nullptr ||
      false_expression->get_parent() != nullptr ||
      (true_expression != nullptr &&
       (true_expression == false_expression ||
        true_expression->get_parent() != nullptr)))
    throw std::runtime_error(
        "AST JSON conditional requires exclusive operands");
  SgType *type = nullableTypeFromJson(p.at("type"), nodes);
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error("AST JSON conditional has no exact result type");
  auto *result = new SgConditionalExp(condition, true_expression,
                                      false_expression, type, kind);
  result->validate();
  return result;
}

SgTypeTraitBuiltinOperator *
typeTraitBuiltinFromProperties(const JsonValue &p, const NodeMap &nodes,
                               const SgExpressionPtrList &arguments) {
  const SgName name = p.requiredName("name");
  if (name.is_null())
    throw std::runtime_error("AST JSON type-trait builtin has an empty name");
  const auto kind =
      requiredEnum<SgTypeTraitBuiltinOperator::builtin_operator_kind_enum>(
          p, "builtin_operator_kind", "SgTypeTraitBuiltinOperator",
          {SgTypeTraitBuiltinOperator::e_type_trait_builtin,
           SgTypeTraitBuiltinOperator::e_offsetof_builtin,
           SgTypeTraitBuiltinOperator::e_convert_vector_builtin});
  SgType *type = nullableTypeFromJson(p.at("type"), nodes);
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error(
        "AST JSON type-trait builtin has no exact result type");
  if (!Rose::BuiltinOperands::haveExclusiveOwnership(arguments, nullptr))
    throw std::runtime_error(
        "AST JSON type-trait builtin requires exclusive operands");
  if (!Rose::BuiltinOperands::haveValidRoles(kind, name, arguments)) {
    throw std::runtime_error(
        "AST JSON SgTypeTraitBuiltinOperator has invalid typed argument "
        "roles");
  }
  auto result = std::make_unique<SgTypeTraitBuiltinOperator>(name, kind, type);
  result->get_args() = arguments;
  for (SgExpression *argument : arguments)
    argument->set_parent(result.get());
  return result.release();
}

SgThisExp *thisExpressionFromProperties(const JsonValue &p,
                                        const NodeMap &nodes) {
  SgClassSymbol *class_symbol = nullptr;
  SgNonrealSymbol *nonreal_symbol = nullptr;
  const int64_t raw_class_decl_id = p.requiredInt("class_symbol_declaration");
  const int64_t raw_nonreal_decl_id =
      p.requiredInt("nonreal_symbol_declaration");
  if (raw_class_decl_id < 0 || raw_nonreal_decl_id < 0 ||
      (raw_class_decl_id == 0) == (raw_nonreal_decl_id == 0)) {
    throw std::runtime_error(
        "AST JSON SgThisExp must name exactly one class or nonreal "
        "symbol declaration");
  }
  if (raw_class_decl_id != 0) {
    SgClassDeclaration *declaration = nodeByIdAs<SgClassDeclaration>(
        nodes, static_cast<uint64_t>(raw_class_decl_id));
    class_symbol = classSymbolForDeclaration(declaration);
  } else {
    SgNonrealDecl *declaration = nodeByIdAs<SgNonrealDecl>(
        nodes, static_cast<uint64_t>(raw_nonreal_decl_id));
    nonreal_symbol = nonrealSymbolForDeclaration(declaration);
  }

  SgType *type = nullableTypeFromJson(p.at("type"), nodes);
  SgPointerType *pointer = isSgPointerType(type);
  if (pointer == nullptr || SageInterface::containsUnknownType(pointer)) {
    throw std::runtime_error(
        "AST JSON SgThisExp result type is not an exact pointer type");
  }
  SgType *symbol_type = class_symbol != nullptr ? class_symbol->get_type()
                                                : nonreal_symbol->get_type();
  SgType *pointee = pointer->get_base_type()->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if (symbol_type != nullptr) {
    symbol_type = symbol_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                         SgType::STRIP_TYPEDEF_TYPE);
  }
  if (SageInterface::containsUnknownType(symbol_type) ||
      pointee != symbol_type) {
    throw std::runtime_error(
        "AST JSON SgThisExp pointer type does not match its symbol type");
  }
  return new SgThisExp(class_symbol, nonreal_symbol, type);
}

FoldConstructionFields foldConstructionFieldsFromJson(const JsonValue &p) {
  const auto kind = requiredEnum<SgFoldExpression::operator_kind_enum>(
      p, "operator_kind", "SgFoldExpression",
      {SgFoldExpression::e_fold_add,
       SgFoldExpression::e_fold_subtract,
       SgFoldExpression::e_fold_multiply,
       SgFoldExpression::e_fold_divide,
       SgFoldExpression::e_fold_remainder,
       SgFoldExpression::e_fold_bit_xor,
       SgFoldExpression::e_fold_bit_and,
       SgFoldExpression::e_fold_bit_or,
       SgFoldExpression::e_fold_assign,
       SgFoldExpression::e_fold_less,
       SgFoldExpression::e_fold_greater,
       SgFoldExpression::e_fold_shift_left,
       SgFoldExpression::e_fold_shift_right,
       SgFoldExpression::e_fold_add_assign,
       SgFoldExpression::e_fold_subtract_assign,
       SgFoldExpression::e_fold_multiply_assign,
       SgFoldExpression::e_fold_divide_assign,
       SgFoldExpression::e_fold_remainder_assign,
       SgFoldExpression::e_fold_xor_assign,
       SgFoldExpression::e_fold_and_assign,
       SgFoldExpression::e_fold_or_assign,
       SgFoldExpression::e_fold_shift_left_assign,
       SgFoldExpression::e_fold_shift_right_assign,
       SgFoldExpression::e_fold_equal,
       SgFoldExpression::e_fold_not_equal,
       SgFoldExpression::e_fold_less_equal,
       SgFoldExpression::e_fold_greater_equal,
       SgFoldExpression::e_fold_logical_and,
       SgFoldExpression::e_fold_logical_or,
       SgFoldExpression::e_fold_comma,
       SgFoldExpression::e_fold_member_pointer,
       SgFoldExpression::e_fold_indirect_member_pointer});
  return {kind, p.requiredBool("is_left_associative")};
}

CastConstructionFields castConstructionFieldsFromJson(const JsonValue &p) {
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
  (void)requiredEnum<SgUnaryOp::Sgop_mode>(p, "mode", "SgCastExp",
                                           {SgUnaryOp::prefix});
  return {cast_type, conversion_kind, value_category};
}

bool constructorElisionFromProperties(const JsonValue &properties) {
  const bool elidable = properties.requiredBool("is_elidable");
  if (properties.requiredBool("elision_permitted") && !elidable)
    throw std::runtime_error(
        "AST JSON constructor permits elision without eligibility");
  const bool parentheses =
      properties.requiredBool("need_parenthesis_after_name");
  const bool braced = properties.requiredBool("is_braced_initialized");
  if (elidable &&
      (properties.requiredBool("need_name") ||
       properties.requiredBool("need_qualifier") || (parentheses && braced) ||
       properties.requiredBool("explicit_name_qualification_present") ||
       properties.requiredBool("explicit_global_qualification") ||
       !stringListFromJson(properties.at("explicit_name_qualification_tokens"),
                           "SgConstructorInitializer qualification")
            .empty() ||
       properties.requiredInt("source_type_elaboration_kind") !=
           SgNonrealDecl::e_source_elaboration_unspecified))
    throw std::runtime_error(
        "AST JSON elidable constructor has independent source spelling");
  return elidable;
}

SgConstructorInitializer::argument_source_enum
constructorArgumentSourceFromProperties(const JsonValue &properties) {
  return requiredEnum<SgConstructorInitializer::argument_source_enum>(
      properties, "argument_source", "SgConstructorInitializer",
      {SgConstructorInitializer::e_constructor_argument_expressions,
       SgConstructorInitializer::e_constructor_inherited_arguments,
       SgConstructorInitializer::
           e_constructor_inherited_virtual_base_arguments});
}

SgConstructorInitializer *constructorInitializerFromProperties(
    const JsonValue &properties, SgMemberFunctionDeclaration *declaration,
    SgExprListExp *arguments, const SgExpressionPtrList &elements,
    SgType *type) {
  if (declaration != nullptr && declaration->get_functionModifier().isDeleted())
    throw std::runtime_error(
        "AST JSON constructor selects a deleted declaration");
  const bool elidable = constructorElisionFromProperties(properties);
  const auto argument_source =
      constructorArgumentSourceFromProperties(properties);
  if (!SgConstructorInitializer::is_valid_argument_source(declaration, elements,
                                                          argument_source) ||
      (argument_source !=
           SgConstructorInitializer::e_constructor_argument_expressions &&
       (elidable || properties.requiredBool("elision_permitted"))))
    throw std::runtime_error(
        "AST JSON inherited constructor requires its selected constructor "
        "and implicit incoming arguments without elision");
  if (arguments == nullptr || arguments->get_parent() != nullptr ||
      SageInterface::containsUnknownType(type) ||
      arguments->get_expressions() != elements)
    throw std::runtime_error("AST JSON constructor requires its exact unowned "
                             "argument list and type");
  std::unordered_set<SgExpression *> seen;
  for (auto *element : elements)
    if (element == nullptr || element == arguments ||
        element->get_parent() != arguments || !seen.insert(element).second)
      throw std::runtime_error(
          "AST JSON constructor argument has conflicting ownership");
  if (elidable && !SgConstructorInitializer::is_valid_elision_arguments(
                      declaration, elements))
    throw std::runtime_error(
        "AST JSON elidable constructor requires its selected declaration "
        "and one owned argument");
  if (!SgCxxDefaultInitExp::is_valid_binding(declaration, elements, 0, type))
    throw std::runtime_error(
        "AST JSON constructor default argument has no exact parameter binding");
  const bool qualification_present =
      properties.requiredBool("explicit_name_qualification_present");
  const bool global_qualification =
      properties.requiredBool("explicit_global_qualification");
  const SgStringList qualification_tokens = stringListFromJson(
      properties.at("explicit_name_qualification_tokens"),
      "SgConstructorInitializer explicit_name_qualification_tokens");
  const auto elaboration_kind =
      requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
          properties, "source_type_elaboration_kind",
          "SgConstructorInitializer",
          {SgNonrealDecl::e_source_elaboration_unspecified,
           SgNonrealDecl::e_source_elaboration_none,
           SgNonrealDecl::e_source_elaboration_typename,
           SgNonrealDecl::e_source_elaboration_class,
           SgNonrealDecl::e_source_elaboration_struct,
           SgNonrealDecl::e_source_elaboration_union,
           SgNonrealDecl::e_source_elaboration_enum});
  if ((!qualification_present &&
       (global_qualification || !qualification_tokens.empty())) ||
      (qualification_present !=
       (elaboration_kind != SgNonrealDecl::e_source_elaboration_unspecified)))
    throw std::runtime_error("AST JSON SgConstructorInitializer has "
                             "contradictory written type qualification");
  const bool type_elaboration =
      properties.requiredBool("type_elaboration_required");
  const bool braced = properties.requiredBool("is_braced_initialized");
  const bool need_name = properties.requiredBool("need_name");
  const bool need_qualifier = properties.requiredBool("need_qualifier");
  const bool parentheses =
      properties.requiredBool("need_parenthesis_after_name");
  const bool class_unknown =
      properties.requiredBool("associated_class_unknown");
  auto initializer = std::make_unique<SgConstructorInitializer>(
      declaration, arguments, type, need_name, need_qualifier, parentheses,
      class_unknown, argument_source);
  initializer->set_explicit_name_qualification_present(qualification_present);
  initializer->set_explicit_global_qualification(global_qualification);
  initializer->set_explicit_name_qualification_tokens(qualification_tokens);
  initializer->set_source_type_elaboration_kind(elaboration_kind);
  initializer->set_type_elaboration_required(type_elaboration);
  initializer->set_is_braced_initialized(braced);
  initializer->set_is_elidable(elidable);
  initializer->set_elision_permitted(
      properties.requiredBool("elision_permitted"));
  initializer->validate_elision();
  return initializer.release();
}

template <class Node, class Allocate>
static Node *constructRecordNodeWithAllocator(const NodeRecord &record,
                                              const NodeMap &dependencies,
                                              Allocate create) {
  static_assert(std::is_base_of_v<SgNode, Node>);
  auto allocate = [&] {
    const auto flags = validateConstructionFlags(record);
    auto construct = [&] {
      auto *node = create();
      flags.initialize(node);
      return node;
    };
    if constexpr (std::is_same_v<Node, SgTemplateInstantiationDecl> ||
                  std::is_same_v<Node, SgTemplateInstantiationFunctionDecl> ||
                  std::is_same_v<Node,
                                 SgTemplateInstantiationMemberFunctionDecl>) {
      const auto &p = record.properties;
      const bool evaluated =
          p.requiredBool("constraint_satisfaction_evaluated");
      const bool satisfied =
          p.requiredBool("constraint_satisfaction_satisfied");
      const bool errors =
          p.requiredBool("constraint_satisfaction_contains_errors");
      const bool substitution_failure =
          p.requiredBool("constraint_satisfaction_substitution_failure");
      const std::string summary =
          p.requiredString("constraint_satisfaction_summary");
      const bool sfinae_evaluated = p.requiredBool("sfinae_evaluated");
      const bool sfinae_failure = p.requiredBool("sfinae_substitution_failure");
      const std::string sfinae_summary = p.requiredString("sfinae_summary");
      auto *node = construct();
      node->set_constraintSatisfactionEvaluated(evaluated);
      node->set_constraintSatisfactionSatisfied(satisfied);
      node->set_constraintSatisfactionContainsErrors(errors);
      node->set_constraintSatisfactionSubstitutionFailure(substitution_failure);
      node->set_constraintSatisfactionSummary(summary);
      node->set_sfinaeEvaluated(sfinae_evaluated);
      node->set_sfinaeSubstitutionFailure(sfinae_failure);
      node->set_sfinaeSummary(sfinae_summary);
      return node;
    } else {
      return construct();
    }
  };
  if constexpr (std::is_base_of_v<SgDeclarationStatement, Node>) {
    std::optional<std::pair<bool, bool>> class_fields;
    if constexpr (std::is_base_of_v<SgClassDeclaration, Node>) {
      class_fields.emplace(
          record.properties.requiredBool("is_unnamed"),
          record.properties.requiredBool("is_autonomous_declaration"));
    }
    const auto statement_fields = statementFieldsFromJson(
        record, static_cast<VariantT>(Node::static_variant));
    auto fields = declarationFieldsFromJson(
        record.properties, static_cast<VariantT>(Node::static_variant),
        "SgDeclarationStatement");
    auto *declaration = allocate();
    if constexpr (std::is_base_of_v<SgClassDeclaration, Node>) {
      declaration->set_isUnNamed(class_fields->first);
      declaration->set_isAutonomousDeclaration(class_fields->second);
    }
    statement_fields.initialize(declaration);
    fields.initialize(declaration);
    return declaration;
  } else if constexpr (std::is_base_of_v<SgStatement, Node>) {
    const auto fields = statementFieldsFromJson(
        record, static_cast<VariantT>(Node::static_variant));
    auto construct = [&] {
      auto *statement = allocate();
      fields.initialize(statement);
      return statement;
    };
    if constexpr (std::is_base_of_v<SgOmpExecStatement, Node>) {
      const bool attribute =
          record.properties.requiredBool("source_form_is_attribute");
      const auto origin =
          requiredEnum<SgOmpExecStatement::runtime_source_origin_enum>(
              record.properties, "runtime_source_origin", "SgOmpExecStatement",
              {SgOmpExecStatement::e_runtime_source_unavailable,
               SgOmpExecStatement::e_runtime_source_directive,
               SgOmpExecStatement::e_runtime_source_generated});
      const std::string function_name =
          record.properties.requiredString("runtime_source_function_name");
      const std::string filename =
          record.properties.requiredString("runtime_source_file");
      const int64_t line = record.properties.requiredInt("runtime_source_line");
      const bool copied =
          record.properties.requiredBool("runtime_source_is_copied");
      if (origin == SgOmpExecStatement::e_runtime_source_unavailable) {
        if (!function_name.empty() || !filename.empty() || line != 0 || copied)
          throw std::runtime_error(
              "AST JSON absent OpenMP runtime source context has a payload");
      } else if (function_name.empty() || filename.empty() || line <= 0 ||
                 line > std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "AST JSON OpenMP runtime source context is incomplete");
      }
      std::optional<bool> combined;
      if constexpr (std::is_base_of_v<SgOmpBodyStatement, Node>)
        combined = record.properties.requiredBool("source_form_is_combined");
      auto *statement = construct();
      statement->set_source_form_is_attribute(attribute);
      if (origin != SgOmpExecStatement::e_runtime_source_unavailable) {
        statement->initialize_runtime_source_context(
            function_name, Sg_File_Info::addFilenameToMap(filename),
            static_cast<int>(line), origin);
        if (copied)
          statement->mark_runtime_source_context_copied();
      }
      if constexpr (std::is_base_of_v<SgOmpBodyStatement, Node>) {
        ROSE_ASSERT(combined.has_value());
        statement->set_source_form_is_combined(*combined);
      }
      return statement;
    } else {
      return construct();
    }
  } else if constexpr (std::is_base_of_v<SgOmpClause, Node>) {
    const auto fields = ompClauseFieldsFromJson(record);
    auto *clause = allocate();
    fields.initialize(clause);
    return clause;
  } else if constexpr (std::is_base_of_v<SgExpression, Node>) {
    auto fields = expressionConstructionFieldsFromJson(record.properties);
    fields.qualification =
        expressionQualificationFieldsFromJson(record.kind, record.properties);
    fields.source =
        expressionSourceFieldsFromJson(record.kind, record.properties);
    (void)validatedExpressionTypeProperty(record);
    validateExpressionLifetime(fields, record.properties, dependencies);
    auto *expression = allocate();
    if constexpr (std::is_base_of_v<SgValueExp, Node>)
      expression->set_literal_spelling_form(fields.source.spelling.value());
    fields.initialize(expression, dependencies);
    return expression;
  } else {
    return allocate();
  }
}

template <class Node, class... Args>
static Node *constructRecordNode(const NodeRecord &record,
                                 RecordNodeConstruction &ownership,
                                 const NodeMap &dependencies, Args &&...args) {
  return constructRecordNodeWithAllocator<Node>(record, dependencies, [&] {
    return ownership.make<Node>(std::forward<Args>(args)...);
  });
}

template <class Node, class... Args>
static Node *constructFunctionRecordNode(const NodeRecord &record,
                                         RecordNodeConstruction &ownership,
                                         const NodeMap &dependencies,
                                         SgName name, Args &&...args) {
  return constructRecordNodeWithAllocator<Node>(record, dependencies, [&] {
    auto *type = semanticFunctionTypeFromJson(
        record.properties.at("function_type"), dependencies);
    if (type == nullptr)
      throw std::runtime_error("AST JSON function requires its canonical "
                               "signature before construction");
    auto discard = &SourceTypeConstruction::discardAst;
    std::unique_ptr<SgFunctionType, decltype(discard)> signature(
        nullableDeclaredExceptionSignatureFromJson(
            record.properties.at("declared_exception_signature"), dependencies),
        discard);
    if ((signature == nullptr && type->get_exception_specification_kind() !=
                                     SgFunctionType::e_exception_none) ||
        (signature != nullptr &&
         (signature->get_semantic_signature() != type ||
          signature->get_return_type() != type->get_return_type() ||
          signature->get_arguments() != type->get_arguments() ||
          signature->variantT() != type->variantT())))
      throw std::runtime_error(
          "AST JSON function has contradictory canonical and declared "
          "signatures at node " +
          std::to_string(record.id));
    auto *node = new Node(name, type, nullptr, signature.get(),
                          std::forward<Args>(args)...);
    signature.release();
    return static_cast<Node *>(ownership.own(node));
  });
}

SgNode *createNodeFromRecordImpl(
    const NodeRecord &record, SgProject *project,
    RecordNodeConstruction &ownership, SgPragma *pragma_child,
    SgTemplateClassDeclaration *template_declaration,
    SgClassDeclaration *class_declaration,
    SgFunctionDeclaration *function_declaration, SgBasicBlock *function_body,
    SgNamespaceDeclarationStatement *namespace_declaration,
    SgDeclarationStatement *declaration_target, SgInitializedName *using_name,
    SgScopeStatement *enum_scope, SgModuleStatement *use_module,
    const SgDeclarationStatementPtrList *dependent_template_candidates,
    const NodeMap &dependencies) {
  const JsonValue &p = record.properties;
  const std::string &kind = record.kind;
  if (kind == "SgFormatItem" || kind == "SgFormatItemList" ||
      kind == "SgFormatStatement") {
    auto require_detached = [&](uint64_t id) {
      SgNode *child = nodeById(dependencies, id);
      if (child->get_parent() != nullptr)
        throw std::runtime_error("AST JSON FORMAT child already has an owner");
      ownership.borrow(child);
      return child;
    };
    if (kind == "SgFormatItemList") {
      SgFormatItemPtrList children;
      std::unordered_set<SgFormatItem *> seen;
      const auto edges = edgesFor(record, "format_item_list");
      for (std::size_t index = 0; index < edges.size(); ++index) {
        auto *child = isSgFormatItem(require_detached(edges[index].target));
        if (child == nullptr || !seen.insert(child).second ||
            edges[index].index != index)
          throw std::runtime_error("AST JSON FORMAT list has invalid children");
        const auto *group =
            std::get_if<SgFormatItem::Group>(&child->get_descriptor().value);
        if (group != nullptr && *group == SgFormatItem::Group::unlimited &&
            index + 1 != edges.size())
          throw std::runtime_error(
              "AST JSON unlimited FORMAT group must be last");
        children.push_back(child);
      }
      return constructRecordNode<SgFormatItemList>(record, ownership,
                                                   dependencies, children);
    }
    const uint64_t id = singleEdgeTarget(record, "format_item_list");
    auto *list = id == 0 ? nullptr : isSgFormatItemList(require_detached(id));
    if ((id != 0 && list == nullptr) ||
        (kind == "SgFormatStatement" && list == nullptr))
      throw std::runtime_error("AST JSON FORMAT requires its typed item list");
    if (kind == "SgFormatStatement")
      return constructRecordNode<SgFormatStatement>(record, ownership,
                                                    dependencies, list);
    const auto fields = formatItemFieldsFromJson(p);
    if (!edgesFor(record, "data").empty() ||
        !SgFormatItem::is_valid_descriptor(
            fields.descriptor, fields.repeat_count, list != nullptr))
      throw std::runtime_error(
          "AST JSON FORMAT descriptor has inconsistent children");
    if (list != nullptr) {
      if (list->get_format_item_list().empty())
        throw std::runtime_error("AST JSON FORMAT group cannot be empty");
      for (auto *child : list->get_format_item_list()) {
        const auto *group =
            std::get_if<SgFormatItem::Group>(&child->get_descriptor().value);
        if (group != nullptr && *group == SgFormatItem::Group::unlimited)
          throw std::runtime_error(
              "AST JSON unlimited FORMAT group cannot be nested");
      }
      if (std::get<SgFormatItem::Group>(fields.descriptor.value) ==
              SgFormatItem::Group::unlimited &&
          !list->contains_data_edit())
        throw std::runtime_error(
            "AST JSON unlimited FORMAT group requires a data edit");
    }
    return constructRecordNode<SgFormatItem>(record, ownership, dependencies,
                                             fields.descriptor,
                                             fields.repeat_count, list);
  }
  std::unordered_set<SgTemplateArgument *> function_arguments;
  auto functionArgumentInputs = [&](const char *field) {
    SgTemplateArgumentPtrList result;
    for (const auto &edge : edgesFor(record, field)) {
      auto *argument =
          nodeByIdAs<SgTemplateArgument>(dependencies, edge.target);
      if (!function_arguments.insert(argument).second)
        throw std::runtime_error(
            "AST JSON function template argument has repeated ownership");
      ownership.borrow(argument);
      result.push_back(argument);
    }
    return result;
  };
  static const VariantVector omp_body_variants(V_SgOmpBodyStatement);
  static const VariantVector omp_clause_statements(V_SgOmpClauseStatement);
  static const VariantVector omp_clause_bodies(V_SgOmpClauseBodyStatement);
  SgOmpClauseList *omp_clauses = nullptr;
  if (kindInVariants(kind, omp_clause_statements) ||
      kindInVariants(kind, omp_clause_bodies) ||
      kind == "SgOmpGroupprivateStatement") {
    omp_clauses = nodeByIdAs<SgOmpClauseList>(
        dependencies, requiredSingleEdgeTarget(record, "clause_list"));
    ownership.borrow(omp_clauses);
  }
  SgStatement *omp_body = nullptr;
  if (kindInVariants(kind, omp_body_variants))
    if (uint64_t body_id = singleEdgeTarget(record, "body")) {
      omp_body = nodeByIdAs<SgStatement>(dependencies, body_id);
      if (omp_body->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON OpenMP body already has a structural owner");
      ownership.borrow(omp_body);
    }
  if (p.find("io_statement"))
    throw std::runtime_error(
        "AST JSON retains removed I/O statement classification");
  auto make_function = [&](auto allocate) {
    const auto constexpr_specifier =
        functionConstexprSpecifierFromJson(p, kind);
    (void)sourceSequenceValueFromJson(p);
    if (p.find("template_instantiation_pattern_is_unpublished"))
      throw std::runtime_error(
          "AST JSON function retains removed instantiation-pattern state");
    const uint64_t pattern_id =
        singleEdgeTarget(record, "template_instantiation_pattern");
    const bool narrow_instantiation =
        kind == "SgTemplateInstantiationFunctionDecl" ||
        kind == "SgTemplateInstantiationMemberFunctionDecl";
    auto *pattern = isSgFunctionDeclaration(declaration_target);
    if ((pattern_id != 0 &&
         (pattern_id == record.id || narrow_instantiation ||
          pattern == nullptr ||
          pattern->get_templateInstantiationPattern() != nullptr)) ||
        (pattern_id == 0 && !narrow_instantiation &&
         declaration_target != nullptr))
      throw std::runtime_error(
          "AST JSON function has an invalid construction-time instantiation "
          "pattern");
    const auto modifiers = functionModifierFieldsFromJson(record);
    SgSymbol *derived_type_symbol =
        kind == "SgProcedureHeaderStatement"
            ? exactBoundSymbolFromJson(
                  p.at("fortran_source_derived_type_symbol"), dependencies)
            : nullptr;
    SgDeclarationStatement *member_owner = nullptr;
    if (kind == "SgMemberFunctionDeclaration" ||
        kind == "SgTemplateMemberFunctionDeclaration" ||
        kind == "SgTemplateInstantiationMemberFunctionDecl") {
      member_owner = nodeByIdAs<SgDeclarationStatement>(
          dependencies,
          requiredSingleEdgeTarget(record, "associatedClassDeclaration"));
      if (isSgClassDeclaration(member_owner) == nullptr &&
          isSgNonrealDecl(member_owner) == nullptr)
        throw std::runtime_error("AST JSON member function has no typed class "
                                 "owner");
    }
    const uint64_t parameter_scope_id =
        singleEdgeTarget(record, "functionParameterScope");
    auto *parameter_scope = parameter_scope_id != 0
                                ? nodeByIdAs<SgFunctionParameterScope>(
                                      dependencies, parameter_scope_id)
                                : nullptr;
    if (parameter_scope != nullptr &&
        parameter_scope->get_parent() != nullptr) {
      auto *owner = isSgFunctionDeclaration(parameter_scope->get_parent());
      bool exact_peer = false;
      for (const char *field :
           {"firstNondefiningDeclaration", "definingDeclaration"}) {
        const uint64_t peer_id = singleEdgeTarget(record, field);
        const auto peer = dependencies.find(peer_id);
        exact_peer =
            exact_peer || (peer != dependencies.end() && peer->second == owner);
      }
      if (owner == nullptr || !exact_peer ||
          owner->get_functionParameterScope() != parameter_scope)
        throw std::runtime_error(
            "AST JSON function parameter scope has no exact constructed peer");
    }
    if (parameter_scope != nullptr &&
        parameter_scope->get_parent() == nullptr) {
      if (parameter_scope->get_construction_semantic_scope() == nullptr &&
          (parameter_scope->get_construction_physical_output_owner() !=
               nullptr ||
           parameter_scope->get_completed_semantic_scope() != nullptr ||
           !parameter_scope->get_declarations().empty() ||
           parameter_scope->get_auxiliary_declaration_scopes() != nullptr ||
           parameter_scope->get_auxiliary_declarations() != nullptr ||
           parameter_scope->get_symbol_table() == nullptr ||
           parameter_scope->get_symbol_table()->size() != 0))
        throw std::runtime_error(
            "AST JSON function parameter scope is neither fresh nor in an "
            "exact construction transaction");
      ownership.borrow(parameter_scope);
    }
    const uint64_t source_list_id =
        singleEdgeTarget(record, "parameterList_syntax");
    const bool aliases_parameters =
        source_list_id != 0 &&
        source_list_id == singleEdgeTarget(record, "parameterList");
    SgFunctionParameterList *source_list = nullptr;
    if (source_list_id != 0 && !aliases_parameters) {
      source_list =
          nodeByIdAs<SgFunctionParameterList>(dependencies, source_list_id);
      if (source_list->get_parent() != nullptr ||
          source_list->get_firstNondefiningDeclaration() != source_list ||
          source_list->get_definingDeclaration() != nullptr ||
          !source_list->get_args().empty())
        throw std::runtime_error(
            "AST JSON source parameter list is not a fresh construction input");
      ownership.borrow(source_list);
    }
    const bool is_deduction_guide = p.requiredBool("is_deduction_guide");
    const auto return_fields = functionReturnTypeSourceFieldsFromJson(p);
    const bool is_implicit_function = p.requiredBool("is_implicit_function");
    const auto region_ordinal = requiredOmpDeclareVariantRegionOrdinal(
        p, p.requiredString("name"), kind);
    const SgName variant_source_name =
        p.requiredName("omp_declare_variant_source_name");
    const bool name_parenthesized =
        p.requiredBool("source_name_parenthesized_for_macro");
    const bool wrapped_function_type =
        p.requiredBool("source_declarator_uses_wrapped_function_type");
    const bool named_in_end_statement =
        p.requiredBool("named_in_end_statement");
    const std::string asm_name = p.requiredString("asm_name");
    const bool old_style_definition = p.requiredBool("old_style_definition");
    const std::string section = p.requiredString("gnu_extension_section");
    const std::string alias = p.requiredString("gnu_extension_alias");
    const bool prototype_without_parameters =
        p.requiredBool("prototype_is_without_parameters");
    const int regparm = p.requiredIntInRange("gnu_regparm_attribute", -1,
                                             std::numeric_limits<int>::max());
    const bool noreturn_keyword = p.requiredBool("using_c11_noreturn_keyword");
    const bool new_return_type_syntax =
        p.requiredBool("using_new_function_return_type_syntax");
    const bool frontend_normalization =
        p.requiredBool("marked_as_frontend_normalization");
    if (!p.requiredString("fortran_anonymous_program_unit_symbol_key").empty())
      throw std::runtime_error("AST JSON " + kind +
                               " exposes an implementation-only anonymous "
                               "program-unit symbol key");
    const auto source_ownership =
        requiredEnum<SgFunctionDeclaration::frontend_source_ownership_enum>(
            p, "frontend_source_ownership", "SgFunctionDeclaration",
            {SgFunctionDeclaration::e_frontend_source_unclassified,
             SgFunctionDeclaration::e_frontend_source_main_file,
             SgFunctionDeclaration::e_frontend_source_application_header,
             SgFunctionDeclaration::e_frontend_source_system_header,
             SgFunctionDeclaration::e_frontend_source_external,
             SgFunctionDeclaration::e_frontend_source_support,
             SgFunctionDeclaration::e_frontend_source_pseudo_file,
             SgFunctionDeclaration::e_frontend_source_implicit});
    const auto origin =
        requiredEnum<SgFunctionDeclaration::frontend_declaration_origin_enum>(
            p, "frontend_declaration_origin", "SgFunctionDeclaration",
            {SgFunctionDeclaration::e_frontend_declaration_unclassified,
             SgFunctionDeclaration::e_frontend_declaration_explicit,
             SgFunctionDeclaration::e_frontend_declaration_implicit});
    SgScopeStatement *semantic_scope = nullptr;
    if (uint64_t scope_id = singleEdgeTarget(record, "scope"))
      semantic_scope = nodeByIdAs<SgScopeStatement>(dependencies, scope_id);
    SgExpression *trailing_constraint = nullptr;
    if (uint64_t clause_id =
            singleEdgeTarget(record, "trailingRequiresClause")) {
      trailing_constraint = nodeByIdAs<SgExpression>(dependencies, clause_id);
      ownership.borrow(trailing_constraint);
    }
    auto *function = allocate();
    if (semantic_scope != nullptr)
      function->set_scope(semantic_scope);
    if (auto *procedure = isSgProcedureHeaderStatement(function))
      procedure->set_fortran_source_derived_type_symbol(derived_type_symbol);
    function->set_source_return_type_global_qualification(
        return_fields.globally_qualified);
    function->get_source_return_type_qualification_tokens() =
        return_fields.qualification_tokens;
    function->set_source_return_type_qualification_present(
        return_fields.qualified);
    function->set_source_return_type_elaboration_kind(
        return_fields.elaboration);
    function->set_type_elaboration_required_for_return_type(
        return_fields.elaboration_required);
    function->set_name_qualification_length_for_return_type(
        return_fields.name_qualification_length);
    function->set_requiresNameQualificationOnReturnType(
        return_fields.requires_name_qualification);
    function->set_global_qualification_required_for_return_type(
        return_fields.global_qualification_required);
    function->set_is_implicit_function(is_implicit_function);
    function->set_omp_declare_variant_source_name(variant_source_name);
    function->set_omp_declare_variant_region_ordinal(region_ordinal);
    function->set_source_name_parenthesized_for_macro(name_parenthesized);
    function->set_source_declarator_uses_wrapped_function_type(
        wrapped_function_type);
    function->set_named_in_end_statement(named_in_end_statement);
    function->set_asm_name(asm_name);
    function->set_oldStyleDefinition(old_style_definition);
    function->set_gnu_extension_section(section);
    function->set_gnu_extension_alias(alias);
    function->set_prototypeIsWithoutParameters(prototype_without_parameters);
    function->set_gnu_regparm_attribute(regparm);
    function->set_using_C11_Noreturn_keyword(noreturn_keyword);
    function->set_using_new_function_return_type_syntax(new_return_type_syntax);
    function->set_marked_as_frontend_normalization(frontend_normalization);
    if (source_ownership !=
        SgFunctionDeclaration::e_frontend_source_unclassified)
      function->initialize_frontend_source_ownership(source_ownership);
    if (origin != SgFunctionDeclaration::e_frontend_declaration_unclassified)
      function->initialize_frontend_declaration_origin(origin);
    if (parameter_scope != nullptr) {
      if (parameter_scope->get_construction_semantic_scope() != nullptr)
        SageInterface::completeDetachedFunctionParameterScopeConstruction(
            function, parameter_scope);
      else {
        function->set_functionParameterScope(parameter_scope);
        if (parameter_scope->get_parent() == nullptr)
          parameter_scope->set_parent(function);
      }
    }
    if (aliases_parameters)
      function->set_parameterList_syntax(function->get_parameterList());
    else if (source_list != nullptr) {
      function->set_parameterList_syntax(source_list);
      source_list->set_parent(function);
    }
    function->set_constexpr_specifier(constexpr_specifier);
    function->set_is_deduction_guide(is_deduction_guide);
    if (member_owner != nullptr)
      isSgMemberFunctionDeclaration(function)->set_associatedClassDeclaration(
          member_owner);
    if (pattern_id != 0)
      function->set_templateInstantiationPattern(pattern);
    function->get_functionModifier().set_modifierVector(modifiers.function);
    function->get_specialFunctionModifier().set_modifierVector(
        modifiers.special);
    function->set_trailingRequiresClause(trailing_constraint);
    if (trailing_constraint != nullptr)
      trailing_constraint->set_parent(function);
    return function;
  };
  if (kind == "SgAsmOp" || kind == "SgAsmStmt") {
    if (kind == "SgAsmOp")
      (void)asmOperandFieldsFromJson(p);
    else
      (void)asmStatementFieldsFromJson(p);
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact owned dependency construction");
  }
  if (kind == "SgFileList") {
    return constructRecordNode<SgFileList>(record, ownership, dependencies);
  }
  if (kind == "SgIncludeFile") {
    const std::string filename = p.requiredString("filename");
    const std::string name = p.requiredString("name_used_in_include_directive");
    const std::string prefix = p.requiredString("directory_prefix");
    const std::string root = p.requiredString("applicationRootDirectory");
    const bool system = p.requiredBool("isSystemInclude");
    const bool preinclude = p.requiredBool("isPreinclude");
    const bool application = p.requiredBool("isApplicationFile");
    const bool root_source = p.requiredBool("isRootSourceFile");
    auto *include = constructRecordNode<SgIncludeFile>(record, ownership,
                                                       dependencies, filename);
    include->set_name_used_in_include_directive(name);
    include->set_directory_prefix(prefix);
    include->set_applicationRootDirectory(root);
    include->set_isSystemInclude(system);
    include->set_isPreinclude(preinclude);
    include->set_isApplicationFile(application);
    include->set_isRootSourceFile(root_source);
    return include;
  }
  if (kind == "SgGlobal") {
    return constructRecordNode<SgGlobal>(record, ownership, dependencies);
  }
  if (kind == "SgBasicBlock") {
    const bool fortran_block = p.requiredBool("is_fortran_block_construct");
    const std::string name = p.requiredString("fortran_block_construct_name");
    if (!fortran_block && !name.empty())
      throw std::runtime_error(
          "AST JSON ordinary block has a Fortran construct name");
    const uint64_t label = singleEdgeTarget(record, "end_numeric_label");
    if (label != singleEdgeTarget(record, "fortran_block_end_numeric_label") ||
        (!fortran_block && label != 0))
      throw std::runtime_error(
          "AST JSON block has inconsistent END BLOCK label ownership");
    const bool implicit_scope =
        p.requiredBool("is_implicit_control_flow_scope");
    if (fortran_block && implicit_scope)
      throw std::runtime_error(
          "AST JSON block has conflicting construct roles");
    auto *block = constructRecordNode<SgBasicBlock>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr));
    block->set_is_fortran_block_construct(fortran_block);
    block->set_fortran_block_construct_name(name);
    block->set_is_implicit_control_flow_scope(implicit_scope);
    return block;
  }
  if (kind == "SgInitializedName") {
    // Validate roles and pack flags before reporting missing typed
    // dependencies.
    (void)p.requiredName("name");
    (void)initializedNameRolesFromJson(p);
    throw std::runtime_error("AST JSON SgInitializedName requires exact typed "
                             "dependency construction");
  }
  if (kind == "SgVariableDeclaration") {
    return constructRecordNode<SgVariableDeclaration>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgVariableDefinition") {
    if (p.requiredBool("is_zero_width_bitfield") &&
        singleEdgeTarget(record, "bitfield") == 0)
      throw std::runtime_error(
          "AST JSON zero-width bitfield has no bitfield expression");
    (void)p.requiredBool("has_no_unique_address");
    throw std::runtime_error(
        "AST JSON variable definition requires its typed declarator before "
        "construction");
  }
  if (kind == "SgTemplateVariableDeclaration") {
    if (p.at("semantic_template_arguments").kind != JsonValue::Kind::Array)
      throw std::runtime_error(
          "AST JSON variable template semantic arguments are not an array");
    auto *selected = isSgTemplateVariableDeclaration(declaration_target);
    if ((singleEdgeTarget(record, "specializedTemplateDeclaration") != 0) !=
            (selected != nullptr) ||
        (declaration_target != nullptr && selected == nullptr))
      throw std::runtime_error(
          "AST JSON variable template requires its exact typed primary or "
          "partial declaration");
    auto *declaration = constructRecordNode<SgTemplateVariableDeclaration>(
        record, ownership, dependencies, SgTemplateArgumentPtrList{});
    declaration->set_specializedTemplateDeclaration(selected);
    return declaration;
  }
  if (kind == "SgProgramHeaderStatement") {
    const std::string name = p.requiredString("name");
    const auto statementKind = programStatementKindFromJson(p);
    const bool namedInEndStatement = p.requiredBool("named_in_end_statement");
    const std::string endStatementName = p.requiredString("end_statement_name");
    validateFortranProgramNameMetadata(name, statementKind, namedInEndStatement,
                                       endStatementName);
    auto *decl = make_function([&] {
      return constructFunctionRecordNode<SgProgramHeaderStatement>(
          record, ownership, dependencies, SgName(name));
    });
    decl->set_program_statement_kind(statementKind);
    decl->set_end_statement_name(SgName(endStatementName));
    return decl;
  }
  if (kind == "SgProcedureHeaderStatement") {
    const SgName name = p.requiredName("name");
    const auto subprogram_kind = subprogramKindFromJson(p, kind);
    const auto block_data_name_kind = blockDataNameKindFromJson(p, kind);
    const auto source_form = procedureSourceFormFromJson(p, kind);
    const SgName binding_name = p.requiredName("fortran_binding_name");
    const auto result_type_spec = procedureResultTypeSpecFromJson(p, kind);
    SgProcedureHeaderStatement *decl = make_function([&] {
      return constructFunctionRecordNode<SgProcedureHeaderStatement>(
          record, ownership, dependencies, name);
    });
    decl->set_subprogram_kind(subprogram_kind);
    decl->set_block_data_name_kind(block_data_name_kind);
    decl->set_fortran_procedure_source_form(source_form);
    decl->set_fortran_binding_name(binding_name);
    decl->set_fortran_result_type_spec(result_type_spec);
    return decl;
  }
  if (kind == "SgContainsStatement") {
    return constructRecordNode<SgContainsStatement>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgStatementFunctionStatement")
    throw std::runtime_error(
        "AST JSON statement function requires exact typed child construction");
  if (kind == "SgFortranContinueStmt") {
    return constructRecordNode<SgFortranContinueStmt>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgFormTeamStatement") {
    (void)requiredEnum<SgImageControlStatement::image_control_statement_enum>(
        p, "image_control_statement", "SgFormTeamStatement",
        {SgImageControlStatement::e_form_team});
    throw std::runtime_error(
        "AST JSON FORM TEAM requires exact typed operands");
  }
  if (kind == "SgCommonBlock") {
    return constructRecordNode<SgCommonBlock>(
        record, ownership, dependencies, static_cast<Sg_File_Info *>(nullptr));
  }
  if (kind == "SgCommonBlockObject") {
    const std::string name = p.requiredString("block_name");
    SgCommonBlockObject *object = constructRecordNode<SgCommonBlockObject>(
        record, ownership, dependencies, static_cast<Sg_File_Info *>(nullptr));
    object->set_block_name(name);
    return object;
  }
  if (kind == "SgFortranIncludeLine") {
    return constructRecordNode<SgFortranIncludeLine>(
        record, ownership, dependencies, p.requiredString("filename"));
  }
  if (kind == "SgFunctionDeclaration") {
    return make_function([&] {
      return constructFunctionRecordNode<SgFunctionDeclaration>(
          record, ownership, dependencies, p.requiredName("name"));
    });
  }
  if (kind == "SgEntryStatement") {
    return make_function([&] {
      return constructFunctionRecordNode<SgEntryStatement>(
          record, ownership, dependencies, p.requiredName("name"));
    });
  }
  if (kind == "SgTemplateInstantiationFunctionDecl") {
    const SgName name = p.requiredName("name");
    const SgName template_name = requiredTemplateName(p, kind);
    const bool explicit_arguments =
        p.requiredBool("template_argument_list_is_explicit");
    auto *template_target = isSgTemplateFunctionDeclaration(declaration_target);
    const SgDeclarationStatementPtrList candidates =
        dependent_template_candidates != nullptr
            ? *dependent_template_candidates
            : SgDeclarationStatementPtrList();
    if ((declaration_target != nullptr && template_target == nullptr) ||
        (template_target != nullptr && !candidates.empty()) ||
        (template_target == nullptr && candidates.size() < 2))
      throw std::runtime_error(
          "AST JSON function instantiation requires one typed primary or "
          "one unresolved candidate set");
    std::unordered_set<SgDeclarationStatement *> unique_candidates;
    for (SgDeclarationStatement *candidate : candidates) {
      if (isSgTemplateFunctionDeclaration(candidate) == nullptr ||
          !unique_candidates.insert(candidate).second)
        throw std::runtime_error(
            "AST JSON function instantiation has invalid typed candidates");
    }
    auto arguments = functionArgumentInputs("templateArguments");
    auto semantic_arguments =
        functionArgumentInputs("deducedTemplateArguments");
    auto *declaration = make_function([&] {
      return constructFunctionRecordNode<SgTemplateInstantiationFunctionDecl>(
          record, ownership, dependencies, name, template_target, arguments,
          semantic_arguments);
    });
    declaration->set_specializedTemplateDeclaration(template_target);
    declaration->get_dependentTemplateCandidates() = candidates;
    declaration->set_templateName(template_name);
    declaration->set_template_argument_list_is_explicit(explicit_arguments);
    return declaration;
  }
  if (kind == "SgTemplateFunctionDeclaration") {
    return make_function([&] {
      return constructFunctionRecordNode<SgTemplateFunctionDeclaration>(
          record, ownership, dependencies, p.requiredName("name"));
    });
  }
  if (kind == "SgFunctionParameterList") {
    return constructRecordNode<SgFunctionParameterList>(record, ownership,
                                                        dependencies);
  }
  if (kind == "SgFunctionParameterTypeList")
    throw std::runtime_error(
        "AST JSON source prototype requires exact typed region construction");
  if (kind == "SgFunctionDefinition") {
    if (function_declaration == nullptr || function_body == nullptr)
      throw std::runtime_error(
          "AST JSON SgFunctionDefinition requires its typed declaration "
          "and body");
    ownership.borrow(function_body);
    return constructRecordNode<SgFunctionDefinition>(
        record, ownership, dependencies, function_declaration, function_body);
  }
  if (kind == "SgTemplateFunctionDefinition") {
    auto *declaration = isSgTemplateFunctionDeclaration(function_declaration);
    if (declaration == nullptr || function_body == nullptr)
      throw std::runtime_error(
          "AST JSON SgTemplateFunctionDefinition requires its typed "
          "declaration and body");
    ownership.borrow(function_body);
    return constructRecordNode<SgTemplateFunctionDefinition>(
        record, ownership, dependencies, declaration, function_body);
  }
  if (kind == "SgTypedefDeclaration" ||
      kind == "SgTemplateTypedefDeclaration" ||
      kind == "SgTemplateInstantiationTypedefDeclaration") {
    validateTypedefBaseShape(p);
    (void)typedefMetadataFromJson(p);
    throw std::runtime_error("AST JSON SgTypedefDeclaration requires exact "
                             "typed dependency construction");
  }
  if (kind == "SgModuleStatement") {
    return constructRecordNode<SgModuleStatement>(
        record, ownership, dependencies, p.requiredName("name"),
        requiredEnum<SgClassDeclaration::class_types>(
            p, "class_type", kind, {SgClassDeclaration::e_fortran_module}),
        nullptr, nullptr);
  }
  if (kind == "SgDerivedTypeStatement") {
    return constructRecordNode<SgDerivedTypeStatement>(
        record, ownership, dependencies, p.requiredName("name"),
        requiredEnum<SgClassDeclaration::class_types>(
            p, "class_type", kind, {SgClassDeclaration::e_struct}),
        nullptr, nullptr);
  }
  if (kind == "SgClassDeclaration") {
    return constructRecordNode<SgClassDeclaration>(
        record, ownership, dependencies, p.requiredName("name"),
        requiredClassType(p, "SgClassDeclaration"), nullptr, nullptr);
  }
  if (kind == "SgClassDefinition") {
    if (class_declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgClassDefinition requires its typed declaration");
    return constructRecordNode<SgClassDefinition>(
        record, ownership, dependencies, class_declaration);
  }
  if (kind == "SgTemplateInstantiationDefn") {
    auto *declaration = isSgTemplateInstantiationDecl(class_declaration);
    if (declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgTemplateInstantiationDefn requires its typed "
          "declaration");
    return constructRecordNode<SgTemplateInstantiationDefn>(
        record, ownership, dependencies, declaration);
  }
  if (kind == "SgBaseClass" || kind == "SgExpBaseClass" ||
      kind == "SgNonrealBaseClass")
    throw std::runtime_error(
        "AST JSON base class requires exact typed dependency construction");
  if (kind == "SgDeclarationScope") {
    const bool default_nonreal = p.requiredBool("is_default_nonreal_scope");
    const uint64_t outer_id = singleEdgeTarget(record, "semantic_outer_scope");
    auto *outer = outer_id != 0
                      ? nodeByIdAs<SgScopeStatement>(dependencies, outer_id)
                      : nullptr;
    auto *scope = constructRecordNode<SgDeclarationScope>(record, ownership,
                                                          dependencies, outer);
    scope->set_is_default_nonreal_scope(default_nonreal);
    return scope;
  }
  if (kind == "SgFunctionParameterScope") {
    return constructRecordNode<SgFunctionParameterScope>(record, ownership,
                                                         dependencies);
  }
  if (kind == "SgTemplateClassDefinition") {
    auto *declaration = isSgTemplateClassDeclaration(class_declaration);
    if (declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgTemplateClassDefinition requires its typed declaration");
    return constructRecordNode<SgTemplateClassDefinition>(
        record, ownership, dependencies, declaration);
  }
  if (kind == "SgNamespaceDeclarationStatement") {
    const SgName name = p.requiredName("name");
    const bool unnamed = p.requiredBool("is_unnamed_namespace");
    const bool inlined = p.requiredBool("is_inlined_namespace");
    auto *declaration = constructRecordNode<SgNamespaceDeclarationStatement>(
        record, ownership, dependencies, name, nullptr, unnamed);
    declaration->set_isInlinedNamespace(inlined);
    return declaration;
  }
  if (kind == "SgNamespaceDefinitionStatement") {
    const bool is_union =
        p.requiredBool("is_union_of_reentrant_namespace_definitions");
    if (namespace_declaration == nullptr)
      throw std::runtime_error(
          "AST JSON namespace definition requires its typed declaration");
    SgNamespaceDefinitionStatement *def =
        constructRecordNode<SgNamespaceDefinitionStatement>(
            record, ownership, dependencies, namespace_declaration);
    def->set_isUnionOfReentrantNamespaceDefinitions(is_union);
    return def;
  }
  if (kind == "SgEnumDeclaration") {
    if (enum_scope == nullptr)
      throw std::runtime_error(
          "AST JSON enum declaration requires its typed scope");
    const SgName name = p.requiredName("name");
    const bool embedded = p.requiredBool("embedded");
    const bool unnamed = p.requiredBool("is_unnamed");
    const bool autonomous = p.requiredBool("is_autonomous_declaration");
    const bool scoped = p.requiredBool("is_scoped_enum");
    const bool underlying_source =
        p.requiredBool("underlying_type_source_spelled");
    const uint64_t first_id =
        requiredSingleEdgeTarget(record, "firstNondefiningDeclaration");
    const uint64_t owner_id = p.requiredNonnegativeNodeId("type_owner");
    if (owner_id == 0 || (owner_id == record.id && first_id != record.id))
      throw std::runtime_error(
          "AST JSON enum declaration has contradictory typed type ownership");
    auto *first = first_id == record.id
                      ? nullptr
                      : nodeByIdAs<SgEnumDeclaration>(dependencies, first_id);
    auto *owner = owner_id == record.id
                      ? nullptr
                      : nodeByIdAs<SgEnumDeclaration>(dependencies, owner_id);
    SgEnumType *type = owner != nullptr ? owner->get_type() : nullptr;
    if ((owner != nullptr &&
         (type == nullptr || type->get_declaration() != owner ||
          owner->get_scope() == nullptr ||
          owner->get_firstNondefiningDeclaration() != owner)) ||
        (first != nullptr &&
         (first->get_scope() == nullptr ||
          first->get_firstNondefiningDeclaration() != first ||
          first->get_type() != type)))
      throw std::runtime_error(
          "AST JSON enum declaration has no exact constructed type owner");
    const JsonValue &promotion_record = p.at("promotion_type");
    (void)promotion_record.requiredBool("present");
    SgType *underlying = nullableTypeFromJson(p.at("field_type"), dependencies);
    if ((underlying_source && underlying == nullptr) ||
        (underlying != nullptr &&
         SageInterface::containsUnknownType(underlying)))
      throw std::runtime_error("AST JSON enum has no exact underlying type");
    SgType *promotion = nullableTypeFromJson(promotion_record, dependencies);
    if (promotion != nullptr) {
      SgType *base = promotion->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                          SgType::STRIP_MODIFIER_TYPE |
                                          SgType::STRIP_DECLTYPE_TYPE);
      if (SageInterface::containsUnknownType(promotion) ||
          (!isSgIntegralType(base) && !isSgTypeDependent(base) &&
           !isSgNonrealType(base) && !isSgTemplateType(base)))
        throw std::runtime_error("AST JSON enum has no exact promotion type");
    }
    auto *declaration = constructRecordNode<SgEnumDeclaration>(
        record, ownership, dependencies, name, nullptr, enum_scope);
    declaration->set_embedded(embedded);
    declaration->set_isUnNamed(unnamed);
    declaration->set_isAutonomousDeclaration(autonomous);
    declaration->set_isScopedEnum(scoped);
    declaration->set_underlying_type_source_spelled(underlying_source);
    declaration->set_firstNondefiningDeclaration(
        first != nullptr ? first : declaration);
    declaration->set_field_type(underlying);
    declaration->set_promotion_type(promotion);
    if (owner == nullptr)
      ownership.own(SgEnumType::createType(declaration));
    else
      declaration->set_type(type);
    return declaration;
  }
  if (kind == "SgEmptyDeclaration") {
    const auto role =
        requiredEnum<SgEmptyDeclaration::empty_declaration_role_enum>(
            p, "empty_declaration_lexical_role", "SgEmptyDeclaration",
            {SgEmptyDeclaration::e_empty_declaration_source_semicolon,
             SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor,
             SgEmptyDeclaration::
                 e_empty_declaration_zero_width_source_replacement});
    return constructRecordNode<SgEmptyDeclaration>(record, ownership,
                                                   dependencies, role);
  }
  if (kind == "SgUsingDirectiveStatement") {
    if (namespace_declaration == nullptr)
      throw std::runtime_error(
          "AST JSON using directive has no exact namespace declaration");
    return constructRecordNode<SgUsingDirectiveStatement>(
        record, ownership, dependencies, namespace_declaration);
  }
  if (kind == "SgUsingDeclarationStatement") {
    const SgName terminal_name = p.requiredName("source_terminal_name");
    const bool inheriting_constructor =
        p.requiredBool("is_inheriting_constructor");
    if ((declaration_target == nullptr) == (using_name == nullptr))
      throw std::runtime_error(
          "AST JSON using declaration has no exact typed target");
    if (using_name != nullptr && using_name->get_parent() == nullptr)
      ownership.borrow(using_name);
    SgUsingDeclarationStatement *statement =
        constructRecordNode<SgUsingDeclarationStatement>(
            record, ownership, dependencies, declaration_target, using_name);
    statement->set_source_terminal_name(terminal_name);
    statement->set_is_inheriting_constructor(inheriting_constructor);
    return statement;
  }
  if (kind == "SgUseStatement") {
    const bool external_module_present =
        p.at("external_module").requiredBool("present");
    const bool module_edge_present = singleEdgeTarget(record, "module") != 0;
    if (module_edge_present && external_module_present) {
      throw std::runtime_error(
          "AST JSON use statement has two module identities");
    }
    if ((module_edge_present || external_module_present) !=
        (use_module != nullptr)) {
      throw std::runtime_error(
          "AST JSON use statement has no exact typed module");
    }
    return constructRecordNode<SgUseStatement>(
        record, ownership, dependencies, p.requiredName("name"),
        p.requiredBool("only_option"), p.requiredString("module_nature"),
        use_module);
  }
  if (kind == "SgImportStatement") {
    const auto import_kind =
        requiredEnum<SgImportStatement::fortran_import_kind_enum>(
            p, "fortran_import_kind", "SgImportStatement",
            {SgImportStatement::e_fortran_import_default,
             SgImportStatement::e_fortran_import_only,
             SgImportStatement::e_fortran_import_none,
             SgImportStatement::e_fortran_import_all});
    const JsonValue &names = p.at("import_list");
    if (names.kind != JsonValue::Kind::Array) {
      throw std::runtime_error("AST JSON import_list is not an array");
    }
    SgStringList imports;
    for (const JsonValue &name : names.array) {
      imports.push_back(name.asString());
    }
    const SgName module_name = p.requiredName("module_name");
    const bool cxx_module = p.requiredBool("is_cxx_module_import");
    const bool header_unit = p.requiredBool("is_cxx_header_unit_import");
    if (!SgImportStatement::is_valid_import_state(
            module_name, cxx_module, header_unit, imports, import_kind))
      throw std::runtime_error("AST JSON import state is invalid");
    auto *stmt = constructRecordNode<SgImportStatement>(
        record, ownership, dependencies, module_name, cxx_module, header_unit,
        imports, import_kind);
    return stmt;
  }
  if (kind == "SgRenamePair") {
    return constructRecordNode<SgRenamePair>(record, ownership, dependencies,
                                             p.requiredName("local_name"),
                                             p.requiredName("use_name"));
  }
  if (kind == "SgCxxLiteralTokenList") {
    return constructRecordNode<SgCxxLiteralTokenList>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgCxxLiteralToken") {
    const auto fields = literalTokenFieldsFromJson(p);
    return constructRecordNode<SgCxxLiteralToken>(
        record, ownership, dependencies, fields.kind, fields.spelling);
  }
  if (kind == "SgToken") {
    const int64_t classification = p.requiredInt("classification_code");
    if (classification < 0 ||
        classification > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON SgToken classification code is out of range");
    }
    return constructRecordNode<SgToken>(
        record, ownership, dependencies, p.requiredString("lexeme_string"),
        static_cast<unsigned int>(classification));
  }
  if (kind == "SgImplicitStatement" || kind == "SgFortranImplicitRange" ||
      kind == "SgFortranDataInitializer" || kind == "SgDataStatementValue" ||
      kind == "SgDataStatementObject" || kind == "SgDataStatementGroup" ||
      kind == "SgNameGroup" || kind == "SgNamelistStatement") {
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact typed Fortran construction");
  }
  if (kind == "SgMemberFunctionDeclaration") {
    return make_function([&] {
      return constructFunctionRecordNode<SgMemberFunctionDeclaration>(
          record, ownership, dependencies, p.requiredName("name"));
    });
  }
  if (kind == "SgTemplateInstantiationMemberFunctionDecl") {
    const SgName name = p.requiredName("name");
    const SgName template_name = requiredTemplateName(p, kind);
    const bool explicit_arguments =
        p.requiredBool("template_argument_list_is_explicit");
    auto *template_target =
        isSgTemplateMemberFunctionDeclaration(declaration_target);
    auto *class_owner = isSgTemplateInstantiationDecl(class_declaration);
    if ((declaration_target != nullptr && template_target == nullptr) ||
        (template_target != nullptr && class_declaration != nullptr) ||
        (template_target == nullptr &&
         (class_owner == nullptr ||
          class_owner->get_templateDeclaration() == nullptr ||
          class_owner->get_specializedTemplateDeclaration() == nullptr ||
          p.requiredInt("specialization") !=
              SgDeclarationStatement::e_specialization ||
          explicit_arguments ||
          !edgesFor(record, "templateArguments").empty() ||
          !edgesFor(record, "deducedTemplateArguments").empty())))
      throw std::runtime_error(
          "AST JSON member instantiation requires one typed function template "
          "or an ordinary specialization's exact class owner");
    auto arguments = functionArgumentInputs("templateArguments");
    auto semantic_arguments =
        functionArgumentInputs("deducedTemplateArguments");
    auto *declaration = make_function([&] {
      return constructFunctionRecordNode<
          SgTemplateInstantiationMemberFunctionDecl>(
          record, ownership, dependencies, name, template_target, arguments,
          semantic_arguments);
    });
    declaration->set_specializedTemplateDeclaration(template_target);
    declaration->set_templateName(template_name);
    declaration->set_template_argument_list_is_explicit(explicit_arguments);
    return declaration;
  }
  if (kind == "SgTemplateMemberFunctionDeclaration") {
    return make_function([&] {
      return constructFunctionRecordNode<SgTemplateMemberFunctionDeclaration>(
          record, ownership, dependencies, p.requiredName("name"));
    });
  }
  if (kind == "SgTemplateClassDeclaration") {
    const SgName name = p.requiredName("name");
    const SgName template_name = requiredTemplateName(p, kind);
    auto *primary = isSgTemplateClassDeclaration(declaration_target);
    if ((singleEdgeTarget(record, "specializedTemplateDeclaration") != 0) !=
            (primary != nullptr) ||
        (declaration_target != nullptr && primary == nullptr))
      throw std::runtime_error(
          "AST JSON template class requires its typed primary declaration");
    SgTemplateClassDeclaration *decl =
        constructRecordNode<SgTemplateClassDeclaration>(
            record, ownership, dependencies, name,
            requiredClassType(p, "SgTemplateClassDeclaration"), nullptr,
            nullptr);
    decl->set_templateName(template_name);
    decl->set_specializedTemplateDeclaration(primary);
    const uint64_t member_pattern =
        singleEdgeTarget(record, "instantiatedFromMemberTemplate");
    decl->set_instantiatedFromMemberTemplate(
        member_pattern != 0 ? nodeByIdAs<SgTemplateClassDeclaration>(
                                  dependencies, member_pattern)
                            : nullptr);
    return decl;
  }
  if (kind == "SgTemplateInstantiationDecl") {
    auto *selected = isSgTemplateClassDeclaration(declaration_target);
    if (template_declaration == nullptr || selected == nullptr) {
      throw std::runtime_error(
          "AST JSON class instantiation requires its typed primary and "
          "selected template declarations");
    }
    const SgName name = p.requiredName("name");
    if (name.is_null()) {
      throw std::runtime_error(
          "AST JSON SgTemplateInstantiationDecl has an empty source name");
    }
    const SgName template_name = requiredTemplateName(p, kind);
    const auto class_type = requiredClassType(p, kind);
    const bool injected = p.requiredBool("source_spells_injected_class_name");
    const SgName header = p.requiredName("template_header");
    SgTemplateInstantiationDecl *decl =
        constructRecordNode<SgTemplateInstantiationDecl>(
            record, ownership, dependencies, name, class_type, nullptr, nullptr,
            template_declaration, SgTemplateArgumentPtrList(),
            SgTemplateArgumentPtrList(), injected, SgTemplateArgumentPtrList());
    decl->set_specializedTemplateDeclaration(selected);
    decl->set_templateName(template_name);
    decl->set_templateHeader(header);
    return decl;
  }
  if (kind == "SgTemplateInstantiationDirectiveStatement") {
    const bool do_not_instantiate = p.requiredBool("do_not_instantiate");
    if (declaration_target == nullptr)
      throw std::runtime_error(
          "AST JSON template instantiation directive has no typed declaration");
    ownership.borrow(declaration_target);
    auto *directive =
        constructRecordNode<SgTemplateInstantiationDirectiveStatement>(
            record, ownership, dependencies, declaration_target);
    directive->set_do_not_instantiate(do_not_instantiate);
    declaration_target->set_parent(directive);
    return directive;
  }
  if (kind == "SgNonrealDecl") {
    const SgName name = p.requiredName("name");
    const bool is_concept = p.requiredBool("is_concept");
    const bool is_template_parameter = p.requiredBool("is_template_param");
    const int64_t position = p.requiredInt("template_parameter_position");
    const int64_t depth = p.requiredInt("template_parameter_depth");
    if (position > std::numeric_limits<int>::max() ||
        depth > std::numeric_limits<int>::max() ||
        (is_template_parameter ? position < 0 || depth < 0
                               : position != -1 || depth != -1)) {
      throw std::runtime_error(
          "AST JSON SgNonrealDecl has invalid template parameter coordinates");
    }
    if (name.is_null()) {
      throw std::runtime_error("AST JSON SgNonrealDecl has an empty name");
    }
    const auto template_role =
        requiredEnum<SgNonrealDecl::nonreal_template_role_enum>(
            p, "nonreal_template_role", "SgNonrealDecl",
            {SgNonrealDecl::e_nonreal_template_none,
             SgNonrealDecl::e_nonreal_template_declaration,
             SgNonrealDecl::e_nonreal_template_id});
    if (is_concept &&
        (template_role != SgNonrealDecl::e_nonreal_template_declaration ||
         is_template_parameter)) {
      throw std::runtime_error(
          "AST JSON concept requires a template declaration, not a parameter "
          "or template-id");
    }
    if (is_concept != (singleEdgeTarget(record, "conceptConstraint") != 0)) {
      throw std::runtime_error("AST JSON concept constraint presence "
                               "contradicts its declaration role");
    }
    const JsonValue &type = p.at("type");
    if (type.find("aliased_type") != nullptr)
      throw std::runtime_error("AST JSON SgNonrealType alias projection must "
                               "be stored on its declaration");
    if (type.find("autonomous_declaration") != nullptr)
      throw std::runtime_error(
          "AST JSON named type contains obsolete autonomous_declaration");
    if (!type.requiredBool("present") ||
        type.requiredString("kind") != "SgNonrealType" ||
        type.requiredBool("fortran_source_syntax") || record.id == 0 ||
        type.requiredInt("declaration") <= 0 ||
        static_cast<uint64_t>(type.requiredInt("declaration")) != record.id) {
      throw std::runtime_error(
          "AST JSON SgNonrealDecl requires its exact owned type identity");
    }
    const uint64_t template_id =
        singleEdgeTarget(record, "templateDeclaration");
    if ((template_id != 0) != (declaration_target != nullptr) ||
        (template_role == SgNonrealDecl::e_nonreal_template_id &&
         declaration_target == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgNonrealDecl requires its exact typed template target "
          "before construction");
    }
    const bool template_keyword = p.requiredBool("has_template_keyword");
    const bool global_qualifier = p.requiredBool("has_global_qualifier");
    const bool suppress_typename = p.requiredBool("suppress_typename");
    const auto elaboration =
        requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
            p, "source_elaboration_kind", "SgNonrealDecl",
            {SgNonrealDecl::e_source_elaboration_unspecified,
             SgNonrealDecl::e_source_elaboration_none,
             SgNonrealDecl::e_source_elaboration_typename,
             SgNonrealDecl::e_source_elaboration_class,
             SgNonrealDecl::e_source_elaboration_struct,
             SgNonrealDecl::e_source_elaboration_union,
             SgNonrealDecl::e_source_elaboration_enum});
    const auto &aliased_json = p.at("aliased_type");
    SgDeclarationStatement *alias_target = declaration_target;
    std::unordered_set<SgDeclarationStatement *> using_targets;
    while (auto *using_declaration =
               isSgUsingDeclarationStatement(alias_target)) {
      if (!using_targets.insert(using_declaration).second)
        throw std::runtime_error(
            "AST JSON alias template target contains a cycle");
      alias_target = using_declaration->get_declaration();
      if (alias_target == nullptr)
        throw std::runtime_error(
            "AST JSON alias template using target is null");
    }
    const bool requires_projection =
        template_role == SgNonrealDecl::e_nonreal_template_id &&
        isSgTemplateTypedefDeclaration(alias_target) != nullptr;
    if (requires_projection != aliased_json.requiredBool("present"))
      throw std::runtime_error("AST JSON SgNonrealType alias projection "
                               "disagrees with its template target");
    SgType *aliased_type = nullableTypeFromJson(aliased_json, dependencies);
    auto *declaration = constructRecordNode<SgNonrealDecl>(
        record, ownership, dependencies, name, declaration_target);
    declaration->set_nonreal_template_role(template_role);
    declaration->set_is_concept(is_concept);
    declaration->set_is_template_param(is_template_parameter);
    declaration->set_template_parameter_position(static_cast<int>(position));
    declaration->set_template_parameter_depth(static_cast<int>(depth));
    declaration->set_has_template_keyword(template_keyword);
    declaration->set_has_global_qualifier(global_qualifier);
    declaration->set_suppress_typename(suppress_typename);
    declaration->set_source_elaboration_kind(elaboration);
    declaration->get_tpl_args() = templateArgumentListFromJson(
        p.at("tpl_args"), dependencies, declaration);
    auto *owned_type = ownership.make<SgNonrealType>(declaration, aliased_type);
    owned_type->set_parent(declaration);
    declaration->set_type(owned_type);
    return declaration;
  }
  if (kind == "SgTypeExpression") {
    throw std::runtime_error("AST JSON SgTypeExpression requires exact "
                             "dependency-first construction");
  }
  if (kind == "SgAsteriskShapeExp") {
    return constructRecordNode<SgAsteriskShapeExp>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgColonShapeExp") {
    return constructRecordNode<SgColonShapeExp>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgExprStatement" || kind == "SgReturnStmt") {
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact typed expression construction");
  }
  if (kind == "SgBreakStmt") {
    return constructRecordNode<SgBreakStmt>(record, ownership, dependencies);
  }
  if (kind == "SgContinueStmt") {
    return constructRecordNode<SgContinueStmt>(record, ownership, dependencies);
  }
  if (kind == "SgGotoStatement") {
    if (singleEdgeTarget(record, "selector_expression") != 0)
      throw std::runtime_error(
          "AST JSON computed goto requires exact typed selector construction");
    return constructRecordNode<SgGotoStatement>(
        record, ownership, dependencies,
        static_cast<SgLabelStatement *>(nullptr), nullptr);
  }
  if (kind == "SgForStatement") {
    throw std::runtime_error(
        "AST JSON FOR requires its exact typed initializer construction");
  }
  if (kind == "SgRangeBasedForStatement") {
    return constructRecordNode<SgRangeBasedForStatement>(
        record, ownership, dependencies,
        static_cast<SgVariableDeclaration *>(nullptr),
        static_cast<SgVariableDeclaration *>(nullptr),
        static_cast<SgVariableDeclaration *>(nullptr),
        static_cast<SgVariableDeclaration *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgAssociateStatement") {
    return constructRecordNode<SgAssociateStatement>(record, ownership,
                                                     dependencies);
  }
  if (kind == "SgFortranDo") {
    throw std::runtime_error(
        "AST JSON SgFortranDo requires exact typed child construction");
  }
  if (kind == "SgForAllStatement") {
    throw std::runtime_error(
        "AST JSON SgForAllStatement requires exact typed child construction");
  }
  if (kind == "SgWhereStatement" || kind == "SgElseWhereStatement") {
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact typed child construction");
  }
  if (kind == "SgImpliedDo") {
    return constructRecordNode<SgImpliedDo>(
        record, ownership, dependencies, static_cast<Sg_File_Info *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExpression *>(nullptr),
        static_cast<SgExprListExp *>(nullptr),
        static_cast<SgScopeStatement *>(nullptr));
  }
  if (kind == "SgForInitStatement") {
    return constructRecordNode<SgForInitStatement>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgIfStmt") {
    const std::string name = p.requiredString("string_label");
    const bool has_end = p.requiredBool("has_end_statement");
    const bool use_then = p.requiredBool("use_then_keyword");
    const bool else_if = p.requiredBool("is_else_if_statement");
    const auto if_kind = ifStatementKindFromJson(p);
    const bool is_constexpr = if_kind == SgIfStmt::e_if_constexpr;
    const bool value_known = p.requiredBool("if_constexpr_value_known");
    const bool value = p.requiredBool("if_constexpr_value");
    if ((value_known && !is_constexpr) || (value && !value_known))
      throw std::runtime_error("AST JSON IF has invalid constexpr state");
    if (else_if && (!use_then || has_end || if_kind != SgIfStmt::e_if_ordinary))
      throw std::runtime_error("AST JSON IF has an invalid ELSE IF role");
    if (use_then && if_kind != SgIfStmt::e_if_ordinary)
      throw std::runtime_error("AST JSON IF has an invalid Fortran role");
    const std::string else_name = p.requiredString("else_construct_name");
    if (!else_name.empty() && (!use_then || is_constexpr))
      throw std::runtime_error("AST JSON IF has an invalid named ELSE role");
    auto *statement = constructRecordNode<SgIfStmt>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr),
        static_cast<SgStatement *>(nullptr),
        static_cast<SgStatement *>(nullptr));
    statement->set_string_label(name);
    statement->set_else_construct_name(else_name);
    statement->set_has_end_statement(has_end);
    statement->set_use_then_keyword(use_then);
    statement->set_is_else_if_statement(else_if);
    statement->set_if_statement_kind(if_kind);
    statement->set_if_constexpr_value_known(value_known);
    statement->set_if_constexpr_value(value);
    return statement;
  }
  if (kind == "SgWhileStmt") {
    const std::string name = p.requiredString("string_label");
    const bool has_end = p.requiredBool("has_end_statement");
    auto *statement = constructRecordNode<SgWhileStmt>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr),
        static_cast<SgStatement *>(nullptr));
    statement->set_string_label(name);
    statement->set_has_end_statement(has_end);
    return statement;
  }
  if (kind == "SgDoWhileStmt") {
    return constructRecordNode<SgDoWhileStmt>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgSwitchStatement") {
    const std::string name = p.requiredString("string_label");
    auto *statement = constructRecordNode<SgSwitchStatement>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr),
        static_cast<SgStatement *>(nullptr));
    statement->set_string_label(name);
    return statement;
  }
  if (kind == "SgCaseOptionStmt" || kind == "SgDefaultOptionStmt") {
    (void)p.requiredString(kind == "SgCaseOptionStmt"
                               ? "case_construct_name"
                               : "default_construct_name");
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact typed child construction");
  }
  if (kind == "SgTryStmt") {
    const bool function_try_block = p.requiredBool("is_function_try_block");
    SgTryStmt *stmt = constructRecordNode<SgTryStmt>(
        record, ownership, dependencies, static_cast<SgStatement *>(nullptr));
    stmt->set_is_function_try_block(function_try_block);
    return stmt;
  }
  if (kind == "SgCatchStatementSeq") {
    return constructRecordNode<SgCatchStatementSeq>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgCatchOptionStmt") {
    return constructRecordNode<SgCatchOptionStmt>(
        record, ownership, dependencies,
        static_cast<SgVariableDeclaration *>(nullptr),
        static_cast<SgStatement *>(nullptr), static_cast<SgTryStmt *>(nullptr));
  }
  if (kind == "SgNullStatement") {
    return constructRecordNode<SgNullStatement>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgAccessLabelStatement") {
    return constructRecordNode<SgAccessLabelStatement>(
        record, ownership, dependencies,
        requiredEnum<SgAccessLabelStatement::access_label_kind_enum>(
            p, "access_label_kind", "SgAccessLabelStatement",
            {SgAccessLabelStatement::e_access_label_private,
             SgAccessLabelStatement::e_access_label_protected,
             SgAccessLabelStatement::e_access_label_public}));
  }
  if (kind == "SgDeclarationGroupStatement") {
    const auto terminator =
        requiredEnum<SgDeclarationGroupStatement::source_terminator_enum>(
            p, "declaration_group_source_terminator",
            "SgDeclarationGroupStatement",
            {SgDeclarationGroupStatement::e_source_terminator_file_semicolon,
             SgDeclarationGroupStatement::e_source_terminator_macro_semicolon});
    SgDeclarationGroupStatement *group =
        constructRecordNode<SgDeclarationGroupStatement>(record, ownership,
                                                         dependencies);
    group->set_source_terminator(terminator);
    return group;
  }
  if (kind == "SgAttributedStatement") {
    throw std::runtime_error("AST JSON SgAttributedStatement requires its "
                             "exact construction children");
  }
  if (kind == "SgNamespaceSourceFragment") {
    return constructRecordNode<SgNamespaceSourceFragment>(
        record, ownership, dependencies,
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
    throw std::runtime_error(
        "AST JSON statement attribute requires its construction operand");
  }
  if (kind == "SgStatementAttributeList") {
    throw std::runtime_error(
        "AST JSON statement attribute list requires its construction children");
  }
  if (kind == "SgLabelStatement") {
    return constructRecordNode<SgLabelStatement>(
        record, ownership, dependencies, p.requiredName("label"),
        static_cast<SgStatement *>(nullptr));
  }
  if (kind == "SgPragma") {
    return constructRecordNode<SgPragma>(record, ownership, dependencies,
                                         p.requiredString("name"), nullptr,
                                         nullptr);
  }
  if (kind == "SgPragmaDeclaration") {
    const auto family =
        requiredEnum<SgPragmaDeclaration::fortran_directive_family_enum>(
            p, "fortran_directive_family", "SgPragmaDeclaration",
            {SgPragmaDeclaration::e_fortran_directive_none,
             SgPragmaDeclaration::e_fortran_directive_openmp,
             SgPragmaDeclaration::e_fortran_directive_ompx,
             SgPragmaDeclaration::e_fortran_directive_openacc,
             SgPragmaDeclaration::e_fortran_directive_cuda,
             SgPragmaDeclaration::e_fortran_directive_compiler});
    const std::string group_id = p.requiredString("fortran_directive_group_id");
    const int64_t memberIndex = p.requiredInt("fortran_directive_member_index");
    const int64_t memberCount = p.requiredInt("fortran_directive_member_count");
    if (memberIndex < 0 || memberCount < 0 ||
        static_cast<uint64_t>(memberIndex) >
            std::numeric_limits<unsigned long>::max() ||
        static_cast<uint64_t>(memberCount) >
            std::numeric_limits<unsigned long>::max()) {
      throw std::runtime_error(
          "AST JSON SgPragmaDeclaration has invalid Fortran directive "
          "membership");
    }
    const bool primary = p.requiredBool("fortran_directive_primary");
    const std::string raw_text = p.requiredString("fortran_directive_raw_text");
    const std::string logical_text =
        p.requiredString("fortran_directive_logical_text");
    const std::string semantic_text =
        p.requiredString("fortran_directive_semantic_text");
    const bool has_fortran_directive =
        family != SgPragmaDeclaration::e_fortran_directive_none &&
        family != SgPragmaDeclaration::e_fortran_directive_compiler;
    const bool valid_fortran_ownership =
        has_fortran_directive && !group_id.empty() && memberCount > 0 &&
        memberIndex < memberCount && primary == (memberIndex == 0) &&
        !raw_text.empty() &&
        (primary ? !logical_text.empty() && !semantic_text.empty()
                 : logical_text.empty() && semantic_text.empty());
    const bool empty_fortran_ownership =
        !has_fortran_directive && group_id.empty() && memberIndex == 0 &&
        memberCount == 0 && !primary && raw_text.empty() &&
        logical_text.empty() && semantic_text.empty();
    if (!valid_fortran_ownership && !empty_fortran_ownership) {
      throw std::runtime_error(
          "AST JSON SgPragmaDeclaration has malformed Fortran directive "
          "ownership");
    }
    const auto payload_kind =
        requiredEnum<SgPragmaDeclaration::cxx_pragma_payload_kind_enum>(
            p, "cxx_pragma_payload_kind", "SgPragmaDeclaration",
            {SgPragmaDeclaration::e_cxx_pragma_payload_none,
             SgPragmaDeclaration::e_cxx_pragma_source_spelled,
             SgPragmaDeclaration::e_cxx_pragma_source_file_only,
             SgPragmaDeclaration::e_cxx_pragma_generated_semantic,
             SgPragmaDeclaration::e_cxx_pragma_source_attribute});
    const std::string cxx_source_text = p.requiredString("cxx_source_text");
    const bool cxx_top_level_macro_expansion =
        p.requiredBool("cxx_top_level_macro_expansion");

    if (pragma_child != nullptr &&
        (pragma_child->get_startOfConstruct() == nullptr) !=
            (pragma_child->get_endOfConstruct() == nullptr))
      throw std::runtime_error(
          "AST JSON pragma has incomplete source position");
    ownership.borrow(pragma_child);
    SgPragmaDeclaration *declaration = constructRecordNode<SgPragmaDeclaration>(
        record, ownership, dependencies, pragma_child, family);
    declaration->set_fortran_directive_group_id(group_id);
    declaration->set_fortran_directive_member_index(
        static_cast<unsigned long>(memberIndex));
    declaration->set_fortran_directive_member_count(
        static_cast<unsigned long>(memberCount));
    declaration->set_fortran_directive_primary(primary);
    declaration->set_fortran_directive_raw_text(raw_text);
    declaration->set_fortran_directive_logical_text(logical_text);
    declaration->set_fortran_directive_semantic_text(semantic_text);
    declaration->set_cxx_pragma_payload_kind(payload_kind);
    declaration->set_cxx_source_text(cxx_source_text);
    declaration->set_cxx_top_level_macro_expansion(
        cxx_top_level_macro_expansion);
    return declaration;
  }
  if (kind == "SgAttributeSpecificationStatement") {
    if (p.requiredInt("attribute_kind") ==
        SgAttributeSpecificationStatement::e_dataStatement)
      throw std::runtime_error("AST JSON DATA requires complete typed groups");
    if (p.find("intent") != nullptr) {
      throw std::runtime_error("AST JSON SgAttributeSpecificationStatement "
                               "carries removed intent metadata");
    }
    const auto attributeKind =
        requiredEnum<SgAttributeSpecificationStatement::attribute_spec_enum>(
            p, "attribute_kind", "SgAttributeSpecificationStatement",
            {SgAttributeSpecificationStatement::e_accessStatement_private,
             SgAttributeSpecificationStatement::e_accessStatement_public,
             SgAttributeSpecificationStatement::e_allocatableStatement,
             SgAttributeSpecificationStatement::e_asynchronousStatement,
             SgAttributeSpecificationStatement::e_bindStatement,
             SgAttributeSpecificationStatement::e_dataStatement,
             SgAttributeSpecificationStatement::e_dimensionStatement,
             SgAttributeSpecificationStatement::e_externalStatement,
             SgAttributeSpecificationStatement::e_intrinsicStatement,
             SgAttributeSpecificationStatement::e_optionalStatement,
             SgAttributeSpecificationStatement::e_parameterStatement,
             SgAttributeSpecificationStatement::e_pointerStatement,
             SgAttributeSpecificationStatement::e_protectedStatement,
             SgAttributeSpecificationStatement::e_saveStatement,
             SgAttributeSpecificationStatement::e_targetStatement,
             SgAttributeSpecificationStatement::e_valueStatement,
             SgAttributeSpecificationStatement::e_volatileStatement});
    SgStringList names = stringListFromJson(p.at("name_list"), "name_list");
    auto *stmt = constructRecordNode<SgAttributeSpecificationStatement>(
        record, ownership, dependencies, attributeKind,
        SgDataStatementGroupPtrList{});
    stmt->get_name_list() = std::move(names);
    return stmt;
  }
  if (kind == "SgInterfaceStatement") {
    const auto fields = fortranInterfaceFieldsFromJson(p);
    auto *statement = constructRecordNode<SgInterfaceStatement>(
        record, ownership, dependencies, fields.name, fields.kind);
    if (fields.is_abstract)
      statement->get_declarationModifier().get_typeModifier().setAbstract();
    return statement;
  }
  if (kind == "SgInterfaceBody") {
    return constructRecordNode<SgInterfaceBody>(
        record, ownership, dependencies, p.requiredName("function_name"),
        static_cast<SgFunctionDeclaration *>(nullptr),
        p.requiredBool("use_function_name"));
  }
  if (kind == "SgPrintStatement") {
    return constructRecordNode<SgPrintStatement>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgReadStatement") {
    return constructRecordNode<SgReadStatement>(
        record, ownership, dependencies,
        exactBoundSymbolFromJson(p.at("namelist"), dependencies));
  }
  if (kind == "SgWriteStatement") {
    return constructRecordNode<SgWriteStatement>(
        record, ownership, dependencies,
        exactBoundSymbolFromJson(p.at("namelist"), dependencies));
  }
  if (kind == "SgOpenStatement") {
    return constructRecordNode<SgOpenStatement>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgCloseStatement") {
    return constructRecordNode<SgCloseStatement>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgFlushStatement") {
    return constructRecordNode<SgFlushStatement>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgBackspaceStatement") {
    return constructRecordNode<SgBackspaceStatement>(record, ownership,
                                                     dependencies);
  }
  if (kind == "SgRewindStatement") {
    return constructRecordNode<SgRewindStatement>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgEndfileStatement") {
    return constructRecordNode<SgEndfileStatement>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgWaitStatement") {
    return constructRecordNode<SgWaitStatement>(record, ownership,
                                                dependencies);
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
                  constructRecordNode<SgClinkageStartStatement>(
                      record, ownership, dependencies))
            : static_cast<SgClinkageDeclarationStatement *>(
                  constructRecordNode<SgClinkageEndStatement>(record, ownership,
                                                              dependencies));
    linkage->set_languageSpecifier(language);
    return linkage;
  }
  if (kind == "SgStaticAssertionDeclaration") {
    throw std::runtime_error("AST JSON static assertion requires its exact "
                             "condition at construction");
  }
  if (kind == "SgAwaitExpression") {
    (void)coroutineKeywordKindFromJson(p);
    throw std::runtime_error("AST JSON SgAwaitExpression requires exact "
                             "operand and type construction");
  }
  if (kind == "SgFoldExpression") {
    (void)foldConstructionFieldsFromJson(p);
    throw std::runtime_error("AST JSON SgFoldExpression requires exact operand "
                             "and type construction");
  }
  if (kind == "SgCxxSubobjectDestruction") {
    (void)subobjectDestructionKindFromJson(p);
    throw std::runtime_error(
        "AST JSON subobject destruction requires exact typed targets");
  }
  if (kind == "SgCxxDecompositionRefExp") {
    throw std::runtime_error(
        "AST JSON decomposition reference requires its exact storage owner");
  }
  if (kind == "SgCxxDefaultInitExp") {
    throw std::runtime_error(
        "AST JSON default argument requires its exact parameter and owned "
        "expansion at construction");
  }
  if (kind == "SgCxxArrayInitBinding" || kind == "SgCxxArrayInitLoop" ||
      kind == "SgCxxArrayInitReference") {
    throw std::runtime_error(
        "AST JSON " + kind +
        " requires complete typed array initialization input");
  }
  if (kind == "SgOpaqueValueBinding" || kind == "SgOpaqueValueReference" ||
      kind == "SgPseudoObjectExp")
    throw std::runtime_error("AST JSON " + kind +
                             " requires complete typed pseudo-object input");
  if (kind == "SgMacroExpansionExp") {
    throw std::runtime_error(
        "AST JSON SgMacroExpansionExp requires its exact expanded expression "
        "at construction");
  }
  if (kind == "SgSourceLocationBuiltinExp") {
    (void)sourceLocationBuiltinKindFromJson(p);
    throw std::runtime_error(
        "AST JSON SgSourceLocationBuiltinExp requires exact type construction");
  }
  if (kind == "SgFortranCommonBlockRefExp") {
    const SgName use_name = p.requiredName("use_name");
    if (use_name.is_null()) {
      throw std::runtime_error(
          "AST JSON SgFortranCommonBlockRefExp has empty use_name");
    }
    return constructRecordNode<SgFortranCommonBlockRefExp>(
        record, ownership, dependencies, use_name,
        static_cast<SgCommonBlockObject *>(nullptr));
  }
  if (kind == "SgFunctionParameterRefExp") {
    return constructRecordNodeWithAllocator<SgFunctionParameterRefExp>(
        record, dependencies, [&] {
          auto *reference =
              functionParameterReferenceFromProperties(p, dependencies);
          ownership.own(reference);
          return reference;
        });
  }
  if (kind == "SgVarRefExp" || kind == "SgCompoundLiteralExp") {
    (void)p.at("symbol");
    throw std::runtime_error("AST JSON " + kind +
                             " requires exact symbol construction");
  }
  if (kind == "SgLabelRefExp") {
    (void)p.at("symbol");
    throw std::runtime_error(
        "AST JSON SgLabelRefExp requires exact symbol construction");
  }
  if (kind == "SgActualArgumentExpression") {
    throw std::runtime_error(
        "AST JSON SgActualArgumentExpression requires exact expression "
        "construction");
  }
  if (kind == "SgFunctionRefExp") {
    (void)p.at("symbol");
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp requires exact symbol construction");
  }
  if (kind == "SgTemplateFunctionRefExp") {
    (void)p.at("symbol");
    throw std::runtime_error("AST JSON SgTemplateFunctionRefExp requires exact "
                             "callable construction");
  }
  if (kind == "SgMemberFunctionRefExp") {
    (void)p.at("symbol");
    throw std::runtime_error(
        "AST JSON SgMemberFunctionRefExp requires exact symbol construction");
  }
  if (kind == "SgTemplateMemberFunctionRefExp") {
    (void)p.at("symbol");
    throw std::runtime_error("AST JSON SgTemplateMemberFunctionRefExp requires "
                             "exact callable construction");
  }
  if (kind == "SgExprListExp") {
    const auto form = requiredEnum<SgExprListExp::source_form_enum>(
        p, "expression_list_source_form", "SgExprListExp",
        {SgExprListExp::e_expression_list_plain,
         SgExprListExp::e_expression_list_braced});
    return constructRecordNodeWithAllocator<SgExprListExp>(
        record, dependencies, [&] {
          auto *list = ownership.make<SgExprListExp>();
          list->set_source_form(form);
          return list;
        });
  }
  if (kind == "SgRequiresExpr") {
    throw std::runtime_error(
        "AST JSON SgRequiresExpr requires exact owned child construction");
  }
  if (kind == "SgSimpleRequirement") {
    throw std::runtime_error(
        "AST JSON SgSimpleRequirement requires exact operand construction");
  }
  if (kind == "SgRequirementSubstitutionFailure") {
    const int64_t failure_kind = p.requiredInt("failure_kind");
    const std::string substituted_entity =
        p.requiredString("substituted_entity");
    if (failure_kind <
            SgRequirementSubstitutionFailure::e_simple_expression_failure ||
        failure_kind >
            SgRequirementSubstitutionFailure::e_compound_return_type_failure ||
        substituted_entity.empty()) {
      throw std::runtime_error(
          "AST JSON SgRequirementSubstitutionFailure has malformed exact "
          "semantic state");
    }
    return constructRecordNode<SgRequirementSubstitutionFailure>(
        record, ownership, dependencies, static_cast<Sg_File_Info *>(nullptr),
        static_cast<SgRequirementSubstitutionFailure::failure_kind_enum>(
            failure_kind),
        substituted_entity, p.requiredString("diagnostic_message"));
  }
  if (kind == "SgTypeRequirement") {
    throw std::runtime_error(
        "AST JSON SgTypeRequirement requires exact type construction");
  }

  if (kind == "SgCompoundRequirement") {
    throw std::runtime_error(
        "AST JSON SgCompoundRequirement requires exact operand construction");
  }
  if (kind == "SgNestedRequirement") {
    throw std::runtime_error(
        "AST JSON SgNestedRequirement requires exact operand construction");
  }

  if (kind == "SgFunctionCallExp") {
    throw std::runtime_error(
        "AST JSON call requires exact operand and type construction");
  }
  if (kind == "SgAggregateInitializer") {
    throw std::runtime_error(
        "AST JSON aggregate requires exact list and type construction");
  }
  if (kind == "SgDesignator") {
    (void)requiredEnum<SgDesignator::designator_kind_enum>(
        p, "designator_kind", "SgDesignator",
        {SgDesignator::e_designator_field, SgDesignator::e_designator_array,
         SgDesignator::e_designator_array_range});
    throw std::runtime_error(
        "AST JSON designator requires typed operands before construction");
  }
  if (kind == "SgBoolValExp") {
    const int64_t value = p.requiredInt("value");
    if (value != 0 && value != 1)
      throw std::runtime_error("AST JSON boolean literal value is not 0 or 1");
    return constructRecordNode<SgBoolValExp>(record, ownership, dependencies,
                                             static_cast<int>(value));
  }
  if (kind == "SgShortVal") {
    return constructRecordNode<SgShortVal>(record, ownership, dependencies,
                                           requiredLiteralValue<short>(p),
                                           p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedShortVal") {
    return constructRecordNode<SgUnsignedShortVal>(
        record, ownership, dependencies,
        requiredLiteralValue<unsigned short>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgIntVal") {
    return constructRecordNode<SgIntVal>(record, ownership, dependencies,
                                         requiredLiteralValue<int>(p),
                                         p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedIntVal") {
    return constructRecordNode<SgUnsignedIntVal>(
        record, ownership, dependencies, requiredLiteralValue<unsigned int>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgLongIntVal") {
    return constructRecordNode<SgLongIntVal>(record, ownership, dependencies,
                                             requiredLiteralValue<long>(p),
                                             p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedLongVal") {
    return constructRecordNode<SgUnsignedLongVal>(
        record, ownership, dependencies,
        static_cast<unsigned long>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgInteger128Val") {
    const auto value = integer128ValueFromJson(p.at("value_bits"));
    return constructRecordNode<SgInteger128Val>(record, ownership, dependencies,
                                                value);
  }
  if (kind == "SgFortranBOZVal") {
    const auto bits = integer128ValueFromJson(p.at("bits"));
    if (!bits.isUnsigned() ||
        nullableTypeFromJson(p.at("type"), dependencies) !=
            SgTypeFortranBOZ::createType())
      throw std::runtime_error("AST JSON BOZ literal requires typeless bits");
    if (literalSpellingFormFromJson(p) !=
        SgValueExp::e_literal_canonical_generated)
      throw std::runtime_error(
          "AST JSON BOZ literal requires canonical spelling");
    (void)p.requiredBool("has_literal_semantic_type");
    return constructRecordNode<SgFortranBOZVal>(record, ownership, dependencies,
                                                bits);
  }
  if (kind == "SgLongLongIntVal") {
    return constructRecordNode<SgLongLongIntVal>(
        record, ownership, dependencies, p.requiredInt("value"),
        p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedLongLongIntVal") {
    return constructRecordNode<SgUnsignedLongLongIntVal>(
        record, ownership, dependencies,
        static_cast<unsigned long long>(p.requiredInt("value")),
        p.requiredString("value_string"));
  }
  if (kind == "SgCharVal") {
    return constructRecordNode<SgCharVal>(record, ownership, dependencies,
                                          requiredLiteralValue<int64_t>(p),
                                          p.requiredString("value_string"));
  }
  if (kind == "SgSignedCharVal") {
    return constructRecordNode<SgSignedCharVal>(
        record, ownership, dependencies, requiredLiteralValue<signed char>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgUnsignedCharVal") {
    return constructRecordNode<SgUnsignedCharVal>(
        record, ownership, dependencies, requiredLiteralValue<unsigned char>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgWcharVal") {
    return constructRecordNode<SgWcharVal>(record, ownership, dependencies,
                                           requiredLiteralValue<int64_t>(p),
                                           p.requiredString("value_string"));
  }
  if (kind == "SgChar16Val") {
    return constructRecordNode<SgChar16Val>(
        record, ownership, dependencies,
        requiredLiteralValue<unsigned short>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgChar32Val") {
    return constructRecordNode<SgChar32Val>(
        record, ownership, dependencies, requiredLiteralValue<unsigned int>(p),
        p.requiredString("value_string"));
  }
  if (kind == "SgFloat16Val") {
    return constructRecordNode<SgFloat16Val>(
        record, ownership, dependencies,
        floatingValueFromJson<float>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgBFloat16Val") {
    return constructRecordNode<SgBFloat16Val>(
        record, ownership, dependencies,
        floatingValueFromJson<float>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgFloat32Val") {
    return constructRecordNode<SgFloat32Val>(
        record, ownership, dependencies,
        floatingValueFromJson<float>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgFloat64Val") {
    return constructRecordNode<SgFloat64Val>(
        record, ownership, dependencies,
        floatingValueFromJson<double>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgFloatVal") {
    return constructRecordNode<SgFloatVal>(
        record, ownership, dependencies,
        floatingValueFromJson<float>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgDoubleVal") {
    return constructRecordNode<SgDoubleVal>(
        record, ownership, dependencies,
        floatingValueFromJson<double>(p.at("value_bits")),
        p.requiredString("value_string"));
  }
  if (kind == "SgLongDoubleVal") {
    const auto value = extendedFloatingValueFromJson(p.at("value_bits"));
    return constructRecordNode<SgLongDoubleVal>(
        record, ownership, dependencies, value,
        p.requiredString("value_string"));
  }
  if (kind == "SgFloat80Val") {
    const auto value = extendedFloatingValueFromJson(p.at("value_bits"));
    if (value.format() != Rose::FloatingValue::Format::x87_binary80)
      throw std::runtime_error(
          "AST JSON SgFloat80Val has the wrong floating format");
    return constructRecordNode<SgFloat80Val>(record, ownership, dependencies,
                                             value,
                                             p.requiredString("value_string"));
  }
  if (kind == "SgFloat128Val") {
    const auto value = extendedFloatingValueFromJson(p.at("value_bits"));
    if (value.format() != Rose::FloatingValue::Format::binary128)
      throw std::runtime_error(
          "AST JSON SgFloat128Val has the wrong floating format");
    return constructRecordNode<SgFloat128Val>(record, ownership, dependencies,
                                              value,
                                              p.requiredString("value_string"));
  }
  if (kind == "SgComplexVal") {
    throw std::runtime_error(
        "AST JSON SgComplexVal requires dependency-first construction");
  }
  if (kind == "SgStringVal") {
    const JsonValue &serialized_units = p.at("code_units");
    if (serialized_units.kind != JsonValue::Kind::Array)
      throw std::runtime_error("AST JSON string code_units is not an array");
    SgStringCodeUnitList units;
    for (const JsonValue &entry : serialized_units.array) {
      const int64_t unit = entry.asInt();
      if (unit < 0 || static_cast<uint64_t>(unit) > UINT32_MAX)
        throw std::runtime_error("AST JSON string code unit is out of range");
      units.push_back(static_cast<uint32_t>(unit));
    }
    const auto encoding =
        requiredEnum<SgStringVal::string_literal_encoding_enum>(
            p, "literal_encoding", "SgStringVal",
            {SgStringVal::e_string_encoding_ordinary,
             SgStringVal::e_string_encoding_wide,
             SgStringVal::e_string_encoding_utf8,
             SgStringVal::e_string_encoding_utf16,
             SgStringVal::e_string_encoding_utf32});
    const uint32_t maximum =
        encoding == SgStringVal::e_string_encoding_ordinary ||
                encoding == SgStringVal::e_string_encoding_utf8
            ? 0xffu
        : encoding == SgStringVal::e_string_encoding_utf16 ? 0xffffu
                                                           : UINT32_MAX;
    for (uint32_t unit : units)
      if (unit > maximum)
        throw std::runtime_error(
            "AST JSON string code unit exceeds its encoding");
    const bool unevaluated = p.requiredBool("cxx_unevaluated");
    const int64_t delimiter = p.requiredInt("string_delimiter");
    const bool raw = p.requiredBool("is_raw_string");
    const std::string raw_delimiter = p.requiredString("raw_string_delimiter");
    const std::string raw_payload = p.requiredString("raw_string_payload");
    if (delimiter != 0 && delimiter != '\'' && delimiter != '"' &&
        delimiter != 'H' && delimiter != 'h')
      throw std::runtime_error("AST JSON string delimiter is invalid");
    auto *value = constructRecordNode<SgStringVal>(record, ownership,
                                                   dependencies, units);
    value->set_literal_encoding(encoding);
    value->set_cxx_unevaluated(unevaluated);
    value->set_stringDelimiter(static_cast<char>(delimiter));
    value->set_isRawString(raw);
    value->set_raw_string_delimiter(raw_delimiter);
    value->set_raw_string_payload(raw_payload);
    return value;
  }
  if (kind == "SgAccCollapseClause" || kind == "SgAccNumGangsClause" ||
      kind == "SgAccNumWorkersClause" || kind == "SgAccVectorLengthClause" ||
      kind == "SgAccAsyncClause" || kind == "SgAccIfClause" ||
      kind == "SgAccVectorClause")
    throw std::runtime_error("AST JSON " + kind +
                             " requires typed expression construction");
  if (kind == "SgAccCopyClause" || kind == "SgAccCopyinClause" ||
      kind == "SgAccCopyoutClause" || kind == "SgAccCreateClause" ||
      kind == "SgAccPresentClause" || kind == "SgAccPrivateClause" ||
      kind == "SgAccDeviceptrClause" || kind == "SgAccDeleteClause" ||
      kind == "SgAccReductionClause")
    throw std::runtime_error("AST JSON " + kind +
                             " requires typed variable-list construction");
  if (kind == "SgAccDefaultClause") {
    const int64_t default_kind = p.requiredInt("default_kind");
    if (default_kind < 0 || default_kind > 1) {
      throw std::runtime_error(
          "AST JSON SgAccDefaultClause has an invalid default kind");
    }
    return constructRecordNode<SgAccDefaultClause>(
        record, ownership, dependencies, static_cast<int>(default_kind));
  }
  if (kind == "SgAccGangClause") {
    return constructRecordNode<SgAccGangClause>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgAccSeqClause") {
    return constructRecordNode<SgAccSeqClause>(record, ownership, dependencies);
  }
  if (kind == "SgAccUpdateClause") {
    return constructRecordNode<SgAccUpdateClause>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgAccReadClause") {
    return constructRecordNode<SgAccReadClause>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgAccWriteClause") {
    return constructRecordNode<SgAccWriteClause>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgAccCaptureClause") {
    return constructRecordNode<SgAccCaptureClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgAccParallelStatement" ||
      kind == "SgAccParallelLoopStatement" || kind == "SgAccDataStatement" ||
      kind == "SgAccKernelsStatement" || kind == "SgAccAtomicStatement")
    throw std::runtime_error("AST JSON " + kind +
                             " requires typed body construction");
  if (kind == "SgAccEnterDataStatement") {
    return constructRecordNode<SgAccEnterDataStatement>(record, ownership,
                                                        dependencies);
  }
  if (kind == "SgAccExitDataStatement") {
    return constructRecordNode<SgAccExitDataStatement>(record, ownership,
                                                       dependencies);
  }
  if (kind == "SgAccRoutineStatement") {
    return constructRecordNode<SgAccRoutineStatement>(
        record, ownership, dependencies, p.requiredName("routine_name"));
  }
  if (kind == "SgAccWaitStatement") {
    throw std::runtime_error(
        "AST JSON SgAccWaitStatement requires typed operand construction");
  }
  if (kind == "SgAccCacheStatement") {
    throw std::runtime_error("AST JSON SgAccCacheStatement requires typed "
                             "variable-list construction");
  }
  if (kind == "SgOmpNameExpression") {
    return constructRecordNode<SgOmpNameExpression>(
        record, ownership, dependencies, p.requiredString("spelling"));
  }
  if (kind == "SgOmpSourceExpression") {
    return constructRecordNode<SgOmpSourceExpression>(
        record, ownership, dependencies, p.requiredString("spelling"));
  }
  if (kind == "SgOmpInitModifierList") {
    return constructRecordNode<SgOmpInitModifierList>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgOmpClauseList") {
    return constructRecordNode<SgOmpClauseList>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgEnumVal") {
    throw std::runtime_error(
        "AST JSON SgEnumVal requires exact declaration construction");
  }
  if (kind == "SgTemplateParameterVal") {
    throw std::runtime_error(
        "AST JSON SgTemplateParameterVal requires exact type construction");
  }
  if (kind == "SgThisExp") {
    throw std::runtime_error(
        "AST JSON SgThisExp requires exact symbol and type construction");
  }

  if (kind == "SgNonrealRefExp") {
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp requires exact symbol construction");
  }
  if (kind == "SgPseudoDestructorRefExp") {
    throw std::runtime_error(
        "AST JSON SgPseudoDestructorRefExp requires delayed construction");
  }
  if (kind == "SgNullExpression") {
    const auto role = requiredEnum<SgNullExpression::null_expression_role_enum>(
        p, "role", "SgNullExpression",
        {SgNullExpression::e_null_expression_syntactic_absence});
    SgNullExpression *result =
        constructRecordNode<SgNullExpression>(record, ownership, dependencies);
    result->set_role(role);
    return result;
  }
  if (kind == "SgNullptrValExp") {
    throw std::runtime_error(
        "AST JSON SgNullptrValExp requires exact type construction");
  }
  if (kind == "SgCastExp") {
    (void)castConstructionFieldsFromJson(p);
    throw std::runtime_error(
        "AST JSON SgCastExp requires exact operand and type construction");
  }
  if (kind == "SgConstructorInitializer") {
    throw std::runtime_error(
        "AST JSON SgConstructorInitializer requires delayed construction");
  }
  if (kind == "SgSizeOfOp" || kind == "SgAlignOfOp" || kind == "SgTypeIdOp") {
    if (kind == "SgTypeIdOp")
      (void)p.requiredBool("operand_is_evaluated");
    throw std::runtime_error(
        "AST JSON unary trait requires exact operand and result construction");
  }
  if (kind == "SgNoexceptOp") {
    throw std::runtime_error(
        "AST JSON SgNoexceptOp requires exact operand construction");
  }
  if (kind == "SgVarArgOp") {
    throw std::runtime_error(
        "AST JSON SgVarArgOp requires exact operand and type construction");
  }
  if (kind == "SgConditionalExp") {
    (void)conditionalOperatorKindFromJson(p);
    throw std::runtime_error(
        "AST JSON conditional requires exact operand and type construction");
  }
  if (kind == "SgChooseExpression") {
    throw std::runtime_error("AST JSON SgChooseExpression requires exact "
                             "operand, type and selection construction");
  }
  if (kind == "SgStatementExpression") {
    throw std::runtime_error(
        "AST JSON SgStatementExpression requires its exact body, semantic "
        "scope and result type at construction");
  }
  if (kind == "SgNewExp") {
    throw std::runtime_error(
        "AST JSON new-expression requires exact type and operand construction");
  }
  if (kind == "SgDeleteExp") {
    throw std::runtime_error(
        "AST JSON SgDeleteExp requires its exact operand and operator "
        "declaration at construction");
  }
  if (kind == "SgPackExpansionExpr") {
    throw std::runtime_error("AST JSON SgPackExpansionExpr requires exact "
                             "pattern and type construction");
  }
  if (kind == "SgTypeTraitBuiltinOperator") {
    throw std::runtime_error(
        "AST JSON type-trait builtin requires exact type construction");
  }
  if (kind == "SgLambdaExp") {
    throw std::runtime_error(
        "AST JSON SgLambdaExp requires exact child construction");
  }
  if (kind == "SgLambdaCaptureList") {
    return constructRecordNode<SgLambdaCaptureList>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgDeclarationScopeList") {
    return constructRecordNode<SgDeclarationScopeList>(record, ownership,
                                                       dependencies);
  }
  if (kind == "SgAuxiliaryDeclarationList") {
    return constructRecordNode<SgAuxiliaryDeclarationList>(record, ownership,
                                                           dependencies);
  }
  if (kind == "SgTemplateParameterList") {
    const auto separator =
        requiredEnum<SgTemplateParameterList::source_header_separator_enum>(
            p, "source_header_separator", "SgTemplateParameterList",
            {SgTemplateParameterList::e_source_header_separator_space,
             SgTemplateParameterList::e_source_header_separator_newline});
    auto *parameters = constructRecordNode<SgTemplateParameterList>(
        record, ownership, dependencies);
    parameters->set_source_header_separator(separator);
    return parameters;
  }
  if (kind == "SgLambdaCapture") {
    throw std::runtime_error(
        "AST JSON SgLambdaCapture requires exact operand construction");
  }

  if (isJsonBinaryOpKind(kind)) {
    throw std::runtime_error(
        "AST JSON binary operation requires exact operand and type "
        "construction");
  }

  if (isJsonUnaryOpKind(kind)) {
    (void)unaryConstructionFieldsFromJson(p, kind, dependencies);
    throw std::runtime_error(
        "AST JSON unary operation requires exact operand and type "
        "construction");
  }
  if (kind == "SgSubscriptExpression" || kind == "SgRangeExp") {
    throw std::runtime_error(
        "AST JSON bounded expression requires exact bounds at construction");
  }

  if (kind == "SgTemplateDeclaration") {
    const SgName name = p.requiredName("name");
    const auto template_kind =
        requiredEnum<SgTemplateDeclaration::template_type_enum>(
            p, "template_kind", "SgTemplateDeclaration",
            {SgTemplateDeclaration::e_template_none,
             SgTemplateDeclaration::e_template_class,
             SgTemplateDeclaration::e_template_m_class,
             SgTemplateDeclaration::e_template_function,
             SgTemplateDeclaration::e_template_m_function,
             SgTemplateDeclaration::e_template_m_data,
             SgTemplateDeclaration::e_template_variable});
    SgTemplateDeclaration *declaration =
        constructRecordNode<SgTemplateDeclaration>(record, ownership,
                                                   dependencies, name);
    declaration->set_template_kind(template_kind);
    return declaration;
  }
  if (kind == "SgTemplateParameter") {
    const auto fields = templateParameterSourceFieldsFromJson(p);
    if (fields.kind == SgTemplateParameter::nontype_parameter)
      throw std::runtime_error(
          "AST JSON non-type parameter requires typed declarator construction");
    const uint64_t template_id =
        p.requiredNonnegativeNodeId("template_declaration");
    SgTemplateDeclaration *parameter_template_declaration =
        template_id != 0
            ? nodeByIdAs<SgTemplateDeclaration>(dependencies, template_id)
            : nullptr;
    if ((fields.kind == SgTemplateParameter::template_parameter) !=
        (parameter_template_declaration != nullptr))
      throw std::runtime_error(
          "AST JSON template parameter has no exact declaration input");
    const auto operands =
        templateParameterOperandsFromRecord(record, dependencies);
    if (operands.argument != nullptr)
      ownership.borrow(operands.argument);
    if (operands.sourceDeclaration != nullptr)
      ownership.borrow(operands.sourceDeclaration);
    SgTemplateParameter *parameter = constructRecordNode<SgTemplateParameter>(
        record, ownership, dependencies, fields.kind, nullptr, nullptr, nullptr,
        parameter_template_declaration, operands.argument, nullptr);
    if (operands.argument != nullptr)
      operands.argument->set_parent(parameter);
    parameter->set_sourceSpelledTemplateDeclaration(operands.sourceDeclaration);
    if (operands.sourceDeclaration != nullptr)
      operands.sourceDeclaration->set_parent(parameter);
    parameter->set_templateParameterKeyword(fields.keyword);
    parameter->set_isAbbreviatedFunctionTemplateParameter(fields.abbreviated);
    parameter->set_is_parameter_pack(fields.pack);
    return parameter;
  }

  if (kind == "SgOmpMapClause")
    throw std::runtime_error(
        "AST JSON map clause requires constructor-owned payloads");
  if (kind == "SgOmpDependClause" || kind == "SgOmpAffinityClause")
    throw std::runtime_error(
        "AST JSON iterator clause requires constructor-owned payloads");
  if (kind == "SgOmpToClause" || kind == "SgOmpFromClause")
    throw std::runtime_error(
        "AST JSON transfer clause requires constructor-owned payloads");
  if (ordinaryOmpVariablesClauseVariant(kind))
    throw std::runtime_error(
        "AST JSON variable clause requires constructor-owned lists");
  if (kind == "SgOmpSelfMapsClause") {
    return constructRecordNode<SgOmpSelfMapsClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpIndirectClause") {
    return constructRecordNode<SgOmpIndirectClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpNoOpenmpClause") {
    return constructRecordNode<SgOmpNoOpenmpClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpNoOpenmpRoutinesClause") {
    return constructRecordNode<SgOmpNoOpenmpRoutinesClause>(record, ownership,
                                                            dependencies);
  }
  if (kind == "SgOmpNoParallelismClause") {
    return constructRecordNode<SgOmpNoParallelismClause>(record, ownership,
                                                         dependencies);
  }
  if (kind == "SgOmpAtClause") {
    return constructRecordNode<SgOmpAtClause>(record, ownership, dependencies,
                                              ompAtKindFromJson(p));
  }
  if (kind == "SgOmpSeverityClause") {
    return constructRecordNode<SgOmpSeverityClause>(
        record, ownership, dependencies, ompSeverityKindFromJson(p));
  }
  if (kind == "SgOmpDoacrossClause") {
    (void)ompDoacrossKindFromJson(p);
    throw std::runtime_error(
        "AST JSON SgOmpDoacrossClause requires its typed expression list");
  }
  if (kind == "SgOmpOtherwiseClause") {
    throw std::runtime_error(
        "AST JSON SgOmpOtherwiseClause requires dependency-first construction");
  }
  if (kind == "SgOmpInductionClause") {
    throw std::runtime_error(
        "AST JSON SgOmpInductionClause requires its complete typed item list");
  }
  if (kind == "SgOmpApplyClause") {
    throw std::runtime_error("AST JSON SgOmpApplyClause requires its complete "
                             "typed transformation list");
  }
  if (kind == "SgOmpAbsentClause") {
    return constructRecordNode<SgOmpAbsentClause>(
        record, ownership, dependencies,
        ompDirectiveKindsFromJson(p, "SgOmpAbsentClause"));
  }
  if (kind == "SgOmpContainsClause") {
    return constructRecordNode<SgOmpContainsClause>(
        record, ownership, dependencies,
        ompDirectiveKindsFromJson(p, "SgOmpContainsClause"));
  }
  if (kind == "SgOmpDefaultClause") {
    (void)ompDefaultOptionFromJson(p);
    throw std::runtime_error(
        "AST JSON SgOmpDefaultClause requires dependency-first construction");
  }
  if (kind == "SgOmpProcBindClause") {
    return constructRecordNode<SgOmpProcBindClause>(
        record, ownership, dependencies,
        requiredEnum<SgOmpClause::omp_proc_bind_policy_enum>(
            p, "policy", "SgOmpProcBindClause",
            {SgOmpClause::e_omp_proc_bind_policy_master,
             SgOmpClause::e_omp_proc_bind_policy_close,
             SgOmpClause::e_omp_proc_bind_policy_spread}));
  }
  if (kind == "SgOmpNowaitClause" || kind == "SgOmpOrderedClause" ||
      kind == "SgOmpSizesClause" || kind == "SgOmpAlignClause" ||
      kind == "SgOmpMessageClause" || kind == "SgOmpGraphIdClause" ||
      kind == "SgOmpGraphResetClause" || kind == "SgOmpTransparentClause" ||
      kind == "SgOmpThreadsetClause" || kind == "SgOmpSafesyncClause" ||
      kind == "SgOmpLooprangeClause" ||
      kind == "SgOmpNoOpenmpConstructsClause" || kind == "SgOmpHoldsClause" ||
      kind == "SgOmpUseClause" || kind == "SgOmpIfClause" ||
      kind == "SgOmpCollapseClause" || kind == "SgOmpNumThreadsClause" ||
      kind == "SgOmpNumTeamsClause" || kind == "SgOmpSafelenClause" ||
      kind == "SgOmpSimdlenClause" || kind == "SgOmpDetachClause" ||
      kind == "SgOmpFinalClause" || kind == "SgOmpPriorityClause" ||
      kind == "SgOmpHintClause" || kind == "SgOmpThreadLimitClause" ||
      kind == "SgOmpNocontextClause" || kind == "SgOmpNovariantsClause" ||
      kind == "SgOmpFilterClause" || kind == "SgOmpPartialClause" ||
      kind == "SgOmpGrainsizeClause" || kind == "SgOmpNumTasksClause" ||
      kind == "SgOmpDeviceClause")
    throw std::runtime_error("AST JSON " + kind +
                             " requires dependency-first construction");
  if (kind == "SgOmpReductionClause")
    throw std::runtime_error(
        "AST JSON reduction requires constructor-owned payloads");
  if (kind == "SgOmpLinearClause" || kind == "SgOmpAlignedClause")
    throw std::runtime_error(
        "AST JSON linear/aligned clause requires constructor-owned payloads");
  if (kind == "SgOmpScheduleClause" || kind == "SgOmpDistScheduleClause")
    throw std::runtime_error(
        "AST JSON schedule clause requires constructor-owned chunk size");
  if (kind == "SgOmpOrderClause") {
    return constructRecordNode<SgOmpOrderClause>(
        record, ownership, dependencies,
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
    return constructRecordNode<SgOmpAtomicDefaultMemOrderClause>(
        record, ownership, dependencies,
        requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
            p, "kind", "SgOmpAtomicDefaultMemOrderClause",
            {SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
             SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed}));
  }
  if (kind == "SgOmpDefaultmapClause") {
    return constructRecordNode<SgOmpDefaultmapClause>(
        record, ownership, dependencies,
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
    return constructRecordNode<SgOmpBindClause>(
        record, ownership, dependencies,
        requiredEnum<SgOmpClause::omp_bind_binding_enum>(
            p, "binding", "SgOmpBindClause",
            {SgOmpClause::e_omp_bind_binding_teams,
             SgOmpClause::e_omp_bind_binding_parallel,
             SgOmpClause::e_omp_bind_binding_thread}));
  }
  if (kind == "SgOmpParallelClause") {
    return constructRecordNode<SgOmpParallelClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpSectionsClause") {
    return constructRecordNode<SgOmpSectionsClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpForClause") {
    return constructRecordNode<SgOmpForClause>(record, ownership, dependencies);
  }
  if (kind == "SgOmpTaskgroupClause") {
    return constructRecordNode<SgOmpTaskgroupClause>(record, ownership,
                                                     dependencies);
  }
  if (kind == "SgOmpFullClause") {
    return constructRecordNode<SgOmpFullClause>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgOmpInbranchClause") {
    return constructRecordNode<SgOmpInbranchClause>(record, ownership,
                                                    dependencies);
  }
  if (kind == "SgOmpBeginClause") {
    return constructRecordNode<SgOmpBeginClause>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgOmpEndClause") {
    return constructRecordNode<SgOmpEndClause>(record, ownership, dependencies);
  }
  if (kind == "SgOmpReadClause") {
    return constructRecordNode<SgOmpReadClause>(record, ownership,
                                                dependencies);
  }
  if (kind == "SgOmpWriteClause") {
    return constructRecordNode<SgOmpWriteClause>(record, ownership,
                                                 dependencies);
  }
  if (kind == "SgOmpUpdateClause") {
    return constructRecordNode<SgOmpUpdateClause>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgOmpCaptureClause") {
    return constructRecordNode<SgOmpCaptureClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpCompareClause") {
    return constructRecordNode<SgOmpCompareClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpSeqCstClause") {
    return constructRecordNode<SgOmpSeqCstClause>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgOmpAcqRelClause") {
    return constructRecordNode<SgOmpAcqRelClause>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgOmpReleaseClause") {
    return constructRecordNode<SgOmpReleaseClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpAcquireClause") {
    return constructRecordNode<SgOmpAcquireClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpRelaxedClause") {
    return constructRecordNode<SgOmpRelaxedClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpFailClause") {
    return constructRecordNode<SgOmpFailClause>(
        record, ownership, dependencies,
        requiredEnum<SgOmpClause::omp_fail_memory_order_kind_enum>(
            p, "memory_order", "SgOmpFailClause",
            {SgOmpClause::e_omp_fail_memory_order_kind_seq_cst,
             SgOmpClause::e_omp_fail_memory_order_kind_acquire,
             SgOmpClause::e_omp_fail_memory_order_kind_relaxed}));
  }
  if (kind == "SgOmpAllocateClause" || kind == "SgOmpAllocatorClause")
    throw std::runtime_error(
        "AST JSON allocator requires constructor-owned payloads");
  if (kind == "SgOmpAdjustArgsClause") {
    throw std::runtime_error(
        "AST JSON adjust_args requires exact typed argument construction");
  }
  if (kind == "SgOmpAppendArgsClause") {
    throw std::runtime_error(
        "AST JSON append_args requires exact typed operation construction");
  }
  if (kind == "SgOmpInReductionClause" || kind == "SgOmpTaskReductionClause")
    throw std::runtime_error(
        "AST JSON reduction requires constructor-owned payloads");
  if (kind == "SgOmpNogroupClause") {
    return constructRecordNode<SgOmpNogroupClause>(record, ownership,
                                                   dependencies);
  }
  if (kind == "SgOmpUntiedClause") {
    return constructRecordNode<SgOmpUntiedClause>(record, ownership,
                                                  dependencies);
  }
  if (kind == "SgOmpMergeableClause") {
    return constructRecordNode<SgOmpMergeableClause>(record, ownership,
                                                     dependencies);
  }
  if (kind == "SgOmpReverseOffloadClause") {
    return constructRecordNode<SgOmpReverseOffloadClause>(record, ownership,
                                                          dependencies);
  }
  if (kind == "SgOmpUnifiedAddressClause") {
    return constructRecordNode<SgOmpUnifiedAddressClause>(record, ownership,
                                                          dependencies);
  }
  if (kind == "SgOmpUnifiedSharedMemoryClause") {
    return constructRecordNode<SgOmpUnifiedSharedMemoryClause>(
        record, ownership, dependencies);
  }
  if (kind == "SgOmpDynamicAllocatorsClause") {
    return constructRecordNode<SgOmpDynamicAllocatorsClause>(record, ownership,
                                                             dependencies);
  }
  if (kind == "SgOmpDepobjUpdateClause") {
    return constructRecordNode<SgOmpDepobjUpdateClause>(
        record, ownership, dependencies,
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
    throw std::runtime_error("AST JSON SgOmpDestroyClause requires its exact "
                             "operand at construction");
  }
  if (kind == "SgOmpUsesAllocatorsClause") {
    return constructRecordNode<SgOmpUsesAllocatorsClause>(record, ownership,
                                                          dependencies);
  }
  if (kind == "SgOmpUsesAllocatorsDefination") {
    const auto allocator =
        requiredEnum<SgOmpClause::omp_uses_allocators_allocator_enum>(
            p, "allocator", kind,
            {SgOmpClause::e_omp_uses_allocators_allocator_unknown,
             SgOmpClause::e_omp_uses_allocators_allocator_default_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_large_cap_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_const_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_high_bw_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_low_lat_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_cgroup_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_pteam_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_thread_mem_alloc,
             SgOmpClause::e_omp_uses_allocators_allocator_user_defined,
             SgOmpClause::e_omp_uses_allocators_allocator_traits});
    SgOmpUsesAllocatorsDefination *definition =
        constructRecordNode<SgOmpUsesAllocatorsDefination>(record, ownership,
                                                           dependencies);
    definition->set_allocator(allocator);
    return definition;
  }
  if (kind == "SgOmpContextSelectorProperty") {
    const auto fields = contextPropertyFieldsFromRecord(record);
    if (singleEdgeTarget(record, "expression") != 0 ||
        singleEdgeTarget(record, "requires_expression") != 0)
      throw std::runtime_error(
          "AST JSON context property requires constructor-owned expressions");
    auto *property = constructRecordNode<SgOmpContextSelectorProperty>(
        record, ownership, dependencies);
    fields.initialize(property, nullptr, nullptr);
    return property;
  }
  if (kind == "SgOmpContextSelector" || kind == "SgOmpContextSelectorSet" ||
      kind == "SgOmpMatchClause")
    throw std::runtime_error(
        "AST JSON context hierarchy requires constructor-owned payloads");
  if (kind == "SgOmpWhenClause") {
    throw std::runtime_error(
        "AST JSON SgOmpWhenClause requires dependency-first construction");
  }
  if (kind == "SgOmpNotinbranchClause") {
    return constructRecordNode<SgOmpNotinbranchClause>(record, ownership,
                                                       dependencies);
  }
  if (kind == "SgOmpExtImplementationDefinedRequirementClause") {
    return constructRecordNode<SgOmpExtImplementationDefinedRequirementClause>(
        record, ownership, dependencies, static_cast<SgExpression *>(nullptr));
  }
  if (kind == "SgOmpRequiresStatement") {
    return constructRecordNode<SgOmpRequiresStatement>(record, ownership,
                                                       dependencies);
  }
  if (kind == "SgOmpBarrierStatement") {
    return constructRecordNode<SgOmpBarrierStatement>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgOmpNothingStatement") {
    return constructRecordNode<SgOmpNothingStatement>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgOmpTaskwaitStatement") {
    return constructRecordNode<SgOmpTaskwaitStatement>(record, ownership,
                                                       dependencies);
  }
  if (kind == "SgOmpOrderedDependStatement") {
    return constructRecordNode<SgOmpOrderedDependStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpErrorStatement") {
    return constructRecordNode<SgOmpErrorStatement>(record, ownership,
                                                    dependencies, omp_clauses);
  }
  if (kind == "SgOmpInteropStatement") {
    return constructRecordNode<SgOmpInteropStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpScanStatement") {
    return constructRecordNode<SgOmpScanStatement>(record, ownership,
                                                   dependencies, omp_clauses);
  }
  if (kind == "SgOmpFlushStatement") {
    auto *variables = nodeByIdAs<SgExprListExp>(
        dependencies, requiredSingleEdgeTarget(record, "variable_list"));
    ownership.borrow(variables);
    return constructRecordNode<SgOmpFlushStatement>(
        record, ownership, dependencies, nullptr, variables, omp_clauses);
  }
  if (kind == "SgOmpDeclareSimdStatement") {
    throw std::runtime_error(
        "AST JSON SgOmpDeclareSimdStatement requires delayed exact-target "
        "construction");
  }
  if (kind == "SgOmpDeclareMapperStatement") {
    throw std::runtime_error(
        "AST JSON SgOmpDeclareMapperStatement requires typed operand "
        "construction");
  }
  if (kind == "SgOmpDeclareTargetStatement") {
    const auto device_type =
        ompDeviceTypeKindFromJson(p, "SgOmpDeclareTargetStatement");
    const bool underscore = p.requiredBool("use_underscore_spelling");
    SgOmpDeclareTargetStatement *stmt =
        constructRecordNode<SgOmpDeclareTargetStatement>(record, ownership,
                                                         dependencies);
    stmt->set_device_type_kind(device_type);
    stmt->set_use_underscore_spelling(underscore);
    return stmt;
  }
  if (kind == "SgOmpBeginDeclareTargetStatement") {
    const bool underscore = p.requiredBool("use_underscore_spelling");
    SgOmpBeginDeclareTargetStatement *stmt =
        constructRecordNode<SgOmpBeginDeclareTargetStatement>(record, ownership,
                                                              dependencies);
    stmt->set_use_underscore_spelling(underscore);
    return stmt;
  }
  if (kind == "SgOmpEndDeclareTargetStatement") {
    const bool underscore = p.requiredBool("use_underscore_spelling");
    SgOmpEndDeclareTargetStatement *stmt =
        constructRecordNode<SgOmpEndDeclareTargetStatement>(record, ownership,
                                                            dependencies);
    stmt->set_use_underscore_spelling(underscore);
    return stmt;
  }
  if (kind == "SgOmpAssumesStatement") {
    return constructRecordNode<SgOmpAssumesStatement>(record, ownership,
                                                      dependencies);
  }
  if (kind == "SgOmpBeginAssumesStatement") {
    return constructRecordNode<SgOmpBeginAssumesStatement>(record, ownership,
                                                           dependencies);
  }
  if (kind == "SgOmpEndAssumesStatement") {
    return constructRecordNode<SgOmpEndAssumesStatement>(record, ownership,
                                                         dependencies);
  }
  if (kind == "SgOmpEndAssumeStatement") {
    return constructRecordNode<SgOmpEndAssumeStatement>(record, ownership,
                                                        dependencies);
  }
  if (kind == "SgOmpGroupprivateStatement") {
    const auto device_type = ompDeviceTypeKindFromJson(p, kind);
    auto *variables = nodeByIdAs<SgExprListExp>(
        dependencies, requiredSingleEdgeTarget(record, "variables"));
    ownership.borrow(variables);
    auto *statement = constructRecordNode<SgOmpGroupprivateStatement>(
        record, ownership, dependencies, variables, omp_clauses);
    variables->set_parent(statement);
    statement->set_device_type_kind(device_type);
    return statement;
  }
  if (kind == "SgOmpDeclareVariantStatement") {
    throw std::runtime_error(
        "AST JSON SgOmpDeclareVariantStatement requires delayed exact-target "
        "construction");
  }
  if (kind == "SgOmpBeginDeclareVariantStatement") {
    return constructRecordNode<SgOmpBeginDeclareVariantStatement>(
        record, ownership, dependencies);
  }
  if (kind == "SgOmpEndDeclareVariantStatement") {
    return constructRecordNode<SgOmpEndDeclareVariantStatement>(
        record, ownership, dependencies);
  }
  if (kind == "SgOmpThreadprivateStatement") {
    return constructRecordNode<SgOmpThreadprivateStatement>(record, ownership,
                                                            dependencies);
  }
  if (kind == "SgOmpAllocateStatement") {
    auto *variables = nodeByIdAs<SgExprListExp>(
        dependencies, requiredSingleEdgeTarget(record, "variable_list"));
    ownership.borrow(variables);
    return constructRecordNode<SgOmpAllocateStatement>(
        record, ownership, dependencies, nullptr, variables, omp_clauses);
  }
  if (kind == "SgOmpTargetUpdateStatement") {
    return constructRecordNode<SgOmpTargetUpdateStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpTargetEnterDataStatement") {
    return constructRecordNode<SgOmpTargetEnterDataStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpTargetExitDataStatement") {
    return constructRecordNode<SgOmpTargetExitDataStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpTargetStatement") {
    return constructRecordNode<SgOmpTargetStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetDataStatement") {
    return constructRecordNode<SgOmpTargetDataStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetDataCompositeStatement") {
    return constructRecordNode<SgOmpTargetDataCompositeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpScopeStatement") {
    return constructRecordNode<SgOmpScopeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelMaskedStatement") {
    return constructRecordNode<SgOmpParallelMaskedStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpAssumeStatement") {
    return constructRecordNode<SgOmpAssumeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskgraphStatement") {
    return constructRecordNode<SgOmpTaskgraphStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpFuseStatement") {
    return constructRecordNode<SgOmpFuseStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpInterchangeStatement") {
    return constructRecordNode<SgOmpInterchangeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpReverseStatement") {
    return constructRecordNode<SgOmpReverseStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelStatement") {
    return constructRecordNode<SgOmpParallelStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpForStatement") {
    return constructRecordNode<SgOmpForStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDoStatement") {
    return constructRecordNode<SgOmpDoStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpForSimdStatement") {
    return constructRecordNode<SgOmpForSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpLoopStatement") {
    return constructRecordNode<SgOmpLoopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpSimdStatement") {
    return constructRecordNode<SgOmpSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpSingleStatement") {
    return constructRecordNode<SgOmpSingleStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskStatement") {
    const bool available =
        p.requiredBool("semantic_data_environment_is_available");
    SgInitializedNamePtrList firstprivate, private_variables;
    for (const auto &entry :
         {std::make_pair("semantic_firstprivate_variables", &firstprivate),
          std::make_pair("semantic_private_variables", &private_variables)})
      for (const EdgeRecord &edge : edgesFor(record, entry.first))
        entry.second->push_back(
            nodeByIdAs<SgInitializedName>(dependencies, edge.target));
    if (!available && (omp_body != nullptr || !firstprivate.empty() ||
                       !private_variables.empty()))
      throw std::runtime_error(
          "AST JSON task has an unavailable semantic data environment");
    auto *task = constructRecordNode<SgOmpTaskStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
    if (available)
      task->initialize_semantic_data_environment(firstprivate,
                                                 private_variables);
    return task;
  }
  if (kind == "SgOmpMasterStatement") {
    return constructRecordNode<SgOmpMasterStatement>(record, ownership,
                                                     dependencies, omp_body);
  }
  if (kind == "SgOmpMaskedStatement") {
    return constructRecordNode<SgOmpMaskedStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpMaskedTaskloopStatement") {
    return constructRecordNode<SgOmpMaskedTaskloopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpMaskedTaskloopSimdStatement") {
    return constructRecordNode<SgOmpMaskedTaskloopSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelMasterStatement") {
    return constructRecordNode<SgOmpParallelMasterStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpMasterTaskloopStatement") {
    return constructRecordNode<SgOmpMasterTaskloopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpMasterTaskloopSimdStatement") {
    return constructRecordNode<SgOmpMasterTaskloopSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelMasterTaskloopStatement") {
    return constructRecordNode<SgOmpParallelMasterTaskloopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelMasterTaskloopSimdStatement") {
    return constructRecordNode<SgOmpParallelMasterTaskloopSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpSectionStatement") {
    return constructRecordNode<SgOmpSectionStatement>(record, ownership,
                                                      dependencies, omp_body);
  }
  if (kind == "SgOmpSectionsStatement") {
    return constructRecordNode<SgOmpSectionsStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpCriticalStatement") {
    return constructRecordNode<SgOmpCriticalStatement>(
        record, ownership, dependencies, omp_body, omp_clauses,
        p.requiredName("name"));
  }
  if (kind == "SgOmpAtomicStatement") {
    return constructRecordNode<SgOmpAtomicStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpOrderedStatement") {
    return constructRecordNode<SgOmpOrderedStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpParallelLoopStatement") {
    return constructRecordNode<SgOmpParallelLoopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskgroupStatement") {
    return constructRecordNode<SgOmpTaskgroupStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskloopStatement") {
    return constructRecordNode<SgOmpTaskloopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskloopSimdStatement") {
    return constructRecordNode<SgOmpTaskloopSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTaskyieldStatement") {
    return constructRecordNode<SgOmpTaskyieldStatement>(record, ownership,
                                                        dependencies);
  }
  if (kind == "SgOmpCancelStatement") {
    return constructRecordNode<SgOmpCancelStatement>(record, ownership,
                                                     dependencies, omp_clauses);
  }
  if (kind == "SgOmpCancellationPointStatement") {
    return constructRecordNode<SgOmpCancellationPointStatement>(
        record, ownership, dependencies, omp_clauses);
  }
  if (kind == "SgOmpDepobjStatement") {
    auto *depobj = nodeByIdAs<SgExpression>(
        dependencies, requiredSingleEdgeTarget(record, "depobj"));
    ownership.borrow(depobj);
    return constructRecordNode<SgOmpDepobjStatement>(
        record, ownership, dependencies, omp_clauses, depobj);
  }
  if (kind == "SgOmpMetadirectiveStatement") {
    return constructRecordNode<SgOmpMetadirectiveStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDispatchStatement") {
    return constructRecordNode<SgOmpDispatchStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDistributeStatement") {
    return constructRecordNode<SgOmpDistributeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpWorkdistributeStatement") {
    return constructRecordNode<SgOmpWorkdistributeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDistributeSimdStatement") {
    return constructRecordNode<SgOmpDistributeSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDistributeParallelForStatement") {
    return constructRecordNode<SgOmpDistributeParallelForStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpDistributeParallelForSimdStatement") {
    return constructRecordNode<SgOmpDistributeParallelForSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetParallelStatement") {
    return constructRecordNode<SgOmpTargetParallelStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetParallelForStatement") {
    return constructRecordNode<SgOmpTargetParallelForStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetParallelForSimdStatement") {
    return constructRecordNode<SgOmpTargetParallelForSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetParallelLoopStatement") {
    return constructRecordNode<SgOmpTargetParallelLoopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetSimdStatement") {
    return constructRecordNode<SgOmpTargetSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsStatement") {
    return constructRecordNode<SgOmpTargetTeamsStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsStatement") {
    return constructRecordNode<SgOmpTeamsStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsDistributeStatement") {
    return constructRecordNode<SgOmpTeamsDistributeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsDistributeSimdStatement") {
    return constructRecordNode<SgOmpTeamsDistributeSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsDistributeParallelForStatement") {
    return constructRecordNode<SgOmpTeamsDistributeParallelForStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsDistributeParallelForSimdStatement") {
    return constructRecordNode<SgOmpTeamsDistributeParallelForSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTeamsLoopStatement") {
    return constructRecordNode<SgOmpTeamsLoopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpWorkshareStatement") {
    return constructRecordNode<SgOmpWorkshareStatement>(record, ownership,
                                                        dependencies, omp_body);
  }
  if (kind == "SgOmpUnrollStatement") {
    return constructRecordNode<SgOmpUnrollStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTileStatement") {
    return constructRecordNode<SgOmpTileStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsDistributeStatement") {
    return constructRecordNode<SgOmpTargetTeamsDistributeStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsDistributeSimdStatement") {
    return constructRecordNode<SgOmpTargetTeamsDistributeSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsLoopStatement") {
    return constructRecordNode<SgOmpTargetTeamsLoopStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForStatement") {
    return constructRecordNode<SgOmpTargetTeamsDistributeParallelForStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }
  if (kind == "SgOmpTargetTeamsDistributeParallelForSimdStatement") {
    return constructRecordNode<
        SgOmpTargetTeamsDistributeParallelForSimdStatement>(
        record, ownership, dependencies, omp_body, omp_clauses);
  }

  throw std::runtime_error("AST JSON deserializer does not support Sage node " +
                           kind);
}

StatementFields statementFieldsFromJson(const NodeRecord &record,
                                        VariantT kind) {
  static const VariantVector scope_kinds(V_SgScopeStatement);
  static const VariantVector variable_kinds(V_SgVariableDeclaration);
  static const VariantVector class_kinds(V_SgClassDeclaration);
  static const VariantVector function_kinds(V_SgFunctionDeclaration);
  const auto matches = [&](const VariantVector &kinds) {
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
  };
  StatementFields fields;
  if (matches(scope_kinds))
    fields.case_insensitive =
        record.properties.requiredBool("case_insensitive");
  fields.source_sequence = sourceSequenceValueFromJson(record.properties);
  fields.directive_end = requiredEnum<SgStatement::directive_end_kind_enum>(
      record.properties, "directive_end_kind", "SgStatement",
      {SgStatement::e_directive_end_not_applicable,
       SgStatement::e_directive_end_implicit,
       SgStatement::e_directive_end_explicit},
      "directive-end kind");
  fields.fortran_spelling =
      requiredEnum<SgStatement::omp_fortran_spelling_enum>(
          record.properties, "omp_fortran_spelling", "SgStatement",
          {SgStatement::e_omp_fortran_spelling_not_applicable,
           SgStatement::e_omp_fortran_spelling_do},
          "OpenMP Fortran spelling");
  const char *specialization_context = nullptr;
  if (matches(variable_kinds)) {
    fields.variable_constexpr = record.properties.requiredBool("is_constexpr");
    fields.variable_thread_local =
        record.properties.requiredBool("is_thread_local");
    fields.variable_origin =
        requiredEnum<SgVariableDeclaration::fortran_declaration_origin_enum>(
            record.properties, "fortran_declaration_origin", record.kind,
            {SgVariableDeclaration::e_fortran_source_declaration,
             SgVariableDeclaration::e_fortran_semantic_only_declaration,
             SgVariableDeclaration::e_fortran_pending_source_declaration});
    fields.variable_global_type_qualification = record.properties.requiredBool(
        "requires_global_name_qualification_on_type");
    specialization_context = "SgVariableDeclaration";
  } else if (matches(class_kinds))
    specialization_context = "SgClassDeclaration";
  else if (matches(function_kinds))
    specialization_context = "SgFunctionDeclaration";
  if (specialization_context != nullptr)
    fields.specialization =
        requiredEnum<SgDeclarationStatement::template_specialization_enum>(
            record.properties, "specialization", specialization_context,
            {SgDeclarationStatement::e_no_specialization,
             SgDeclarationStatement::e_partial_specialization,
             SgDeclarationStatement::e_specialization});
  return fields;
}

void StatementFields::initialize(SgStatement *statement) const {
  ASSERT_not_null(statement);
  if (auto *scope = isSgScopeStatement(statement)) {
    ROSE_ASSERT(case_insensitive.has_value());
    scope->setCaseInsensitive(*case_insensitive);
  }
  statement->set_source_sequence_value(source_sequence);
  statement->set_directive_end_kind(directive_end);
  statement->set_omp_fortran_spelling(fortran_spelling);
  const auto initialize_specialization = [&](auto *declaration) {
    ROSE_ASSERT(specialization.has_value());
    declaration->set_specialization(*specialization);
  };
  if (auto *variable = isSgVariableDeclaration(statement)) {
    ROSE_ASSERT(variable_constexpr.has_value() &&
                variable_thread_local.has_value() &&
                variable_origin.has_value() &&
                variable_global_type_qualification.has_value());
    variable->set_is_constexpr(*variable_constexpr);
    variable->set_is_thread_local(*variable_thread_local);
    variable->set_fortran_declaration_origin(*variable_origin);
    variable->set_requiresGlobalNameQualificationOnType(
        *variable_global_type_qualification);
    initialize_specialization(variable);
  } else if (auto *class_declaration = isSgClassDeclaration(statement))
    initialize_specialization(class_declaration);
  else if (auto *function = isSgFunctionDeclaration(statement))
    initialize_specialization(function);
}

OmpClauseFields ompClauseFieldsFromJson(const NodeRecord &record) {
  OmpClauseFields fields{
      requiredEnum<SgOmpClause::omp_directive_name_modifier_enum>(
          record.properties, "directive_name_modifier", record.kind,
          {SgOmpClause::e_omp_directive_name_modifier_unspecified,
           SgOmpClause::e_omp_directive_name_modifier_parallel,
           SgOmpClause::e_omp_directive_name_modifier_for,
           SgOmpClause::e_omp_directive_name_modifier_do,
           SgOmpClause::e_omp_directive_name_modifier_distribute,
           SgOmpClause::e_omp_directive_name_modifier_sections,
           SgOmpClause::e_omp_directive_name_modifier_single,
           SgOmpClause::e_omp_directive_name_modifier_scope,
           SgOmpClause::e_omp_directive_name_modifier_target,
           SgOmpClause::e_omp_directive_name_modifier_task,
           SgOmpClause::e_omp_directive_name_modifier_taskloop,
           SgOmpClause::e_omp_directive_name_modifier_teams,
           SgOmpClause::e_omp_directive_name_modifier_unknown}),
      std::nullopt};
  const JsonValue &source_order = record.properties.at("combined_source_order");
  if (source_order.kind != JsonValue::Kind::Null) {
    const int64_t value = source_order.asInt();
    if (value < 0 || static_cast<uint64_t>(value) >
                         std::numeric_limits<std::size_t>::max()) {
      throw std::runtime_error(
          "AST JSON OpenMP combined clause source order is out of range");
    }
    fields.source_order = static_cast<std::size_t>(value);
  }
  return fields;
}

void OmpClauseFields::initialize(SgOmpClause *clause) const {
  ASSERT_not_null(clause);
  clause->set_directive_name_modifier(directive_modifier);
  if (auto *directive = isSgOmpDirectiveKindClause(clause))
    directive->validate_directive_kinds();
  if (source_order.has_value())
    clause->initialize_combined_source_order(*source_order);
}

FunctionModifierFields
functionModifierFieldsFromJson(const NodeRecord &record) {
  const auto &p = record.properties;
  FunctionModifierFields fields{
      bitVectorFromJson(p.at("function_modifier_vector"),
                        "function_modifier_vector",
                        SgFunctionModifier::e_last_modifier),
      bitVectorFromJson(p.at("special_function_modifier_vector"),
                        "special_function_modifier_vector",
                        SgSpecialFunctionModifier::e_last_modifier)};
  const bool constructor =
      fields.special[SgSpecialFunctionModifier::e_constructor];
  if (fields.function[SgFunctionModifier::e_marked_delete] &&
      (!fields.function[SgFunctionModifier::e_cxx_deleted] ||
       fields.function[SgFunctionModifier::e_marked_default]))
    throw std::runtime_error(
        "AST JSON function has invalid deletion properties");
  unsigned copy_move_roles = 0;
  for (auto flag : {SgFunctionModifier::e_cxx_copy_constructor,
                    SgFunctionModifier::e_cxx_move_constructor,
                    SgFunctionModifier::e_cxx_copy_assignment,
                    SgFunctionModifier::e_cxx_move_assignment})
    copy_move_roles += fields.function[flag] ? 1 : 0;
  const bool copy_move_constructor =
      fields.function[SgFunctionModifier::e_cxx_copy_constructor] ||
      fields.function[SgFunctionModifier::e_cxx_move_constructor];
  const bool copy_move_assignment =
      fields.function[SgFunctionModifier::e_cxx_copy_assignment] ||
      fields.function[SgFunctionModifier::e_cxx_move_assignment];
  static const VariantVector members(V_SgMemberFunctionDeclaration);
  if (copy_move_roles > 1 ||
      (copy_move_roles != 0 && !kindInVariants(record.kind, members)) ||
      (copy_move_constructor &&
       (!constructor ||
        fields.special[SgSpecialFunctionModifier::e_operator])) ||
      (copy_move_assignment &&
       (constructor ||
        !fields.special[SgSpecialFunctionModifier::e_operator])) ||
      (copy_move_roles != 0 &&
       (fields.special[SgSpecialFunctionModifier::e_destructor] ||
        fields.special[SgSpecialFunctionModifier::e_conversion] ||
        fields.special[SgSpecialFunctionModifier::e_uld_operator] ||
        p.requiredBool("is_deduction_guide"))))
    throw std::runtime_error(
        "AST JSON function has invalid copy/move properties");
  const bool is_default =
      fields.function[SgFunctionModifier::e_cxx_default_constructor];
  const bool trivial =
      fields.function[SgFunctionModifier::e_cxx_trivial_special_member];
  const bool nontrivial =
      fields.function[SgFunctionModifier::e_cxx_nontrivial_special_member];
  const bool classified =
      p.requiredInt("frontend_declaration_origin") !=
      SgFunctionDeclaration::e_frontend_declaration_unclassified;
  const bool special_member = constructor || copy_move_assignment;
  if ((trivial && nontrivial) || (!constructor && is_default) ||
      (!special_member &&
       (trivial || nontrivial ||
        fields
            .function[SgFunctionModifier::e_cxx_ineligible_special_member])) ||
      (special_member && !trivial && !nontrivial))
    throw std::runtime_error(
        "AST JSON function has invalid special-member properties");
  unsigned destructor_properties = 0;
  for (auto flag : {SgFunctionModifier::e_cxx_dependent_destructor,
                    SgFunctionModifier::e_cxx_unselected_destructor,
                    SgFunctionModifier::e_cxx_trivial_destructor,
                    SgFunctionModifier::e_cxx_nontrivial_destructor})
    destructor_properties += fields.function[flag] ? 1 : 0;
  const bool destructor =
      fields.special[SgSpecialFunctionModifier::e_destructor];
  if (destructor_properties > 1 ||
      (!destructor && destructor_properties != 0) ||
      (destructor && classified && destructor_properties != 1))
    throw std::runtime_error(
        "AST JSON function has invalid destructor properties");
  if (fields.function[SgFunctionModifier::e_explicit] && !constructor &&
      !fields.special[SgSpecialFunctionModifier::e_conversion] &&
      !p.requiredBool("is_deduction_guide"))
    throw std::runtime_error("AST JSON explicit specifier has no typed owner");
  return fields;
}

SourceDeclaratorScopeConstruction::SourceDeclaratorScopeConstruction(
    const NodeRecord &record, SgNode *node, const NodeMap &nodes) {
  auto *owner = isSgDeclarationStatement(node);
  const uint64_t scope_id = singleEdgeTarget(record, "source_declarator_scope");
  if (scope_id == 0) {
    if (owner != nullptr && owner->get_source_declarator_scope() != nullptr)
      throw std::runtime_error(
          "AST JSON declaration has an unrecorded source declarator scope");
    return;
  }
  if (owner == nullptr || isSgFunctionDeclaration(owner) != nullptr)
    throw std::runtime_error(
        "AST JSON source declarator scope requires a nonfunction declaration");
  auto *scope = nodeByIdAs<SgDeclarationScope>(nodes, scope_id);
  if (owner->get_source_declarator_scope() != nullptr) {
    if (owner->get_source_declarator_scope() != scope ||
        scope->get_parent() != owner)
      throw std::runtime_error("AST JSON declaration changed its constructed "
                               "source scope ownership");
    owner_ = owner;
    scope_ = scope;
    return;
  }
  if (owner->get_parent() != nullptr || scope->get_parent() != nullptr ||
      !scope->get_declarations().empty() ||
      scope->get_is_default_nonreal_scope())
    throw std::runtime_error("AST JSON source declarator scope has conflicting "
                             "construction ownership");
  SageBuilder::beginSourceDeclaratorScopeConstruction(owner, scope);
  owner_ = owner;
  scope_ = scope;
  attached_ = true;
}

void SourceDeclaratorScopeConstruction::populate(const AstFileRecord &ast,
                                                 const NodeRecord &record,
                                                 const NodeMap &nodes) {
  if (owner_ == nullptr)
    return;
  const auto &scope_record =
      ast.node(requiredSingleEdgeTarget(record, "source_declarator_scope"));
  SgDeclarationStatementPtrList declarations;
  for (const auto &edge : edgesFor(scope_record, "declarations")) {
    auto *tag = nodeByIdAs<SgDeclarationStatement>(nodes, edge.target);
    if (tag->get_parent() != scope_ || tag->get_scope() == nullptr)
      throw std::runtime_error("AST JSON source declarator tag lacks its "
                               "constructed owner or scope");
    declarations.push_back(tag);
  }
  if (!scope_->get_declarations().empty()) {
    if (scope_->get_declarations() != declarations)
      throw std::runtime_error(
          "AST JSON source declarator scope changed its constructed tag list");
  } else {
    scope_->get_declarations() = std::move(declarations);
    populated_ = true;
  }
  SageBuilder::finishSourceDeclaratorScopeConstruction(owner_, scope_);
}

SourceDeclaratorScopeConstruction::~SourceDeclaratorScopeConstruction() {
  if (owner_ != nullptr) {
    ROSE_ASSERT(owner_->get_source_declarator_scope() == scope_ &&
                scope_->get_parent() == owner_);
    if (populated_)
      scope_->get_declarations().clear();
    if (attached_) {
      owner_->set_source_declarator_scope(nullptr);
      scope_->set_parent(nullptr);
    }
  }
}

static SgNode *createNodeFromRecordWithDependencies(
    const NodeRecord &record, std::optional<SourcePositions> positions,
    SgProject *project, SgPragma *pragma_child,
    SgTemplateClassDeclaration *template_declaration,
    SgClassDeclaration *class_declaration,
    SgFunctionDeclaration *function_declaration, SgBasicBlock *function_body,
    SgNamespaceDeclarationStatement *namespace_declaration,
    SgDeclarationStatement *declaration_target, SgInitializedName *using_name,
    SgScopeStatement *enum_scope, SgModuleStatement *use_module = nullptr,
    const SgDeclarationStatementPtrList *dependent_template_candidates =
        nullptr,
    const NodeMap &dependencies = {}) {
  const auto position_fields = validateSourcePositionRecord(record);
  const bool is_modified = record.flags.requiredBool("is_modified");
  if (uint64_t scope_id = singleEdgeTarget(record, "source_declarator_scope")) {
    static const VariantVector declarations(V_SgDeclarationStatement);
    static const VariantVector functions(V_SgFunctionDeclaration);
    if (!kindInVariants(record.kind, declarations) ||
        kindInVariants(record.kind, functions))
      throw std::runtime_error("AST JSON source declarator scope requires a "
                               "nonfunction declaration");
    auto *scope = nodeByIdAs<SgDeclarationScope>(dependencies, scope_id);
    if (scope->get_parent() != nullptr || !scope->get_declarations().empty() ||
        scope->get_is_default_nonreal_scope())
      throw std::runtime_error(
          "AST JSON source declarator scope has conflicting "
          "construction ownership");
  }
  SgScopeStatement *parameter_enclosing_scope = nullptr;
  if (record.kind == "SgFunctionParameterScope")
    if (uint64_t enclosing_id =
            singleEdgeTarget(record, "completed_semantic_scope"))
      parameter_enclosing_scope =
          nodeByIdAs<SgScopeStatement>(dependencies, enclosing_id);
  SgNode::SourceStateConstruction construction;
  SourceTypeConstruction source_types;
  RecordNodeConstruction ownership;
  SgNode *node = ownership.own(createNodeFromRecordImpl(
      record, project, ownership, pragma_child, template_declaration,
      class_declaration, function_declaration, function_body,
      namespace_declaration, declaration_target, using_name, enum_scope,
      use_module, dependent_template_candidates, dependencies));
  if (auto *statement = isSgOmpBodyStatement(node))
    if (auto *body = statement->get_body())
      body->set_parent(statement);
  installSourcePositions(node,
                         positions ? std::move(*positions)
                                   : constructSourcePositions(position_fields),
                         record.kind);
  if (parameter_enclosing_scope != nullptr)
    SageInterface::beginDetachedFunctionParameterScopeConstruction(
        isSgFunctionParameterScope(node), parameter_enclosing_scope,
        parameter_enclosing_scope);
  SourceDeclaratorScopeConstruction source_scope(record, node, dependencies);
  node->set_isModified(is_modified);
  if (isSgProgramHeaderStatement(node) != nullptr ||
      isSgProcedureHeaderStatement(node) != nullptr)
    SageInterface::isFortranProgramUnitWithoutSourceName(
        isSgFunctionDeclaration(node));
  if (isJsonScalarLiteralKind(record.kind))
    initializeScalarLiteralType(isSgValueExp(node), record.properties,
                                dependencies);
  if (auto *parameter = isSgTemplateParameter(node))
    initializeTemplateParameterTypes(parameter, record, dependencies);
  source_scope.publish();
  source_types.publish();
  ownership.publish();
  return node;
}

SgNode *createNodeFromRecord(
    const NodeRecord &record, SgProject *project, SgPragma *pragma_child,
    SgTemplateClassDeclaration *template_declaration,
    SgClassDeclaration *class_declaration,
    SgNamespaceDeclarationStatement *namespace_declaration,
    SgDeclarationStatement *declaration_target, SgInitializedName *using_name,
    const SgDeclarationStatementPtrList *dependent_template_candidates,
    const NodeMap &dependencies) {
  return createNodeFromRecordWithDependencies(
      record, std::nullopt, project, pragma_child, template_declaration,
      class_declaration, nullptr, nullptr, namespace_declaration,
      declaration_target, using_name, nullptr, nullptr,
      dependent_template_candidates, dependencies);
}

static SgNode *createNodeFromRecordWithMappedDependencies(
    const NodeRecord &record, SgProject *project, const NodeMap &dependencies,
    std::optional<SourcePositions> positions) {
  SgDeclarationStatement *target = nullptr;
  SgInitializedName *using_name = nullptr;
  SgClassDeclaration *class_owner = nullptr;
  SgScopeStatement *enum_scope = nullptr;
  SgDeclarationStatementPtrList candidates;
  if (record.kind == "SgMemberFunctionDeclaration" ||
      record.kind == "SgTemplateMemberFunctionDeclaration" ||
      record.kind == "SgFunctionDeclaration" ||
      record.kind == "SgTemplateFunctionDeclaration") {
    if (uint64_t pattern =
            singleEdgeTarget(record, "template_instantiation_pattern"))
      target = nodeByIdAs<SgFunctionDeclaration>(dependencies, pattern);
  } else if (record.kind == "SgTemplateInstantiationMemberFunctionDecl") {
    if (uint64_t primary = singleEdgeTarget(record, "templateDeclaration"))
      target = nodeByIdAs<SgTemplateMemberFunctionDeclaration>(dependencies,
                                                               primary);
    else
      class_owner = nodeByIdAs<SgTemplateInstantiationDecl>(
          dependencies,
          requiredSingleEdgeTarget(record, "associatedClassDeclaration"));
  } else if (record.kind == "SgTemplateInstantiationFunctionDecl") {
    if (uint64_t primary = singleEdgeTarget(record, "templateDeclaration"))
      target = nodeByIdAs<SgTemplateFunctionDeclaration>(dependencies, primary);
    for (const EdgeRecord &edge :
         edgesFor(record, "dependentTemplateCandidates"))
      candidates.push_back(
          nodeByIdAs<SgTemplateFunctionDeclaration>(dependencies, edge.target));
  } else if (record.kind == "SgNonrealDecl") {
    if (uint64_t primary = singleEdgeTarget(record, "templateDeclaration"))
      target = nodeByIdAs<SgDeclarationStatement>(dependencies, primary);
  } else if (record.kind == "SgUsingDeclarationStatement") {
    (void)record.properties.requiredName("source_terminal_name");
    (void)record.properties.requiredBool("is_inheriting_constructor");
    if (uint64_t declaration =
            exactSerializedPointerTarget(record, "declaration", "declaration"))
      target = nodeByIdAs<SgDeclarationStatement>(dependencies, declaration);
    if (uint64_t name = exactSerializedPointerTarget(record, "initialized_name",
                                                     "initializedName"))
      using_name = nodeByIdAs<SgInitializedName>(dependencies, name);
  } else if (record.kind == "SgEnumDeclaration") {
    enum_scope = nodeByIdAs<SgScopeStatement>(
        dependencies, requiredSingleEdgeTarget(record, "scope"));
  }
  return createNodeFromRecordWithDependencies(
      record, std::move(positions), project, nullptr, nullptr, class_owner,
      nullptr, nullptr, nullptr, target, using_name, enum_scope, nullptr,
      &candidates, dependencies);
}

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const NodeMap &dependencies) {
  return createNodeFromRecordWithMappedDependencies(record, project,
                                                    dependencies, std::nullopt);
}

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const NodeMap &dependencies,
                             SourcePositions positions) {
  return createNodeFromRecordWithMappedDependencies(
      record, project, dependencies, std::move(positions));
}

SgNode *createUseStatementFromRecord(const NodeRecord &record,
                                     SgProject *project,
                                     SgModuleStatement *module) {
  if (record.kind != "SgUseStatement")
    throw std::runtime_error("AST JSON expected a USE statement record");
  return createNodeFromRecordWithDependencies(
      record, std::nullopt, project, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, module);
}

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             SgFunctionDeclaration *declaration,
                             SgBasicBlock *body) {
  return createNodeFromRecordWithDependencies(
      record, std::nullopt, project, nullptr, nullptr, nullptr, declaration,
      body, nullptr, nullptr, nullptr, nullptr);
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

TemplateParameterOperands
templateParameterOperandsFromRecord(const NodeRecord &record,
                                    const NodeMap &nodes,
                                    const SgTemplateParameter *owner) {
  const uint64_t expression_id =
      record.properties.at("default_expression_parameter")
          .requiredNonnegativeNodeId("node");
  if (singleEdgeTarget(record, "defaultExpressionParameter") != expression_id)
    throw std::runtime_error("AST JSON template parameter has conflicting "
                             "default_expression_parameter");
  const uint64_t argument_id =
      record.properties.requiredNonnegativeNodeId("default_template_argument");
  if (singleEdgeTarget(record, "defaultTemplateArgument") != argument_id)
    throw std::runtime_error("AST JSON template parameter has conflicting "
                             "default_template_argument");
  const uint64_t source_id = record.properties.requiredNonnegativeNodeId(
      "source_spelled_template_declaration");
  if (singleEdgeTarget(record, "sourceSpelledTemplateDeclaration") != source_id)
    throw std::runtime_error("AST JSON template parameter has conflicting "
                             "source_spelled_template_declaration");
  SgExpression *expression =
      expression_id != 0 ? nodeByIdAs<SgExpression>(nodes, expression_id)
                         : nullptr;
  SgTemplateArgument *argument =
      argument_id != 0 ? nodeByIdAs<SgTemplateArgument>(nodes, argument_id)
                       : nullptr;
  SgTemplateDeclaration *source =
      source_id != 0 ? nodeByIdAs<SgTemplateDeclaration>(nodes, source_id)
                     : nullptr;
  if (expression != nullptr && expression->get_parent() != nullptr &&
      expression->get_parent() != owner)
    throw std::runtime_error("AST JSON template parameter has a foreign "
                             "default_expression_parameter");
  if (argument != nullptr) {
    if (argument->get_argumentType() !=
            SgTemplateArgument::template_template_argument ||
        argument->get_templateDeclaration() == nullptr)
      throw std::runtime_error(
          "AST JSON template default has no exact template argument");
    if (argument->get_parent() != nullptr && argument->get_parent() != owner)
      throw std::runtime_error("AST JSON template default has a foreign owner");
  }
  if (owner != nullptr &&
      (owner->get_defaultExpressionParameter() != expression ||
       owner->get_defaultTemplateArgument() != argument ||
       (expression != nullptr && expression->get_parent() != owner) ||
       (argument != nullptr && argument->get_parent() != owner)))
    throw std::runtime_error(
        "AST JSON template parameter defaults differ from construction");
  if (source != nullptr) {
    const uint64_t template_id =
        record.properties.requiredNonnegativeNodeId("template_declaration");
    if (template_id == 0 || source == nodeById(nodes, template_id) ||
        source->get_scope() == nullptr)
      throw std::runtime_error(
          "AST JSON template parameter has a malformed source-spelled "
          "template declaration");
    if (source->get_parent() != nullptr && source->get_parent() != owner)
      throw std::runtime_error(
          "AST JSON source template declaration has a foreign owner");
  }
  if (owner != nullptr &&
      (owner->get_sourceSpelledTemplateDeclaration() != source ||
       (source != nullptr && source->get_parent() != owner)))
    throw std::runtime_error(
        "AST JSON source template declaration differs from construction");
  return {expression, argument, source};
}

uint64_t exactSerializedPointerTarget(const NodeRecord &record,
                                      const std::string &property,
                                      const std::string &edge) {
  const int64_t raw_id = record.properties.requiredInt(property);
  if (raw_id < 0 ||
      singleEdgeTarget(record, edge) != static_cast<uint64_t>(raw_id)) {
    throw std::runtime_error("AST JSON node " + std::to_string(record.id) +
                             " has conflicting pointer identity '" + property +
                             "'");
  }
  return static_cast<uint64_t>(raw_id);
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
                              const std::string &field_name,
                              std::size_t expected_size) {
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + field_name +
                             " field is not a bool array");
  }
  if (json.array.size() != expected_size) {
    throw std::runtime_error("AST JSON " + field_name +
                             " has an invalid modifier vector size");
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

std::function<SgModuleStatement *()>
decodeExternalModule(const JsonValue &json) {
  if (!json.requiredBool("present")) {
    return []() -> SgModuleStatement * { return nullptr; };
  }
  if (requiredTranslationUnitSourceOrder(json, "external_module").has_value()) {
    throw std::runtime_error(
        "AST JSON external_module must be semantic-only and unordered");
  }
  const std::string name = json.requiredString("name");
  if (name.empty() || name.find('\0') != std::string::npos) {
    throw std::runtime_error(
        "AST JSON external_module requires a valid module name");
  }
  const std::string source_file = json.requiredString("source_file");
  if (source_file.empty() || source_file.find('\0') != std::string::npos)
    throw std::runtime_error(
        "AST JSON external_module requires a valid source_file");
  const auto class_type = requiredEnum<SgClassDeclaration::class_types>(
      json, "class_type", "external_module",
      {SgClassDeclaration::e_fortran_module});
  return [name, class_type, source_file] {
    auto *module =
        new SgModuleStatement(SgName(name), class_type, nullptr, nullptr);
    installTransformationSourcePosition(module);
    markAstJsonExternalModule(module, source_file);
    return module;
  };
}

SgModuleStatement *externalModuleFromJson(const JsonValue &json) {
  return decodeExternalModule(json)();
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
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
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
                                     const JsonValue &json,
                                     const SourcePositionFields &positions) {
  Sg_File_Info *candidate_start =
      candidate != nullptr ? candidate->get_startOfConstruct() : nullptr;
  Sg_File_Info *candidate_end =
      candidate != nullptr ? candidate->get_endOfConstruct() : nullptr;
  if (candidate_start == nullptr || candidate_end == nullptr ||
      !positions.start || !positions.end) {
    return false;
  }
  return sourceFileMatchesExternalRecord(
             SageInterface::getEnclosingSourceFile(candidate, true),
             json.requiredString("source_file")) &&
         sameAstJsonPath(
             candidate_start->get_filenameString(),
             Sg_File_Info::getFilenameFromID(positions.start->file_id)) &&
         candidate_start->get_line() == positions.start->line &&
         candidate_start->get_col() == positions.start->column &&
         sameAstJsonPath(
             candidate_end->get_filenameString(),
             Sg_File_Info::getFilenameFromID(positions.end->file_id)) &&
         candidate_end->get_line() == positions.end->line &&
         candidate_end->get_col() == positions.end->column;
}

bool externalFunctionCandidateMatches(SgFunctionDeclaration *candidate,
                                      SgFunctionType *function_type,
                                      const JsonValue &json,
                                      const NodeMap &nodes,
                                      VariantT expected_variant,
                                      const SourcePositionFields &positions) {
  if (candidate == nullptr || function_type == nullptr ||
      candidate->get_type() == nullptr ||
      candidate->variantT() != expected_variant ||
      candidate->get_name().getString() != json.requiredString("name") ||
      !externalFunctionLocationMatches(candidate, json, positions) ||
      !SageInterface::isEquivalentFunctionType(candidate->get_type(),
                                               function_type)) {
    return false;
  }
  SgFunctionParameterList *parameters = candidate->get_parameterList();
  SgFunctionParameterList *syntax = candidate->get_parameterList_syntax();
  const bool distinct_syntax =
      json.requiredBool("parameter_list_syntax_is_distinct");
  std::unordered_map<const SgNode *, uint64_t> ids;
  for (const auto &entry : nodes)
    ids.emplace(entry.second, entry.first);
  auto parameter_bindings_match = [&](SgFunctionParameterList *list,
                                      const JsonValue &serialized) {
    if (list == nullptr || list->get_parent() != candidate ||
        serialized.kind != JsonValue::Kind::Array ||
        list->get_args().size() != serialized.array.size())
      return false;
    for (size_t i = 0; i < serialized.array.size(); ++i) {
      SgInitializedName *parameter = list->get_args()[i];
      const JsonValue &record = serialized.array[i];
      if (parameter == nullptr || parameter->get_parent() != list ||
          parameter->get_declptr() != candidate ||
          parameter->get_fortran_source_derived_type_symbol() !=
              exactBoundSymbolFromJson(
                  record.at("fortran_source_derived_type_symbol"), nodes))
        return false;
    }
    return true;
  };
  if (!parameter_bindings_match(parameters, json.at("parameters")) ||
      (distinct_syntax &&
       !parameter_bindings_match(syntax, json.at("syntax_parameters")))) {
    return false;
  }
  const JsonValue native =
      parseJson(rawExternalFunctionJson(candidate, ids, true));
  return native.exactlyEquals(json);
}

SgFunctionDeclaration *loadedExternalFunctionFromJson(
    const JsonValue &json, SgFunctionType *function_type, const NodeMap &nodes,
    VariantT expected_variant, const SourcePositionFields &positions) {
  const std::string source_file = json.requiredString("source_file");
  ROSE_ASSERT(positions.start.has_value());
  const bool names_loaded_source =
      sameAstJsonPath(Sg_File_Info::getFilenameFromID(positions.start->file_id),
                      source_file) ||
      sameAstJsonPath(
          Sg_File_Info::getFilenameFromID(positions.start->physical_file_id),
          source_file);
  if (!names_loaded_source) {
    return nullptr;
  }
  bool saw_source_file = false;
  SgFunctionDeclaration *matched = nullptr;
  for (SgSourceFile *source : currentDeserializationSourceFiles()) {
    if (!sourceFileMatchesExternalRecord(source, source_file)) {
      continue;
    }
    saw_source_file = true;
    for (SgFunctionDeclaration *candidate :
         externalFunctionDeclarationsForSource(source)) {
      if (!externalFunctionCandidateMatches(candidate, function_type, json,
                                            nodes, expected_variant,
                                            positions)) {
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
        json.requiredString("name"));
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

std::function<SgFunctionDeclaration *()>
decodeExternalFunction(const JsonValue &json, const NodeMap &nodes) {
  if (!json.requiredBool("present")) {
    return []() -> SgFunctionDeclaration * { return nullptr; };
  }
  if (requiredTranslationUnitSourceOrder(json, "external_function")
          .has_value()) {
    throw std::runtime_error(
        "AST JSON external_function must be semantic-only and unordered");
  }
  const std::string name = json.at("name").asString();
  if (name.find('\0') != std::string::npos)
    throw std::runtime_error("AST JSON external_function has an invalid name");
  const std::string source_file = json.at("source_file").asString();
  if (source_file.empty() || source_file.find('\0') != std::string::npos) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " requires a valid source_file");
  }
  const std::string kind = json.requiredString("kind");
  VariantT expected_variant;
  if (kind == "SgProcedureHeaderStatement")
    expected_variant = V_SgProcedureHeaderStatement;
  else if (kind == "SgFunctionDeclaration")
    expected_variant = V_SgFunctionDeclaration;
  else
    throw std::runtime_error(
        "AST JSON external_function has unsupported declaration kind: " + kind);
  if (expected_variant == V_SgFunctionDeclaration && name.empty())
    throw std::runtime_error("AST JSON external_function is missing name");
  const auto constexpr_specifier =
      functionConstexprSpecifierFromJson(json, kind);
  std::optional<SgProcedureHeaderStatement::subprogram_kind_enum>
      subprogram_kind;
  std::optional<SgProcedureHeaderStatement::block_data_name_kind_enum>
      block_data_name_kind;
  std::optional<SgProcedureHeaderStatement::fortran_procedure_source_form_enum>
      procedure_source_form;
  std::optional<SgProcedureHeaderStatement::fortran_result_type_spec_enum>
      result_type_spec;
  SgName binding_name;
  if (expected_variant == V_SgProcedureHeaderStatement) {
    subprogram_kind = subprogramKindFromJson(json, "external function");
    block_data_name_kind = blockDataNameKindFromJson(json, "external function");
    procedure_source_form =
        procedureSourceFormFromJson(json, "external function");
    binding_name = json.requiredName("fortran_binding_name");
    result_type_spec =
        procedureResultTypeSpecFromJson(json, "external function");
  }
  const SgName anonymous_symbol_key =
      json.requiredName("fortran_anonymous_program_unit_symbol_key");
  const bool requires_anonymous_symbol_key =
      expected_variant == V_SgProcedureHeaderStatement &&
      *subprogram_kind ==
          SgProcedureHeaderStatement::e_block_data_subprogram_kind &&
      *block_data_name_kind == SgProcedureHeaderStatement::e_unnamed_block_data;
  if (requires_anonymous_symbol_key != !anonymous_symbol_key.is_null())
    throw std::runtime_error(
        "AST JSON external function anonymous program-unit symbol key "
        "disagrees with its typed source identity");
  const SgName omp_declare_variant_source_name =
      json.requiredName("omp_declare_variant_source_name");
  const auto omp_declare_variant_region_ordinal =
      requiredOmpDeclareVariantRegionOrdinal(json, name,
                                             "external function " + name);
  const bool name_parenthesized =
      json.requiredBool("source_name_parenthesized_for_macro");
  const bool wrapped_function_type =
      json.requiredBool("source_declarator_uses_wrapped_function_type");
  const bool is_explicit = json.requiredBool("is_explicit");
  const bool is_deduction_guide = json.requiredBool("is_deduction_guide");
  const auto &explicit_condition_json =
      json.at("explicit_specifier_expression");
  if ((is_explicit || explicit_condition_json.requiredInt("node") != 0 ||
       explicit_condition_json.find("owned_kind") != nullptr) &&
      (!is_deduction_guide || expected_variant != V_SgFunctionDeclaration))
    throw std::runtime_error(
        "AST JSON external explicit specifier has no deduction guide owner");
  const bool syntax_aliases_parameters =
      json.requiredBool("parameter_list_syntax_aliases_parameter_list");
  const bool distinct_syntax =
      json.requiredBool("parameter_list_syntax_is_distinct");
  const JsonValue &declarator_scope_json = json.at("function_declarator_scope");
  const bool declarator_scope_present =
      declarator_scope_json.requiredBool("present");
  if (declarator_scope_present !=
          (declarator_scope_json.find("location") != nullptr) ||
      declarator_scope_present !=
          (declarator_scope_json.find("flags") != nullptr))
    throw std::runtime_error("AST JSON external_function " + name +
                             " has incomplete typed declarator-scope state");
  NodeRecord scope_record;
  std::optional<NodeConstructionFlags> scope_flags;
  if (declarator_scope_present) {
    scope_record.flags = declarator_scope_json.at("flags");
    scope_flags = validateConstructionFlags(scope_record);
  }
  const JsonValue &function_parameter_scope =
      json.at("function_parameter_scope");
  const bool parameter_scope_present =
      function_parameter_scope.requiredBool("present");
  const JsonValue *syntax_location =
      json.find("parameter_list_syntax_location");
  const JsonValue *syntax_parameters = json.find("syntax_parameters");
  const JsonValue &parameters = json.at("parameters");
  if (parameters.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " parameters is not an array");
  }
  if ((syntax_aliases_parameters && distinct_syntax) ||
      (distinct_syntax && !declarator_scope_present) ||
      distinct_syntax != (syntax_location != nullptr) ||
      distinct_syntax != (syntax_parameters != nullptr) ||
      (syntax_parameters != nullptr &&
       syntax_parameters->kind != JsonValue::Kind::Array)) {
    throw std::runtime_error("AST JSON external_function " + name +
                             " has inconsistent source parameter-list state");
  }
  if (distinct_syntax &&
      syntax_parameters->array.size() != parameters.array.size())
    throw std::runtime_error("AST JSON external_function " + name +
                             " has mismatched semantic and source "
                             "parameter counts");
  std::vector<ExternalInitializedNameFields> parameter_fields;
  parameter_fields.reserve(parameters.array.size());
  for (const JsonValue &parameter : parameters.array)
    parameter_fields.push_back(externalInitializedNameFieldsFromJson(
        parameter, nodes, "external_function " + name));
  std::vector<ExternalInitializedNameFields> syntax_parameter_fields;
  if (distinct_syntax)
    for (const JsonValue &parameter : syntax_parameters->array)
      syntax_parameter_fields.push_back(externalInitializedNameFieldsFromJson(
          parameter, nodes, "external_function " + name));
  const JsonValue &function_type_syntax_json = json.at("function_type_syntax");
  const bool type_syntax_is_available =
      json.requiredBool("type_syntax_is_available");
  if (type_syntax_is_available !=
      function_type_syntax_json.requiredBool("present"))
    throw std::runtime_error("AST JSON external_function " + name +
                             " has inconsistent function type syntax state");
  const auto position_fields = requiredSourcePositionFieldsFromJson(
      json.at("location"), "external_function " + name);
  const auto parameter_position_fields = requiredSourcePositionFieldsFromJson(
      json.at("parameter_list_location"),
      "external_function parameterList " + name);
  std::optional<SourcePositionFields> syntax_position_fields;
  if (distinct_syntax)
    syntax_position_fields = requiredSourcePositionFieldsFromJson(
        *syntax_location, "external_function parameterList_syntax " + name);
  std::optional<SourcePositionFields> declarator_scope_position_fields;
  if (declarator_scope_present)
    declarator_scope_position_fields = requiredSourcePositionFieldsFromJson(
        declarator_scope_json.at("location"),
        "external_function declarator scope " + name);
  std::optional<SourcePositionFields> parameter_scope_position_fields;
  if (parameter_scope_present)
    parameter_scope_position_fields = requiredSourcePositionFieldsFromJson(
        function_parameter_scope.at("location"),
        "external_function functionParameterScope " + name);
  return [=, &json, &nodes, &parameters, &function_parameter_scope,
          &explicit_condition_json, &declarator_scope_json,
          &function_type_syntax_json]() -> SgFunctionDeclaration * {
    if (!externalFunctionDeserializationIdentityActive) {
      throw std::runtime_error(
          "AST JSON external function was reconstructed outside its exact "
          "identity transaction");
    }
    const auto *identity = externalFunctionIdentity(json);
    if (identity != nullptr && identity->declaration != nullptr) {
      if (!identity->parameter_scope_symbols_pending) {
        std::unordered_map<const SgNode *, uint64_t> ids;
        for (const auto &entry : nodes)
          ids.emplace(entry.second, entry.first);
        const JsonValue native = parseJson(
            rawExternalFunctionJson(identity->declaration, ids, true));
        if (!native.exactlyEquals(json))
          throw std::runtime_error(
              "AST JSON cached external function differs from its completed "
              "record: " +
              name);
      }
      return identity->declaration;
    }
    const auto construction_context =
        identity != nullptr ? identity->context : std::nullopt;
    SgScopeStatement *construction_scope = nullptr;
    SgNode *construction_parent = nullptr;
    if (construction_context) {
      construction_scope =
          nodeByIdAs<SgScopeStatement>(nodes, construction_context->scope_id);
      construction_parent =
          construction_context->parent_id
              ? nodeById(nodes, *construction_context->parent_id)
              : currentDeserializationProject;
      if (construction_parent == nullptr)
        throw std::runtime_error(
            "AST JSON external function has no recorded construction parent");
    }
    auto publish_identity = [&](SgFunctionDeclaration *declaration,
                                bool parameter_scope_symbols_pending) {
      auto *entry = externalFunctionIdentity(json);
      if (entry != nullptr) {
        ROSE_ASSERT(entry->declaration == nullptr && entry->context);
        entry->declaration = declaration;
        entry->parameter_scope_symbols_pending =
            parameter_scope_symbols_pending;
      } else {
        externalFunctionDeserializationIdentities[exactJsonValueHash(json)]
            .push_back({json, declaration, std::nullopt,
                        parameter_scope_symbols_pending});
      }
    };
    SgFunctionType *function_type =
        semanticFunctionTypeFromJson(json.at("function_type"), nodes);
    if (function_type == nullptr) {
      throw std::runtime_error(
          "AST JSON external_function function_type is not a SgFunctionType");
    }
    if (SgFunctionDeclaration *loaded = loadedExternalFunctionFromJson(
            json, function_type, nodes, expected_variant, position_fields)) {
      if (construction_context && (loaded->get_scope() != construction_scope ||
                                   loaded->get_parent() != construction_parent))
        throw std::runtime_error(
            "AST JSON external function would move a loaded declaration");
      publish_identity(loaded, false);
      return loaded;
    }
    auto positions = constructSourcePositions(position_fields);
    auto parameter_positions =
        constructSourcePositions(parameter_position_fields);
    std::optional<SourcePositions> syntax_positions;
    if (distinct_syntax)
      syntax_positions = constructSourcePositions(*syntax_position_fields);
    std::optional<SourcePositions> declarator_scope_positions;
    if (declarator_scope_present)
      declarator_scope_positions =
          constructSourcePositions(*declarator_scope_position_fields);
    std::optional<SourcePositions> parameter_scope_positions;
    if (parameter_scope_present)
      parameter_scope_positions =
          constructSourcePositions(*parameter_scope_position_fields);
    auto discard_ast = &SourceTypeConstruction::discardAst;
    SgFunctionType *function_type_syntax =
        nullableFunctionSyntaxTypeFromJson(function_type_syntax_json, nodes);
    if (function_type_syntax != nullptr &&
        (function_type_syntax == function_type ||
         function_type_syntax->get_parent() != nullptr))
      throw std::runtime_error("AST JSON external_function " + name +
                               " does not own one distinct source function "
                               "type syntax node");
    std::unique_ptr<SgFunctionType, decltype(discard_ast)> syntax_owner(
        function_type_syntax, discard_ast);
    InlineExpressionConstruction explicit_ownership;
    auto *explicit_condition =
        expressionFromRef(explicit_condition_json, nodes, explicit_ownership);
    if (explicit_condition != nullptr &&
        explicit_condition->get_parent() != nullptr)
      throw std::runtime_error(
          "AST JSON external explicit condition is already owned");

    std::unique_ptr<SgFunctionType, decltype(discard_ast)> exception_signature(
        nullableDeclaredExceptionSignatureFromJson(
            json.at("declared_exception_signature"), nodes),
        discard_ast);
    std::unique_ptr<SgFunctionDeclaration, decltype(discard_ast)>
        function_owner(nullptr, discard_ast);
    SgFunctionDeclaration *decl = nullptr;
    if (expected_variant == V_SgProcedureHeaderStatement) {
      SgProcedureHeaderStatement *procedure = new SgProcedureHeaderStatement(
          SgName(name), function_type, nullptr, exception_signature.get());
      function_owner.reset(procedure);
      exception_signature.release();
      procedure->set_subprogram_kind(*subprogram_kind);
      procedure->set_block_data_name_kind(*block_data_name_kind);
      procedure->set_fortran_procedure_source_form(*procedure_source_form);
      procedure->set_fortran_binding_name(binding_name);
      procedure->set_fortran_result_type_spec(*result_type_spec);
      SageInterface::isFortranProgramUnitWithoutSourceName(procedure);
      decl = procedure;
    } else {
      decl = new SgFunctionDeclaration(SgName(name), function_type, nullptr,
                                       exception_signature.get());
      function_owner.reset(decl);
      exception_signature.release();
    }
    installSourcePositions(decl, std::move(positions),
                           "external_function " + name);
    decl->set_is_deduction_guide(is_deduction_guide);
    if (is_explicit)
      decl->get_functionModifier().setExplicit();
    decl->set_explicit_specifier_expression(explicit_condition);
    if (explicit_condition != nullptr)
      explicit_condition->set_parent(decl);
    explicit_ownership.publish();
    decl->validate_explicit_specifier();
    SgFunctionParameterList *parameter_list = decl->get_parameterList();
    if (parameter_list == nullptr) {
      throw std::runtime_error("AST JSON external_function " + name +
                               " constructor did not create a parameterList");
    }
    installSourcePositions(parameter_list, std::move(parameter_positions),
                           "external_function parameterList " + name);
    decl->set_constexpr_specifier(constexpr_specifier);
    decl->set_firstNondefiningDeclaration(decl);
    decl->set_definingDeclaration(nullptr);
    if (construction_context) {
      decl->set_scope(construction_scope);
      decl->set_parent(construction_parent);
    }
    constructExternalFunctionOwnedAttributes(decl, json, nodes);
    if (!anonymous_symbol_key.is_null()) {
      decl->initialize_fortran_anonymous_program_unit_symbol_key(
          anonymous_symbol_key);
    }
    decl->set_omp_declare_variant_source_name(omp_declare_variant_source_name);
    decl->set_omp_declare_variant_region_ordinal(
        omp_declare_variant_region_ordinal);
    decl->set_source_name_parenthesized_for_macro(name_parenthesized);
    decl->set_source_declarator_uses_wrapped_function_type(
        wrapped_function_type);
    decl->set_type_syntax(function_type_syntax);
    decl->set_type_syntax_is_available(type_syntax_is_available);
    if (function_type_syntax != nullptr)
      function_type_syntax->set_parent(decl);
    syntax_owner.release();

    if (declarator_scope_present) {
      auto scope = std::make_unique<SgDeclarationScope>();
      installSourcePositions(scope.get(),
                             std::move(*declarator_scope_positions),
                             "external_function declarator scope " + name);
      scope_flags.value().initialize(scope.get());
      SageBuilder::adoptFunctionDeclaratorScope(decl, scope.get());
      scope.release();
    }
    if (parameter_scope_present) {
      auto scope = std::make_unique<SgFunctionParameterScope>();
      scope->set_parent(decl);
      decl->set_functionParameterScope(scope.release());
    }
    for (size_t index = 0; index < parameters.array.size(); ++index) {
      SgInitializedName *parameter = constructExternalInitializedName(
          parameters.array[index], nodes, parameter_fields[index], decl,
          parameter_list, decl->get_functionParameterScope(),
          "external_function " + name);
      parameter_list->append_arg(parameter);
    }
    if (syntax_aliases_parameters) {
      decl->set_parameterList_syntax(parameter_list);
    } else if (distinct_syntax) {
      auto *source_declarations = new SgFunctionParameterList();
      decl->set_parameterList_syntax(source_declarations);
      source_declarations->set_parent(decl);
      installSourcePositions(source_declarations, std::move(*syntax_positions),
                             "external_function parameterList_syntax " + name);
      for (size_t index = 0; index < syntax_parameters->array.size(); ++index) {
        SgInitializedName *parameter = constructExternalInitializedName(
            syntax_parameters->array[index], nodes,
            syntax_parameter_fields[index], decl, source_declarations,
            decl->get_function_declarator_scope(),
            "external_function syntax " + name);
        source_declarations->append_arg(parameter);
      }
    }
    constructExternalFunctionParameterScopeDeclarationsFromJson(
        function_parameter_scope, nodes, decl->get_functionParameterScope(),
        parameter_list->get_args(), name, std::move(parameter_scope_positions));
    if (decl->get_source_declarator_uses_wrapped_function_type()) {
      decl->validate_source_declarator_form();
    }
    if (construction_context) {
      markAstJsonExternalFunction(decl, source_file);
      publish_identity(decl, parameter_scope_present);
      return function_owner.release();
    }
    std::unique_ptr<SgGlobal, decltype(discard_ast)> global_owner(
        new SgGlobal(), discard_ast);
    SgGlobal *external_global = global_owner.get();
    installTransformationSourcePosition(external_global);
    decl->set_scope(external_global);
    SageBuilder::attachAuxiliaryDeclaration(external_global, decl);
    function_owner.release();
    markAstJsonExternalFunction(decl, source_file);
    const SgName symbol_key =
        isSgProcedureHeaderStatement(decl) != nullptr
            ? SageInterface::getFortranProgramUnitSymbolTableKey(decl)
            : decl->get_name();
    SgFunctionSymbol *canonical_symbol = new SgFunctionSymbol(decl);
    // This global is an isolated, non-output semantic root, not a project file.
    // Publish into its exact table without the project-wide alias side effect
    // of SgScopeStatement::insert_symbol.
    external_global->get_symbol_table()->insert(symbol_key, canonical_symbol);
    if (external_global->find_symbol_from_declaration(decl) !=
            canonical_symbol ||
        canonical_symbol->get_parent() != external_global->get_symbol_table() ||
        !external_global->get_symbol_table()->exists(canonical_symbol)) {
      throw std::runtime_error("AST JSON external function " + name +
                               " has no exact canonical symbol publication");
    }
    publish_identity(decl, parameter_scope_present);
    global_owner.release();
    return decl;
  };
}

SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes) {
  return decodeExternalFunction(json, nodes)();
}

SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue &json, const NodeMap &nodes, const std::string &context) {
  if (!json.requiredBool("present")) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a present external declaration");
  }
  const std::string kind = json.at("kind").asString();
  const JsonValue *external_function = json.find("external_function");
  const JsonValue *external_module = json.find("external_module");
  const JsonValue *external_class = json.find("external_class");
  const int identity_count = (external_function != nullptr) +
                             (external_module != nullptr) +
                             (external_class != nullptr);
  if (identity_count != 1) {
    throw std::runtime_error("AST JSON " + context +
                             " requires exactly one external declaration");
  }
  if (external_function != nullptr) {
    if (!external_function->requiredBool("present") ||
        kind != external_function->requiredString("kind")) {
      throw std::runtime_error("AST JSON " + context +
                               " kind disagrees with external_function");
    }
    return externalFunctionFromJson(*external_function, nodes);
  }
  if (external_module != nullptr) {
    if (!external_module->requiredBool("present") ||
        kind != "SgModuleStatement") {
      throw std::runtime_error("AST JSON " + context +
                               " kind disagrees with external_module");
    }
    return externalModuleFromJson(*external_module);
  }
  if (!external_class->requiredBool("present") ||
      kind != external_class->requiredString("kind")) {
    throw std::runtime_error("AST JSON " + context +
                             " kind disagrees with external_class");
  }
  return externalClassDeclarationFromJson(*external_class);
}

template <typename ListT, typename NodeT>
void appendEdgeList(ListT &list, const NodeRecord &record,
                    const std::string &field, const NodeMap &nodes,
                    SgNode *parent, bool owns_children = true,
                    bool require_constructed_list = false) {
  ListT serialized;
  auto constructed = list.begin();
  for (const EdgeRecord &edge : edgesFor(record, field)) {
    NodeT *child = nodeByIdAs<NodeT>(nodes, edge.target);
    serialized.push_back(child);
    if (owns_children && child->get_parent() != parent) {
      throw std::runtime_error("AST JSON " + record.kind + "." + field +
                               " does not match its recorded child owner");
    }
    if (constructed != list.end() && *constructed == child)
      ++constructed;
  }
  if (constructed != list.end() ||
      (require_constructed_list && list != serialized)) {
    throw std::runtime_error("AST JSON " + record.kind + "." + field +
                             " conflicts with its construction-time list");
  }
  if (require_constructed_list)
    return;
  list = std::move(serialized);
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
  if (statement->get_clause_list() != serialized_list ||
      serialized_list->get_parent() != statement) {
    throw std::runtime_error(
        "AST JSON OpenMP clause-list parent edge does not identify its "
        "owning statement");
  }
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
  if (statement->get_variable_list() != serialized_list ||
      serialized_list->get_parent() != statement) {
    throw std::runtime_error(
        "AST JSON OpenMP variable-list parent edge does not identify its "
        "owning statement");
  }
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

  for (SgToken *token : file->get_token_list()) {
    if (token == nullptr || token->get_startOfConstruct() == nullptr ||
        token->get_endOfConstruct() == nullptr) {
      throw std::runtime_error(
          "AST JSON token stream contains a token without source extent");
    }
  }
  auto attributes = std::make_unique<ROSEAttributesList>();
  attributes->setFileName(filename);
  attributes->set_rawTokenStream(new LexTokenStreamType());
  auto *stream = attributes->get_rawTokenStream();
  for (SgToken *token : file->get_token_list()) {
    std::unique_ptr<stream_element> element(new stream_element());
    auto token_data = std::make_unique<token_element>();
    token_data->token_lexeme = token->get_lexeme_string();
    token_data->token_id = static_cast<int>(token->get_classification_code());
    element->p_tok_elem = token_data.get();
    element->p_preprocessingInfo = nullptr;
    element->beginning_fpi.line_num = token->get_startOfConstruct()->get_line();
    element->beginning_fpi.column_num =
        token->get_startOfConstruct()->get_col();
    element->ending_fpi.line_num = token->get_endOfConstruct()->get_line();
    element->ending_fpi.column_num = token->get_endOfConstruct()->get_col();
    stream->push_back(element.get());
    token_data.release();
    element.release();
  }

  std::unique_ptr<ROSEAttributesListContainer> container(
      new ROSEAttributesListContainer());
  container->addList(filename, attributes.get());
  attributes.release();
  file->set_preprocessorDirectivesAndCommentsList(container.release());
}

void NodeConstructionFlags::initialize(SgNode *node) const {
  ASSERT_not_null(node);
  node->set_containsTransformation(contains_transformation);
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_containsTransformationToSurroundingWhitespace(
        contains_transformation_to_surrounding_whitespace);
    located->set_source_range_ends_in_macro_expansion(
        source_range_ends_in_macro_expansion);
    located->set_source_range_is_macro_expansion_fragment(
        source_range_is_macro_expansion_fragment);
  }
  node->set_isModified(modified);
}

void restoreAvailableConstructionScopes(const AstFileRecord &ast,
                                        const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
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
      block->set_implied_do_semantic_scope(implied_semantic_scope);
    }
  }

  restoreAvailableParentsAndScopes(ast, nodes);
}

void restoreAvailableParentsAndScopes(const AstFileRecord &ast,
                                      const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    if (found == nodes.end()) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      auto parent = nodes.find(target);
      if (parent != nodes.end()) {
        if (auto *scope = isSgDeclarationScope(found->second)) {
          const NodeRecord &owner = ast.node(target);
          if (singleEdgeTarget(owner, "nonreal_decl_scope") == record.id) {
            SageBuilder::setNonrealDeclarationScope(
                isSgDeclarationStatement(parent->second), scope);
            continue;
          }
          if (singleEdgeTarget(owner, "function_declarator_scope") ==
              record.id) {
            SageBuilder::adoptFunctionDeclaratorScope(
                isSgFunctionDeclaration(parent->second), scope);
            continue;
          }
          if (singleEdgeTarget(owner, "closure_scope") == record.id)
            continue;
        }
        if (isSgOmpClause(found->second) != nullptr &&
            isSgOmpClauseList(parent->second) != nullptr)
          continue;
        if (isSgInitializedName(found->second) != nullptr &&
            isSgCtorInitializerList(parent->second) != nullptr)
          continue;
        if (isSgInitializedName(found->second) != nullptr &&
            isSgFunctionParameterList(parent->second) != nullptr &&
            ast.node(requiredSingleEdgeTarget(ast.node(target), "parent"))
                    .kind == "SgRequiresExpr")
          continue;
        if (found->second->get_parent() == nullptr)
          found->second->set_parent(parent->second);
        else if (found->second->get_parent() != parent->second)
          throw std::runtime_error(
              "AST JSON " + record.kind + " id=" + std::to_string(record.id) +
              " has a conflicting early typed parent: expected " +
              parent->second->class_name() + ", actual " +
              found->second->get_parent()->class_name());
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    auto found = nodes.find(record.id);
    const uint64_t scope_id = singleEdgeTarget(record, "scope");
    auto scope = nodes.find(scope_id);
    if (found == nodes.end() || scope_id == 0 || scope == nodes.end()) {
      continue;
    }
    auto *exact_scope = isSgScopeStatement(scope->second);
    if (exact_scope == nullptr)
      throw std::runtime_error(
          "AST JSON dependent names a non-scope semantic owner");
    if (auto *name = isSgInitializedName(found->second)) {
      if (name->get_scope() != nullptr && name->get_scope() != exact_scope)
        throw std::runtime_error(
            "AST JSON initialized name has a conflicting semantic scope");
      if (name->get_scope() == nullptr)
        name->set_scope(exact_scope);
    }
    if (auto *declaration = isSgDeclarationStatement(found->second);
        declaration != nullptr && declaration->hasExplicitScope()) {
      if ((isSgFunctionDeclaration(declaration) != nullptr ||
           declaration->get_scope() != nullptr) &&
          declaration->get_scope() != exact_scope)
        throw std::runtime_error(
            "AST JSON declaration has a conflicting semantic scope");
      if (declaration->get_scope() == nullptr)
        declaration->set_scope(exact_scope);
    }
    if (auto *label = isSgLabelStatement(found->second)) {
      if (label->get_scope() != nullptr && label->get_scope() != exact_scope)
        throw std::runtime_error(
            "AST JSON label has a conflicting semantic scope");
      if (label->get_scope() == nullptr)
        label->set_scope(exact_scope);
    }
  }
}

void restoreAvailableAuxiliaryDeclarationOwnership(const AstFileRecord &ast,
                                                   const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgAuxiliaryDeclarationList" ||
        nodes.count(record.id) == 0)
      continue;
    const uint64_t owner_id = requiredSingleEdgeTarget(record, "parent");
    if (nodes.count(owner_id) == 0)
      continue;
    auto *container = nodeByIdAs<SgAuxiliaryDeclarationList>(nodes, record.id);
    auto *owner = nodeByIdAs<SgScopeStatement>(nodes, owner_id);
    if (requiredSingleEdgeTarget(ast.node(owner_id),
                                 "auxiliary_declarations") != record.id ||
        container->get_parent() != owner ||
        (owner->get_auxiliary_declarations() != nullptr &&
         owner->get_auxiliary_declarations() != container))
      throw std::runtime_error(
          "AST JSON auxiliary declarations have conflicting scope ownership");
    container->validate_semantic_non_output_role();
    SgDeclarationStatementPtrList available;
    for (const EdgeRecord &edge : edgesFor(record, "declarations")) {
      if (requiredSingleEdgeTarget(ast.node(edge.target), "parent") !=
          record.id)
        throw std::runtime_error(
            "AST JSON auxiliary declaration names a different owner");
      if (nodes.count(edge.target) == 0)
        continue;
      auto *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, edge.target);
      if (declaration->get_parent() != container ||
          std::find(available.begin(), available.end(), declaration) !=
              available.end())
        throw std::runtime_error(
            "AST JSON auxiliary declaration has invalid scope or membership");
      if (declaration->hasExplicitScope() && declaration->get_scope() != owner)
        throw std::runtime_error(
            "AST JSON auxiliary declaration has a different semantic scope");
      available.push_back(declaration);
    }
    auto next = available.begin();
    for (SgDeclarationStatement *existing : container->get_declarations()) {
      next = std::find(next, available.end(), existing);
      if (next == available.end())
        throw std::runtime_error(
            "AST JSON auxiliary declarations changed their recorded order");
      ++next;
    }
    owner->set_auxiliary_declarations(container);
    container->get_declarations() = std::move(available);
  }
}

std::vector<PreprocessingInfoFields>
preprocessingInfoFieldsFromJson(const NodeRecord &record) {
  if (record.preprocessing.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON node preprocessing payload is not an array");
  }
  auto prepare = [](const JsonValue &entry) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error("AST JSON preprocessing entry is not an object");
    }
    const auto file_info = fileInfoFieldsFromJson(entry.at("file_info"));
    if (!file_info) {
      throw std::runtime_error(
          "AST JSON preprocessing entry requires present file_info");
    }
    if (file_info->classification & Sg_File_Info::e_shared)
      throw std::runtime_error(
          "AST JSON preprocessing entry has shared file_info");
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
    return PreprocessingInfoFields{
        *file_info,
        directive,
        entry.requiredString("text"),
        entry.requiredIntInRange("lines", -1, std::numeric_limits<int>::max()),
        requiredEnum<PreprocessingInfo::RelativePositionType>(
            entry, "relative", "preprocessing entry",
            {PreprocessingInfo::before, PreprocessingInfo::after,
             PreprocessingInfo::inside, PreprocessingInfo::before_syntax,
             PreprocessingInfo::after_syntax}),
        entry.requiredBool("transformation"),
        requiredEnum<PreprocessingInfo::OutputPlacementType>(
            entry, "output_placement", "preprocessing entry",
            {PreprocessingInfo::source_position,
             PreprocessingInfo::attached_output_boundary,
             PreprocessingInfo::attached_output_trailing_line})};
  };
  std::vector<PreprocessingInfoFields> prepared;
  prepared.reserve(record.preprocessing.array.size());
  for (const JsonValue &entry : record.preprocessing.array)
    prepared.push_back(prepare(entry));
  return prepared;
}

PreparedPreprocessingInfo constructPreprocessingInfo(
    const std::vector<PreprocessingInfoFields> &prepared) {
  PreparedPreprocessingInfo pending;
  pending.reserve(prepared.size());
  for (const auto &fields : prepared) {
    std::unique_ptr<Sg_File_Info> position(
        constructFileInfo(fields.position, nullptr));
    auto info = std::make_unique<PreprocessingInfo>(
        fields.directive, fields.text, position->get_filenameString(),
        position->get_line(), position->get_col(), fields.lines,
        fields.relative);
    std::unique_ptr<Sg_File_Info> constructor_position(info->get_file_info());
    info->set_file_info(position.release());
    if (fields.transformation)
      info->setAsTransformation();
    else
      info->unsetAsTransformation();
    info->setOutputPlacement(fields.placement);
    pending.push_back(std::move(info));
  }
  return pending;
}

PreparedPreprocessingInfo preparePreprocessingInfo(const NodeRecord &record) {
  return constructPreprocessingInfo(preprocessingInfoFieldsFromJson(record));
}

void attachPreprocessingInfo(SgNode *node, PreparedPreprocessingInfo entries) {
  SgLocatedNode *located = isSgLocatedNode(node);
  if (located == nullptr) {
    if (!entries.empty())
      throw std::runtime_error(
          "AST JSON non-located node owns preprocessing entries");
    return;
  }
  if (located->getAttachedPreprocessingInfo() != nullptr)
    located->clearAttachedPreprocessingInfo();
  for (auto &info : entries) {
    located->attachPreprocessingInfo(
        info.get(), info->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::back);
    info.release();
  }
  located->validateAttachedPreprocessingInfoOwnership();
}

void attachPreprocessingInfo(SgNode *node, const NodeRecord &record) {
  attachPreprocessingInfo(node, preparePreprocessingInfo(record));
}

PreparedAstAttributes prepareAstAttributes(const NodeRecord &record) {
  const JsonValue *attributes = record.properties.find("attributes");
  if (attributes == nullptr) {
    throw std::runtime_error("AST JSON node is missing required attributes");
  }
  if (attributes->kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON node attributes are not an array");
  }
  std::unordered_set<std::string> names;
  PreparedAstAttributes pending;
  for (const JsonValue &entry : attributes->array) {
    const std::string name = entry.requiredString("name");
    const std::string type = entry.requiredString("type");
    if (name.empty()) {
      throw std::runtime_error("AST JSON attribute has an empty name");
    }
    if (!names.insert(name).second) {
      throw std::runtime_error("AST JSON attribute name is duplicated: " +
                               name);
    }
    std::unique_ptr<AstAttribute> attribute;
    if (type == "AstIntAttribute") {
      const int64_t value = entry.requiredInt("value");
      if (value < std::numeric_limits<int>::min() ||
          value > std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "AST JSON integer attribute is out of range: " + name);
      }
      attribute = std::make_unique<AstIntAttribute>(static_cast<int>(value));
    } else if (type == "AstStringAttribute") {
      attribute = std::make_unique<AstValueAttribute<std::string>>(
          entry.requiredString("value"));
    } else {
      throw std::runtime_error("AST JSON attribute type is unsupported: " +
                               type);
    }
    pending.emplace_back(name, std::move(attribute));
  }
  return pending;
}

void attachAstAttributes(SgNode *node, PreparedAstAttributes attributes) {
  if (node == nullptr)
    throw std::runtime_error(
        "AST JSON cannot attach attributes to a null node");
  for (const auto &[name, attribute] : attributes)
    if (node->attributeExists(name))
      throw std::runtime_error("AST JSON attribute name is duplicated: " +
                               name);
  for (auto &[name, attribute] : attributes) {
    node->setAttribute(name, attribute.get());
    attribute.release();
  }
}

void attachAstAttributes(SgNode *node, const NodeRecord &record) {
  if (node == nullptr)
    throw std::runtime_error(
        "AST JSON cannot attach attributes to a null node");
  attachAstAttributes(node, prepareAstAttributes(record));
}

void validateSharedFunctionParameterScopeBindings(
    SgFunctionDeclaration *function) {
  ASSERT_not_null(function);
  ASSERT_not_null(function->get_parameterList());
  if (function->get_functionParameterScope() == nullptr) {
    return;
  }
  SgFunctionParameterScope *parameter_scope =
      function->get_functionParameterScope();
  SgFunctionDeclaration *scope_owner =
      isSgFunctionDeclaration(parameter_scope->get_parent());
  if (scope_owner == nullptr ||
      scope_owner->get_functionParameterScope() != parameter_scope ||
      (scope_owner != function &&
       scope_owner != function->get_firstNondefiningDeclaration() &&
       scope_owner != function->get_definingDeclaration())) {
    throw std::runtime_error(
        "AST JSON function parameter scope has no exact declaration owner");
  }
  if (scope_owner == function)
    return;
  for (SgInitializedName *parameter :
       function->get_parameterList()->get_args()) {
    if (parameter == nullptr || parameter->get_scope() != parameter_scope) {
      throw std::runtime_error(
          "AST JSON shared function parameter scope has a conflicting "
          "parameter binding");
    }
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
  if (auto *attribute = isSgStatementAttribute(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression_argument");
    auto *operand =
        target == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, target);
    if (attribute->get_expression_argument() != operand)
      throw std::runtime_error(
          "AST JSON statement attribute lost its construction operand");
    attribute->validate();
  } else if (auto *list = isSgStatementAttributeList(node)) {
    const auto &edges = edgesFor(record, "attributes");
    const auto &attributes = list->get_attributes();
    if (edges.size() != attributes.size())
      throw std::runtime_error(
          "AST JSON statement attribute list lost its construction children");
    for (size_t i = 0; i < edges.size(); ++i)
      if (attributes[i] !=
          nodeByIdAs<SgStatementAttribute>(nodes, edges[i].target))
        throw std::runtime_error(
            "AST JSON statement attribute list has a conflicting child");
    list->validate();
  } else if (auto *statement = isSgAttributedStatement(node)) {
    statement->validate();
  }
  if (auto *declaration = isSgDeclarationStatement(node)) {
    appendEdgeList<SgAlignmentSpecifierPtrList, SgAlignmentSpecifier>(
        declaration->get_alignment_specifiers(), record, "alignment_specifiers",
        nodes, declaration, true, true);
  } else if (auto *name = isSgInitializedName(node)) {
    appendEdgeList<SgAlignmentSpecifierPtrList, SgAlignmentSpecifier>(
        name->get_alignment_specifiers(), record, "alignment_specifiers", nodes,
        name, true, true);
  }
  if (auto *alignment = isSgAlignmentSpecifier(node)) {
    const uint64_t operand_id = singleEdgeTarget(record, "operand");
    auto *operand =
        operand_id == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, operand_id);
    if (alignment->get_operand() != operand ||
        (operand != nullptr && operand->get_parent() != alignment))
      throw std::runtime_error(
          "AST JSON alignment lost its construction-time operand");
  }
  if (auto *clause = isSgOmpUsesAllocatorsClause(node)) {
    appendEdgeList<SgOmpUsesAllocatorsDefinationPtrList,
                   SgOmpUsesAllocatorsDefination>(
        clause->get_uses_allocators_defination(), record,
        "uses_allocators_defination", nodes, clause);
  }
  auto check_conversion_type = [&](auto *reference) {
    const uint64_t target = singleEdgeTarget(record, "conversion_type_id");
    auto *type_id =
        target == 0 ? nullptr : nodeByIdAs<SgTypeExpression>(nodes, target);
    if (reference->get_conversion_type_id() != type_id ||
        (type_id != nullptr && type_id->get_parent() != reference))
      throw std::runtime_error(
          "AST JSON member conversion operand differs from construction");
  };
  if (auto *member = isSgMemberFunctionRefExp(node)) {
    check_conversion_type(member);
  } else if (auto *member = isSgTemplateMemberFunctionRefExp(node)) {
    check_conversion_type(member);
  }
  if (uint64_t target = singleEdgeTarget(record, "parent")) {
    if (node->get_parent() != nodeById(nodes, target))
      throw std::runtime_error(
          "AST JSON node lost its recorded parent during edge linking");
  }
  if (SgRequiresExpr *requires_expr = isSgRequiresExpr(node)) {
    const uint64_t list_id = singleEdgeTarget(record, "local_parameter_list");
    const uint64_t syntax_id =
        singleEdgeTarget(record, "local_parameter_list_syntax");
    auto *syntax = syntax_id != 0
                       ? nodeByIdAs<SgFunctionParameterList>(nodes, syntax_id)
                       : nullptr;
    const uint64_t scope_id = singleEdgeTarget(record, "local_parameter_scope");
    SgFunctionParameterList *parameters =
        list_id != 0 ? nodeByIdAs<SgFunctionParameterList>(nodes, list_id)
                     : nullptr;
    SgFunctionParameterScope *parameter_scope =
        scope_id != 0 ? nodeByIdAs<SgFunctionParameterScope>(nodes, scope_id)
                      : nullptr;
    if ((parameters == nullptr) != (parameter_scope == nullptr) ||
        requires_expr->get_local_parameter_list() != parameters ||
        requires_expr->get_local_parameter_list_syntax() != syntax ||
        (syntax != nullptr && syntax->get_parent() != requires_expr) ||
        requires_expr->get_local_parameter_scope() != parameter_scope ||
        (parameters != nullptr &&
         (parameters->get_parent() != requires_expr ||
          parameter_scope->get_parent() != requires_expr))) {
      throw std::runtime_error("AST JSON requires-expression local region "
                               "differs from construction");
    }
    const uint64_t target = requiredSingleEdgeTarget(record, "requirements");
    SgExprListExp *requirements = nodeByIdAs<SgExprListExp>(nodes, target);
    if (requires_expr->get_requirements() != requirements ||
        requirements->get_parent() != requires_expr) {
      throw std::runtime_error("AST JSON requires-expression requirement list "
                               "differs from construction");
    }
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    for (const auto &[field, actual] :
         {std::pair{"numeric_label", stmt->get_numeric_label()},
          std::pair{"end_numeric_label", stmt->has_end_numeric_label()
                                             ? stmt->get_end_numeric_label()
                                             : nullptr}}) {
      const uint64_t id = singleEdgeTarget(record, field);
      SgLabelRefExp *expected =
          id != 0 ? nodeByIdAs<SgLabelRefExp>(nodes, id) : nullptr;
      if (actual != expected ||
          (actual != nullptr && actual->get_parent() != stmt))
        throw std::runtime_error(
            "AST JSON statement lost its constructed label");
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
      if (scope->get_auxiliary_declarations() != nullptr &&
          scope->get_auxiliary_declarations() != container) {
        throw std::runtime_error(
            "AST JSON scope has conflicting auxiliary declaration "
            "containers");
      }
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
        container->get_declarations(), record, "declarations", nodes,
        container);
    if (container->get_declarations().empty())
      throw std::runtime_error(
          "AST JSON SgAuxiliaryDeclarationList must not be empty");
  }
  if (SgTemplateParameterList *container = isSgTemplateParameterList(node)) {
    SgTemplateParameterPtrList parameters;
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        parameters, record, "args", nodes, container);
    container->set_args(parameters);
  }

  if (SgSourceFile *file = isSgSourceFile(node)) {
    if (!(file->get_target_abi() ==
          targetAbiFromJson(record.properties.at("target_abi")))) {
      throw std::runtime_error(
          "AST JSON SgSourceFile target ABI differs from construction");
    }
    if (file->get_target_size_type() !=
        targetSizeTypeFromJson(record.properties.at("target_size_type"), nodes,
                               file->get_target_abi().available)) {
      throw std::runtime_error(
          "AST JSON SgSourceFile target size_t differs from construction");
    }
    const uint64_t target = singleEdgeTarget(record, "globalScope");
    if (target != 0) {
      SgGlobal *global = nodeByIdAs<SgGlobal>(nodes, target);
      file->set_globalScope(global);
      ROSE_ASSERT(global->get_parent() != nullptr);
    }
    appendEdgeList<SgTokenPtrList, SgToken>(file->get_token_list(), record,
                                            "token_list", nodes, file);
    if (uint64_t external =
            singleEdgeTarget(record, "frontendExternalFileList")) {
      file->set_frontendExternalFileList(
          nodeByIdAs<SgFileList>(nodes, external));
    }
    if (uint64_t include = singleEdgeTarget(record, "associated_include_file"))
      file->set_associated_include_file(
          nodeByIdAs<SgIncludeFile>(nodes, include));
    restoreTokenStreamContainer(file);
    return;
  }
  if (SgFileList *files = isSgFileList(node)) {
    appendEdgeList<SgFilePtrList, SgFile>(files->get_listOfFiles(), record,
                                          "listOfFiles", nodes, files);
    return;
  }
  if (SgIncludeFile *include = isSgIncludeFile(node)) {
    include->set_source_file(nodeByIdAs<SgSourceFile>(
        nodes, requiredSingleEdgeTarget(record, "source_file")));
    include->set_source_file_of_translation_unit(nodeByIdAs<SgSourceFile>(
        nodes,
        requiredSingleEdgeTarget(record, "source_file_of_translation_unit")));
    include->set_including_source_file(nodeByIdAs<SgSourceFile>(
        nodes, requiredSingleEdgeTarget(record, "including_source_file")));
    if (uint64_t parent = singleEdgeTarget(record, "parent_include_file"))
      include->set_parent_include_file(
          nodeByIdAs<SgIncludeFile>(nodes, parent));
    appendEdgeList<SgIncludeFilePtrList, SgIncludeFile>(
        include->get_include_file_list(), record, "include_file_list", nodes,
        include);
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
    auto *owner = isSgDeclarationStatement(scope->get_parent());
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        scope->get_declarations(), record, "declarations", nodes, scope, true,
        owner != nullptr && owner->get_source_declarator_scope() == scope);
  }
  if (auto *prototype = isSgFunctionParameterTypeList(node)) {
    auto *function = isSgFunctionType(prototype->get_parent());
    if (function == nullptr || function->get_argument_list() != prototype ||
        prototype->get_source_parameter_scope() !=
            nodeByIdAs<SgFunctionParameterScope>(
                nodes,
                requiredSingleEdgeTarget(record, "source_parameter_scope")) ||
        prototype->get_source_parameters() !=
            nodeByIdAs<SgFunctionParameterList>(
                nodes, requiredSingleEdgeTarget(record, "source_parameters")))
      throw std::runtime_error(
          "AST JSON prototype lost its constructed function-type ownership");
    prototype->validate_source_prototype();
    return;
  }
  if (SgFunctionParameterScope *scope = isSgFunctionParameterScope(node)) {
    const uint64_t target =
        singleEdgeTarget(record, "completed_semantic_scope");
    if (scope->get_completed_semantic_scope() !=
        (target != 0 ? nodeByIdAs<SgScopeStatement>(nodes, target) : nullptr)) {
      throw std::runtime_error(
          "AST JSON function parameter scope lost its construction-time "
          "semantic owner");
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
    const auto arguments = edgesFor(record, "args");
    if (params->get_args().size() != arguments.size())
      throw std::runtime_error(
          "AST JSON function parameter list differs from construction");
    for (size_t index = 0; index < arguments.size(); ++index) {
      auto *parameter =
          nodeByIdAs<SgInitializedName>(nodes, arguments[index].target);
      if (params->get_args()[index] != parameter ||
          parameter->get_parent() != params)
        throw std::runtime_error(
            "AST JSON function parameter list lost its constructed ordered "
            "ownership");
    }
    if (SgFunctionDeclaration *function =
            isSgFunctionDeclaration(params->get_parent())) {
      if (function->get_parameterList() == params)
        validateSharedFunctionParameterScopeBindings(function);
    } else if (SgRequiresExpr *requires_expr =
                   isSgRequiresExpr(params->get_parent())) {
      SgFunctionParameterScope *scope =
          requires_expr->get_local_parameter_scope();
      const bool written =
          requires_expr->get_local_parameter_list_syntax() == params;
      if ((!written && requires_expr->get_local_parameter_list() != params) ||
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
        if (written) {
          if (scope->find_symbol_from_declaration(parameter) != nullptr)
            throw std::runtime_error("AST JSON requires-expression written "
                                     "parameter has a lookup symbol");
        } else if (!parameter->get_name().getString().empty()) {
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
  if (auto *definition = isSgFunctionDefinition(node)) {
    auto *owner = isSgMemberFunctionDeclaration(definition->get_declaration());
    const bool destructor =
        owner != nullptr && owner->get_specialFunctionModifier().isDestructor();
    const bool dependent = destructor && record.properties.requiredBool(
                                             "destructor_epilogue_dependent");
    const uint64_t epilogue_id =
        singleEdgeTarget(record, "destructor_epilogue");
    auto *epilogue = epilogue_id != 0
                         ? nodeByIdAs<SgExprListExp>(nodes, epilogue_id)
                         : nullptr;
    if (definition->get_destructor_epilogue_dependent() != dependent ||
        definition->get_destructor_epilogue() != epilogue)
      throw std::runtime_error(
          "AST JSON destructor epilogue differs from construction");
  }
  if (auto *destruction = isSgCxxSubobjectDestruction(node)) {
    const uint64_t member = singleEdgeTarget(record, "member_target");
    const uint64_t base = singleEdgeTarget(record, "base_target");
    if (destruction->get_destructor_declaration() !=
            nodeByIdAs<SgMemberFunctionDeclaration>(
                nodes,
                requiredSingleEdgeTarget(record, "destructor_declaration")) ||
        destruction->get_member_target() !=
            (member != 0 ? nodeByIdAs<SgInitializedName>(nodes, member)
                         : nullptr) ||
        destruction->get_base_target() !=
            (base != 0 ? nodeByIdAs<SgClassDeclaration>(nodes, base)
                       : nullptr) ||
        destruction->get_target_kind() !=
            subobjectDestructionKindFromJson(record.properties))
      throw std::runtime_error(
          "AST JSON subobject destruction differs from construction");
    destruction->validate();
  }
  if (SgExprListExp *exprs = isSgExprListExp(node)) {
    if (static_cast<int>(exprs->get_source_form()) !=
        record.properties.requiredInt("expression_list_source_form"))
      throw std::runtime_error(
          "AST JSON expression-list source form differs from construction");
    const std::vector<EdgeRecord> expression_edges =
        edgesFor(record, "expressions");
    if (exprs->get_expressions().size() != expression_edges.size())
      throw std::runtime_error(
          "AST JSON constructed expression list disagrees with its exact "
          "serialized edge count");
    for (size_t index = 0; index < expression_edges.size(); ++index) {
      auto *expected =
          nodeByIdAs<SgExpression>(nodes, expression_edges[index].target);
      if (exprs->get_expressions()[index] != expected ||
          expected->get_parent() != exprs)
        throw std::runtime_error(
            "AST JSON constructed expression list disagrees with its exact "
            "serialized child ownership");
    }
    return;
  }
  if (auto *use = isSgCxxDefaultInitExp(node)) {
    if (use->get_expression() !=
        nodeByIdAs<SgExpression>(
            nodes, requiredSingleEdgeTarget(record, "expression")))
      throw std::runtime_error(
          "AST JSON default argument differs from its constructed expansion");
    if (const uint64_t parameter = defaultInitializerDeclarationNodeFromJson(
            record.properties.at("source_declaration"));
        parameter != 0 && use->get_source_declaration() !=
                              nodeByIdAs<SgInitializedName>(nodes, parameter))
      throw std::runtime_error(
          "AST JSON default argument differs from its constructed parameter");
    use->validate();
  }
  if (auto *binding = isSgOpaqueValueBinding(node)) {
    if (binding->get_source_expression() !=
        nodeByIdAs<SgExpression>(
            nodes, requiredSingleEdgeTarget(record, "source_expression")))
      throw std::runtime_error(
          "AST JSON opaque binding differs from construction");
    binding->validate();
  }
  if (auto *reference = isSgOpaqueValueReference(node)) {
    if (reference->get_binding() !=
        nodeByIdAs<SgOpaqueValueBinding>(
            nodes, requiredSingleEdgeTarget(record, "binding")))
      throw std::runtime_error(
          "AST JSON opaque reference differs from construction");
    reference->validate();
  }
  if (auto *pseudo = isSgPseudoObjectExp(node)) {
    const uint64_t result = singleEdgeTarget(record, "result_expression");
    SgOpaqueValueBindingPtrList captures;
    for (const auto &edge : edgesFor(record, "captured_bindings"))
      captures.push_back(nodeByIdAs<SgOpaqueValueBinding>(nodes, edge.target));
    if (pseudo->get_written_expression() !=
            nodeByIdAs<SgExpression>(
                nodes,
                requiredSingleEdgeTarget(record, "written_expression")) ||
        pseudo->get_semantic_evaluations() !=
            nodeByIdAs<SgExprListExp>(
                nodes,
                requiredSingleEdgeTarget(record, "semantic_evaluations")) ||
        pseudo->get_result_expression() !=
            (result == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, result)) ||
        pseudo->get_captured_bindings() != captures)
      throw std::runtime_error(
          "AST JSON pseudo-object differs from construction");
    pseudo->validate();
  }
  if (auto *binding = isSgCxxArrayInitBinding(node)) {
    if (binding->get_common_source() !=
        nodeByIdAs<SgExpression>(
            nodes, requiredSingleEdgeTarget(record, "common_source")))
      throw std::runtime_error(
          "AST JSON array initialization binding differs from construction");
    binding->validate();
  }
  if (auto *loop = isSgCxxArrayInitLoop(node)) {
    if (loop->get_binding() !=
            nodeByIdAs<SgCxxArrayInitBinding>(
                nodes, requiredSingleEdgeTarget(record, "binding")) ||
        loop->get_element_initializer() !=
            nodeByIdAs<SgExpression>(
                nodes, requiredSingleEdgeTarget(record, "element_initializer")))
      throw std::runtime_error(
          "AST JSON array initialization loop differs from construction");
    loop->validate();
  }
  if (auto *reference = isSgCxxArrayInitReference(node)) {
    if (reference->get_binding() !=
        nodeByIdAs<SgCxxArrayInitBinding>(
            nodes, requiredSingleEdgeTarget(record, "binding")))
      throw std::runtime_error(
          "AST JSON array initialization reference differs from construction");
    reference->validate();
  }
  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(node)) {
    const uint64_t target =
        requiredSingleEdgeTarget(record, "expanded_expression");
    SgExpression *expanded = nodeByIdAs<SgExpression>(nodes, target);
    if (macro->get_expanded_expression() != expanded ||
        expanded->get_parent() != macro) {
      throw std::runtime_error(
          "AST JSON SgMacroExpansionExp has an inconsistent exact expansion");
    }
  }
  if (SgDesignator *designator = isSgDesignator(node)) {
    const uint64_t first_target =
        requiredSingleEdgeTarget(record, "first_expression");
    SgExpression *first = nodeByIdAs<SgExpression>(nodes, first_target);
    const uint64_t second_target =
        singleEdgeTarget(record, "second_expression");
    SgExpression *second = second_target != 0
                               ? nodeByIdAs<SgExpression>(nodes, second_target)
                               : nullptr;
    if (designator->get_first_expression() != first ||
        first->get_parent() != designator ||
        designator->get_second_expression() != second ||
        (second != nullptr && second->get_parent() != designator))
      throw std::runtime_error(
          "AST JSON designator operands differ from typed construction");
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
    appendEdgeList<SgBaseClassPtrList, SgBaseClass>(
        def->get_inheritances(), record, "inheritances", nodes, def);
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        def->get_members(), record, "members", nodes, def);
    return;
  }
  if (SgCtorInitializerList *ctors = isSgCtorInitializerList(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        ctors->get_ctors(), record, "ctors", nodes, ctors,
        /*owns_children=*/true, /*require_constructed_list=*/true);
  }
  if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
    appendEdgeList<SgInitializedNamePtrList, SgInitializedName>(
        decl->get_variables(), record, "variables", nodes, decl,
        /*owns_children=*/true, /*require_constructed_list=*/true);
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
    SgInitializedName *name = nodeByIdAs<SgInitializedName>(
        nodes, requiredSingleEdgeTarget(record, "vardefn"));
    const uint64_t bitfield_id = singleEdgeTarget(record, "bitfield");
    SgExpression *bitfield = bitfield_id != 0
                                 ? nodeByIdAs<SgExpression>(nodes, bitfield_id)
                                 : nullptr;
    if (def->get_vardefn() != name || name->get_definition() != def ||
        def->get_parent() != name || def->get_bitfield() != bitfield ||
        (bitfield != nullptr && bitfield->get_parent() != def)) {
      throw std::runtime_error(
          "AST JSON variable definition lost its typed construction links");
    }
  }
  if (SgCommonBlock *stmt = isSgCommonBlock(node)) {
    appendEdgeList<SgCommonBlockObjectPtrList, SgCommonBlockObject>(
        stmt->get_block_list(), record, "block_list", nodes, stmt);
  }
  if (SgCommonBlockObject *object = isSgCommonBlockObject(node)) {
    if (object->get_block_name() !=
        record.properties.requiredString("block_name"))
      throw std::runtime_error(
          "AST JSON common block name differs from construction");
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
  if (isSgFortranDataInitializer(node) || isSgDataStatementGroup(node) ||
      isSgDataStatementObject(node) || isSgDataStatementValue(node) ||
      isSgNameGroup(node) || isSgNamelistStatement(node) ||
      isSgFormatItem(node) || isSgFormatItemList(node) ||
      isSgFormatStatement(node)) {
    auto check_list = [&](const auto &children, const char *field) {
      const auto edges = edgesFor(record, field);
      if (edges.size() != children.size())
        throw std::runtime_error(
            "AST JSON DATA list differs from construction");
      for (size_t index = 0; index < edges.size(); ++index)
        if (nodeById(nodes, edges[index].target) != children[index] ||
            children[index]->get_parent() != node)
          throw std::runtime_error(
              "AST JSON DATA list ownership differs from construction");
    };
    auto check_child = [&](SgNode *child, const char *field) {
      const uint64_t id = singleEdgeTarget(record, field);
      if ((id == 0) != (child == nullptr) ||
          (child != nullptr &&
           (nodeById(nodes, id) != child || child->get_parent() != node)))
        throw std::runtime_error(
            "AST JSON DATA child differs from construction");
    };
    if (auto *item = isSgFormatItem(node)) {
      check_child(item->get_format_item_list(), "format_item_list");
      item->validate();
    } else if (auto *list = isSgFormatItemList(node)) {
      check_list(list->get_format_item_list(), "format_item_list");
      list->validate();
    } else if (auto *statement = isSgFormatStatement(node)) {
      check_child(statement->get_format_item_list(), "format_item_list");
      statement->get_format_item_list()->validate();
    } else if (auto *group = isSgNameGroup(node)) {
      if (group->get_group_name() !=
          record.properties.requiredString("group_name"))
        throw std::runtime_error(
            "AST JSON NAMELIST group name changed during construction");
      check_list(group->get_objects(), "objects");
      group->validate_group();
    } else if (auto *statement = isSgNamelistStatement(node)) {
      check_list(statement->get_group_list(), "group_list");
    } else if (auto *initializer = isSgFortranDataInitializer(node)) {
      check_list(initializer->get_values(), "values");
      initializer->validate_initializer();
    } else if (auto *group = isSgDataStatementGroup(node)) {
      check_list(group->get_object_list(), "object_list");
      check_list(group->get_value_list(), "value_list");
      group->validate_group();
    } else if (auto *object = isSgDataStatementObject(node)) {
      check_child(object->get_variableReference_list(),
                  "variableReference_list");
      object->validate_object();
    } else if (auto *value = isSgDataStatementValue(node)) {
      check_child(value->get_constant_expression(), "constant_expression");
      check_child(value->get_repeat_expression(), "repeat_expression");
      value->validate_value();
    }
  }
  if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
    const auto ranges = edgesFor(record, "ranges");
    if (ranges.size() != stmt->get_ranges().size())
      throw std::runtime_error(
          "AST JSON implicit ranges differ from construction");
    for (size_t index = 0; index < ranges.size(); ++index) {
      auto *range =
          nodeByIdAs<SgFortranImplicitRange>(nodes, ranges[index].target);
      if (stmt->get_ranges()[index] != range || range->get_parent() != stmt)
        throw std::runtime_error(
            "AST JSON implicit range ownership differs from construction");
    }
    stmt->validate_implicit_statement();
  }
  if (SgForInitStatement *init = isSgForInitStatement(node)) {
    appendEdgeList<SgStatementPtrList, SgStatement>(
        init->get_init_stmt(), record, "init_stmt", nodes, init);
  }
  if (SgAccExpressionClause *clause = isSgAccExpressionClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    SgExpression *expression =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_expression() != expression ||
        (expression != nullptr && expression->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenACC expression differs from construction");
  }
  if (SgAccVariablesClause *clause = isSgAccVariablesClause(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "variables");
    SgExprListExp *variables = nodeByIdAs<SgExprListExp>(nodes, target);
    if (clause->get_variables() != variables ||
        variables->get_parent() != clause)
      throw std::runtime_error(
          "AST JSON OpenACC variables differ from construction");
  }
  if (SgAccBodyStatement *stmt = isSgAccBodyStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "body");
    SgStatement *body = nodeByIdAs<SgStatement>(nodes, target);
    if (stmt->get_body() != body || body->get_parent() != stmt)
      throw std::runtime_error(
          "AST JSON OpenACC body differs from construction");
  }
  if (SgAccClauseBodyStatement *stmt = isSgAccClauseBodyStatement(node)) {
    appendEdgeList<SgAccClausePtrList, SgAccClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  } else if (SgAccClauseStatement *stmt = isSgAccClauseStatement(node)) {
    appendEdgeList<SgAccClausePtrList, SgAccClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
  }
  if (SgAccWaitStatement *stmt = isSgAccWaitStatement(node)) {
    const uint64_t list_id = singleEdgeTarget(record, "wait_list");
    const uint64_t devnum_id = singleEdgeTarget(record, "devnum");
    SgExprListExp *wait_list =
        list_id != 0 ? nodeByIdAs<SgExprListExp>(nodes, list_id) : nullptr;
    SgExpression *devnum =
        devnum_id != 0 ? nodeByIdAs<SgExpression>(nodes, devnum_id) : nullptr;
    if (stmt->get_wait_list() != wait_list ||
        (wait_list != nullptr && wait_list->get_parent() != stmt) ||
        stmt->get_devnum() != devnum ||
        (devnum != nullptr && devnum->get_parent() != stmt) ||
        stmt->get_queues() != record.properties.requiredBool("queues"))
      throw std::runtime_error(
          "AST JSON OpenACC wait operands differ from construction");
  }
  if (SgAccCacheStatement *stmt = isSgAccCacheStatement(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "variables");
    SgExprListExp *variables = nodeByIdAs<SgExprListExp>(nodes, target);
    if (stmt->get_variables() != variables || variables->get_parent() != stmt ||
        stmt->get_modifier() != record.properties.requiredInt("modifier"))
      throw std::runtime_error(
          "AST JSON OpenACC cache operands differ from construction");
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
    SgExprListExp *variables = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "variables"));
    if (stmt->get_variables() != variables || variables->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON groupprivate variables changed after construction");
    }
    linkRequiredOmpClauseList(stmt, record, nodes);
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    appendEdgeList<SgOmpClausePtrList, SgOmpClause>(stmt->get_clauses(), record,
                                                    "clauses", nodes, stmt);
    for (auto [field, operand] :
         {std::pair{"user_defined_identifier",
                    stmt->get_user_defined_identifier()},
          std::pair{"mapper_type", stmt->get_mapper_type()},
          std::pair{"mapper_variable", stmt->get_mapper_variable()}}) {
      const uint64_t target = singleEdgeTarget(record, field);
      if (operand != (target != 0 ? nodeByIdAs<SgExpression>(nodes, target)
                                  : nullptr) ||
          (operand != nullptr && operand->get_parent() != stmt))
        throw std::runtime_error(
            "AST JSON declare mapper lost its constructed operand");
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
    if (stmt->get_namelist() !=
        exactBoundSymbolFromJson(record.properties.at("namelist"), nodes))
      throw std::runtime_error(
          "AST JSON WRITE lost its constructed NAMELIST binding");
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgReadStatement *stmt = isSgReadStatement(node)) {
    if (stmt->get_namelist() !=
        exactBoundSymbolFromJson(record.properties.at("namelist"), nodes))
      throw std::runtime_error(
          "AST JSON READ lost its constructed NAMELIST binding");
    if (uint64_t target = singleEdgeTarget(record, "format")) {
      SgExpression *expr = nodeByIdAs<SgExpression>(nodes, target);
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }
  if (SgAttributeSpecificationStatement *stmt =
          isSgAttributeSpecificationStatement(node)) {
    const auto groups = edgesFor(record, "data_statement_group_list");
    if (groups.size() != stmt->get_data_statement_group_list().size())
      throw std::runtime_error("AST JSON DATA groups differ from construction");
    for (size_t index = 0; index < groups.size(); ++index)
      if (nodeById(nodes, groups[index].target) !=
          stmt->get_data_statement_group_list()[index])
        throw std::runtime_error(
            "AST JSON DATA group identity differs from construction");
    stmt->validate_data_groups();
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
  }
  if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    uint64_t target = singleEdgeTarget(record, "functionDeclaration");
    if (target == 0) {
      target =
          record.properties.requiredNonnegativeNodeId("function_declaration");
    }
    if (target != 0) {
      body->set_functionDeclaration(
          nodeByIdAs<SgFunctionDeclaration>(nodes, target));
    }
  }

  if (SgInitializedName *name = isSgInitializedName(node)) {
    const auto owner = singleEdgeTarget(record, "structured_binding_owner");
    const auto value = singleEdgeTarget(record, "structured_binding_value");
    const auto holding =
        singleEdgeTarget(record, "structured_binding_holding_variable");
    if (name->get_structured_binding_kind() !=
            structuredBindingKindFromJson(record.properties) ||
        name->get_structured_binding_owner() !=
            (owner != 0 ? nodeByIdAs<SgInitializedName>(nodes, owner)
                        : nullptr) ||
        name->get_structured_binding_value() !=
            (value != 0 ? nodeByIdAs<SgExpression>(nodes, value) : nullptr) ||
        name->get_structured_binding_holding_variable() !=
            (holding != 0 ? nodeByIdAs<SgInitializedName>(nodes, holding)
                          : nullptr))
      throw std::runtime_error(
          "AST JSON structured-binding data differs from construction");
    const uint64_t pattern =
        singleEdgeTarget(record, "structured_binding_pattern");
    const uint64_t binding_scope =
        singleEdgeTarget(record, "structured_binding_scope");
    if (record.properties.requiredNonnegativeNodeId(
            "structured_binding_scope") != binding_scope ||
        name->get_structured_binding_scope() !=
            (binding_scope != 0
                 ? nodeByIdAs<SgDeclarationScope>(nodes, binding_scope)
                 : nullptr))
      throw std::runtime_error(
          "AST JSON structured-binding scope differs from construction");
    if (name->get_structured_binding_pattern() !=
        (pattern != 0 ? nodeByIdAs<SgExprListExp>(nodes, pattern) : nullptr))
      throw std::runtime_error(
          "AST JSON structured-binding pattern differs from construction");
    const uint64_t shape_edge_target =
        singleEdgeTarget(record, "fortran_cray_pointer_pointee_shape");
    if (shape_edge_target != 0) {
      SgExprListExp *shape =
          nodeByIdAs<SgExprListExp>(nodes, shape_edge_target);
      name->set_fortran_cray_pointer_pointee_shape(shape);
      shape->set_parent(name);
    }
    if (uint64_t target = singleEdgeTarget(record, "initptr")) {
      SgInitializer *init = nodeByIdAs<SgInitializer>(nodes, target);
      if (auto *assignment = isSgAssignInitializer(init)) {
        if (auto *value = isSgEnumVal(assignment->get_operand_i());
            value != nullptr && name->get_enum_constant_value().has_value() &&
            value->get_value() != *name->get_enum_constant_value()) {
          throw std::runtime_error(
              "AST JSON enum initializer disagrees with its evaluated value");
        }
      }
      if (name->get_initializer() != init || init->get_parent() != name) {
        throw std::runtime_error(
            "AST JSON initialized-name initializer differs from construction");
      }
    } else if (name->get_initializer() != nullptr) {
      throw std::runtime_error("AST JSON initialized-name has an unrecorded "
                               "constructed initializer");
    }
    if (uint64_t target = singleEdgeTarget(record, "declptr")) {
      SgDeclarationStatement *declaration =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      if (name->get_declptr() != declaration) {
        throw std::runtime_error(
            "AST JSON initialized-name declaration owner changed during "
            "edge linking");
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "variable_definition")) {
      SgVariableDefinition *definition =
          nodeByIdAs<SgVariableDefinition>(nodes, target);
      if (name->get_definition() != definition) {
        throw std::runtime_error(
            "AST JSON initialized-name variable definition changed during "
            "edge linking");
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      if (name->get_scope() != nodeByIdAs<SgScopeStatement>(nodes, target)) {
        throw std::runtime_error(
            "AST JSON initialized-name scope changed during edge linking");
      }
    }
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      SgScopeStatement *scope = nodeByIdAs<SgScopeStatement>(nodes, target);
      if (decl->get_scope() != scope)
        throw std::runtime_error(
            "AST JSON declaration scope changed during edge linking");
    }
    if (isSgFunctionParameterList(decl) != nullptr) {
      if (singleEdgeTarget(record, "firstNondefiningDeclaration") !=
              record.id ||
          singleEdgeTarget(record, "definingDeclaration") != 0 ||
          decl->get_firstNondefiningDeclaration() != decl ||
          decl->get_definingDeclaration() != nullptr)
        throw std::runtime_error(
            "AST JSON function parameter list lost its constructor-owned "
            "declaration identity");
    } else if (uint64_t target =
                   singleEdgeTarget(record, "firstNondefiningDeclaration")) {
      SgDeclarationStatement *first =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      if (decl->get_firstNondefiningDeclaration() != first)
        throw std::runtime_error("AST JSON declaration canonical identity "
                                 "changed during edge linking");
    }
    if (isSgFunctionParameterList(decl) == nullptr) {
      if (uint64_t target = singleEdgeTarget(record, "definingDeclaration")) {
        if (decl->get_definingDeclaration() !=
            nodeByIdAs<SgDeclarationStatement>(nodes, target))
          throw std::runtime_error("AST JSON declaration defining identity "
                                   "changed during edge linking");
      }
    }
    if (uint64_t target = singleEdgeTarget(record, "declarationScope")) {
      decl->set_declarationScope(nodeByIdAs<SgDeclarationScope>(nodes, target));
    }
    const uint64_t source_scope_id =
        singleEdgeTarget(record, "source_declarator_scope");
    auto *source_scope =
        source_scope_id != 0
            ? nodeByIdAs<SgDeclarationScope>(nodes, source_scope_id)
            : nullptr;
    if (decl->get_source_declarator_scope() != source_scope ||
        (source_scope != nullptr && source_scope->get_parent() != decl))
      throw std::runtime_error(
          "AST JSON declaration lost its constructed source scope ownership");
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
  }
  auto link_result_name = [&](auto *decl) {
    if (uint64_t target = singleEdgeTarget(record, "result_name")) {
      SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, target);
      if (name->get_parent() == nullptr || name->get_scope() == nullptr)
        throw std::runtime_error(
            "AST JSON function result has no restored owner or scope");
      decl->set_result_name(name);
    }
  };
  if (SgProcedureHeaderStatement *decl = isSgProcedureHeaderStatement(node)) {
    link_result_name(decl);
  }
  if (SgEntryStatement *decl = isSgEntryStatement(node)) {
    link_result_name(decl);
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    const uint64_t template_id =
        singleEdgeTarget(record, "templateDeclaration");
    if (decl->get_templateDeclaration() !=
        (template_id != 0
             ? nodeByIdAs<SgDeclarationStatement>(nodes, template_id)
             : nullptr))
      throw std::runtime_error(
          "AST JSON nonreal declaration changed its typed template target");
    const uint64_t constraint_id =
        singleEdgeTarget(record, "conceptConstraint");
    SgExpression *constraint =
        constraint_id != 0 ? nodeByIdAs<SgExpression>(nodes, constraint_id)
                           : nullptr;
    if (constraint != nullptr && constraint->get_parent() != decl) {
      throw std::runtime_error("AST JSON concept constraint does not match its "
                               "recorded child owner");
    }
    appendEdgeList<SgTemplateParameterPtrList, SgTemplateParameter>(
        decl->get_tpl_params(), record, "tpl_params", nodes, decl);
    const auto arguments = edgesFor(record, "tpl_args");
    if (arguments.size() != decl->get_tpl_args().size())
      throw std::runtime_error("AST JSON nonreal template arguments differ "
                               "from construction");
    for (size_t index = 0; index < arguments.size(); ++index)
      if (nodeById(nodes, arguments[index].target) !=
              decl->get_tpl_args()[index] ||
          decl->get_tpl_args()[index]->get_parent() != decl)
        throw std::runtime_error("AST JSON nonreal template argument owner "
                                 "differs from construction");
    decl->set_conceptConstraint(constraint);
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    SgDeclarationStatement *base_decl = nullptr;
    if (uint64_t target = singleEdgeTarget(record, "declaration"))
      base_decl = nodeByIdAs<SgDeclarationStatement>(nodes, target);
    if (decl->get_declaration() != base_decl)
      throw std::runtime_error(
          "AST JSON typedef lost its typed base declaration");
  }
  if (SgTemplateInstantiationDirectiveStatement *decl =
          isSgTemplateInstantiationDirectiveStatement(node)) {
    auto *instantiated = nodeByIdAs<SgDeclarationStatement>(
        nodes, requiredSingleEdgeTarget(record, "declaration"));
    if (decl->get_declaration() != instantiated ||
        instantiated->get_parent() != decl)
      throw std::runtime_error(
          "AST JSON template instantiation directive lost its typed child");
  }
  if (SgUseStatement *stmt = isSgUseStatement(node)) {
    appendEdgeList<SgRenamePairPtrList, SgRenamePair>(
        stmt->get_rename_list(), record, "rename_list", nodes, stmt);
    if (uint64_t target = singleEdgeTarget(record, "module")) {
      if (stmt->get_module() != nodeByIdAs<SgModuleStatement>(nodes, target))
        throw std::runtime_error(
            "AST JSON USE statement lost its constructor module");
    } else {
      const bool external_module_present =
          record.properties.at("external_module").requiredBool("present");
      if ((stmt->get_module() != nullptr) != external_module_present ||
          (external_module_present &&
           !isAstJsonExternalModule(stmt->get_module())))
        throw std::runtime_error(
            "AST JSON USE statement has an inconsistent external module");
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
        decl->get_semanticTemplateArguments(), record,
        "semanticTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl);
    const uint64_t target =
        singleEdgeTarget(record, "specializedTemplateDeclaration");
    auto *selected =
        target != 0 ? nodeByIdAs<SgTemplateVariableDeclaration>(nodes, target)
                    : nullptr;
    if (decl->get_specializedTemplateDeclaration() != selected)
      throw std::runtime_error(
          "AST JSON variable template identity differs from construction");
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
    const uint64_t primary =
        singleEdgeTarget(record, "specializedTemplateDeclaration");
    if (decl->get_specializedTemplateDeclaration() !=
        (primary != 0 ? nodeByIdAs<SgTemplateClassDeclaration>(nodes, primary)
                      : nullptr))
      throw std::runtime_error(
          "AST JSON template class changed its primary after construction");
    const uint64_t member_pattern =
        singleEdgeTarget(record, "instantiatedFromMemberTemplate");
    if (decl->get_instantiatedFromMemberTemplate() !=
        (member_pattern != 0
             ? nodeByIdAs<SgTemplateClassDeclaration>(nodes, member_pattern)
             : nullptr))
      throw std::runtime_error(
          "AST JSON member template changed its pattern after construction");
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
    if (singleEdgeTarget(record, "expression") != 0)
      throw std::runtime_error(
          "AST JSON template parameter cannot own an expression payload");
    const auto fields =
        templateParameterSourceFieldsFromJson(record.properties);
    const uint64_t canonical_edge =
        singleEdgeTarget(record, "canonicalNonTypeParameter");
    if (canonical_edge != 0 &&
        canonical_edge !=
            static_cast<uint64_t>(fields.canonicalNonTypeParameter))
      throw std::runtime_error(
          "AST JSON template parameter has conflicting canonical identity");
    SgTemplateParameter *canonical =
        fields.canonicalNonTypeParameter == 0
            ? nullptr
            : nodeByIdAs<SgTemplateParameter>(
                  nodes,
                  static_cast<uint64_t>(fields.canonicalNonTypeParameter));
    if (parameter->get_canonicalNonTypeParameter() != canonical)
      throw std::runtime_error("AST JSON template parameter canonical identity "
                               "differs from construction");
    auto reference_id = [&](const char *edge, const char *property) {
      const int64_t id = record.properties.requiredInt(property);
      const uint64_t edge_id = singleEdgeTarget(record, edge);
      if (id < 0 || (edge_id != 0 && edge_id != static_cast<uint64_t>(id)))
        throw std::runtime_error(
            "AST JSON template parameter has conflicting " +
            std::string(property));
      return static_cast<uint64_t>(id);
    };
    auto set_expression =
        [&](const std::string &edge, const std::string &property,
            void (SgTemplateParameter::*setter)(SgExpression *)) {
          SgExpression *expr =
              expressionFromRef(record.properties.at(property), nodes);
          if (uint64_t target = singleEdgeTarget(record, edge)) {
            if (nodeByIdAs<SgExpression>(nodes, target) != expr)
              throw std::runtime_error(
                  "AST JSON template parameter has conflicting " + property);
          }
          if (expr != nullptr) {
            if (expr->get_parent() != nullptr &&
                expr->get_parent() != parameter)
              throw std::runtime_error(
                  "AST JSON template parameter has a foreign " + property);
            expr->set_parent(parameter);
          }
          (parameter->*setter)(expr);
        };
    set_expression("typeConstraint", "type_constraint",
                   &SgTemplateParameter::set_typeConstraint);
    set_expression("sourceTypeConstraint", "source_type_constraint",
                   &SgTemplateParameter::set_sourceTypeConstraint);
    (void)templateParameterOperandsFromRecord(record, nodes, parameter);
    const uint64_t template_target =
        reference_id("templateDeclaration", "template_declaration");
    SgDeclarationStatement *template_declaration =
        template_target != 0
            ? nodeByIdAs<SgTemplateDeclaration>(nodes, template_target)
            : nullptr;
    if (parameter->get_templateDeclaration() != template_declaration)
      throw std::runtime_error(
          "AST JSON template parameter declaration differs from construction");
    if (uint64_t target = reference_id("initializedName", "initialized_name")) {
      auto *name = nodeByIdAs<SgInitializedName>(nodes, target);
      if (parameter->get_initializedName() != name ||
          name->get_parent() != parameter)
        throw std::runtime_error(
            "AST JSON template parameter declarator differs from construction");
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    appendEdgeList<SgTemplateParameterListPtrList, SgTemplateParameterList>(
        decl->get_sourceSpelledTemplateHeaders(), record,
        "source_spelled_template_headers", nodes, decl);
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    const uint64_t lambda_id = singleEdgeTarget(record, "lambda_expression");
    auto *lambda =
        lambda_id == 0 ? nullptr : nodeByIdAs<SgLambdaExp>(nodes, lambda_id);
    if (lambda != nullptr &&
        (decl->get_class_type() != SgClassDeclaration::e_lambda_closure ||
         lambda->get_lambda_closure_class() != decl))
      throw std::runtime_error(
          "AST JSON closure recipe names a different class");
    if (decl->get_lambda_expression() != lambda)
      throw std::runtime_error(
          "AST JSON closure recipe differs from native construction");
    appendEdgeList<SgTemplateParameterListPtrList, SgTemplateParameterList>(
        decl->get_sourceSpelledTemplateHeaders(), record,
        "source_spelled_template_headers", nodes, decl);
  }
  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes, decl,
        /*owns_children=*/true, /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_semanticTemplateArguments(), record,
        "semanticTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
    if (decl->get_templateDeclaration() !=
        nodeByIdAs<SgTemplateClassDeclaration>(
            nodes, requiredSingleEdgeTarget(record, "templateDeclaration"))) {
      throw std::runtime_error(
          "AST JSON class instantiation changed its template declaration after "
          "construction");
    }
    if (decl->get_specializedTemplateDeclaration() !=
        nodeByIdAs<SgTemplateClassDeclaration>(
            nodes,
            requiredSingleEdgeTarget(record, "specializedTemplateDeclaration")))
      throw std::runtime_error(
          "AST JSON class instantiation changed its selected template after "
          "construction");
  }
  if (SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(node)) {
    if (decl->get_templateDeclaration() !=
            decl->get_specializedTemplateDeclaration() ||
        decl->get_templateDeclaration() !=
            nodeByIdAs<SgTemplateTypedefDeclaration>(
                nodes, requiredSingleEdgeTarget(record, "templateDeclaration")))
      throw std::runtime_error(
          "AST JSON template typedef instantiation changed its typed "
          "template declaration");
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes, decl,
        /*owns_children=*/true, /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
  }
  if (SgTemplateInstantiationFunctionDecl *decl =
          isSgTemplateInstantiationFunctionDecl(node)) {
    const uint64_t primary = singleEdgeTarget(record, "templateDeclaration");
    if (decl->get_templateDeclaration() !=
            decl->get_specializedTemplateDeclaration() ||
        decl->get_templateDeclaration() !=
            (primary != 0
                 ? nodeByIdAs<SgTemplateFunctionDeclaration>(nodes, primary)
                 : nullptr))
      throw std::runtime_error(
          "AST JSON function instantiation changed its typed template "
          "declaration");
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes, decl,
        /*owns_children=*/true, /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
    appendEdgeList<SgDeclarationStatementPtrList, SgDeclarationStatement>(
        decl->get_dependentTemplateCandidates(), record,
        "dependentTemplateCandidates", nodes, decl,
        /*owns_children=*/false, /*require_constructed_list=*/true);
  }
  if (SgTemplateInstantiationMemberFunctionDecl *decl =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    const uint64_t primary = singleEdgeTarget(record, "templateDeclaration");
    if (decl->get_templateDeclaration() !=
            decl->get_specializedTemplateDeclaration() ||
        decl->get_templateDeclaration() !=
            (primary != 0 ? nodeByIdAs<SgTemplateMemberFunctionDeclaration>(
                                nodes, primary)
                          : nullptr))
      throw std::runtime_error(
          "AST JSON member-function instantiation changed its typed "
          "template declaration");
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_templateArguments(), record, "templateArguments", nodes, decl,
        /*owns_children=*/true, /*require_constructed_list=*/true);
    appendEdgeList<SgTemplateArgumentPtrList, SgTemplateArgument>(
        decl->get_deducedTemplateArguments(), record,
        "deducedTemplateArguments", nodes, decl, /*owns_children=*/true,
        /*require_constructed_list=*/true);
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
    const uint64_t bounds_id =
        singleEdgeTarget(record, "cuda_launch_bounds_expression");
    auto *bounds =
        bounds_id == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, bounds_id);
    if (decl->get_cuda_launch_bounds_expression() != bounds ||
        (bounds != nullptr && bounds->get_parent() != decl))
      throw std::runtime_error(
          "AST JSON CUDA launch bounds differ from construction");
    decl->validate_cuda_launch_bounds_expression();
    if (uint64_t target = singleEdgeTarget(record, "parameterList")) {
      SgFunctionParameterList *params =
          nodeByIdAs<SgFunctionParameterList>(nodes, target);
      if (decl->get_parameterList() != params || params->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON function parameter list differs from construction");
      }
    }
    const uint64_t source_declarations =
        singleEdgeTarget(record, "parameterList_syntax");
    if (decl->get_parameterList_syntax() !=
        (source_declarations != 0
             ? nodeByIdAs<SgFunctionParameterList>(nodes, source_declarations)
             : nullptr))
      throw std::runtime_error(
          "AST JSON function source parameter list differs from construction");
    if (uint64_t target = singleEdgeTarget(record, "definition")) {
      SgFunctionDefinition *def =
          nodeByIdAs<SgFunctionDefinition>(nodes, target);
      ROSE_ASSERT(decl->get_definition() == def);
      ROSE_ASSERT(def->get_parent() == decl);
      ROSE_ASSERT(def->get_declaration() == decl);
    }
    if (uint64_t target = singleEdgeTarget(record, "functionParameterScope")) {
      if (decl->get_functionParameterScope() !=
          nodeByIdAs<SgFunctionParameterScope>(nodes, target))
        throw std::runtime_error(
            "AST JSON function parameter scope changed during edge linking");
    }
  }
  if (SgMemberFunctionDeclaration *decl = isSgMemberFunctionDeclaration(node)) {
    SgCtorInitializerList *ctors = nodeByIdAs<SgCtorInitializerList>(
        nodes, requiredSingleEdgeTarget(record, "CtorInitializerList"));
    if (decl->get_CtorInitializerList() != ctors || ctors->get_parent() != decl)
      throw std::runtime_error(
          "AST JSON member function lost its constructor-owned initializer "
          "list");
    if (decl->get_associatedClassDeclaration() !=
        nodeByIdAs<SgDeclarationStatement>(
            nodes,
            requiredSingleEdgeTarget(record, "associatedClassDeclaration")))
      throw std::runtime_error("AST JSON member function class owner differs "
                               "from construction");
  }
  if (SgFunctionDefinition *def = isSgFunctionDefinition(node)) {
    if (uint64_t target = singleEdgeTarget(record, "body")) {
      SgBasicBlock *body = nodeByIdAs<SgBasicBlock>(nodes, target);
      ROSE_ASSERT(def->get_body() == body);
      ROSE_ASSERT(body->get_parent() == def);
    }
  }
  if (SgPragmaDeclaration *decl = isSgPragmaDeclaration(node)) {
    SgPragma *pragma =
        nodeByIdAs<SgPragma>(nodes, requiredSingleEdgeTarget(record, "pragma"));
    if (decl->get_pragma() != pragma || pragma->get_parent() != decl)
      throw std::runtime_error(
          "AST JSON pragma declaration has no exact owned pragma");
  }
  if (SgExprStatement *stmt = isSgExprStatement(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    SgExpression *expression =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (stmt->get_expression() != expression ||
        (expression != nullptr && expression->get_parent() != stmt))
      throw std::runtime_error(
          "AST JSON expression statement lost its constructed expression");
  }
  if (SgOmpDepobjStatement *stmt = isSgOmpDepobjStatement(node)) {
    SgExpression *depobj = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "depobj"));
    if (stmt->get_depobj() != depobj || depobj->get_parent() != stmt)
      throw std::runtime_error(
          "AST JSON depobj statement lost its constructed operand");
  }
  if (SgReturnStmt *stmt = isSgReturnStmt(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    SgExpression *expression =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    const auto fields = returnStatementFieldsFromJson(record.properties);
    auto *candidate =
        fields.nrvo_candidate != 0
            ? nodeByIdAs<SgInitializedName>(nodes, fields.nrvo_candidate)
            : nullptr;
    if (stmt->get_expression() != expression ||
        stmt->get_nrvo_candidate() != candidate ||
        (expression != nullptr && expression->get_parent() != stmt))
      throw std::runtime_error(
          "AST JSON return statement lost its constructed expression");
  }
  if (SgBreakStmt *stmt = isSgBreakStmt(node)) {
    stmt->set_do_string_label(
        record.properties.requiredString("do_string_label"));
  }
  if (SgContinueStmt *stmt = isSgContinueStmt(node)) {
    stmt->set_do_string_label(
        record.properties.requiredString("do_string_label"));
  }
  if (auto *stmt = isSgFormTeamStatement(node)) {
    stmt->validate_operands();
  }
  if (auto *allocation = isSgAllocateStatement(node)) {
    SgExprListExp *list = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "expr_list"));
    if (allocation->get_expr_list() != list || list->get_parent() != allocation)
      throw std::runtime_error(
          "AST JSON allocation lost its constructed expression list");
    auto verify = [&](const char *field, SgExpression *actual) {
      const uint64_t id = singleEdgeTarget(record, field);
      SgExpression *expected =
          id != 0 ? nodeByIdAs<SgExpression>(nodes, id) : nullptr;
      if (actual != expected ||
          (actual != nullptr && actual->get_parent() != allocation))
        throw std::runtime_error("AST JSON allocation lost its constructed " +
                                 std::string(field));
    };
    verify("stat_expression", allocation->get_stat_expression());
    verify("errmsg_expression", allocation->get_errmsg_expression());
    verify("source_expression", allocation->get_source_expression());
    verify("mold_expression", allocation->get_mold_expression());
    verify("stream_expression", allocation->get_stream_expression());
    verify("pinned_expression", allocation->get_pinned_expression());
  }
  if (auto *deallocation = isSgDeallocateStatement(node)) {
    SgExprListExp *list = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "expr_list"));
    if (deallocation->get_expr_list() != list ||
        list->get_parent() != deallocation)
      throw std::runtime_error(
          "AST JSON deallocation lost its constructed expression list");
    auto verify = [&](const char *field, SgExpression *actual) {
      const uint64_t id = singleEdgeTarget(record, field);
      SgExpression *expected =
          id != 0 ? nodeByIdAs<SgExpression>(nodes, id) : nullptr;
      if (actual != expected ||
          (actual != nullptr && actual->get_parent() != deallocation))
        throw std::runtime_error("AST JSON deallocation lost its constructed " +
                                 std::string(field));
    };
    verify("stat_expression", deallocation->get_stat_expression());
    verify("errmsg_expression", deallocation->get_errmsg_expression());
  }
  if (SgNullifyStatement *stmt = isSgNullifyStatement(node)) {
    SgExprListExp *list = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "pointer_list"));
    if (stmt->get_pointer_list() != list || list->get_parent() != stmt)
      throw std::runtime_error(
          "AST JSON NULLIFY lost its constructed pointer list");
  }
  const auto restore_condition_value = [&](auto *stmt) {
    const uint64_t id = singleEdgeTarget(record, "condition_value");
    auto *value = id != 0 ? nodeByIdAs<SgExpression>(nodes, id) : nullptr;
    stmt->set_condition_value(value);
    if (value != nullptr)
      value->set_parent(stmt);
  };
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    const uint64_t false_body = singleEdgeTarget(record, "false_body");
    if (!stmt->get_else_construct_name().empty() &&
        (false_body == 0 ||
         isSgBasicBlock(nodeById(nodes, false_body)) == nullptr))
      throw std::runtime_error("AST JSON IF has an invalid named ELSE role");
    const uint64_t condition_id =
        stmt->get_is_if_consteval_statement()
            ? singleEdgeTarget(record, "conditional")
            : requiredSingleEdgeTarget(record, "conditional");
    auto *condition = condition_id != 0
                          ? nodeByIdAs<SgStatement>(nodes, condition_id)
                          : nullptr;
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "true_body"));
    stmt->set_conditional(condition);
    if (condition != nullptr)
      condition->set_parent(stmt);
    restore_condition_value(stmt);
    stmt->set_true_body(body);
    body->set_parent(stmt);
    if (false_body != 0) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, false_body);
      stmt->set_false_body(child);
      child->set_parent(stmt);
    }
    const uint64_t id = singleEdgeTarget(record, "else_numeric_label");
    SgLabelRefExp *expected =
        id != 0 ? nodeByIdAs<SgLabelRefExp>(nodes, id) : nullptr;
    if (stmt->get_else_numeric_label() != expected ||
        (expected != nullptr && expected->get_parent() != stmt))
      throw std::runtime_error("AST JSON IF lost its constructed ELSE label");
  }
  if (auto *stmt = isSgRangeBasedForStatement(node)) {
    for (const auto &[field, setter] :
         {std::pair{"iterator_declaration",
                    &SgRangeBasedForStatement::set_iterator_declaration},
          std::pair{"range_declaration",
                    &SgRangeBasedForStatement::set_range_declaration},
          std::pair{"begin_declaration",
                    &SgRangeBasedForStatement::set_begin_declaration},
          std::pair{"end_declaration",
                    &SgRangeBasedForStatement::set_end_declaration}}) {
      const uint64_t id = singleEdgeTarget(record, field);
      auto *child =
          id != 0 ? nodeByIdAs<SgVariableDeclaration>(nodes, id) : nullptr;
      (stmt->*setter)(child);
      if (child != nullptr)
        child->set_parent(stmt);
    }
    for (const auto &[field, setter] :
         {std::pair{"not_equal_expression",
                    &SgRangeBasedForStatement::set_not_equal_expression},
          std::pair{"increment_expression",
                    &SgRangeBasedForStatement::set_increment_expression}}) {
      const uint64_t id = singleEdgeTarget(record, field);
      auto *child = id != 0 ? nodeByIdAs<SgExpression>(nodes, id) : nullptr;
      (stmt->*setter)(child);
      if (child != nullptr)
        child->set_parent(stmt);
    }
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "loop_body"));
    stmt->set_loop_body(body);
    body->set_parent(stmt);
  }
  if (SgForStatement *stmt = isSgForStatement(node)) {
    auto *initializer = nodeByIdAs<SgForInitStatement>(
        nodes, requiredSingleEdgeTarget(record, "for_init_stmt"));
    if (stmt->get_for_init_stmt() != initializer ||
        initializer->get_parent() != stmt)
      throw std::runtime_error(
          "AST JSON FOR changed its constructed initializer");
    if (uint64_t target = singleEdgeTarget(record, "test")) {
      SgStatement *child = nodeByIdAs<SgStatement>(nodes, target);
      stmt->set_test(child);
      child->set_parent(stmt);
    }
    restore_condition_value(stmt);
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
    const uint64_t selector_id =
        singleEdgeTarget(record, "selector_expression");
    if (selector_id != 0 || stmt->get_selector_expression() != nullptr) {
      SgExpression *selector =
          selector_id != 0 ? nodeByIdAs<SgExpression>(nodes, selector_id)
                           : nullptr;
      if (selector == nullptr || stmt->get_selector_expression() != selector ||
          selector->get_parent() != stmt ||
          singleEdgeTarget(record, "label") != 0 ||
          singleEdgeTarget(record, "label_expression") != 0)
        throw std::runtime_error(
            "AST JSON computed goto changed its constructed selector");
    }
    if (uint64_t target = singleEdgeTarget(record, "label")) {
      stmt->set_label(nodeByIdAs<SgLabelStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "label_expression")) {
      SgLabelRefExp *label = nodeByIdAs<SgLabelRefExp>(nodes, target);
      stmt->set_label_expression(label);
      label->set_parent(stmt);
    }
  }
  if (auto *stmt = isSgAssociateStatement(node)) {
    auto *body = nodeByIdAs<SgBasicBlock>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    appendEdgeList<SgDeclarationStatementPtrList, SgVariableDeclaration>(
        stmt->get_associates(), record, "associates", nodes, stmt);
    if (stmt->get_associates().empty())
      throw std::runtime_error("AST JSON ASSOCIATE requires an association");
    stmt->set_body(body);
    body->set_parent(stmt);
  }
  if (isSgWhereStatement(node) || isSgElseWhereStatement(node)) {
    auto *where = isSgWhereStatement(node);
    auto *elsewhere = isSgElseWhereStatement(node);
    auto matches_child = [&](const char *field, SgNode *actual) {
      const uint64_t id = singleEdgeTarget(record, field);
      SgNode *expected = id != 0 ? nodeById(nodes, id) : nullptr;
      if (actual != expected ||
          (expected != nullptr && expected->get_parent() != node))
        throw std::runtime_error("AST JSON " + record.kind +
                                 " lost its constructed " + field);
    };
    matches_child("condition",
                  where ? where->get_condition() : elsewhere->get_condition());
    matches_child("body", where ? where->get_body() : elsewhere->get_body());
    matches_child("elsewhere",
                  where ? where->get_elsewhere() : elsewhere->get_elsewhere());
    if (where)
      matches_child("end_numeric_label", where->get_end_numeric_label());
  }
  if (auto *stmt = isSgForAllStatement(node)) {
    if (stmt->get_forall_header() !=
            nodeByIdAs<SgExprListExp>(
                nodes, requiredSingleEdgeTarget(record, "forall_header")) ||
        stmt->get_body() !=
            nodeByIdAs<SgBasicBlock>(
                nodes, requiredSingleEdgeTarget(record, "body")) ||
        stmt->get_end_numeric_label() !=
            (singleEdgeTarget(record, "end_numeric_label") != 0
                 ? nodeByIdAs<SgLabelRefExp>(
                       nodes, singleEdgeTarget(record, "end_numeric_label"))
                 : nullptr))
      throw std::runtime_error(
          "AST JSON FORALL lost its construction-time children");
  }
  if (SgFortranDo *stmt = isSgFortranDo(node)) {
    auto matches_child = [&](const char *field, SgNode *actual) {
      const uint64_t target = singleEdgeTarget(record, field);
      SgNode *expected = target != 0 ? nodeById(nodes, target) : nullptr;
      if (actual != expected ||
          (expected != nullptr && expected->get_parent() != stmt))
        throw std::runtime_error("AST JSON SgFortranDo lost its exact " +
                                 std::string(field) + " child");
    };
    matches_child("initialization", stmt->get_initialization());
    matches_child("bound", stmt->get_bound());
    matches_child("increment", stmt->get_increment());
    matches_child("body", stmt->get_body());
    matches_child("end_numeric_label", stmt->get_end_numeric_label());
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
    auto *condition = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "condition"));
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    stmt->set_condition(condition);
    condition->set_parent(stmt);
    restore_condition_value(stmt);
    stmt->set_body(body);
    body->set_parent(stmt);
  }
  if (SgDoWhileStmt *stmt = isSgDoWhileStmt(node)) {
    auto *condition = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "condition"));
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    stmt->set_condition(condition);
    condition->set_parent(stmt);
    stmt->set_body(body);
    body->set_parent(stmt);
  }
  if (SgSwitchStatement *stmt = isSgSwitchStatement(node)) {
    auto *condition = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "item_selector"));
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    stmt->set_item_selector(condition);
    condition->set_parent(stmt);
    restore_condition_value(stmt);
    stmt->set_body(body);
    body->set_parent(stmt);
  }
  if (SgCaseOptionStmt *stmt = isSgCaseOptionStmt(node)) {
    const uint64_t end = singleEdgeTarget(record, "key_range_end");
    auto *key = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "key"));
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    auto *range_end = end != 0 ? nodeByIdAs<SgExpression>(nodes, end) : nullptr;
    if (stmt->get_key() != key || stmt->get_body() != body ||
        stmt->get_key_range_end() != range_end || key->get_parent() != stmt ||
        body->get_parent() != stmt ||
        (range_end != nullptr && range_end->get_parent() != stmt) ||
        stmt->get_case_construct_name() !=
            record.properties.requiredString("case_construct_name"))
      throw std::runtime_error(
          "AST JSON CASE lost its construction-time children or name");
  }
  if (SgDefaultOptionStmt *stmt = isSgDefaultOptionStmt(node)) {
    auto *body = nodeByIdAs<SgStatement>(
        nodes, requiredSingleEdgeTarget(record, "body"));
    if (stmt->get_body() != body || body->get_parent() != stmt ||
        stmt->get_default_construct_name() !=
            record.properties.requiredString("default_construct_name"))
      throw std::runtime_error(
          "AST JSON DEFAULT lost its construction-time body or name");
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
    SgClassDeclaration *closure =
        nodeByIdAs<SgClassDeclaration>(nodes, closure_target);
    SgFunctionDeclaration *function =
        nodeByIdAs<SgFunctionDeclaration>(nodes, function_target);
    if (lambda->get_lambda_capture_list() != captures ||
        captures->get_parent() != lambda ||
        lambda->get_lambda_closure_class() != closure ||
        isSgAuxiliaryDeclarationList(closure->get_parent()) == nullptr ||
        lambda->get_lambda_function() != function ||
        isSgAuxiliaryDeclarationList(function->get_parent()) == nullptr) {
      throw std::runtime_error(
          "AST JSON SgLambdaExp lost its captures or canonical references");
    }
    const uint64_t scope_id = singleEdgeTarget(record, "closure_scope");
    auto *scope = scope_id != 0
                      ? nodeByIdAs<SgDeclarationScope>(nodes, scope_id)
                      : nullptr;
    if (lambda->get_closure_scope() != scope ||
        (scope != nullptr && scope->get_parent() != lambda))
      throw std::runtime_error("AST JSON lambda lost its declaration scope");
  }
  if (SgLambdaCaptureList *captures = isSgLambdaCaptureList(node)) {
    appendEdgeList<SgLambdaCapturePtrList, SgLambdaCapture>(
        captures->get_capture_list(), record, "capture_list", nodes, captures);
  }
  if (SgLambdaCapture *capture = isSgLambdaCapture(node)) {
    const uint64_t variable_target =
        requiredSingleEdgeTarget(record, "capture_variable");
    SgExpression *variable = nodeByIdAs<SgExpression>(nodes, variable_target);
    const uint64_t initializer_target =
        requiredSingleEdgeTarget(record, "capture_initializer");
    const uint64_t closure_target =
        singleEdgeTarget(record, "closure_variable");
    SgExpression *initializer =
        nodeByIdAs<SgExpression>(nodes, initializer_target);
    SgExpression *closure =
        closure_target != 0 ? nodeByIdAs<SgExpression>(nodes, closure_target)
                            : nullptr;
    if (capture->get_capture_variable() != variable ||
        variable->get_parent() != capture ||
        capture->get_capture_initializer() != initializer ||
        initializer->get_parent() != capture ||
        capture->get_closure_variable() != closure ||
        (closure != nullptr && closure->get_parent() != capture)) {
      throw std::runtime_error(
          "AST JSON SgLambdaCapture lost constructor-owned operands");
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
    appendEdgeList<SgStatementPtrList, SgCatchOptionStmt>(
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
    const uint64_t target = singleEdgeTarget(record, "body");
    SgStatement *body =
        target != 0 ? nodeByIdAs<SgStatement>(nodes, target) : nullptr;
    if (stmt->get_body() != body ||
        (body != nullptr && body->get_parent() != stmt))
      throw std::runtime_error(
          "AST JSON OpenMP body differs from construction");
  }
  if (SgStaticAssertionDeclaration *decl =
          isSgStaticAssertionDeclaration(node)) {
    const uint64_t condition_target =
        requiredSingleEdgeTarget(record, "condition");
    SgExpression *condition = nodeByIdAs<SgExpression>(nodes, condition_target);
    const uint64_t message_target = singleEdgeTarget(record, "message");
    SgExpression *message =
        message_target != 0 ? nodeByIdAs<SgExpression>(nodes, message_target)
                            : nullptr;
    if (decl->get_condition() != condition || condition->get_parent() != decl ||
        decl->get_message() != message ||
        (message != nullptr && message->get_parent() != decl)) {
      throw std::runtime_error(
          "AST JSON static assertion has inconsistent exact typed children");
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
    auto *list = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "initializers"));
    if (init->get_initializers() != list || list->get_parent() != init ||
        init->get_source_element_roles() !=
            aggregateSourceElementRolesFromJson(record.properties))
      throw std::runtime_error(
          "AST JSON aggregate lost its construction-time initializer list");
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
    const auto fields = functionCallSourceFieldsFromJson(record.properties);
    if (call->get_uses_operator_syntax() != fields.uses_operator_syntax ||
        call->get_source_syntax() != fields.syntax ||
        call->get_source_operator_surface() != fields.surface ||
        call->get_source_operator_callee_form() != fields.callee_form ||
        call->get_source_operator_operand_roles() != fields.operand_roles ||
        call->get_source_user_defined_literal_suffix() !=
            fields.literal_suffix ||
        call->get_source_user_defined_literal_suffix_roles() !=
            fields.suffix_roles ||
        call->get_source_user_defined_literal_was_modified() !=
            fields.literal_modified)
      throw std::runtime_error(
          "AST JSON call metadata differs from construction");
    auto *function = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "function"));
    auto *arguments = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "args"));
    const auto lexical_id =
        singleEdgeTarget(record, "source_user_defined_literal_operands");
    auto *lexical = lexical_id
                        ? nodeByIdAs<SgCxxLiteralTokenList>(nodes, lexical_id)
                        : nullptr;
    if (call->get_function() != function || function->get_parent() != call ||
        call->get_args() != arguments || arguments->get_parent() != call ||
        call->get_source_user_defined_literal_operands() != lexical ||
        (lexical != nullptr && lexical->get_parent() != call))
      throw std::runtime_error(
          "AST JSON call operands differ from construction");
  }
  if (SgCxxLiteralTokenList *lexical = isSgCxxLiteralTokenList(node)) {
    if (isSgFunctionCallExp(lexical->get_parent())) {
      const auto &edges = edgesFor(record, "tokens");
      if (edges.size() != lexical->get_tokens().size())
        throw std::runtime_error(
            "AST JSON call token count differs from construction");
      for (size_t i = 0; i < edges.size(); ++i)
        if (nodeById(nodes, edges[i].target) != lexical->get_tokens()[i] ||
            lexical->get_tokens()[i]->get_parent() != lexical)
          throw std::runtime_error(
              "AST JSON call token identity differs from construction");
    } else {
      appendEdgeList<SgCxxLiteralTokenPtrList, SgCxxLiteralToken>(
          lexical->get_tokens(), record, "tokens", nodes, lexical);
    }
  }
  if (SgAwaitExpression *await_expression = isSgAwaitExpression(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "value");
    SgExpression *operand = nodeByIdAs<SgExpression>(nodes, target);
    if (await_expression->get_value() != operand ||
        operand->get_parent() != await_expression) {
      throw std::runtime_error(
          "AST JSON SgAwaitExpression operand differs from construction");
    }
  }
  if (SgFoldExpression *fold = isSgFoldExpression(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "operands");
    SgExpression *operands = nodeByIdAs<SgExpression>(nodes, target);
    if (fold->get_operands() != operands || operands->get_parent() != fold) {
      throw std::runtime_error(
          "AST JSON SgFoldExpression operands differ from construction");
    }
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
    const auto &edges = edgesFor(record, "args");
    if (edges.size() != op->get_args().size())
      throw std::runtime_error(
          "AST JSON type-trait operand count differs from construction");
    for (size_t i = 0; i < edges.size(); ++i) {
      if (nodeById(nodes, edges[i].target) != op->get_args()[i] ||
          op->get_args()[i]->get_parent() != op)
        throw std::runtime_error(
            "AST JSON type-trait operand identity differs from construction");
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
      if (op->get_operand_i() != operand || operand->get_parent() != op) {
        throw std::runtime_error(
            "AST JSON unary operand differs from construction");
      }
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
    if (op->get_operand_expr() != operand || operand->get_parent() != op) {
      throw std::runtime_error(
          "AST JSON SgNoexceptOp operand does not name its exact owner");
    }
  }
  if (SgBinaryOp *op = isSgBinaryOp(node)) {
    SgExpression *lhs = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "lhs_operand_i"));
    SgExpression *rhs = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "rhs_operand_i"));
    if (op->get_lhs_operand_i() != lhs || op->get_rhs_operand_i() != rhs ||
        lhs == rhs || lhs->get_parent() != op || rhs->get_parent() != op) {
      throw std::runtime_error(
          "AST JSON binary operands differ from construction");
    }
  }
  if (auto *choice = isSgChooseExpression(node)) {
    auto *condition = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "condition"));
    auto *true_expression = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "true_expression"));
    auto *false_expression = nodeByIdAs<SgExpression>(
        nodes, requiredSingleEdgeTarget(record, "false_expression"));
    if (choice->get_condition() != condition ||
        choice->get_true_expression() != true_expression ||
        choice->get_false_expression() != false_expression)
      throw std::runtime_error(
          "AST JSON choose operands differ from construction");
    choice->validate();
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
    if (expr->get_conditional_exp() != condition ||
        expr->get_true_exp() != true_expression ||
        expr->get_false_exp() != false_expression)
      throw std::runtime_error(
          "AST JSON conditional operand identity differs from construction");
  }
  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    auto *variables = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "variables"));
    const uint64_t source_id = singleEdgeTarget(record, "source_variables");
    auto *source =
        source_id != 0 ? nodeByIdAs<SgExprListExp>(nodes, source_id) : nullptr;
    if (clause->get_variables() != variables ||
        variables->get_parent() != clause ||
        clause->get_source_variables() != source ||
        (source != nullptr && source->get_parent() != clause) ||
        clause->get_has_source_variables_override() !=
            record.properties.requiredBool("has_source_variables_override"))
      throw std::runtime_error(
          "AST JSON variable lists differ from construction");
    if (clause->get_has_source_variables_override() !=
        (clause->get_source_variables() != nullptr)) {
      throw std::runtime_error(
          "AST JSON OpenMP source-variable override discriminator and edge "
          "disagree");
    }
  }
  if (SgOmpExpressionClause *clause = isSgOmpExpressionClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    SgExpression *expr =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_expression() != expr ||
        (expr != nullptr && expr->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenMP expression differs from construction");
  }
  if (SgOmpDefaultClause *clause = isSgOmpDefaultClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "variant_directive");
    SgStatement *stmt =
        target != 0 ? nodeByIdAs<SgStatement>(nodes, target) : nullptr;
    if (clause->get_variant_directive() != stmt ||
        (stmt != nullptr && stmt->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenMP default variant differs from construction");
  }
  auto verify_reduction_identifier = [&](auto *clause) {
    const uint64_t target = singleEdgeTarget(record, "user_defined_identifier");
    auto *identifier =
        target != 0 ? nodeByIdAs<SgOmpNameExpression>(nodes, target) : nullptr;
    if (clause->get_user_defined_identifier() != identifier ||
        (identifier != nullptr && identifier->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON reduction identifier differs from construction");
  };
  if (auto *clause = isSgOmpReductionClause(node))
    verify_reduction_identifier(clause);
  if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "step");
    auto *step =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_step() != step ||
        (step != nullptr && step->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON linear step differs from construction");
  }
  if (SgOmpAlignedClause *clause = isSgOmpAlignedClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "alignment");
    auto *alignment =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_alignment() != alignment ||
        (alignment != nullptr && alignment->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON aligned operand differs from construction");
  }
  if (SgOmpScheduleClause *clause = isSgOmpScheduleClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "chunk_size");
    auto *chunk =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_chunk_size() != chunk ||
        (chunk != nullptr && chunk->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON schedule chunk differs from construction");
  }
  if (SgOmpDistScheduleClause *clause = isSgOmpDistScheduleClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "chunk_size");
    auto *chunk =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_chunk_size() != chunk ||
        (chunk != nullptr && chunk->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON dist_schedule chunk differs from construction");
  }
  if (SgOmpDestroyClause *clause = isSgOmpDestroyClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "expression");
    SgExpression *expression =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_expression() != expression ||
        (expression != nullptr && expression->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenMP destroy operand differs from construction");
  }
  if (SgOmpDoacrossClause *clause = isSgOmpDoacrossClause(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "expressions");
    SgExprListExp *expressions = nodeByIdAs<SgExprListExp>(nodes, target);
    if (clause->get_expressions() != expressions ||
        expressions->get_parent() != clause)
      throw std::runtime_error(
          "AST JSON OpenMP doacross expression list differs from construction");
  }
  if (SgOmpOtherwiseClause *clause = isSgOmpOtherwiseClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "variant_directive");
    SgStatement *directive =
        target != 0 ? nodeByIdAs<SgStatement>(nodes, target) : nullptr;
    if (clause->get_variant_directive() != directive ||
        (directive != nullptr && directive->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenMP otherwise variant differs from construction");
  }
  if (SgOmpInductionClause *clause = isSgOmpInductionClause(node)) {
    appendEdgeList<SgOmpInductionItemPtrList, SgOmpInductionItem>(
        clause->get_items(), record, "items", nodes, clause, true, true);
  }
  if (SgOmpApplyClause *clause = isSgOmpApplyClause(node)) {
    appendEdgeList<SgOmpApplyTransformationPtrList, SgOmpApplyTransformation>(
        clause->get_transformations(), record, "transformations", nodes, clause,
        true, true);
  }
  if (SgOmpInitModifierList *modifier_list = isSgOmpInitModifierList(node)) {
    appendEdgeList<SgOmpInitModifierPtrList, SgOmpInitModifier>(
        modifier_list->get_modifiers(), record, "modifiers", nodes,
        modifier_list, true, true);
  }
  if (SgOmpAppendArgsOperation *operation = isSgOmpAppendArgsOperation(node)) {
    const uint64_t modifier_list_target =
        requiredSingleEdgeTarget(record, "modifier_list");
    SgOmpInitModifierList *modifier_list =
        nodeByIdAs<SgOmpInitModifierList>(nodes, modifier_list_target);
    if (operation->get_modifier_list() != modifier_list ||
        modifier_list->get_parent() != operation)
      throw std::runtime_error("AST JSON OpenMP append operation lost its "
                               "constructor-owned modifiers");
  }
  if (SgOmpInitClause *clause = isSgOmpInitClause(node)) {
    const uint64_t modifier_list_target =
        requiredSingleEdgeTarget(record, "modifier_list");
    SgOmpInitModifierList *modifier_list =
        nodeByIdAs<SgOmpInitModifierList>(nodes, modifier_list_target);
    const uint64_t operand_target = requiredSingleEdgeTarget(record, "operand");
    SgExpression *operand = nodeByIdAs<SgExpression>(nodes, operand_target);
    if (clause->get_modifier_list() != modifier_list ||
        modifier_list->get_parent() != clause ||
        clause->get_operand() != operand || operand->get_parent() != clause)
      throw std::runtime_error(
          "AST JSON OpenMP init clause lost its constructor-owned operands");
  }
  if (SgOmpInductionItem *item = isSgOmpInductionItem(node)) {
    const uint64_t target = requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression = nodeByIdAs<SgExpression>(nodes, target);
    if (item->get_expression() != expression ||
        expression->get_parent() != item)
      throw std::runtime_error("AST JSON OpenMP induction item lost its "
                               "constructor-owned expression");
  }
  if (SgOmpMapItem *item = isSgOmpMapItem(node)) {
    const uint64_t expression_target =
        requiredSingleEdgeTarget(record, "expression");
    SgExpression *expression =
        nodeByIdAs<SgExpression>(nodes, expression_target);
    if (item->get_expression() != expression ||
        expression->get_parent() != item)
      throw std::runtime_error(
          "AST JSON OpenMP map item lost its constructor-owned locator");
    appendEdgeList<SgOmpMapDistDataPolicyPtrList, SgOmpMapDistDataPolicy>(
        item->get_policies(), record, "policies", nodes, item, true, true);
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
    const uint64_t modifier_id =
        singleEdgeTarget(record, "user_defined_modifier");
    auto *modifier = modifier_id != 0
                         ? nodeByIdAs<SgExpression>(nodes, modifier_id)
                         : nullptr;
    const uint64_t alignment_id = singleEdgeTarget(record, "alignment");
    auto *alignment = alignment_id != 0
                          ? nodeByIdAs<SgExpression>(nodes, alignment_id)
                          : nullptr;
    if (clause->get_user_defined_modifier() != modifier ||
        (modifier != nullptr && modifier->get_parent() != clause) ||
        clause->get_alignment() != alignment ||
        (alignment != nullptr && alignment->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON allocate operands differ from construction");
  }
  if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    const uint64_t target = singleEdgeTarget(record, "user_defined_modifier");
    auto *modifier =
        target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
    if (clause->get_user_defined_modifier() != modifier ||
        (modifier != nullptr && modifier->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON allocator operand differs from construction");
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    auto *arguments = nodeByIdAs<SgExprListExp>(
        nodes, requiredSingleEdgeTarget(record, "arguments"));
    if (clause->get_arguments() != arguments ||
        arguments->get_parent() != clause)
      throw std::runtime_error(
          "AST JSON adjust_args arguments differ from construction");
  }
  if (SgOmpAppendArgsClause *clause = isSgOmpAppendArgsClause(node)) {
    appendEdgeList<SgOmpAppendArgsOperationPtrList, SgOmpAppendArgsOperation>(
        clause->get_interop_operations(), record, "interop_operations", nodes,
        clause, true, true);
  }
  if (auto *clause = isSgOmpInReductionClause(node))
    verify_reduction_identifier(clause);
  if (auto *clause = isSgOmpTaskReductionClause(node))
    verify_reduction_identifier(clause);
  auto verify_transfer_payload = [&](auto *clause) {
    const uint64_t target = singleEdgeTarget(record, "mapper_identifier");
    auto *mapper =
        target != 0 ? nodeByIdAs<SgOmpNameExpression>(nodes, target) : nullptr;
    if (clause->get_mapper_identifier() != mapper ||
        (mapper != nullptr && mapper->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON transfer mapper differs from construction");
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause, true, true);
  };
  if (auto *clause = isSgOmpMapClause(node))
    verify_transfer_payload(clause);
  if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause, true, true);
    const uint64_t target = singleEdgeTarget(record, "sink_vectors");
    auto *vectors =
        target != 0 ? nodeByIdAs<SgExprListExp>(nodes, target) : nullptr;
    if (clause->get_sink_vectors() != vectors ||
        (vectors != nullptr && vectors->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON depend sink vectors differ from construction");
  }
  if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    appendEdgeList<SgOmpIteratorDefinitionPtrList, SgOmpIteratorDefinition>(
        clause->get_iterator_definitions(), record, "iterator_definitions",
        nodes, clause, true, true);
  }
  if (auto *clause = isSgOmpToClause(node))
    verify_transfer_payload(clause);
  if (auto *clause = isSgOmpFromClause(node))
    verify_transfer_payload(clause);
  if (SgOmpContextSelectorSet *set = isSgOmpContextSelectorSet(node)) {
    appendEdgeList<SgOmpContextSelectorPtrList, SgOmpContextSelector>(
        set->get_selectors(), record, "selectors", nodes, set, true, true);
    if (set->get_selectors().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpContextSelectorSet has no selectors");
    }
  }
  if (SgOmpContextSelectorProperty *property =
          isSgOmpContextSelectorProperty(node)) {
    for (const auto &[role, expression] :
         {std::pair{"expression", property->get_expression()},
          std::pair{"requires_expression",
                    property->get_requires_expression()}}) {
      const uint64_t target = singleEdgeTarget(record, role);
      auto *expected =
          target != 0 ? nodeByIdAs<SgExpression>(nodes, target) : nullptr;
      if (expression != expected ||
          (expression != nullptr && expression->get_parent() != property))
        throw std::runtime_error(
            "AST JSON context property operand differs from construction");
    }
  }
  if (SgOmpContextSelector *selector = isSgOmpContextSelector(node)) {
    for (const auto &[role, child] :
         {std::pair{"score", static_cast<SgNode *>(selector->get_score())},
          std::pair{
              "construct_directive",
              static_cast<SgNode *>(selector->get_construct_directive())}}) {
      const uint64_t target = singleEdgeTarget(record, role);
      auto *expected = target != 0 ? nodeById(nodes, target) : nullptr;
      if (child != expected ||
          (child != nullptr && child->get_parent() != selector))
        throw std::runtime_error(
            "AST JSON context selector operand differs from construction");
    }
    appendEdgeList<SgOmpContextSelectorPropertyPtrList,
                   SgOmpContextSelectorProperty>(selector->get_properties(),
                                                 record, "properties", nodes,
                                                 selector, true, true);
  }
  if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    appendEdgeList<SgOmpContextSelectorSetPtrList, SgOmpContextSelectorSet>(
        clause->get_context_selector_sets(), record, "context_selector_sets",
        nodes, clause, true, true);
    if (clause->get_context_selector_sets().empty()) {
      throw std::runtime_error(
          "AST JSON SgOmpWhenClause has no context selector sets");
    }
    const uint64_t target = singleEdgeTarget(record, "variant_directive");
    SgStatement *stmt =
        target != 0 ? nodeByIdAs<SgStatement>(nodes, target) : nullptr;
    if (clause->get_variant_directive() != stmt ||
        (stmt != nullptr && stmt->get_parent() != clause))
      throw std::runtime_error(
          "AST JSON OpenMP when variant differs from construction");
  }
  if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    appendEdgeList<SgOmpContextSelectorSetPtrList, SgOmpContextSelectorSet>(
        clause->get_context_selector_sets(), record, "context_selector_sets",
        nodes, clause, true, true);
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
    auto require_child = [&](const char *field, SgExpression *actual) {
      const auto id = singleEdgeTarget(record, field);
      auto *expected = id == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, id);
      if (actual != expected ||
          (actual != nullptr && actual->get_parent() != expr))
        throw std::runtime_error(
            "AST JSON new-expression operand differs from construction");
    };
    require_child("placement_args", expr->get_placement_args());
    require_child("constructor_args", expr->get_constructor_args());
    require_child("builtin_args", expr->get_builtin_args());
    require_child("array_bound", expr->get_array_bound());
    const auto declaration_id =
        singleEdgeTarget(record, "newOperatorDeclaration");
    auto *declaration =
        declaration_id == 0
            ? nullptr
            : nodeByIdAs<SgFunctionDeclaration>(nodes, declaration_id);
    if (expr->get_newOperatorDeclaration() != declaration)
      throw std::runtime_error("AST JSON new-expression operator declaration "
                               "differs from construction");
  }
  if (SgPackExpansionExpr *expr = isSgPackExpansionExpr(node)) {
    const uint64_t target =
        requiredSingleEdgeTarget(record, "pattern_expression");
    SgExpression *pattern = nodeByIdAs<SgExpression>(nodes, target);
    if (expr->get_pattern_expression() != pattern ||
        pattern->get_parent() != expr) {
      throw std::runtime_error(
          "AST JSON SgPackExpansionExpr pattern differs from construction");
    }
  }
  if (SgNonrealRefExp *expr = isSgNonrealRefExp(node)) {
    const auto name_kind = record.properties.requiredInt("name_kind");
    const uint64_t target = singleEdgeTarget(record, "conversion_type_id");
    if ((name_kind != SgNonrealRefExp::e_ordinary_name &&
         name_kind != SgNonrealRefExp::e_conversion_function_id &&
         name_kind != SgNonrealRefExp::e_destructor_id) ||
        (name_kind == SgNonrealRefExp::e_conversion_function_id) !=
            (target != 0))
      throw std::runtime_error(
          "AST JSON nonreal reference has inconsistent typed name");
    auto *type_id =
        target == 0 ? nullptr : nodeByIdAs<SgTypeExpression>(nodes, target);
    if (type_id != nullptr && type_id->get_parent() != nullptr &&
        type_id->get_parent() != expr)
      throw std::runtime_error(
          "AST JSON conversion type-id has a foreign owner");
    if (expr->get_name_kind() != name_kind ||
        expr->get_conversion_type_id() != type_id ||
        (type_id != nullptr && type_id->get_parent() != expr))
      throw std::runtime_error(
          "AST JSON nonreal conversion operand differs from construction");
    const auto &arguments = expr->get_templateArguments();
    const auto edges = edgesFor(record, "templateArguments");
    if (arguments.size() != edges.size())
      throw std::runtime_error(
          "AST JSON nonreal reference arguments differ from construction");
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (arguments[i] !=
              nodeByIdAs<SgTemplateArgument>(nodes, edges[i].target) ||
          arguments[i]->get_parent() != expr)
        throw std::runtime_error(
            "AST JSON nonreal reference arguments differ from construction");
    }
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
      const std::array<SgOmpClause::omp_map_modifier_enum, 3> modifiers = {
          clause->get_modifier1(), clause->get_modifier2(),
          clause->get_modifier3()};
      Rose::OpenMP::Detail::MapModifierFields modifier_fields;
      std::string modifier_error;
      if (!Rose::OpenMP::Detail::validateMapModifiers(
              modifiers, modifier_fields, &modifier_error))
        fail(clause, modifier_error);
      validate_name(clause, clause->get_mapper_identifier(),
                    modifier_fields.mapper, "map mapper");
      validate_iterators(clause, clause->get_iterator_definitions(),
                         modifier_fields.iterator);
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

SgScopeStatement *nearestScope(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(current)) {
      return scope;
    }
  }
  return nullptr;
}

} // namespace AstJson
} // namespace Rose
