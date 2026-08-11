#include "ompAstConstruction.h"
#include "sageAstJsonPrivate.h"
#include "tokenStreamMapping.h"

namespace Rose {
namespace AstJson {

namespace {

bool jsonValuesEqual(const JsonValue &lhs, const JsonValue &rhs) {
  if (lhs.kind != rhs.kind || lhs.bool_value != rhs.bool_value ||
      lhs.text != rhs.text || lhs.array.size() != rhs.array.size() ||
      lhs.object.size() != rhs.object.size()) {
    return false;
  }
  for (size_t index = 0; index < lhs.array.size(); ++index) {
    if (!jsonValuesEqual(lhs.array[index], rhs.array[index])) {
      return false;
    }
  }
  auto lhs_field = lhs.object.begin();
  auto rhs_field = rhs.object.begin();
  for (; lhs_field != lhs.object.end(); ++lhs_field, ++rhs_field) {
    if (lhs_field->first != rhs_field->first ||
        !jsonValuesEqual(lhs_field->second, rhs_field->second)) {
      return false;
    }
  }
  return true;
}

uint64_t requiredPositiveId(const JsonValue &value,
                            const std::string &context) {
  const int64_t raw_id = value.asInt();
  if (raw_id <= 0) {
    throw std::runtime_error("AST JSON " + context +
                             " must be a positive node ID");
  }
  return static_cast<uint64_t>(raw_id);
}

std::size_t requiredOpenMPProducerSize(const JsonValue &object,
                                       const std::string &field,
                                       const std::string &context,
                                       bool require_positive = false) {
  const int64_t raw = object.requiredInt(field);
  if (raw < 0 || (require_positive && raw == 0)) {
    throw std::runtime_error("AST JSON OpenMP " + context + " has invalid " +
                             field);
  }
  return static_cast<std::size_t>(raw);
}

SgNode *openMPProducerNodeFromJson(const JsonValue &object,
                                   const std::string &field,
                                   const NodeMap &nodes,
                                   const std::string &context) {
  const int64_t raw = object.requiredInt(field);
  if (raw < 0) {
    throw std::runtime_error("AST JSON OpenMP " + context + " has negative " +
                             field);
  }
  return raw != 0 ? nodeById(nodes, static_cast<uint64_t>(raw)) : nullptr;
}

OpenMPDirectiveKind openMPDirectiveKindFromJson(const JsonValue &object,
                                                const std::string &context,
                                                bool allow_unknown) {
  const int64_t raw = object.requiredInt("directive_kind");
  if (raw < 0 || raw > static_cast<int64_t>(OMPD_unknown) ||
      (!allow_unknown && raw == static_cast<int64_t>(OMPD_unknown))) {
    throw std::runtime_error("AST JSON OpenMP " + context +
                             " has an invalid directive kind");
  }
  return static_cast<OpenMPDirectiveKind>(raw);
}

OpenMPClauseKind openMPClauseKindFromJson(const JsonValue &object,
                                          const std::string &context) {
  const int64_t raw = object.requiredInt("clause_kind");
  if (raw < 0 || raw > static_cast<int64_t>(OMPC_unknown)) {
    throw std::runtime_error("AST JSON OpenMP " + context +
                             " has an invalid clause kind");
  }
  return static_cast<OpenMPClauseKind>(raw);
}

OpenMPExprParseMode openMPParseModeFromJson(const JsonValue &object,
                                            const std::string &context) {
  const int64_t raw = object.requiredInt("parse_mode");
  if (raw <= static_cast<int64_t>(OMP_EXPR_PARSE_none) ||
      raw > static_cast<int64_t>(OMP_EXPR_PARSE_verbatim)) {
    throw std::runtime_error("AST JSON OpenMP " + context +
                             " has an invalid expression parse mode");
  }
  return static_cast<OpenMPExprParseMode>(raw);
}

std::vector<OmpExactSubexpressionType>
openMPSubexpressionsFromJson(const JsonValue &json, const NodeMap &nodes,
                             const std::string &context) {
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON OpenMP " + context +
                             " subexpressions is not an array");
  }
  std::vector<OmpExactSubexpressionType> result;
  result.reserve(json.array.size());
  for (const JsonValue &entry : json.array) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error("AST JSON OpenMP " + context +
                               " subexpression is not an object");
    }
    const int64_t raw_kind = entry.requiredInt("kind");
    if (raw_kind <= static_cast<int64_t>(OmpExactSubexpressionKind::invalid) ||
        raw_kind >
            static_cast<int64_t>(OmpExactSubexpressionKind::string_literal)) {
      throw std::runtime_error("AST JSON OpenMP " + context +
                               " has an invalid subexpression kind");
    }
    result.emplace_back(static_cast<OmpExactSubexpressionKind>(raw_kind),
                        typeFromJson(entry.at("result_type"), nodes));
  }
  return result;
}

OpenACCCxxExactSemanticBindings
openACCCxxExactSemanticBindingsFromJson(const JsonValue &json,
                                        const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON OpenACC C/C++ semantic bindings is not an object");
  }
  const JsonValue &expressions = json.at("expressions");
  if (expressions.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON OpenACC C/C++ semantic expressions is not an array");
  }
  OpenACCCxxExactSemanticBindings::BindingSequence restored_expressions;
  restored_expressions.reserve(expressions.array.size());
  for (const JsonValue &expression : expressions.array) {
    if (expression.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON OpenACC C/C++ semantic expression is not an object");
    }
    const JsonValue &identifiers = expression.at("identifiers");
    if (identifiers.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON OpenACC C/C++ identifiers is not an array");
    }
    std::vector<OpenACCCxxExactSemanticBindings::Binding> restored_identifiers;
    restored_identifiers.reserve(identifiers.array.size());
    for (const JsonValue &identifier : identifiers.array) {
      if (identifier.kind != JsonValue::Kind::Object) {
        throw std::runtime_error(
            "AST JSON OpenACC C/C++ identifier is not an object");
      }
      const auto kind =
          requiredEnum<OpenACCCxxExactSemanticBindings::BindingKind>(
              identifier, "kind", "OpenACC C/C++ semantic identifier",
              {OpenACCCxxExactSemanticBindings::BindingKind::qualifier,
               OpenACCCxxExactSemanticBindings::BindingKind::value,
               OpenACCCxxExactSemanticBindings::BindingKind::current_this});
      SgNode *semantic_node =
          openMPProducerNodeFromJson(identifier, "semantic_node", nodes,
                                     "OpenACC C/C++ semantic identifier");
      SgSymbol *symbol =
          exactBoundSymbolFromJson(identifier.at("symbol"), nodes);
      restored_identifiers.emplace_back(identifier.requiredString("spelling"),
                                        kind, semantic_node, symbol);
    }
    restored_expressions.emplace_back(
        openMPParseModeFromJson(expression,
                                "OpenACC C/C++ semantic expression"),
        expression.requiredString("expression"),
        std::move(restored_identifiers),
        openMPSubexpressionsFromJson(expression.at("subexpressions"), nodes,
                                     "OpenACC C/C++ semantic expression"));
  }
  return OpenACCCxxExactSemanticBindings(std::move(restored_expressions));
}

OmpFortranExactSemanticBindings
openMPFortranSemanticBindingsFromJson(const JsonValue &json,
                                      const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON OpenMP Fortran semantic bindings is not an object");
  }
  const JsonValue &bindings = json.at("bindings");
  const JsonValue &expressions = json.at("expressions");
  if (bindings.kind != JsonValue::Kind::Array ||
      expressions.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON OpenMP Fortran semantic records are not arrays");
  }
  std::vector<OmpFortranExactSemanticBindings::Binding> restored_bindings;
  restored_bindings.reserve(bindings.array.size());
  for (const JsonValue &binding : bindings.array) {
    if (binding.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON OpenMP Fortran semantic binding is not an object");
    }
    const auto kind =
        requiredEnum<OmpFortranExactSemanticBindings::BindingKind>(
            binding, "kind", "Fortran semantic identifier",
            {OmpFortranExactSemanticBindings::BindingKind::value,
             OmpFortranExactSemanticBindings::BindingKind::common_block,
             OmpFortranExactSemanticBindings::BindingKind::directive_local,
             OmpFortranExactSemanticBindings::BindingKind::syntax_name,
             OmpFortranExactSemanticBindings::BindingKind::
                 directive_local_declaration});
    SgNode *semantic_node = openMPProducerNodeFromJson(
        binding, "semantic_node", nodes, "Fortran semantic identifier");
    SgSymbol *symbol = exactBoundSymbolFromJson(binding.at("symbol"), nodes);
    if (kind == OmpFortranExactSemanticBindings::BindingKind::value) {
      if (semantic_node != nullptr || symbol == nullptr) {
        throw std::runtime_error(
            "AST JSON OpenMP Fortran value binding has contradictory node "
            "and symbol identity");
      }
      semantic_node = symbol;
    }
    restored_bindings.emplace_back(
        requiredOpenMPProducerSize(binding, "source_offset",
                                   "Fortran semantic identifier"),
        requiredOpenMPProducerSize(binding, "source_size",
                                   "Fortran semantic identifier", true),
        binding.requiredString("spelling"),
        binding.requiredString("source_spelling"), kind, semantic_node, symbol,
        nullableTypeFromJson(binding.at("directive_local_type"), nodes));
  }

  std::vector<OmpFortranExactSemanticBindings::ExpressionTypes>
      restored_expressions;
  restored_expressions.reserve(expressions.array.size());
  for (const JsonValue &expression : expressions.array) {
    if (expression.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON OpenMP Fortran semantic expression is not an object");
    }
    restored_expressions.emplace_back(
        requiredOpenMPProducerSize(expression, "source_offset",
                                   "Fortran semantic expression"),
        requiredOpenMPProducerSize(expression, "source_size",
                                   "Fortran semantic expression", true),
        expression.requiredString("expression"),
        openMPSubexpressionsFromJson(expression.at("subexpressions"), nodes,
                                     "Fortran semantic expression"));
  }
  return OmpFortranExactSemanticBindings(
      requiredEnum<OmpFortranExactSemanticBindings::Producer>(
          json, "producer", "Fortran semantic producer",
          {OmpFortranExactSemanticBindings::Producer::flang_parse_tree,
           OmpFortranExactSemanticBindings::Producer::rex_typed_scope}),
      json.requiredString("directive_source"),
      typeFromJson(json.at("default_integer_type"), nodes),
      std::move(restored_bindings), std::move(restored_expressions));
}

void restoreOpenMPProducerSemanticRecords(const AstFileRecord &ast,
                                          const NodeMap &nodes) {
  SgSourceFile *source_file = isSgSourceFile(nodeById(nodes, ast.root_id));
  if (source_file == nullptr) {
    throw std::runtime_error(
        "AST JSON OpenMP producer records have no source-file root");
  }
  std::vector<std::pair<SgPragmaDeclaration *, OpenMPProducerSemanticRecords>>
      restored_records;
  for (const NodeRecord &record : ast.nodes) {
    SgPragmaDeclaration *pragma =
        isSgPragmaDeclaration(nodeById(nodes, record.id));
    if (pragma == nullptr) {
      continue;
    }
    const JsonValue &json =
        record.properties.at("openmp_producer_semantic_records");
    if (json.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON OpenMP producer semantic records is not an object");
    }
    OpenMPProducerSemanticRecords records;
    const JsonValue &exact = json.at("openacc_cxx_semantic_bindings");
    if (exact.kind != JsonValue::Kind::Null) {
      records.openacc_cxx_semantic_bindings.emplace(
          openACCCxxExactSemanticBindingsFromJson(exact, nodes));
    }
    const JsonValue &fortran = json.at("fortran_exact_semantic_bindings");
    if (fortran.kind != JsonValue::Kind::Null) {
      records.fortran_exact_semantic_bindings.emplace(
          openMPFortranSemanticBindingsFromJson(fortran, nodes));
    }
    if (records.openacc_cxx_semantic_bindings.has_value() &&
        records.fortran_exact_semantic_bindings.has_value()) {
      throw std::runtime_error(
          "AST JSON OpenMP pragma owns both C/C++ and Fortran semantic "
          "records");
    }
    if (!records.empty()) {
      restored_records.emplace_back(pragma, std::move(records));
    }
  }
  for (auto &entry : restored_records) {
    OmpSupport::registerOpenMPProducerSemanticRecords(source_file, entry.first,
                                                      std::move(entry.second));
  }
}

int requiredTokenBoundary(const JsonValue &object, const char *field) {
  const int64_t value = object.at(field).asInt();
  if (value < 0 || value > std::numeric_limits<int>::max()) {
    throw std::runtime_error("AST JSON token mapping field " +
                             std::string(field) +
                             " is not a nonnegative int boundary");
  }
  return static_cast<int>(value);
}

void validateHalfOpenTokenInterval(int begin, int end, size_t token_count,
                                   const char *name) {
  if (end < begin || static_cast<size_t>(end) > token_count) {
    std::ostringstream message;
    message << "AST JSON token mapping has invalid " << name << " interval ["
            << begin << "," << end << ") for " << token_count << " tokens";
    throw std::runtime_error(message.str());
  }
}

void restoreTokenMappings(SgSourceFile *file, const JsonValue &json,
                          const NodeMap &nodes) {
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON token mapping restoration requires a source file");
  }
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON token_mappings must be an object");
  }
  const JsonValue &mappings_json = json.at("mappings");
  const JsonValue &entries_json = json.at("entries");
  if (mappings_json.kind != JsonValue::Kind::Array ||
      entries_json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON token mapping records and entries must be arrays");
  }
  if (file->get_token_list().size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error(
        "AST JSON token list is too large for token mapping indices");
  }
  if (!file->get_tokenSubsequenceMap().empty()) {
    throw std::runtime_error(
        "AST JSON token mapping restoration requires a fresh source-file "
        "map");
  }

  struct RestoredMapping {
    std::unique_ptr<TokenStreamSequenceToNodeMapping> value;
    std::unordered_set<SgNode *> associated_nodes;
    bool referenced = false;
  };
  std::unordered_map<uint64_t, RestoredMapping> mappings;
  mappings.reserve(mappings_json.array.size());
  const size_t token_count = file->get_token_list().size();
  for (const JsonValue &mapping_json : mappings_json.array) {
    if (mapping_json.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON token mapping record must be an object");
    }
    const uint64_t mapping_id =
        requiredPositiveId(mapping_json.at("id"), "token mapping ID");
    const uint64_t owner_id =
        requiredPositiveId(mapping_json.at("node"), "token mapping owner");
    SgNode *owner = nodeById(nodes, owner_id);
    const int leading_begin =
        requiredTokenBoundary(mapping_json, "leading_whitespace_begin");
    const int leading_end =
        requiredTokenBoundary(mapping_json, "leading_whitespace_end");
    const int token_begin =
        requiredTokenBoundary(mapping_json, "token_subsequence_begin");
    const int token_end =
        requiredTokenBoundary(mapping_json, "token_subsequence_end");
    const int trailing_begin =
        requiredTokenBoundary(mapping_json, "trailing_whitespace_begin");
    const int trailing_end =
        requiredTokenBoundary(mapping_json, "trailing_whitespace_end");
    const int else_begin =
        requiredTokenBoundary(mapping_json, "else_whitespace_begin");
    const int else_end =
        requiredTokenBoundary(mapping_json, "else_whitespace_end");
    const TokenStreamHalfOpenInterval leading(leading_begin, leading_end);
    const TokenStreamHalfOpenInterval core(token_begin, token_end);
    const TokenStreamHalfOpenInterval trailing(trailing_begin, trailing_end);
    const TokenStreamHalfOpenInterval else_interval(else_begin, else_end);
    validateHalfOpenTokenInterval(leading.begin, leading.end, token_count,
                                  "leading-whitespace");
    validateHalfOpenTokenInterval(core.begin, core.end, token_count,
                                  "token-subsequence");
    validateHalfOpenTokenInterval(trailing.begin, trailing.end, token_count,
                                  "trailing-whitespace");
    validateHalfOpenTokenInterval(else_interval.begin, else_interval.end,
                                  token_count, "else-whitespace");
    if ((token_count != 0 && core.empty()) || leading.end != core.begin ||
        trailing.begin != core.end ||
        (!else_interval.empty() &&
         (else_interval.begin < core.begin || else_interval.end > core.end)) ||
        (else_interval.empty() && else_interval.begin != core.end)) {
      throw std::runtime_error(
          "AST JSON token mapping has inconsistent half-open interval "
          "ownership");
    }

    std::unique_ptr<TokenStreamSequenceToNodeMapping> mapping(
        TokenStreamSequenceToNodeMapping::createPublished(
            owner, leading, core, trailing, else_interval, token_count));
    mapping->shared = mapping_json.at("shared").asBool();
    mapping->constructedInEvaluationOfSynthesizedAttribute =
        mapping_json.at("constructed_in_synthesized_attribute").asBool();

    const JsonValue &node_vector = mapping_json.at("node_vector");
    if (node_vector.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON token mapping node_vector must be an array");
    }
    std::unordered_set<SgNode *> associated_nodes;
    for (const JsonValue &node_id_json : node_vector.array) {
      SgNode *associated = nodeById(
          nodes,
          requiredPositiveId(node_id_json, "token mapping node_vector entry"));
      if (!associated_nodes.insert(associated).second) {
        throw std::runtime_error(
            "AST JSON token mapping node_vector contains a duplicate node");
      }
      mapping->nodeVector.push_back(associated);
    }
    if (!mapping->nodeVector.empty() &&
        associated_nodes.find(owner) == associated_nodes.end()) {
      throw std::runtime_error(
          "AST JSON token mapping owner is absent from node_vector");
    }
    if (mapping->shared && mapping->nodeVector.size() < 2) {
      throw std::runtime_error(
          "AST JSON shared token mapping has fewer than two associated "
          "nodes");
    }
    RestoredMapping restored;
    restored.value = std::move(mapping);
    restored.associated_nodes = std::move(associated_nodes);
    if (!mappings.emplace(mapping_id, std::move(restored)).second) {
      throw std::runtime_error(
          "AST JSON token mappings contain a duplicate mapping ID");
    }
  }

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> restored_entries;
  for (const JsonValue &entry_json : entries_json.array) {
    if (entry_json.kind != JsonValue::Kind::Object) {
      throw std::runtime_error("AST JSON token map entry must be an object");
    }
    SgNode *node = nodeById(nodes, requiredPositiveId(entry_json.at("node"),
                                                      "token map entry node"));
    const uint64_t mapping_id = requiredPositiveId(
        entry_json.at("mapping"), "token map entry mapping ID");
    auto mapping_it = mappings.find(mapping_id);
    if (mapping_it == mappings.end()) {
      throw std::runtime_error(
          "AST JSON token map entry references an unknown mapping ID");
    }
    TokenStreamSequenceToNodeMapping *mapping = mapping_it->second.value.get();
    if (node != mapping->node &&
        mapping_it->second.associated_nodes.find(node) ==
            mapping_it->second.associated_nodes.end()) {
      throw std::runtime_error(
          "AST JSON token map entry is not associated with its mapping");
    }
    if (!restored_entries.emplace(node, mapping).second) {
      throw std::runtime_error(
          "AST JSON token map contains duplicate entries for one node");
    }
    mapping_it->second.referenced = true;
  }

  for (auto &entry : mappings) {
    if (!entry.second.referenced) {
      throw std::runtime_error(
          "AST JSON contains an unreferenced token mapping record");
    }
  }
  auto *published_entries =
      new std::map<SgNode *, TokenStreamSequenceToNodeMapping *>(
          std::move(restored_entries));
  file->set_tokenSubsequenceMap(published_entries);
  for (auto &entry : mappings) {
    entry.second.value.release();
  }
}

} // namespace

static void
restoreTemplateArgumentSemanticProperties(SgTemplateArgument *argument,
                                          const JsonValue &properties,
                                          const NodeMap &nodes) {
  if (argument == nullptr) {
    throw std::runtime_error(
        "AST JSON template-argument restoration requires an exact node");
  }
  argument->set_isArrayBoundUnknownType(
      properties.requiredBool("is_array_bound_unknown_type"));
  if (const JsonValue *type = properties.find("type")) {
    argument->set_type(nullableTypeFromJson(*type, nodes));
  }
  argument->set_sourceSpelledType(
      nullableTypeFromJson(properties.at("source_spelled_type"), nodes));
  argument->set_source_type_qualification_present(
      properties.requiredBool("source_type_qualification_present"));
  argument->set_source_type_global_qualification(
      properties.requiredBool("source_type_global_qualification"));
  argument->get_source_type_qualification_tokens() =
      stringListFromJson(properties.at("source_type_qualification_tokens"),
                         "source_type_qualification_tokens");
  if (const JsonValue *expr = properties.find("expression")) {
    SgExpression *expression = expressionFromRef(*expr, nodes);
    argument->set_expression(expression);
    if (expression != nullptr) {
      expression->set_parent(argument);
    }
  }
  if (uint64_t target = static_cast<uint64_t>(
          properties.requiredInt("template_declaration"))) {
    argument->set_templateDeclaration(
        nodeByIdAs<SgDeclarationStatement>(nodes, target));
  }
  if (uint64_t target =
          static_cast<uint64_t>(properties.requiredInt("initialized_name"))) {
    argument->set_initializedName(nodeByIdAs<SgInitializedName>(nodes, target));
  }
  argument->set_explicitlySpecified(
      properties.requiredBool("explicitly_specified"));
  argument->set_is_pack_element(properties.requiredBool("is_pack_element"));
}

static void restoreTemplateInstantiationClassProperties(
    SgTemplateInstantiationDecl *declaration, const JsonValue &properties,
    const NodeMap &nodes) {
  if (declaration == nullptr) {
    throw std::runtime_error(
        "AST JSON class-instantiation restoration requires an exact node");
  }
  const std::string template_name = properties.requiredString("template_name");
  if (template_name.empty()) {
    throw std::runtime_error(
        "AST JSON SgTemplateInstantiationDecl has an empty template_name");
  }
  declaration->set_templateName(SgName(template_name));
  declaration->set_templateHeader(
      SgName(properties.requiredString("template_header")));
  declaration->set_sourceSpellsInjectedClassName(
      properties.requiredBool("source_spells_injected_class_name"));
  declaration->set_constraintSatisfactionEvaluated(
      properties.requiredBool("constraint_satisfaction_evaluated"));
  declaration->set_constraintSatisfactionSatisfied(
      properties.requiredBool("constraint_satisfaction_satisfied"));
  declaration->set_constraintSatisfactionContainsErrors(
      properties.requiredBool("constraint_satisfaction_contains_errors"));
  declaration->set_constraintSatisfactionSubstitutionFailure(
      properties.requiredBool("constraint_satisfaction_substitution_failure"));
  declaration->set_constraintSatisfactionSummary(
      properties.requiredString("constraint_satisfaction_summary"));
  declaration->set_sfinaeEvaluated(properties.requiredBool("sfinae_evaluated"));
  declaration->set_sfinaeSubstitutionFailure(
      properties.requiredBool("sfinae_substitution_failure"));
  declaration->set_sfinaeSummary(properties.requiredString("sfinae_summary"));
  if (const JsonValue *template_arguments =
          properties.find("template_arguments")) {
    declaration->get_templateArguments() =
        templateArgumentListFromJson(*template_arguments, nodes, declaration);
  }
  declaration->get_semanticTemplateArguments() = templateArgumentListFromJson(
      properties.at("semantic_template_arguments"), nodes, declaration);
  if (const JsonValue *deduced_template_arguments =
          properties.find("deduced_template_arguments")) {
    declaration->get_deducedTemplateArguments() = templateArgumentListFromJson(
        *deduced_template_arguments, nodes, declaration);
  }
}

static void restoreTemplateInstantiationDefinitions(const AstFileRecord &ast,
                                                    NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateInstantiationDefn") {
      continue;
    }
    const uint64_t declaration_id = requiredSingleEdgeTarget(record, "parent");
    SgTemplateInstantiationDecl *declaration =
        nodeByIdAs<SgTemplateInstantiationDecl>(nodes, declaration_id);
    if (declaration->get_definition() != nullptr ||
        nodes.find(record.id) != nodes.end()) {
      throw std::runtime_error(
          "AST JSON template-instantiation definition was published twice");
    }
    SgTemplateInstantiationDefn *definition =
        new SgTemplateInstantiationDefn(declaration);
    if (definition->get_parent() != declaration ||
        declaration->get_definition() != definition) {
      throw std::runtime_error(
          "AST JSON template-instantiation definition did not publish its "
          "exact declaration owner");
    }
    setNodeSourcePosition(definition, record);
    setNodeFlags(definition, record);
    if (!nodes.emplace(record.id, definition).second) {
      throw std::runtime_error(
          "AST JSON template-instantiation definition ID was published "
          "twice");
    }
  }
}

static void
restoreTemplateInstantiationDefinitionDependentContext(const AstFileRecord &ast,
                                                       const NodeMap &nodes) {
  // Template-instantiation definitions are constructor-only scopes and do not
  // exist during the first source-context pass.  Publish every parent and
  // semantic-scope edge that becomes available with those definitions before
  // any delayed initializer reconstructs a type.  In particular, an
  // initialized anonymous enum declared in a suppressed instantiation needs
  // its exact SgTemplateInstantiationDefn scope before SgEnumType::createType
  // is permitted to compute the canonical identity.
  for (const NodeRecord &record : ast.nodes) {
    auto restored = nodes.find(record.id);
    if (restored == nodes.end()) {
      continue;
    }

    if (const uint64_t parent_id = singleEdgeTarget(record, "parent")) {
      auto parent = nodes.find(parent_id);
      if (parent != nodes.end()) {
        if (restored->second->get_parent() != nullptr &&
            restored->second->get_parent() != parent->second) {
          throw std::runtime_error(
              "AST JSON template-definition dependent has a conflicting "
              "construction-time parent");
        }
        restored->second->set_parent(parent->second);
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored = nodes.find(record.id);
    const uint64_t scope_id = singleEdgeTarget(record, "scope");
    auto scope = nodes.find(scope_id);
    if (restored == nodes.end() || scope_id == 0 || scope == nodes.end()) {
      continue;
    }
    SgScopeStatement *exact_scope = isSgScopeStatement(scope->second);
    if (exact_scope == nullptr) {
      throw std::runtime_error(
          "AST JSON template-definition dependent names a non-scope "
          "semantic owner");
    }

    if (SgInitializedName *name = isSgInitializedName(restored->second)) {
      if (name->get_scope() != nullptr && name->get_scope() != exact_scope) {
        throw std::runtime_error(
            "AST JSON template-definition initialized name has a "
            "conflicting semantic scope");
      }
      name->set_scope(exact_scope);
    }
    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(restored->second)) {
      if (declaration->get_scope() != nullptr &&
          declaration->get_scope() != exact_scope) {
        throw std::runtime_error(
            "AST JSON template-definition declaration has a conflicting "
            "semantic scope");
      }
      declaration->set_scope(exact_scope);
    }
    if (SgLabelStatement *label = isSgLabelStatement(restored->second)) {
      if (label->get_scope() != nullptr && label->get_scope() != exact_scope) {
        throw std::runtime_error(
            "AST JSON template-definition label has a conflicting semantic "
            "scope");
      }
      label->set_scope(exact_scope);
    }
  }
}

static void restoreNamespaceCanonicalIdentityEdges(const AstFileRecord &ast,
                                                   const NodeMap &nodes) {
  std::unordered_map<uint64_t, const NodeRecord *> records;
  records.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    if (!records.emplace(record.id, &record).second) {
      throw std::runtime_error(
          "AST JSON namespace identity pass found a duplicate node ID");
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(restored_node->second);
    if (declaration == nullptr) {
      continue;
    }

    const uint64_t definition_id =
        requiredSingleEdgeTarget(record, "definition");
    auto definition_record = records.find(definition_id);
    if (definition_record == records.end()) {
      throw std::runtime_error(
          "AST JSON namespace declaration references an unknown definition");
    }
    SgNamespaceDefinitionStatement *definition =
        nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, definition_id);
    const uint64_t inverse_declaration_id = requiredSingleEdgeTarget(
        *definition_record->second, "namespaceDeclaration");
    const uint64_t definition_parent_id =
        requiredSingleEdgeTarget(*definition_record->second, "parent");
    if (inverse_declaration_id != record.id ||
        definition_parent_id != record.id ||
        (declaration->get_definition() != nullptr &&
         declaration->get_definition() != definition) ||
        (definition->get_namespaceDeclaration() != nullptr &&
         definition->get_namespaceDeclaration() != declaration) ||
        definition->get_parent() != declaration) {
      throw std::runtime_error(
          "AST JSON namespace declaration/definition identity is not exact "
          "and bidirectional");
    }
    declaration->set_definition(definition);
    definition->set_namespaceDeclaration(declaration);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    SgNamespaceDefinitionStatement *definition =
        isSgNamespaceDefinitionStatement(restored_node->second);
    if (definition == nullptr) {
      continue;
    }

    const uint64_t declaration_id =
        requiredSingleEdgeTarget(record, "namespaceDeclaration");
    const uint64_t global_id =
        requiredSingleEdgeTarget(record, "global_definition");
    const uint64_t previous_id =
        singleEdgeTarget(record, "previousNamespaceDefinition");
    const uint64_t next_id =
        singleEdgeTarget(record, "nextNamespaceDefinition");
    SgNamespaceDeclarationStatement *declaration =
        nodeByIdAs<SgNamespaceDeclarationStatement>(nodes, declaration_id);
    SgNamespaceDefinitionStatement *global =
        nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, global_id);
    SgNamespaceDefinitionStatement *previous =
        previous_id != 0
            ? nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, previous_id)
            : nullptr;
    SgNamespaceDefinitionStatement *next =
        next_id != 0
            ? nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, next_id)
            : nullptr;
    if (definition->get_namespaceDeclaration() != declaration ||
        declaration->get_definition() != definition ||
        definition->get_parent() != declaration ||
        (definition->get_global_definition() != nullptr &&
         definition->get_global_definition() != global) ||
        (definition->get_previousNamespaceDefinition() != nullptr &&
         definition->get_previousNamespaceDefinition() != previous) ||
        (definition->get_nextNamespaceDefinition() != nullptr &&
         definition->get_nextNamespaceDefinition() != next)) {
      throw std::runtime_error(
          "AST JSON namespace definition has conflicting canonical identity "
          "edges");
    }
    definition->set_global_definition(global);
    definition->set_previousNamespaceDefinition(previous);
    definition->set_nextNamespaceDefinition(next);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    SgNamespaceDefinitionStatement *definition =
        isSgNamespaceDefinitionStatement(restored_node->second);
    if (definition == nullptr) {
      continue;
    }
    SgNamespaceDeclarationStatement *declaration =
        definition->get_namespaceDeclaration();
    SgNamespaceDefinitionStatement *global =
        definition->get_global_definition();
    SgNamespaceDeclarationStatement *global_declaration =
        global != nullptr ? global->get_namespaceDeclaration() : nullptr;
    SgNamespaceDefinitionStatement *previous =
        definition->get_previousNamespaceDefinition();
    SgNamespaceDefinitionStatement *next =
        definition->get_nextNamespaceDefinition();
    if (declaration == nullptr || declaration->get_definition() != definition ||
        definition->get_parent() != declaration || global == nullptr ||
        global_declaration == nullptr ||
        global->get_global_definition() != global ||
        global->get_previousNamespaceDefinition() != nullptr ||
        declaration->get_name().getString() !=
            global_declaration->get_name().getString() ||
        declaration->get_firstNondefiningDeclaration() !=
            global_declaration->get_firstNondefiningDeclaration() ||
        (previous != nullptr &&
         previous->get_nextNamespaceDefinition() != definition) ||
        (next != nullptr &&
         next->get_previousNamespaceDefinition() != definition)) {
      throw std::runtime_error(
          "AST JSON namespace canonical definition chain is malformed");
    }
  }
}

static void restoreClassTypeConstructionInputs(const AstFileRecord &ast,
                                               const NodeMap &nodes) {
  std::unordered_map<uint64_t, const NodeRecord *> records;
  std::unordered_map<SgNode *, uint64_t> node_ids;
  records.reserve(ast.nodes.size());
  node_ids.reserve(nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    records.emplace(record.id, &record);
  }
  for (const auto &[id, node] : nodes) {
    node_ids.emplace(node, id);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    SgClassDeclaration *declaration =
        isSgClassDeclaration(restored_node->second);
    const uint64_t definition_id = singleEdgeTarget(record, "definition");
    if (declaration == nullptr || definition_id == 0) {
      continue;
    }
    SgClassDefinition *definition =
        nodeByIdAs<SgClassDefinition>(nodes, definition_id);
    if ((declaration->get_definition() != nullptr &&
         declaration->get_definition() != definition) ||
        (definition->get_declaration() != nullptr &&
         definition->get_declaration() != declaration) ||
        definition->get_parent() != declaration) {
      throw std::runtime_error(
          "AST JSON class definition disagrees with its canonical typed "
          "owner");
    }
    declaration->set_definition(definition);
    definition->set_declaration(declaration);
  }

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    SgNode *node = restored_node->second;
    if (SgTemplateClassDeclaration *declaration =
            isSgTemplateClassDeclaration(node)) {
      const std::string template_name =
          record.properties.requiredString("template_name");
      if (template_name.empty()) {
        throw std::runtime_error(
            "AST JSON SgTemplateClassDeclaration has an empty "
            "template_name");
      }
      declaration->set_templateName(SgName(template_name));
      if (!declaration->get_templateParameters().empty() ||
          !declaration->get_templateSpecializationArguments().empty()) {
        throw std::runtime_error(
            "AST JSON fresh template class already owns canonical identity "
            "operands");
      }
      for (const EdgeRecord &edge : edgesFor(record, "templateParameters")) {
        SgTemplateParameter *parameter =
            nodeByIdAs<SgTemplateParameter>(nodes, edge.target);
        if (parameter->get_parent() != declaration) {
          throw std::runtime_error(
              "AST JSON template class parameter has no exact declaration "
              "owner");
        }
        declaration->get_templateParameters().push_back(parameter);
      }
      for (const EdgeRecord &edge :
           edgesFor(record, "templateSpecializationArguments")) {
        SgTemplateArgument *argument =
            nodeByIdAs<SgTemplateArgument>(nodes, edge.target);
        if (argument->get_parent() != declaration) {
          throw std::runtime_error(
              "AST JSON template class specialization argument has no exact "
              "declaration owner");
        }
        declaration->get_templateSpecializationArguments().push_back(argument);
      }
      if (const uint64_t specialized_id =
              singleEdgeTarget(record, "specializedTemplateDeclaration")) {
        declaration->set_specializedTemplateDeclaration(
            nodeByIdAs<SgTemplateClassDeclaration>(nodes, specialized_id));
      }
    }
    if (SgTemplateInstantiationDecl *declaration =
            isSgTemplateInstantiationDecl(node)) {
      restoreTemplateInstantiationClassProperties(declaration,
                                                  record.properties, nodes);
    }
  }

  enum class RestoreState { Visiting, Complete };
  std::unordered_map<uint64_t, RestoreState> argument_states;
  std::unordered_map<uint64_t, RestoreState> class_states;
  std::function<void(uint64_t)> restore_argument;
  std::function<void(uint64_t)> restore_class_arguments;
  std::function<void(const JsonValue &)> restore_type_dependencies;

  restore_type_dependencies = [&](const JsonValue &value) {
    if (value.kind == JsonValue::Kind::Array) {
      for (const JsonValue &element : value.array) {
        restore_type_dependencies(element);
      }
      return;
    }
    if (value.kind != JsonValue::Kind::Object) {
      return;
    }
    const JsonValue *kind = value.find("kind");
    if (kind != nullptr && kind->kind == JsonValue::Kind::String &&
        kind->asString() == "SgClassType") {
      if (const JsonValue *declaration = value.find("declaration")) {
        const int64_t raw_id = declaration->asInt();
        if (raw_id <= 0) {
          throw std::runtime_error(
              "AST JSON class type construction dependency has no positive "
              "declaration ID");
        }
        restore_class_arguments(static_cast<uint64_t>(raw_id));
      }
    }
    for (const auto &[field, child] : value.object) {
      (void)field;
      restore_type_dependencies(child);
    }
  };

  restore_argument = [&](uint64_t id) {
    auto state = argument_states.find(id);
    if (state != argument_states.end()) {
      if (state->second == RestoreState::Visiting) {
        throw std::runtime_error(
            "AST JSON class type has a cyclic template-argument identity");
      }
      return;
    }
    argument_states.emplace(id, RestoreState::Visiting);
    auto record = records.find(id);
    if (record == records.end()) {
      throw std::runtime_error(
          "AST JSON class type references an unknown template argument");
    }
    SgTemplateArgument *argument = nodeByIdAs<SgTemplateArgument>(nodes, id);
    restore_type_dependencies(record->second->properties.at("type"));
    restore_type_dependencies(
        record->second->properties.at("source_spelled_type"));
    restoreTemplateArgumentSemanticProperties(
        argument, record->second->properties, nodes);
    argument_states[id] = RestoreState::Complete;
  };

  restore_class_arguments = [&](uint64_t id) {
    auto state = class_states.find(id);
    if (state != class_states.end()) {
      if (state->second == RestoreState::Visiting) {
        throw std::runtime_error(
            "AST JSON class type has cyclic canonical construction inputs");
      }
      return;
    }
    class_states.emplace(id, RestoreState::Visiting);
    SgClassDeclaration *class_declaration =
        nodeByIdAs<SgClassDeclaration>(nodes, id);
    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(class_declaration);
    if (declaration == nullptr) {
      class_states[id] = RestoreState::Complete;
      return;
    }
    for (SgTemplateArgument *argument : declaration->get_templateArguments()) {
      auto argument_id = node_ids.find(argument);
      if (argument_id == node_ids.end()) {
        throw std::runtime_error(
            "AST JSON class type template argument has no node identity");
      }
      restore_argument(argument_id->second);
    }
    for (SgTemplateArgument *argument :
         declaration->get_semanticTemplateArguments()) {
      auto argument_id = node_ids.find(argument);
      if (argument_id == node_ids.end()) {
        throw std::runtime_error(
            "AST JSON class type semantic argument has no node identity");
      }
      restore_argument(argument_id->second);
    }
    for (SgTemplateArgument *argument :
         declaration->get_deducedTemplateArguments()) {
      auto argument_id = node_ids.find(argument);
      if (argument_id == node_ids.end()) {
        throw std::runtime_error(
            "AST JSON class type deduced argument has no node identity");
      }
      restore_argument(argument_id->second);
    }
    class_states[id] = RestoreState::Complete;
  };

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node != nodes.end() &&
        isSgTemplateInstantiationDecl(restored_node->second) != nullptr) {
      restore_class_arguments(record.id);
    }
  }

  // Any later semantic type can require mangling a nonreal/template-id type,
  // whose SgNonrealDecl owns ordinary SgTemplateArgument nodes rather than an
  // SgTemplateInstantiationDecl.  Publish every argument's complete typed
  // identity before any class/function type factory is allowed to run.  Node
  // record order is not a dependency order and must never decide whether an
  // argument is initialized when its enclosing type is mangled.
  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node != nodes.end() &&
        isSgTemplateArgument(restored_node->second) != nullptr) {
      restore_argument(record.id);
    }
  }
}

static void restoreVariableInlineTypeOwnershipRoles(const AstFileRecord &ast,
                                                    const NodeMap &nodes) {
  std::unordered_map<uint64_t, const NodeRecord *> records;
  records.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    if (!records.emplace(record.id, &record).second) {
      throw std::runtime_error(
          "AST JSON inline type ownership pass found a duplicate node ID");
    }
  }

  for (const NodeRecord &variable_record : ast.nodes) {
    if (variable_record.kind != "SgVariableDeclaration") {
      continue;
    }
    const uint64_t nondefining_id =
        singleEdgeTarget(variable_record, "baseTypeNondefiningDeclaration");
    const uint64_t defining_id =
        singleEdgeTarget(variable_record, "baseTypeDefiningDeclaration");
    if (nondefining_id != 0 && defining_id != 0) {
      throw std::runtime_error(
          "AST JSON variable owns both an inline tag introduction and "
          "definition");
    }
    const uint64_t owned_id =
        nondefining_id != 0 ? nondefining_id : defining_id;
    if (owned_id == 0) {
      continue;
    }

    auto owned_record = records.find(owned_id);
    if (owned_record == records.end() ||
        requiredSingleEdgeTarget(*owned_record->second, "parent") !=
            variable_record.id) {
      throw std::runtime_error(
          "AST JSON variable inline type does not name its exact serialized "
          "owner");
    }
    SgDeclarationStatement *owned_declaration =
        nodeByIdAs<SgDeclarationStatement>(nodes, owned_id);
    if (owned_record->second->properties.requiredBool(
            "is_autonomous_declaration")) {
      throw std::runtime_error(
          "AST JSON variable inline type is serialized as an autonomous "
          "declaration");
    }
    if (SgClassDeclaration *class_declaration =
            isSgClassDeclaration(owned_declaration)) {
      class_declaration->set_isAutonomousDeclaration(false);
    } else if (SgEnumDeclaration *enum_declaration =
                   isSgEnumDeclaration(owned_declaration)) {
      enum_declaration->set_isAutonomousDeclaration(false);
    } else {
      throw std::runtime_error(
          "AST JSON variable inline type is not a class or enum declaration");
    }

    const std::vector<EdgeRecord> &declarator_edges =
        edgesFor(variable_record, "variables");
    if (declarator_edges.empty()) {
      throw std::runtime_error(
          "AST JSON variable inline type has no exact declarator");
    }
    for (const EdgeRecord &edge : declarator_edges) {
      auto declarator_record = records.find(edge.target);
      if (declarator_record == records.end() ||
          declarator_record->second->kind != "SgInitializedName") {
        throw std::runtime_error(
            "AST JSON variable inline type references an unknown declarator");
      }
      SgInitializedName *declarator =
          nodeByIdAs<SgInitializedName>(nodes, edge.target);
      const JsonValue *type_json =
          declarator_record->second->properties.find("type");
      if (type_json == nullptr) {
        throw std::runtime_error(
            "AST JSON variable inline declarator has no serialized type");
      }
      SgType *restored_type = typeFromJson(*type_json, nodes);
      declarator->set_typeptr(restored_type);
      SgNamedType *named_type =
          restored_type != nullptr
              ? isSgNamedType(restored_type->findBaseType())
              : nullptr;
      SgDeclarationStatement *type_declaration =
          named_type != nullptr ? named_type->get_declaration() : nullptr;
      SgDeclarationStatement *type_identity = nullptr;
      if (defining_id != 0) {
        type_identity = type_declaration != nullptr
                            ? type_declaration->get_definingDeclaration()
                            : nullptr;
        if (type_identity == nullptr) {
          type_identity = type_declaration;
        }
      } else {
        type_identity =
            type_declaration != nullptr
                ? type_declaration->get_firstNondefiningDeclaration()
                : nullptr;
        if (type_identity == nullptr) {
          type_identity = type_declaration;
        }
      }
      if (declarator->get_typeptr() != restored_type ||
          type_identity != owned_declaration) {
        throw std::runtime_error(
            "AST JSON variable inline declarator type does not name its "
            "exact owned declaration identity");
      }
    }
  }
}

static void restoreDesignatorFieldReferenceConstructionDependencies(
    const AstFileRecord &ast, const NodeMap &nodes) {
  std::unordered_map<uint64_t, const NodeRecord *> records;
  records.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    if (!records.emplace(record.id, &record).second) {
      throw std::runtime_error(
          "AST JSON designator dependency pass found a duplicate node ID");
    }
  }

  auto record_by_id = [&](uint64_t id,
                          const std::string &context) -> const NodeRecord & {
    auto found = records.find(id);
    if (found == records.end()) {
      throw std::runtime_error("AST JSON " + context +
                               " references an unknown node ID");
    }
    return *found->second;
  };

  for (const NodeRecord &designator_record : ast.nodes) {
    if (designator_record.kind != "SgDesignator") {
      continue;
    }
    SgDesignator *designator =
        nodeByIdAs<SgDesignator>(nodes, designator_record.id);
    const SgDesignator::designator_kind_enum kind =
        requiredEnum<SgDesignator::designator_kind_enum>(
            designator_record.properties, "designator_kind", "SgDesignator",
            {SgDesignator::e_designator_field, SgDesignator::e_designator_array,
             SgDesignator::e_designator_array_range});
    if (designator->get_kind() != kind) {
      throw std::runtime_error(
          "AST JSON designator construction kind disagrees with its exact "
          "serialized kind");
    }
    if (kind != SgDesignator::e_designator_field) {
      continue;
    }
    if (singleEdgeTarget(designator_record, "second_expression") != 0) {
      throw std::runtime_error(
          "AST JSON field designator has an unexpected second expression");
    }

    const uint64_t reference_id =
        requiredSingleEdgeTarget(designator_record, "first_expression");
    const NodeRecord &reference_record =
        record_by_id(reference_id, "field designator reference");
    if (reference_record.kind != "SgVarRefExp") {
      throw std::runtime_error(
          "AST JSON field designator does not own an exact SgVarRefExp");
    }
    SgVarRefExp *reference = nodeByIdAs<SgVarRefExp>(nodes, reference_id);
    const JsonValue *symbol_json = reference_record.properties.find("symbol");
    if (symbol_json == nullptr ||
        symbol_json->kind != JsonValue::Kind::Object ||
        symbol_json->requiredString("symbol_kind") != "SgVariableSymbol") {
      throw std::runtime_error(
          "AST JSON field designator reference has no canonical typed "
          "variable symbol");
    }

    const uint64_t basis_id =
        requiredPositiveId(symbol_json->at("symbol_declaration"),
                           "field designator variable symbol declaration");
    if (reference_record.properties.requiredInt("symbol_declaration") !=
            static_cast<int64_t>(basis_id) ||
        reference_record.properties.requiredString("symbol_name") !=
            symbol_json->requiredString("symbol_name")) {
      throw std::runtime_error(
          "AST JSON field designator symbol identity fields disagree");
    }
    const NodeRecord &basis_record =
        record_by_id(basis_id, "field designator symbol basis");
    if (basis_record.kind != "SgInitializedName") {
      throw std::runtime_error(
          "AST JSON field designator variable symbol has no exact named "
          "basis");
    }
    SgInitializedName *basis = nodeByIdAs<SgInitializedName>(nodes, basis_id);
    const std::string serialized_symbol_name =
        symbol_json->requiredString("symbol_name");
    if (serialized_symbol_name.empty() ||
        basis->get_name().getString() != serialized_symbol_name) {
      throw std::runtime_error(
          "AST JSON field designator variable symbol name disagrees with "
          "its exact basis");
    }

    const uint64_t owner_id = requiredSingleEdgeTarget(basis_record, "parent");
    const NodeRecord &owner_record =
        record_by_id(owner_id, "field designator named basis owner");
    SgVariableDeclaration *owner =
        nodeByIdAs<SgVariableDeclaration>(nodes, owner_id);
    if (owner_record.kind != "SgVariableDeclaration" ||
        basis->get_parent() != owner) {
      throw std::runtime_error(
          "AST JSON field designator named basis has no exact variable "
          "declaration owner");
    }
    const std::vector<EdgeRecord> &owner_variables =
        edgesFor(owner_record, "variables");
    if (std::count_if(owner_variables.begin(), owner_variables.end(),
                      [&](const EdgeRecord &edge) {
                        return edge.target == basis_id;
                      }) != 1) {
      throw std::runtime_error(
          "AST JSON field designator variable declaration does not own its "
          "named basis exactly once");
    }

    const uint64_t scope_id = requiredSingleEdgeTarget(basis_record, "scope");
    SgScopeStatement *scope = nodeByIdAs<SgScopeStatement>(nodes, scope_id);
    if (basis->get_scope() != scope || owner->get_parent() != scope) {
      throw std::runtime_error(
          "AST JSON field designator named basis has an inconsistent exact "
          "semantic scope");
    }

    if (requiredSingleEdgeTarget(basis_record, "declptr") != owner_id ||
        basis->get_declptr() != owner) {
      throw std::runtime_error(
          "AST JSON field designator named basis has no exact semantic "
          "declaration owner");
    }

    const uint64_t definition_id =
        requiredSingleEdgeTarget(basis_record, "variable_definition");
    const NodeRecord &definition_record = record_by_id(
        definition_id, "field designator named basis variable definition");
    if (definition_record.kind != "SgVariableDefinition" ||
        requiredSingleEdgeTarget(definition_record, "vardefn") != basis_id ||
        requiredSingleEdgeTarget(definition_record, "parent") != basis_id) {
      throw std::runtime_error(
          "AST JSON field designator named basis has no exact inverse "
          "variable definition");
    }
    SgVariableDefinition *definition =
        nodeByIdAs<SgVariableDefinition>(nodes, definition_id);
    if ((basis->get_definition() != nullptr &&
         basis->get_definition() != definition) ||
        (definition->get_vardefn() != nullptr &&
         definition->get_vardefn() != basis) ||
        (definition->get_parent() != nullptr &&
         definition->get_parent() != basis)) {
      throw std::runtime_error(
          "AST JSON field designator named basis has conflicting "
          "construction-time declaration links");
    }
    // The designator constructor validates its field symbol before the general
    // edge-linking pass.  Publish this exact serialized bidirectional identity
    // now as part of node construction; linkNodeEdges later verifies the same
    // edges and must not invent or replace either endpoint.
    basis->set_definition(definition);
    definition->set_vardefn(basis);
    definition->set_parent(basis);
    if (basis->get_definition() != definition ||
        definition->get_vardefn() != basis ||
        definition->get_parent() != basis) {
      throw std::runtime_error(
          "AST JSON field designator named basis failed exact "
          "construction-time declaration publication");
    }

    const JsonValue *basis_type_json = basis_record.properties.find("type");
    const JsonValue *reference_type_json =
        reference_record.properties.find("type");
    if (basis_type_json == nullptr || reference_type_json == nullptr) {
      throw std::runtime_error(
          "AST JSON field designator variable dependency has no serialized "
          "semantic type");
    }
    if (!jsonValuesEqual(*basis_type_json, *reference_type_json)) {
      throw std::runtime_error(
          "AST JSON field designator reference type record differs from its "
          "exact variable basis type record");
    }
    SgType *basis_type = typeFromJson(*basis_type_json, nodes);
    if (basis_type == nullptr || isSgTypeUnknown(basis_type) != nullptr ||
        isSgTypeDefault(basis_type) != nullptr) {
      throw std::runtime_error(
          "AST JSON field designator reference type does not name its exact "
          "variable basis type");
    }
    basis->set_typeptr(basis_type);

    SgVariableSymbol *symbol =
        isSgVariableSymbol(symbolFromJson(*symbol_json, nodes));
    if (symbol == nullptr || symbol->get_declaration() != basis ||
        symbol->get_symbol_basis() != basis ||
        symbol->get_name().getString() != serialized_symbol_name ||
        symbol->get_type() != basis_type) {
      throw std::runtime_error(
          "AST JSON field designator failed exact variable symbol "
          "reconstruction");
    }
    reference->set_symbol(symbol);
    if (reference->get_symbol() != symbol ||
        reference->get_type() != basis_type) {
      throw std::runtime_error(
          "AST JSON field designator reference failed exact typed symbol "
          "publication");
    }
  }
}

void publishTemplateParameterCanonicalTypes(const AstFileRecord &ast,
                                            const NodeMap &nodes) {
  // A template parameter owns its canonical SgTemplateType. Publish all of
  // those identities before arbitrary use-site types are reconstructed so JSON
  // record order or a delayed constructor cannot let a packed or otherwise
  // specialized use claim the parameter's canonical type edge first.
  for (const NodeRecord &record : ast.nodes) {
    auto existing = nodes.find(record.id);
    if (existing == nodes.end()) {
      continue;
    }
    SgTemplateParameter *parameter = isSgTemplateParameter(existing->second);
    if (parameter == nullptr) {
      continue;
    }
    if (parameter->get_parameterType() != SgTemplateParameter::type_parameter) {
      continue;
    }
    const JsonValue *type = record.properties.find("type");
    if (type == nullptr) {
      throw std::runtime_error(
          "AST JSON SgTemplateParameter has no canonical type field");
    }
    SgType *canonical_type = nullableTypeFromJson(*type, nodes);
    if (canonical_type == nullptr || parameter->get_type() != canonical_type ||
        isSgTemplateType(canonical_type) == nullptr) {
      throw std::runtime_error(
          "AST JSON SgTemplateParameter failed canonical type publication");
    }
  }
}

void applyTypesAndSymbols(const AstFileRecord &ast, const NodeMap &nodes) {
  TypeOwnedSymbolDeserializationGuard type_owned_symbol_guard;

  // Array bounds and non-type template arguments participate in canonical
  // type mangling.  Their dependent references must therefore bind to the
  // already-restored symbol tables before any type factory is invoked; JSON
  // node order is not a semantic dependency order.
  for (const NodeRecord &record : ast.nodes) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(nodeById(nodes, record.id));
    if (reference == nullptr) {
      continue;
    }
    const JsonValue &properties = record.properties;
    const uint64_t declaration_id =
        requiredPositiveId(properties.at("symbol_declaration"),
                           "SgNonrealRefExp symbol declaration");
    SgNonrealDecl *declaration =
        nodeByIdAs<SgNonrealDecl>(nodes, declaration_id);
    SgNonrealSymbol *symbol =
        isSgNonrealSymbol(symbolFromJson(properties.at("symbol"), nodes));
    if (symbol == nullptr || symbol->get_declaration() != declaration ||
        properties.requiredString("symbol_name") !=
            symbol->get_name().getString()) {
      throw std::runtime_error(
          "AST JSON early SgNonrealRefExp symbol dependency is inconsistent");
    }
    reference->set_symbol(symbol);
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;

    rejectRemovedQualifiedNameState(p);

    if (SgScopeStatement *scope = isSgScopeStatement(node)) {
      scope->setCaseInsensitive(p.requiredBool("case_insensitive"));
    }
    if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
      scope->set_is_default_nonreal_scope(
          p.requiredBool("is_default_nonreal_scope"));
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      const bool present = p.requiredBool("source_name_qualification_present");
      const bool global = p.requiredBool("source_name_global_qualification");
      const SgStringList tokens =
          stringListFromJson(p.at("source_name_qualification_tokens"),
                             "source_name_qualification_tokens");
      if (!present && (global || !tokens.empty())) {
        throw std::runtime_error(
            "AST JSON declaration source qualifier payload is present "
            "without its presence bit");
      }
      decl->set_source_name_global_qualification(global);
      decl->get_source_name_qualification_tokens() = tokens;
      decl->set_source_name_qualification_present(present);
    }

    if (SgInitializedName *name = isSgInitializedName(node)) {
      name->set_fortran_source_type(
          nullableTypeFromJson(p.at("fortran_source_type"), nodes));
      name->set_cxx_source_type(
          nullableTypeFromJson(p.at("cxx_source_type"), nodes));
      const uint64_t cray_pointee_id =
          static_cast<uint64_t>(p.requiredInt("cray_pointer_pointee"));
      name->set_cray_pointer_pointee(
          cray_pointee_id != 0
              ? nodeByIdAs<SgInitializedName>(nodes, cray_pointee_id)
              : nullptr);
      name->set_fortran_type_spec(
          requiredFortranTypeSpec(p, "SgInitializedName"));
      name->set_fortran_procedure_interface(
          SgName(p.requiredString("fortran_procedure_interface")));
      const uint64_t separate_shape_declaration_id = static_cast<uint64_t>(
          p.requiredInt("fortran_separate_shape_declaration"));
      name->set_fortran_separate_shape_declaration(
          separate_shape_declaration_id != 0
              ? nodeByIdAs<SgStatement>(nodes, separate_shape_declaration_id)
              : nullptr);
      const uint64_t separate_pointer_declaration_id = static_cast<uint64_t>(
          p.requiredInt("fortran_separate_pointer_declaration"));
      name->set_fortran_separate_pointer_declaration(
          separate_pointer_declaration_id != 0
              ? nodeByIdAs<SgStatement>(nodes, separate_pointer_declaration_id)
              : nullptr);
      name->set_shapeDeferred(p.requiredBool("shape_deferred"));
      name->set_is_predefined_identifier(
          p.requiredBool("is_predefined_identifier"));
      name->set_generated_variable_role(
          requiredEnum<SgInitializedName::generated_variable_role_enum>(
              p, "generated_variable_role", "SgInitializedName",
              {SgInitializedName::e_generated_variable_none,
               SgInitializedName::e_generated_loop_tiling_index,
               SgInitializedName::e_generated_loop_tiling_increment}));
      const auto enum_source_ownership =
          requiredEnum<SgInitializedName::enum_constant_source_ownership_enum>(
              p, "enum_constant_source_ownership", "SgInitializedName",
              {SgInitializedName::e_enum_constant_source_unclassified,
               SgInitializedName::e_enum_constant_source_body,
               SgInitializedName::e_enum_constant_source_external,
               SgInitializedName::e_enum_constant_semantic_only});
      if (name->get_enum_constant_source_ownership() != enum_source_ownership) {
        throw std::runtime_error(
            "AST JSON enum-constant source ownership changed after "
            "construction");
      }
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
      const bool present = p.requiredBool("source_type_qualification_present");
      const bool global = p.requiredBool("source_type_global_qualification");
      const SgStringList tokens =
          stringListFromJson(p.at("source_type_qualification_tokens"),
                             "source_type_qualification_tokens");
      if (!present && (global || !tokens.empty())) {
        throw std::runtime_error(
            "AST JSON initialized-name source type qualifier payload is "
            "present without its presence bit");
      }
      name->set_source_type_global_qualification(global);
      name->get_source_type_qualification_tokens() = tokens;
      name->set_source_type_qualification_present(present);
      const bool source_name_present =
          p.requiredBool("source_name_qualification_present");
      const bool source_name_global =
          p.requiredBool("source_name_global_qualification");
      const SgStringList source_name_tokens =
          stringListFromJson(p.at("source_name_qualification_tokens"),
                             "source_name_qualification_tokens");
      if (!source_name_present &&
          (source_name_global || !source_name_tokens.empty())) {
        throw std::runtime_error(
            "AST JSON initialized-name source name qualifier payload is "
            "present without its presence bit");
      }
      name->set_source_name_global_qualification(source_name_global);
      name->get_source_name_qualification_tokens() = source_name_tokens;
      name->set_source_name_qualification_present(source_name_present);
    }

    if (SgTemplateInstantiationTypedefDeclaration *tmpl =
            isSgTemplateInstantiationTypedefDeclaration(node)) {
      const std::string template_name = p.at("template_name").asString();
      if (template_name.empty()) {
        throw std::runtime_error(
            "AST JSON SgTemplateInstantiationTypedefDeclaration has an empty "
            "template_name");
      }
      tmpl->set_templateName(SgName(template_name));
      tmpl->set_templateHeader(SgName(p.requiredString("template_header")));
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
      const std::string template_name = p.at("template_name").asString();
      if (template_name.empty()) {
        throw std::runtime_error(
            "AST JSON SgTemplateInstantiationFunctionDecl has an empty "
            "template_name");
      }
      tmpl->set_templateName(SgName(template_name));
      tmpl->set_template_argument_list_is_explicit(
          p.requiredBool("template_argument_list_is_explicit"));
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
      const std::string template_name = p.at("template_name").asString();
      if (template_name.empty()) {
        throw std::runtime_error(
            "AST JSON SgTemplateInstantiationMemberFunctionDecl has an empty "
            "template_name");
      }
      tmpl->set_templateName(SgName(template_name));
      tmpl->set_template_argument_list_is_explicit(
          p.requiredBool("template_argument_list_is_explicit"));
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
      restoreDeclarationTypeModifierFields(
          decl->get_declarationModifier().get_typeModifier(), p,
          "declaration statement");
      decl->get_declarationModifier().get_storageModifier().set_modifier(
          requiredStorageModifier(p, "declaration_storage_modifier",
                                  "declaration statement"));
      decl->get_declarationModifier()
          .get_storageModifier()
          .set_thread_local_storage(
              p.requiredBool("declaration_thread_local_storage"));
      const SgAccessModifier::access_modifier_enum access_modifier =
          isSgAccessLabelStatement(decl) != nullptr
              ? requiredEnum<SgAccessModifier::access_modifier_enum>(
                    p, "declaration_access_modifier", "SgAccessLabelStatement",
                    {SgAccessModifier::e_unknown})
              : requiredDeclarationAccessModifier(
                    p, "declaration_access_modifier", "declaration statement");
      decl->get_declarationModifier().get_accessModifier().set_modifier(
          access_modifier);
      decl->get_declarationModifier().get_accessModifier().set_is_explicit(
          p.requiredBool("declaration_access_is_explicit"));
      decl->set_nameOnly(p.at("name_only").asBool());
      decl->set_forward(p.at("forward").asBool());
      decl->set_externBrace(p.at("extern_brace").asBool());
      decl->set_skipElaborateType(p.at("skip_elaborate_type").asBool());
      decl->set_binding_label(p.at("binding_label").asString());
      decl->set_binding_cdefined(p.requiredBool("binding_cdefined"));
      decl->set_unparse_template_ast(p.at("unparse_template_ast").asBool());
      decl->get_declarationModifier().set_gnu_attribute_visibility(
          requiredGnuDeclarationVisibility(
              p, "declaration_gnu_attribute_visibility",
              "declaration statement"));
      decl->get_declarationModifier().set_gnu_type_visibility(
          requiredGnuDeclarationVisibility(p, "declaration_gnu_type_visibility",
                                           "declaration statement"));
    }

    if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
      SgFunctionType *functionType =
          semanticFunctionTypeFromJson(p.at("function_type"), nodes);
      if (functionType == nullptr) {
        throw std::runtime_error("AST JSON SgFunctionDeclaration " +
                                 decl->get_name().getString() +
                                 " has no exact semantic function type");
      }
      decl->set_type(functionType);
      SgFunctionType *function_type_syntax = isSgFunctionType(
          nullableTypeFromJson(p.at("function_type_syntax"), nodes));
      decl->set_type_syntax(function_type_syntax);
      const std::string omp_declare_variant_source_name =
          p.requiredString("omp_declare_variant_source_name");
      const std::optional<unsigned int> omp_declare_variant_region_ordinal =
          requiredOmpDeclareVariantRegionOrdinal(
              p, decl->get_name().getString(),
              "SgFunctionDeclaration " + decl->get_name().getString());
      decl->set_omp_declare_variant_source_name(
          SgName(omp_declare_variant_source_name));
      decl->set_omp_declare_variant_region_ordinal(
          omp_declare_variant_region_ordinal);
      decl->set_source_name_parenthesized_for_macro(
          p.requiredBool("source_name_parenthesized_for_macro"));
      decl->set_source_declarator_uses_wrapped_function_type(
          p.requiredBool("source_declarator_uses_wrapped_function_type"));
      const std::string anonymous_symbol_key =
          p.requiredString("fortran_anonymous_program_unit_symbol_key");
      if (!anonymous_symbol_key.empty()) {
        throw std::runtime_error(
            "AST JSON function exposes an implementation-only anonymous "
            "program-unit symbol key");
      }
      const bool is_fortran_program_unit =
          isSgProgramHeaderStatement(decl) != nullptr ||
          isSgProcedureHeaderStatement(decl) != nullptr;
      const bool requires_anonymous_symbol_key =
          is_fortran_program_unit &&
          SageInterface::isFortranProgramUnitWithoutSourceName(decl);
      if (requires_anonymous_symbol_key !=
          !decl->get_fortran_anonymous_program_unit_symbol_key().is_null()) {
        throw std::runtime_error(
            "AST JSON function anonymous program-unit identity was not "
            "published before property restoration");
      }
      decl->get_functionModifier().set_modifierVector(bitVectorFromJson(
          p.at("function_modifier_vector"), "function_modifier_vector"));
      decl->get_specialFunctionModifier().set_modifierVector(
          bitVectorFromJson(p.at("special_function_modifier_vector"),
                            "special_function_modifier_vector"));
      decl->set_named_in_end_statement(p.at("named_in_end_statement").asBool());
      decl->set_asm_name(p.at("asm_name").asString());
      decl->set_oldStyleDefinition(p.at("old_style_definition").asBool());
      decl->set_requiresNameQualificationOnReturnType(
          p.at("requires_name_qualification_on_return_type").asBool());
      decl->set_gnu_extension_section(p.at("gnu_extension_section").asString());
      decl->set_gnu_extension_alias(p.at("gnu_extension_alias").asString());
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
      const bool source_return_type_present =
          p.requiredBool("source_return_type_qualification_present");
      const bool source_return_type_global =
          p.requiredBool("source_return_type_global_qualification");
      const SgStringList source_return_type_tokens =
          stringListFromJson(p.at("source_return_type_qualification_tokens"),
                             "source_return_type_qualification_tokens");
      if (!source_return_type_present &&
          (source_return_type_global || !source_return_type_tokens.empty())) {
        throw std::runtime_error(
            "AST JSON function return-type source qualifier payload is "
            "present without its presence bit");
      }
      decl->set_source_return_type_global_qualification(
          source_return_type_global);
      decl->get_source_return_type_qualification_tokens() =
          source_return_type_tokens;
      decl->set_source_return_type_qualification_present(
          source_return_type_present);
      const int source_return_type_elaboration_kind =
          static_cast<int>(p.at("source_return_type_elaboration_kind").asInt());
      if (source_return_type_elaboration_kind <
              static_cast<int>(
                  SgFunctionDeclaration::
                      e_source_return_type_elaboration_unspecified) ||
          source_return_type_elaboration_kind >
              static_cast<int>(SgFunctionDeclaration::
                                   e_source_return_type_elaboration_enum)) {
        throw std::runtime_error(
            "AST JSON function return-type elaboration kind is invalid");
      }
      decl->set_source_return_type_elaboration_kind(
          static_cast<
              SgFunctionDeclaration::source_return_type_elaboration_kind_enum>(
              source_return_type_elaboration_kind));
      if (source_return_type_present &&
          decl->get_source_return_type_elaboration_kind() ==
              SgFunctionDeclaration::
                  e_source_return_type_elaboration_unspecified) {
        throw std::runtime_error(
            "AST JSON source function return type has no exact elaboration "
            "kind");
      }
      if (decl->get_source_return_type_elaboration_kind() !=
              SgFunctionDeclaration::
                  e_source_return_type_elaboration_unspecified &&
          decl->get_type_elaboration_required_for_return_type() !=
              (decl->get_source_return_type_elaboration_kind() !=
               SgFunctionDeclaration::e_source_return_type_elaboration_none)) {
        throw std::runtime_error(
            "AST JSON exact function return-type elaboration disagrees with "
            "its typed boolean projection");
      }
      decl->set_prototypeIsWithoutParameters(
          p.at("prototype_is_without_parameters").asBool());
      decl->set_gnu_regparm_attribute(
          static_cast<int>(p.at("gnu_regparm_attribute").asInt()));
      decl->set_type_syntax_is_available(
          p.at("type_syntax_is_available").asBool());
      if (decl->get_type_syntax_is_available() !=
          (decl->get_type_syntax() != nullptr)) {
        throw std::runtime_error("AST JSON SgFunctionDeclaration " +
                                 decl->get_name().getString() +
                                 " has inconsistent function type syntax "
                                 "state");
      }
      if (function_type_syntax != nullptr) {
        if (function_type_syntax == functionType ||
            function_type_syntax->get_parent() != nullptr) {
          throw std::runtime_error(
              "AST JSON SgFunctionDeclaration " + decl->get_name().getString() +
              " does not own one distinct source function type syntax node");
        }
        function_type_syntax->set_parent(decl);
      }
      if (decl->get_source_declarator_uses_wrapped_function_type()) {
        decl->validate_source_declarator_form();
      }
      decl->set_using_C11_Noreturn_keyword(
          p.at("using_c11_noreturn_keyword").asBool());
      decl->set_is_constexpr(p.at("is_constexpr").asBool());
      decl->set_using_new_function_return_type_syntax(
          p.at("using_new_function_return_type_syntax").asBool());
      decl->set_is_deduction_guide(p.at("is_deduction_guide").asBool());
      decl->set_marked_as_frontend_normalization(
          p.at("marked_as_frontend_normalization").asBool());
      decl->set_is_implicit_function(p.at("is_implicit_function").asBool());
      decl->set_template_instantiation_pattern_is_unpublished(
          p.requiredBool("template_instantiation_pattern_is_unpublished"));
    }

    if (SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node)) {
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
        variable->set_fortran_declaration_origin(
            requiredEnum<
                SgVariableDeclaration::fortran_declaration_origin_enum>(
                p, "fortran_declaration_origin", "SgVariableDeclaration",
                {SgVariableDeclaration::e_fortran_source_declaration,
                 SgVariableDeclaration::e_fortran_semantic_only_declaration,
                 SgVariableDeclaration::e_fortran_pending_source_declaration}));
        variable->set_requiresGlobalNameQualificationOnType(
            p.at("requires_global_name_qualification_on_type").asBool());
      }
    }

    if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
      decl->set_isUnNamed(p.requiredBool("is_unnamed"));
      decl->set_isAutonomousDeclaration(
          p.requiredBool("is_autonomous_declaration"));
      if (SgTemplateClassDeclaration *tmpl =
              isSgTemplateClassDeclaration(decl)) {
        const std::string template_name = p.at("template_name").asString();
        if (template_name.empty()) {
          throw std::runtime_error(
              "AST JSON SgTemplateClassDeclaration has an empty "
              "template_name");
        }
        tmpl->set_templateName(SgName(template_name));
      }
      if (SgTemplateInstantiationDecl *tmpl =
              isSgTemplateInstantiationDecl(decl)) {
        restoreTemplateInstantiationClassProperties(tmpl, p, nodes);
      }
      ensureClassTypeForDeclaration(decl);
    } else if (SgBaseClass *base = isSgBaseClass(node)) {
      SgType *source_type = typeFromJson(p.at("source_type"), nodes);
      if (source_type == nullptr) {
        throw std::runtime_error(
            "AST JSON SgBaseClass has no exact source_type");
      }
      base->set_source_type(source_type);
      base->set_isDirectBaseClass(p.requiredBool("is_direct_base_class"));
      base->set_pack_expansion(p.requiredBool("pack_expansion"));
      if (base->get_baseClassModifier() != nullptr) {
        base->get_baseClassModifier()->set_modifier(
            requiredEnum<SgBaseClassModifier::baseclass_modifier_enum>(
                p, "base_class_modifier", "SgBaseClass",
                {SgBaseClassModifier::e_default,
                 SgBaseClassModifier::e_virtual}));
        base->get_baseClassModifier()->get_accessModifier().set_modifier(
            requiredBaseClassAccessModifier(p, "base_class_access_modifier",
                                            "SgBaseClass"));
        base->get_baseClassModifier()->get_accessModifier().set_is_explicit(
            p.requiredBool("base_class_access_is_explicit"));
      }
      base->set_name_qualification_length(
          static_cast<int>(p.requiredInt("name_qualification_length")));
      base->set_type_elaboration_required(
          p.requiredBool("type_elaboration_required"));
      base->set_global_qualification_required(
          p.requiredBool("global_qualification_required"));
      const bool source_type_present =
          p.requiredBool("source_type_qualification_present");
      const bool source_type_global =
          p.requiredBool("source_type_global_qualification");
      const SgStringList source_type_tokens =
          stringListFromJson(p.at("source_type_qualification_tokens"),
                             "source_type_qualification_tokens");
      const bool source_type_owns_terminal =
          p.requiredBool("source_type_qualification_owns_terminal_name");
      const bool source_type_owns_arguments =
          p.requiredBool("source_type_qualification_owns_template_arguments");
      if (!source_type_present &&
          (source_type_global || !source_type_tokens.empty())) {
        throw std::runtime_error(
            "AST JSON SgBaseClass has source qualifier components without "
            "source qualification provenance");
      }
      if ((source_type_owns_terminal &&
           (!source_type_present || source_type_tokens.empty())) ||
          (source_type_owns_arguments && !source_type_owns_terminal)) {
        throw std::runtime_error(
            "AST JSON SgBaseClass has inconsistent source qualifier terminal "
            "ownership");
      }
      base->set_source_type_global_qualification(source_type_global);
      base->get_source_type_qualification_tokens() = source_type_tokens;
      base->set_source_type_qualification_present(source_type_present);
      base->set_source_type_qualification_owns_terminal_name(
          source_type_owns_terminal);
      base->set_source_type_qualification_owns_template_arguments(
          source_type_owns_arguments);
    } else if (SgNamespaceDeclarationStatement *decl =
                   isSgNamespaceDeclarationStatement(node)) {
      decl->set_isUnnamedNamespace(p.requiredBool("is_unnamed_namespace"));
      decl->set_isInlinedNamespace(p.requiredBool("is_inlined_namespace"));
    } else if (SgFortranIncludeLine *stmt = isSgFortranIncludeLine(node)) {
      stmt->set_filename(p.requiredString("filename"));
    } else if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
      stmt->set_label(SgName(p.requiredString("label")));
      stmt->set_gnu_extension_unused(p.requiredBool("gnu_extension_unused"));
    } else if (SgBasicBlock *stmt = isSgBasicBlock(node)) {
      stmt->set_is_fortran_block_construct(
          p.requiredBool("is_fortran_block_construct"));
      stmt->set_fortran_block_construct_name(
          p.requiredString("fortran_block_construct_name"));
    } else if (SgIfStmt *stmt = isSgIfStmt(node)) {
      stmt->set_string_label(p.requiredString("string_label"));
      stmt->set_has_end_statement(p.requiredBool("has_end_statement"));
      stmt->set_use_then_keyword(p.requiredBool("use_then_keyword"));
      stmt->set_is_else_if_statement(p.requiredBool("is_else_if_statement"));
    } else if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
      stmt->set_string_label(p.requiredString("string_label"));
      stmt->set_has_end_statement(p.requiredBool("has_end_statement"));
    } else if (SgFortranDo *stmt = isSgFortranDo(node)) {
      stmt->set_string_label(p.requiredString("string_label"));
      stmt->set_old_style(p.requiredBool("old_style"));
      stmt->set_has_end_statement(p.requiredBool("has_end_statement"));
    } else if (SgIOStatement *stmt = isSgIOStatement(node)) {
      stmt->set_io_statement(requiredEnum<SgIOStatement::io_statement_enum>(
          p, "io_statement", "SgIOStatement",
          {SgIOStatement::e_read, SgIOStatement::e_print,
           SgIOStatement::e_write, SgIOStatement::e_open,
           SgIOStatement::e_close, SgIOStatement::e_inquire,
           SgIOStatement::e_backspace, SgIOStatement::e_endfile,
           SgIOStatement::e_rewind, SgIOStatement::e_flush,
           SgIOStatement::e_wait}));
    } else if (SgAttributeSpecificationStatement *stmt =
                   isSgAttributeSpecificationStatement(node)) {
      stmt->set_attribute_kind(
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
               SgAttributeSpecificationStatement::e_intentStatement,
               SgAttributeSpecificationStatement::e_intrinsicStatement,
               SgAttributeSpecificationStatement::e_optionalStatement,
               SgAttributeSpecificationStatement::e_parameterStatement,
               SgAttributeSpecificationStatement::e_pointerStatement,
               SgAttributeSpecificationStatement::e_protectedStatement,
               SgAttributeSpecificationStatement::e_saveStatement,
               SgAttributeSpecificationStatement::e_targetStatement,
               SgAttributeSpecificationStatement::e_valueStatement,
               SgAttributeSpecificationStatement::e_volatileStatement}));
      stmt->set_intent(static_cast<int>(p.requiredInt("intent")));
      if (const JsonValue *names = p.find("name_list")) {
        stmt->get_name_list() = stringListFromJson(*names, "name_list");
      }
    } else if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
      body->set_function_name(SgName(p.requiredString("function_name")));
      body->set_use_function_name(p.requiredBool("use_function_name"));
    } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
      const std::string semantic_name = p.at("semantic_name").asString();
      if (semantic_name.empty()) {
        throw std::runtime_error(
            "AST JSON SgNonrealDecl has an empty semantic_name");
      }
      decl->set_semantic_name(SgName(semantic_name));
      decl->set_template_parameter_position(
          static_cast<int>(p.requiredInt("template_parameter_position")));
      decl->set_template_parameter_depth(
          static_cast<int>(p.requiredInt("template_parameter_depth")));
      decl->set_is_class_member(p.requiredBool("is_class_member"));
      decl->set_is_template_param(p.requiredBool("is_template_param"));
      decl->set_is_template_template_param(
          p.requiredBool("is_template_template_param"));
      decl->set_has_template_keyword(p.requiredBool("has_template_keyword"));
      decl->set_has_global_qualifier(p.requiredBool("has_global_qualifier"));
      decl->set_suppress_typename(p.requiredBool("suppress_typename"));
      decl->set_source_elaboration_kind(
          requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
              p, "source_elaboration_kind", "SgNonrealDecl",
              {SgNonrealDecl::e_source_elaboration_unspecified,
               SgNonrealDecl::e_source_elaboration_none,
               SgNonrealDecl::e_source_elaboration_typename,
               SgNonrealDecl::e_source_elaboration_class,
               SgNonrealDecl::e_source_elaboration_struct,
               SgNonrealDecl::e_source_elaboration_union,
               SgNonrealDecl::e_source_elaboration_enum}));
      decl->set_nonreal_template_role(
          requiredEnum<SgNonrealDecl::nonreal_template_role_enum>(
              p, "nonreal_template_role", "SgNonrealDecl",
              {SgNonrealDecl::e_nonreal_template_none,
               SgNonrealDecl::e_nonreal_template_declaration,
               SgNonrealDecl::e_nonreal_template_id}));
      if (decl->get_nonreal_template_role() ==
          SgNonrealDecl::e_nonreal_template_id) {
        if (semantic_name.find('<') == std::string::npos ||
            semantic_name.rfind('>') == std::string::npos) {
          throw std::runtime_error(
              "AST JSON SgNonrealDecl template semantic_name is not a "
              "complete template-id");
        }
      } else if (semantic_name != decl->get_name().getString()) {
        throw std::runtime_error(
            "AST JSON non-template SgNonrealDecl semantic_name differs from "
            "its name");
      }
      decl->set_is_concept(p.requiredBool("is_concept"));
      decl->set_is_nonreal_function(p.requiredBool("is_nonreal_function"));
      if (decl->get_type() == nullptr) {
        decl->set_type(new SgNonrealType(decl));
      }
      if (const JsonValue *type = p.find("type")) {
        decl->get_type()->set_autonomous_declaration(
            type->requiredBool("autonomous_declaration"));
      }
    } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
      decl->set_embedded(p.requiredBool("embedded"));
      decl->set_isUnNamed(p.requiredBool("is_unnamed"));
      decl->set_isAutonomousDeclaration(
          p.requiredBool("is_autonomous_declaration"));
      decl->set_isScopedEnum(p.requiredBool("is_scoped_enum"));
      decl->set_underlying_type_source_spelled(
          p.requiredBool("underlying_type_source_spelled"));
      if (const JsonValue *field_type = p.find("field_type")) {
        decl->set_field_type(field_type->requiredBool("present")
                                 ? typeFromJson(*field_type, nodes)
                                 : nullptr);
      }
      if (decl->get_type() == nullptr) {
        decl->set_type(SgEnumType::createType(decl));
      }
    } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
      decl->set_typedef_type(
          requiredEnum<SgTypedefDeclaration::typedef_type_enum>(
              p, "typedef_type", "SgTypedefDeclaration",
              {SgTypedefDeclaration::e_typedef,
               SgTypedefDeclaration::e_using}));
      decl->set_typedefBaseTypeContainsDefiningDeclaration(
          p.requiredBool("typedef_base_type_contains_defining_declaration"));
      decl->set_isAutonomousDeclaration(
          p.requiredBool("is_autonomous_declaration"));
      const bool present =
          p.requiredBool("source_base_type_qualification_present");
      const bool global =
          p.requiredBool("source_base_type_global_qualification");
      const SgStringList tokens =
          stringListFromJson(p.at("source_base_type_qualification_tokens"),
                             "source_base_type_qualification_tokens");
      if (!present && (global || !tokens.empty())) {
        throw std::runtime_error(
            "AST JSON typedef source base-type qualifier payload is present "
            "without its presence bit");
      }
      decl->set_source_base_type_global_qualification(global);
      decl->get_source_base_type_qualification_tokens() = tokens;
      decl->set_source_base_type_qualification_present(present);
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
          requiredStorageModifier(p, "storage_modifier", "SgInitializedName"));
      const std::string section =
          p.requiredString("gnu_attribute_section_name");
      if (!section.empty()) {
        name->set_gnu_attribute_section_name(section);
      }
    } else if (isSgTemplateArgument(node) != nullptr) {
      // restoreClassTypeConstructionInputs publishes every template argument
      // before this general type pass. Reapplying a serialized type here can
      // allocate a competing wrapper graph and make semantic identity depend
      // on JSON record order.
    } else if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
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
      if (const JsonValue *expr = p.find("source_type_constraint")) {
        SgExpression *expression = expressionFromRef(*expr, nodes);
        parameter->set_sourceTypeConstraint(expression);
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
              static_cast<uint64_t>(p.requiredInt("template_declaration"))) {
        parameter->set_templateDeclaration(
            nodeByIdAs<SgDeclarationStatement>(nodes, target));
      }
      if (uint64_t target = static_cast<uint64_t>(
              p.requiredInt("source_spelled_template_declaration"))) {
        SgTemplateDeclaration *source_declaration =
            nodeByIdAs<SgTemplateDeclaration>(nodes, target);
        parameter->set_sourceSpelledTemplateDeclaration(source_declaration);
        source_declaration->set_parent(parameter);
      }
      if (uint64_t target = static_cast<uint64_t>(
              p.requiredInt("default_template_declaration_parameter"))) {
        parameter->set_defaultTemplateDeclarationParameter(
            nodeByIdAs<SgDeclarationStatement>(nodes, target));
      }
      if (uint64_t target =
              static_cast<uint64_t>(p.requiredInt("initialized_name"))) {
        parameter->set_initializedName(
            nodeByIdAs<SgInitializedName>(nodes, target));
      }
      parameter->set_templateParameterKeyword(
          requiredEnum<SgTemplateParameter::template_parameter_keyword_enum>(
              p, "template_parameter_keyword", "SgTemplateParameter",
              {SgTemplateParameter::keyword_unspecified,
               SgTemplateParameter::keyword_class,
               SgTemplateParameter::keyword_typename}));
      parameter->set_isAbbreviatedFunctionTemplateParameter(
          p.requiredBool("is_abbreviated_function_template_parameter"));
      parameter->set_is_parameter_pack(p.requiredBool("is_parameter_pack"));
    } else if (SgExpression *expr = isSgExpression(node)) {
      if (const JsonValue *type = validatedExpressionTypeProperty(expr, p)) {
        SgType *restored_type = typeFromJson(*type, nodes);
        validateExactSemanticExpressionType(expr, restored_type,
                                            "deserialization");
        if (SgValueExp *value = isSgValueExp(expr)) {
          const bool hasLiteralSemanticType =
              p.requiredBool("has_literal_semantic_type");
          if (hasLiteralSemanticType) {
            value->set_literal_type(restored_type);
          } else if (value->get_literal_type() != nullptr) {
            throw std::runtime_error(
                "AST JSON value expression unexpectedly owns a semantic "
                "literal type before reconstruction");
          }
        } else if (restoreExactStoredExpressionType(expr, restored_type)) {
          // Stored by the exact-type node helper.
        } else if (SgCastExp *cast = isSgCastExp(expr)) {
          cast->set_type(restored_type);
          restoreCastExpressionProperties(cast, p, nodes);
        } else if (SgCallExpression *call = isSgCallExpression(expr)) {
          call->set_expression_type(restored_type);
        } else if (SgSourceLocationBuiltinExp *builtin =
                       isSgSourceLocationBuiltinExp(expr)) {
          builtin->set_expression_type(restored_type);
        } else if (SgTypeTraitBuiltinOperator *builtin =
                       isSgTypeTraitBuiltinOperator(expr)) {
          builtin->set_expression_type(restored_type);
        } else if (SgAggregateInitializer *init =
                       isSgAggregateInitializer(expr)) {
          init->set_expression_type(restored_type);
        } else if (SgConstructorInitializer *init =
                       isSgConstructorInitializer(expr)) {
          init->set_expression_type(restored_type);
        }
      }
      if (SgTypeExpression *type_expression = isSgTypeExpression(expr)) {
        SgType *represented_type =
            typeFromJson(p.at("represented_type"), nodes);
        if (represented_type == nullptr ||
            isSgTypeUnknown(represented_type) != nullptr ||
            isSgTypeDefault(represented_type) != nullptr) {
          throw std::runtime_error(
              "AST JSON SgTypeExpression has no exact represented type");
        }
        type_expression->set_represented_type(represented_type);
      }
      if (SgAggregateInitializer *init = isSgAggregateInitializer(expr)) {
        const bool has_explicit_type =
            p.requiredBool("fortran_has_source_explicit_type");
        SgType *explicit_type =
            nullableTypeFromJson(p.at("fortran_source_explicit_type"), nodes);
        const bool typed_fortran_constructor =
            init->get_source_form() ==
                SgAggregateInitializer::
                    e_aggregate_initializer_source_fortran ||
            init->get_source_form() ==
                SgAggregateInitializer::
                    e_aggregate_initializer_source_fortran_structure;
        if (has_explicit_type != (explicit_type != nullptr) ||
            (!typed_fortran_constructor && has_explicit_type)) {
          throw std::runtime_error(
              "AST JSON SgAggregateInitializer has contradictory Fortran "
              "source type-spec state");
        }
        init->set_fortran_has_source_explicit_type(has_explicit_type);
        init->set_fortran_source_explicit_type(explicit_type);
      }
      expr->set_lvalue(p.requiredBool("lvalue"));
      expr->set_need_paren(p.requiredBool("need_paren"));
      expr->set_global_qualified_name(p.requiredBool("global_qualified_name"));
      expr->set_semantic_wrapper_mask(
          static_cast<SgExpression::semantic_wrapper_mask_enum>(
              p.requiredInt("semantic_wrapper_mask")));
      const bool fortran_integer_value_available =
          p.requiredBool("fortran_integer_constant_value_is_available");
      const std::int64_t fortran_integer_value =
          p.requiredInt("fortran_integer_constant_value");
      if (!fortran_integer_value_available && fortran_integer_value != 0) {
        throw std::runtime_error(
            "AST JSON expression has a Fortran folded selector value without "
            "availability");
      }
      expr->set_fortran_integer_constant_value(fortran_integer_value);
      expr->set_fortran_integer_constant_value_is_available(
          fortran_integer_value_available);
      if (SgValueExp *value = isSgValueExp(expr)) {
        value->set_literal_spelling_form(
            requiredEnum<SgValueExp::literal_spelling_form_enum>(
                p, "literal_spelling_form", "SgValueExp",
                {SgValueExp::e_literal_source_spelled,
                 SgValueExp::e_literal_canonical_generated}));
      }
      if (SgArrowExp *arrow = isSgArrowExp(expr)) {
        const int64_t raw_role = p.requiredInt("arrow_emission_role");
        if (raw_role != SgArrowExp::e_emit_arrow_operator &&
            raw_role != SgArrowExp::e_implicit_object_access) {
          throw std::runtime_error(
              "AST JSON SgArrowExp has invalid arrow_emission_role");
        }
        arrow->set_emission_role(
            static_cast<SgArrowExp::emission_role_enum>(raw_role));
      }
      if (SgInitializer *initializer = isSgInitializer(expr)) {
        initializer->set_is_braced_initialized(
            p.requiredBool("is_braced_initialized"));
      }
      restoreExpressionQualificationFields(expr, p);
      if (SgNewExp *new_expr = isSgNewExp(expr)) {
        if (const JsonValue *type = p.find("specified_type")) {
          new_expr->set_specified_type(typeFromJson(*type, nodes));
        }
        if (const JsonValue *type = p.find("type")) {
          installNewExpressionResultType(new_expr, typeFromJson(*type, nodes),
                                         *type);
        }
        new_expr->set_need_global_specifier(
            static_cast<short>(p.requiredInt("need_global_specifier")));
        new_expr->set_type_id_is_parenthesized(
            p.requiredBool("type_id_is_parenthesized"));
      }
      if (SgDeleteExp *delete_expr = isSgDeleteExp(expr)) {
        delete_expr->set_is_array(
            static_cast<short>(p.requiredInt("is_array")));
        delete_expr->set_need_global_specifier(
            static_cast<short>(p.requiredInt("need_global_specifier")));
      }
      if (SgEnumVal *value = isSgEnumVal(expr)) {
        const uint64_t declaration_id =
            static_cast<uint64_t>(p.requiredInt("declaration"));
        if (declaration_id == 0) {
          throw std::runtime_error(
              "AST JSON SgEnumVal has no enum declaration");
        }
        value->set_declaration(
            nodeByIdAs<SgEnumDeclaration>(nodes, declaration_id));
        value->set_requiresNameQualification(
            p.requiredBool("requires_name_qualification"));
      } else if (SgTemplateParameterVal *value =
                     isSgTemplateParameterVal(expr)) {
        value->set_template_parameter_position(
            static_cast<int>(p.requiredInt("template_parameter_position")));
        value->set_valueString(p.requiredString("value_string"));
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
      const uint64_t parameter_list_id =
          requiredSingleEdgeTarget(record, "parameterList");
      SgFunctionParameterList *parameter_list =
          nodeByIdAs<SgFunctionParameterList>(nodes, parameter_list_id);
      if (decl->get_parameterList() != parameter_list ||
          parameter_list->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON function declaration has no exact parameter-list "
            "ownership");
      }
      SgFunctionType *restored_type = decl->get_type();
      if (restored_type == nullptr) {
        throw std::runtime_error(
            "AST JSON function_type is not an exact SgFunctionType");
      }
      // return_type is a serialized cross-check of the return edge already
      // owned by function_type. Reconstructing it independently would create
      // a second identity for non-interned types such as SgTemplateType.
      const JsonValue &serialized_function_return_type =
          p.at("function_type").at("return_type");
      if (!jsonValuesEqual(serialized_function_return_type,
                           p.at("return_type"))) {
        throw std::runtime_error(
            "AST JSON function_type and return_type disagree");
      }
      SgType *return_type = restored_type->get_return_type();
      if (return_type == nullptr || isSgTypeUnknown(return_type) != nullptr ||
          isSgTypeDefault(return_type) != nullptr) {
        throw std::runtime_error(
            "AST JSON function declaration has no exact canonical return "
            "type edge");
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      if (decl->get_scope() == nullptr) {
        throw std::runtime_error("AST JSON " + record.kind + " id " +
                                 std::to_string(record.id) +
                                 " has no exact semantic scope");
      }
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (name->get_scope() == nullptr) {
        throw std::runtime_error("AST JSON SgInitializedName has no scope: " +
                                 name->get_name().getString());
      }
    }
  }

  restoreRecordedScopeEdges(ast, nodes);
  type_owned_symbol_guard.resolve(nodes);

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;
    if (SgInitializedName *name = isSgInitializedName(node)) {
      name->set_fortran_source_derived_type_symbol(exactBoundSymbolFromJson(
          p.at("fortran_source_derived_type_symbol"), nodes));
    }
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(node)) {
      procedure->set_fortran_source_derived_type_symbol(
          exactBoundSymbolFromJson(p.at("fortran_source_derived_type_symbol"),
                                   nodes));
    }
    if (SgAggregateInitializer *aggregate = isSgAggregateInitializer(node)) {
      aggregate->set_fortran_source_derived_type_symbol(
          exactBoundSymbolFromJson(p.at("fortran_source_derived_type_symbol"),
                                   nodes));
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;
    if (SgVarRefExp *ref = isSgVarRefExp(node)) {
      if (const JsonValue *symbol_json = p.find("symbol")) {
        ref->set_symbol(
            isSgVariableSymbol(symbolFromJson(*symbol_json, nodes)));
      } else {
        const uint64_t decl_id =
            static_cast<uint64_t>(p.requiredInt("symbol_declaration"));
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
      validateAnonymousDataMemberReferenceQualification(ref);
    } else if (SgLabelRefExp *ref = isSgLabelRefExp(node)) {
      if (const JsonValue *symbol_json = p.find("symbol")) {
        ref->set_symbol(isSgLabelSymbol(symbolFromJson(*symbol_json, nodes)));
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgLabelRefExp symbol");
      }
    } else if (SgTemplateMemberFunctionRefExp *ref =
                   isSgTemplateMemberFunctionRefExp(node)) {
      if (ref->get_symbol() != nullptr) {
        if (ref->get_symbol()->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON preconstructed SgTemplateMemberFunctionRefExp has "
              "no exact symbol basis");
        }
      } else if (const JsonValue *symbol_json = p.find("symbol")) {
        SgTemplateMemberFunctionSymbol *symbol =
            isSgTemplateMemberFunctionSymbol(
                symbolFromJson(*symbol_json, nodes));
        validateExternalSymbolBasisOwnership(symbol);
        ref->set_symbol(symbol);
      }
      ref->set_virtual_call(static_cast<int>(p.requiredInt("virtual_call")));
      ref->set_need_qualifier(
          static_cast<int>(p.requiredInt("need_qualifier")));
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgTemplateMemberFunctionRefExp "
            "symbol");
      }
      const uint64_t semantic_id = static_cast<uint64_t>(
          p.requiredInt("semantic_member_function_declaration"));
      SgMemberFunctionDeclaration *semantic_function =
          isSgMemberFunctionDeclaration(nodeById(nodes, semantic_id));
      if (semantic_id == 0 || semantic_function == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgTemplateMemberFunctionRefExp "
            "semantic member-function declaration");
      }
      ref->set_semantic_member_function_declaration(semantic_function);
      if (ref->getAssociatedMemberFunctionDeclaration() != semantic_function) {
        throw std::runtime_error(
            "AST JSON restored an inconsistent "
            "SgTemplateMemberFunctionRefExp semantic member-function "
            "declaration");
      }
    } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
      if (ref->get_symbol_i() != nullptr) {
        if (ref->get_symbol_i()->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON preconstructed SgMemberFunctionRefExp has no exact "
              "symbol basis");
        }
        ref->set_virtual_call(static_cast<int>(p.requiredInt("virtual_call")));
        ref->set_need_qualifier(
            static_cast<int>(p.requiredInt("need_qualifier")));
        continue;
      }
      SgMemberFunctionSymbol *symbol = nullptr;
      if (const JsonValue *symbol_json = p.find("symbol")) {
        symbol = isSgMemberFunctionSymbol(symbolFromJson(*symbol_json, nodes));
        validateExternalSymbolBasisOwnership(symbol);
      } else {
        const uint64_t decl_id =
            static_cast<uint64_t>(p.requiredInt("symbol_declaration"));
        if (decl_id != 0) {
          SgMemberFunctionDeclaration *decl =
              isSgMemberFunctionDeclaration(nodeById(nodes, decl_id));
          if (decl != nullptr) {
            symbol = new SgMemberFunctionSymbol(decl);
          }
        }
      }
      ref->set_symbol_i(symbol);
      ref->set_virtual_call(static_cast<int>(p.requiredInt("virtual_call")));
      ref->set_need_qualifier(
          static_cast<int>(p.requiredInt("need_qualifier")));
      if (ref->get_symbol_i() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgMemberFunctionRefExp symbol");
      }
    } else if (SgThisExp *expr = isSgThisExp(node)) {
      const int64_t raw_class_decl_id =
          p.requiredInt("class_symbol_declaration");
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
        SgClassSymbol *symbol = classSymbolForDeclaration(declaration);
        if (symbol == nullptr || symbol->get_declaration() != declaration ||
            p.requiredString("class_symbol_name") !=
                symbol->get_name().getString() ||
            !p.requiredString("nonreal_symbol_name").empty()) {
          throw std::runtime_error(
              "AST JSON SgThisExp class symbol cross-edge is inconsistent");
        }
        expr->set_class_symbol(symbol);
      } else {
        SgNonrealDecl *declaration = nodeByIdAs<SgNonrealDecl>(
            nodes, static_cast<uint64_t>(raw_nonreal_decl_id));
        SgNonrealSymbol *symbol = nonrealSymbolForDeclaration(declaration);
        if (symbol == nullptr || symbol->get_declaration() != declaration ||
            p.requiredString("nonreal_symbol_name") !=
                symbol->get_name().getString() ||
            !p.requiredString("class_symbol_name").empty()) {
          throw std::runtime_error(
              "AST JSON SgThisExp nonreal symbol cross-edge is inconsistent");
        }
        expr->set_nonreal_symbol(symbol);
      }
      expr->set_pobj_this(static_cast<int>(p.requiredInt("pobj_this")));
    } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
      const uint64_t decl_id = requiredPositiveId(
          p.at("symbol_declaration"), "SgNonrealRefExp symbol declaration");
      SgNonrealDecl *declaration = nodeByIdAs<SgNonrealDecl>(nodes, decl_id);
      SgNonrealSymbol *symbol =
          isSgNonrealSymbol(symbolFromJson(p.at("symbol"), nodes));
      if (symbol == nullptr || symbol->get_declaration() != declaration ||
          p.requiredString("symbol_name") != symbol->get_name().getString()) {
        throw std::runtime_error(
            "AST JSON SgNonrealRefExp symbol cross-edge is inconsistent");
      }
      ref->set_symbol(symbol);
      const uint64_t resolved_function_id =
          static_cast<uint64_t>(p.requiredInt("resolved_function_declaration"));
      if (resolved_function_id != 0) {
        SgFunctionDeclaration *resolved_function =
            isSgFunctionDeclaration(nodeById(nodes, resolved_function_id));
        if (resolved_function == nullptr) {
          throw std::runtime_error(
              "AST JSON SgNonrealRefExp resolved callable is not a function "
              "declaration");
        }
        ref->set_resolved_function_declaration(resolved_function);
      }
      const uint64_t resolved_variable_id =
          static_cast<uint64_t>(p.requiredInt("resolved_variable_declaration"));
      if (resolved_variable_id != 0) {
        if (resolved_function_id != 0) {
          throw std::runtime_error(
              "AST JSON SgNonrealRefExp resolves to both a function and a "
              "variable template");
        }
        SgTemplateVariableDeclaration *resolved_variable =
            isSgTemplateVariableDeclaration(
                nodeById(nodes, resolved_variable_id));
        if (resolved_variable == nullptr ||
            resolved_variable->get_variables().size() != 1 ||
            resolved_variable->get_variables().front() == nullptr ||
            resolved_variable->get_variables().front()->get_parent() !=
                resolved_variable ||
            resolved_variable->get_variables().front()->get_type() == nullptr) {
          throw std::runtime_error(
              "AST JSON SgNonrealRefExp resolved variable template is not an "
              "exact typed specialization declaration");
        }
        ref->set_resolved_variable_declaration(resolved_variable);
      }
      const int64_t semantic_role = p.requiredInt("semantic_role");
      if (semantic_role != SgNonrealRefExp::e_nonreal_reference &&
          semantic_role != SgNonrealRefExp::e_dependent_callable) {
        throw std::runtime_error(
            "AST JSON SgNonrealRefExp has invalid semantic_role");
      }
      ref->set_semantic_role(
          static_cast<SgNonrealRefExp::semantic_role_enum>(semantic_role));
      if (const JsonValue *template_arguments = p.find("template_arguments")) {
        ref->get_templateArguments() =
            templateArgumentListFromJson(*template_arguments, nodes, ref);
      }
      ref->set_explicit_template_argument_list(
          p.requiredBool("explicit_template_argument_list"));
      ref->set_constraintSatisfactionEvaluated(
          p.requiredBool("constraint_satisfaction_evaluated"));
      ref->set_constraintSatisfactionSatisfied(
          p.requiredBool("constraint_satisfaction_satisfied"));
      ref->set_constraintSatisfactionContainsErrors(
          p.requiredBool("constraint_satisfaction_contains_errors"));
      ref->set_constraintSatisfactionSubstitutionFailure(
          p.requiredBool("constraint_satisfaction_substitution_failure"));
      ref->set_constraintSatisfactionSummary(
          p.requiredString("constraint_satisfaction_summary"));
      ref->set_sfinaeEvaluated(p.requiredBool("sfinae_evaluated"));
      ref->set_sfinaeSubstitutionFailure(
          p.requiredBool("sfinae_substitution_failure"));
      ref->set_sfinaeSummary(p.requiredString("sfinae_summary"));
    } else if (SgTemplateFunctionRefExp *ref =
                   isSgTemplateFunctionRefExp(node)) {
      if (ref->get_symbol() != nullptr) {
        if (ref->get_symbol()->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON preconstructed SgTemplateFunctionRefExp has no "
              "exact symbol basis");
        }
      } else {
        if (const JsonValue *symbol_json = p.find("symbol")) {
          SgTemplateFunctionSymbol *symbol =
              isSgTemplateFunctionSymbol(symbolFromJson(*symbol_json, nodes));
          validateExternalSymbolBasisOwnership(symbol);
          ref->set_symbol(symbol);
        } else {
          const uint64_t decl_id =
              static_cast<uint64_t>(p.requiredInt("symbol_declaration"));
          if (decl_id != 0) {
            SgFunctionDeclaration *decl =
                isSgFunctionDeclaration(nodeById(nodes, decl_id));
            if (decl != nullptr) {
              ref->set_symbol(new SgTemplateFunctionSymbol(decl));
            }
          }
        }
      }
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgTemplateFunctionRefExp symbol");
      }
      const uint64_t semantic_id =
          static_cast<uint64_t>(p.requiredInt("semantic_function_declaration"));
      SgFunctionDeclaration *semantic_function =
          isSgFunctionDeclaration(nodeById(nodes, semantic_id));
      if (semantic_id == 0 || semantic_function == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgTemplateFunctionRefExp semantic "
            "function declaration");
      }
      ref->set_semantic_function_declaration(semantic_function);
      if (ref->getAssociatedFunctionDeclaration() != semantic_function) {
        throw std::runtime_error(
            "AST JSON restored an inconsistent SgTemplateFunctionRefExp "
            "semantic function declaration");
      }
    } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
      SgSymbol *serializedSourceVisible = exactBoundSymbolFromJson(
          p.at("fortran_source_visible_symbol"), nodes);
      SgFunctionSymbol *sourceVisible =
          isSgFunctionSymbol(serializedSourceVisible);
      if (serializedSourceVisible != nullptr && sourceVisible == nullptr) {
        throw std::runtime_error(
            "AST JSON SgFunctionRefExp Fortran source-visible binding is not "
            "a function symbol");
      }
      ref->set_fortran_source_visible_symbol(sourceVisible);
      const int sourceVisibleBindingKind = static_cast<int>(
          p.requiredInt("fortran_source_visible_binding_kind"));
      switch (sourceVisibleBindingKind) {
      case SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable:
        if (sourceVisible != nullptr) {
          throw std::runtime_error(
              "AST JSON SgFunctionRefExp has a source-visible symbol without "
              "a Fortran binding kind");
        }
        break;
      case SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed:
      case SgFunctionRefExp::e_fortran_source_visible_binding_use_rename:
      case SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload:
      case SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow:
      case SgFunctionRefExp::
          e_fortran_source_visible_binding_semantic_publication:
        if (sourceVisible == nullptr) {
          throw std::runtime_error(
              "AST JSON SgFunctionRefExp has a Fortran binding kind without "
              "a source-visible symbol");
        }
        break;
      default:
        throw std::runtime_error(
            "AST JSON SgFunctionRefExp has an invalid Fortran "
            "source-visible binding kind");
      }
      ref->set_fortran_source_visible_binding_kind(
          static_cast<
              SgFunctionRefExp::fortran_source_visible_binding_kind_enum>(
              sourceVisibleBindingKind));
      const JsonValue &symbol_json = p.at("symbol");
      SgFunctionSymbol *symbol = functionReferenceSemanticSymbolFromJson(
          symbol_json, p.at("fortran_source_visible_symbol"), sourceVisible,
          nodes);
      validateExternalSymbolBasisOwnership(symbol);
      ref->set_symbol(symbol);
      if (ref->get_symbol() == nullptr ||
          ref->get_symbol()->get_symbol_basis() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgFunctionRefExp symbol");
      }
    }
  }

  // A non-type template parameter and its initialized-name declarator share
  // one semantic type identity.  Both records carry the type so malformed
  // input can be rejected locally, but type reconstruction must publish the
  // shared pointer only after the unordered node pass has finished.  Doing
  // this in the SgTemplateParameter branch lets a later SgInitializedName
  // record replace the edge with a structurally equal, independently rebuilt
  // type and makes the result depend on JSON record order.
  for (const NodeRecord &record : ast.nodes) {
    SgTemplateParameter *parameter =
        isSgTemplateParameter(nodeById(nodes, record.id));
    if (parameter == nullptr) {
      continue;
    }

    const JsonValue &properties = record.properties;
    if (parameter->get_parameterType() ==
            SgTemplateParameter::nontype_parameter &&
        parameter->get_initializedName() != nullptr) {
      const uint64_t initialized_name_id =
          static_cast<uint64_t>(properties.requiredInt("initialized_name"));
      if (initialized_name_id == 0) {
        throw std::runtime_error(
            "AST JSON non-type template parameter has an initialized-name "
            "edge without an initialized_name property");
      }
      SgInitializedName *initialized_name =
          nodeByIdAs<SgInitializedName>(nodes, initialized_name_id);
      const JsonValue &initialized_type =
          ast.node(initialized_name_id).properties.at("type");
      if (!jsonValuesEqual(properties.at("type"), initialized_type)) {
        throw std::runtime_error(
            "AST JSON non-type template parameter and initialized name "
            "serialize different semantic types");
      }
      initialized_name->set_typeptr(parameter->get_type());
    }
    validateTemplateParameterContract(parameter, "reconstruction");
  }

  applyOmpAuxiliaryState(ast, nodes);
}

void rebuildConstructorOnlyNodes(const AstFileRecord &ast, NodeMap &nodes) {
  auto require_exact_initializer_type = [](SgType *type,
                                           const std::string &kind) {
    if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
        isSgTypeDefault(type) != nullptr) {
      throw std::runtime_error("AST JSON " + kind +
                               " requires an exact stored destination type");
    }
    return type;
  };
  auto publish_delayed_node = [&](const NodeRecord &record, SgNode *node) {
    if (node == nullptr || !nodes.emplace(record.id, node).second) {
      throw std::runtime_error("AST JSON duplicate delayed node " +
                               record.kind);
    }
    setNodeSourcePosition(node, record);
    setNodeFlags(node, record);
  };
  auto record_by_id = [&](uint64_t id) -> const NodeRecord & {
    for (const NodeRecord &candidate : ast.nodes) {
      if (candidate.id == id) {
        return candidate;
      }
    }
    throw std::runtime_error("AST JSON has no node record id " +
                             std::to_string(id));
  };
  auto require_unowned_delayed_child = [&](uint64_t child_id,
                                           const NodeRecord &owner,
                                           const std::string &context) {
    auto child = nodes.find(child_id);
    if (child == nodes.end() || child->second == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " has no constructed child node");
    }
    const NodeRecord &child_record = record_by_id(child_id);
    if (requiredSingleEdgeTarget(child_record, "parent") != owner.id) {
      throw std::runtime_error("AST JSON " + context +
                               " child does not name its exact delayed owner");
    }
    if (child->second->get_parent() != nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " child already has a structural owner");
    }
    return child->second;
  };

  // Constructor initializers whose result is a dependent class type validate
  // the nonreal declaration's exact template-class association in their
  // constructor. Publish that serialized semantic edge before invoking any
  // delayed constructor; setting it afterward would only hide malformed
  // construction input.
  for (const NodeRecord &record : ast.nodes) {
    auto existing = nodes.find(record.id);
    if (existing == nodes.end()) {
      continue;
    }
    SgNonrealDecl *nonreal = isSgNonrealDecl(existing->second);
    if (nonreal == nullptr) {
      continue;
    }
    const uint64_t template_id =
        singleEdgeTarget(record, "templateDeclaration");
    SgDeclarationStatement *template_declaration =
        template_id != 0
            ? nodeByIdAs<SgDeclarationStatement>(nodes, template_id)
            : nullptr;
    if (nonreal->get_templateDeclaration() != nullptr &&
        nonreal->get_templateDeclaration() != template_declaration) {
      throw std::runtime_error(
          "AST JSON SgNonrealDecl has a conflicting constructor-time "
          "template declaration");
    }
    nonreal->set_templateDeclaration(template_declaration);
  }

  auto publish_designator_list_children = [&](uint64_t list_id,
                                              const NodeRecord &owner) {
    SgExprListExp *list = isSgExprListExp(nodeById(nodes, list_id));
    if (list == nullptr || !list->get_expressions().empty()) {
      throw std::runtime_error(
          "AST JSON SgDesignatedInitializer requires one fresh exact "
          "designator list");
    }
    const NodeRecord &list_record = record_by_id(list_id);
    const std::vector<EdgeRecord> &designator_edges =
        edgesFor(list_record, "expressions");
    if (designator_edges.empty()) {
      throw std::runtime_error(
          "AST JSON SgDesignatedInitializer has an empty designator list");
    }
    for (const EdgeRecord &edge : designator_edges) {
      SgDesignator *designator = isSgDesignator(nodeById(nodes, edge.target));
      const NodeRecord &designator_record = record_by_id(edge.target);
      if (designator == nullptr || designator->get_parent() != list ||
          requiredSingleEdgeTarget(designator_record, "parent") != list_id) {
        throw std::runtime_error(
            "AST JSON SgDesignatedInitializer designator does not name its "
            "exact list owner");
      }
      const size_t expected_index = list->get_expressions().size();
      list->append_expression(designator);
      if (list->get_expressions().size() != expected_index + 1 ||
          list->get_expressions()[expected_index] != designator ||
          designator->get_parent() != list) {
        throw std::runtime_error(
            "AST JSON SgDesignatedInitializer failed to publish its exact "
            "ordered designator child");
      }
    }
    if (requiredSingleEdgeTarget(list_record, "parent") != owner.id) {
      throw std::runtime_error(
          "AST JSON SgDesignatedInitializer list does not name its exact "
          "initializer owner");
    }
  };
  auto require_function_reference = [&](uint64_t id,
                                        const std::string &context) {
    SgExpression *reference = nodeByIdAs<SgExpression>(nodes, id);
    const NodeRecord &reference_record = record_by_id(id);
    const JsonValue &properties = reference_record.properties;
    std::string expected_symbol_kind;
    bool ordinary_function_reference = false;
    if (SgTemplateMemberFunctionRefExp *typed =
            isSgTemplateMemberFunctionRefExp(reference)) {
      if (typed->get_symbol() != nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " acquired a symbol before symbol-table "
                                 "reconstruction");
      }
      expected_symbol_kind = "SgTemplateMemberFunctionSymbol";
    } else if (SgTemplateFunctionRefExp *typed =
                   isSgTemplateFunctionRefExp(reference)) {
      if (typed->get_symbol() != nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " acquired a symbol before symbol-table "
                                 "reconstruction");
      }
      expected_symbol_kind = "SgTemplateFunctionSymbol";
    } else if (SgMemberFunctionRefExp *typed =
                   isSgMemberFunctionRefExp(reference)) {
      if (typed->get_symbol() != nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " acquired a symbol before symbol-table "
                                 "reconstruction");
      }
      expected_symbol_kind = "SgMemberFunctionSymbol";
    } else if (SgFunctionRefExp *typed = isSgFunctionRefExp(reference)) {
      if (typed->get_symbol() != nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " acquired a symbol before symbol-table "
                                 "reconstruction");
      }
      expected_symbol_kind = "SgFunctionSymbol";
      ordinary_function_reference = true;
    } else {
      throw std::runtime_error("AST JSON " + context +
                               " is not a typed function reference");
    }

    const JsonValue *serialized_binding = properties.find("symbol");
    if (serialized_binding == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " has no exact typed symbol identity");
    }
    const JsonValue &serialized_symbol = *serialized_binding;
    if (!ordinary_function_reference &&
        serialized_symbol.requiredString("symbol_kind") !=
            expected_symbol_kind) {
      throw std::runtime_error("AST JSON " + context +
                               " has no exact typed symbol identity");
    }
    const int64_t serialized_declaration =
        serialized_symbol.requiredInt("symbol_declaration");
    if (serialized_declaration < 0 ||
        properties.requiredInt("symbol_declaration") !=
            serialized_declaration) {
      throw std::runtime_error("AST JSON " + context +
                               " has inconsistent symbol declarations");
    }
    if (serialized_declaration != 0) {
      SgFunctionDeclaration *declaration = nodeByIdAs<SgFunctionDeclaration>(
          nodes, static_cast<uint64_t>(serialized_declaration));
      if (declaration->get_name().getString() !=
          serialized_symbol.requiredString("symbol_name")) {
        throw std::runtime_error("AST JSON " + context +
                                 " has inconsistent function identity");
      }
    } else if (serialized_symbol.find("external_function") == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " has no external function identity");
    }

    SgSymbol *symbol = nullptr;
    if (ordinary_function_reference) {
      const JsonValue &source_visible_record =
          properties.at("fortran_source_visible_symbol");
      SgFunctionSymbol *source_visible = isSgFunctionSymbol(
          exactBoundSymbolFromJson(source_visible_record, nodes));
      symbol = functionReferenceSemanticSymbolFromJson(
          serialized_symbol, source_visible_record, source_visible, nodes);
    } else {
      symbol = symbolFromJson(serialized_symbol, nodes);
    }
    if (symbol == nullptr ||
        (ordinary_function_reference
             ? isSgFunctionSymbol(symbol) == nullptr
             : symbol->class_name() != expected_symbol_kind)) {
      throw std::runtime_error("AST JSON " + context +
                               " failed exact typed symbol resolution");
    }
    if (serialized_declaration != 0 &&
        symbolBasis(symbol) !=
            nodeById(nodes, static_cast<uint64_t>(serialized_declaration))) {
      throw std::runtime_error("AST JSON " + context +
                               " resolved a different function declaration");
    }
    if (SgTemplateMemberFunctionRefExp *typed =
            isSgTemplateMemberFunctionRefExp(reference)) {
      typed->set_symbol(isSgTemplateMemberFunctionSymbol(symbol));
    } else if (SgTemplateFunctionRefExp *typed =
                   isSgTemplateFunctionRefExp(reference)) {
      typed->set_symbol(isSgTemplateFunctionSymbol(symbol));
    } else if (SgMemberFunctionRefExp *typed =
                   isSgMemberFunctionRefExp(reference)) {
      typed->set_symbol_i(isSgMemberFunctionSymbol(symbol));
    } else {
      isSgFunctionRefExp(reference)->set_symbol(isSgFunctionSymbol(symbol));
    }
    if (reference->get_parent() != nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " is already structurally owned");
    }
    return reference;
  };

  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgTemplateInstantiationDefn") {
      SgTemplateInstantiationDefn *definition =
          nodeByIdAs<SgTemplateInstantiationDefn>(nodes, record.id);
      SgTemplateInstantiationDecl *declaration =
          nodeByIdAs<SgTemplateInstantiationDecl>(
              nodes, requiredSingleEdgeTarget(record, "parent"));
      if (definition->get_parent() != declaration ||
          definition->get_declaration() != declaration ||
          declaration->get_definition() != definition) {
        throw std::runtime_error(
            "AST JSON template-instantiation definition lost its exact "
            "declaration owner");
      }
      continue;
    }

    if (record.kind == "SgOmpDeclareSimdStatement") {
      const uint64_t reference_id =
          requiredSingleEdgeTarget(record, "function_ref");
      SgExpression *reference = require_function_reference(
          reference_id, "SgOmpDeclareSimdStatement function_ref");
      const int64_t ordinal =
          record.properties.requiredInt("semantic_variant_ordinal");
      if (ordinal < 0 || static_cast<uint64_t>(ordinal) >
                             std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "AST JSON declare simd semantic variant ordinal is out of range");
      }
      publish_delayed_node(record, new SgOmpDeclareSimdStatement(
                                       reference,
                                       record.properties.requiredBool(
                                           "function_ref_is_explicit"),
                                       static_cast<std::size_t>(ordinal)));
      continue;
    }
    if (record.kind == "SgOmpDeclareVariantStatement") {
      SgExpression *variant_reference = require_function_reference(
          requiredSingleEdgeTarget(record, "variant_function_ref"),
          "SgOmpDeclareVariantStatement variant_function_ref");
      SgExpression *base_reference = require_function_reference(
          requiredSingleEdgeTarget(record, "base_function_ref"),
          "SgOmpDeclareVariantStatement base_function_ref");
      const int64_t ordinal =
          record.properties.requiredInt("semantic_variant_ordinal");
      if (ordinal < 0 || static_cast<uint64_t>(ordinal) >
                             std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "AST JSON declare variant semantic variant ordinal is out of "
            "range");
      }
      publish_delayed_node(record, new SgOmpDeclareVariantStatement(
                                       variant_reference, base_reference,
                                       record.properties.requiredBool(
                                           "base_function_ref_is_explicit"),
                                       static_cast<std::size_t>(ordinal)));
      continue;
    }

    if (record.kind == "SgPseudoDestructorRefExp") {
      SgType *object_type =
          typeFromJson(record.properties.at("object_type"), nodes);
      SgType *expression_type =
          typeFromJson(record.properties.at("type"), nodes);
      SgMemberFunctionType *callable_type =
          isSgMemberFunctionType(expression_type);
      if (object_type == nullptr || isSgTypeUnknown(object_type) != nullptr ||
          isSgTypeDefault(object_type) != nullptr || callable_type == nullptr ||
          isSgTypeVoid(callable_type->get_return_type()) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgPseudoDestructorRefExp has incomplete exact type "
            "identity");
      }
      publish_delayed_node(
          record, new SgPseudoDestructorRefExp(object_type, expression_type));
      continue;
    }

    const JsonValue *type = record.properties.find("type");
    if (type == nullptr) {
      continue;
    }
    if (record.kind == "SgBracedInitializer") {
      const uint64_t initializers_id =
          requiredSingleEdgeTarget(record, "initializers");
      SgExprListExp *initializers =
          isSgExprListExp(require_unowned_delayed_child(
              initializers_id, record, "SgBracedInitializer initializers"));
      if (initializers == nullptr) {
        throw std::runtime_error(
            "AST JSON SgBracedInitializer child is not an expression list");
      }
      SgType *destination_type = require_exact_initializer_type(
          typeFromJson(*type, nodes), record.kind);
      SgBracedInitializer *initializer =
          new SgBracedInitializer(initializers, destination_type);
      if (initializers->get_parent() != initializer) {
        throw std::runtime_error(
            "AST JSON SgBracedInitializer did not claim its exact list");
      }
      publish_delayed_node(record, initializer);
      continue;
    }
    if (record.kind == "SgConstructorInitializer") {
      const uint64_t arguments_id = requiredSingleEdgeTarget(record, "args");
      SgExprListExp *arguments = isSgExprListExp(require_unowned_delayed_child(
          arguments_id, record, "SgConstructorInitializer args"));
      if (arguments == nullptr) {
        throw std::runtime_error(
            "AST JSON SgConstructorInitializer args are not an expression "
            "list");
      }
      const uint64_t declaration_id = singleEdgeTarget(record, "declaration");
      SgMemberFunctionDeclaration *declaration =
          declaration_id != 0
              ? nodeByIdAs<SgMemberFunctionDeclaration>(nodes, declaration_id)
              : nullptr;
      SgConstructorInitializer *replacement = new SgConstructorInitializer(
          declaration, arguments,
          require_exact_initializer_type(typeFromJson(*type, nodes),
                                         record.kind),
          record.properties.requiredBool("need_name"),
          record.properties.requiredBool("need_qualifier"),
          record.properties.requiredBool("need_parenthesis_after_name"),
          record.properties.requiredBool("associated_class_unknown"));
      const bool qualification_present =
          record.properties.requiredBool("explicit_name_qualification_present");
      const bool global_qualification =
          record.properties.requiredBool("explicit_global_qualification");
      const SgStringList qualification_tokens = stringListFromJson(
          record.properties.at("explicit_name_qualification_tokens"),
          "SgConstructorInitializer explicit_name_qualification_tokens");
      if (!qualification_present &&
          (global_qualification || !qualification_tokens.empty())) {
        throw std::runtime_error(
            "AST JSON SgConstructorInitializer qualifier payload has no "
            "presence bit");
      }
      replacement->set_explicit_name_qualification_present(
          qualification_present);
      replacement->set_explicit_global_qualification(global_qualification);
      replacement->set_explicit_name_qualification_tokens(qualification_tokens);
      replacement->set_source_type_elaboration_kind(
          requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
              record.properties, "source_type_elaboration_kind",
              "SgConstructorInitializer",
              {SgNonrealDecl::e_source_elaboration_unspecified,
               SgNonrealDecl::e_source_elaboration_none,
               SgNonrealDecl::e_source_elaboration_typename,
               SgNonrealDecl::e_source_elaboration_class,
               SgNonrealDecl::e_source_elaboration_struct,
               SgNonrealDecl::e_source_elaboration_union,
               SgNonrealDecl::e_source_elaboration_enum}));
      replacement->set_type_elaboration_required(
          record.properties.requiredBool("type_elaboration_required"));
      if (arguments->get_parent() != replacement) {
        throw std::runtime_error(
            "AST JSON SgConstructorInitializer did not claim its exact args");
      }
      publish_delayed_node(record, replacement);
      continue;
    }
  }

  size_t remaining_assign_initializers = 0;
  for (const NodeRecord &record : ast.nodes) {
    remaining_assign_initializers += record.kind == "SgAssignInitializer";
  }
  while (remaining_assign_initializers != 0) {
    bool made_progress = false;
    for (const NodeRecord &record : ast.nodes) {
      if (record.kind != "SgAssignInitializer" ||
          nodes.find(record.id) != nodes.end()) {
        continue;
      }
      const uint64_t operand_id = requiredSingleEdgeTarget(record, "operand_i");
      auto operand = nodes.find(operand_id);
      if (operand == nodes.end()) {
        continue;
      }
      SgExpression *operand_expression =
          isSgExpression(require_unowned_delayed_child(
              operand_id, record, "SgAssignInitializer operand"));
      if (operand_expression == nullptr) {
        throw std::runtime_error(
            "AST JSON SgAssignInitializer operand is not an expression");
      }
      const JsonValue &type = record.properties.at("type");
      SgType *destination_type = require_exact_initializer_type(
          typeFromJson(type, nodes), record.kind);
      SgAssignInitializer *initializer =
          new SgAssignInitializer(operand_expression, destination_type);
      if (operand_expression->get_parent() != initializer) {
        throw std::runtime_error(
            "AST JSON SgAssignInitializer did not claim its exact operand");
      }
      initializer->set_source_form(requiredEnum<
                                   SgAssignInitializer::
                                       assignment_initializer_source_form_enum>(
          record.properties, "assignment_initializer_source_form",
          "SgAssignInitializer",
          {SgAssignInitializer::e_assignment_initializer_source_ast,
           SgAssignInitializer::
               e_assignment_initializer_source_include_operand_expansion,
           SgAssignInitializer::
               e_assignment_initializer_source_include_complete_expansion}));
      publish_delayed_node(record, initializer);
      --remaining_assign_initializers;
      made_progress = true;
    }
    if (!made_progress) {
      throw std::runtime_error(
          "AST JSON SgAssignInitializer has a missing or cyclic operand");
    }
  }

  size_t remaining_designated_initializers = 0;
  for (const NodeRecord &record : ast.nodes) {
    remaining_designated_initializers +=
        record.kind == "SgDesignatedInitializer";
  }
  while (remaining_designated_initializers != 0) {
    bool made_progress = false;
    for (const NodeRecord &record : ast.nodes) {
      if (record.kind != "SgDesignatedInitializer" ||
          nodes.find(record.id) != nodes.end()) {
        continue;
      }
      const uint64_t designators_target =
          requiredSingleEdgeTarget(record, "designatorList");
      const uint64_t member_target =
          requiredSingleEdgeTarget(record, "memberInit");
      if (nodes.find(designators_target) == nodes.end() ||
          nodes.find(member_target) == nodes.end()) {
        continue;
      }
      publish_designator_list_children(designators_target, record);
      SgExprListExp *designators =
          isSgExprListExp(require_unowned_delayed_child(
              designators_target, record,
              "SgDesignatedInitializer designatorList"));
      SgInitializer *member = isSgInitializer(require_unowned_delayed_child(
          member_target, record, "SgDesignatedInitializer memberInit"));
      if (designators == nullptr || member == nullptr) {
        throw std::runtime_error(
            "AST JSON SgDesignatedInitializer has invalid typed children");
      }
      SgDesignatedInitializer *initializer =
          new SgDesignatedInitializer(designators, member);
      if (designators->get_parent() != initializer ||
          member->get_parent() != initializer) {
        throw std::runtime_error(
            "AST JSON SgDesignatedInitializer did not claim its exact "
            "children");
      }
      publish_delayed_node(record, initializer);
      --remaining_designated_initializers;
      made_progress = true;
    }
    if (!made_progress) {
      throw std::runtime_error(
          "AST JSON SgDesignatedInitializer has missing or cyclic children");
    }
  }
}

void restoreDeclarationIdentityEdges(
    const AstFileRecord &ast, const NodeMap &nodes, SgProject *project,
    std::unordered_set<uint64_t> &restored_declarations,
    bool require_complete) {
  auto external_peer_needs_internal_symbol_tables =
      [](const JsonValue &external_reference) {
        const JsonValue *external_function =
            external_reference.find("external_function");
        if (external_function == nullptr) {
          return false;
        }
        const JsonValue *parameter_scope =
            external_function->find("function_parameter_scope");
        if (parameter_scope == nullptr ||
            !parameter_scope->requiredBool("present")) {
          return false;
        }
        const JsonValue *symbol_table = parameter_scope->find("symbol_table");
        if (symbol_table == nullptr ||
            symbol_table->kind != JsonValue::Kind::Array) {
          return false;
        }
        for (const JsonValue &entry : symbol_table->array) {
          const std::string kind = entry.requiredString("symbol_kind");
          const char *dependency_field = nullptr;
          if (kind == "SgAliasSymbol") {
            dependency_field = "alias_target";
          } else if (kind == "SgRenameSymbol") {
            dependency_field = "original_symbol";
          }
          if (dependency_field == nullptr) {
            continue;
          }
          const JsonValue *dependency = entry.find(dependency_field);
          if (dependency != nullptr &&
              dependency->requiredInt("symbol_declaration") != 0) {
            return true;
          }
        }
        return false;
      };
  auto restore_external_peer_context = [&](SgDeclarationStatement *owner,
                                           SgDeclarationStatement *peer,
                                           const JsonValue &json,
                                           const std::string &context) {
    if (owner == nullptr || peer == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " reconstructed a null declaration peer");
    }
    if (owner->variantT() != peer->variantT() ||
        SageInterface::get_name(owner) != SageInterface::get_name(peer)) {
      throw std::runtime_error("AST JSON " + context +
                               " is not the same declaration identity");
    }

    const std::string scope_source = json.requiredString("scope_source");
    const int64_t raw_scope_id = json.requiredInt("scope");
    SgScopeStatement *scope = nullptr;
    if (scope_source == "node") {
      if (raw_scope_id <= 0) {
        throw std::runtime_error("AST JSON " + context +
                                 " has no exact serialized scope");
      }
      scope = nodeByIdAs<SgScopeStatement>(nodes,
                                           static_cast<uint64_t>(raw_scope_id));
    } else if (scope_source == "redeclaration_owner") {
      if (raw_scope_id != 0 || owner->get_scope() == nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid owner-scope contract");
      }
      scope = owner->get_scope();
    } else {
      throw std::runtime_error("AST JSON " + context +
                               " has an invalid scope_source");
    }
    peer->set_scope(scope);

    const std::string parent_source = json.requiredString("parent_source");
    const int64_t raw_parent_id = json.requiredInt("parent");
    SgNode *parent = nullptr;
    if (parent_source == "node") {
      if (raw_parent_id <= 0) {
        throw std::runtime_error("AST JSON " + context +
                                 " has no exact serialized parent");
      }
      parent = nodeById(nodes, static_cast<uint64_t>(raw_parent_id));
    } else if (parent_source == "redeclaration_owner_parent") {
      if (raw_parent_id != 0 || owner->get_parent() == nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid owner-parent contract");
      }
      parent = owner->get_parent();
    } else if (parent_source == "project") {
      if (raw_parent_id != 0 || project == nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid project-parent contract");
      }
      parent = project;
    } else {
      throw std::runtime_error("AST JSON " + context +
                               " has an invalid parent_source");
    }
    peer->set_parent(parent);

    if (peer->get_scope() != scope || peer->get_parent() != parent) {
      throw std::runtime_error("AST JSON " + context +
                               " failed exact context reconstruction");
    }
  };

  for (const NodeRecord &record : ast.nodes) {
    if (restored_declarations.find(record.id) != restored_declarations.end()) {
      continue;
    }
    auto node = nodes.find(record.id);
    if (node == nodes.end()) {
      if (require_complete) {
        throw std::runtime_error(
            "AST JSON declaration identity pass is missing node id " +
            std::to_string(record.id));
      }
      continue;
    }
    SgDeclarationStatement *decl = isSgDeclarationStatement(node->second);
    if (decl == nullptr) {
      restored_declarations.insert(record.id);
      continue;
    }

    auto dependency_is_available = [&](uint64_t target) {
      return target == 0 || nodes.find(target) != nodes.end();
    };
    bool dependencies_available = true;
    for (const char *field : {"parent", "scope", "firstNondefiningDeclaration",
                              "definingDeclaration"}) {
      dependencies_available &=
          dependency_is_available(singleEdgeTarget(record, field));
    }
    for (const char *field : {"external_first_nondefining_declaration",
                              "external_defining_declaration"}) {
      const JsonValue *external_json = record.properties.find(field);
      if (external_json == nullptr || !external_json->requiredBool("present")) {
        continue;
      }
      if (!require_complete &&
          external_peer_needs_internal_symbol_tables(*external_json)) {
        dependencies_available = false;
      }
      const std::string context = record.kind + "." + field;
      const std::string scope_source =
          external_json->requiredString("scope_source");
      const int64_t raw_scope = external_json->requiredInt("scope");
      if (scope_source == "node") {
        if (raw_scope <= 0) {
          throw std::runtime_error("AST JSON " + context +
                                   " has no exact serialized scope");
        }
        dependencies_available &=
            dependency_is_available(static_cast<uint64_t>(raw_scope));
      } else if (scope_source == "redeclaration_owner") {
        if (raw_scope != 0) {
          throw std::runtime_error("AST JSON " + context +
                                   " has an invalid owner-scope contract");
        }
      } else {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid scope_source");
      }

      const std::string parent_source =
          external_json->requiredString("parent_source");
      const int64_t raw_parent = external_json->requiredInt("parent");
      if (parent_source == "node") {
        if (raw_parent <= 0) {
          throw std::runtime_error("AST JSON " + context +
                                   " has no exact serialized parent");
        }
        dependencies_available &=
            dependency_is_available(static_cast<uint64_t>(raw_parent));
      } else if (parent_source == "redeclaration_owner_parent" ||
                 parent_source == "project") {
        if (raw_parent != 0) {
          throw std::runtime_error("AST JSON " + context +
                                   " has an invalid parent-source contract");
        }
      } else {
        throw std::runtime_error("AST JSON " + context +
                                 " has an invalid parent_source");
      }
    }
    if (!dependencies_available) {
      if (require_complete) {
        throw std::runtime_error(
            "AST JSON declaration identity has an unavailable structural "
            "dependency for node id " +
            std::to_string(record.id));
      }
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
    } else if (const JsonValue *external_json = record.properties.find(
                   "external_first_nondefining_declaration");
               external_json != nullptr &&
               external_json->requiredBool("present")) {
      const std::string context = record.kind + ".firstNondefiningDeclaration";
      SgDeclarationStatement *external =
          externalDeclarationReferenceFromJson(external_json, nodes, context);
      restore_external_peer_context(decl, external, *external_json, context);
      decl->set_firstNondefiningDeclaration(external);
    } else {
      // Declaration constructors commonly install self as a provisional
      // canonical identity.  An absent serialized edge is an exact null
      // identity, not permission to retain that constructor default.
      decl->set_firstNondefiningDeclaration(nullptr);
    }
    if (uint64_t target = singleEdgeTarget(record, "definingDeclaration")) {
      decl->set_definingDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (const JsonValue *external_json =
                   record.properties.find("external_defining_declaration");
               external_json != nullptr &&
               external_json->requiredBool("present")) {
      const std::string context = record.kind + ".definingDeclaration";
      SgDeclarationStatement *external =
          externalDeclarationReferenceFromJson(external_json, nodes, context);
      restore_external_peer_context(decl, external, *external_json, context);
      decl->set_definingDeclaration(external);
    } else {
      decl->set_definingDeclaration(nullptr);
    }
    restored_declarations.insert(record.id);
  }
}

bool declarationSpecializationKind(
    SgDeclarationStatement *declaration,
    SgDeclarationStatement::template_specialization_enum &kind) {
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(declaration)) {
    kind = variable->get_specialization();
    return true;
  }
  if (SgClassDeclaration *class_declaration =
          isSgClassDeclaration(declaration)) {
    kind = class_declaration->get_specialization();
    return true;
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration)) {
    kind = function->get_specialization();
    return true;
  }
  return false;
}

void validateDeclarationSpecializationFamilies(const NodeMap &nodes) {
  for (const auto &[id, node] : nodes) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    SgDeclarationStatement::template_specialization_enum specialization;
    if (declaration == nullptr ||
        !declarationSpecializationKind(declaration, specialization)) {
      continue;
    }
    const std::string node_id = std::to_string(id);

    auto require_matching_peer = [&](SgDeclarationStatement *peer,
                                     const char *role) {
      if (peer == nullptr) {
        return;
      }
      SgDeclarationStatement::template_specialization_enum peer_kind;
      if (!declarationSpecializationKind(peer, peer_kind) ||
          peer_kind != specialization) {
        throw std::runtime_error(
            "AST JSON declaration specialization family is malformed: " +
            declaration->class_name() + " node " + node_id +
            " disagrees with its " + role);
      }
    };
    require_matching_peer(isSgDeclarationStatement(
                              declaration->get_firstNondefiningDeclaration()),
                          "first nondefining declaration");
    require_matching_peer(
        isSgDeclarationStatement(declaration->get_definingDeclaration()),
        "defining declaration");
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

void publishAnonymousFortranProgramUnitSymbolKeys(const AstFileRecord &ast,
                                                  const NodeMap &nodes) {
  std::unordered_map<SgFunctionDeclaration *, SgName> chain_keys;
  for (const NodeRecord &record : ast.nodes) {
    const auto restored = nodes.find(record.id);
    if (restored == nodes.end()) {
      continue;
    }
    SgFunctionDeclaration *declaration =
        isSgFunctionDeclaration(restored->second);
    if (declaration == nullptr) {
      continue;
    }
    const bool anonymous =
        (isSgProgramHeaderStatement(declaration) != nullptr ||
         isSgProcedureHeaderStatement(declaration) != nullptr) &&
        SageInterface::isFortranProgramUnitWithoutSourceName(declaration);
    if (!anonymous) {
      if (!declaration->get_fortran_anonymous_program_unit_symbol_key()
               .is_null()) {
        throw std::runtime_error(
            "AST JSON named function owns an anonymous program-unit key");
      }
      continue;
    }
    if (!declaration->get_fortran_anonymous_program_unit_symbol_key()
             .is_null()) {
      throw std::runtime_error(
          "AST JSON anonymous program-unit key was published before its "
          "exact declaration identity and source anchor");
    }

    SgFunctionDeclaration *basis =
        isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
    if (basis == nullptr) {
      basis = declaration;
    }
    SgFunctionDeclaration *source_anchor =
        isSgFunctionDeclaration(declaration->get_definingDeclaration());
    if (source_anchor == nullptr) {
      source_anchor = declaration;
    }
    Sg_File_Info *position = source_anchor->get_startOfConstruct();
    if (position == nullptr || position->get_line() <= 0 ||
        position->get_col() <= 0 || position->get_parent() != source_anchor) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran program unit has no exact source "
          "anchor for symbol-key publication");
    }
    const bool implicit_program =
        isSgProgramHeaderStatement(declaration) != nullptr;
    if (implicit_program != (isSgProgramHeaderStatement(basis) != nullptr)) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran declaration chain changes program-unit "
          "kind");
    }
    const char *prefix = implicit_program
                             ? "__rex_internal_implicit_program_"
                             : "__rex_internal_unnamed_block_data_";
    const SgName key(std::string(prefix) +
                     std::to_string(position->get_line()) + "_" +
                     std::to_string(position->get_col()));
    auto [chain, inserted] = chain_keys.emplace(basis, key);
    if (!inserted && chain->second != key) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran declaration chain has divergent exact "
          "source anchors");
    }
    declaration->initialize_fortran_anonymous_program_unit_symbol_key(key);
  }
}

void finalizeAuxiliaryDeclarationContainers(const AstFileRecord &ast,
                                            const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgDeclarationScopeList *container = isSgDeclarationScopeList(node)) {
      SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
      if (owner == nullptr ||
          owner->get_auxiliary_declaration_scopes() != container) {
        throw std::runtime_error(
            "AST JSON SgDeclarationScopeList has no lexical owner");
      }
      SgDeclarationScope *default_nonreal_scope = nullptr;
      for (SgDeclarationScope *scope : container->get_scopes()) {
        if (scope == nullptr || scope->get_parent() != container ||
            std::count(container->get_scopes().begin(),
                       container->get_scopes().end(), scope) != 1) {
          throw std::runtime_error(
              "AST JSON SgDeclarationScopeList has malformed ownership");
        }
        if (scope->get_is_default_nonreal_scope()) {
          if (default_nonreal_scope != nullptr) {
            throw std::runtime_error(
                "AST JSON lexical scope owns multiple default nonreal scopes");
          }
          default_nonreal_scope = scope;
        }
      }
    }
    if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
      if (scope->get_is_default_nonreal_scope()) {
        SgDeclarationScopeList *container =
            isSgDeclarationScopeList(scope->get_parent());
        SgScopeStatement *owner =
            container != nullptr ? isSgScopeStatement(container->get_parent())
                                 : nullptr;
        if (container == nullptr || owner == nullptr ||
            owner->get_auxiliary_declaration_scopes() != container ||
            std::count(container->get_scopes().begin(),
                       container->get_scopes().end(), scope) != 1) {
          throw std::runtime_error(
              "AST JSON default nonreal scope has no exact lexical owner");
        }
      }
    }
    if (SgAuxiliaryDeclarationList *container =
            isSgAuxiliaryDeclarationList(node)) {
      container->validate_semantic_non_output_role();
      SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
      if (owner == nullptr ||
          owner->get_auxiliary_declarations() != container) {
        throw std::runtime_error(
            "AST JSON SgAuxiliaryDeclarationList has no lexical owner");
      }
      for (SgDeclarationStatement *declaration :
           container->get_declarations()) {
        if (declaration == nullptr ||
            std::count(container->get_declarations().begin(),
                       container->get_declarations().end(), declaration) != 1 ||
            declaration->get_parent() != container) {
          throw std::runtime_error(
              "AST JSON SgAuxiliaryDeclarationList has malformed ownership");
        }
        if (declaration->get_scope() != owner) {
          throw std::runtime_error(
              "AST JSON auxiliary declaration has inconsistent lexical "
              "scope");
        }
        if (owner->statementExistsInScope(declaration)) {
          throw std::runtime_error(
              "AST JSON auxiliary declaration also has source-emission "
              "ownership");
        }
      }
    }
  }
}

void validateDeclarationGroups(const AstFileRecord &ast, const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    if (SgDeclarationGroupStatement *group =
            isSgDeclarationGroupStatement(nodeById(nodes, record.id))) {
      group->validate();
    }
  }
}

void validateAccessLabels(const AstFileRecord &ast, const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgAccessLabelStatement *label =
        isSgAccessLabelStatement(nodeById(nodes, record.id));
    if (label == nullptr) {
      continue;
    }
    label->validate();
    SgClassDefinition *owner = isSgClassDefinition(label->get_parent());
    if (owner == nullptr || label->get_scope() != owner ||
        std::count(owner->get_members().begin(), owner->get_members().end(),
                   label) != 1) {
      throw std::runtime_error(
          "AST JSON access label has no exact lexical class-member owner");
    }
  }
}

struct FortranTypeContractSurface {
  SgType *base = nullptr;
  size_t ordinary_rank = 0;
  bool pointer = false;
};

FortranTypeContractSurface fortranTypeContractSurface(SgType *type) {
  FortranTypeContractSurface result;
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      result.pointer = true;
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        result.ordinary_rank += array->get_rank();
      }
      type = array->get_base_type();
    } else {
      result.base = type;
      break;
    }
  }
  return result;
}

bool exactFortranSourceDerivedTypeBinding(SgSymbol *symbol,
                                          SgClassType *expected_type) {
  if (expected_type == nullptr) {
    return symbol == nullptr;
  }
  if (symbol == nullptr || symbol->get_name().getString().empty()) {
    return false;
  }
  std::unordered_set<SgAliasSymbol *> aliases;
  while (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
    if (!aliases.insert(alias).second || alias->get_alias() == nullptr) {
      return false;
    }
    symbol = alias->get_alias();
  }
  SgClassSymbol *class_symbol = isSgClassSymbol(symbol);
  SgClassDeclaration *declaration =
      class_symbol != nullptr ? class_symbol->get_declaration() : nullptr;
  return declaration != nullptr && declaration->get_type() == expected_type;
}

bool validFortranSeparatePointerOwnership(const SgInitializedName *name) {
  SgStatement *owner = name != nullptr
                           ? name->get_fortran_separate_pointer_declaration()
                           : nullptr;
  if (owner == nullptr) {
    return true;
  }
  SgAttributeSpecificationStatement *pointer =
      isSgAttributeSpecificationStatement(owner);
  SgExprListExp *parameters =
      pointer != nullptr ? pointer->get_parameter_list() : nullptr;
  const FortranTypeContractSurface source =
      fortranTypeContractSurface(name->get_fortran_source_type());
  const FortranTypeContractSurface semantic =
      fortranTypeContractSurface(name->get_type());
  if (pointer == nullptr ||
      pointer->get_attribute_kind() !=
          SgAttributeSpecificationStatement::e_pointerStatement ||
      pointer->get_scope() == nullptr ||
      pointer->get_scope() != name->get_scope() || parameters == nullptr ||
      parameters->get_parent() != pointer ||
      parameters->get_expressions().empty() || source.base == nullptr ||
      semantic.base == nullptr || source.pointer || !semantic.pointer) {
    return false;
  }
  size_t matches = 0;
  for (SgExpression *item : parameters->get_expressions()) {
    SgPntrArrRefExp *array = isSgPntrArrRefExp(item);
    SgVarRefExp *reference = array != nullptr
                                 ? isSgVarRefExp(array->get_lhs_operand())
                                 : isSgVarRefExp(item);
    SgVariableSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    SgInitializedName *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (item == nullptr || item->get_parent() != parameters ||
        declaration == nullptr ||
        declaration->get_fortran_separate_pointer_declaration() != pointer) {
      return false;
    }
    if (array != nullptr) {
      SgExprListExp *shape = isSgExprListExp(array->get_rhs_operand());
      if (reference->get_parent() != array || shape == nullptr ||
          shape->get_parent() != array || shape->get_expressions().empty() ||
          declaration->get_fortran_separate_shape_declaration() != pointer) {
        return false;
      }
    }
    if (declaration == name) {
      ++matches;
    }
  }
  return matches == 1;
}

bool validFortranTypeContract(const SgInitializedName *name) {
  SgType *source = name != nullptr ? name->get_fortran_source_type() : nullptr;
  SgType *semantic = name != nullptr ? name->get_type() : nullptr;
  if (isSgTypeCrayPointer(source) != nullptr) {
    const FortranTypeContractSurface semantic_surface =
        fortranTypeContractSurface(semantic);
    SgInitializedName *pointee = name->get_cray_pointer_pointee();
    SgExprListExp *shape = name->get_fortran_cray_pointer_pointee_shape();
    SgVariableDeclaration *pointer_declaration =
        isSgVariableDeclaration(name->get_parent());
    const FortranTypeContractSurface pointee_surface =
        fortranTypeContractSurface(pointee != nullptr ? pointee->get_type()
                                                      : nullptr);
    const bool valid_shape =
        shape == nullptr
            ? (pointee == nullptr ||
               pointee->get_fortran_separate_shape_declaration() !=
                   pointer_declaration)
            : (pointee != nullptr && pointer_declaration != nullptr &&
               shape->get_parent() == name &&
               !shape->get_expressions().empty() &&
               pointee_surface.ordinary_rank > 0 &&
               shape->get_expressions().size() ==
                   pointee_surface.ordinary_rank &&
               pointee->get_fortran_separate_shape_declaration() ==
                   pointer_declaration &&
               pointee->get_shapeDeferred());
    return source->get_fortran_source_syntax() &&
           semantic_surface.base != nullptr &&
           (isSgTypeInt(semantic_surface.base) != nullptr ||
            isSgTypeSignedInt(semantic_surface.base) != nullptr) &&
           semantic_surface.ordinary_rank == 0 && !semantic_surface.pointer &&
           pointee != nullptr &&
           isSgTypeCrayPointer(pointee->get_fortran_source_type()) == nullptr &&
           name->get_fortran_source_derived_type_symbol() == nullptr &&
           valid_shape;
  }
  if (name->get_cray_pointer_pointee() != nullptr ||
      name->get_fortran_cray_pointer_pointee_shape() != nullptr) {
    return false;
  }
  const FortranTypeContractSurface source_surface =
      fortranTypeContractSurface(source);
  const FortranTypeContractSurface semantic_surface =
      fortranTypeContractSurface(semantic);
  const bool source_assumed =
      isSgTypeFortranAssumed(source_surface.base) != nullptr;
  const bool source_unlimited =
      isSgTypeFortranUnlimitedPolymorphic(source_surface.base) != nullptr;
  const bool source_derived = isSgClassType(source_surface.base) != nullptr;
  if ((source_assumed &&
       name->get_fortran_type_spec() !=
           SgInitializedName::e_fortran_type_spec_type_star) ||
      (source_unlimited &&
       name->get_fortran_type_spec() !=
           SgInitializedName::e_fortran_type_spec_class_star) ||
      (!source_assumed && !source_unlimited &&
       (name->get_fortran_type_spec() ==
            SgInitializedName::e_fortran_type_spec_type_star ||
        name->get_fortran_type_spec() ==
            SgInitializedName::e_fortran_type_spec_class_star)) ||
      (source_derived &&
       name->get_fortran_type_spec() !=
           SgInitializedName::e_fortran_type_spec_type &&
       name->get_fortran_type_spec() !=
           SgInitializedName::e_fortran_type_spec_class) ||
      (!source_derived && (name->get_fortran_type_spec() ==
                               SgInitializedName::e_fortran_type_spec_type ||
                           name->get_fortran_type_spec() ==
                               SgInitializedName::e_fortran_type_spec_class))) {
    return false;
  }
  SgStatement *separate_shape_owner =
      name != nullptr ? name->get_fortran_separate_shape_declaration()
                      : nullptr;
  SgAttributeSpecificationStatement *attribute_owner =
      isSgAttributeSpecificationStatement(separate_shape_owner);
  SgVariableDeclaration *cray_pointer_owner =
      isSgVariableDeclaration(separate_shape_owner);
  const bool separate_statement_owns_shape = separate_shape_owner != nullptr;
  if (separate_statement_owns_shape &&
      (separate_shape_owner->get_scope() == nullptr ||
       separate_shape_owner->get_scope() != name->get_scope() ||
       (attribute_owner == nullptr &&
        isSgCommonBlock(separate_shape_owner) == nullptr &&
        cray_pointer_owner == nullptr) ||
       (attribute_owner != nullptr &&
        attribute_owner->get_attribute_kind() !=
            SgAttributeSpecificationStatement::e_dimensionStatement &&
        attribute_owner->get_attribute_kind() !=
            SgAttributeSpecificationStatement::e_allocatableStatement &&
        attribute_owner->get_attribute_kind() !=
            SgAttributeSpecificationStatement::e_pointerStatement))) {
    return false;
  }
  if (cray_pointer_owner != nullptr) {
    if (cray_pointer_owner->get_variables().size() != 1) {
      return false;
    }
    SgInitializedName *pointer = cray_pointer_owner->get_variables().front();
    SgExprListExp *shape =
        pointer != nullptr ? pointer->get_fortran_cray_pointer_pointee_shape()
                           : nullptr;
    if (pointer == nullptr ||
        isSgTypeCrayPointer(pointer->get_fortran_source_type()) == nullptr ||
        pointer->get_cray_pointer_pointee() != name || shape == nullptr ||
        shape->get_parent() != pointer || shape->get_expressions().empty() ||
        shape->get_expressions().size() != semantic_surface.ordinary_rank) {
      return false;
    }
  }
  const bool separate_pointer_owner =
      name->get_fortran_separate_pointer_declaration() != nullptr;
  return validFortranSeparatePointerOwnership(name) &&
         source_surface.base != nullptr && semantic_surface.base != nullptr &&
         (source_surface.pointer == semantic_surface.pointer ||
          (separate_pointer_owner && !source_surface.pointer &&
           semantic_surface.pointer)) &&
         (source_surface.ordinary_rank == semantic_surface.ordinary_rank ||
          (separate_statement_owns_shape && source_surface.ordinary_rank == 0 &&
           semantic_surface.ordinary_rank > 0)) &&
         exactFortranSourceDerivedTypeBinding(
             name->get_fortran_source_derived_type_symbol(),
             isSgClassType(source_surface.base)) &&
         SageInterface::fortranSourceTypeMatchesSemanticType(
             source_surface.base, semantic_surface.base);
}

void validateFortranTypeContracts(const AstFileRecord &ast,
                                  const NodeMap &nodes) {
  SgSourceFile *file = isSgSourceFile(nodeById(nodes, ast.root_id));
  if (file == nullptr ||
      file->get_inputLanguage() != SgFile::e_Fortran_language) {
    return;
  }
  for (const NodeRecord &record : ast.nodes) {
    if (SgInitializedName *parameter =
            isSgInitializedName(nodeById(nodes, record.id));
        parameter != nullptr &&
        isSgFunctionParameterList(parameter->get_parent()) != nullptr &&
        (parameter->get_fortran_source_type() != nullptr ||
         parameter->get_fortran_source_derived_type_symbol() != nullptr ||
         parameter->get_fortran_type_spec() !=
             SgInitializedName::e_fortran_type_spec_default ||
         !parameter->get_fortran_procedure_interface().is_null() ||
         parameter->get_fortran_separate_shape_declaration() != nullptr ||
         parameter->get_fortran_separate_pointer_declaration() != nullptr ||
         parameter->get_cray_pointer_pointee() != nullptr ||
         parameter->get_fortran_cray_pointer_pointee_shape() != nullptr ||
         parameter->get_shapeDeferred())) {
      throw std::runtime_error(
          "AST JSON Fortran procedure parameter owns declaration-statement "
          "source syntax");
    }
    if (SgAggregateInitializer *aggregate =
            isSgAggregateInitializer(nodeById(nodes, record.id))) {
      const bool has_explicit_type =
          aggregate->get_fortran_has_source_explicit_type();
      SgType *explicit_type = aggregate->get_fortran_source_explicit_type();
      const auto source_form = aggregate->get_source_form();
      const bool is_array_constructor =
          source_form ==
          SgAggregateInitializer::e_aggregate_initializer_source_fortran;
      const bool is_structure_constructor =
          source_form == SgAggregateInitializer::
                             e_aggregate_initializer_source_fortran_structure;
      if (has_explicit_type != (explicit_type != nullptr) ||
          ((!is_array_constructor && !is_structure_constructor) &&
           has_explicit_type)) {
        throw std::runtime_error(
            "AST JSON Fortran constructor has contradictory explicit "
            "source type-spec state");
      }
      if (has_explicit_type) {
        const FortranTypeContractSurface source_surface =
            fortranTypeContractSurface(explicit_type);
        const FortranTypeContractSurface semantic_surface =
            fortranTypeContractSurface(aggregate->get_expression_type());
        const bool rank_contract = is_array_constructor
                                       ? (source_surface.ordinary_rank == 0 &&
                                          semantic_surface.ordinary_rank != 0)
                                       : (is_structure_constructor &&
                                          source_surface.ordinary_rank == 0 &&
                                          semantic_surface.ordinary_rank == 0);
        if (source_surface.base == nullptr ||
            semantic_surface.base == nullptr || !rank_contract ||
            !SageInterface::fortranSourceTypeMatchesSemanticType(
                source_surface.base, semantic_surface.base)) {
          throw std::runtime_error(
              "AST JSON Fortran constructor has a contradictory explicit "
              "source/semantic type contract");
        }
        if (!exactFortranSourceDerivedTypeBinding(
                aggregate->get_fortran_source_derived_type_symbol(),
                isSgClassType(source_surface.base))) {
          throw std::runtime_error(
              "AST JSON Fortran constructor has a contradictory exact "
              "source derived-type binding");
        }
        if (is_structure_constructor &&
            aggregate->get_fortran_source_explicit_type() !=
                aggregate->get_expression_type()) {
          throw std::runtime_error(
              "AST JSON Fortran structure constructor split its exact "
              "source and semantic derived type identity");
        }
      } else if (aggregate->get_fortran_source_derived_type_symbol() !=
                 nullptr) {
        throw std::runtime_error(
            "AST JSON Fortran constructor without an explicit type "
            "owns a source derived-type binding");
      } else if (is_structure_constructor) {
        throw std::runtime_error(
            "AST JSON Fortran structure constructor has no explicit source "
            "type identity");
      }
    }
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(nodeById(nodes, record.id))) {
      const auto source_form = procedure->get_fortran_procedure_source_form();
      const bool typed_source =
          source_form == SgProcedureHeaderStatement::
                             e_fortran_procedure_source_form_type_declaration ||
          source_form == SgProcedureHeaderStatement::
                             e_fortran_procedure_source_form_type_external;
      const bool source_header =
          source_form == SgProcedureHeaderStatement::
                             e_fortran_procedure_source_form_header ||
          source_form ==
              SgProcedureHeaderStatement::
                  e_fortran_procedure_source_form_compiler_module_header;
      const auto result_type_spec = procedure->get_fortran_result_type_spec();
      const bool has_explicit_result_surface =
          typed_source ||
          (source_header &&
           result_type_spec !=
               SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown);
      if (!has_explicit_result_surface &&
          (result_type_spec !=
               SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown ||
           procedure->get_type_syntax() != nullptr ||
           procedure->get_type_syntax_is_available() ||
           procedure->get_fortran_source_derived_type_symbol() != nullptr)) {
        throw std::runtime_error(
            "AST JSON Fortran procedure without an explicit result surface "
            "owns source result type state");
      }
      if (has_explicit_result_surface &&
          (!procedure->get_type_syntax_is_available() ||
           !SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
               procedure->get_type_syntax(), procedure->get_type()))) {
        throw std::runtime_error(
            "AST JSON Fortran procedure has a contradictory explicit "
            "source/semantic result type contract");
      }
      if (has_explicit_result_surface) {
        SgType *source_result = procedure->get_type_syntax()->get_return_type();
        while (source_result != nullptr) {
          if (SgModifierType *modifier = isSgModifierType(source_result)) {
            source_result = modifier->get_base_type();
          } else if (SgPointerType *pointer = isSgPointerType(source_result)) {
            source_result = pointer->get_base_type();
          } else if (SgArrayType *array = isSgArrayType(source_result)) {
            source_result = array->get_base_type();
          } else {
            break;
          }
        }
        const bool valid_result_spec =
            isSgClassType(source_result) != nullptr
                ? (result_type_spec == SgProcedureHeaderStatement::
                                           e_fortran_result_type_spec_type ||
                   result_type_spec == SgProcedureHeaderStatement::
                                           e_fortran_result_type_spec_class)
            : isSgTypeFortranAssumed(source_result) != nullptr
                ? result_type_spec == SgProcedureHeaderStatement::
                                          e_fortran_result_type_spec_type_star
            : isSgTypeFortranUnlimitedPolymorphic(source_result) != nullptr
                ? result_type_spec == SgProcedureHeaderStatement::
                                          e_fortran_result_type_spec_class_star
                : result_type_spec == SgProcedureHeaderStatement::
                                          e_fortran_result_type_spec_intrinsic;
        if (!valid_result_spec) {
          throw std::runtime_error(
              "AST JSON Fortran procedure has a contradictory result "
              "source type-spec identity");
        }
        if (!exactFortranSourceDerivedTypeBinding(
                procedure->get_fortran_source_derived_type_symbol(),
                isSgClassType(source_result))) {
          throw std::runtime_error(
              "AST JSON Fortran procedure has a contradictory exact source "
              "derived-type binding");
        }
      }
    }
    SgVariableDeclaration *declaration =
        isSgVariableDeclaration(nodeById(nodes, record.id));
    if (declaration == nullptr) {
      continue;
    }
    switch (declaration->get_fortran_declaration_origin()) {
    case SgVariableDeclaration::e_fortran_source_declaration:
      if (declaration->get_variables().empty()) {
        throw std::runtime_error(
            "AST JSON Fortran source declaration has no entities");
      }
      for (SgInitializedName *name : declaration->get_variables()) {
        if (!validFortranTypeContract(name)) {
          SgType *source =
              name != nullptr ? name->get_fortran_source_type() : nullptr;
          SgType *semantic = name != nullptr ? name->get_type() : nullptr;
          const FortranTypeContractSurface source_surface =
              fortranTypeContractSurface(source);
          const FortranTypeContractSurface semantic_surface =
              fortranTypeContractSurface(semantic);
          fprintf(
              stderr,
              "REX_AST_JSON_INVARIANT[fortran-type-contract]: name=%p/'%s' "
              "source=%p/%s semantic=%p/%s source-base=%p/%s "
              "semantic-base=%p/%s source-rank=%zu semantic-rank=%zu "
              "source-pointer=%d semantic-pointer=%d source-syntax=%d "
              "type-spec=%d source-matches-semantic=%d\n",
              static_cast<void *>(name),
              name != nullptr ? name->get_name().str() : "<null>",
              static_cast<void *>(source),
              source != nullptr ? source->class_name().c_str() : "<null>",
              static_cast<void *>(semantic),
              semantic != nullptr ? semantic->class_name().c_str() : "<null>",
              static_cast<void *>(source_surface.base),
              source_surface.base != nullptr
                  ? source_surface.base->class_name().c_str()
                  : "<null>",
              static_cast<void *>(semantic_surface.base),
              semantic_surface.base != nullptr
                  ? semantic_surface.base->class_name().c_str()
                  : "<null>",
              source_surface.ordinary_rank, semantic_surface.ordinary_rank,
              source_surface.pointer ? 1 : 0, semantic_surface.pointer ? 1 : 0,
              source != nullptr && source->get_fortran_source_syntax() ? 1 : 0,
              name != nullptr ? static_cast<int>(name->get_fortran_type_spec())
                              : -1,
              source_surface.base != nullptr &&
                      semantic_surface.base != nullptr &&
                      SageInterface::fortranSourceTypeMatchesSemanticType(
                          source_surface.base, semantic_surface.base)
                  ? 1
                  : 0);
          throw std::runtime_error(
              "AST JSON Fortran source declaration has a missing or "
              "contradictory semantic/source type pair: " +
              (name != nullptr ? name->get_name().getString()
                               : std::string("<null>")));
        }
      }
      break;
    case SgVariableDeclaration::e_fortran_semantic_only_declaration:
      for (SgInitializedName *name : declaration->get_variables()) {
        if (name == nullptr || name->get_type() == nullptr ||
            name->get_fortran_source_type() != nullptr ||
            name->get_fortran_source_derived_type_symbol() != nullptr ||
            name->get_cray_pointer_pointee() != nullptr ||
            name->get_fortran_cray_pointer_pointee_shape() != nullptr ||
            name->get_fortran_separate_shape_declaration() != nullptr ||
            name->get_fortran_separate_pointer_declaration() != nullptr ||
            name->get_shapeDeferred()) {
          throw std::runtime_error(
              "AST JSON semantic-only Fortran declaration has malformed type "
              "ownership");
        }
      }
      break;
    case SgVariableDeclaration::e_fortran_pending_source_declaration:
      throw std::runtime_error(
          "AST JSON pending Fortran source declaration escaped the frontend");
    default:
      throw std::runtime_error(
          "AST JSON Fortran declaration has an invalid typed origin");
    }
  }
}

void validateRestoredCombinedClauseSourceOrders(const AstFileRecord &ast,
                                                const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgOmpClause *clause = isSgOmpClause(nodeById(nodes, record.id));
    if (clause == nullptr) {
      continue;
    }
    SgOmpClauseList *list = isSgOmpClauseList(clause->get_parent());
    SgOmpBodyStatement *owner =
        list != nullptr ? isSgOmpBodyStatement(list->get_parent()) : nullptr;
    SgOmpBodyStatement *combined_owner =
        owner != nullptr ? isSgOmpBodyStatement(owner->get_parent()) : nullptr;
    const bool combined_participant =
        owner != nullptr && (owner->get_source_form_is_combined() ||
                             (combined_owner != nullptr &&
                              combined_owner->get_source_form_is_combined() &&
                              combined_owner->get_body() == owner));
    if (!combined_participant &&
        clause->get_combined_source_order().has_value()) {
      throw std::runtime_error(
          "AST JSON non-combined OpenMP directive owns combined clause source "
          "order");
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    SgOmpBodyStatement *outer =
        isSgOmpBodyStatement(nodeById(nodes, record.id));
    if (outer == nullptr) {
      continue;
    }

    SgOmpBodyStatement *combined_owner =
        isSgOmpBodyStatement(outer->get_parent());
    const bool nested_combined_component =
        combined_owner != nullptr &&
        combined_owner->get_source_form_is_combined() &&
        combined_owner->get_body() == outer;
    if (!outer->get_source_form_is_combined()) {
      if (nested_combined_component) {
        continue;
      }
      continue;
    }

    SgOmpBodyStatement *inner = isSgOmpBodyStatement(outer->get_body());
    if (isSgOmpParallelStatement(outer) == nullptr || inner == nullptr ||
        inner->get_parent() != outer) {
      throw std::runtime_error(
          "AST JSON combined OpenMP directive has no exact nested semantic "
          "component");
    }

    std::vector<SgOmpClause *> clauses;
    auto append_clauses = [&](SgStatement *statement) {
      SgOmpClauseList *list = nullptr;
      if (SgOmpClauseBodyStatement *body =
              isSgOmpClauseBodyStatement(statement)) {
        list = body->get_clause_list();
      } else if (SgOmpClauseStatement *clause_statement =
                     isSgOmpClauseStatement(statement)) {
        list = clause_statement->get_clause_list();
      }
      if (list == nullptr || list->get_parent() != statement) {
        throw std::runtime_error(
            "AST JSON combined OpenMP component has no exact clause list");
      }
      for (SgOmpClause *clause : list->get_clauses()) {
        if (clause == nullptr || clause->get_parent() != list) {
          throw std::runtime_error(
              "AST JSON combined OpenMP component owns a malformed clause");
        }
        clauses.push_back(clause);
      }
    };
    append_clauses(outer);
    append_clauses(inner);

    std::vector<bool> seen(clauses.size(), false);
    for (SgOmpClause *clause : clauses) {
      const std::optional<std::size_t> &source_order =
          clause->get_combined_source_order();
      if (!source_order.has_value() || *source_order >= clauses.size() ||
          seen[*source_order]) {
        throw std::runtime_error(
            "AST JSON combined OpenMP clause source order is absent, "
            "duplicate, or out of range");
      }
      seen[*source_order] = true;
    }
  }
}

void validateRestoredDeclarationSourceOrders(const AstFileRecord &ast,
                                             const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgDeclarationStatement *declaration =
        isSgDeclarationStatement(nodeById(nodes, record.id));
    if (declaration == nullptr ||
        !declaration->get_translation_unit_source_order().has_value()) {
      continue;
    }

    const Sg_File_Info *position = nullptr;
    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(declaration)) {
      namespace_declaration->validate_source_fragments();
      if (!namespace_declaration->has_source_fragments() ||
          namespace_declaration->get_opening_source_fragment()
                  ->get_source_form() !=
              SgNamespaceSourceFragment::
                  e_namespace_source_fragment_source_spelled) {
        throw std::runtime_error(
            "AST JSON namespace declaration source order has no exact "
            "source-spelled opening fragment");
      }
      position = namespace_declaration->get_first_opening_source_fragment()
                     ->get_startOfConstruct();
    } else {
      position = declaration->get_startOfConstruct();
    }

    const unsigned int source_order =
        *declaration->get_translation_unit_source_order();
    if (position == nullptr ||
        position->get_source_sequence_number() != source_order) {
      throw std::runtime_error(
          "AST JSON declaration translation-unit source order disagrees "
          "with its exact source-position occurrence: " +
          declaration->class_name() + " node " + std::to_string(record.id) +
          " has order " + std::to_string(source_order) +
          " and position order " +
          (position != nullptr
               ? std::to_string(position->get_source_sequence_number())
               : std::string("<null>")));
    }
  }
}

SgSourceFile *reconstructSourceFile(const AstFileRecord &ast,
                                    SgSourceFile *old_file) {
  SgProject *project = owningProject(old_file);
  ROSE_ASSERT(project != nullptr);
  // Evict file-dependent canonical types before factories rebuild the copy.
  // Purging after reconstruction can remove the newly rebuilt live type under
  // the same mangled key, leaving the replacement AST outside its type table.
  purgeStaleCheckpointTypeCaches(old_file);
  DeserializationProjectGuard project_guard(project);
  PointerMemberTypeDeserializationIdentityGuard
      pointer_member_type_identity_guard;
  ArrayTypeDeserializationIdentityGuard array_type_identity_guard;
  TemplateTypeDeserializationIdentityGuard template_type_identity_guard;
  FunctionTypeDeserializationIdentityGuard function_type_identity_guard;
  ExternalFunctionDeserializationIdentityGuard external_function_identity_guard;
  ExternalClassDeserializationIdentityGuard external_class_identity_guard;
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

  publishTemplateParameterCanonicalTypes(ast, nodes);
  restoreAvailableSourcePositionsAndScopes(ast, nodes);
  restoreAvailableAuxiliaryNamespaceOwnership(ast, nodes);
  std::unordered_set<uint64_t> restored_declaration_identities;
  restoreDeclarationIdentityEdges(ast, nodes, project,
                                  restored_declaration_identities, false);
  validateDeclarationSpecializationFamilies(nodes);
  publishAnonymousFortranProgramUnitSymbolKeys(ast, nodes);
  // Template-definition and class-type construction can compute mangled
  // identities immediately. Publish the namespace declaration/definition
  // chain first so every enclosing namespace scope already has its exact
  // canonical declaration at either construction boundary.
  restoreNamespaceCanonicalIdentityEdges(ast, nodes);
  restoreTemplateInstantiationDefinitions(ast, nodes);
  restoreTemplateInstantiationDefinitionDependentContext(ast, nodes);
  // Class-type construction inputs can contain type-owned dependent
  // expressions whose exact nonreal/function symbols live in serialized
  // lexical tables. Delayed OpenMP declaration constructors likewise require
  // exact function symbols. Publish every lexical table before either
  // construction boundary is allowed to claim a symbol reference.
  restoreSerializedSymbolTables(ast, nodes);
  restoreClassTypeConstructionInputs(ast, nodes);
  rebuildConstructorOnlyNodes(ast, nodes);
  restoreDeclarationIdentityEdges(ast, nodes, project,
                                  restored_declaration_identities, true);
  validateDeclarationSpecializationFamilies(nodes);
  for (const NodeRecord &record : ast.nodes) {
    attachJsonNodeId(nodeById(nodes, record.id), record.id);
  }
  for (const NodeRecord &record : ast.nodes) {
    requireRestoredKind(nodeById(nodes, record.id), record);
  }
  // Delayed nodes did not exist during the early context pass. Publish their
  // own exact recorded parents now so a non-delayed expression owner can verify
  // a delayed child without repairing it during edge linking.
  for (const NodeRecord &record : ast.nodes) {
    if (!requiresDelayedRebuild(record)) {
      continue;
    }
    if (uint64_t target = singleEdgeTarget(record, "parent")) {
      SgNode *node = nodeById(nodes, record.id);
      SgNode *parent = nodeById(nodes, target);
      if (node->get_parent() != nullptr && node->get_parent() != parent) {
        throw std::runtime_error("AST JSON delayed node has a conflicting "
                                 "construction-time parent");
      }
      node->set_parent(parent);
    }
  }

  // Inline class/enum declarations carry a construction-time ownership role.
  // Restore that exact serialized role before the variable edge setter checks
  // the non-autonomous declaration invariant.
  restoreVariableInlineTypeOwnershipRoles(ast, nodes);
  // A field designator validates its referenced member type while claiming its
  // expression edge. Publish the exact serialized declaration, type, and symbol
  // chain before that construction boundary.
  restoreDesignatorFieldReferenceConstructionDependencies(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    linkNodeEdges(record, nodes);
  }
  linkDeclarationGroupEdges(ast, nodes);
  linkStatementAttributeEdges(ast, nodes);
  finalizeAuxiliaryDeclarationContainers(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    setNodeSourcePosition(node, record);
    setNodeFlags(node, record);
    attachPreprocessingInfo(node, record);
    attachAstAttributes(node, record);
    requireRestoredKind(nodeById(nodes, record.id), record);
  }
  restoreRecordedScopeEdges(ast, nodes);
  restoreRecordedParentEdges(ast, nodes);
  // Every published node now has its exact structure and source context.
  // Restore semantic types and symbols before any validation is allowed to
  // query them.
  applyTypesAndSymbols(ast, nodes);
  validateRestoredOmpOwnedClausePayloads(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgInitializedName *name = isSgInitializedName(node)) {
      const bool is_enum_constant =
          isSgEnumDeclaration(name->get_parent()) != nullptr;
      const bool has_enum_source_role =
          name->get_enum_constant_source_ownership() !=
          SgInitializedName::e_enum_constant_source_unclassified;
      if (is_enum_constant != has_enum_source_role) {
        throw std::runtime_error(
            "AST JSON SgInitializedName has inconsistent enum-constant "
            "source ownership");
      }
      const bool is_constructor_preinitializer =
          isSgCtorInitializerList(name->get_parent()) != nullptr;
      const bool has_constructor_preinitialization_role =
          name->get_preinitialization() !=
          SgInitializedName::e_unknown_preinitialization;
      if (is_constructor_preinitializer !=
          has_constructor_preinitialization_role) {
        throw std::runtime_error(
            "AST JSON SgInitializedName has inconsistent constructor "
            "preinitialization ownership");
      }
    }
    if (SgEnumDeclaration *declaration = isSgEnumDeclaration(node)) {
      declaration->validate_enumerator_source_ownership();
    }
    if (SgFunctionCallExp *call = isSgFunctionCallExp(node)) {
      const bool has_operator_surface =
          call->get_source_operator_surface() !=
          SgFunctionCallExp::e_no_operator_surface;
      const bool literal_surface =
          call->get_source_operator_surface() ==
          SgFunctionCallExp::e_user_defined_literal_surface;
      if (has_operator_surface &&
          (call->get_args() == nullptr ||
           call->get_args()->get_expressions().size() !=
               call->get_source_operator_operand_roles().size())) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp operator roles do not match semantic "
            "arguments");
      }
      SgExprListExp *lexical = call->get_source_user_defined_literal_operands();
      if (literal_surface) {
        if (lexical == nullptr || lexical->get_parent() != call ||
            lexical->get_expressions().empty() ||
            lexical->get_expressions().size() !=
                call->get_source_user_defined_literal_suffix_roles().size()) {
          throw std::runtime_error(
              "AST JSON SgFunctionCallExp has malformed UDL lexical ownership");
        }
        for (SgExpression *operand : lexical->get_expressions()) {
          if (operand == nullptr || operand->get_parent() != lexical ||
              isSgValueExp(operand) == nullptr ||
              isSgValueExp(operand)->get_literal_spelling_form() !=
                  SgValueExp::e_literal_source_spelled) {
            throw std::runtime_error(
                "AST JSON SgFunctionCallExp has malformed UDL lexical operand");
          }
        }
      } else if (lexical != nullptr) {
        throw std::runtime_error(
            "AST JSON non-UDL SgFunctionCallExp owns UDL lexical operands");
      }
    }
    if (SgDesignatedInitializer *init = isSgDesignatedInitializer(node)) {
      if (init->get_designatorList() == nullptr ||
          init->get_designatorList()->get_expressions().empty() ||
          init->get_memberInit() == nullptr) {
        throw std::runtime_error(
            "AST JSON reconstructed malformed SgDesignatedInitializer");
      }
      for (SgExpression *entry :
           init->get_designatorList()->get_expressions()) {
        SgDesignator *designator = isSgDesignator(entry);
        if (designator == nullptr) {
          throw std::runtime_error(
              "AST JSON SgDesignatedInitializer has a non-designator entry");
        }
        designator->validate_designator();
      }
    }
    if (SgStaticAssertionDeclaration *declaration =
            isSgStaticAssertionDeclaration(node)) {
      declaration->validate_static_assertion();
    }
    if (SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node)) {
      SgExpression *launch_bounds =
          declaration->get_cuda_launch_bounds_expression();
      if (launch_bounds != nullptr &&
          (launch_bounds->get_parent() != declaration ||
           launch_bounds->get_type() == nullptr ||
           isSgTypeUnknown(launch_bounds->get_type()) != nullptr ||
           isSgTypeDefault(launch_bounds->get_type()) != nullptr)) {
        throw std::runtime_error(
            "AST JSON reconstructed malformed CUDA launch bounds");
      }
      const uint64_t pattern_id =
          singleEdgeTarget(record, "template_instantiation_pattern");
      SgFunctionDeclaration *pattern =
          declaration->get_templateInstantiationPattern();
      if ((pattern_id == 0) != (pattern == nullptr) ||
          (pattern_id != 0 &&
           pattern != nodeByIdAs<SgFunctionDeclaration>(nodes, pattern_id))) {
        throw std::runtime_error(
            "AST JSON function instantiation-pattern edge changed during "
            "reconstruction");
      }
      if (declaration->get_template_instantiation_pattern_is_unpublished() &&
          pattern != nullptr) {
        throw std::runtime_error(
            "AST JSON reconstructed both exact and unpublished function "
            "instantiation patterns");
      }
      if (pattern != nullptr) {
        if (pattern == declaration ||
            isSgTemplateInstantiationFunctionDecl(declaration) != nullptr ||
            isSgTemplateInstantiationMemberFunctionDecl(declaration) !=
                nullptr ||
            pattern->get_templateInstantiationPattern() != nullptr) {
          throw std::runtime_error(
              "AST JSON reconstructed a malformed exact function "
              "instantiation-pattern edge");
        }
      }
    }
    if (SgClassDefinition *def = isSgClassDefinition(node)) {
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
    if (SgThisExp *this_expression = isSgThisExp(node)) {
      (void)this_expression->get_type();
    } else if (SgSizeOfOp *size_of = isSgSizeOfOp(node)) {
      (void)size_of->get_type();
    } else if (SgAlignOfOp *align_of = isSgAlignOfOp(node)) {
      (void)align_of->get_type();
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(nodeById(nodes, record.id));
    if (reference != nullptr &&
        reference->get_resolved_function_declaration() != nullptr) {
      SgNonrealDecl *spelling = reference->get_symbol() != nullptr
                                    ? reference->get_symbol()->get_declaration()
                                    : nullptr;
      if (spelling == nullptr ||
          spelling->get_templateDeclaration() !=
              reference->get_resolved_function_declaration()) {
        throw std::runtime_error(
            "AST JSON SgNonrealRefExp resolved function identity disagrees "
            "with its synthetic callable edge");
      }
      SageInterface::requireResolvedFunctionTemplateReference(
          reference, "AST JSON deserialization");
    } else if (reference != nullptr &&
               reference->get_resolved_variable_declaration() != nullptr) {
      SgNonrealDecl *spelling = reference->get_symbol() != nullptr
                                    ? reference->get_symbol()->get_declaration()
                                    : nullptr;
      if (spelling == nullptr ||
          spelling->get_templateDeclaration() !=
              reference->get_resolved_variable_declaration()) {
        throw std::runtime_error(
            "AST JSON SgNonrealRefExp resolved variable identity disagrees "
            "with its synthetic specialization edge");
      }
      SageInterface::requireResolvedVariableTemplateReference(
          reference, "AST JSON deserialization");
    }
  }
  validateFortranTypeContracts(ast, nodes);
  validateRestoredDeclarationSourceOrders(ast, nodes);
  validateRestoredCombinedClauseSourceOrders(ast, nodes);
  // Context-selector scores and semantic properties cannot be checked until
  // their expression types, symbols, and recorded ownership have all been
  // restored.  Validate them at the first fully semantic reconstruction
  // boundary rather than accepting a syntax-only or nonconstant payload.
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (SgOmpContextSelectorProperty *property =
            isSgOmpContextSelectorProperty(node)) {
      validateOmpContextSelectorProperty(
          property, isSgOmpContextSelector(property->get_parent()));
    } else if (SgOmpContextSelector *selector = isSgOmpContextSelector(node)) {
      validateOmpContextSelector(selector);
    } else if (SgOmpContextSelectorSet *set = isSgOmpContextSelectorSet(node)) {
      validateOmpContextSelectorSet(set);
    } else if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
      validateOmpContextSelectorSets(clause->get_context_selector_sets(),
                                     clause);
    } else if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
      validateOmpContextSelectorSets(clause->get_context_selector_sets(),
                                     clause);
    }
  }
  validateDeclarationGroups(ast, nodes);
  validateAccessLabels(ast, nodes);
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
  restoreTokenMappings(
      file, ast.node(ast.root_id).properties.at("token_mappings"), nodes);
  restoreFileIdMapFromMetadata(ast.metadata);
  restoreOpenMPProducerSemanticRecords(ast, nodes);
  return file;
}

} // namespace AstJson
} // namespace Rose
