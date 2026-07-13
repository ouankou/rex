#include "sageAstJsonPrivate.h"

#include <stdexcept>
#include <string>

namespace {

std::string astJson(const std::string &properties,
                    const std::string &schema_version) {
  return "{\n"
         "  \"format\": \"rex-sage-ast-json\",\n"
         "  \"schema_version\": " +
         schema_version +
         ",\n"
         "  \"root_id\": 1,\n"
         "  \"root_kind\": \"SgSourceFile\",\n"
         "  \"node_count\": 1,\n"
         "  \"metadata\": {\"checkpoint\": \"required-fields\"},\n"
         "  \"nodes\": [{\n"
         "    \"id\": 1,\n"
         "    \"kind\": \"SgSourceFile\",\n"
         "    \"variant\": 0,\n"
         "    \"flags\": {},\n"
         "    \"location\": {},\n"
         "    \"properties\": " +
         properties +
         ",\n"
         "    \"edges\": []\n"
         "  }]\n"
         "}\n";
}

void requireFailure(const std::string &json, const std::string &message) {
  try {
    (void)Rose::AstJson::parseAstFileJson(json, "required-fields");
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(message) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("malformed AST JSON was accepted");
}

void requireRemovedQualifiedNameStateRejected() {
  const Rose::AstJson::JsonValue properties = Rose::AstJson::parseJson(
      R"({"qualified_name_state":{"type_name":"legacy::rendered<int>"}})");
  try {
    Rose::AstJson::rejectRemovedQualifiedNameState(properties);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find("removed qualified_name_state") !=
        std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error(
      "removed AST JSON qualified_name_state was accepted");
}

void requireInvalidVisibilityRejected(int value) {
  const Rose::AstJson::JsonValue properties = Rose::AstJson::parseJson(
      "{\"visibility\":" + std::to_string(value) + "}");
  try {
    (void)Rose::AstJson::requiredGnuDeclarationVisibility(
        properties, "visibility", "required-fields test");
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find("not a valid source visibility") !=
        std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("invalid AST JSON visibility was accepted");
}

void requireNodeEnumFailure(const std::string &kind,
                            const std::string &properties,
                            const std::string &message) {
  Rose::AstJson::NodeRecord record;
  record.kind = kind;
  record.properties = Rose::AstJson::parseJson(properties);
  try {
    (void)Rose::AstJson::createNodeFromRecord(
        record, nullptr, Rose::AstJson::JsonValue::objectValue({}));
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(message) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("invalid AST JSON node enum was accepted");
}

std::string withValidStatementProperties(const std::string &members) {
  return "{" + (members.empty() ? "" : members + ",") +
         "\"source_sequence_value\":0," + "\"directive_end_kind\":" +
         std::to_string(
             static_cast<int>(SgStatement::e_directive_end_not_applicable)) +
         ",\"omp_fortran_spelling\":" +
         std::to_string(static_cast<int>(
             SgStatement::e_omp_fortran_spelling_not_applicable)) +
         "}";
}

std::string withValidFortranPragmaProperties(const std::string &members) {
  return "{\"fortran_directive_family\":0,"
         "\"fortran_directive_group_id\":\"\","
         "\"fortran_directive_member_index\":0,"
         "\"fortran_directive_member_count\":0,"
         "\"fortran_directive_primary\":false,"
         "\"fortran_directive_raw_text\":\"\","
         "\"fortran_directive_logical_text\":\"\","
         "\"fortran_directive_semantic_text\":\"\"" +
         (members.empty() ? "" : "," + members) + "}";
}

void requireFunctionCallPropertyFailure(const std::string &properties,
                                        const std::string &message) {
  Rose::AstJson::NodeRecord record;
  record.id = 1;
  record.kind = "SgFunctionCallExp";
  record.properties = Rose::AstJson::parseJson(properties);
  SgFunctionCallExp *call = new SgFunctionCallExp(
      static_cast<SgExpression *>(nullptr),
      static_cast<SgExprListExp *>(nullptr), SageBuilder::buildUnknownType());
  Rose::AstJson::NodeMap nodes;
  nodes.emplace(record.id, call);
  try {
    Rose::AstJson::linkNodeEdges(record, nodes);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(message) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error(
      "malformed AST JSON function-call metadata was accepted");
}

template <typename Enum>
void requireEnumFailure(const std::string &field, const std::string &context,
                        std::initializer_list<Enum> legal_values) {
  const Rose::AstJson::JsonValue properties =
      Rose::AstJson::parseJson("{\"" + field + "\":9223372036854775807}");
  try {
    (void)Rose::AstJson::requiredEnum<Enum>(properties, field, context,
                                            legal_values);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find("has an invalid " + field) !=
        std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("invalid AST JSON enum was accepted");
}

template <typename Enum>
void requireEnumMissing(const std::string &field, const std::string &context,
                        std::initializer_list<Enum> legal_values) {
  const Rose::AstJson::JsonValue properties = Rose::AstJson::parseJson("{}");
  try {
    (void)Rose::AstJson::requiredEnum<Enum>(properties, field, context,
                                            legal_values);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find("missing key: " + field) !=
        std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("missing AST JSON enum was accepted");
}

template <typename Enum>
void requireEnumValue(const std::string &field, const std::string &context,
                      Enum value, std::initializer_list<Enum> legal_values) {
  const Rose::AstJson::JsonValue properties = Rose::AstJson::parseJson(
      "{\"" + field + "\":" + std::to_string(static_cast<int>(value)) + "}");
  if (Rose::AstJson::requiredEnum<Enum>(properties, field, context,
                                        legal_values) != value) {
    throw std::runtime_error("valid AST JSON enum value was not preserved");
  }
}

void requireAttributeFailure(const std::string &properties,
                             const std::string &message) {
  Rose::AstJson::NodeRecord record;
  record.properties = Rose::AstJson::parseJson(properties);
  SgNullStatement *node = SageBuilder::buildNullStatement();
  try {
    Rose::AstJson::attachAstAttributes(node, record);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(message) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("malformed AST JSON attributes were accepted");
}

} // namespace

int main() {
  if (Rose::AstJson::kSchemaVersion != 78) {
    throw std::runtime_error("update the required-fields schema contract");
  }
  const std::string schema = std::to_string(Rose::AstJson::kSchemaVersion);
  const Rose::AstJson::AstFileRecord valid = Rose::AstJson::parseAstFileJson(
      astJson("{\"preprocessing\": []}", schema), "required-fields");
  if (valid.nodes.size() != 1) {
    throw std::runtime_error("valid AST JSON did not parse exactly one node");
  }
  const std::optional<unsigned int> absent_source_order =
      Rose::AstJson::requiredTranslationUnitSourceOrder(
          Rose::AstJson::parseJson(R"({"translation_unit_source_order":null})"),
          "required-fields null order");
  const std::optional<unsigned int> present_source_order =
      Rose::AstJson::requiredTranslationUnitSourceOrder(
          Rose::AstJson::parseJson(R"({"translation_unit_source_order":17})"),
          "required-fields present order");
  if (absent_source_order.has_value() || !present_source_order.has_value() ||
      *present_source_order != 17) {
    throw std::runtime_error(
        "typed translation-unit source order was not preserved");
  }
  for (const std::string &invalid_integer : {"17.5", "1e2"}) {
    try {
      (void)Rose::AstJson::requiredTranslationUnitSourceOrder(
          Rose::AstJson::parseJson(
              "{\"translation_unit_source_order\":" + invalid_integer + "}"),
          "required-fields noninteger order");
    } catch (const std::runtime_error &error) {
      if (std::string(error.what()).find("not an integer") !=
          std::string::npos) {
        continue;
      }
      throw;
    }
    throw std::runtime_error(
        "noninteger AST JSON translation-unit source order was accepted");
  }

  requireFailure(astJson("{}", schema), "preprocessing");
  requireFailure(astJson("{\"preprocessing\": []}", "null"), "not a number");
  requireFailure(astJson("{\"preprocessing\": []}",
                         std::to_string(Rose::AstJson::kSchemaVersion - 1)),
                 "schema_version is unsupported");
  requireRemovedQualifiedNameStateRejected();
  const Rose::AstJson::JsonValue valid_visibility =
      Rose::AstJson::parseJson("{\"visibility\":3}");
  if (Rose::AstJson::requiredGnuDeclarationVisibility(
          valid_visibility, "visibility", "required-fields test") !=
      SgDeclarationModifier::e_hidden_visibility) {
    throw std::runtime_error("valid AST JSON visibility was not preserved");
  }
  requireInvalidVisibilityRejected(SgDeclarationModifier::e_unknown_visibility);
  requireInvalidVisibilityRejected(SgDeclarationModifier::e_error_visibility);
  requireInvalidVisibilityRejected(
      SgDeclarationModifier::e_last_visibility_attribute);
  requireNodeEnumFailure("SgProcessControlStatement",
                         "{\"control_kind\":9223372036854775807}",
                         "SgProcessControlStatement has an invalid "
                         "control_kind");
  requireNodeEnumFailure(
      "SgClassDeclaration",
      "{\"name\":\"Malformed\",\"class_type\":9223372036854775807}",
      "SgClassDeclaration has an invalid class_type");
  requireNodeEnumFailure("SgAccessLabelStatement", "{}",
                         "missing key: access_label_kind");
  requireNodeEnumFailure(
      "SgAccessLabelStatement", "{\"access_label_kind\":9223372036854775807}",
      "SgAccessLabelStatement has an invalid access_label_kind");
  requireNodeEnumFailure("SgEmptyDeclaration", "{}",
                         "missing key: empty_declaration_lexical_role");
  requireNodeEnumFailure(
      "SgEmptyDeclaration",
      "{\"empty_declaration_lexical_role\":9223372036854775807}",
      "SgEmptyDeclaration has an invalid empty_declaration_lexical_role");
  requireNodeEnumFailure(
      "SgEmptyDeclaration",
      withValidStatementProperties("\"empty_declaration_lexical_role\":0"),
      "missing key: translation_unit_source_order");
  requireNodeEnumFailure(
      "SgEmptyDeclaration",
      withValidStatementProperties("\"empty_declaration_lexical_role\":0,"
                                   "\"translation_unit_source_order\":-1"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure(
      "SgEmptyDeclaration",
      withValidStatementProperties("\"empty_declaration_lexical_role\":0,"
                                   "\"translation_unit_source_order\":0"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure(
      "SgEmptyDeclaration",
      withValidStatementProperties(
          "\"empty_declaration_lexical_role\":0,"
          "\"translation_unit_source_order\":4294967296"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure("SgUsingDeclarationStatement", "{}",
                         "missing key: source_terminal_name");
  requireNodeEnumFailure("SgUsingDeclarationStatement",
                         "{\"source_terminal_name\":\"\"}",
                         "missing key: is_inheriting_constructor");
  requireNodeEnumFailure("SgNamespaceSourceFragment",
                         "{\"namespace_source_fragment_kind\":1}",
                         "missing key: namespace_source_fragment_form");
  requireNodeEnumFailure(
      "SgNamespaceSourceFragment",
      "{\"namespace_source_fragment_kind\":1,"
      "\"namespace_source_fragment_form\":9223372036854775807}",
      "SgNamespaceSourceFragment has an invalid "
      "namespace_source_fragment_form");
  requireNodeEnumFailure(
      "SgNamespaceDeclarationStatement",
      withValidStatementProperties("\"name\":\"rex\","
                                   "\"is_unnamed_namespace\":false"),
      "missing key: translation_unit_source_order");
  requireNodeEnumFailure(
      "SgNamespaceDeclarationStatement",
      withValidStatementProperties(
          "\"name\":\"rex\",\"is_unnamed_namespace\":false,"
          "\"translation_unit_source_order\":-1"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure(
      "SgNamespaceDeclarationStatement",
      withValidStatementProperties(
          "\"name\":\"rex\",\"is_unnamed_namespace\":false,"
          "\"translation_unit_source_order\":0"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure(
      "SgNamespaceDeclarationStatement",
      withValidStatementProperties(
          "\"name\":\"rex\",\"is_unnamed_namespace\":false,"
          "\"translation_unit_source_order\":4294967296"),
      "SgDeclarationStatement has an invalid translation_unit_source_order");
  requireNodeEnumFailure("SgPragmaDeclaration", "{}",
                         "missing key: fortran_directive_family");
  requireNodeEnumFailure("SgPragmaDeclaration",
                         withValidFortranPragmaProperties(""),
                         "missing key: cxx_pragma_payload_kind");
  requireNodeEnumFailure(
      "SgPragmaDeclaration",
      withValidFortranPragmaProperties(
          "\"cxx_pragma_payload_kind\":9223372036854775807,"
          "\"cxx_source_text\":\"\","
          "\"cxx_top_level_macro_expansion\":false"),
      "SgPragmaDeclaration has an invalid cxx_pragma_payload_kind");
  requireNodeEnumFailure("SgCastExp", "{}", "missing key: cast_type");
  requireNodeEnumFailure("SgCastExp", "{\"cast_type\":9223372036854775807}",
                         "SgCastExp has an invalid cast_type");
  requireNodeEnumFailure("SgCastExp", "{\"cast_type\":1}",
                         "missing key: semantic_conversion_kind");
  requireNodeEnumFailure("SgCastExp",
                         "{\"cast_type\":1,\"semantic_conversion_kind\":0}",
                         "SgCastExp has an invalid semantic_conversion_kind");
  requireNodeEnumFailure("SgCastExp",
                         "{\"cast_type\":1,\"semantic_conversion_kind\":1}",
                         "missing key: value_category");
  requireNodeEnumFailure("SgCastExp",
                         "{\"cast_type\":1,\"semantic_conversion_kind\":1,"
                         "\"value_category\":9223372036854775807}",
                         "SgCastExp has an invalid value_category");
  requireNodeEnumFailure("SgCastExp",
                         "{\"cast_type\":1,\"semantic_conversion_kind\":1,"
                         "\"value_category\":3}",
                         "missing key: conversion_base_path");
  requireNodeEnumFailure("SgCastExp",
                         "{\"cast_type\":1,\"semantic_conversion_kind\":1,"
                         "\"value_category\":3,\"conversion_base_path\":{}}",
                         "conversion_base_path is not an array");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_callee_form":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "missing key: source_operator_surface");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":9223372036854775807,"source_operator_callee_form":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "invalid source operator surface");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "missing key: source_operator_callee_form");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":9223372036854775807,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "invalid source operator callee form");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":0,"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "missing key: source_operator_operand_roles");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":0,"source_operator_operand_roles":[3],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "invalid source operator operand role");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":0,"source_operator_operand_roles":[]})",
      "missing key: source_user_defined_literal_suffix");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":false,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":""})",
      "missing key: source_user_defined_literal_suffix_roles");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":true,"source_syntax":0,"source_operator_surface":0,"source_operator_callee_form":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"_rex","source_user_defined_literal_suffix_roles":[1]})",
      "inconsistent operator source metadata");
  requireFunctionCallPropertyFailure(
      "{\"uses_operator_syntax\":true,\"source_syntax\":0,"
      "\"source_operator_surface\":" +
          std::to_string(static_cast<int>(
              SgFunctionCallExp::e_user_defined_literal_surface)) +
          ",\"source_operator_callee_form\":" +
          std::to_string(static_cast<int>(
              SgFunctionCallExp::e_nonmember_operator_callee)) +
          ",\"source_operator_operand_roles\":[2],"
          "\"source_user_defined_literal_suffix\":\"_rex\","
          "\"source_user_defined_literal_suffix_roles\":[2]}",
      "invalid UDL suffix role");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":true,"source_syntax":0,"source_operator_surface":1,"source_operator_callee_form":0,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "inconsistent operator source metadata");
  requireFunctionCallPropertyFailure(
      R"({"uses_operator_syntax":true,"source_syntax":1,"source_operator_surface":1,"source_operator_callee_form":1,"source_operator_operand_roles":[],"source_user_defined_literal_suffix":"","source_user_defined_literal_suffix_roles":[]})",
      "inconsistent operator source metadata");
  requireEnumMissing<SgValueExp::literal_spelling_form_enum>(
      "literal_spelling_form", "SgValueExp",
      {SgValueExp::e_literal_source_spelled,
       SgValueExp::e_literal_canonical_generated});
  requireEnumFailure<SgValueExp::literal_spelling_form_enum>(
      "literal_spelling_form", "SgValueExp",
      {SgValueExp::e_literal_source_spelled,
       SgValueExp::e_literal_canonical_generated});
  requireNodeEnumFailure("SgOmpDefaultClause",
                         "{\"data_sharing\":9223372036854775807}",
                         "SgOmpDefaultClause has an invalid data_sharing");
  requireNodeEnumFailure("SgOmpAbsentClause", "{}",
                         "missing key: directive_kinds");
  requireNodeEnumFailure("SgOmpContainsClause", "{\"directive_kinds\":{}}",
                         "directive_kinds is not an array");
  requireNodeEnumFailure("SgOmpAbsentClause", "{\"directive_kinds\":[]}",
                         "has an empty directive_kinds list");
  requireNodeEnumFailure("SgOmpContainsClause", "{\"directive_kinds\":[0]}",
                         "has an invalid directive_kinds entry");
  requireNodeEnumFailure(
      "SgOmpAbsentClause",
      "{\"directive_kinds\":[" +
          std::to_string(SgOmpClause::e_omp_directive_kind_parallel) + "," +
          std::to_string(SgOmpClause::e_omp_directive_kind_parallel) + "]}",
      "has a duplicate directive_kinds entry");
  requireNodeEnumFailure("SgOmpParallelStatement",
                         withValidStatementProperties(""),
                         "missing key: source_form_is_combined");
  requireEnumFailure<SgStorageModifier::storage_modifier_enum>(
      "storage_modifier", "SgInitializedName",
      {SgStorageModifier::e_default, SgStorageModifier::e_static});
  requireEnumMissing<SgInitializedName::generated_variable_role_enum>(
      "generated_variable_role", "SgInitializedName",
      {SgInitializedName::e_generated_variable_none,
       SgInitializedName::e_generated_loop_tiling_index,
       SgInitializedName::e_generated_loop_tiling_increment});
  requireEnumFailure<SgInitializedName::generated_variable_role_enum>(
      "generated_variable_role", "SgInitializedName",
      {SgInitializedName::e_generated_variable_none,
       SgInitializedName::e_generated_loop_tiling_index,
       SgInitializedName::e_generated_loop_tiling_increment});
  const std::initializer_list<
      SgInitializedName::enum_constant_source_ownership_enum>
      enum_source_roles = {
          SgInitializedName::e_enum_constant_source_unclassified,
          SgInitializedName::e_enum_constant_source_body,
          SgInitializedName::e_enum_constant_source_external,
          SgInitializedName::e_enum_constant_semantic_only};
  requireEnumMissing<SgInitializedName::enum_constant_source_ownership_enum>(
      "enum_constant_source_ownership", "SgInitializedName", enum_source_roles);
  requireEnumFailure<SgInitializedName::enum_constant_source_ownership_enum>(
      "enum_constant_source_ownership", "SgInitializedName", enum_source_roles);
  for (SgInitializedName::enum_constant_source_ownership_enum role :
       enum_source_roles) {
    requireEnumValue("enum_constant_source_ownership", "SgInitializedName",
                     role, enum_source_roles);
  }
  const std::initializer_list<SgInitializedName::preinitialization_enum>
      preinitialization_roles = {SgInitializedName::e_unknown_preinitialization,
                                 SgInitializedName::e_virtual_base_class,
                                 SgInitializedName::e_nonvirtual_base_class,
                                 SgInitializedName::e_data_member,
                                 SgInitializedName::e_delegation_constructor};
  requireEnumMissing<SgInitializedName::preinitialization_enum>(
      "preinitialization", "SgInitializedName", preinitialization_roles);
  requireEnumFailure<SgInitializedName::preinitialization_enum>(
      "preinitialization", "SgInitializedName", preinitialization_roles);
  for (SgInitializedName::preinitialization_enum role :
       preinitialization_roles) {
    requireEnumValue("preinitialization", "SgInitializedName", role,
                     preinitialization_roles);
  }
  requireNodeEnumFailure("SgInitializedName", "{\"name\":\"field\"}",
                         "missing key: enum_constant_source_ownership");
  requireNodeEnumFailure(
      "SgInitializedName",
      "{\"name\":\"field\","
      "\"enum_constant_source_ownership\":9223372036854775807}",
      "SgInitializedName has an invalid enum_constant_source_ownership");
  requireNodeEnumFailure(
      "SgInitializedName",
      "{\"name\":\"field\",\"enum_constant_source_ownership\":" +
          std::to_string(static_cast<int>(
              SgInitializedName::e_enum_constant_source_unclassified)) +
          "}",
      "missing key: preinitialization");
  requireNodeEnumFailure(
      "SgInitializedName",
      "{\"name\":\"field\",\"enum_constant_source_ownership\":" +
          std::to_string(static_cast<int>(
              SgInitializedName::e_enum_constant_source_unclassified)) +
          ",\"preinitialization\":9223372036854775807}",
      "SgInitializedName has an invalid preinitialization");
  Rose::AstJson::NodeRecord valid_initialized_name_record;
  valid_initialized_name_record.kind = "SgInitializedName";
  valid_initialized_name_record.properties = Rose::AstJson::parseJson(
      "{\"name\":\"field\",\"enum_constant_source_ownership\":" +
      std::to_string(static_cast<int>(
          SgInitializedName::e_enum_constant_source_unclassified)) +
      ",\"preinitialization\":" +
      std::to_string(
          static_cast<int>(SgInitializedName::e_unknown_preinitialization)) +
      "}");
  SgInitializedName *valid_initialized_name =
      isSgInitializedName(Rose::AstJson::createNodeFromRecord(
          valid_initialized_name_record, nullptr,
          Rose::AstJson::JsonValue::objectValue({})));
  if (valid_initialized_name == nullptr ||
      valid_initialized_name->get_enum_constant_source_ownership() !=
          SgInitializedName::e_enum_constant_source_unclassified ||
      valid_initialized_name->get_preinitialization() !=
          SgInitializedName::e_unknown_preinitialization ||
      valid_initialized_name->get_fortran_source_type() != nullptr ||
      valid_initialized_name->get_cray_pointer_pointee() != nullptr ||
      valid_initialized_name->get_fortran_cray_pointer_pointee_shape() !=
          nullptr ||
      valid_initialized_name->get_is_predefined_identifier() ||
      valid_initialized_name->get_generated_variable_role() !=
          SgInitializedName::e_generated_variable_none ||
      valid_initialized_name->get_fortran_type_spec() !=
          SgInitializedName::e_fortran_type_spec_default ||
      !valid_initialized_name->get_fortran_procedure_interface().is_null() ||
      valid_initialized_name->get_fortran_separate_shape_declaration() !=
          nullptr ||
      valid_initialized_name->get_fortran_separate_pointer_declaration() !=
          nullptr ||
      valid_initialized_name->get_shapeDeferred() ||
      valid_initialized_name->get_source_type_qualification_present() ||
      valid_initialized_name->get_source_type_global_qualification() ||
      !valid_initialized_name->get_source_type_qualification_tokens().empty() ||
      valid_initialized_name->get_source_name_qualification_present() ||
      valid_initialized_name->get_source_name_global_qualification() ||
      !valid_initialized_name->get_source_name_qualification_tokens().empty()) {
    throw std::runtime_error(
        "valid AST JSON initialized-name schema defaults were not constructed");
  }
  requireEnumFailure<PreprocessingInfo::DirectiveType>(
      "directive", "preprocessing entry",
      {PreprocessingInfo::C_StyleComment,
       PreprocessingInfo::CpreprocessorPragmaDeclaration});
  requireEnumFailure<PreprocessingInfo::OutputPlacementType>(
      "output_placement", "preprocessing entry",
      {PreprocessingInfo::source_position,
       PreprocessingInfo::attached_output_boundary,
       PreprocessingInfo::attached_output_trailing_line});
  requireAttributeFailure("{}", "missing required attributes");
  requireAttributeFailure("{\"attributes\":{}}", "not an array");
  requireAttributeFailure(
      "{\"attributes\":[{\"name\":\"\",\"type\":\"AstIntAttribute\","
      "\"value\":0}]}",
      "empty name");
  requireAttributeFailure(
      "{\"attributes\":[{\"name\":\"duplicate\",\"type\":"
      "\"AstIntAttribute\",\"value\":0},{\"name\":\"duplicate\","
      "\"type\":\"AstIntAttribute\",\"value\":1}]}",
      "duplicated");
  requireAttributeFailure(
      "{\"attributes\":[{\"name\":\"large\",\"type\":"
      "\"AstIntAttribute\",\"value\":9223372036854775807}]}",
      "out of range");
  requireAttributeFailure("{\"attributes\":[{\"name\":\"legacy\",\"type\":"
                          "\"AstMarkerAttribute\"}]}",
                          "unsupported");
  return 0;
}
