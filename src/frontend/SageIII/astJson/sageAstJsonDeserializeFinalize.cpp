#include "AstNodes/Expression/ExpressionTypeClassification.h"
#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "ompAstConstruction.h"
#include "sageAstJsonPrivate.h"
#include "tokenStreamMapping.h"

#include <variant>

namespace Rose {
namespace AstJson {

namespace {

struct FortranImplicitRangeInput {
  char first;
  std::optional<char> last;
  SgFortranImplicitRange::type_spec_enum type_spec;
};

uint64_t declarationScopeOuterId(const AstFileRecord &ast,
                                 const NodeRecord &scope) {
  const auto parent = singleEdgeTarget(scope, "parent");
  if (parent != 0 && ast.node(parent).kind == "SgInitializedName") {
    const auto &owner = ast.node(parent);
    if (singleEdgeTarget(owner, "structured_binding_scope") != scope.id ||
        singleEdgeTarget(owner, "structured_binding_pattern") == 0 ||
        singleEdgeTarget(scope, "semantic_outer_scope") != 0 ||
        scope.properties.requiredBool("is_default_nonreal_scope"))
      throw std::runtime_error(
          "AST JSON structured-binding scope has invalid ownership");
    return singleEdgeTarget(owner, "scope");
  }
  return singleEdgeTarget(scope, "semantic_outer_scope");
}

FortranImplicitRangeInput
fortranImplicitRangeInput(const JsonValue &properties) {
  auto letter = [](const JsonValue &value) {
    const int64_t code = value.asInt();
    if (!((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')))
      throw std::runtime_error(
          "AST JSON implicit range endpoint is not a letter");
    return static_cast<char>(code);
  };
  const char first = letter(properties.at("first_letter"));
  const JsonValue &last_value = properties.at("last_letter");
  const std::optional<char> last =
      last_value.kind == JsonValue::Kind::Null
          ? std::nullopt
          : std::optional<char>(letter(last_value));
  auto letter_index = [](char value) {
    return value >= 'a' ? value - 'a' : value - 'A';
  };
  if (last && letter_index(first) > letter_index(*last))
    throw std::runtime_error(
        "AST JSON implicit range endpoints are descending");
  const auto spec = requiredEnum<SgFortranImplicitRange::type_spec_enum>(
      properties, "type_spec", "SgFortranImplicitRange",
      {SgFortranImplicitRange::e_intrinsic, SgFortranImplicitRange::e_type,
       SgFortranImplicitRange::e_class, SgFortranImplicitRange::e_type_star,
       SgFortranImplicitRange::e_class_star});
  if (!properties.at("type").requiredBool("present"))
    throw std::runtime_error("AST JSON implicit range has no exact type");
  const auto &symbol = properties.at("source_derived_type_symbol");
  if (symbol.kind != JsonValue::Kind::Null &&
      symbol.kind != JsonValue::Kind::Object)
    throw std::runtime_error("AST JSON implicit range symbol is not typed");
  return {first, last, spec};
}

SgImplicitStatement::implicit_spec_enum
implicitStatementSpec(const JsonValue &properties) {
  return requiredEnum<SgImplicitStatement::implicit_spec_enum>(
      properties, "implicit_spec", "SgImplicitStatement",
      {SgImplicitStatement::e_has_implicit_spec_list,
       SgImplicitStatement::e_none, SgImplicitStatement::e_none_external,
       SgImplicitStatement::e_none_type,
       SgImplicitStatement::e_none_external_and_type});
}

std::size_t requiredOpenMPProducerSize(const JsonValue &object,
                                       const std::string &field,
                                       const std::string &context,
                                       bool require_positive = false) {
  const int64_t raw = object.requiredInt(field);
  if (raw < 0 || (require_positive && raw == 0) ||
      static_cast<uint64_t>(raw) > std::numeric_limits<std::size_t>::max()) {
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
  struct RestoredRecord {
    SgSourceFile *source;
    SgPragmaDeclaration *pragma;
    OpenMPProducerSemanticRecords records;
  };
  std::vector<RestoredRecord> restored_records;
  std::unordered_set<SgSourceFile *> source_files;
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (auto *source = isSgSourceFile(node))
      source_files.insert(source);
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node);
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
      restored_records.push_back({nullptr, pragma, std::move(records)});
    }
  }
  for (auto &entry : restored_records) {
    for (SgNode *owner = entry.pragma; owner != nullptr;
         owner = owner->get_parent()) {
      if ((entry.source = isSgSourceFile(owner)) != nullptr)
        break;
    }
    if (entry.source == nullptr || source_files.count(entry.source) == 0 ||
        isSgScopeStatement(entry.pragma->get_parent()) == nullptr) {
      throw std::runtime_error(
          "AST JSON OpenMP producer record has no exact source-file owner");
    }
  }
  struct RegistrationRollback {
    const std::unordered_set<SgSourceFile *> &sources;
    bool committed = false;

    ~RegistrationRollback() {
      if (!committed)
        for (SgSourceFile *source : sources)
          OmpSupport::discardOpenMPProducerSemanticRecords(source);
    }
  } registration{source_files};
  for (auto &entry : restored_records)
    OmpSupport::registerOpenMPProducerSemanticRecords(
        entry.source, entry.pragma, std::move(entry.records));
  registration.committed = true;
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

void restoreResolvedIncludeDirectives(SgSourceFile *file, const JsonValue &json,
                                      const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Array ||
      !file->get_frontendResolvedIncludeDirectivesMap().empty())
    throw std::runtime_error(
        "AST JSON include edges require an array and a fresh map");
  std::map<std::string, std::set<PreprocessingInfo *>> includes;
  for (const JsonValue &edge : json.array) {
    const std::string path = edge.requiredString("path");
    const JsonValue &records = edge.at("records");
    if (path.empty() || records.kind != JsonValue::Kind::Array ||
        records.array.empty() || includes.count(path) != 0)
      throw std::runtime_error(
          "AST JSON include edge has missing or duplicate identities");
    auto &directives = includes[path];
    for (const JsonValue &record : records.array) {
      auto *owner = nodeByIdAs<SgLocatedNode>(
          nodes,
          requiredPositiveId(record.at("owner"), "include record owner"));
      owner->validateAttachedPreprocessingInfoOwnership();
      const auto *attached = owner->getAttachedPreprocessingInfo();
      const int64_t index = record.at("index").asInt();
      if (attached == nullptr || index < 0 ||
          static_cast<uint64_t>(index) >= attached->size())
        throw std::runtime_error(
            "AST JSON include record has no exact attachment");
      PreprocessingInfo *directive = (*attached)[index];
      const auto kind = directive->getTypeOfDirective();
      if ((kind != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
           kind != PreprocessingInfo::CpreprocessorIncludeNextDeclaration) ||
          !directives.insert(directive).second)
        throw std::runtime_error("AST JSON include record has invalid or "
                                 "duplicate directive identity");
    }
  }
  file->set_frontendResolvedIncludeDirectivesMap(includes);
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

    TokenStreamSequenceToNodeMapping::NodeAssociation association{
        owner,
        {},
        mapping_json.at("shared").asBool(),
        mapping_json.at("constructed_in_synthesized_attribute").asBool()};

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
      association.nodes.push_back(associated);
    }
    if (associated_nodes.find(owner) == associated_nodes.end()) {
      throw std::runtime_error(
          "AST JSON token mapping owner is absent from node_vector");
    }
    if (association.shared && association.nodes.size() < 2) {
      throw std::runtime_error(
          "AST JSON shared token mapping has fewer than two associated "
          "nodes");
    }
    RestoredMapping restored;
    restored.value.reset(TokenStreamSequenceToNodeMapping::createPublished(
        association, leading, core, trailing, else_interval, token_count));
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

struct TemplateArgumentFields {
  SgTemplateArgument::template_argument_enum kind;
  bool explicitly_specified;
  bool array_bound_unknown;
  bool elaboration;
  bool qualification_present;
  bool global_qualification;
  SgStringList qualification_tokens;
  uint64_t template_id;
  bool pack_element;
};

static TemplateArgumentFields
templateArgumentFieldsFromRecord(const NodeRecord &record) {
  const auto &properties = record.properties;
  const bool elaboration = properties.requiredBool("type_elaboration_required");
  return {requiredTemplateArgumentType(properties, record.kind),
          properties.requiredBool("explicitly_specified"),
          properties.requiredBool("is_array_bound_unknown_type"),
          elaboration,
          properties.requiredBool("source_type_qualification_present"),
          properties.requiredBool("source_type_global_qualification"),
          stringListFromJson(properties.at("source_type_qualification_tokens"),
                             "source_type_qualification_tokens"),
          properties.requiredNonnegativeNodeId("template_declaration"),
          properties.requiredBool("is_pack_element")};
}

struct DeleteUnpublishedTemplateArgument {
  void operator()(SgTemplateArgument *argument) const {
    if (auto *expression = argument->get_expression()) {
      ROSE_ASSERT(expression->get_parent() == argument);
      expression->set_parent(nullptr);
    }
    if (auto *source = argument->get_sourceSpelledType();
        source != nullptr && source->get_parent() == argument)
      source->set_parent(nullptr);
    delete argument;
  }
};

static std::unique_ptr<SgTemplateArgument, DeleteUnpublishedTemplateArgument>
constructTemplateArgument(const NodeRecord &record, const NodeMap &nodes,
                          const TemplateArgumentFields &fields,
                          const NodeConstructionFlags &flags,
                          const SourcePositionFields &position_fields) {
  const JsonValue &properties = record.properties;
  auto positions = constructSourcePositions(position_fields);
  SgType *type = nullableTypeFromJson(properties.at("type"), nodes);
  SgType *source_type =
      nullableTypeFromJson(properties.at("source_spelled_type"), nodes);
  SgExpression *expression =
      expressionFromRef(properties.at("expression"), nodes);
  SgDeclarationStatement *template_declaration =
      fields.template_id != 0
          ? nodeByIdAs<SgDeclarationStatement>(nodes, fields.template_id)
          : nullptr;
  if (expression != nullptr && expression->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON template argument expression has a foreign owner");
  if (source_type != nullptr && source_type->has_source_prototype() &&
      source_type->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON template argument source prototype has a foreign owner");
  if (fields.kind == SgTemplateArgument::nontype_argument &&
      (expression == nullptr || SageInterface::containsUnknownType(type) ||
       SageInterface::containsUnknownType(expression->get_type()) ||
       !SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
           expression->get_type(), type)))
    throw std::runtime_error(
        "AST JSON non-type template argument has incompatible value type");
  std::unique_ptr<SgTemplateArgument, DeleteUnpublishedTemplateArgument>
      argument;
  if (fields.kind == SgTemplateArgument::type_argument) {
    argument.reset(
        new SgTemplateArgument(type, fields.explicitly_specified, source_type));
    argument->set_isArrayBoundUnknownType(fields.array_bound_unknown);
  } else {
    argument.reset(new SgTemplateArgument(
        fields.kind, fields.array_bound_unknown, type, expression,
        template_declaration, fields.explicitly_specified));
  }
  installSourcePositions(argument.get(), std::move(positions), record.kind);
  argument->set_type_elaboration_required(fields.elaboration);
  argument->set_source_type_qualification_present(fields.qualification_present);
  argument->set_source_type_global_qualification(fields.global_qualification);
  argument->get_source_type_qualification_tokens() =
      fields.qualification_tokens;
  argument->set_is_pack_element(fields.pack_element);
  flags.initialize(argument.get());
  if (uint64_t parent = singleEdgeTarget(record, "parent")) {
    auto owner = nodes.find(parent);
    if (owner != nodes.end())
      argument->set_parent(owner->second);
  }
  return argument;
}

static void validateNamespaceCanonicalDefinitionChains(const AstFileRecord &ast,
                                                       const NodeMap &nodes) {
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

static bool isClassDeclarationRecordKind(const std::string &kind) {
  return kind == "SgClassDeclaration" || kind == "SgTemplateClassDeclaration" ||
         kind == "SgTemplateInstantiationDecl" ||
         kind == "SgDerivedTypeStatement" || kind == "SgModuleStatement";
}

static bool isFunctionDeclarationRecordKind(const std::string &kind) {
  return kind == "SgFunctionDeclaration" ||
         kind == "SgTemplateFunctionDeclaration" ||
         kind == "SgTemplateInstantiationFunctionDecl" ||
         kind == "SgMemberFunctionDeclaration" ||
         kind == "SgTemplateMemberFunctionDeclaration" ||
         kind == "SgTemplateInstantiationMemberFunctionDecl" ||
         kind == "SgProgramHeaderStatement" ||
         kind == "SgProcedureHeaderStatement" || kind == "SgEntryStatement";
}

static bool isMemberFunctionDeclarationRecordKind(const std::string &kind) {
  return kind == "SgMemberFunctionDeclaration" ||
         kind == "SgTemplateMemberFunctionDeclaration" ||
         kind == "SgTemplateInstantiationMemberFunctionDecl";
}

static std::unordered_map<uint64_t, uint64_t>
validateCtorInitializerListOwners(const AstFileRecord &ast) {
  std::unordered_map<uint64_t, uint64_t> owners;
  for (const NodeRecord &record : ast.nodes) {
    if (!isMemberFunctionDeclarationRecordKind(record.kind))
      continue;
    const NodeRecord &class_owner = ast.node(
        requiredSingleEdgeTarget(record, "associatedClassDeclaration"));
    if (class_owner.kind != "SgClassDeclaration" &&
        class_owner.kind != "SgTemplateClassDeclaration" &&
        class_owner.kind != "SgTemplateInstantiationDecl" &&
        class_owner.kind != "SgNonrealDecl")
      throw std::runtime_error("AST JSON member function has no typed class "
                               "owner");
    const uint64_t list_id =
        requiredSingleEdgeTarget(record, "CtorInitializerList");
    const NodeRecord &list = ast.node(list_id);
    if (list.kind != "SgCtorInitializerList" ||
        singleEdgeTarget(list, "parent") != record.id ||
        !owners.emplace(list_id, record.id).second)
      throw std::runtime_error(
          "AST JSON constructor initializer list has no unique typed owner");
    std::unordered_set<uint64_t> children;
    for (const EdgeRecord &edge : edgesFor(list, "ctors")) {
      const NodeRecord &child = ast.node(edge.target);
      if (child.kind != "SgInitializedName" ||
          requiredSingleEdgeTarget(child, "parent") != list_id ||
          !children.insert(edge.target).second)
        throw std::runtime_error(
            "AST JSON constructor initializer list has a duplicate or "
            "misowned child");
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgCtorInitializerList" && owners.count(record.id) == 0)
      throw std::runtime_error(
          "AST JSON constructor initializer list has no member function");
  }
  return owners;
}

struct StatementAttributeFields {
  SgStatementAttribute::statement_attribute_kind_enum kind;
  SgStatementAttribute::statement_attribute_spelling_enum spelling;
  unsigned long integral;
  SgStatementAttribute::loop_hint_option_enum option;
  SgStatementAttribute::loop_hint_state_enum state;
};

StatementAttributeFields
statementAttributeFieldsFromJson(const NodeRecord &record) {
  const auto &p = record.properties;
  const int64_t raw_integral =
      p.requiredInt("statement_attribute_integral_argument");
  if (raw_integral < 0 || static_cast<uint64_t>(raw_integral) >
                              std::numeric_limits<unsigned long>::max()) {
    throw std::runtime_error(
        "AST JSON SgStatementAttribute has invalid typed fields");
  }
  using Attribute = SgStatementAttribute;
  StatementAttributeFields fields{
      static_cast<Attribute::statement_attribute_kind_enum>(
          p.requiredIntInRange("statement_attribute_kind",
                               Attribute::e_statement_attribute_fallthrough,
                               Attribute::e_last_statement_attribute_kind - 1)),
      static_cast<Attribute::statement_attribute_spelling_enum>(
          p.requiredIntInRange(
              "statement_attribute_spelling",
              Attribute::e_statement_attribute_spelling_cxx11_unscoped,
              Attribute::e_last_statement_attribute_spelling - 1)),
      static_cast<unsigned long>(raw_integral),
      static_cast<Attribute::loop_hint_option_enum>(
          p.requiredIntInRange("statement_attribute_loop_hint_option",
                               Attribute::e_loop_hint_option_none,
                               Attribute::e_last_loop_hint_option - 1)),
      static_cast<Attribute::loop_hint_state_enum>(
          p.requiredIntInRange("statement_attribute_loop_hint_state",
                               Attribute::e_loop_hint_state_none,
                               Attribute::e_last_loop_hint_state - 1))};
  if (!SgStatementAttribute::fields_are_valid(
          fields.kind, fields.spelling,
          singleEdgeTarget(record, "expression_argument") != 0, fields.integral,
          fields.option, fields.state))
    throw std::runtime_error(
        "AST JSON statement attribute has conflicting typed fields");
  return fields;
}

struct DesignatorInput {
  SgDesignator::designator_kind_enum kind;
  uint64_t first, second, list;
};

struct ProcessControlFields {
  uint64_t code;
  uint64_t quiet;
  SgProcessControlStatement::control_enum kind;
};

struct OmpApplyTransformationFields {
  SgOmpClause::omp_apply_transform_kind_enum kind;
  SgOmpClause::omp_clause_separator_enum separator;
  std::string name;
};

struct OmpInductionItemFields {
  SgOmpClause::omp_induction_item_kind_enum kind;
  std::string label;
};

struct OmpScheduleFields {
  SgOmpClause::omp_schedule_modifier_enum modifier, modifier1;
  SgOmpClause::omp_schedule_kind_enum kind;
};

struct OmpMapFields {
  SgOmpClause::omp_map_operator_enum operation;
  std::array<SgOmpClause::omp_map_modifier_enum, 3> modifiers;
  bool source_variables_override;
};

struct OmpToFields {
  SgOmpClause::omp_to_kind_enum kind;
  bool source_variables_override, declare_target_extended_list;
};

struct OmpFromFields {
  SgOmpClause::omp_from_kind_enum kind;
  bool source_variables_override;
};

struct OmpDependFields {
  SgOmpClause::omp_depend_modifier_enum modifier;
  SgOmpClause::omp_dependence_type_enum type;
  bool source_variables_override;
};

struct OmpAffinityFields {
  SgOmpClause::omp_affinity_modifier_enum modifier;
  bool source_variables_override;
};

template <class Identifier> struct OmpReductionFields {
  Identifier identifier;
  bool source_variables_override;
};

struct OmpAllocateFields {
  SgOmpClause::omp_allocate_modifier_enum modifier;
  bool modifier_syntax, source_variables_override;
};

struct OmpLinearFields {
  SgOmpClause::omp_linear_modifier_enum modifier;
  bool source_variables_override;
};

struct OmpVariablesFields {
  VariantT kind;
  bool source_variables_override;
};

struct RecordConstructionFields {
  NodeConstructionFlags flags;
  std::optional<ExpressionConstructionFields> expression;
  std::optional<StatementFields> statement;
  std::optional<ReturnStatementFields> return_statement;
  std::vector<uint64_t> destructor_base_declarations;
  std::vector<uint64_t> destructor_members;
  std::optional<DeclarationFields> declaration;
  std::optional<OmpClauseFields> clause;
  std::optional<ProcessControlFields> process_control;
  std::optional<StatementAttributeFields> statement_attribute;
  std::optional<DesignatorInput> designator;
  std::optional<SgOmpClause::omp_init_modifier_kind_enum> init_modifier;
  std::optional<OmpInductionItemFields> induction_item;
  std::optional<SgOmpClause::omp_map_dist_data_enum> dist_data_policy;
  std::optional<OmpApplyTransformationFields> apply_transformation;
  std::optional<std::string> apply_label;
  std::optional<SgOmpClause::omp_adjust_args_modifier_enum> adjust_args;
  std::optional<OmpScheduleFields> schedule;
  std::optional<SgOmpClause::omp_dist_schedule_kind_enum> dist_schedule;
  std::optional<OmpMapFields> map;
  std::optional<OmpToFields> to;
  std::optional<OmpFromFields> from;
  std::optional<OmpDependFields> depend;
  std::optional<OmpAffinityFields> affinity;
  std::optional<OmpReductionFields<SgOmpClause::omp_reduction_identifier_enum>>
      reduction;
  std::optional<SgOmpClause::omp_reduction_modifier_enum> reduction_modifier;
  std::optional<
      OmpReductionFields<SgOmpClause::omp_in_reduction_identifier_enum>>
      in_reduction;
  std::optional<
      OmpReductionFields<SgOmpClause::omp_task_reduction_identifier_enum>>
      task_reduction;
  std::optional<OmpAllocateFields> allocate;
  std::optional<SgOmpClause::omp_allocator_modifier_enum> allocator;
  std::optional<OmpLinearFields> linear;
  std::optional<bool> aligned_source_variables_override;
  std::optional<OmpVariablesFields> variables;
  std::optional<bool> firstprivate_saved;
  std::optional<SgOmpClause::omp_lastprivate_modifier_enum>
      lastprivate_modifier;
  std::optional<OmpContextPropertyFields> context_property;
  std::optional<OmpContextSelectorFields> context_selector;
  std::optional<SgOmpClause::omp_context_selector_set_kind_enum>
      context_set_kind;
  PreparedAstAttributes attributes;
  std::vector<PreprocessingInfoFields> preprocessing;
  std::optional<SourcePositionFields> positions;
  std::optional<TemplateArgumentFields> template_argument;
};
using RecordConstructionFieldMap =
    std::unordered_map<uint64_t, RecordConstructionFields>;

static void prepareRecordPublicationFields(const NodeRecord &record,
                                           RecordConstructionFields &fields) {
  static const VariantVector expressions(V_SgExpression);
  static const VariantVector statements(V_SgStatement);
  static const VariantVector declarations(V_SgDeclarationStatement);
  static const VariantVector clauses(V_SgOmpClause);
  if (kindInVariants(record.kind, expressions)) {
    fields.expression = expressionConstructionFieldsFromJson(record.properties);
    fields.expression->qualification =
        expressionQualificationFieldsFromJson(record.kind, record.properties);
    fields.expression->source =
        expressionSourceFieldsFromJson(record.kind, record.properties);
  }
  if (record.kind == "SgReturnStmt" && !fields.return_statement)
    fields.return_statement = returnStatementFieldsFromJson(record.properties);
  const auto kind =
      std::find_if(statements.begin(), statements.end(), [&](VariantT variant) {
        return record.kind == roseGlobalVariantNameList[variant];
      });
  if (kind != statements.end()) {
    if (*kind == V_SgProcessControlStatement) {
      const uint64_t code = singleEdgeTarget(record, "code");
      const uint64_t quiet = singleEdgeTarget(record, "quiet");
      if (code != 0 && code == quiet)
        throw std::runtime_error(
            "AST JSON process control operands claim the same child");
      fields.process_control = ProcessControlFields{
          code, quiet,
          requiredEnum<SgProcessControlStatement::control_enum>(
              record.properties, "control_kind", "SgProcessControlStatement",
              {SgProcessControlStatement::e_abort,
               SgProcessControlStatement::e_stop,
               SgProcessControlStatement::e_error_stop,
               SgProcessControlStatement::e_exit,
               SgProcessControlStatement::e_fail_image,
               SgProcessControlStatement::e_pause})};
    }
    fields.statement = statementFieldsFromJson(record, *kind);
    if (kindInVariants(record.kind, declarations))
      fields.declaration =
          declarationFieldsFromJson(record.properties, *kind, record.kind);
  }
  if (kindInVariants(record.kind, clauses))
    fields.clause = ompClauseFieldsFromJson(record);
  if (record.kind == "SgOmpContextSelectorProperty")
    fields.context_property = contextPropertyFieldsFromRecord(record);
  if (record.kind == "SgOmpContextSelector")
    fields.context_selector = contextSelectorFieldsFromJson(record.properties);
  if (record.kind == "SgOmpContextSelectorSet")
    fields.context_set_kind = contextSelectorSetKindFromJson(record.properties);
  if (const auto variant = ordinaryOmpVariablesClauseVariant(record.kind)) {
    const bool source_override =
        record.properties.requiredBool("has_source_variables_override");
    if (source_override != (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error("AST JSON OpenMP source-variable override "
                               "discriminator and edge disagree");
    fields.variables = OmpVariablesFields{*variant, source_override};
    if (*variant == V_SgOmpFirstprivateClause)
      fields.firstprivate_saved = record.properties.requiredBool("saved");
    if (*variant == V_SgOmpLastprivateClause)
      fields.lastprivate_modifier =
          requiredEnum<SgOmpClause::omp_lastprivate_modifier_enum>(
              record.properties, "modifier", record.kind,
              {SgOmpClause::e_omp_lastprivate_modifier_unspecified,
               SgOmpClause::e_omp_lastprivate_conditional});
  }
  if (record.kind == "SgOmpAdjustArgsClause")
    fields.adjust_args =
        requiredEnum<SgOmpClause::omp_adjust_args_modifier_enum>(
            record.properties, "modifier", record.kind,
            {SgOmpClause::e_omp_adjust_args_modifier_need_device_addr,
             SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr,
             SgOmpClause::e_omp_adjust_args_modifier_nothing});
  if (record.kind == "SgStatementAttribute")
    fields.statement_attribute = statementAttributeFieldsFromJson(record);
  if (record.kind == "SgOmpInitModifier")
    fields.init_modifier = initModifierKindFromJson(record.properties);
  if (record.kind == "SgOmpMapClause") {
    auto modifier = [&](const char *field) {
      return requiredEnum<SgOmpClause::omp_map_modifier_enum>(
          record.properties, field, record.kind,
          {SgOmpClause::e_omp_map_modifier_unspecified,
           SgOmpClause::e_omp_map_modifier_always,
           SgOmpClause::e_omp_map_modifier_close,
           SgOmpClause::e_omp_map_modifier_present,
           SgOmpClause::e_omp_map_modifier_self,
           SgOmpClause::e_omp_map_modifier_mapper,
           SgOmpClause::e_omp_map_modifier_iterator});
    };
    fields.map = OmpMapFields{
        requiredEnum<SgOmpClause::omp_map_operator_enum>(
            record.properties, "operation", record.kind,
            {SgOmpClause::e_omp_map_unknown, SgOmpClause::e_omp_map_alloc,
             SgOmpClause::e_omp_map_to, SgOmpClause::e_omp_map_from,
             SgOmpClause::e_omp_map_tofrom, SgOmpClause::e_omp_map_storage,
             SgOmpClause::e_omp_map_release, SgOmpClause::e_omp_map_delete,
             SgOmpClause::e_omp_map_present, SgOmpClause::e_omp_map_self}),
        {modifier("modifier1"), modifier("modifier2"), modifier("modifier3")},
        record.properties.requiredBool("has_source_variables_override")};
    Rose::OpenMP::Detail::MapModifierFields modifiers;
    std::string detail;
    if (!Rose::OpenMP::Detail::validateMapModifiers(fields.map->modifiers,
                                                    modifiers, &detail))
      throw std::runtime_error("AST JSON OpenMP " + detail);
    if (modifiers.mapper !=
            (singleEdgeTarget(record, "mapper_identifier") != 0) ||
        modifiers.iterator !=
            !edgesFor(record, "iterator_definitions").empty() ||
        fields.map->source_variables_override !=
            (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error(
          "AST JSON OpenMP map modifiers and structural payloads disagree");
  }
  if (record.kind == "SgOmpToClause" || record.kind == "SgOmpFromClause") {
    bool mapper, iterator;
    const bool source_override =
        record.properties.requiredBool("has_source_variables_override");
    if (record.kind == "SgOmpToClause") {
      fields.to = OmpToFields{
          requiredEnum<SgOmpClause::omp_to_kind_enum>(
              record.properties, "kind", record.kind,
              {SgOmpClause::e_omp_to_kind_unknown,
               SgOmpClause::e_omp_to_kind_iterator,
               SgOmpClause::e_omp_to_kind_mapper,
               SgOmpClause::e_omp_to_kind_present}),
          source_override,
          record.properties.requiredBool("declare_target_extended_list")};
      mapper = fields.to->kind == SgOmpClause::e_omp_to_kind_mapper;
      iterator = fields.to->kind == SgOmpClause::e_omp_to_kind_iterator;
      if (fields.to->declare_target_extended_list &&
          fields.to->kind != SgOmpClause::e_omp_to_kind_unknown)
        throw std::runtime_error(
            "AST JSON OpenMP to kind and structural payloads disagree");
    } else {
      fields.from = OmpFromFields{requiredEnum<SgOmpClause::omp_from_kind_enum>(
                                      record.properties, "kind", record.kind,
                                      {SgOmpClause::e_omp_from_kind_unknown,
                                       SgOmpClause::e_omp_from_kind_iterator,
                                       SgOmpClause::e_omp_from_kind_mapper,
                                       SgOmpClause::e_omp_from_kind_present}),
                                  source_override};
      mapper = fields.from->kind == SgOmpClause::e_omp_from_kind_mapper;
      iterator = fields.from->kind == SgOmpClause::e_omp_from_kind_iterator;
    }
    if (mapper != (singleEdgeTarget(record, "mapper_identifier") != 0) ||
        iterator != !edgesFor(record, "iterator_definitions").empty()) {
      const char *kind = record.kind == "SgOmpToClause" ? "to" : "from";
      throw std::runtime_error(std::string("AST JSON OpenMP ") + kind +
                               " kind and structural payloads disagree");
    }
    if (source_override != (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error("AST JSON OpenMP source-variable override "
                               "discriminator and edge disagree");
  }
  if (record.kind == "SgOmpDependClause" ||
      record.kind == "SgOmpAffinityClause") {
    const bool source_override =
        record.properties.requiredBool("has_source_variables_override");
    bool iterator;
    if (record.kind == "SgOmpDependClause") {
      fields.depend = OmpDependFields{
          requiredEnum<SgOmpClause::omp_depend_modifier_enum>(
              record.properties, "depend_modifier", record.kind,
              {SgOmpClause::e_omp_depend_modifier_unspecified,
               SgOmpClause::e_omp_depend_modifier_iterator}),
          requiredEnum<SgOmpClause::omp_dependence_type_enum>(
              record.properties, "dependence_type", record.kind,
              {SgOmpClause::e_omp_depend_in, SgOmpClause::e_omp_depend_out,
               SgOmpClause::e_omp_depend_inout,
               SgOmpClause::e_omp_depend_inoutset,
               SgOmpClause::e_omp_depend_mutexinoutset,
               SgOmpClause::e_omp_depend_depobj,
               SgOmpClause::e_omp_depend_source,
               SgOmpClause::e_omp_depend_sink}),
          source_override};
      iterator = fields.depend->modifier ==
                 SgOmpClause::e_omp_depend_modifier_iterator;
      if ((fields.depend->type == SgOmpClause::e_omp_depend_sink) !=
          (singleEdgeTarget(record, "sink_vectors") != 0))
        throw std::runtime_error(
            "AST JSON OpenMP depend kind and owned sink-vector edge disagree");
    } else {
      fields.affinity = OmpAffinityFields{
          requiredEnum<SgOmpClause::omp_affinity_modifier_enum>(
              record.properties, "affinity_modifier", record.kind,
              {SgOmpClause::e_omp_affinity_modifier_unspecified,
               SgOmpClause::e_omp_affinity_modifier_iterator}),
          source_override};
      iterator = fields.affinity->modifier ==
                 SgOmpClause::e_omp_affinity_modifier_iterator;
    }
    if (iterator != !edgesFor(record, "iterator_definitions").empty()) {
      const char *kind =
          record.kind == "SgOmpDependClause" ? "depend" : "affinity";
      throw std::runtime_error(
          std::string("AST JSON OpenMP ") + kind +
          " modifier and owned iterator definitions disagree");
    }
    if (source_override != (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error("AST JSON OpenMP source-variable override "
                               "discriminator and edge disagree");
    if (singleEdgeTarget(record, "mapper_identifier") != 0)
      throw std::runtime_error(
          "AST JSON iterator clause has an unexpected mapper");
  }
  if (record.kind == "SgOmpReductionClause" ||
      record.kind == "SgOmpInReductionClause" ||
      record.kind == "SgOmpTaskReductionClause") {
    const bool source_override =
        record.properties.requiredBool("has_source_variables_override");
    bool named;
    const char *context;
    if (record.kind == "SgOmpReductionClause") {
      fields.reduction_modifier =
          requiredEnum<SgOmpClause::omp_reduction_modifier_enum>(
              record.properties, "modifier", record.kind,
              {SgOmpClause::e_omp_reduction_modifier_unknown,
               SgOmpClause::e_omp_reduction_inscan,
               SgOmpClause::e_omp_reduction_task,
               SgOmpClause::e_omp_reduction_default,
               SgOmpClause::e_omp_reduction_original_private});
      fields.reduction =
          OmpReductionFields<SgOmpClause::omp_reduction_identifier_enum>{
              requiredEnum<SgOmpClause::omp_reduction_identifier_enum>(
                  record.properties, "identifier", record.kind,
                  {SgOmpClause::e_omp_reduction_plus,
                   SgOmpClause::e_omp_reduction_mul,
                   SgOmpClause::e_omp_reduction_minus,
                   SgOmpClause::e_omp_reduction_bitand,
                   SgOmpClause::e_omp_reduction_bitor,
                   SgOmpClause::e_omp_reduction_bitxor,
                   SgOmpClause::e_omp_reduction_logand,
                   SgOmpClause::e_omp_reduction_logor,
                   SgOmpClause::e_omp_reduction_and,
                   SgOmpClause::e_omp_reduction_or,
                   SgOmpClause::e_omp_reduction_eqv,
                   SgOmpClause::e_omp_reduction_neqv,
                   SgOmpClause::e_omp_reduction_max,
                   SgOmpClause::e_omp_reduction_min,
                   SgOmpClause::e_omp_reduction_iand,
                   SgOmpClause::e_omp_reduction_ior,
                   SgOmpClause::e_omp_reduction_ieor,
                   SgOmpClause::e_omp_reduction_user_defined_identifier}),
              source_override};
      named = fields.reduction->identifier ==
              SgOmpClause::e_omp_reduction_user_defined_identifier;
      context = "reduction";
    } else if (record.kind == "SgOmpInReductionClause") {
      fields.in_reduction =
          OmpReductionFields<SgOmpClause::omp_in_reduction_identifier_enum>{
              requiredEnum<SgOmpClause::omp_in_reduction_identifier_enum>(
                  record.properties, "identifier", record.kind,
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
              source_override};
      named = fields.in_reduction->identifier ==
              SgOmpClause::e_omp_in_reduction_user_defined_identifier;
      context = "in_reduction";
    } else {
      fields.task_reduction =
          OmpReductionFields<SgOmpClause::omp_task_reduction_identifier_enum>{
              requiredEnum<SgOmpClause::omp_task_reduction_identifier_enum>(
                  record.properties, "identifier", record.kind,
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
              source_override};
      named = fields.task_reduction->identifier ==
              SgOmpClause::e_omp_task_reduction_user_defined_identifier;
      context = "task_reduction";
    }
    if (named != (singleEdgeTarget(record, "user_defined_identifier") != 0))
      throw std::runtime_error(std::string("AST JSON OpenMP ") + context +
                               " identifier kind and owned payload disagree");
    if (source_override != (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error("AST JSON OpenMP source-variable override "
                               "discriminator and edge disagree");
  }
  if (record.kind == "SgOmpAllocateClause" ||
      record.kind == "SgOmpAllocatorClause") {
    bool user_defined;
    if (record.kind == "SgOmpAllocateClause") {
      fields.allocate = OmpAllocateFields{
          requiredEnum<SgOmpClause::omp_allocate_modifier_enum>(
              record.properties, "modifier", record.kind,
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
          record.properties.requiredBool("uses_allocator_modifier_syntax"),
          record.properties.requiredBool("has_source_variables_override")};
      user_defined = fields.allocate->modifier ==
                     SgOmpClause::e_omp_allocate_user_defined_modifier;
      if (fields.allocate->modifier_syntax && !user_defined)
        throw std::runtime_error(
            "AST JSON allocate modifier syntax has no typed allocator");
      if (fields.allocate->source_variables_override !=
          (singleEdgeTarget(record, "source_variables") != 0))
        throw std::runtime_error("AST JSON OpenMP source-variable override "
                                 "discriminator and edge disagree");
    } else {
      fields.allocator = requiredEnum<SgOmpClause::omp_allocator_modifier_enum>(
          record.properties, "modifier", record.kind,
          {SgOmpClause::e_omp_allocator_default_mem_alloc,
           SgOmpClause::e_omp_allocator_large_cap_mem_alloc,
           SgOmpClause::e_omp_allocator_const_mem_alloc,
           SgOmpClause::e_omp_allocator_high_bw_mem_alloc,
           SgOmpClause::e_omp_allocator_low_lat_mem_alloc,
           SgOmpClause::e_omp_allocator_cgroup_mem_alloc,
           SgOmpClause::e_omp_allocator_pteam_mem_alloc,
           SgOmpClause::e_omp_allocator_thread_mem_alloc,
           SgOmpClause::e_omp_allocator_user_defined_modifier});
      user_defined = *fields.allocator ==
                     SgOmpClause::e_omp_allocator_user_defined_modifier;
    }
    if (user_defined !=
        (singleEdgeTarget(record, "user_defined_modifier") != 0))
      throw std::runtime_error(
          "AST JSON allocator kind and typed payload disagree");
    auto validate_reference = [&](const char *role) {
      const int64_t id = record.properties.at(role).requiredInt("node");
      if (id < 0 || static_cast<uint64_t>(id) != singleEdgeTarget(record, role))
        throw std::runtime_error(
            "AST JSON allocator operand property conflicts with its edge");
    };
    validate_reference("user_defined_modifier");
    if (record.kind == "SgOmpAllocateClause")
      validate_reference("alignment");
  }
  if (record.kind == "SgOmpLinearClause" ||
      record.kind == "SgOmpAlignedClause") {
    const bool source_override =
        record.properties.requiredBool("has_source_variables_override");
    if (source_override != (singleEdgeTarget(record, "source_variables") != 0))
      throw std::runtime_error("AST JSON OpenMP source-variable override "
                               "discriminator and edge disagree");
    if (record.kind == "SgOmpLinearClause")
      fields.linear =
          OmpLinearFields{requiredEnum<SgOmpClause::omp_linear_modifier_enum>(
                              record.properties, "modifier", record.kind,
                              {SgOmpClause::e_omp_linear_modifier_unspecified,
                               SgOmpClause::e_omp_linear_modifier_ref,
                               SgOmpClause::e_omp_linear_modifier_val,
                               SgOmpClause::e_omp_linear_modifier_uval}),
                          source_override};
    else
      fields.aligned_source_variables_override = source_override;
  }
  if (record.kind == "SgOmpScheduleClause") {
    auto modifier = [&](const char *field) {
      return requiredEnum<SgOmpClause::omp_schedule_modifier_enum>(
          record.properties, field, record.kind,
          {SgOmpClause::e_omp_schedule_modifier_unspecified,
           SgOmpClause::e_omp_schedule_modifier_monotonic,
           SgOmpClause::e_omp_schedule_modifier_nonmonotonic,
           SgOmpClause::e_omp_schedule_modifier_simd});
    };
    fields.schedule =
        OmpScheduleFields{modifier("modifier"), modifier("modifier1"),
                          requiredEnum<SgOmpClause::omp_schedule_kind_enum>(
                              record.properties, "kind", record.kind,
                              {SgOmpClause::e_omp_schedule_kind_static,
                               SgOmpClause::e_omp_schedule_kind_dynamic,
                               SgOmpClause::e_omp_schedule_kind_guided,
                               SgOmpClause::e_omp_schedule_kind_auto,
                               SgOmpClause::e_omp_schedule_kind_runtime})};
  }
  if (record.kind == "SgOmpDistScheduleClause")
    fields.dist_schedule =
        requiredEnum<SgOmpClause::omp_dist_schedule_kind_enum>(
            record.properties, "kind", record.kind,
            {SgOmpClause::e_omp_dist_schedule_kind_static});
  if (record.kind == "SgOmpInductionItem") {
    fields.induction_item = OmpInductionItemFields{
        requiredEnum<SgOmpClause::omp_induction_item_kind_enum>(
            record.properties, "kind", record.kind,
            {SgOmpClause::e_omp_induction_item_step,
             SgOmpClause::e_omp_induction_item_binding,
             SgOmpClause::e_omp_induction_item_expression}),
        record.properties.requiredString("label")};
    const auto &input = *fields.induction_item;
    if (!Rose::OpenMP::Detail::validInductionItemFields(input.kind,
                                                        input.label))
      throw std::runtime_error(
          "AST JSON induction item label disagrees with its kind");
  }
  if (record.kind == "SgOmpApplyClause") {
    fields.apply_label = record.properties.requiredString("label");
    if (fields.apply_label->empty() &&
        edgesFor(record, "transformations").empty())
      throw std::runtime_error(
          "AST JSON apply clause has neither a label nor a transformation");
  }
  if (record.kind == "SgOmpMapDistDataPolicy") {
    fields.dist_data_policy =
        requiredClosedEnumRange(record.properties, "policy", record.kind,
                                SgOmpClause::e_omp_map_dist_data_unknown,
                                SgOmpClause::e_omp_map_dist_data_last);
    if (!Rose::OpenMP::Detail::validMapDistDataPolicyFields(
            *fields.dist_data_policy,
            singleEdgeTarget(record, "expression") != 0))
      throw std::runtime_error(
          "AST JSON map dist_data policy payload disagrees with its kind");
  }
  if (record.kind == "SgOmpApplyTransformation") {
    fields.apply_transformation = OmpApplyTransformationFields{
        requiredEnum<SgOmpClause::omp_apply_transform_kind_enum>(
            record.properties, "kind", record.kind,
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
            record.properties, "separator", record.kind,
            {SgOmpClause::e_omp_clause_separator_none,
             SgOmpClause::e_omp_clause_separator_comma,
             SgOmpClause::e_omp_clause_separator_space}),
        record.properties.requiredString("transformation_name")};
    const auto &input = *fields.apply_transformation;
    if (!Rose::OpenMP::Detail::validApplyTransformationFields(
            input.kind, input.name, singleEdgeTarget(record, "argument") != 0,
            singleEdgeTarget(record, "nested_apply") != 0))
      throw std::runtime_error(
          "AST JSON apply transformation payload disagrees with its kind");
  }
  if (record.kind == "SgDesignator" && !fields.designator) {
    const auto kind = requiredEnum<SgDesignator::designator_kind_enum>(
        record.properties, "designator_kind", "SgDesignator",
        {SgDesignator::e_designator_field, SgDesignator::e_designator_array,
         SgDesignator::e_designator_array_range});
    const uint64_t first = requiredSingleEdgeTarget(record, "first_expression");
    const uint64_t second = singleEdgeTarget(record, "second_expression");
    if (first == second ||
        (kind == SgDesignator::e_designator_array_range) != (second != 0))
      throw std::runtime_error(
          "AST JSON designator has invalid typed operand edges");
    fields.designator = DesignatorInput{
        kind, first, second, requiredSingleEdgeTarget(record, "parent")};
  }
}

static std::optional<std::pair<uint64_t, bool>>
selectedDestructorRecord(const AstFileRecord &ast, const NodeRecord &scope) {
  std::optional<std::pair<uint64_t, bool>> selected;
  const auto inspect = [&](const NodeRecord &container, const char *field) {
    for (const auto &edge : edgesFor(container, field)) {
      const auto &member = ast.node(edge.target);
      if (!isMemberFunctionDeclarationRecordKind(member.kind))
        continue;
      const auto modifiers = functionModifierFieldsFromJson(member);
      if (!modifiers.special[SgSpecialFunctionModifier::e_destructor] ||
          modifiers.function[SgFunctionModifier::e_cxx_unselected_destructor])
        continue;
      const bool trivial =
          modifiers.function[SgFunctionModifier::e_cxx_trivial_destructor];
      if (!trivial &&
          !modifiers.function[SgFunctionModifier::e_cxx_nontrivial_destructor])
        throw std::runtime_error("AST JSON class has no selected destructor");
      const auto identity = std::make_pair(
          requiredSingleEdgeTarget(member, "firstNondefiningDeclaration"),
          trivial);
      if (selected && *selected != identity)
        throw std::runtime_error(
            "AST JSON class has conflicting destructor selections");
      selected = identity;
    }
  };
  inspect(scope, "members");
  if (const uint64_t auxiliary =
          singleEdgeTarget(scope, "auxiliary_declarations"))
    inspect(ast.node(auxiliary), "declarations");
  return selected;
}

static std::vector<uint64_t>
validateDestructorBaseTargets(const AstFileRecord &ast, const NodeRecord &owner,
                              const NodeRecord &epilogue,
                              const NodeMap &nodes) {
  const auto canonical = [&](uint64_t id) {
    const auto &declaration = ast.node(id);
    if (!isClassDeclarationRecordKind(declaration.kind))
      throw std::runtime_error("AST JSON destructor base is not a class");
    return requiredSingleEdgeTarget(declaration, "firstNondefiningDeclaration");
  };
  const uint64_t owner_scope = requiredSingleEdgeTarget(owner, "scope");
  std::vector<uint64_t> dependencies;
  std::unordered_map<uint64_t, std::optional<size_t>> direct_bases;
  std::unordered_map<uint64_t, std::optional<size_t>> virtual_bases;
  std::unordered_map<uint64_t, uint64_t> base_scopes;
  std::unordered_map<uint64_t, bool> complete;
  std::function<void(uint64_t)> visit = [&](uint64_t scope_id) {
    const auto [state, inserted] = complete.emplace(scope_id, false);
    if (!inserted) {
      if (!state->second)
        throw std::runtime_error("AST JSON destructor has cyclic inheritance");
      return;
    }
    const auto &scope = ast.node(scope_id);
    for (const auto &edge : edgesFor(scope, "inheritances")) {
      const auto &base = ast.node(edge.target);
      if (base.kind != "SgBaseClass")
        throw std::runtime_error(
            "AST JSON concrete destructor has a dependent base");
      if (!base.properties.requiredBool("is_direct_base_class"))
        continue;
      const uint64_t target =
          exactSerializedPointerTarget(base, "base_class", "base_class");
      const auto modifier =
          requiredEnum<SgBaseClassModifier::baseclass_modifier_enum>(
              base.properties, "base_class_modifier", "SgBaseClass",
              {SgBaseClassModifier::e_default, SgBaseClassModifier::e_virtual});
      const auto &definition = ast.node(
          requiredSingleEdgeTarget(ast.node(target), "definingDeclaration"));
      const uint64_t definition_scope =
          requiredSingleEdgeTarget(definition, "definition");
      const uint64_t canonical_target = canonical(target);
      const auto [scope_entry, new_scope] =
          base_scopes.emplace(canonical_target, definition_scope);
      if (!new_scope && scope_entry->second != definition_scope)
        throw std::runtime_error("AST JSON base destruction order has "
                                 "conflicting canonical definitions");
      visit(definition_scope);
      if (modifier == SgBaseClassModifier::e_virtual) {
        const size_t ordinal = virtual_bases.size();
        virtual_bases.emplace(canonical_target, ordinal);
      } else if (scope_id == owner_scope) {
        const size_t ordinal = direct_bases.size();
        if (!direct_bases.emplace(canonical_target, ordinal).second)
          throw std::runtime_error(
              "AST JSON base destruction order has duplicate canonical bases");
      }
    }
    complete.at(scope_id) = true;
  };
  visit(owner_scope);
  size_t previous_base = direct_bases.size();
  size_t previous_virtual = virtual_bases.size();
  for (const auto &edge : edgesFor(epilogue, "expressions")) {
    const auto &step = ast.node(edge.target);
    const auto kind = subobjectDestructionKindFromJson(step.properties);
    if (kind == SgCxxSubobjectDestruction::e_member)
      continue;
    const uint64_t target =
        canonical(requiredSingleEdgeTarget(step, "base_target"));
    const auto direct_base = direct_bases.find(target);
    const auto virtual_base = virtual_bases.find(target);
    if (kind == SgCxxSubobjectDestruction::e_virtual_base
            ? virtual_base == virtual_bases.end()
            : direct_base == direct_bases.end())
      throw std::runtime_error(
          "AST JSON destructor base target has no matching inheritance role");
    if (kind == SgCxxSubobjectDestruction::e_nonvirtual_base) {
      if (!direct_base->second || *direct_base->second >= previous_base)
        throw std::runtime_error(
            "AST JSON base destruction order is not reverse declaration order");
      previous_base = *direct_base->second;
      direct_base->second.reset();
    } else {
      if (!virtual_base->second || *virtual_base->second >= previous_virtual)
        throw std::runtime_error("AST JSON virtual base destruction order is "
                                 "not reverse declaration order");
      previous_virtual = *virtual_base->second;
      virtual_base->second.reset();
    }
  }
  const auto require_trivial = [&](const auto &remaining) {
    for (const auto &[base, ordinal] : remaining) {
      if (!ordinal)
        continue;
      const uint64_t scope_id = base_scopes.at(base);
      const auto &scope = ast.node(scope_id);
      const auto selected = selectedDestructorRecord(ast, scope);
      std::optional<bool> trivial;
      if (selected)
        trivial = selected->second;
      if (nodes.count(scope_id) != 0) {
        auto *destructor = SageInterface::getDefaultDestructor(
            nodeByIdAs<SgClassDefinition>(nodes, scope_id)->get_declaration());
        if (destructor == nullptr)
          throw std::runtime_error(
              "AST JSON borrowed base class has no selected destructor");
        const auto properties =
            destructor->get_functionModifier().getDestructorProperties();
        if (properties != SgFunctionModifier::e_destructor_selected_trivial &&
            properties != SgFunctionModifier::e_destructor_selected_nontrivial)
          throw std::runtime_error(
              "AST JSON borrowed base class has no selected destructor");
        const bool native_trivial =
            properties == SgFunctionModifier::e_destructor_selected_trivial;
        if (trivial.has_value() && *trivial != native_trivial)
          throw std::runtime_error("AST JSON base destructor selection "
                                   "disagrees with borrowed class");
        trivial = native_trivial;
      }
      if (!trivial.has_value())
        throw std::runtime_error(
            "AST JSON base class has no selected destructor");
      if (!*trivial)
        throw std::runtime_error(
            "AST JSON destructor epilogue omits a required base destructor");
      if (nodes.count(scope_id) == 0)
        dependencies.push_back(selected->first);
    }
  };
  require_trivial(direct_bases);
  require_trivial(virtual_bases);
  return dependencies;
}

static std::vector<uint64_t> destructorMemberRecords(const AstFileRecord &ast,
                                                     const NodeRecord &owner) {
  const auto &scope = ast.node(requiredSingleEdgeTarget(owner, "scope"));
  if (requiredClassType(
          ast.node(requiredSingleEdgeTarget(scope, "declaration")).properties,
          "destructor owner") == SgClassDeclaration::e_union)
    return {};
  std::vector<uint64_t> members;
  std::unordered_set<uint64_t> visited;
  std::set<unsigned int> indices;
  std::function<void(uint64_t)> inspect = [&](uint64_t id) {
    if (!visited.insert(id).second)
      throw std::runtime_error(
          "AST JSON destructor fields have duplicate or cyclic ownership");
    const auto &declaration = ast.node(id);
    if (declaration.kind == "SgDeclarationGroupStatement") {
      for (const auto &edge : edgesFor(declaration, "declarations"))
        inspect(edge.target);
    } else if (declaration.kind == "SgVariableDeclaration") {
      for (const auto &edge : edgesFor(declaration, "variables")) {
        const auto &member = ast.node(edge.target);
        const auto index = requiredOptionalUnsignedInteger(
            member.properties, "cxx_field_index", "destructor member");
        if (!index)
          continue;
        if (member.kind != "SgInitializedName" ||
            requiredSingleEdgeTarget(member, "scope") != scope.id ||
            requiredSingleEdgeTarget(member, "parent") != id ||
            requiredSingleEdgeTarget(member, "declptr") != id ||
            requiredStorageModifier(
                declaration.properties, "declaration_storage_modifier",
                "destructor member") == SgStorageModifier::e_static ||
            !indices.insert(*index).second)
          throw std::runtime_error(
              "AST JSON destructor has no exact physical field set");
        members.push_back(edge.target);
      }
    }
  };
  for (const auto &edge : edgesFor(scope, "members"))
    inspect(edge.target);
  if (const uint64_t auxiliary =
          singleEdgeTarget(scope, "auxiliary_declarations"))
    for (const auto &edge : edgesFor(ast.node(auxiliary), "declarations"))
      inspect(edge.target);
  return members;
}

static std::optional<SgInitializedNamePtrList>
destructorMemberInputs(const std::vector<uint64_t> &ids, const NodeMap &nodes) {
  SgInitializedNamePtrList members;
  for (uint64_t id : ids) {
    if (nodes.count(id) == 0)
      return std::nullopt;
    auto *member = nodeByIdAs<SgInitializedName>(nodes, id);
    auto *variable = isSgVariableDeclaration(member->get_parent());
    if (variable == nullptr || member->get_declptr() != variable ||
        std::count(variable->get_variables().begin(),
                   variable->get_variables().end(), member) != 1)
      return std::nullopt;
    if (auto *object = SgCxxSubobjectDestruction::get_member_class(member))
      if (SageInterface::getDefaultDestructor(
              isSgClassDeclaration(object->get_declaration())) == nullptr)
        return std::nullopt;
    members.push_back(member);
  }
  return members;
}

static void validateNonrealAliasTypes(const AstFileRecord &ast,
                                      const NodeMap &nodes) {
  std::unordered_set<uint64_t> active;
  std::unordered_set<uint64_t> checked;
  auto alias_template_target = [&](uint64_t target) {
    std::unordered_set<uint64_t> using_targets;
    while (target != 0) {
      if (ast.index_by_id.count(target) == 0) {
        auto *declaration = nodeByIdAs<SgDeclarationStatement>(nodes, target);
        std::unordered_set<SgDeclarationStatement *> existing_targets;
        while (auto *using_declaration =
                   isSgUsingDeclarationStatement(declaration)) {
          if (!existing_targets.insert(using_declaration).second)
            throw std::runtime_error(
                "AST JSON alias template target contains a cycle");
          declaration = using_declaration->get_declaration();
          if (declaration == nullptr)
            throw std::runtime_error(
                "AST JSON alias template using target is null");
        }
        return isSgTemplateTypedefDeclaration(declaration) != nullptr;
      }
      const auto &declaration = ast.node(target);
      if (declaration.kind != "SgUsingDeclarationStatement")
        return declaration.kind == "SgTemplateTypedefDeclaration";
      if (!using_targets.insert(target).second)
        throw std::runtime_error(
            "AST JSON alias template target contains a cycle");
      target = exactSerializedPointerTarget(declaration, "declaration",
                                            "declaration");
      if (target == 0)
        throw std::runtime_error(
            "AST JSON alias template using target is null");
    }
    return false;
  };
  std::function<void(const JsonValue &)> visit;
  std::function<void(uint64_t, bool)> visit_declaration;
  visit_declaration = [&](uint64_t id, bool nonreal) {
    if (active.count(id) != 0)
      throw std::runtime_error(
          "AST JSON alias type projection contains a cycle");
    if (ast.index_by_id.count(id) == 0) {
      if (nonreal) {
        auto *declaration = nodeByIdAs<SgNonrealDecl>(nodes, id);
        auto *type = isSgNonrealType(declaration->get_type());
        if (type == nullptr || type->get_declaration() != declaration ||
            type->get_parent() != declaration)
          throw std::runtime_error(
              "AST JSON SgNonrealType conflicts with its exact declaration "
              "owner");
        type->validate_aliased_type();
      } else {
        auto *declaration = nodeByIdAs<SgTypedefDeclaration>(nodes, id);
        auto *type = isSgTypedefType(declaration->get_type());
        if (type == nullptr || type->get_declaration() != declaration ||
            declaration->get_base_type() == nullptr)
          throw std::runtime_error(
              "AST JSON alias projection has no exact typedef declaration");
        for (auto *internal : type->getInternalTypes())
          if (auto *alias = isSgNonrealType(internal))
            alias->validate_aliased_type();
      }
      return;
    }
    const auto &declaration = ast.node(id);
    const JsonValue *projection = nullptr;
    if (nonreal) {
      if (declaration.kind != "SgNonrealDecl")
        throw std::runtime_error(
            "AST JSON SgNonrealType has no exact nonreal declaration");
      const auto &type = declaration.properties.at("type");
      if (!type.requiredBool("present") ||
          type.requiredString("kind") != "SgNonrealType" ||
          type.requiredBool("fortran_source_syntax") ||
          requiredPositiveId(type.at("declaration"),
                             "nonreal type declaration") != id)
        throw std::runtime_error(
            "AST JSON SgNonrealDecl requires its exact owned type identity");
      if (type.find("aliased_type") != nullptr)
        throw std::runtime_error("AST JSON SgNonrealType alias projection must "
                                 "be stored on its declaration");
      projection = &declaration.properties.at("aliased_type");
      const bool requires_projection =
          declaration.properties.requiredInt("nonreal_template_role") ==
              SgNonrealDecl::e_nonreal_template_id &&
          alias_template_target(
              singleEdgeTarget(declaration, "templateDeclaration"));
      if (projection->requiredBool("present") != requires_projection)
        throw std::runtime_error("AST JSON SgNonrealType alias projection "
                                 "disagrees with its template target");
    } else {
      if (declaration.kind != "SgTypedefDeclaration" &&
          declaration.kind != "SgTemplateTypedefDeclaration" &&
          declaration.kind != "SgTemplateInstantiationTypedefDeclaration")
        throw std::runtime_error(
            "AST JSON alias projection has no exact typedef declaration");
      projection = &declaration.properties.at("base_type");
    }
    if (!checked.insert(id).second)
      return;
    active.insert(id);
    visit(*projection);
    active.erase(id);
  };
  visit = [&](const JsonValue &value) {
    if (value.kind == JsonValue::Kind::Array) {
      for (const auto &element : value.array)
        visit(element);
      return;
    }
    if (value.kind != JsonValue::Kind::Object)
      return;
    const auto *kind = value.find("kind");
    if (kind != nullptr && kind->kind == JsonValue::Kind::String &&
        (kind->asString() == "SgNonrealType" ||
         kind->asString() == "SgTypedefType")) {
      const uint64_t id = requiredPositiveId(
          value.at("declaration"), "alias projection type declaration");
      const bool nonreal = kind->asString() == "SgNonrealType";
      if (nonreal && value.find("aliased_type") != nullptr)
        throw std::runtime_error("AST JSON SgNonrealType alias projection must "
                                 "be stored on its declaration");
      visit_declaration(id, nonreal);
      return;
    }
    if (kind != nullptr && kind->kind == JsonValue::Kind::String &&
        (kind->asString() == "SgClassType" ||
         kind->asString() == "SgEnumType")) {
      if (const auto *declaration = value.find("declaration");
          declaration != nullptr &&
          active.count(requiredPositiveId(*declaration,
                                          "alias projection declaration")) != 0)
        throw std::runtime_error(
            "AST JSON alias type projection contains a cycle");
    }
    for (const auto &field : value.object)
      visit(field.second);
  };
  for (const auto &record : ast.nodes)
    if (record.kind == "SgNonrealDecl")
      visit_declaration(record.id, true);
}

static RecordConstructionFieldMap
validateConstructorOnlyRecords(const AstFileRecord &ast,
                               const NodeMap &nodes = {}) {
  for (const auto &record : ast.nodes)
    if (&ast.node(record.id) != &record)
      throw std::runtime_error("AST JSON duplicate construction record id " +
                               std::to_string(record.id));
  validateNonrealAliasTypes(ast, nodes);
  RecordConstructionFieldMap construction_fields;
  std::unordered_map<uint64_t, uint64_t> context_owners;
  const VariantVector expression_variants(V_SgExpression);
  const VariantVector initializer_variants(V_SgInitializer);
  const VariantVector statement_variants(V_SgStatement);
  const VariantVector image_control_variants(V_SgImageControlStatement);
  const VariantVector declaration_variants(V_SgDeclarationStatement);
  const VariantVector class_variants(V_SgClassDeclaration);
  const VariantVector scope_variants(V_SgScopeStatement);
  const VariantVector omp_clause_statements(V_SgOmpClauseStatement);
  const VariantVector omp_clause_bodies(V_SgOmpClauseBodyStatement);
  const VariantVector omp_executable_statements(V_SgOmpExecStatement);
  const auto is_expression = [&](const NodeRecord &record) {
    return kindInVariants(record.kind, expression_variants);
  };
  const auto is_statement = [&](const NodeRecord &record) {
    return kindInVariants(record.kind, statement_variants);
  };
  const auto require_destructor = [&](uint64_t id) {
    if (nodes.count(id) != 0) {
      const auto &modifier = nodeByIdAs<SgMemberFunctionDeclaration>(nodes, id)
                                 ->get_functionModifier();
      if (modifier.isDeleted())
        throw std::runtime_error(
            "AST JSON cleanup selects a deleted declaration");
      const auto selection = modifier.getDestructorProperties();
      if (selection != SgFunctionModifier::e_destructor_selected_trivial &&
          selection != SgFunctionModifier::e_destructor_selected_nontrivial)
        throw std::runtime_error("AST JSON cleanup has no selected destructor");
    }
    const auto &callee = ast.node(id);
    if (!isMemberFunctionDeclarationRecordKind(callee.kind))
      throw std::runtime_error("AST JSON cleanup has no member destructor");
    const auto modifiers = functionModifierFieldsFromJson(callee);
    if (!modifiers.special[SgSpecialFunctionModifier::e_destructor] ||
        (!modifiers.function[SgFunctionModifier::e_cxx_trivial_destructor] &&
         !modifiers.function[SgFunctionModifier::e_cxx_nontrivial_destructor]))
      throw std::runtime_error("AST JSON cleanup has no selected destructor");
    if (modifiers.function[SgFunctionModifier::e_cxx_deleted])
      throw std::runtime_error(
          "AST JSON cleanup selects a deleted declaration");
  };
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgMemberFunctionRefExp" ||
        record.kind == "SgTemplateMemberFunctionRefExp")
      for (const char *field : {"virtual_call", "need_qualifier"})
        (void)requiredIntegerBoolean(record.properties, field);
    if (record.properties.find("bound_temporary_destructor") != nullptr) {
      require_destructor(exactSerializedPointerTarget(
          record, "bound_temporary_destructor", "bound_temporary_destructor"));
    }
    if (record.properties.find("materialized_lifetime_owner") != nullptr) {
      const uint64_t owner = exactSerializedPointerTarget(
          record, "materialized_lifetime_owner", "materialized_lifetime_owner");
      if (owner != 0 && ast.node(owner).kind != "SgInitializedName")
        throw std::runtime_error(
            "AST JSON materialized temporary has no initialized-name owner");
      if (owner != 0 &&
          record.properties.requiredInt("materialized_storage_duration") ==
              SgExpression::e_temporary_storage_containing_object) {
        const auto &field = ast.node(owner);
        const auto &declaration =
            ast.node(requiredSingleEdgeTarget(field, "parent"));
        const auto &scope = ast.node(requiredSingleEdgeTarget(field, "scope"));
        const auto variables = edgesFor(declaration, "variables");
        const auto storage = requiredStorageModifier(
            declaration.properties, "declaration_storage_modifier",
            "materialized temporary owner");
        if (!requiredOptionalUnsignedInteger(field.properties,
                                             "cxx_field_index",
                                             "materialized temporary owner") ||
            declaration.kind != "SgVariableDeclaration" ||
            requiredSingleEdgeTarget(field, "declptr") != declaration.id ||
            (scope.kind != "SgClassDefinition" &&
             scope.kind != "SgTemplateClassDefinition" &&
             scope.kind != "SgTemplateInstantiationDefn") ||
            (storage != SgStorageModifier::e_default &&
             storage != SgStorageModifier::e_mutable) ||
            std::count_if(variables.begin(), variables.end(),
                          [owner](const EdgeRecord &edge) {
                            return edge.target == owner;
                          }) != 1 ||
            (nodes.count(owner) != 0 &&
             !SgExpression::is_valid_temporary_materialization(
                 SgExpression::e_temporary_storage_containing_object,
                 nodeByIdAs<SgInitializedName>(nodes, owner))))
          throw std::runtime_error(
              "AST JSON materialized temporary has no exact containing field");
      } else if (owner != 0) {
        const auto duration =
            static_cast<SgExpression::temporary_storage_duration_enum>(
                record.properties.requiredInt("materialized_storage_duration"));
        const auto &name = ast.node(owner);
        std::unordered_set<uint64_t> visited;
        bool static_storage = false, thread_storage = false;
        for (uint64_t current = owner; current != 0;) {
          const auto &item = ast.node(current);
          const auto &declaration =
              ast.node(requiredSingleEdgeTarget(item, "parent"));
          const auto variables = edgesFor(declaration, "variables");
          if (!visited.insert(current).second ||
              item.kind != "SgInitializedName" ||
              declaration.kind != "SgVariableDeclaration" ||
              requiredSingleEdgeTarget(item, "declptr") != declaration.id ||
              requiredOptionalUnsignedInteger(item.properties,
                                              "cxx_field_index",
                                              "materialized temporary owner") ||
              std::count_if(variables.begin(), variables.end(),
                            [current](const EdgeRecord &edge) {
                              return edge.target == current;
                            }) != 1)
            throw std::runtime_error(
                "AST JSON materialized temporary has no exact lifetime owner");
          const auto storage = requiredStorageModifier(
              declaration.properties, "declaration_storage_modifier",
              "materialized temporary owner");
          static_storage |= storage == SgStorageModifier::e_static ||
                            storage == SgStorageModifier::e_extern;
          thread_storage |=
              declaration.properties.requiredBool(
                  "declaration_thread_local_storage") ||
              declaration.properties.requiredBool("is_thread_local");
          current = singleEdgeTarget(item, "prev_decl_item");
        }
        const NodeRecord *scope =
            &ast.node(requiredSingleEdgeTarget(name, "scope"));
        while (scope->kind == "SgDeclarationScope") {
          if (!visited.insert(scope->id).second)
            throw std::runtime_error(
                "AST JSON materialized temporary has no exact lifetime owner");
          const auto outer = declarationScopeOuterId(ast, *scope);
          if (outer == 0)
            throw std::runtime_error(
                "AST JSON materialized temporary has no exact lifetime owner");
          scope = &ast.node(outer);
        }
        if (static_cast<size_t>(scope->variant) >= V_SgNumVariants ||
            scope->kind != roseGlobalVariantNameList[scope->variant] ||
            !Rose::AST::temporaryStorageMatchesOwner(
                duration, scope->variant, static_storage, thread_storage) ||
            (nodes.count(owner) != 0 &&
             !SgExpression::is_valid_temporary_materialization(
                 duration, nodeByIdAs<SgInitializedName>(nodes, owner))))
          throw std::runtime_error(
              "AST JSON materialized temporary has no exact lifetime owner");
      }
    }
    if (record.kind == "SgInterfaceStatement")
      (void)fortranInterfaceFieldsFromJson(record.properties);
    if (kindInVariants(record.kind, omp_executable_statements))
      (void)record.properties.requiredBool("source_form_is_attribute");
    if (record.kind == "SgOmpTaskStatement") {
      const bool available = record.properties.requiredBool(
          "semantic_data_environment_is_available");
      std::unordered_set<uint64_t> variables;
      for (const char *role :
           {"semantic_firstprivate_variables", "semantic_private_variables"}) {
        size_t index = 0;
        for (const auto &edge : edgesFor(record, role))
          if (!available || edge.index != index++ ||
              ast.node(edge.target).kind != "SgInitializedName" ||
              !variables.insert(edge.target).second)
            throw std::runtime_error(
                "AST JSON task has malformed semantic variable references");
      }
      if (!available && singleEdgeTarget(record, "body") != 0)
        throw std::runtime_error(
            "AST JSON executable task has no semantic data environment");
    }
    if (kindInVariants(record.kind, omp_clause_statements) ||
        kindInVariants(record.kind, omp_clause_bodies) ||
        record.kind == "SgOmpGroupprivateStatement") {
      if (!edgesFor(record, "clauses").empty())
        throw std::runtime_error(
            "AST JSON OpenMP statement has flattened 'clauses' edges");
      const auto &list =
          ast.node(requiredSingleEdgeTarget(record, "clause_list"));
      if (list.kind != "SgOmpClauseList" ||
          requiredSingleEdgeTarget(list, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenMP clause list has no exact owning statement");
    }
    if (record.kind == "SgOmpClauseList") {
      static const VariantVector clauses(V_SgOmpClause);
      std::unordered_set<uint64_t> children;
      for (const auto &edge : edgesFor(record, "clauses")) {
        const auto &child = ast.node(edge.target);
        if (!kindInVariants(child.kind, clauses) ||
            singleEdgeTarget(child, "parent") != record.id ||
            !children.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON OpenMP clause list has conflicting child ownership");
      }
    }
    (void)binaryStorageSizeFromJson(record.properties, record.kind);
    (void)binarySubscriptStridesFromJson(record.properties, record.kind);
    if (record.properties.find("io_statement"))
      throw std::runtime_error(
          "AST JSON retains removed I/O statement classification");
    if (record.kind == "SgStatementAttribute") {
      if (uint64_t operand = singleEdgeTarget(record, "expression_argument")) {
        const auto &child = ast.node(operand);
        if (!is_expression(child) ||
            singleEdgeTarget(child, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON statement attribute operand has invalid kind or owner");
      }
    } else if (record.kind == "SgStatementAttributeList") {
      std::unordered_set<uint64_t> attributes;
      for (const auto &edge : edgesFor(record, "attributes")) {
        const auto &child = ast.node(edge.target);
        if (child.kind != "SgStatementAttribute" ||
            singleEdgeTarget(child, "parent") != record.id ||
            !attributes.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON attribute list has invalid child kind or ownership");
      }
    } else if (record.kind == "SgAttributedStatement") {
      const auto &statement =
          ast.node(requiredSingleEdgeTarget(record, "statement"));
      const auto &list =
          ast.node(requiredSingleEdgeTarget(record, "attribute_list"));
      if (!is_statement(statement) || statement.id == record.id ||
          singleEdgeTarget(statement, "parent") != record.id ||
          list.kind != "SgStatementAttributeList" ||
          singleEdgeTarget(list, "parent") != record.id ||
          edgesFor(list, "attributes").empty())
        throw std::runtime_error(
            "AST JSON attributed statement has invalid children or ownership");
    }
    if (record.kind == "SgDesignatedInitializer") {
      const auto &list =
          ast.node(requiredSingleEdgeTarget(record, "designatorList"));
      const auto &member =
          ast.node(requiredSingleEdgeTarget(record, "memberInit"));
      if (list.kind != "SgExprListExp" ||
          singleEdgeTarget(list, "parent") != record.id ||
          !kindInVariants(member.kind, initializer_variants) ||
          singleEdgeTarget(member, "parent") != record.id ||
          edgesFor(list, "expressions").empty())
        throw std::runtime_error("AST JSON designated initializer has invalid "
                                 "children or ownership");
      for (const auto &edge : edgesFor(list, "expressions"))
        if (ast.node(edge.target).kind != "SgDesignator")
          throw std::runtime_error(
              "AST JSON designated initializer has a non-designator entry");
    }
    if (uint64_t scope_id =
            singleEdgeTarget(record, "source_declarator_scope")) {
      const bool declaration =
          kindInVariants(record.kind, declaration_variants);
      const NodeRecord &scope = ast.node(scope_id);
      const auto children = edgesFor(scope, "declarations");
      if (!declaration || isFunctionDeclarationRecordKind(record.kind) ||
          scope.kind != "SgDeclarationScope" ||
          requiredSingleEdgeTarget(scope, "parent") != record.id ||
          scope.properties.requiredBool("is_default_nonreal_scope") ||
          children.empty())
        throw std::runtime_error(
            "AST JSON source declarator scope has no exact typed owner");
      std::unordered_set<uint64_t> tags;
      for (const auto &edge : children) {
        const NodeRecord &tag = ast.node(edge.target);
        const bool class_tag = kindInVariants(tag.kind, class_variants);
        if ((!class_tag && tag.kind != "SgEnumDeclaration") ||
            !tags.insert(edge.target).second ||
            requiredSingleEdgeTarget(tag, "parent") != scope_id ||
            singleEdgeTarget(tag, "scope") == 0 ||
            tag.properties.requiredBool("is_autonomous_declaration"))
          throw std::runtime_error(
              "AST JSON source declarator scope has malformed tag ownership");
      }
    }
    if (record.kind == "SgIfStmt" || record.kind == "SgWhileStmt" ||
        record.kind == "SgForStatement" || record.kind == "SgSwitchStatement") {
      const char *field = record.kind == "SgIfStmt"            ? "conditional"
                          : record.kind == "SgForStatement"    ? "test"
                          : record.kind == "SgSwitchStatement" ? "item_selector"
                                                               : "condition";
      const uint64_t condition = singleEdgeTarget(record, field);
      const uint64_t value = singleEdgeTarget(record, "condition_value");
      const bool declaration =
          condition != 0 && ast.node(condition).kind == "SgVariableDeclaration";
      if (declaration != (value != 0) ||
          (value != 0 &&
           (!is_expression(ast.node(value)) ||
            singleEdgeTarget(ast.node(value), "parent") != record.id)))
        throw std::runtime_error(
            "AST JSON declaration condition has no exact converted predicate");
    }
    if (record.kind == "SgForStatement") {
      const NodeRecord &initializer =
          ast.node(requiredSingleEdgeTarget(record, "for_init_stmt"));
      if (initializer.kind != "SgForInitStatement" ||
          singleEdgeTarget(initializer, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON FOR initializer has no exact typed owner");
    }
    if (record.kind == "SgIfStmt" || record.kind == "SgWhileStmt" ||
        record.kind == "SgDoWhileStmt" || record.kind == "SgSwitchStatement") {
      const char *condition_edge = record.kind == "SgIfStmt" ? "conditional"
                                   : record.kind == "SgSwitchStatement"
                                       ? "item_selector"
                                       : "condition";
      const auto if_kind = record.kind == "SgIfStmt"
                               ? ifStatementKindFromJson(record.properties)
                               : SgIfStmt::e_if_ordinary;
      const bool is_consteval = if_kind == SgIfStmt::e_if_consteval ||
                                if_kind == SgIfStmt::e_if_not_consteval;
      const uint64_t condition =
          is_consteval ? singleEdgeTarget(record, condition_edge)
                       : requiredSingleEdgeTarget(record, condition_edge);
      const uint64_t body = requiredSingleEdgeTarget(
          record, record.kind == "SgIfStmt" ? "true_body" : "body");
      const uint64_t alternative = record.kind == "SgIfStmt"
                                       ? singleEdgeTarget(record, "false_body")
                                       : 0;
      if (is_consteval) {
        if (condition != 0 || ast.node(body).kind != "SgBasicBlock" ||
            (alternative != 0 && ast.node(alternative).kind != "SgBasicBlock"))
          throw std::runtime_error(
              "AST JSON IF has an invalid consteval shape");
      } else {
        const NodeRecord &selector = ast.node(condition);
        if (selector.kind != "SgExprStatement" &&
            selector.kind != "SgVariableDeclaration")
          throw std::runtime_error(
              "AST JSON control has an invalid condition kind");
      }
      if ((condition != 0 && (condition == body || condition == alternative)) ||
          body == alternative)
        throw std::runtime_error("AST JSON control children share one node");
      for (uint64_t id : {condition, body, alternative}) {
        if (id == 0)
          continue;
        const NodeRecord &child = ast.node(id);
        if (id == record.id || !is_statement(child) ||
            singleEdgeTarget(child, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON control child has no exact statement owner");
      }
      if (record.kind == "SgIfStmt") {
        const bool use_then =
            record.properties.requiredBool("use_then_keyword");
        const bool is_constexpr = if_kind == SgIfStmt::e_if_constexpr;
        if (use_then && if_kind != SgIfStmt::e_if_ordinary)
          throw std::runtime_error("AST JSON IF has an invalid Fortran role");
        if (record.properties.requiredBool("is_else_if_statement")) {
          const uint64_t owner = singleEdgeTarget(record, "parent");
          if (!use_then || is_constexpr ||
              record.properties.requiredBool("has_end_statement") ||
              owner == 0 || ast.node(owner).kind != "SgIfStmt" ||
              singleEdgeTarget(ast.node(owner), "false_body") != record.id)
            throw std::runtime_error("AST JSON IF has an invalid ELSE IF role");
        }
        if (use_then && alternative != 0 &&
            ast.node(alternative).kind != "SgBasicBlock" &&
            (ast.node(alternative).kind != "SgIfStmt" ||
             !ast.node(alternative)
                  .properties.requiredBool("is_else_if_statement")))
          throw std::runtime_error("AST JSON IF has an invalid ELSE IF role");
        if (!record.properties.requiredString("else_construct_name").empty() &&
            (!use_then || is_constexpr || alternative == 0 ||
             ast.node(alternative).kind != "SgBasicBlock"))
          throw std::runtime_error(
              "AST JSON IF has an invalid named ELSE role");
      }
    }
    if (record.kind == "SgRangeBasedForStatement") {
      std::unordered_set<uint64_t> children;
      const auto child_record = [&](const char *field,
                                    bool required) -> const NodeRecord * {
        const uint64_t id = required ? requiredSingleEdgeTarget(record, field)
                                     : singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        const NodeRecord &child = ast.node(id);
        if (id == record.id || singleEdgeTarget(child, "parent") != record.id ||
            !children.insert(id).second)
          throw std::runtime_error(
              "AST JSON range-for child has no unique owner");
        return &child;
      };
      const bool has_iteration =
          singleEdgeTarget(record, "begin_declaration") != 0;
      for (const char *field : {"begin_declaration", "end_declaration",
                                "not_equal_expression", "increment_expression"})
        if ((singleEdgeTarget(record, field) != 0) != has_iteration)
          throw std::runtime_error(
              "AST JSON range-for has partial iteration children");
      for (const char *field : {"iterator_declaration", "range_declaration",
                                "begin_declaration", "end_declaration"}) {
        const NodeRecord *child = child_record(
            field, std::string_view(field) == "iterator_declaration" ||
                       std::string_view(field) == "range_declaration");
        if (child == nullptr)
          continue;
        const auto names = edgesFor(*child, "variables");
        if (child->kind != "SgVariableDeclaration" || names.size() != 1)
          throw std::runtime_error(
              "AST JSON range-for declaration has no single variable");
        const NodeRecord &name = ast.node(names.front().target);
        if (name.kind != "SgInitializedName" ||
            singleEdgeTarget(name, "parent") != child->id ||
            singleEdgeTarget(name, "declptr") != child->id ||
            singleEdgeTarget(name, "scope") != record.id)
          throw std::runtime_error(
              "AST JSON range-for variable has no exact declaration and scope");
      }
      for (const char *field : {"not_equal_expression", "increment_expression"})
        if (const NodeRecord *child = child_record(field, false))
          if (!is_expression(*child))
            throw std::runtime_error(
                "AST JSON range-for iteration child is not an expression");
      if (!is_statement(*child_record("loop_body", true)))
        throw std::runtime_error("AST JSON range-for body is not a statement");
    }
    if (record.kind == "SgTypeIdOp") {
      const bool evaluated =
          record.properties.requiredBool("operand_is_evaluated");
      const uint64_t operand = singleEdgeTarget(record, "operand_expr");
      const bool type_operand =
          record.properties.at("operand_type").requiredBool("present");
      if ((operand != 0) == type_operand || (evaluated && type_operand) ||
          (operand != 0 && !is_expression(ast.node(operand))) ||
          singleEdgeTarget(record, "type_defining_declaration") != 0)
        throw std::runtime_error("AST JSON typeid requires one typed operand "
                                 "and a consistent evaluation role");
    }
    if (record.kind == "SgAggregateInitializer") {
      const auto roles = aggregateSourceElementRolesFromJson(record.properties);
      const auto &list =
          ast.node(requiredSingleEdgeTarget(record, "initializers"));
      if (list.kind != "SgExprListExp")
        throw std::runtime_error(
            "AST JSON aggregate initializers are not an expression list");
      if (!roles.empty() &&
          roles.size() != edgesFor(list, "expressions").size())
        throw std::runtime_error(
            "AST JSON aggregate source role count differs from elements");
    }
    if (record.kind == "SgInitializedName") {
      const auto binding_kind =
          structuredBindingKindFromJson(record.properties);
      const auto binding_owner =
          singleEdgeTarget(record, "structured_binding_owner");
      const auto binding_value =
          singleEdgeTarget(record, "structured_binding_value");
      const auto binding_holding =
          singleEdgeTarget(record, "structured_binding_holding_variable");
      const auto binding_scope =
          singleEdgeTarget(record, "structured_binding_scope");
      for (const char *field :
           {"structured_binding_owner", "structured_binding_value",
            "structured_binding_holding_variable", "structured_binding_scope"})
        if (record.properties.requiredNonnegativeNodeId(field) !=
            singleEdgeTarget(record, field))
          throw std::runtime_error(
              "AST JSON structured-binding property disagrees with its edge");
      if (binding_kind != SgInitializedName::e_not_structured_binding &&
          binding_scope != 0)
        throw std::runtime_error(
            "AST JSON structured-binding alias owns a decomposition scope");
      if (binding_kind == SgInitializedName::e_not_structured_binding) {
        if (binding_owner != 0 || binding_value != 0 || binding_holding != 0)
          throw std::runtime_error(
              "AST JSON ordinary initialized name has structured-binding data");
      } else if (binding_kind ==
                 SgInitializedName::e_structured_binding_holding_variable) {
        if (binding_owner == 0 || binding_owner == record.id ||
            binding_value != 0 || binding_holding != 0 ||
            singleEdgeTarget(record, "initptr") == 0 ||
            ast.node(binding_owner).kind != "SgInitializedName" ||
            !ast.node(binding_owner).properties.requiredName("name").is_null())
          throw std::runtime_error("AST JSON structured-binding holding "
                                   "variable has invalid ownership");
        const auto &scope = ast.node(requiredSingleEdgeTarget(record, "scope"));
        if (scope.kind != "SgDeclarationScope" ||
            singleEdgeTarget(scope, "semantic_outer_scope") !=
                singleEdgeTarget(ast.node(binding_owner),
                                 "structured_binding_scope"))
          throw std::runtime_error(
              "AST JSON structured-binding holding variable has invalid scope");
        const auto &pattern = ast.node(requiredSingleEdgeTarget(
            ast.node(binding_owner), "structured_binding_pattern"));
        size_t matches = 0;
        for (const auto &edge : edgesFor(pattern, "expressions")) {
          const auto &reference = ast.node(edge.target);
          if (reference.kind != "SgVarRefExp")
            throw std::runtime_error(
                "AST JSON structured-binding pattern is invalid");
          const auto &alias = ast.node(
              reference.properties.at("symbol").requiredNonnegativeNodeId(
                  "symbol_declaration"));
          if (singleEdgeTarget(alias, "structured_binding_holding_variable") ==
                  record.id &&
              singleEdgeTarget(alias, "structured_binding_owner") ==
                  binding_owner)
            ++matches;
        }
        if (matches != 1)
          throw std::runtime_error("AST JSON structured-binding holding "
                                   "variable has no exact alias");
      } else {
        if (binding_owner == 0 || binding_owner == record.id ||
            record.properties.requiredName("name").is_null() ||
            singleEdgeTarget(record, "initptr") != 0 ||
            ast.node(binding_owner).kind != "SgInitializedName" ||
            !ast.node(binding_owner)
                 .properties.requiredName("name")
                 .is_null() ||
            singleEdgeTarget(record, "scope") !=
                singleEdgeTarget(ast.node(binding_owner),
                                 "structured_binding_scope") ||
            (binding_kind == SgInitializedName::e_dependent_structured_binding
                 ? binding_value != 0 || binding_holding != 0
                 : binding_value == 0) ||
            (binding_value != 0 &&
             (!kindInVariants(ast.node(binding_value).kind,
                              expression_variants) ||
              singleEdgeTarget(ast.node(binding_value), "parent") !=
                  record.id)) ||
            (binding_holding != 0 &&
             (ast.node(binding_holding).kind != "SgInitializedName" ||
              structuredBindingKindFromJson(
                  ast.node(binding_holding).properties) !=
                  SgInitializedName::e_structured_binding_holding_variable ||
              singleEdgeTarget(ast.node(binding_holding),
                               "structured_binding_owner") != binding_owner)))
          throw std::runtime_error(
              "AST JSON structured-binding alias has invalid typed ownership");
        const auto &pattern = ast.node(requiredSingleEdgeTarget(
            ast.node(binding_owner), "structured_binding_pattern"));
        size_t matches = 0;
        for (const auto &edge : edgesFor(pattern, "expressions")) {
          const auto &element = ast.node(edge.target);
          if (element.kind == "SgVarRefExp" &&
              element.properties.at("symbol").requiredNonnegativeNodeId(
                  "symbol_declaration") == record.id)
            ++matches;
        }
        if (matches != 1)
          throw std::runtime_error(
              "AST JSON structured-binding alias has no exact pattern entry");
      }
      const uint64_t pattern =
          singleEdgeTarget(record, "structured_binding_pattern");
      if (record.properties.requiredNonnegativeNodeId(
              "structured_binding_pattern") != pattern)
        throw std::runtime_error("AST JSON structured-binding pattern property "
                                 "disagrees with its edge");
      if ((pattern == 0) != (binding_scope == 0))
        throw std::runtime_error(
            "AST JSON structured-binding pattern has no exact owned scope");
      if (pattern != 0) {
        const auto &list = ast.node(pattern);
        const auto &scope = ast.node(binding_scope);
        if (singleEdgeTarget(record, "initptr") == 0 ||
            list.kind != "SgExprListExp" ||
            scope.kind != "SgDeclarationScope" ||
            singleEdgeTarget(scope, "parent") != record.id ||
            singleEdgeTarget(scope, "semantic_outer_scope") != 0 ||
            scope.properties.requiredBool("is_default_nonreal_scope") ||
            binding_kind != SgInitializedName::e_not_structured_binding ||
            !record.properties.requiredName("name").is_null() ||
            singleEdgeTarget(list, "parent") != record.id ||
            ast.node(requiredSingleEdgeTarget(record, "parent")).kind !=
                "SgVariableDeclaration" ||
            singleEdgeTarget(record, "declptr") !=
                singleEdgeTarget(record, "parent"))
          throw std::runtime_error(
              "AST JSON structured-binding pattern has invalid ownership");
        std::unordered_set<uint64_t> bindings;
        for (const auto &edge : edgesFor(list, "expressions")) {
          const auto &element = ast.node(edge.target);
          if (element.kind != "SgVarRefExp" ||
              singleEdgeTarget(element, "parent") != pattern)
            throw std::runtime_error(
                "AST JSON structured-binding pattern has an invalid element");
          const auto &symbol = element.properties.at("symbol");
          const auto &binding =
              ast.node(symbol.requiredNonnegativeNodeId("symbol_declaration"));
          if (symbol.requiredString("symbol_kind") != "SgVariableSymbol" ||
              binding.kind != "SgInitializedName" || binding.id == record.id ||
              singleEdgeTarget(binding, "structured_binding_owner") !=
                  record.id ||
              (structuredBindingKindFromJson(binding.properties) !=
                   SgInitializedName::e_resolved_structured_binding &&
               structuredBindingKindFromJson(binding.properties) !=
                   SgInitializedName::e_dependent_structured_binding) ||
              binding.properties.requiredName("name").is_null() ||
              singleEdgeTarget(binding, "scope") != binding_scope ||
              !bindings.insert(binding.id).second)
            throw std::runtime_error("AST JSON structured-binding pattern has "
                                     "an invalid named binding");
        }
      }
      const uint64_t shape =
          singleEdgeTarget(record, "fortran_cray_pointer_pointee_shape");
      if (record.properties.requiredNonnegativeNodeId(
              "fortran_cray_pointer_pointee_shape") != shape)
        throw std::runtime_error(
            "AST JSON initialized-name Cray pointee shape property does not "
            "match its exact owned edge");
      if (shape != 0 &&
          (ast.node(shape).kind != "SgExprListExp" ||
           singleEdgeTarget(ast.node(shape), "parent") != record.id))
        throw std::runtime_error(
            "AST JSON initialized-name Cray pointee shape has invalid "
            "serialized ownership");
      if (shape != 0 && edgesFor(ast.node(shape), "expressions").empty())
        throw std::runtime_error(
            "AST JSON Cray pointee shape has no dimensions");
    }
    if (record.kind == "SgAuxiliaryDeclarationList") {
      std::unordered_set<uint64_t> declarations;
      for (const auto &edge : edgesFor(record, "declarations")) {
        if (!declarations.insert(edge.target).second ||
            singleEdgeTarget(ast.node(edge.target), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON SgAuxiliaryDeclarationList has malformed ownership");
      }
    }
    std::unordered_set<uint64_t> alignments;
    for (const auto &edge : edgesFor(record, "alignment_specifiers")) {
      const NodeRecord &child = ast.node(edge.target);
      if (!alignments.insert(edge.target).second ||
          child.kind != "SgAlignmentSpecifier" ||
          singleEdgeTarget(child, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON alignment has invalid serialized ownership");
    }
    if (const uint64_t bounds =
            singleEdgeTarget(record, "cuda_launch_bounds_expression")) {
      const NodeRecord &child = ast.node(bounds);
      if (!isFunctionDeclarationRecordKind(record.kind) ||
          singleEdgeTarget(child, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON CUDA launch bounds have invalid serialized ownership");
      if (child.kind == "SgExprListExp") {
        const auto arguments = edgesFor(child, "expressions");
        if (arguments.empty() || arguments.size() > 3)
          throw std::runtime_error(
              "AST JSON CUDA launch bounds have invalid operand count");
        std::unordered_set<uint64_t> operands;
        for (const auto &argument : arguments)
          if (!operands.insert(argument.target).second ||
              singleEdgeTarget(ast.node(argument.target), "parent") != bounds)
            throw std::runtime_error("AST JSON CUDA launch operands have "
                                     "invalid serialized ownership");
      }
    }
    if (record.kind == "SgLambdaCapture") {
      (void)record.properties.requiredBool("capture_by_reference");
      (void)record.properties.requiredBool("implicit");
      (void)record.properties.requiredBool("pack_expansion");
      (void)record.properties.requiredBool("is_init_capture");
      const uint64_t variable =
          requiredSingleEdgeTarget(record, "capture_variable");
      const uint64_t initializer =
          requiredSingleEdgeTarget(record, "capture_initializer");
      const uint64_t closure = singleEdgeTarget(record, "closure_variable");
      for (const uint64_t operand : {variable, initializer, closure}) {
        if (operand != 0 && !is_expression(ast.node(operand)))
          throw std::runtime_error(
              "AST JSON lambda capture operand is not an expression");
      }
      if (variable == initializer ||
          (closure != 0 && (closure == variable || closure == initializer)) ||
          requiredSingleEdgeTarget(ast.node(variable), "parent") != record.id ||
          requiredSingleEdgeTarget(ast.node(initializer), "parent") !=
              record.id ||
          (closure != 0 &&
           requiredSingleEdgeTarget(ast.node(closure), "parent") != record.id))
        throw std::runtime_error("AST JSON lambda capture requires distinct "
                                 "operands with an exact owner");
    }
    if (record.kind == "SgLambdaCaptureList") {
      std::unordered_set<uint64_t> captures;
      for (const auto &edge : edgesFor(record, "capture_list")) {
        const auto &capture = ast.node(edge.target);
        if (capture.kind != "SgLambdaCapture" ||
            singleEdgeTarget(capture, "parent") != record.id ||
            !captures.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON lambda capture list has invalid child ownership");
      }
    }
    if (record.kind == "SgLambdaExp") {
      const JsonValue &properties = record.properties;
      for (const char *field : {"is_mutable", "explicit_return_type",
                                "has_parameter_decl", "is_device"})
        (void)properties.requiredBool(field);
      const bool capture_default = properties.requiredBool("capture_default");
      const bool default_is_by_reference =
          properties.requiredBool("default_is_by_reference");
      if (!capture_default && default_is_by_reference)
        throw std::runtime_error(
            "AST JSON lambda has reference default without capture default");
      const auto &captures =
          ast.node(requiredSingleEdgeTarget(record, "lambda_capture_list"));
      const auto &closure =
          ast.node(requiredSingleEdgeTarget(record, "lambda_closure_class"));
      const auto &function =
          ast.node(requiredSingleEdgeTarget(record, "lambda_function"));
      if (captures.kind != "SgLambdaCaptureList" ||
          !isClassDeclarationRecordKind(closure.kind) ||
          !isFunctionDeclarationRecordKind(function.kind))
        throw std::runtime_error("AST JSON lambda children have invalid kinds");
      const auto &closure_owner =
          ast.node(requiredSingleEdgeTarget(closure, "parent"));
      const auto &function_owner =
          ast.node(requiredSingleEdgeTarget(function, "parent"));
      const uint64_t definition =
          requiredSingleEdgeTarget(closure, "definition");
      auto has_exact_entry = [&](const NodeRecord &owner,
                                 uint64_t declaration) {
        const auto &entries = edgesFor(owner, "declarations");
        return std::count_if(entries.begin(), entries.end(),
                             [&](const EdgeRecord &edge) {
                               return edge.target == declaration;
                             }) == 1;
      };
      if (requiredSingleEdgeTarget(captures, "parent") != record.id ||
          requiredClassType(closure.properties, "lambda closure") !=
              SgClassDeclaration::e_lambda_closure ||
          closure_owner.kind != "SgAuxiliaryDeclarationList" ||
          function_owner.kind != "SgAuxiliaryDeclarationList" ||
          requiredSingleEdgeTarget(closure_owner, "parent") !=
              requiredSingleEdgeTarget(closure, "scope") ||
          requiredSingleEdgeTarget(function_owner, "parent") != definition ||
          requiredSingleEdgeTarget(function, "scope") != definition ||
          !has_exact_entry(closure_owner, closure.id) ||
          !has_exact_entry(function_owner, function.id))
        throw std::runtime_error(
            "AST JSON lambda children have a different owner");
      const uint64_t recipe_id = singleEdgeTarget(closure, "lambda_expression");
      const auto &scope = ast.node(requiredSingleEdgeTarget(closure, "scope"));
      const uint64_t scope_id = singleEdgeTarget(record, "closure_scope");
      if (scope.kind != "SgDeclarationScope" ||
          singleEdgeTarget(scope, "semantic_outer_scope") == 0 ||
          (scope_id != 0 &&
           (scope_id != scope.id ||
            requiredSingleEdgeTarget(scope, "parent") != record.id ||
            recipe_id != record.id)) ||
          (scope_id == 0 &&
           (recipe_id == 0 || recipe_id == record.id ||
            requiredSingleEdgeTarget(scope, "parent") != recipe_id ||
            singleEdgeTarget(ast.node(recipe_id), "closure_scope") !=
                scope.id)))
        throw std::runtime_error(
            "AST JSON lambda has no exact declaration-scope owner");
      if (ast.node(recipe_id).kind != "SgLambdaExp" ||
          requiredSingleEdgeTarget(ast.node(recipe_id),
                                   "lambda_closure_class") != closure.id)
        throw std::runtime_error(
            "AST JSON closure recipe has a different identity");
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgClassDefinition" ||
        record.kind == "SgTemplateClassDefinition" ||
        record.kind == "SgTemplateInstantiationDefn") {
      const uint64_t owner_id = requiredSingleEdgeTarget(record, "parent");
      const NodeRecord &owner = ast.node(owner_id);
      if (!isClassDeclarationRecordKind(owner.kind) ||
          requiredSingleEdgeTarget(record, "declaration") != owner_id ||
          requiredSingleEdgeTarget(owner, "definition") != record.id ||
          (record.kind == "SgTemplateClassDefinition" &&
           owner.kind != "SgTemplateClassDeclaration") ||
          (record.kind == "SgTemplateInstantiationDefn" &&
           owner.kind != "SgTemplateInstantiationDecl")) {
        throw std::runtime_error(
            "AST JSON class definition has no exact declaration owner");
      }
    } else if (record.kind == "SgFunctionDefinition" ||
               record.kind == "SgTemplateFunctionDefinition") {
      const uint64_t owner_id = requiredSingleEdgeTarget(record, "parent");
      const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
      const uint64_t declaration_id = singleEdgeTarget(record, "declaration");
      const NodeRecord &owner = ast.node(owner_id);
      if (!isFunctionDeclarationRecordKind(owner.kind) ||
          requiredSingleEdgeTarget(owner, "definition") != record.id ||
          requiredSingleEdgeTarget(ast.node(body_id), "parent") != record.id ||
          ast.node(body_id).kind != "SgBasicBlock" ||
          (declaration_id != 0 && declaration_id != owner_id) ||
          (record.kind == "SgTemplateFunctionDefinition" &&
           owner.kind != "SgTemplateFunctionDeclaration")) {
        throw std::runtime_error(
            "AST JSON function definition has no exact declaration and "
            "body ownership");
      }
      const bool destructor =
          (owner.kind == "SgMemberFunctionDeclaration" ||
           owner.kind == "SgTemplateMemberFunctionDeclaration" ||
           owner.kind == "SgTemplateInstantiationMemberFunctionDecl") &&
          functionModifierFieldsFromJson(owner)
              .special[SgSpecialFunctionModifier::e_destructor];
      const bool dependent = destructor && record.properties.requiredBool(
                                               "destructor_epilogue_dependent");
      const uint64_t epilogue = singleEdgeTarget(record, "destructor_epilogue");
      if (dependent) {
        uint64_t context = requiredSingleEdgeTarget(owner, "scope");
        std::unordered_set<uint64_t> visited;
        bool template_context = false;
        while (context != 0) {
          if (!visited.insert(context).second)
            throw std::runtime_error(
                "AST JSON dependent destructor context is cyclic");
          const auto &scope = ast.node(context);
          if (scope.kind == "SgTemplateClassDeclaration" ||
              scope.kind == "SgTemplateFunctionDeclaration" ||
              scope.kind == "SgTemplateMemberFunctionDeclaration") {
            template_context = true;
            break;
          }
          context = isClassDeclarationRecordKind(scope.kind) ||
                            isFunctionDeclarationRecordKind(scope.kind)
                        ? singleEdgeTarget(scope, "scope")
                        : singleEdgeTarget(scope, "parent");
        }
        if (!template_context)
          throw std::runtime_error(
              "AST JSON dependent destructor has no template context");
      }
      if ((!destructor && record.properties.find(
                              "destructor_epilogue_dependent") != nullptr) ||
          destructor != (dependent || epilogue != 0) ||
          (dependent && epilogue != 0) ||
          (epilogue != 0 && (ast.node(epilogue).kind != "SgExprListExp" ||
                             requiredSingleEdgeTarget(ast.node(epilogue),
                                                      "parent") != record.id)))
        throw std::runtime_error(
            "AST JSON function definition has no exact destructor epilogue");
      if (epilogue != 0) {
        int previous_kind = SgCxxSubobjectDestruction::e_member;
        std::optional<unsigned int> previous_member;
        for (const auto &edge : edgesFor(ast.node(epilogue), "expressions")) {
          const auto &step = ast.node(edge.target);
          if (step.kind != "SgCxxSubobjectDestruction" ||
              requiredSingleEdgeTarget(step, "parent") != epilogue)
            throw std::runtime_error(
                "AST JSON destructor epilogue has a foreign child");
          const int kind = subobjectDestructionKindFromJson(step.properties);
          const uint64_t target = requiredSingleEdgeTarget(
              step, kind == SgCxxSubobjectDestruction::e_member
                        ? "member_target"
                        : "base_target");
          if (kind == SgCxxSubobjectDestruction::e_member) {
            const auto index = requiredOptionalUnsignedInteger(
                ast.node(target).properties, "cxx_field_index",
                "destructor member");
            if (!index || (previous_member && *index >= *previous_member))
              throw std::runtime_error("AST JSON member destruction order is "
                                       "not reverse field order");
            previous_member = index;
            const auto &scope =
                ast.node(requiredSingleEdgeTarget(ast.node(target), "scope"));
            if ((scope.kind != "SgClassDefinition" &&
                 scope.kind != "SgTemplateClassDefinition" &&
                 scope.kind != "SgTemplateInstantiationDefn") ||
                requiredClassType(
                    ast.node(requiredSingleEdgeTarget(scope, "declaration"))
                        .properties,
                    "destructor member owner") == SgClassDeclaration::e_union)
              throw std::runtime_error(
                  "AST JSON destructor member is not a non-union class field");
            const auto &variable =
                ast.node(requiredSingleEdgeTarget(ast.node(target), "parent"));
            if (variable.kind != "SgVariableDeclaration" ||
                requiredStorageModifier(
                    variable.properties, "declaration_storage_modifier",
                    "destructor member") == SgStorageModifier::e_static)
              throw std::runtime_error(
                  "AST JSON destructor member is not a nonstatic field");
          }
          if (kind < previous_kind ||
              (kind == SgCxxSubobjectDestruction::e_member &&
               requiredSingleEdgeTarget(ast.node(target), "scope") !=
                   requiredSingleEdgeTarget(owner, "scope")))
            throw std::runtime_error("AST JSON destructor epilogue has invalid "
                                     "target order or scope");
          previous_kind = kind;
        }
        auto dependencies = validateDestructorBaseTargets(
            ast, owner, ast.node(epilogue), nodes);
        auto members = destructorMemberRecords(ast, owner);
        if (auto inputs = destructorMemberInputs(members, nodes)) {
          SgExpressionPtrList steps;
          bool ready = true;
          for (const auto &edge : edgesFor(ast.node(epilogue), "expressions")) {
            if (nodes.count(edge.target) == 0) {
              ready = false;
              break;
            }
            steps.push_back(nodeByIdAs<SgExpression>(nodes, edge.target));
          }
          if (ready && !SgFunctionDefinition::has_complete_member_destruction(
                           steps, *inputs))
            throw std::runtime_error("AST JSON destructor epilogue omits a "
                                     "required member destructor");
        }
        if (nodes.count(record.id) == 0) {
          auto &prepared =
              construction_fields
                  .try_emplace(record.id,
                               RecordConstructionFields{
                                   validateConstructionFlags(record), {}})
                  .first->second;
          prepareRecordPublicationFields(record, prepared);
          prepared.destructor_base_declarations = std::move(dependencies);
          prepared.destructor_members = std::move(members);
        }
      }
    }
  }
  std::unordered_set<uint64_t> function_parameter_lists;
  std::unordered_set<uint64_t> detached_template_identities;
  auto require_opaque_binding = [&](const NodeRecord &use, uint64_t id) {
    const auto &binding = ast.node(id);
    const auto &list = ast.node(requiredSingleEdgeTarget(binding, "parent"));
    const auto &owner = ast.node(requiredSingleEdgeTarget(list, "parent"));
    const auto steps = edgesFor(list, "expressions");
    const auto definition =
        std::find_if(steps.begin(), steps.end(), [id](const EdgeRecord &edge) {
          return edge.target == id;
        });
    if (binding.kind != "SgOpaqueValueBinding" ||
        list.kind != "SgExprListExp" || owner.kind != "SgPseudoObjectExp" ||
        requiredSingleEdgeTarget(owner, "semantic_evaluations") != list.id ||
        definition == steps.end())
      throw std::runtime_error(
          "AST JSON opaque binding lacks an evaluation owner");
    std::unordered_set<uint64_t> visited;
    const NodeRecord *child = &use;
    while (const uint64_t parent_id = singleEdgeTarget(*child, "parent")) {
      if (!visited.insert(parent_id).second)
        throw std::runtime_error(
            "AST JSON opaque reference has cyclic ownership");
      const auto &parent = ast.node(parent_id);
      if (parent.id == list.id) {
        const auto evaluation = std::find_if(
            steps.begin(), steps.end(),
            [&](const EdgeRecord &edge) { return edge.target == child->id; });
        if (evaluation == steps.end() || evaluation <= definition)
          throw std::runtime_error(
              "AST JSON opaque reference precedes its binding");
        return;
      }
      if (parent.kind == "SgPseudoObjectExp") {
        const auto captures = edgesFor(parent, "captured_bindings");
        if (requiredSingleEdgeTarget(parent, "semantic_evaluations") !=
                child->id ||
            std::none_of(
                captures.begin(), captures.end(),
                [id](const EdgeRecord &edge) { return edge.target == id; }))
          throw std::runtime_error(
              "AST JSON opaque reference lacks an enclosing capture");
      }
      child = &parent;
    }
    throw std::runtime_error(
        "AST JSON opaque reference has no enclosing binding");
  };
  for (const NodeRecord &record : ast.nodes) {
    rejectRemovedQualifiedNameState(record.properties);
    std::unordered_set<uint64_t> owned_argument_ids;
    auto validate_argument_list =
        [&](const char *property,
            const char *edge_field) -> std::vector<EdgeRecord> {
      const JsonValue &list = record.properties.at(property);
      const auto edges = edgesFor(record, edge_field);
      if (list.kind != JsonValue::Kind::Array ||
          list.array.size() != edges.size()) {
        throw std::runtime_error("AST JSON " + record.kind +
                                 " argument property and edges disagree");
      }
      for (size_t index = 0; index < edges.size(); ++index) {
        const int64_t id = list.array[index].asInt();
        if (id <= 0 || static_cast<uint64_t>(id) != edges[index].target) {
          throw std::runtime_error("AST JSON " + record.kind +
                                   " argument property and edges disagree");
        }
        const NodeRecord &argument = ast.node(edges[index].target);
        if (argument.kind != "SgTemplateArgument") {
          throw std::runtime_error("AST JSON " + record.kind +
                                   " template argument has the wrong kind");
        }
        if (singleEdgeTarget(argument, "parent") != record.id) {
          throw std::runtime_error("AST JSON " + record.kind +
                                   " template argument has no reciprocal "
                                   "owner");
        }
        if (!owned_argument_ids.insert(argument.id).second) {
          throw std::runtime_error("AST JSON " + record.kind +
                                   " repeats one template argument across "
                                   "its surfaces");
        }
      }
      return edges;
    };
    if (record.kind == "SgTemplateTypedefDeclaration") {
      std::unordered_set<uint64_t> parameters;
      const auto edges = edgesFor(record, "templateParameters");
      for (size_t index = 0; index < edges.size(); ++index) {
        const auto &edge = edges[index];
        const NodeRecord &parameter = ast.node(edge.target);
        if (parameter.kind != "SgTemplateParameter" || edge.index != index ||
            singleEdgeTarget(parameter, "parent") != record.id ||
            !parameters.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON alias parameter has no unique exact header owner");
      }
    }
    if (nodes.count(record.id) != 0)
      (void)expressionQualificationFieldsFromJson(record.kind,
                                                  record.properties);
    if (record.kind == "SgFunctionParameterScope" &&
        (singleEdgeTarget(record, "construction_physical_output_owner") != 0 ||
         singleEdgeTarget(record, "construction_semantic_scope") != 0))
      throw std::runtime_error(
          "AST JSON function parameter scope retains an incomplete "
          "construction transaction");
    if (record.kind == "SgFunctionParameterScope" &&
        isFunctionDeclarationRecordKind(
            ast.node(requiredSingleEdgeTarget(record, "parent")).kind) &&
        singleEdgeTarget(record, "completed_semantic_scope") != 0)
      throw std::runtime_error(
          "AST JSON function parameter scope has an invalid completed "
          "semantic scope");
    if (record.kind == "SgInitializedName" &&
        singleEdgeTarget(record, "scope") == 0) {
      const NodeRecord &parent =
          ast.node(requiredSingleEdgeTarget(record, "parent"));
      if (parent.kind == "SgTemplateParameter") {
        const auto fields =
            templateParameterSourceFieldsFromJson(parent.properties);
        if (fields.kind != SgTemplateParameter::nontype_parameter ||
            static_cast<uint64_t>(fields.canonicalNonTypeParameter) !=
                parent.id ||
            singleEdgeTarget(parent, "parent") != 0 ||
            exactSerializedPointerTarget(parent, "initialized_name",
                                         "initializedName") != record.id ||
            singleEdgeTarget(record, "declptr") != 0 ||
            !parent.properties.at("type").exactlyEquals(
                record.properties.at("type")))
          throw std::runtime_error(
              "AST JSON scope-free template declarator has no exact "
              "canonical parameter owner");
      } else {
        const NodeRecord &function =
            ast.node(requiredSingleEdgeTarget(record, "declptr"));
        if (!isFunctionDeclarationRecordKind(function.kind) ||
            !function.properties.find(
                "external_function_parameter_scope_owner") ||
            parent.id != requiredSingleEdgeTarget(function, "parameterList"))
          throw std::runtime_error(
              "AST JSON initialized name has no exact construction-time scope");
      }
    }
    if (record.kind == "SgStatementFunctionStatement") {
      const auto &function =
          ast.node(requiredSingleEdgeTarget(record, "function"));
      const auto &expression =
          ast.node(requiredSingleEdgeTarget(record, "expression"));
      if (!isFunctionDeclarationRecordKind(function.kind) ||
          !is_expression(expression) ||
          requiredSingleEdgeTarget(function, "parent") != record.id ||
          requiredSingleEdgeTarget(expression, "parent") != record.id)
        throw std::runtime_error("AST JSON statement function has conflicting "
                                 "typed child ownership");
    }
    if (isFunctionDeclarationRecordKind(record.kind)) {
      (void)functionConstexprSpecifierFromJson(record.properties, record.kind);
      const auto modifiers = functionModifierFieldsFromJson(record);
      if (uint64_t clause_id =
              singleEdgeTarget(record, "trailingRequiresClause")) {
        const auto &clause = ast.node(clause_id);
        if (!is_expression(clause) ||
            requiredSingleEdgeTarget(clause, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON trailing constraint has invalid serialized ownership");
      }
      const uint64_t condition_id =
          singleEdgeTarget(record, "explicit_specifier_expression");
      if (condition_id != 0) {
        const bool member =
            record.kind == "SgMemberFunctionDeclaration" ||
            record.kind == "SgTemplateMemberFunctionDeclaration" ||
            record.kind == "SgTemplateInstantiationMemberFunctionDecl";
        if ((!member ||
             (!modifiers.special[SgSpecialFunctionModifier::e_constructor] &&
              !modifiers.special[SgSpecialFunctionModifier::e_conversion])) &&
            !record.properties.requiredBool("is_deduction_guide"))
          throw std::runtime_error("AST JSON explicit specifier has no typed "
                                   "constructor, conversion or guide owner");
        const auto &condition = ast.node(condition_id);
        if (!is_expression(condition) ||
            requiredSingleEdgeTarget(condition, "parent") != record.id)
          throw std::runtime_error("AST JSON explicit condition has invalid "
                                   "serialized expression ownership");
      }
      if (record.properties.find(
              "template_instantiation_pattern_is_unpublished"))
        throw std::runtime_error(
            "AST JSON function retains removed instantiation-pattern state");
      if (uint64_t pattern_id =
              singleEdgeTarget(record, "template_instantiation_pattern")) {
        const NodeRecord &pattern = ast.node(pattern_id);
        if (pattern_id == record.id ||
            record.kind == "SgTemplateInstantiationFunctionDecl" ||
            record.kind == "SgTemplateInstantiationMemberFunctionDecl" ||
            !isFunctionDeclarationRecordKind(pattern.kind) ||
            singleEdgeTarget(pattern, "template_instantiation_pattern") != 0)
          throw std::runtime_error("AST JSON function has an invalid "
                                   "construction-time instantiation "
                                   "pattern");
      }
      if (record.properties.find("external_function_parameter_scope") ||
          record.properties.find("external_function_parameter_scope_source"))
        throw std::runtime_error(
            "AST JSON function retains removed external parameter-scope state");
      if (record.properties.find("external_function_parameter_scope_owner")) {
        const auto owner = requiredEnum<ExternalFunctionParameterScopeOwner>(
            record.properties, "external_function_parameter_scope_owner",
            record.kind,
            {ExternalFunctionParameterScopeOwner::FirstNondefining,
             ExternalFunctionParameterScopeOwner::Defining});
        const auto &peer = record.properties.at(
            owner == ExternalFunctionParameterScopeOwner::FirstNondefining
                ? "external_first_nondefining_declaration"
                : "external_defining_declaration");
        if (singleEdgeTarget(record, "functionParameterScope") != 0 ||
            !peer.requiredBool("present") ||
            !peer.at("external_function")
                 .at("function_parameter_scope")
                 .requiredBool("present"))
          throw std::runtime_error(
              "AST JSON function has no unique external parameter-scope owner");
      }
      const uint64_t list_id =
          requiredSingleEdgeTarget(record, "parameterList");
      const NodeRecord &list_record = ast.node(list_id);
      if (!function_parameter_lists.insert(list_id).second) {
        throw std::runtime_error(
            "AST JSON function parameter list has no unique typed owner");
      }
      const auto semantic_parameters = edgesFor(list_record, "args");
      for (uint64_t parameter_list_id :
           {list_id, singleEdgeTarget(record, "parameterList_syntax")}) {
        if (parameter_list_id == 0)
          continue;
        const NodeRecord &parameters = ast.node(parameter_list_id);
        if (parameters.kind != "SgFunctionParameterList" ||
            singleEdgeTarget(parameters, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON function parameter list has no unique typed owner");
        const auto parameter_edges = edgesFor(parameters, "args");
        if (parameter_edges.size() != semantic_parameters.size())
          throw std::runtime_error(
              "AST JSON function has mismatched semantic and source "
              "parameter counts");
        for (const EdgeRecord &edge : parameter_edges) {
          const NodeRecord &parameter = ast.node(edge.target);
          if (parameter.kind != "SgInitializedName" ||
              singleEdgeTarget(parameter, "parent") != parameter_list_id ||
              singleEdgeTarget(parameter, "declptr") != record.id)
            throw std::runtime_error(
                "AST JSON function parameter has no exact declaration owner");
        }
      }
      if (uint64_t scope_id =
              singleEdgeTarget(record, "functionParameterScope")) {
        const NodeRecord &scope = ast.node(scope_id);
        const uint64_t owner_id = requiredSingleEdgeTarget(scope, "parent");
        if (scope.kind != "SgFunctionParameterScope" ||
            !isFunctionDeclarationRecordKind(ast.node(owner_id).kind) ||
            requiredSingleEdgeTarget(ast.node(owner_id),
                                     "functionParameterScope") != scope_id ||
            (owner_id != record.id &&
             owner_id !=
                 singleEdgeTarget(record, "firstNondefiningDeclaration") &&
             owner_id != singleEdgeTarget(record, "definingDeclaration")))
          throw std::runtime_error("AST JSON function parameter scope has "
                                   "conflicting typed ownership");
        for (const EdgeRecord &edge : semantic_parameters) {
          const NodeRecord &parameter = ast.node(edge.target);
          if (singleEdgeTarget(parameter, "scope") != scope_id)
            throw std::runtime_error(
                "AST JSON function parameter has no exact construction-time "
                "semantic scope");
        }
      }
    }
    if (record.kind == "SgTemplateArgument") {
      if (record.properties.find("initialized_name") != nullptr ||
          !edgesFor(record, "initializedName").empty())
        throw std::runtime_error("AST JSON retains removed initialized-name "
                                 "template argument payload");
      auto argument_fields = templateArgumentFieldsFromRecord(record);
      auto &fields = construction_fields
                         .emplace(record.id,
                                  RecordConstructionFields{
                                      validateConstructionFlags(record), {}})
                         .first->second;
      fields.template_argument = std::move(argument_fields);
      const auto argument_type = fields.template_argument->kind;
      const bool has_type =
          record.properties.at("type").requiredBool("present");
      const JsonValue &expression = record.properties.at("expression");
      const int64_t expression_id = expression.requiredInt("node");
      const uint64_t template_id = exactSerializedPointerTarget(
          record, "template_declaration", "templateDeclaration");
      if (expression_id < 0 || expression.find("owned_kind") != nullptr)
        throw std::runtime_error(
            "AST JSON template argument expression must reference an exact "
            "collected node");
      if (singleEdgeTarget(record, "expression") !=
          static_cast<uint64_t>(expression_id))
        throw std::runtime_error(
            "AST JSON template argument expression disagrees with its typed "
            "edge");
      if ((argument_type == SgTemplateArgument::type_argument && !has_type) ||
          (argument_type == SgTemplateArgument::nontype_argument &&
           expression_id == 0) ||
          (argument_type == SgTemplateArgument::template_template_argument &&
           template_id == 0))
        throw std::runtime_error(
            "AST JSON template argument lacks its typed semantic operand");
      if ((argument_type == SgTemplateArgument::type_argument &&
           (expression_id != 0 || template_id != 0)) ||
          (argument_type == SgTemplateArgument::nontype_argument &&
           (!has_type || template_id != 0)) ||
          (argument_type == SgTemplateArgument::template_template_argument &&
           (has_type || expression_id != 0)) ||
          (argument_type ==
               SgTemplateArgument::start_of_pack_expansion_argument &&
           (has_type || expression_id != 0 || template_id != 0)))
        throw std::runtime_error(
            "AST JSON template argument has incompatible semantic payloads");
      if (expression_id > 0 &&
          (!is_expression(ast.node(static_cast<uint64_t>(expression_id))) ||
           requiredSingleEdgeTarget(
               ast.node(static_cast<uint64_t>(expression_id)), "parent") !=
               record.id))
        throw std::runtime_error(
            "AST JSON template argument expression has no exact owner");
      if (template_id > 0) {
        const NodeRecord &declaration = ast.node(template_id);
        if (declaration.kind != "SgTemplateClassDeclaration" &&
            declaration.kind != "SgTemplateTypedefDeclaration" &&
            declaration.kind != "SgTemplateDeclaration" &&
            declaration.kind != "SgNonrealDecl")
          throw std::runtime_error(
              "AST JSON template argument declaration is not a "
              "template-bearing AST category");
      }
    } else if (record.kind == "SgTemplateParameter") {
      const auto fields =
          templateParameterSourceFieldsFromJson(record.properties);
      const uint64_t template_id = exactSerializedPointerTarget(
          record, "template_declaration", "templateDeclaration");
      if (fields.kind == SgTemplateParameter::template_parameter) {
        if (template_id == 0 ||
            ast.node(template_id).kind != "SgTemplateDeclaration")
          throw std::runtime_error(
              "AST JSON template-template parameter has no exact template "
              "declaration");
        const NodeRecord &identity = ast.node(template_id);
        if (singleEdgeTarget(identity, "scope") == 0) {
          const NodeRecord &parameter_scope = ast.node(
              requiredSingleEdgeTarget(identity, "nonreal_decl_scope"));
          if (singleEdgeTarget(record, "parent") != 0 ||
              singleEdgeTarget(identity, "parent") != 0 ||
              requiredSingleEdgeTarget(
                  identity, "firstNondefiningDeclaration") != identity.id ||
              singleEdgeTarget(identity, "definingDeclaration") != 0 ||
              identity.properties.requiredInt("template_kind") !=
                  SgTemplateDeclaration::e_template_class ||
              parameter_scope.kind != "SgDeclarationScope" ||
              requiredSingleEdgeTarget(parameter_scope, "parent") !=
                  identity.id)
            throw std::runtime_error(
                "AST JSON detached template identity has conflicting "
                "parameter ownership");
          detached_template_identities.insert(identity.id);
        }
      } else if (template_id != 0) {
        throw std::runtime_error(
            "AST JSON non-template parameter owns a template declaration");
      }
      const uint64_t expression_id =
          record.properties.at("default_expression_parameter")
              .requiredNonnegativeNodeId("node");
      if (singleEdgeTarget(record, "defaultExpressionParameter") !=
          expression_id)
        throw std::runtime_error("AST JSON template parameter has conflicting "
                                 "default_expression_parameter");
      const uint64_t argument_id = exactSerializedPointerTarget(
          record, "default_template_argument", "defaultTemplateArgument");
      const uint64_t source_id = exactSerializedPointerTarget(
          record, "source_spelled_template_declaration",
          "sourceSpelledTemplateDeclaration");
      if (source_id != 0 &&
          (source_id == template_id ||
           ast.node(source_id).kind != "SgTemplateDeclaration" ||
           singleEdgeTarget(ast.node(source_id), "scope") == 0))
        throw std::runtime_error(
            "AST JSON template parameter has a malformed source-spelled "
            "template declaration");
      for (uint64_t child : {expression_id, argument_id, source_id})
        if (child != 0 &&
            requiredSingleEdgeTarget(ast.node(child), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON template parameter child has a foreign owner");
    } else if (record.kind == "SgEnumDeclaration") {
      const uint64_t first_id =
          requiredSingleEdgeTarget(record, "firstNondefiningDeclaration");
      const uint64_t owner_id =
          record.properties.requiredNonnegativeNodeId("type_owner");
      if (owner_id == 0)
        throw std::runtime_error("AST JSON enum has no typed type owner");
      if (uint64_t parent = singleEdgeTarget(record, "parent");
          parent != 0 &&
          ast.node(parent).kind == "SgAuxiliaryDeclarationList" &&
          requiredSingleEdgeTarget(ast.node(parent), "parent") !=
              requiredSingleEdgeTarget(record, "scope"))
        throw std::runtime_error(
            "AST JSON enum auxiliary owner has a different semantic scope");
      const NodeRecord &first = ast.node(first_id);
      const NodeRecord &owner = ast.node(owner_id);
      if (first.kind != "SgEnumDeclaration" ||
          owner.kind != "SgEnumDeclaration" ||
          requiredSingleEdgeTarget(first, "firstNondefiningDeclaration") !=
              first_id ||
          requiredSingleEdgeTarget(owner, "firstNondefiningDeclaration") !=
              owner_id ||
          first.properties.requiredNonnegativeNodeId("type_owner") !=
              owner_id ||
          owner.properties.requiredNonnegativeNodeId("type_owner") != owner_id)
        throw std::runtime_error(
            "AST JSON enum declaration has contradictory typed type ownership");
      if (uint64_t label_id = singleEdgeTarget(record, "end_numeric_label")) {
        const NodeRecord &label = ast.node(label_id);
        if (label.kind != "SgLabelRefExp" ||
            singleEdgeTarget(label, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON ENUM end label has no exact typed owner");
      }
    } else if (record.kind == "SgFunctionParameterTypeList") {
      const uint64_t scope_id =
          requiredSingleEdgeTarget(record, "source_parameter_scope");
      const uint64_t list_id =
          requiredSingleEdgeTarget(record, "source_parameters");
      const NodeRecord &scope = ast.node(scope_id);
      const NodeRecord &list = ast.node(list_id);
      if (singleEdgeTarget(record, "parent") != 0 ||
          scope.kind != "SgFunctionParameterScope" ||
          list.kind != "SgFunctionParameterList" ||
          requiredSingleEdgeTarget(scope, "parent") != record.id ||
          requiredSingleEdgeTarget(list, "parent") != record.id ||
          requiredSingleEdgeTarget(scope, "completed_semantic_scope") ==
              scope_id)
        throw std::runtime_error(
            "AST JSON prototype lacks exact reciprocal parameter ownership");
      const NodeRecord &enclosing =
          ast.node(requiredSingleEdgeTarget(scope, "completed_semantic_scope"));
      if (!kindInVariants(enclosing.kind, scope_variants))
        throw std::runtime_error(
            "AST JSON prototype semantic owner is not a scope");
      std::unordered_set<uint64_t> names;
      const auto parameters = edgesFor(list, "args");
      for (size_t index = 0; index < parameters.size(); ++index) {
        const auto &edge = parameters[index];
        const NodeRecord &parameter = ast.node(edge.target);
        if (edge.index != index || parameter.kind != "SgInitializedName" ||
            !names.insert(edge.target).second ||
            requiredSingleEdgeTarget(parameter, "parent") != list_id ||
            requiredSingleEdgeTarget(parameter, "scope") != scope_id ||
            singleEdgeTarget(parameter, "declptr") != 0)
          throw std::runtime_error(
              "AST JSON prototype parameter lacks its exact list and scope");
      }
    } else if (record.kind == "SgFunctionParameterList") {
      const JsonValue *external_first =
          record.properties.find("external_first_nondefining_declaration");
      const JsonValue *external_defining =
          record.properties.find("external_defining_declaration");
      if (requiredSingleEdgeTarget(record, "firstNondefiningDeclaration") !=
              record.id ||
          singleEdgeTarget(record, "definingDeclaration") != 0 ||
          (external_first != nullptr &&
           external_first->requiredBool("present")) ||
          (external_defining != nullptr &&
           external_defining->requiredBool("present"))) {
        throw std::runtime_error(
            "AST JSON function parameter list requires its constructor-owned "
            "self declaration identity");
      }
    } else if (record.kind == "SgRequiresExpr") {
      const uint64_t list_id = singleEdgeTarget(record, "local_parameter_list");
      const uint64_t syntax_id =
          singleEdgeTarget(record, "local_parameter_list_syntax");
      const uint64_t scope_id =
          singleEdgeTarget(record, "local_parameter_scope");
      const uint64_t requirements_id =
          requiredSingleEdgeTarget(record, "requirements");
      const NodeRecord &requirements = ast.node(requirements_id);
      if ((list_id == 0) != (scope_id == 0) ||
          requirements.kind != "SgExprListExp" ||
          edgesFor(requirements, "expressions").empty() ||
          singleEdgeTarget(requirements, "parent") != record.id ||
          (list_id != 0 &&
           (ast.node(list_id).kind != "SgFunctionParameterList" ||
            ast.node(scope_id).kind != "SgFunctionParameterScope" ||
            singleEdgeTarget(ast.node(list_id), "parent") != record.id ||
            singleEdgeTarget(ast.node(scope_id), "parent") != record.id))) {
        throw std::runtime_error("AST JSON requires-expression children lack "
                                 "exact reciprocal ownership");
      }
      if (scope_id != 0) {
        const uint64_t enclosing_id = requiredSingleEdgeTarget(
            ast.node(scope_id), "completed_semantic_scope");
        const NodeRecord &enclosing = ast.node(enclosing_id);
        if (enclosing_id == scope_id ||
            !kindInVariants(enclosing.kind, scope_variants))
          throw std::runtime_error(
              "AST JSON requires-expression parameter scope has no exact "
              "semantic owner");
      }
      if (syntax_id != 0) {
        const NodeRecord &syntax = ast.node(syntax_id);
        if (list_id == 0 || syntax_id == list_id ||
            syntax.kind != "SgFunctionParameterList" ||
            singleEdgeTarget(syntax, "parent") != record.id ||
            edgesFor(syntax, "args").size() !=
                edgesFor(ast.node(list_id), "args").size())
          throw std::runtime_error("AST JSON requires-expression written "
                                   "parameters lack exact ownership");
        const auto written = edgesFor(syntax, "args");
        const auto semantic = edgesFor(ast.node(list_id), "args");
        for (size_t index = 0; index < written.size(); ++index) {
          const auto &source = ast.node(written[index].target);
          const auto &object = ast.node(semantic[index].target);
          if (source.kind != "SgInitializedName" ||
              object.kind != "SgInitializedName" || source.id == object.id ||
              singleEdgeTarget(source, "parent") != syntax_id ||
              singleEdgeTarget(source, "scope") != scope_id ||
              source.properties.requiredString("name") !=
                  object.properties.requiredString("name") ||
              source.properties.requiredBool("is_parameter_pack") !=
                  object.properties.requiredBool("is_parameter_pack") ||
              singleEdgeTarget(source, "initptr") != 0)
            throw std::runtime_error(
                "AST JSON requires-expression written parameter differs from "
                "its lookup object");
        }
      }
      std::unordered_set<uint64_t> children;
      for (const EdgeRecord &edge : edgesFor(requirements, "expressions")) {
        const NodeRecord &child = ast.node(edge.target);
        if (!children.insert(edge.target).second ||
            singleEdgeTarget(child, "parent") != requirements_id ||
            (child.kind != "SgSimpleRequirement" &&
             child.kind != "SgTypeRequirement" &&
             child.kind != "SgCompoundRequirement" &&
             child.kind != "SgNestedRequirement" &&
             child.kind != "SgRequirementSubstitutionFailure")) {
          throw std::runtime_error("AST JSON requires-expression list has an "
                                   "invalid owned requirement");
        }
      }
    } else if (record.kind == "SgBaseClass" ||
               record.kind == "SgExpBaseClass" ||
               record.kind == "SgNonrealBaseClass") {
      const JsonValue &source =
          record.properties.at("fortran_source_derived_type_symbol");
      const std::string source_kind =
          source.kind != JsonValue::Kind::Null
              ? source.at("symbol").requiredString("symbol_kind")
              : std::string();
      if (source.kind != JsonValue::Kind::Null &&
          (record.kind != "SgBaseClass" ||
           (source_kind != "SgClassSymbol" && source_kind != "SgAliasSymbol")))
        throw std::runtime_error(
            "AST JSON Fortran base has no exact class-symbol binding");
      const uint64_t class_id =
          exactSerializedPointerTarget(record, "base_class", "base_class");
      if (class_id != 0 &&
          ast.node(class_id).kind == "SgDerivedTypeStatement" &&
          source.kind == JsonValue::Kind::Null)
        throw std::runtime_error(
            "AST JSON Fortran base has no exact class-symbol binding");
      if (record.kind == "SgNonrealBaseClass")
        (void)exactSerializedPointerTarget(record, "base_class_nonreal",
                                           "base_class_nonreal");
    } else if (record.kind == "SgUsingDirectiveStatement") {
      const uint64_t namespace_id = exactSerializedPointerTarget(
          record, "namespace_declaration", "namespaceDeclaration");
      if (namespace_id == 0 ||
          ast.node(namespace_id).kind != "SgNamespaceDeclarationStatement") {
        throw std::runtime_error(
            "AST JSON using directive has no exact namespace declaration");
      }
    } else if (record.kind == "SgUsingDeclarationStatement") {
      (void)record.properties.requiredString("source_terminal_name");
      (void)record.properties.requiredBool("is_inheriting_constructor");
      const uint64_t declaration_id =
          exactSerializedPointerTarget(record, "declaration", "declaration");
      const uint64_t name_id = exactSerializedPointerTarget(
          record, "initialized_name", "initializedName");
      if ((declaration_id == 0) == (name_id == 0) ||
          (name_id != 0 && ast.node(name_id).kind != "SgInitializedName")) {
        throw std::runtime_error(
            "AST JSON using declaration has no exact typed target");
      }
      if (name_id != 0) {
        const NodeRecord &name = ast.node(name_id);
        const uint64_t parent = singleEdgeTarget(name, "parent");
        if (parent == 0 ||
            (parent == record.id && singleEdgeTarget(name, "declptr") != 0))
          throw std::runtime_error(
              "AST JSON using target has conflicting typed ownership");
      }
      std::unordered_set<uint64_t> seen{record.id};
      for (uint64_t target = declaration_id; target != 0;) {
        const NodeRecord &target_record = ast.node(target);
        if (target_record.kind != "SgUsingDeclarationStatement")
          break;
        if (!seen.insert(target).second)
          throw std::runtime_error(
              "AST JSON using declaration has a cyclic typed target");
        target = exactSerializedPointerTarget(target_record, "declaration",
                                              "declaration");
      }
    } else if (record.kind == "SgTemplateInstantiationDirectiveStatement") {
      (void)record.properties.requiredBool("do_not_instantiate");
      const uint64_t target = requiredSingleEdgeTarget(record, "declaration");
      const NodeRecord &child = ast.node(target);
      if (target == record.id ||
          child.kind == "SgTemplateInstantiationDirectiveStatement" ||
          requiredSingleEdgeTarget(child, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON template instantiation directive has no exact owned "
            "declaration");
    } else if (kindInVariants(record.kind, image_control_variants)) {
      using Kind = SgImageControlStatement::image_control_statement_enum;
      Kind kind;
      if (record.kind == "SgSyncAllStatement")
        kind = SgImageControlStatement::e_sync_all;
      else if (record.kind == "SgSyncImagesStatement")
        kind = SgImageControlStatement::e_sync_images;
      else if (record.kind == "SgSyncMemoryStatement")
        kind = SgImageControlStatement::e_sync_memory;
      else if (record.kind == "SgSyncTeamStatement")
        kind = SgImageControlStatement::e_sync_team;
      else if (record.kind == "SgFormTeamStatement")
        kind = SgImageControlStatement::e_form_team;
      else if (record.kind == "SgLockStatement")
        kind = SgImageControlStatement::e_lock;
      else if (record.kind == "SgUnlockStatement")
        kind = SgImageControlStatement::e_unlock;
      else
        throw std::runtime_error(
            "AST JSON unsupported image-control statement " + record.kind);
      (void)requiredEnum<SgImageControlStatement::image_control_statement_enum>(
          record.properties, "image_control_statement", record.kind.c_str(),
          {kind});
      if (kind == SgImageControlStatement::e_form_team &&
          singleEdgeTarget(record, "acquired_lock") != 0)
        throw std::runtime_error(
            "AST JSON FORM TEAM cannot have ACQUIRED_LOCK");
      std::unordered_set<uint64_t> children;
      auto check_child = [&](const char *field, bool allowed, bool required) {
        const uint64_t target = required
                                    ? requiredSingleEdgeTarget(record, field)
                                    : singleEdgeTarget(record, field);
        if (target == 0)
          return;
        if (!allowed)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " has an unexpected " + field + " operand");
        if (target == record.id || !children.insert(target).second ||
            !is_expression(ast.node(target)) ||
            requiredSingleEdgeTarget(ast.node(target), "parent") != record.id)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " has conflicting operand ownership");
      };
      check_child("stat", true, false);
      check_child("err_msg", true, false);
      check_child("acquired_lock", kind == SgImageControlStatement::e_lock,
                  false);
      const bool form_team = kind == SgImageControlStatement::e_form_team;
      check_child("team_number", form_team, form_team);
      check_child("team_variable", form_team, form_team);
      check_child("new_index", form_team, false);
      const bool lock = kind == SgImageControlStatement::e_lock ||
                        kind == SgImageControlStatement::e_unlock;
      check_child("lock_variable", lock, lock);
      const bool sync_team = kind == SgImageControlStatement::e_sync_team;
      check_child("team_value", sync_team, sync_team);
      const bool sync_images = kind == SgImageControlStatement::e_sync_images;
      check_child("image_set", sync_images, false);
      if (sync_images) {
        const uint64_t image_set = singleEdgeTarget(record, "image_set");
        if (record.properties.requiredBool("all_images") != (image_set == 0) ||
            (image_set != 0 && ast.node(image_set).kind == "SgNullExpression"))
          throw std::runtime_error(
              "AST JSON SYNC IMAGES has inconsistent image-set operands");
      }
    } else if (record.kind == "SgNamespaceDeclarationStatement") {
      const uint64_t definition_id =
          requiredSingleEdgeTarget(record, "definition");
      const NodeRecord &definition = ast.node(definition_id);
      if (definition.kind != "SgNamespaceDefinitionStatement" ||
          requiredSingleEdgeTarget(definition, "namespaceDeclaration") !=
              record.id ||
          requiredSingleEdgeTarget(definition, "parent") != record.id) {
        throw std::runtime_error(
            "AST JSON namespace declaration/definition identity is not "
            "exact and bidirectional");
      }
    } else if (record.kind == "SgNamespaceDefinitionStatement") {
      const uint64_t declaration_id =
          requiredSingleEdgeTarget(record, "namespaceDeclaration");
      const NodeRecord &declaration = ast.node(declaration_id);
      if (declaration.kind != "SgNamespaceDeclarationStatement" ||
          requiredSingleEdgeTarget(declaration, "definition") != record.id ||
          requiredSingleEdgeTarget(record, "parent") != declaration_id) {
        throw std::runtime_error(
            "AST JSON namespace definition has no exact declaration owner");
      }
      const uint64_t global_id =
          requiredSingleEdgeTarget(record, "global_definition");
      if (ast.node(global_id).kind != "SgNamespaceDefinitionStatement")
        throw std::runtime_error(
            "AST JSON namespace definition has no typed global definition");
      const uint64_t previous_id =
          singleEdgeTarget(record, "previousNamespaceDefinition");
      const uint64_t next_id =
          singleEdgeTarget(record, "nextNamespaceDefinition");
      if ((previous_id != 0 &&
           (ast.node(previous_id).kind != "SgNamespaceDefinitionStatement" ||
            singleEdgeTarget(ast.node(previous_id),
                             "nextNamespaceDefinition") != record.id)) ||
          (next_id != 0 &&
           (ast.node(next_id).kind != "SgNamespaceDefinitionStatement" ||
            singleEdgeTarget(ast.node(next_id),
                             "previousNamespaceDefinition") != record.id))) {
        throw std::runtime_error(
            "AST JSON namespace definition chain is not reciprocal");
      }
      std::unordered_set<uint64_t> seen;
      uint64_t root_id = 0;
      for (uint64_t current_id = record.id; current_id != 0;
           current_id = singleEdgeTarget(ast.node(current_id),
                                         "previousNamespaceDefinition")) {
        if (!seen.insert(current_id).second) {
          throw std::runtime_error(
              "AST JSON namespace definition chain cycles");
        }
        const NodeRecord &current = ast.node(current_id);
        if (current.kind != "SgNamespaceDefinitionStatement" ||
            requiredSingleEdgeTarget(current, "global_definition") !=
                global_id) {
          throw std::runtime_error(
              "AST JSON namespace definition chain has a different root");
        }
        root_id = current_id;
      }
      if (root_id != global_id) {
        throw std::runtime_error(
            "AST JSON namespace definition chain misses its canonical root");
      }
    } else if (record.kind == "SgTemplateVariableDeclaration") {
      const auto semantic_arguments = validate_argument_list(
          "semantic_template_arguments", "semanticTemplateArguments");
      for (const char *field :
           {"templateSpecializationArguments", "deducedTemplateArguments"}) {
        for (const EdgeRecord &edge : edgesFor(record, field)) {
          if (!owned_argument_ids.insert(edge.target).second)
            throw std::runtime_error(
                "AST JSON variable template repeats one argument across "
                "its surfaces");
        }
      }
      const uint64_t selected =
          singleEdgeTarget(record, "specializedTemplateDeclaration");
      const int64_t role = record.properties.requiredInt("specialization");
      if (selected == 0) {
        if (role != SgDeclarationStatement::e_no_specialization ||
            !edgesFor(record, "templateSpecializationArguments").empty() ||
            !semantic_arguments.empty() ||
            !edgesFor(record, "deducedTemplateArguments").empty())
          throw std::runtime_error(
              "AST JSON specialized variable template has no typed target");
      } else {
        const NodeRecord &target = ast.node(selected);
        if (target.kind != "SgTemplateVariableDeclaration")
          throw std::runtime_error("AST JSON variable template target has the "
                                   "wrong declaration kind");
        const int64_t target_role =
            target.properties.requiredInt("specialization");
        const uint64_t target_primary =
            singleEdgeTarget(target, "specializedTemplateDeclaration");
        if ((target_role != SgDeclarationStatement::e_no_specialization &&
             target_role != SgDeclarationStatement::e_partial_specialization) ||
            (target_role == SgDeclarationStatement::e_no_specialization &&
             target_primary != 0) ||
            (target_role == SgDeclarationStatement::e_partial_specialization &&
             target_primary == 0) ||
            (role == SgDeclarationStatement::e_partial_specialization &&
             target_primary != 0))
          throw std::runtime_error(
              "AST JSON variable template has an invalid typed primary or "
              "partial declaration");
      }
    } else if (record.kind == "SgTemplateClassDeclaration") {
      std::unordered_set<uint64_t> patterns{record.id};
      for (uint64_t pattern =
               singleEdgeTarget(record, "instantiatedFromMemberTemplate");
           pattern != 0;
           pattern = singleEdgeTarget(ast.node(pattern),
                                      "instantiatedFromMemberTemplate")) {
        const NodeRecord &target = ast.node(pattern);
        if (!patterns.insert(pattern).second ||
            target.kind != "SgTemplateClassDeclaration" ||
            singleEdgeTarget(target, "firstNondefiningDeclaration") != pattern)
          throw std::runtime_error(
              "AST JSON member template has no acyclic canonical pattern");
      }
      const uint64_t primary =
          singleEdgeTarget(record, "specializedTemplateDeclaration");
      const bool partial = record.properties.at("specialization").asInt() ==
                           SgDeclarationStatement::e_partial_specialization;
      if (partial != (primary != 0))
        throw std::runtime_error(
            "AST JSON declaration specialization family is malformed: "
            "template class primary link disagrees with specialization kind");
      if (primary != 0 &&
          (ast.node(primary).kind != "SgTemplateClassDeclaration" ||
           singleEdgeTarget(ast.node(primary),
                            "specializedTemplateDeclaration") != 0)) {
        throw std::runtime_error(
            "AST JSON partial template class requires its exact primary "
            "declaration");
      }
    } else if (record.kind == "SgNonrealDecl" ||
               record.kind == "SgTemplateInstantiationDecl" ||
               record.kind == "SgTemplateInstantiationFunctionDecl" ||
               record.kind == "SgTemplateInstantiationMemberFunctionDecl" ||
               record.kind == "SgTemplateInstantiationTypedefDeclaration") {
      if (record.kind == "SgNonrealDecl") {
        const auto role =
            requiredEnum<SgNonrealDecl::nonreal_template_role_enum>(
                record.properties, "nonreal_template_role", "SgNonrealDecl",
                {SgNonrealDecl::e_nonreal_template_none,
                 SgNonrealDecl::e_nonreal_template_declaration,
                 SgNonrealDecl::e_nonreal_template_id});
        if (role != SgNonrealDecl::e_nonreal_template_id &&
            !edgesFor(record, "tpl_args").empty()) {
          throw std::runtime_error(
              "AST JSON non-template SgNonrealDecl owns template arguments");
        }
        validate_argument_list("tpl_args", "tpl_args");
        const uint64_t template_id =
            singleEdgeTarget(record, "templateDeclaration");
        if (role == SgNonrealDecl::e_nonreal_template_id && template_id == 0)
          throw std::runtime_error(
              "AST JSON SgNonrealDecl requires its exact typed template target "
              "before construction");
        if (template_id != 0) {
          const NodeRecord &template_record = ast.node(template_id);
          if (template_record.id == record.id) {
            throw std::runtime_error("AST JSON SgNonrealDecl cannot be its own "
                                     "template declaration");
          }
        }
      } else {
        const auto written_arguments =
            validate_argument_list("template_arguments", "templateArguments");
        if (record.kind == "SgTemplateInstantiationDecl") {
          validate_argument_list("semantic_template_arguments",
                                 "semanticTemplateArguments");
          if (record.properties.requiredBool(
                  "source_spells_injected_class_name") &&
              !written_arguments.empty()) {
            throw std::runtime_error(
                "AST JSON injected class name cannot have written template "
                "arguments");
          }
        }
        validate_argument_list("deduced_template_arguments",
                               "deducedTemplateArguments");
        if (record.kind == "SgTemplateInstantiationDecl" &&
            ast.node(requiredSingleEdgeTarget(record, "templateDeclaration"))
                    .kind != "SgTemplateClassDeclaration") {
          throw std::runtime_error(
              "AST JSON class instantiation requires an exact template "
              "declaration");
        }
        if (record.kind == "SgTemplateInstantiationDecl" &&
            ast.node(requiredSingleEdgeTarget(record,
                                              "specializedTemplateDeclaration"))
                    .kind != "SgTemplateClassDeclaration") {
          throw std::runtime_error(
              "AST JSON class instantiation requires its exact selected "
              "template declaration");
        }
        if (record.kind == "SgTemplateInstantiationFunctionDecl") {
          const uint64_t primary =
              singleEdgeTarget(record, "templateDeclaration");
          const auto candidates =
              edgesFor(record, "dependentTemplateCandidates");
          if ((primary != 0 && !candidates.empty()) ||
              (primary == 0 && candidates.size() < 2) ||
              (primary != 0 &&
               ast.node(primary).kind != "SgTemplateFunctionDeclaration")) {
            throw std::runtime_error(
                "AST JSON function instantiation requires one exact primary "
                "or one unresolved candidate set");
          }
          std::unordered_set<uint64_t> unique_candidates;
          for (const EdgeRecord &edge : candidates) {
            const NodeRecord &candidate = ast.node(edge.target);
            if (candidate.kind != "SgTemplateFunctionDeclaration" ||
                singleEdgeTarget(candidate, "firstNondefiningDeclaration") !=
                    candidate.id ||
                singleEdgeTarget(candidate, "scope") !=
                    singleEdgeTarget(record, "scope") ||
                candidate.properties.requiredString("name") !=
                    record.properties.requiredString("template_name") ||
                !unique_candidates.insert(candidate.id).second) {
              throw std::runtime_error(
                  "AST JSON dependent template candidate has no exact "
                  "canonical identity");
            }
          }
        }
        if (record.kind == "SgTemplateInstantiationMemberFunctionDecl") {
          const uint64_t primary =
              singleEdgeTarget(record, "templateDeclaration");
          if (primary != 0) {
            if (ast.node(primary).kind != "SgTemplateMemberFunctionDeclaration")
              throw std::runtime_error(
                  "AST JSON member-function instantiation requires an exact "
                  "template declaration");
          } else {
            const NodeRecord &scope =
                ast.node(requiredSingleEdgeTarget(record, "scope"));
            if (scope.kind != "SgTemplateInstantiationDefn" ||
                ast.node(requiredSingleEdgeTarget(scope, "declaration")).kind !=
                    "SgTemplateInstantiationDecl" ||
                record.properties.requiredInt("specialization") !=
                    SgDeclarationStatement::e_specialization ||
                !written_arguments.empty() ||
                !edgesFor(record, "deducedTemplateArguments").empty())
              throw std::runtime_error(
                  "AST JSON ordinary member specialization requires its "
                  "exact class instantiation and no function-template "
                  "arguments");
          }
        }
        if (record.kind == "SgTemplateInstantiationTypedefDeclaration" &&
            ast.node(requiredSingleEdgeTarget(record, "templateDeclaration"))
                    .kind != "SgTemplateTypedefDeclaration") {
          throw std::runtime_error(
              "AST JSON template typedef instantiation requires an exact "
              "template declaration");
        }
        if (record.kind != "SgTemplateInstantiationDecl" &&
            singleEdgeTarget(record, "templateDeclaration") !=
                singleEdgeTarget(record, "specializedTemplateDeclaration"))
          throw std::runtime_error(
              "AST JSON instantiation has conflicting primary and specialized "
              "template identities");
      }
    } else if (record.kind == "SgPragmaDeclaration") {
      const uint64_t pragma_id = requiredSingleEdgeTarget(record, "pragma");
      const NodeRecord &pragma = ast.node(pragma_id);
      if (pragma.kind != "SgPragma" ||
          singleEdgeTarget(pragma, "parent") != record.id) {
        throw std::runtime_error(
            "AST JSON pragma declaration has no exact owned pragma");
      }
      const bool has_start =
          pragma.location.at("start").requiredBool("present");
      const bool has_end = pragma.location.at("end").requiredBool("present");
      if (has_start != has_end)
        throw std::runtime_error(
            "AST JSON pragma has incomplete source position");
    } else if (record.kind == "SgAsmOp") {
      (void)asmOperandFieldsFromJson(record.properties);
      const uint64_t expression =
          requiredSingleEdgeTarget(record, "expression");
      if (singleEdgeTarget(ast.node(expression), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON asm operand lacks its owned expression");
    } else if (record.kind == "SgAsmStmt") {
      const auto fields = asmStatementFieldsFromJson(record.properties);
      const auto operands = edgesFor(record, "operands");
      if (!fields.extended && !operands.empty())
        throw std::runtime_error("AST JSON simple asm has extended operands");
      std::unordered_set<uint64_t> seen;
      for (const auto &edge : operands) {
        const auto &operand = ast.node(edge.target);
        if (operand.kind != "SgAsmOp" ||
            singleEdgeTarget(operand, "parent") != record.id ||
            !seen.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON asm statement lacks exclusive operands");
      }
    } else if (record.kind == "SgCxxDefaultInitExp") {
      const auto kind = defaultInitializerKindFromJson(record.properties);
      const auto &expression =
          ast.node(requiredSingleEdgeTarget(record, "expression"));
      if (!is_expression(expression) ||
          singleEdgeTarget(expression, "parent") != record.id ||
          record.properties.requiredBool("lvalue") !=
              expression.properties.requiredBool("lvalue"))
        throw std::runtime_error(
            "AST JSON default argument lacks its exact owned expansion");
      const uint64_t parameter_id = defaultInitializerDeclarationNodeFromJson(
          record.properties.at("source_declaration"));
      if (singleEdgeTarget(record, "source_declaration") != parameter_id)
        throw std::runtime_error(
            "AST JSON default argument has conflicting parameter identities");
      if (kind == SgCxxDefaultInitExp::e_member) {
        if (parameter_id == 0)
          throw std::runtime_error(
              "AST JSON default member requires its exact field declaration");
        const auto &field = ast.node(parameter_id);
        const auto &declaration =
            ast.node(requiredSingleEdgeTarget(field, "parent"));
        const uint64_t scope = requiredSingleEdgeTarget(field, "scope");
        const auto &owner = ast.node(scope);
        const auto fields = edgesFor(declaration, "variables");
        if (field.kind != "SgInitializedName" ||
            declaration.kind != "SgVariableDeclaration" ||
            (owner.kind != "SgClassDefinition" &&
             owner.kind != "SgTemplateClassDefinition" &&
             owner.kind != "SgTemplateInstantiationDefn") ||
            singleEdgeTarget(field, "declptr") != declaration.id ||
            singleEdgeTarget(declaration, "parent") != scope ||
            singleEdgeTarget(field, "initptr") == 0 ||
            std::count_if(fields.begin(), fields.end(),
                          [&](const EdgeRecord &edge) {
                            return edge.target == parameter_id;
                          }) != 1)
          throw std::runtime_error(
              "AST JSON default member lacks exact field ownership");
      } else if (parameter_id != 0) {
        const auto &parameter = ast.node(parameter_id);
        const auto &list =
            ast.node(requiredSingleEdgeTarget(parameter, "parent"));
        const uint64_t owner_id = requiredSingleEdgeTarget(list, "parent");
        const auto &owner = ast.node(owner_id);
        const auto parameters = edgesFor(list, "args");
        if (parameter.kind != "SgInitializedName" ||
            list.kind != "SgFunctionParameterList" ||
            !isFunctionDeclarationRecordKind(owner.kind) ||
            singleEdgeTarget(parameter, "declptr") != owner_id ||
            (singleEdgeTarget(owner, "parameterList") != list.id &&
             singleEdgeTarget(owner, "parameterList_syntax") != list.id) ||
            std::count_if(parameters.begin(), parameters.end(),
                          [&](const EdgeRecord &edge) {
                            return edge.target == parameter_id;
                          }) != 1)
          throw std::runtime_error(
              "AST JSON default argument lacks exact parameter ownership");
      }
    } else if (record.kind == "SgOpaqueValueBinding") {
      (void)opaqueValueFieldsFromJson(record.properties);
      const auto &source =
          ast.node(requiredSingleEdgeTarget(record, "source_expression"));
      const auto &list = ast.node(requiredSingleEdgeTarget(record, "parent"));
      const auto &owner = ast.node(requiredSingleEdgeTarget(list, "parent"));
      const auto steps = edgesFor(list, "expressions");
      if (singleEdgeTarget(source, "parent") != record.id ||
          list.kind != "SgExprListExp" || owner.kind != "SgPseudoObjectExp" ||
          requiredSingleEdgeTarget(owner, "semantic_evaluations") != list.id ||
          std::count_if(steps.begin(), steps.end(),
                        [&](const EdgeRecord &edge) {
                          return edge.target == record.id;
                        }) != 1)
        throw std::runtime_error(
            "AST JSON opaque binding lacks exact owned source");
    } else if (record.kind == "SgOpaqueValueReference") {
      const uint64_t binding = requiredSingleEdgeTarget(record, "binding");
      const auto &identity = record.properties.at("binding_reference");
      if (identity.find("binding_depth") != nullptr ||
          requiredPositiveId(identity.at("binding"), "opaque binding") !=
              binding)
        throw std::runtime_error(
            "AST JSON opaque reference has conflicting identity");
      require_opaque_binding(record, binding);
    } else if (record.kind == "SgPseudoObjectExp") {
      (void)opaqueValueFieldsFromJson(record.properties);
      const auto &written =
          ast.node(requiredSingleEdgeTarget(record, "written_expression"));
      const auto &evaluations =
          ast.node(requiredSingleEdgeTarget(record, "semantic_evaluations"));
      const uint64_t result = singleEdgeTarget(record, "result_expression");
      const auto steps = edgesFor(evaluations, "expressions");
      if (written.id == evaluations.id || evaluations.kind != "SgExprListExp" ||
          singleEdgeTarget(written, "parent") != record.id ||
          singleEdgeTarget(evaluations, "parent") != record.id ||
          (result != 0 && std::count_if(steps.begin(), steps.end(),
                                        [result](const EdgeRecord &edge) {
                                          return edge.target == result;
                                        }) != 1))
        throw std::runtime_error(
            "AST JSON pseudo-object lacks exact operands and result");
      std::unordered_set<uint64_t> captures;
      for (const auto &edge : edgesFor(record, "captured_bindings")) {
        if (!captures.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON pseudo-object has duplicate captures");
        require_opaque_binding(record, edge.target);
      }
    } else if (record.kind == "SgCxxDecompositionRefExp") {
      const auto owner = requiredSingleEdgeTarget(record, "decomposition");
      if (record.properties.requiredNonnegativeNodeId("decomposition") !=
              owner ||
          ast.node(owner).kind != "SgInitializedName" ||
          !ast.node(owner).properties.requiredName("name").is_null() ||
          singleEdgeTarget(ast.node(owner), "structured_binding_pattern") ==
              0 ||
          structuredBindingKindFromJson(ast.node(owner).properties) !=
              SgInitializedName::e_not_structured_binding ||
          !record.properties.requiredBool("lvalue"))
        throw std::runtime_error(
            "AST JSON decomposition reference has invalid storage identity");
    } else if (record.kind == "SgCxxSubobjectDestruction") {
      const auto kind = subobjectDestructionKindFromJson(record.properties);
      const uint64_t member = singleEdgeTarget(record, "member_target");
      const uint64_t base = singleEdgeTarget(record, "base_target");
      require_destructor(
          requiredSingleEdgeTarget(record, "destructor_declaration"));
      const auto &list = ast.node(requiredSingleEdgeTarget(record, "parent"));
      const auto &definition =
          ast.node(requiredSingleEdgeTarget(list, "parent"));
      const auto entries = edgesFor(list, "expressions");
      if (record.properties.requiredBool("lvalue") ||
          (kind == SgCxxSubobjectDestruction::e_member
               ? member == 0 || base != 0 ||
                     ast.node(member).kind != "SgInitializedName"
               : base == 0 || member != 0 ||
                     !isClassDeclarationRecordKind(ast.node(base).kind)) ||
          list.kind != "SgExprListExp" ||
          (definition.kind != "SgFunctionDefinition" &&
           definition.kind != "SgTemplateFunctionDefinition") ||
          requiredSingleEdgeTarget(definition, "destructor_epilogue") !=
              list.id ||
          std::count_if(entries.begin(), entries.end(),
                        [&](const EdgeRecord &edge) {
                          return edge.target == record.id;
                        }) != 1)
        throw std::runtime_error(
            "AST JSON subobject destruction has invalid targets or ownership");
    } else if (record.kind == "SgCxxArrayInitBinding") {
      (void)arrayInitBindingCategoryFromJson(record.properties);
      const auto &source =
          ast.node(requiredSingleEdgeTarget(record, "common_source"));
      const auto &owner = ast.node(requiredSingleEdgeTarget(record, "parent"));
      if (singleEdgeTarget(source, "parent") != record.id ||
          owner.kind != "SgCxxArrayInitLoop" ||
          requiredSingleEdgeTarget(owner, "binding") != record.id)
        throw std::runtime_error(
            "AST JSON array initialization binding lacks exact ownership");
    } else if (record.kind == "SgCxxArrayInitLoop") {
      const auto &binding =
          ast.node(requiredSingleEdgeTarget(record, "binding"));
      const auto &element =
          ast.node(requiredSingleEdgeTarget(record, "element_initializer"));
      (void)unsigned64ValueFromJson(record.properties.at("element_count"));
      if (record.properties.requiredBool("lvalue") ||
          binding.kind != "SgCxxArrayInitBinding" || binding.id == element.id ||
          singleEdgeTarget(binding, "parent") != record.id ||
          singleEdgeTarget(element, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON array initialization loop lacks exact owned operands");
    } else if (record.kind == "SgCxxArrayInitReference") {
      const auto fields = arrayInitReferenceFieldsFromJson(record.properties);
      const uint64_t binding_id = requiredSingleEdgeTarget(record, "binding");
      if (record.properties.find("binding_depth") != nullptr ||
          requiredPositiveId(record.properties.at("binding"),
                             "array initialization binding") != binding_id ||
          ast.node(binding_id).kind != "SgCxxArrayInitBinding")
        throw std::runtime_error("AST JSON array initialization reference has "
                                 "inconsistent binding identity");
      const uint64_t owner_id =
          requiredSingleEdgeTarget(ast.node(binding_id), "parent");
      bool found = false;
      bool passed_element = false;
      std::unordered_set<uint64_t> visited;
      const NodeRecord *child = &record;
      while (const uint64_t parent_id = singleEdgeTarget(*child, "parent")) {
        if (!visited.insert(parent_id).second)
          throw std::runtime_error(
              "AST JSON array initialization reference has cyclic ownership");
        const auto &parent = ast.node(parent_id);
        if (parent.kind == "SgCxxArrayInitLoop" &&
            requiredSingleEdgeTarget(parent, "element_initializer") ==
                child->id) {
          if (parent.id == owner_id) {
            found = !passed_element ||
                    fields.kind == SgCxxArrayInitReference::e_common_value;
            break;
          }
          passed_element = true;
        }
        child = &parent;
      }
      if (!found)
        throw std::runtime_error("AST JSON array initialization reference has "
                                 "no enclosing element binding");
    } else if (record.kind == "SgMacroExpansionExp") {
      const uint64_t expanded_id =
          requiredSingleEdgeTarget(record, "expanded_expression");
      if (record.properties.requiredString("spelling").empty() ||
          singleEdgeTarget(ast.node(expanded_id), "parent") != record.id) {
        throw std::runtime_error(
            "AST JSON macro expansion lacks an exact owned expression");
      }
      (void)record.properties.requiredBool("expansion_is_modified");
    } else if (record.kind == "SgStaticAssertionDeclaration") {
      const uint64_t condition_id =
          requiredSingleEdgeTarget(record, "condition");
      const uint64_t message_id = singleEdgeTarget(record, "message");
      if (condition_id == message_id ||
          singleEdgeTarget(ast.node(condition_id), "parent") != record.id ||
          (message_id != 0 &&
           singleEdgeTarget(ast.node(message_id), "parent") != record.id)) {
        throw std::runtime_error(
            "AST JSON static assertion lacks exact owned children");
      }
    } else if (record.kind == "SgConstructorInitializer") {
      const uint64_t declaration = singleEdgeTarget(record, "declaration");
      std::optional<FunctionModifierFields> selected_modifiers;
      bool deleted = false;
      if (declaration != 0) {
        if (auto mapped = nodes.find(declaration); mapped != nodes.end())
          deleted = nodeByIdAs<SgMemberFunctionDeclaration>(nodes, declaration)
                        ->get_functionModifier()
                        .isDeleted();
        if (auto serialized = ast.index_by_id.find(declaration);
            serialized != ast.index_by_id.end()) {
          const auto &selected = ast.nodes.at(serialized->second);
          if (isFunctionDeclarationRecordKind(selected.kind)) {
            selected_modifiers = functionModifierFieldsFromJson(selected);
            deleted =
                deleted ||
                selected_modifiers->function[SgFunctionModifier::e_cxx_deleted];
          }
        }
      }
      if (deleted)
        throw std::runtime_error(
            "AST JSON constructor selects a deleted declaration");
      const bool elidable = constructorElisionFromProperties(record.properties);
      const auto argument_source =
          constructorArgumentSourceFromProperties(record.properties);
      if (argument_source !=
          SgConstructorInitializer::e_constructor_argument_expressions) {
        const auto &name = ast.node(requiredSingleEdgeTarget(record, "parent"));
        const auto &list = ast.node(requiredSingleEdgeTarget(name, "parent"));
        const auto &owner = ast.node(requiredSingleEdgeTarget(list, "parent"));
        const auto preinitialization =
            name.properties.requiredInt("preinitialization");
        const auto entries = edgesFor(list, "ctors");
        if (name.kind != "SgInitializedName" ||
            singleEdgeTarget(name, "initptr") != record.id ||
            (preinitialization != SgInitializedName::e_virtual_base_class &&
             preinitialization != SgInitializedName::e_nonvirtual_base_class) ||
            name.properties.at("ctor_initializer_source_order").kind !=
                JsonValue::Kind::Null ||
            list.kind != "SgCtorInitializerList" ||
            std::count_if(entries.begin(), entries.end(),
                          [&](const EdgeRecord &edge) {
                            return edge.target == name.id;
                          }) != 1 ||
            singleEdgeTarget(owner, "CtorInitializerList") != list.id)
          throw std::runtime_error(
              "AST JSON inherited constructor argument context is invalid");
        const auto modifiers = bitVectorFromJson(
            owner.properties.at("special_function_modifier_vector"),
            "special_function_modifier_vector",
            SgSpecialFunctionModifier::e_last_modifier);
        if (!modifiers[SgSpecialFunctionModifier::e_constructor])
          throw std::runtime_error(
              "AST JSON inherited constructor argument context is invalid");
      }
      if (elidable) {
        const NodeRecord &arguments =
            ast.node(requiredSingleEdgeTarget(record, "args"));
        const auto operands = edgesFor(arguments, "expressions");
        if (declaration == 0 || arguments.kind != "SgExprListExp" ||
            singleEdgeTarget(arguments, "parent") != record.id ||
            operands.empty() ||
            ast.node(operands.front().target).kind == "SgCxxDefaultInitExp" ||
            singleEdgeTarget(ast.node(operands.front().target), "parent") !=
                arguments.id ||
            !std::all_of(operands.begin() + 1, operands.end(),
                         [&](const EdgeRecord &edge) {
                           const auto &operand = ast.node(edge.target);
                           return operand.kind == "SgCxxDefaultInitExp" &&
                                  singleEdgeTarget(operand, "parent") ==
                                      arguments.id;
                         }))
          throw std::runtime_error(
              "AST JSON elidable constructor requires its selected declaration "
              "and one owned argument");
        const NodeRecord &selected = ast.node(declaration);
        if (!isMemberFunctionDeclarationRecordKind(selected.kind))
          throw std::runtime_error(
              "AST JSON elidable constructor has no selected constructor");
        ROSE_ASSERT(selected_modifiers.has_value());
        const auto &modifiers = *selected_modifiers;
        if (!modifiers.function[SgFunctionModifier::e_cxx_copy_constructor] &&
            !modifiers.function[SgFunctionModifier::e_cxx_move_constructor])
          throw std::runtime_error("AST JSON elidable constructor has no "
                                   "selected copy/move constructor");
        const auto &scope =
            ast.node(requiredSingleEdgeTarget(selected, "scope"));
        if (!modifiers.special[SgSpecialFunctionModifier::e_constructor] ||
            (scope.kind != "SgClassDefinition" &&
             scope.kind != "SgTemplateClassDefinition" &&
             scope.kind != "SgTemplateInstantiationDefn"))
          throw std::runtime_error(
              "AST JSON elidable constructor has no selected constructor");
      }
    } else if (record.kind == "SgNewExp") {
      const auto implicit_arguments =
          record.properties.requiredInt("allocation_implicit_argument_count");
      if (implicit_arguments < 1 || implicit_arguments > 3)
        throw std::runtime_error(
            "AST JSON new-expression implicit argument count is out of range");
      const auto source_form =
          newArrayBoundSourceFormFromJson(record.properties);
      const auto bound = singleEdgeTarget(record, "array_bound");
      if ((bound == 0) != (source_form == SgNewExp::e_no_array_bound))
        throw std::runtime_error(
            "AST JSON new-expression array bound and source form disagree");
      if (bound != 0 &&
          (!is_expression(ast.node(bound)) ||
           singleEdgeTarget(ast.node(bound), "parent") != record.id))
        throw std::runtime_error(
            "AST JSON new-expression array bound has conflicting ownership");
    } else if (record.kind == "SgGotoStatement") {
      if (const uint64_t selector =
              singleEdgeTarget(record, "selector_expression")) {
        if (singleEdgeTarget(record, "label") != 0 ||
            singleEdgeTarget(record, "label_expression") != 0 ||
            !is_expression(ast.node(selector)) ||
            singleEdgeTarget(ast.node(selector), "parent") != record.id)
          throw std::runtime_error("AST JSON computed goto has invalid "
                                   "selector ownership or target");
      }
    } else if (record.kind == "SgExprStatement" ||
               record.kind == "SgReturnStmt") {
      if (record.kind == "SgReturnStmt") {
        const auto fields = returnStatementFieldsFromJson(record.properties);
        exactSerializedPointerTarget(record, "nrvo_candidate",
                                     "nrvo_candidate");
        if (fields.nrvo_candidate != 0) {
          const auto &candidate = ast.node(fields.nrvo_candidate);
          if (candidate.kind != "SgInitializedName")
            throw std::runtime_error(
                "AST JSON return NRVO candidate is not an initialized name");
          const auto &variable =
              ast.node(requiredSingleEdgeTarget(candidate, "parent"));
          const auto storage = requiredStorageModifier(
              variable.properties, "declaration_storage_modifier",
              "NRVO candidate");
          const auto &scope =
              ast.node(requiredSingleEdgeTarget(candidate, "scope"));
          const auto variables = edgesFor(variable, "variables");
          if (variable.kind != "SgVariableDeclaration" ||
              requiredSingleEdgeTarget(candidate, "declptr") != variable.id ||
              requiredOptionalUnsignedInteger(
                  candidate.properties, "cxx_field_index", "NRVO candidate")
                  .has_value() ||
              scope.kind == "SgGlobal" ||
              scope.kind == "SgNamespaceDefinitionStatement" ||
              scope.kind == "SgClassDefinition" ||
              scope.kind == "SgTemplateClassDefinition" ||
              scope.kind == "SgTemplateInstantiationDefn" ||
              scope.kind == "SgFunctionParameterScope" ||
              storage == SgStorageModifier::e_static ||
              storage == SgStorageModifier::e_extern ||
              variable.properties.requiredBool(
                  "declaration_thread_local_storage") ||
              variable.properties.requiredBool("is_thread_local") ||
              std::count_if(variables.begin(), variables.end(),
                            [&](const EdgeRecord &edge) {
                              return edge.target == candidate.id;
                            }) != 1)
            throw std::runtime_error(
                "AST JSON return NRVO candidate is not a local variable");
          const auto function_owner = [&](uint64_t id) {
            std::unordered_set<uint64_t> visited;
            while (id != 0) {
              if (!visited.insert(id).second)
                throw std::runtime_error(
                    "AST JSON return NRVO ownership is cyclic");
              const auto &owner = ast.node(id);
              if (owner.kind == "SgFunctionDefinition" ||
                  owner.kind == "SgTemplateFunctionDefinition")
                return id;
              id = singleEdgeTarget(owner, "parent");
            }
            return uint64_t{0};
          };
          const uint64_t owner = function_owner(record.id);
          if (owner == 0 || function_owner(scope.id) != owner)
            throw std::runtime_error("AST JSON return NRVO candidate has no "
                                     "matching enclosing function");
        }
        auto &prepared =
            construction_fields
                .try_emplace(record.id,
                             RecordConstructionFields{
                                 validateConstructionFlags(record), {}})
                .first->second;
        prepared.return_statement = fields;
      }
      if (const uint64_t expression_id =
              singleEdgeTarget(record, "expression")) {
        if (singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " expression has no exact owning statement");
      }
    } else if (record.kind == "SgAssignStatement" ||
               record.kind == "SgAssignedGotoStatement" ||
               record.kind == "SgComputedGotoStatement") {
      auto require_operand = [&](const char *field) {
        const uint64_t id = requiredSingleEdgeTarget(record, field);
        if (singleEdgeTarget(ast.node(id), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON Fortran branch operand has no exact statement owner");
      };
      if (record.kind == "SgAssignStatement") {
        require_operand("label");
        require_operand("value");
      } else if (record.kind == "SgAssignedGotoStatement") {
        require_operand("targets");
      } else {
        require_operand("labelList");
        require_operand("label_index");
      }
    } else if (record.kind == "SgReadStatement" ||
               record.kind == "SgWriteStatement") {
      const auto &binding = record.properties.at("namelist");
      if (binding.kind != JsonValue::Kind::Null) {
        const auto scope =
            requiredPositiveId(binding.at("binding_scope"), "NAMELIST scope");
        const auto &symbol = binding.at("symbol");
        const auto group = requiredPositiveId(symbol.at("symbol_declaration"),
                                              "NAMELIST declaration");
        const auto kind =
            symbolVariantFromJsonKind(symbol.requiredString("symbol_kind"));
        const auto name = binding.requiredName("binding_name");
        if ((kind != V_SgNamelistSymbol && kind != V_SgAliasSymbol) ||
            ast.node(group).kind != "SgNameGroup" || name.is_null() ||
            symbol.requiredName("symbol_name").is_null() ||
            singleEdgeTarget(record, "format") != 0)
          throw std::runtime_error(
              "AST JSON NAMELIST I/O has an invalid typed binding");
        const auto entries = symbolTableEntriesFromRecord(ast.node(scope));
        rose_hash_multimap names;
        names.set_case_insensitive_semantics(
            ast.node(scope).properties.requiredBool("case_insensitive"));
        size_t matches = 0;
        for (const auto &entry : entries) {
          const auto basis = entry.visibility ? entry.visibility->basis_id
                                              : entry.basis_id.value();
          if (entry.symbol_kind == kind && basis == group &&
              names.key_eq()(entry.entry_name, name))
            ++matches;
        }
        if (matches != 1)
          throw std::runtime_error(
              "AST JSON NAMELIST I/O has no exact symbol binding");
      }
    } else if (record.kind == "SgNameGroup" ||
               record.kind == "SgNamelistStatement") {
      if (record.properties.find("name_list") != nullptr)
        throw std::runtime_error(
            "AST JSON NAMELIST retains string object names");
      const char *role =
          record.kind == "SgNameGroup" ? "objects" : "group_list";
      const auto children = edgesFor(record, role);
      if (children.empty() ||
          (record.kind == "SgNameGroup" &&
           record.properties.requiredName("group_name").is_null()))
        throw std::runtime_error(
            "AST JSON NAMELIST has incomplete typed inputs");
      if (record.kind == "SgNamelistStatement") {
        static const VariantVector scopes(V_SgScopeStatement);
        if (!kindInVariants(
                ast.node(requiredSingleEdgeTarget(record, "parent")).kind,
                scopes))
          throw std::runtime_error(
              "AST JSON NAMELIST has no exact owning scope");
      }
      std::unordered_set<uint64_t> seen;
      for (size_t index = 0; index < children.size(); ++index) {
        const auto &edge = children[index];
        const auto &child = ast.node(edge.target);
        if (edge.index != index || !seen.insert(edge.target).second ||
            requiredSingleEdgeTarget(child, "parent") != record.id ||
            child.kind !=
                (record.kind == "SgNameGroup" ? "SgVarRefExp" : "SgNameGroup"))
          throw std::runtime_error(
              "AST JSON NAMELIST has conflicting child ownership");
      }
    } else if (record.kind == "SgFormatItem" ||
               record.kind == "SgFormatItemList" ||
               record.kind == "SgFormatStatement") {
      const auto children = edgesFor(record, "format_item_list");
      if (record.kind != "SgFormatItemList" && children.size() > 1)
        throw std::runtime_error("AST JSON FORMAT has multiple child lists");
      if (record.kind == "SgFormatStatement" && children.size() != 1)
        throw std::runtime_error(
            "AST JSON FORMAT statement requires its item list");
      if (record.kind == "SgFormatItem") {
        const auto fields = formatItemFieldsFromJson(record.properties);
        if (!edgesFor(record, "data").empty() ||
            !SgFormatItem::is_valid_descriptor(
                fields.descriptor, fields.repeat_count, !children.empty()))
          throw std::runtime_error(
              "AST JSON FORMAT descriptor has inconsistent children");
      }
      std::unordered_set<uint64_t> seen;
      for (std::size_t index = 0; index < children.size(); ++index) {
        const auto &edge = children[index];
        const auto &child = ast.node(edge.target);
        if (edge.index != index || !seen.insert(edge.target).second ||
            singleEdgeTarget(child, "parent") != record.id ||
            child.kind != (record.kind == "SgFormatItemList"
                               ? "SgFormatItem"
                               : "SgFormatItemList"))
          throw std::runtime_error(
              "AST JSON FORMAT child has conflicting ownership");
      }
    } else if (record.kind == "SgDataStatementValue" ||
               record.kind == "SgDataStatementObject" ||
               record.kind == "SgDataStatementGroup" ||
               record.kind == "SgFortranDataInitializer" ||
               record.kind == "SgAttributeSpecificationStatement") {
      auto require_child = [&](uint64_t id, uint64_t owner) {
        if (id == 0 || singleEdgeTarget(ast.node(id), "parent") != owner)
          throw std::runtime_error(
              "AST JSON DATA child has conflicting ownership");
      };
      auto require_list = [&](const char *field, const char *kind) {
        const auto edges = edgesFor(record, field);
        if (edges.empty())
          throw std::runtime_error("AST JSON DATA has an empty required list");
        std::unordered_set<uint64_t> children;
        for (size_t index = 0; index < edges.size(); ++index) {
          const auto &edge = edges[index];
          require_child(edge.target, record.id);
          if (edge.index != index || ast.node(edge.target).kind != kind ||
              !children.insert(edge.target).second)
            throw std::runtime_error(
                "AST JSON DATA has invalid typed list children");
        }
      };
      auto require_owner = [&](const char *kind, const char *field) {
        const auto &owner =
            ast.node(requiredSingleEdgeTarget(record, "parent"));
        const auto edges = edgesFor(owner, field);
        if (owner.kind != kind || std::count_if(edges.begin(), edges.end(),
                                                [&](const EdgeRecord &edge) {
                                                  return edge.target ==
                                                         record.id;
                                                }) != 1)
          throw std::runtime_error(
              "AST JSON DATA child has no exact typed owner");
      };
      if (record.kind == "SgDataStatementValue") {
        const uint64_t constant =
            requiredSingleEdgeTarget(record, "constant_expression");
        const uint64_t repeat = singleEdgeTarget(record, "repeat_expression");
        if (ast.node(constant).kind == "SgExprListExp" ||
            (repeat != 0 && ast.node(repeat).kind == "SgExprListExp"))
          throw std::runtime_error(
              "AST JSON DATA value cannot use an expression list operand");
        require_child(constant, record.id);
        if (repeat != 0) {
          require_child(repeat, record.id);
          const auto &properties = ast.node(repeat).properties;
          if (repeat == constant ||
              !properties.requiredBool(
                  "fortran_integer_constant_value_is_available") ||
              properties.requiredInt("fortran_integer_constant_value") < 0)
            throw std::runtime_error(
                "AST JSON DATA repeat has no valid folded integer");
        }
        const auto &owner =
            ast.node(requiredSingleEdgeTarget(record, "parent"));
        const char *field =
            owner.kind == "SgDataStatementGroup" ? "value_list" : "values";
        const auto owner_edges = edgesFor(owner, field);
        if ((owner.kind != "SgDataStatementGroup" &&
             owner.kind != "SgFortranDataInitializer") ||
            std::count_if(owner_edges.begin(), owner_edges.end(),
                          [&](const EdgeRecord &edge) {
                            return edge.target == record.id;
                          }) != 1)
          throw std::runtime_error(
              "AST JSON DATA value has no exact typed owner");
      } else if (record.kind == "SgDataStatementObject") {
        require_owner("SgDataStatementGroup", "object_list");
        const uint64_t id =
            requiredSingleEdgeTarget(record, "variableReference_list");
        require_child(id, record.id);
        const auto &list = ast.node(id);
        if (list.kind != "SgExprListExp" ||
            edgesFor(list, "expressions").empty())
          throw std::runtime_error(
              "AST JSON DATA object has no expression list");
        std::unordered_set<uint64_t> children;
        size_t index = 0;
        for (const auto &edge : edgesFor(list, "expressions")) {
          require_child(edge.target, id);
          if (edge.index != index++ || !children.insert(edge.target).second)
            throw std::runtime_error(
                "AST JSON DATA object has invalid expression ownership");
        }
      } else if (record.kind == "SgDataStatementGroup") {
        require_owner("SgAttributeSpecificationStatement",
                      "data_statement_group_list");
        require_list("object_list", "SgDataStatementObject");
        require_list("value_list", "SgDataStatementValue");
      } else if (record.kind == "SgFortranDataInitializer") {
        require_list("values", "SgDataStatementValue");
        const auto &owner =
            ast.node(requiredSingleEdgeTarget(record, "parent"));
        if (owner.kind != "SgInitializedName" ||
            singleEdgeTarget(owner, "initptr") != record.id ||
            !record.properties.at("type").exactlyEquals(
                owner.properties.at("type")))
          throw std::runtime_error(
              "AST JSON DATA initializer has no exact destination type");
      } else if (record.properties.requiredInt("attribute_kind") ==
                 SgAttributeSpecificationStatement::e_dataStatement) {
        require_list("data_statement_group_list", "SgDataStatementGroup");
        if (record.properties.find("intent") != nullptr ||
            !stringListFromJson(record.properties.at("name_list"), "name_list")
                 .empty() ||
            singleEdgeTarget(record, "parameter_list") != 0 ||
            singleEdgeTarget(record, "bind_list") != 0)
          throw std::runtime_error(
              "AST JSON DATA statement has contradictory attribute payloads");
      } else if (!edgesFor(record, "data_statement_group_list").empty()) {
        throw std::runtime_error(
            "AST JSON non-DATA attribute owns DATA groups");
      }
    } else if (record.kind == "SgFortranImplicitRange") {
      (void)fortranImplicitRangeInput(record.properties);
      const auto &owner = ast.node(requiredSingleEdgeTarget(record, "parent"));
      const auto ranges = edgesFor(owner, "ranges");
      if (owner.kind != "SgImplicitStatement" ||
          std::count_if(ranges.begin(), ranges.end(),
                        [&](const EdgeRecord &edge) {
                          return edge.target == record.id;
                        }) != 1)
        throw std::runtime_error("AST JSON implicit range has no exact owner");
    } else if (record.kind == "SgImplicitStatement") {
      const auto spec = implicitStatementSpec(record.properties);
      const auto ranges = edgesFor(record, "ranges");
      if ((spec == SgImplicitStatement::e_has_implicit_spec_list) ==
          ranges.empty())
        throw std::runtime_error(
            "AST JSON implicit statement has invalid ranges");
      std::unordered_set<uint64_t> children;
      bool assigned_letters[26] = {};
      auto letter_index = [](char letter) {
        return letter >= 'a' ? letter - 'a' : letter - 'A';
      };
      for (size_t index = 0; index < ranges.size(); ++index) {
        const auto &edge = ranges[index];
        const auto &range = ast.node(edge.target);
        if (edge.index != index || range.kind != "SgFortranImplicitRange" ||
            singleEdgeTarget(range, "parent") != record.id ||
            !children.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON implicit statement has invalid range ownership");
        const auto input = fortranImplicitRangeInput(range.properties);
        const int first = letter_index(input.first);
        const int last = letter_index(input.last.value_or(input.first));
        for (int letter = first; letter <= last; ++letter) {
          if (assigned_letters[letter])
            throw std::runtime_error(
                "AST JSON implicit statement has overlapping letter ranges");
          assigned_letters[letter] = true;
        }
      }
    } else if (record.kind == "SgCaseOptionStmt" ||
               record.kind == "SgDefaultOptionStmt") {
      const bool is_case = record.kind == "SgCaseOptionStmt";
      (void)record.properties.requiredString(
          is_case ? "case_construct_name" : "default_construct_name");
      const uint64_t body = requiredSingleEdgeTarget(record, "body");
      const uint64_t key =
          is_case ? requiredSingleEdgeTarget(record, "key") : 0;
      const uint64_t end = singleEdgeTarget(record, "key_range_end");
      if (!is_statement(ast.node(body)) ||
          (key != 0 && !is_expression(ast.node(key))) ||
          (end != 0 && (!is_case || !is_expression(ast.node(end)))) ||
          (!is_case && singleEdgeTarget(record, "key") != 0))
        throw std::runtime_error(
            "AST JSON CASE/DEFAULT requires typed children");
      std::unordered_set<uint64_t> children;
      for (uint64_t id : {body, key, end})
        if (id != 0 && (id == record.id || !children.insert(id).second ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id))
          throw std::runtime_error(
              "AST JSON CASE/DEFAULT children lack distinct exact ownership");
      if (key != 0 && ast.node(key).kind == "SgExprListExp") {
        if (end != 0 || edgesFor(ast.node(key), "expressions").empty())
          throw std::runtime_error("AST JSON CASE has an invalid key list");
        std::unordered_set<uint64_t> keys;
        const auto &elements = edgesFor(ast.node(key), "expressions");
        for (size_t index = 0; index != elements.size(); ++index) {
          const auto &edge = elements[index];
          if (edge.index != index || !is_expression(ast.node(edge.target)) ||
              !keys.insert(edge.target).second ||
              singleEdgeTarget(ast.node(edge.target), "parent") != key)
            throw std::runtime_error(
                "AST JSON CASE key list lacks exact ordered ownership");
        }
      }
    } else if (record.kind == "SgWhereStatement" ||
               record.kind == "SgElseWhereStatement") {
      const bool where = record.kind == "SgWhereStatement";
      if (where) {
        const bool has_end =
            record.properties.requiredBool("has_end_statement");
        const auto name = record.properties.requiredString("string_label");
        if (!has_end &&
            (!name.empty() || singleEdgeTarget(record, "elsewhere") != 0 ||
             singleEdgeTarget(record, "end_numeric_label") != 0))
          throw std::runtime_error(
              "AST JSON WHERE has inconsistent construct form");
      }
      const uint64_t condition = requiredSingleEdgeTarget(record, "condition");
      const uint64_t body = requiredSingleEdgeTarget(record, "body");
      const uint64_t next = singleEdgeTarget(record, "elsewhere");
      const uint64_t label = singleEdgeTarget(record, "end_numeric_label");
      if (!is_expression(ast.node(condition)) ||
          ast.node(body).kind != "SgBasicBlock" ||
          (next != 0 && ast.node(next).kind != "SgElseWhereStatement") ||
          (label != 0 && (!where || ast.node(label).kind != "SgLabelRefExp")) ||
          (ast.node(condition).kind == "SgNullExpression" &&
           (where || next != 0)))
        throw std::runtime_error("AST JSON WHERE requires a typed mask, block "
                                 "and exact clause chain");
      std::unordered_set<uint64_t> children;
      for (uint64_t id : {condition, body, next, label})
        if (id != 0 && (!children.insert(id).second ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id))
          throw std::runtime_error(
              "AST JSON WHERE children lack exact ownership");
      if (!where) {
        const auto &owner =
            ast.node(requiredSingleEdgeTarget(record, "parent"));
        if ((owner.kind != "SgWhereStatement" &&
             owner.kind != "SgElseWhereStatement") ||
            singleEdgeTarget(owner, "elsewhere") != record.id)
          throw std::runtime_error(
              "AST JSON ELSEWHERE has no exact clause owner");
        std::unordered_set<uint64_t> chain{record.id};
        for (const NodeRecord *clause = &owner;
             clause->kind == "SgElseWhereStatement";
             clause = &ast.node(requiredSingleEdgeTarget(*clause, "parent")))
          if (!chain.insert(clause->id).second)
            throw std::runtime_error(
                "AST JSON ELSEWHERE has a cyclic clause chain");
      }
    } else if (record.kind == "SgForAllStatement") {
      (void)requiredEnum<SgForAllStatement::forall_statement_kind_enum>(
          record.properties, "forall_statement_kind", "SgForAllStatement",
          {SgForAllStatement::e_forall_statement,
           SgForAllStatement::e_do_concurrent_statement});
      (void)record.properties.requiredBool("has_end_statement");
      (void)record.properties.requiredString("string_label");
      const uint64_t header = requiredSingleEdgeTarget(record, "forall_header");
      const uint64_t body = requiredSingleEdgeTarget(record, "body");
      const uint64_t label = singleEdgeTarget(record, "end_numeric_label");
      if (ast.node(header).kind != "SgExprListExp" ||
          edgesFor(ast.node(header), "expressions").empty() ||
          ast.node(body).kind != "SgBasicBlock" ||
          (label != 0 && ast.node(label).kind != "SgLabelRefExp"))
        throw std::runtime_error("AST JSON FORALL requires an exact nonempty "
                                 "header, block and label");
      std::unordered_set<uint64_t> children;
      for (uint64_t id : {header, body, label})
        if (id != 0 && (!children.insert(id).second ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id))
          throw std::runtime_error(
              "AST JSON FORALL children lack distinct exact ownership");
    } else if (record.kind == "SgFortranDo") {
      (void)record.properties.requiredBool("has_end_statement");
      (void)record.properties.requiredString("string_label");
      const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
      if (ast.node(body_id).kind != "SgBasicBlock")
        throw std::runtime_error("AST JSON SgFortranDo body is not a block");
      std::unordered_set<uint64_t> children{body_id};
      for (const char *field :
           {"initialization", "bound", "increment", "end_numeric_label"}) {
        if (const uint64_t id = singleEdgeTarget(record, field)) {
          if ((std::string(field) == "end_numeric_label"
                   ? ast.node(id).kind != "SgLabelRefExp"
                   : !is_expression(ast.node(id))) ||
              !children.insert(id).second ||
              singleEdgeTarget(ast.node(id), "parent") != record.id)
            throw std::runtime_error(
                "AST JSON SgFortranDo has duplicate or misowned children");
        }
      }
      if (singleEdgeTarget(ast.node(body_id), "parent") != record.id)
        throw std::runtime_error("AST JSON SgFortranDo body is not owned");
    } else if (record.kind == "SgAllocateStatement" ||
               record.kind == "SgDeallocateStatement") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "expr_list");
      const NodeRecord &list = ast.node(list_id);
      if (list.kind != "SgExprListExp" ||
          singleEdgeTarget(list, "parent") != record.id ||
          edgesFor(list, "expressions").empty()) {
        throw std::runtime_error("AST JSON " + record.kind +
                                 " requires an owned nonempty expression list");
      }
      std::unordered_set<uint64_t> children{list_id};
      auto validate_operand = [&](const char *field) {
        if (const uint64_t id = singleEdgeTarget(record, field)) {
          if (!children.insert(id).second ||
              singleEdgeTarget(ast.node(id), "parent") != record.id)
            throw std::runtime_error("AST JSON " + record.kind +
                                     " has duplicate or misowned operands");
        }
      };
      validate_operand("stat_expression");
      validate_operand("errmsg_expression");
      if (record.kind == "SgAllocateStatement") {
        for (const char *field : {"source_expression", "mold_expression",
                                  "stream_expression", "pinned_expression"})
          validate_operand(field);
      }
    } else if (record.kind == "SgNullifyStatement") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "pointer_list");
      const NodeRecord &list = ast.node(list_id);
      if (list.kind != "SgExprListExp" ||
          singleEdgeTarget(list, "parent") != record.id ||
          edgesFor(list, "expressions").empty()) {
        throw std::runtime_error(
            "AST JSON NULLIFY requires an owned nonempty pointer list");
      }
    } else if (record.kind == "SgOmpFlushStatement" ||
               record.kind == "SgOmpAllocateStatement") {
      if (!edgesFor(record, "variables").empty())
        throw std::runtime_error(
            "AST JSON OpenMP statement has flattened 'variables' edges");
      const auto &variables =
          ast.node(requiredSingleEdgeTarget(record, "variable_list"));
      if (variables.kind != "SgExprListExp" ||
          requiredSingleEdgeTarget(variables, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenMP variable list has no exact owning statement");
    } else if (record.kind == "SgOmpDepobjStatement") {
      const uint64_t depobj_id = requiredSingleEdgeTarget(record, "depobj");
      if (singleEdgeTarget(ast.node(depobj_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON depobj operand has no exact owning statement");
    } else if (record.kind == "SgOmpGroupprivateStatement") {
      const uint64_t variables_id =
          requiredSingleEdgeTarget(record, "variables");
      const NodeRecord &variables = ast.node(variables_id);
      if (variables.kind != "SgExprListExp" ||
          singleEdgeTarget(variables, "parent") != record.id) {
        throw std::runtime_error(
            "AST JSON groupprivate variables lack an exact owned list");
      }
      (void)ompDeviceTypeKindFromJson(record.properties, record.kind);
    } else if (record.kind == "SgAccCollapseClause" ||
               record.kind == "SgAccNumGangsClause" ||
               record.kind == "SgAccNumWorkersClause" ||
               record.kind == "SgAccVectorLengthClause" ||
               record.kind == "SgAccAsyncClause" ||
               record.kind == "SgAccIfClause" ||
               record.kind == "SgAccVectorClause") {
      const bool optional = record.kind == "SgAccAsyncClause" ||
                            record.kind == "SgAccVectorClause";
      const uint64_t expression_id =
          optional ? singleEdgeTarget(record, "expression")
                   : requiredSingleEdgeTarget(record, "expression");
      if (expression_id != 0 &&
          singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " expression has no exact owning clause");
    } else if (record.kind == "SgAccCopyClause" ||
               record.kind == "SgAccCopyinClause" ||
               record.kind == "SgAccCopyoutClause" ||
               record.kind == "SgAccCreateClause" ||
               record.kind == "SgAccPresentClause" ||
               record.kind == "SgAccPrivateClause" ||
               record.kind == "SgAccDeviceptrClause" ||
               record.kind == "SgAccDeleteClause" ||
               record.kind == "SgAccReductionClause") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "variables");
      const NodeRecord &list = ast.node(list_id);
      if (list.kind != "SgExprListExp" ||
          singleEdgeTarget(list, "parent") != record.id ||
          edgesFor(list, "expressions").empty())
        throw std::runtime_error("AST JSON " + record.kind +
                                 " requires one exact owned nonempty list");
      if (record.kind == "SgAccReductionClause") {
        const int64_t reduction_operator =
            record.properties.requiredInt("reduction_operator");
        if (reduction_operator < 0 || reduction_operator > 16)
          throw std::runtime_error("AST JSON SgAccReductionClause has an "
                                   "invalid reduction operator");
      }
    } else if (record.kind == "SgAccWaitStatement") {
      const uint64_t list_id = singleEdgeTarget(record, "wait_list");
      const uint64_t devnum_id = singleEdgeTarget(record, "devnum");
      const bool queues = record.properties.requiredBool("queues");
      if ((queues && list_id == 0) || (list_id != 0 && list_id == devnum_id))
        throw std::runtime_error(
            "AST JSON OpenACC wait has missing or duplicate operands");
      if (list_id != 0) {
        const NodeRecord &list = ast.node(list_id);
        if (list.kind != "SgExprListExp" ||
            singleEdgeTarget(list, "parent") != record.id ||
            edgesFor(list, "expressions").empty())
          throw std::runtime_error(
              "AST JSON OpenACC wait requires an exact owned nonempty list");
      }
      if (devnum_id != 0 &&
          singleEdgeTarget(ast.node(devnum_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenACC wait device number has no exact owner");
    } else if (record.kind == "SgAccCacheStatement") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "variables");
      const NodeRecord &list = ast.node(list_id);
      if (list.kind != "SgExprListExp" ||
          singleEdgeTarget(list, "parent") != record.id ||
          edgesFor(list, "expressions").empty())
        throw std::runtime_error(
            "AST JSON OpenACC cache requires an exact owned nonempty list");
      const int64_t modifier = record.properties.requiredInt("modifier");
      if (modifier != 0 && modifier != 1)
        throw std::runtime_error(
            "AST JSON SgAccCacheStatement has an invalid modifier");
    } else if (record.kind == "SgAccParallelStatement" ||
               record.kind == "SgAccParallelLoopStatement" ||
               record.kind == "SgAccDataStatement" ||
               record.kind == "SgAccKernelsStatement" ||
               record.kind == "SgAccAtomicStatement") {
      const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
      if (body_id == record.id ||
          singleEdgeTarget(ast.node(body_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenACC body has no exact owning statement");
    } else if (record.kind == "SgOmpNowaitClause" ||
               record.kind == "SgOmpOrderedClause" ||
               record.kind == "SgOmpAlignClause" ||
               record.kind == "SgOmpMessageClause" ||
               record.kind == "SgOmpGraphIdClause" ||
               record.kind == "SgOmpGraphResetClause" ||
               record.kind == "SgOmpTransparentClause" ||
               record.kind == "SgOmpThreadsetClause" ||
               record.kind == "SgOmpSafesyncClause" ||
               record.kind == "SgOmpLooprangeClause" ||
               record.kind == "SgOmpNoOpenmpConstructsClause" ||
               record.kind == "SgOmpHoldsClause" ||
               record.kind == "SgOmpUseClause") {
      if (const uint64_t expression_id =
              singleEdgeTarget(record, "expression")) {
        if (singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
          throw std::runtime_error("AST JSON OpenMP " + record.kind +
                                   " expression has no exact owning clause");
      }
    } else if (record.kind == "SgOmpSizesClause") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "expression");
      if (ast.node(list_id).kind != "SgExprListExp" ||
          singleEdgeTarget(ast.node(list_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenMP sizes requires one exact owned expression list");
    } else if (record.kind == "SgOmpIfClause" ||
               record.kind == "SgOmpCollapseClause" ||
               record.kind == "SgOmpNumThreadsClause" ||
               record.kind == "SgOmpNumTeamsClause" ||
               record.kind == "SgOmpSafelenClause" ||
               record.kind == "SgOmpSimdlenClause" ||
               record.kind == "SgOmpDetachClause" ||
               record.kind == "SgOmpFinalClause" ||
               record.kind == "SgOmpPriorityClause" ||
               record.kind == "SgOmpHintClause" ||
               record.kind == "SgOmpThreadLimitClause" ||
               record.kind == "SgOmpNocontextClause" ||
               record.kind == "SgOmpNovariantsClause" ||
               record.kind == "SgOmpFilterClause" ||
               record.kind == "SgOmpPartialClause" ||
               record.kind == "SgOmpGrainsizeClause" ||
               record.kind == "SgOmpNumTasksClause" ||
               record.kind == "SgOmpDeviceClause") {
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expression");
      if (singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenMP expression has no exact owning clause");
    } else if (record.kind == "SgOmpDoacrossClause") {
      (void)ompDoacrossKindFromJson(record.properties);
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expressions");
      if (ast.node(expression_id).kind != "SgExprListExp" ||
          singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
        throw std::runtime_error(
            "AST JSON OpenMP doacross requires one exact owned expression "
            "list");
    } else if (record.kind == "SgOmpDefaultClause" ||
               record.kind == "SgOmpOtherwiseClause") {
      if (record.kind == "SgOmpDefaultClause")
        (void)ompDefaultOptionFromJson(record.properties);
      if (const uint64_t directive_id =
              singleEdgeTarget(record, "variant_directive")) {
        if (singleEdgeTarget(ast.node(directive_id), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON OpenMP variant has no exact owning clause");
      }
    } else if (ordinaryOmpVariablesClauseVariant(record.kind) ||
               record.kind == "SgOmpMapClause" ||
               record.kind == "SgOmpToClause" ||
               record.kind == "SgOmpFromClause" ||
               record.kind == "SgOmpDependClause" ||
               record.kind == "SgOmpAffinityClause" ||
               record.kind == "SgOmpReductionClause" ||
               record.kind == "SgOmpInReductionClause" ||
               record.kind == "SgOmpTaskReductionClause" ||
               record.kind == "SgOmpAllocateClause" ||
               record.kind == "SgOmpAllocatorClause" ||
               record.kind == "SgOmpLinearClause" ||
               record.kind == "SgOmpAlignedClause") {
      std::unordered_set<uint64_t> children;
      const uint64_t variables =
          record.kind == "SgOmpAllocatorClause"
              ? 0
              : requiredSingleEdgeTarget(record, "variables");
      for (uint64_t id :
           {variables, singleEdgeTarget(record, "source_variables"),
            record.kind == "SgOmpDependClause"
                ? singleEdgeTarget(record, "sink_vectors")
                : 0}) {
        if (id != 0 && (ast.node(id).kind != "SgExprListExp" ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id ||
                        !children.insert(id).second))
          throw std::runtime_error(
              "AST JSON transfer variable lists lack distinct exact ownership");
      }
      if (record.kind == "SgOmpAllocateClause" ||
          record.kind == "SgOmpAllocatorClause") {
        for (const char *role : {"user_defined_modifier", "alignment"}) {
          if (const uint64_t id = singleEdgeTarget(record, role)) {
            if (!is_expression(ast.node(id)) ||
                singleEdgeTarget(ast.node(id), "parent") != record.id ||
                !children.insert(id).second)
              throw std::runtime_error(
                  "AST JSON allocator operands lack distinct exact ownership");
          }
        }
        if (record.kind == "SgOmpAllocatorClause" &&
            (singleEdgeTarget(record, "variables") != 0 ||
             singleEdgeTarget(record, "source_variables") != 0 ||
             singleEdgeTarget(record, "alignment") != 0))
          throw std::runtime_error(
              "AST JSON allocator has an unexpected list or alignment");
        if (singleEdgeTarget(record, "mapper_identifier") != 0 ||
            !edgesFor(record, "iterator_definitions").empty())
          throw std::runtime_error(
              "AST JSON allocator has an unexpected mapper or iterator");
      }
      if (record.kind == "SgOmpLinearClause" ||
          record.kind == "SgOmpAlignedClause") {
        const char *role =
            record.kind == "SgOmpLinearClause" ? "step" : "alignment";
        if (const uint64_t id = singleEdgeTarget(record, role)) {
          if (!is_expression(ast.node(id)) ||
              singleEdgeTarget(ast.node(id), "parent") != record.id ||
              !children.insert(id).second)
            throw std::runtime_error("AST JSON linear/aligned operand lacks "
                                     "distinct exact ownership");
        }
      }
      const bool reduction = record.kind == "SgOmpReductionClause" ||
                             record.kind == "SgOmpInReductionClause" ||
                             record.kind == "SgOmpTaskReductionClause";
      if (ordinaryOmpVariablesClauseVariant(record.kind) &&
          (singleEdgeTarget(record, "mapper_identifier") != 0 ||
           !edgesFor(record, "iterator_definitions").empty() ||
           singleEdgeTarget(record, "sink_vectors") != 0 ||
           singleEdgeTarget(record, "alignment") != 0 ||
           singleEdgeTarget(record, "step") != 0 ||
           singleEdgeTarget(record, "user_defined_modifier") != 0 ||
           singleEdgeTarget(record, "user_defined_identifier") != 0))
        throw std::runtime_error("AST JSON ordinary variable clause has an "
                                 "unexpected transfer payload");
      const char *name_role =
          reduction ? "user_defined_identifier" : "mapper_identifier";
      if (const uint64_t name_id = singleEdgeTarget(record, name_role)) {
        const auto &name = ast.node(name_id);
        if (name.kind != "SgOmpNameExpression" ||
            name.properties.requiredString("spelling").empty() ||
            singleEdgeTarget(name, "parent") != record.id ||
            !children.insert(name_id).second)
          throw std::runtime_error(
              "AST JSON transfer mapper lacks an exact owned name");
      }
      if (reduction && (!edgesFor(record, "iterator_definitions").empty() ||
                        singleEdgeTarget(record, "mapper_identifier") != 0 ||
                        singleEdgeTarget(record, "sink_vectors") != 0))
        throw std::runtime_error(
            "AST JSON reduction has an unexpected iterator payload");
      for (const auto &edge : edgesFor(record, "iterator_definitions"))
        if (edge.target == 0 ||
            ast.node(edge.target).kind != "SgOmpIteratorDefinition" ||
            singleEdgeTarget(ast.node(edge.target), "parent") != record.id ||
            !children.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON transfer iterator is null, aliased, or misowned");
    } else if (record.kind == "SgOmpContextSelectorProperty") {
      std::unordered_set<uint64_t> operands;
      for (const char *role : {"expression", "requires_expression"}) {
        if (const uint64_t id = singleEdgeTarget(record, role))
          if (!is_expression(ast.node(id)) ||
              singleEdgeTarget(ast.node(id), "parent") != record.id ||
              !operands.insert(id).second)
            throw std::runtime_error("AST JSON context property operands lack "
                                     "distinct exact ownership");
      }
    } else if (record.kind == "SgOmpContextSelector") {
      std::unordered_set<uint64_t> children;
      for (const auto &[role, kind] :
           {std::pair{"score", V_SgExpression},
            std::pair{"construct_directive", V_SgStatement}}) {
        if (const uint64_t id = singleEdgeTarget(record, role)) {
          const auto &child = ast.node(id);
          const bool typed = kind == V_SgExpression ? is_expression(child)
                                                    : is_statement(child);
          if (!typed || singleEdgeTarget(child, "parent") != record.id ||
              !children.insert(id).second)
            throw std::runtime_error("AST JSON context selector operands lack "
                                     "distinct exact ownership");
        }
      }
      for (const auto &edge : edgesFor(record, "properties")) {
        if (edge.target == 0)
          throw std::runtime_error("AST JSON context selector properties lack "
                                   "distinct exact ownership");
        const auto &child = ast.node(edge.target);
        if (child.kind != "SgOmpContextSelectorProperty" ||
            singleEdgeTarget(child, "parent") != record.id)
          throw std::runtime_error("AST JSON context selector properties lack "
                                   "distinct exact ownership");
        if (!children.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON OpenMP context selector has a duplicate property");
        const bool indexed =
            context_owners.emplace(edge.target, record.id).second;
        ROSE_ASSERT(indexed);
      }
    } else if (record.kind == "SgOmpContextSelectorSet" ||
               record.kind == "SgOmpWhenClause" ||
               record.kind == "SgOmpMatchClause") {
      const bool selector_set = record.kind == "SgOmpContextSelectorSet";
      const char *role = selector_set ? "selectors" : "context_selector_sets";
      const VariantT kind =
          selector_set ? V_SgOmpContextSelector : V_SgOmpContextSelectorSet;
      std::unordered_set<uint64_t> children;
      for (const auto &edge : edgesFor(record, role)) {
        if (edge.target == 0)
          throw std::runtime_error(
              "AST JSON context hierarchy list lacks distinct exact ownership");
        const auto &child = ast.node(edge.target);
        if (child.kind != roseGlobalVariantNameList[kind] ||
            singleEdgeTarget(child, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON context hierarchy list lacks distinct exact ownership");
        if (!children.insert(edge.target).second)
          throw std::runtime_error(selector_set
                                       ? "AST JSON OpenMP context selector set "
                                         "has a duplicate selector"
                                       : "AST JSON OpenMP variant clause has a "
                                         "duplicate selector set");
        const bool indexed =
            context_owners.emplace(edge.target, record.id).second;
        ROSE_ASSERT(indexed);
      }
      if (record.kind == "SgOmpWhenClause")
        if (const uint64_t id = singleEdgeTarget(record, "variant_directive"))
          if (!is_statement(ast.node(id)) ||
              singleEdgeTarget(ast.node(id), "parent") != record.id ||
              !children.insert(id).second)
            throw std::runtime_error(
                "AST JSON OpenMP variant has no exact owned directive");
    } else if (record.kind == "SgOmpScheduleClause" ||
               record.kind == "SgOmpDistScheduleClause") {
      if (const uint64_t id = singleEdgeTarget(record, "chunk_size")) {
        const auto &chunk = ast.node(id);
        if (!is_expression(chunk) ||
            singleEdgeTarget(chunk, "parent") != record.id)
          throw std::runtime_error(
              "AST JSON schedule chunk lacks an exact owned expression");
      }
    } else if (record.kind == "SgOmpDestroyClause") {
      if (const uint64_t expression_id =
              singleEdgeTarget(record, "expression")) {
        if (singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON OpenMP destroy operand has no exact owning clause");
      }
    } else if (record.kind == "SgOmpMapItem" ||
               record.kind == "SgOmpInductionItem") {
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expression");
      if (singleEdgeTarget(ast.node(expression_id), "parent") != record.id)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " expression has no exact owning item");
      if (record.kind == "SgOmpMapItem") {
        std::unordered_set<uint64_t> identities;
        for (const auto &edge : edgesFor(record, "policies"))
          if (edge.target == 0 ||
              ast.node(edge.target).kind != "SgOmpMapDistDataPolicy" ||
              singleEdgeTarget(ast.node(edge.target), "parent") != record.id ||
              !identities.insert(edge.target).second)
            throw std::runtime_error(
                "AST JSON map policy is null, aliased, or misowned");
      }
    } else if (record.kind == "SgOmpAppendArgsClause" ||
               record.kind == "SgOmpInitModifierList") {
      const bool append = record.kind == "SgOmpAppendArgsClause";
      const auto children =
          edgesFor(record, append ? "interop_operations" : "modifiers");
      if (append && children.empty())
        throw std::runtime_error(
            "AST JSON SgOmpAppendArgsClause has no interop operations");
      std::unordered_set<uint64_t> identities;
      for (const auto &edge : children) {
        if (edge.target == 0 ||
            ast.node(edge.target).kind !=
                (append ? "SgOmpAppendArgsOperation" : "SgOmpInitModifier") ||
            singleEdgeTarget(ast.node(edge.target), "parent") != record.id ||
            !identities.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON reconstructed malformed " + record.kind + ": " +
              (append ? "append_args operation is null, aliased, or misowned"
                      : "modifier is null, aliased, or misowned"));
      }
    } else if (record.kind == "SgOmpInductionClause" ||
               record.kind == "SgOmpApplyClause") {
      const bool induction = record.kind == "SgOmpInductionClause";
      const char *role = induction ? "items" : "transformations";
      const char *kind =
          induction ? "SgOmpInductionItem" : "SgOmpApplyTransformation";
      std::unordered_set<uint64_t> identities;
      for (const auto &edge : edgesFor(record, role))
        if (edge.target == 0 || ast.node(edge.target).kind != kind ||
            singleEdgeTarget(ast.node(edge.target), "parent") != record.id ||
            !identities.insert(edge.target).second)
          throw std::runtime_error(
              "AST JSON " + record.kind +
              " requires distinct exactly owned typed children");
    } else if (record.kind == "SgOmpAdjustArgsClause") {
      const std::string context =
          "AST JSON reconstructed malformed " + record.kind + ": ";
      const uint64_t arguments_id = singleEdgeTarget(record, "arguments");
      if (arguments_id == 0 || ast.node(arguments_id).kind != "SgExprListExp" ||
          singleEdgeTarget(ast.node(arguments_id), "parent") != record.id ||
          edgesFor(ast.node(arguments_id), "expressions").empty())
        throw std::runtime_error(
            context +
            "adjust_args argument list is missing, empty, or misowned");
      std::unordered_set<uint64_t> arguments;
      for (const auto &edge : edgesFor(ast.node(arguments_id), "expressions")) {
        if (edge.target == 0 || !arguments.insert(edge.target).second ||
            singleEdgeTarget(ast.node(edge.target), "parent") != arguments_id)
          throw std::runtime_error(
              context + "adjust_args argument is null, aliased, or misowned");
        if (!is_expression(ast.node(edge.target)) ||
            validatedExpressionTypeProperty(ast.node(edge.target)) == nullptr)
          throw std::runtime_error(
              context +
              "adjust_args argument has no semantic value-expression role");
      }
    } else if (record.kind == "SgOmpInitClause" ||
               record.kind == "SgOmpAppendArgsOperation") {
      const uint64_t modifier_id =
          requiredSingleEdgeTarget(record, "modifier_list");
      if (ast.node(modifier_id).kind != "SgOmpInitModifierList" ||
          singleEdgeTarget(ast.node(modifier_id), "parent") != record.id)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " has no exact owned modifier list");
      if (record.kind == "SgOmpInitClause") {
        const uint64_t operand_id = requiredSingleEdgeTarget(record, "operand");
        if (operand_id == modifier_id ||
            singleEdgeTarget(ast.node(operand_id), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON OpenMP init operand has no exact clause owner");
        if (!is_expression(ast.node(operand_id)) ||
            validatedExpressionTypeProperty(ast.node(operand_id)) == nullptr)
          throw std::runtime_error(
              "AST JSON reconstructed malformed SgOmpInitClause: operand has "
              "no semantic value-expression role");
      }
    } else if (record.kind == "SgOmpIteratorDefinition") {
      const uint64_t name_id =
          requiredSingleEdgeTarget(record, "iterator_name");
      if (ast.node(name_id).kind != "SgOmpNameExpression")
        throw std::runtime_error(
            "AST JSON OpenMP iterator name has the wrong node kind");
      std::unordered_set<uint64_t> children;
      for (uint64_t id : {singleEdgeTarget(record, "iterator_type"), name_id,
                          requiredSingleEdgeTarget(record, "begin"),
                          requiredSingleEdgeTarget(record, "end"),
                          singleEdgeTarget(record, "step")}) {
        if (id != 0 && (!children.insert(id).second ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id))
          throw std::runtime_error(
              "AST JSON OpenMP iterator fields lack distinct exact ownership");
      }
    } else if (record.kind == "SgOmpApplyTransformation") {
      const uint64_t argument_id = singleEdgeTarget(record, "argument");
      const uint64_t nested_id = singleEdgeTarget(record, "nested_apply");
      if (argument_id != 0 && argument_id == nested_id)
        throw std::runtime_error(
            "AST JSON OpenMP apply operands share one child");
      for (uint64_t id : {argument_id, nested_id}) {
        if (id != 0 && singleEdgeTarget(ast.node(id), "parent") != record.id)
          throw std::runtime_error(
              "AST JSON OpenMP apply operand has no exact owner");
      }
      if (nested_id != 0 && ast.node(nested_id).kind != "SgOmpApplyClause")
        throw std::runtime_error(
            "AST JSON OpenMP nested apply has the wrong node kind");
    } else if (record.kind == "SgOmpInitModifier" ||
               record.kind == "SgOmpMapDistDataPolicy") {
      if (const uint64_t id = singleEdgeTarget(record, "expression")) {
        if (singleEdgeTarget(ast.node(id), "parent") != record.id)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " expression has no exact owner");
      }
    }
    if (nodes.count(record.id) != 0)
      continue;
    if (is_expression(record))
      (void)validatedExpressionTypeProperty(record);
    if (!requiresDelayedRebuild(record) && !is_expression(record))
      continue;
    if (construction_fields.count(record.id) == 0)
      construction_fields.emplace(
          record.id,
          RecordConstructionFields{validateConstructionFlags(record), {}});
    if (record.kind == "SgVarRefExp" || record.kind == "SgNonrealRefExp")
      (void)record.properties.requiredBool("is_non_odr_use");
    if (record.kind == "SgAlignmentSpecifier")
      (void)alignmentSpellingFromProperties(
          record.properties, singleEdgeTarget(record, "operand") != 0);
    prepareRecordPublicationFields(record, construction_fields.at(record.id));
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgDesignator") {
      auto prepared =
          construction_fields
              .try_emplace(record.id,
                           RecordConstructionFields{
                               validateConstructionFlags(record), {}})
              .first;
      if (!prepared->second.designator)
        prepareRecordPublicationFields(record, prepared->second);
      const auto &input = *prepared->second.designator;
      const auto &list = ast.node(input.list);
      const auto children = edgesFor(list, "expressions");
      if (list.kind != "SgExprListExp" ||
          std::count_if(children.begin(), children.end(),
                        [&](const EdgeRecord &edge) {
                          return edge.target == record.id;
                        }) != 1)
        throw std::runtime_error("AST JSON designator has no exact list owner");
      for (uint64_t id : {input.first, input.second})
        if (id != 0 && (!is_expression(ast.node(id)) ||
                        singleEdgeTarget(ast.node(id), "parent") != record.id))
          throw std::runtime_error(
              "AST JSON designator has invalid operand kind or ownership");
      if (input.kind == SgDesignator::e_designator_field &&
          ast.node(input.first).kind != "SgVarRefExp")
        throw std::runtime_error(
            "AST JSON field designator requires a variable reference");
    }
    if (record.kind != "SgExprListExp")
      continue;
    std::unordered_set<uint64_t> expressions;
    for (const auto &edge : edgesFor(record, "expressions")) {
      const NodeRecord &child = ast.node(edge.target);
      if (!is_expression(child))
        throw std::runtime_error(
            "AST JSON expression list has a non-expression child");
      if (!expressions.insert(edge.target).second ||
          singleEdgeTarget(child, "parent") != record.id)
        throw std::runtime_error(
            "AST JSON expression list has conflicting ownership");
    }
  }
  for (const NodeRecord &record : ast.nodes)
    if (record.kind == "SgTemplateDeclaration" &&
        singleEdgeTarget(record, "scope") == 0 &&
        detached_template_identities.count(record.id) == 0)
      throw std::runtime_error(
          "AST JSON scope-free template declaration has no exact parameter "
          "identity");
  for (const NodeRecord &record : ast.nodes) {
    auto validate_scope = [&](uint64_t id, bool valid_owner,
                              const char *context) {
      const NodeRecord &scope = ast.node(id);
      if (!valid_owner || scope.kind != "SgDeclarationScope")
        throw std::runtime_error(std::string("AST JSON ") + context +
                                 " has invalid typed endpoints");
      if (singleEdgeTarget(scope, "parent") != record.id)
        throw std::runtime_error(std::string("AST JSON ") + context +
                                 " parent disagrees with its typed owner");
    };
    const uint64_t nonreal_scope =
        singleEdgeTarget(record, "nonreal_decl_scope");
    const uint64_t function_scope =
        singleEdgeTarget(record, "function_declarator_scope");
    if (const uint64_t lambda_scope = singleEdgeTarget(record, "closure_scope"))
      validate_scope(lambda_scope, record.kind == "SgLambdaExp",
                     "lambda closure scope");
    if (record.kind == "SgDeclarationScope") {
      const uint64_t outer = singleEdgeTarget(record, "semantic_outer_scope");
      if (outer != 0) {
        if (record.properties.requiredBool("is_default_nonreal_scope"))
          throw std::runtime_error(
              "AST JSON default nonreal scope has a semantic outer scope");
        std::unordered_set<uint64_t> parents;
        for (uint64_t owner = outer; owner != 0;
             owner = singleEdgeTarget(ast.node(owner), "parent")) {
          if (owner == record.id || !parents.insert(owner).second)
            throw std::runtime_error(
                "AST JSON declaration scope has a descendant semantic owner");
        }
      }
      std::unordered_set<uint64_t> seen{record.id};
      const NodeRecord *current = &record;
      while (const uint64_t outer = declarationScopeOuterId(ast, *current)) {
        if (!seen.insert(outer).second ||
            !kindInVariants(ast.node(outer).kind, scope_variants))
          throw std::runtime_error(
              "AST JSON declaration scope has an invalid semantic outer scope");
        current = &ast.node(outer);
        if (current->kind != "SgDeclarationScope")
          break;
      }
    }
    if (nonreal_scope != 0)
      validate_scope(nonreal_scope,
                     kindInVariants(record.kind, declaration_variants),
                     "nonreal declaration scope");
    if (function_scope != 0)
      validate_scope(function_scope,
                     isFunctionDeclarationRecordKind(record.kind),
                     "function declarator scope");
    if (nonreal_scope != 0 &&
        (nonreal_scope == function_scope ||
         nonreal_scope == singleEdgeTarget(record, "source_declarator_scope")))
      throw std::runtime_error(
          "AST JSON declaration scope repeats distinct ownership roles");
    if (record.kind == "SgBasicBlock" &&
        singleEdgeTarget(record, "statement_expression_semantic_scope") != 0 &&
        singleEdgeTarget(record, "implied_do_semantic_scope") != 0)
      throw std::runtime_error(
          "AST JSON SgBasicBlock has multiple typed expression semantic roles");
  }
  for (const NodeRecord &record : ast.nodes) {
    const auto prepared = construction_fields.find(record.id);
    if (prepared != construction_fields.end() &&
        prepared->second.context_selector) {
      const auto &fields = *prepared->second.context_selector;
      validateOmpContextSelectorShape(
          fields.kind, fields.implementation_name,
          singleEdgeTarget(record, "score") != 0,
          singleEdgeTarget(record, "construct_directive") != 0,
          edgesFor(record, "properties").size());
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgOmpContextSelectorSet") {
      std::set<OmpContextSelectorIdentity> identities;
      const auto selectors = edgesFor(record, "selectors");
      for (const auto &edge : selectors) {
        OmpContextSelectorFields fields;
        std::optional<VariantT> construct_kind;
        if (nodes.count(edge.target) != 0) {
          const auto *selector =
              nodeByIdAs<SgOmpContextSelector>(nodes, edge.target);
          fields = {selector->get_selector_kind(),
                    selector->get_implementation_defined_name()};
          if (const auto *directive = selector->get_construct_directive())
            construct_kind = directive->variantT();
        } else {
          const auto &prepared =
              construction_fields.at(edge.target).context_selector;
          ROSE_ASSERT(prepared.has_value());
          fields = *prepared;
          if (const uint64_t id = singleEdgeTarget(ast.node(edge.target),
                                                   "construct_directive")) {
            const auto &kind = ast.node(id).kind;
            const auto variant = std::find_if(
                statement_variants.begin(), statement_variants.end(),
                [&](VariantT candidate) {
                  return kind == roseGlobalVariantNameList[candidate];
                });
            ROSE_ASSERT(variant != statement_variants.end());
            construct_kind = *variant;
          }
        }
        if (!identities
                 .insert(ompContextSelectorIdentity(
                     fields.kind, fields.implementation_name, construct_kind))
                 .second)
          throw std::runtime_error(
              "AST JSON OpenMP context selector set has a duplicate selector");
        if (fields.kind == SgOmpClause::e_omp_context_trait_kind) {
          const auto properties = edgesFor(ast.node(edge.target), "properties");
          bool has_any = false;
          for (const auto &property : properties) {
            SgOmpClause::omp_when_context_kind_enum kind;
            if (nodes.count(property.target) != 0) {
              kind = nodeByIdAs<SgOmpContextSelectorProperty>(nodes,
                                                              property.target)
                         ->get_context_kind();
            } else {
              const auto &prepared =
                  construction_fields.at(property.target).context_property;
              ROSE_ASSERT(prepared.has_value());
              kind = prepared->kind;
            }
            has_any =
                has_any || kind == SgOmpClause::e_omp_when_context_kind_any;
          }
          validateOmpContextKindAny(has_any, properties.size(),
                                    selectors.size());
        }
      }
    } else if (record.kind == "SgOmpWhenClause" ||
               record.kind == "SgOmpMatchClause") {
      std::set<SgOmpClause::omp_context_selector_set_kind_enum> kinds;
      for (const auto &edge : edgesFor(record, "context_selector_sets")) {
        SgOmpClause::omp_context_selector_set_kind_enum kind;
        if (nodes.count(edge.target) != 0) {
          kind = nodeByIdAs<SgOmpContextSelectorSet>(nodes, edge.target)
                     ->get_set_kind();
        } else {
          const auto &prepared =
              construction_fields.at(edge.target).context_set_kind;
          ROSE_ASSERT(prepared.has_value());
          kind = *prepared;
        }
        if (!kinds.insert(kind).second)
          throw std::runtime_error(
              "AST JSON OpenMP variant clause has a duplicate selector set");
      }
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    const auto prepared = construction_fields.find(record.id);
    if (prepared == construction_fields.end())
      continue;
    const auto &fields = prepared->second;
    if (!fields.context_property && !fields.context_selector &&
        !fields.context_set_kind)
      continue;
    const uint64_t owner_id = singleEdgeTarget(record, "parent");
    if (owner_id == 0)
      continue;
    const auto &owner = ast.node(owner_id);
    bool owner_type_valid;
    const char *message;
    if (fields.context_property) {
      owner_type_valid = owner.kind == "SgOmpContextSelector";
      message = "AST JSON context property has no exact selector owner";
    } else if (fields.context_selector) {
      owner_type_valid = owner.kind == "SgOmpContextSelectorSet";
      message = "AST JSON context selector has no exact set owner";
    } else {
      owner_type_valid =
          owner.kind == "SgOmpWhenClause" || owner.kind == "SgOmpMatchClause";
      message = "AST JSON context selector set has no exact clause owner";
    }
    const auto indexed_owner = context_owners.find(record.id);
    if (!owner_type_valid || indexed_owner == context_owners.end() ||
        indexed_owner->second != owner_id)
      throw std::runtime_error(message);
    if (fields.context_property) {
      SgOmpClause::omp_context_trait_selector_kind_enum selector_kind;
      if (nodes.count(owner_id) != 0) {
        selector_kind = nodeByIdAs<SgOmpContextSelector>(nodes, owner_id)
                            ->get_selector_kind();
      } else {
        const auto &selector =
            construction_fields.at(owner_id).context_selector;
        ROSE_ASSERT(selector.has_value());
        selector_kind = selector->kind;
      }
      const auto &property = *fields.context_property;
      validateOmpContextPropertySelectorKind(
          selector_kind, singleEdgeTarget(record, "expression") != 0,
          property.kind, property.vendor, property.atomic_order,
          property.requires_kind);
    } else if (fields.context_selector) {
      SgOmpClause::omp_context_selector_set_kind_enum set_kind;
      if (nodes.count(owner_id) != 0) {
        set_kind = nodeByIdAs<SgOmpContextSelectorSet>(nodes, owner_id)
                       ->get_set_kind();
      } else {
        const auto &kind = construction_fields.at(owner_id).context_set_kind;
        ROSE_ASSERT(kind.has_value());
        set_kind = *kind;
      }
      validateOmpContextTraitInSet(set_kind, fields.context_selector->kind);
    }
  }
  return construction_fields;
}

static void publishRecordedNode(const NodeRecord &record, SgNode *node,
                                NodeMap &nodes,
                                const RecordConstructionFields &prepared) {
  SgNode::SourceStateConstruction construction;
  if (node == nullptr) {
    throw std::runtime_error("AST JSON null delayed node " + record.kind);
  }
  if (auto *expression = isSgExpression(node);
      expression != nullptr && isJsonScalarLiteralKind(record.kind)) {
    ROSE_ASSERT(prepared.expression.has_value());
    const auto &fields = *prepared.expression;
    if (expression->get_lvalue() != fields.lvalue ||
        expression->get_need_paren() != fields.need_paren ||
        expression->get_global_qualified_name() != fields.global_qualified ||
        expression->get_semantic_wrapper_mask() != fields.wrappers ||
        expression->get_fortran_integer_constant_value() !=
            fields.folded_value ||
        expression->get_fortran_integer_constant_value_is_available() !=
            fields.folded_value_available)
      throw std::runtime_error(
          "AST JSON literal lost its constructed expression metadata");
  }
  if (!nodes.emplace(record.id, node).second) {
    throw std::runtime_error("AST JSON duplicate delayed node " + record.kind);
  }
}

static void publishConstructorOnlyNode(const NodeRecord &record, SgNode *node,
                                       NodeMap &nodes,
                                       SourcePositions positions,
                                       RecordConstructionFields &fields) {
  auto *expression = isSgExpression(node);
  auto *statement = isSgStatement(node);
  auto *declaration = isSgDeclarationStatement(node);
  auto *clause = isSgOmpClause(node);
  ROSE_ASSERT(fields.expression.has_value() ==
              (expression != nullptr && !isJsonScalarLiteralKind(record.kind)));
  ROSE_ASSERT(fields.statement.has_value() == (statement != nullptr));
  ROSE_ASSERT(fields.declaration.has_value() == (declaration != nullptr));
  ROSE_ASSERT(fields.clause.has_value() == (clause != nullptr));
  SgNode::SourceStateConstruction construction;
  installSourcePositions(node, std::move(positions), record.kind);
  SourceDeclaratorScopeConstruction source_scope(record, node, nodes);
  if (fields.declaration)
    fields.declaration->initialize(declaration);
  if (fields.statement)
    fields.statement->initialize(statement);
  if (fields.clause)
    fields.clause->initialize(clause);
  if (fields.expression)
    fields.expression->initialize(expression, nodes);
  fields.flags.initialize(node);
  publishRecordedNode(record, node, nodes, fields);
  source_scope.publish();
}

static void validateDefinitionOwners(const AstFileRecord &ast,
                                     const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      continue;
    }
    if (auto *definition = isSgFunctionDefinition(restored_node->second)) {
      const uint64_t owner = requiredSingleEdgeTarget(record, "parent");
      auto *declaration = nodeByIdAs<SgFunctionDeclaration>(nodes, owner);
      const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
      auto *body = nodeByIdAs<SgBasicBlock>(nodes, body_id);
      if (declaration->get_definition() != definition ||
          definition->get_declaration() != declaration ||
          definition->get_parent() != declaration ||
          definition->get_body() != body || body->get_parent() != definition) {
        throw std::runtime_error(
            "AST JSON function definition has inconsistent ownership");
      }
      continue;
    }
    SgClassDefinition *definition = isSgClassDefinition(restored_node->second);
    if (definition == nullptr) {
      continue;
    }
    const uint64_t owner_id = requiredSingleEdgeTarget(record, "parent");
    SgClassDeclaration *declaration =
        nodeByIdAs<SgClassDeclaration>(nodes, owner_id);
    if (requiredSingleEdgeTarget(record, "declaration") != owner_id ||
        requiredSingleEdgeTarget(ast.node(owner_id), "definition") !=
            record.id ||
        declaration->get_definition() != definition ||
        definition->get_declaration() != declaration ||
        definition->get_parent() != declaration) {
      throw std::runtime_error(
          "AST JSON class definition disagrees with its canonical typed "
          "owner");
    }
  }
}

namespace {
struct ArrayConstruction;

struct SymbolTableBinding {
  uint64_t scope_id;
  const SerializedSymbolTableEntry *entry;
};
struct SymbolBindingIndex {
  SerializedSymbolTableRecords tables;
  std::unordered_map<uint64_t, std::vector<SymbolTableBinding>> by_basis;
};

SymbolBindingIndex indexSymbolBindings(const AstFileRecord &ast) {
  SymbolBindingIndex bindings;
  std::unordered_set<uint64_t> namelist_groups;
  for (const NodeRecord &record : ast.nodes) {
    if (record.properties.find("symbol_table") == nullptr)
      continue;
    const auto inserted = bindings.tables.emplace(
        record.id, symbolTableEntriesFromRecord(record));
    ROSE_ASSERT(inserted.second);
    for (const auto &decoded : inserted.first->second) {
      if (decoded.visibility)
        continue;
      if (decoded.symbol_kind == V_SgNamelistSymbol) {
        rose_hash_multimap names;
        names.set_case_insensitive_semantics(
            record.properties.requiredBool("case_insensitive"));
        for (const auto &prior : inserted.first->second) {
          if (&prior == &decoded)
            break;
          if (prior.symbol_kind == V_SgNamelistSymbol &&
              names.key_eq()(prior.entry_name, decoded.entry_name))
            throw std::runtime_error(
                "AST JSON NAMELIST name has multiple canonical symbols");
        }
        for (auto id : decoded.namelist_declarations) {
          const auto &group = ast.node(id);
          const auto &owner =
              ast.node(requiredSingleEdgeTarget(group, "parent"));
          if (group.kind != "SgNameGroup" ||
              owner.kind != "SgNamelistStatement" ||
              requiredSingleEdgeTarget(owner, "parent") != record.id ||
              !names.key_eq()(decoded.entry_name,
                              group.properties.requiredName("group_name")) ||
              !namelist_groups.insert(id).second)
            throw std::runtime_error(
                "AST JSON NAMELIST symbol has conflicting declarations");
        }
      }
      const uint64_t basis = decoded.basis_id.value();
      if (basis != 0)
        bindings.by_basis[basis].push_back({record.id, &decoded});
    }
  }
  for (const auto &record : ast.nodes)
    if (record.kind == "SgNameGroup" && namelist_groups.count(record.id) == 0)
      throw std::runtime_error(
          "AST JSON NAMELIST declaration has no canonical symbol");
  using Entry = SerializedSymbolTableEntry;
  std::vector<const Entry *> entries;
  std::unordered_map<const Entry *, size_t> indices;
  for (const auto &scope : bindings.tables)
    for (const auto &entry : scope.second) {
      indices.emplace(&entry, entries.size());
      entries.push_back(&entry);
    }
  std::vector<std::optional<size_t>> dependencies(entries.size());
  for (const auto &scope : bindings.tables) {
    for (const auto &entry : scope.second) {
      if (!entry.visibility || entry.visibility->target_binding == nullptr)
        continue;
      const auto &binding = *entry.visibility->target_binding;
      const auto scope_id = binding.requiredNonnegativeNodeId("binding_scope");
      const auto target_scope = bindings.tables.find(scope_id);
      if (target_scope == bindings.tables.end())
        throw std::runtime_error(
            "AST JSON alias target scope has no symbol table");
      const auto &encoded = ast.node(scope_id).properties.at("symbol_table");
      const auto target = exactSymbolBindingEntryIndex(binding, encoded);
      dependencies[indices.at(&entry)] =
          indices.at(&target_scope->second.at(target));
    }
  }
  symbolDependencyOrder(dependencies);
  return bindings;
}
} // namespace

static void drainDelayedDeclarations(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    std::vector<const NodeRecord *> &pending, NodeMap &nodes,
    RecordConstructionFieldMap &construction_fields, bool require_complete,
    const SymbolConstructionAdvance &symbols = {});

static void constructRecordedNodes(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    NodeMap &nodes, const std::vector<const NodeRecord *> &records,
    NodeIds &node_ids,
    std::vector<const NodeRecord *> *delayed_declarations = nullptr,
    const SymbolConstructionAdvance &advance_symbols = {},
    ArrayConstruction *source_construction = nullptr,
    RecordConstructionFieldMap *prepared_construction_fields = nullptr);

namespace {
void collectDelayedDeclarationDependencies(
    const AstFileRecord &ast, const NodeRecord &record, const NodeMap &nodes,
    std::unordered_set<uint64_t> &ids, std::unordered_set<uint64_t> &symbol_ids,
    std::unordered_set<uint64_t> &type_ids);
}

static bool constructTemplateClassIdentityInputs(
    const AstFileRecord &ast, const NodeMap &nodes, uint64_t id,
    const std::function<bool(uint64_t)> &construct_dependency) {
  auto *declaration = nodeByIdAs<SgTemplateClassDeclaration>(nodes, id);
  const NodeRecord &record = ast.node(id);
  SgTemplateParameterPtrList parameters;
  SgTemplateArgumentPtrList arguments;
  for (const EdgeRecord &edge : edgesFor(record, "templateParameters")) {
    if (!construct_dependency(edge.target))
      return false;
    auto *parameter = nodeByIdAs<SgTemplateParameter>(nodes, edge.target);
    if (parameter->get_parent() != declaration)
      throw std::runtime_error("AST JSON template class parameter has no "
                               "exact declaration owner");
    parameters.push_back(parameter);
  }
  for (const EdgeRecord &edge :
       edgesFor(record, "templateSpecializationArguments")) {
    if (!construct_dependency(edge.target))
      return false;
    auto *argument = nodeByIdAs<SgTemplateArgument>(nodes, edge.target);
    if (argument->get_parent() != declaration)
      throw std::runtime_error(
          "AST JSON template class specialization argument has no exact "
          "declaration owner");
    arguments.push_back(argument);
  }
  if (declaration->get_templateParameters().empty() &&
      declaration->get_templateSpecializationArguments().empty()) {
    declaration->get_templateParameters().swap(parameters);
    declaration->get_templateSpecializationArguments().swap(arguments);
  } else if (declaration->get_templateParameters() != parameters ||
             declaration->get_templateSpecializationArguments() != arguments) {
    throw std::runtime_error(
        "AST JSON template class has conflicting canonical identity operands");
  }
  return true;
}

static bool constructTemplateVariableDeclarator(
    const AstFileRecord &ast, NodeMap &nodes, uint64_t id,
    const std::function<bool(uint64_t)> &construct_dependency,
    ArrayConstruction *construction = nullptr);

static void restoreClassTypeConstructionInputs(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    NodeMap &nodes, std::vector<const NodeRecord *> &delayed_declarations,
    RecordConstructionFieldMap &construction_fields) {
  auto node_ids = mappedNodePointers(nodes);

  enum class RestoreState { Visiting, Complete };
  std::unordered_map<uint64_t, RestoreState> argument_states;
  std::unordered_map<uint64_t, RestoreState> class_states;
  std::unordered_map<uint64_t, RestoreState> dependency_states;
  std::vector<uint64_t> dependency_stack;
  std::function<void(uint64_t)> restore_argument;
  std::function<void(uint64_t)> restore_class_arguments;
  std::function<void(uint64_t)> construct_dependency;
  std::function<void(const JsonValue &)> restore_type_dependencies;

  construct_dependency = [&](uint64_t id) {
    const NodeRecord &record = ast.node(id);
    if (record.kind == "SgTemplateArgument") {
      restore_argument(id);
      return;
    }
    const bool declaration_pending =
        std::find(delayed_declarations.begin(), delayed_declarations.end(),
                  &record) != delayed_declarations.end();
    const bool variable_template =
        record.kind == "SgTemplateVariableDeclaration";
    const uint64_t parameter_owner = record.kind == "SgFunctionParameterList"
                                         ? singleEdgeTarget(record, "parent")
                                         : 0;
    const bool prototype_parameters =
        parameter_owner != 0 &&
        ast.node(parameter_owner).kind == "SgFunctionParameterTypeList";
    const bool prototype_region =
        prototype_parameters || record.kind == "SgFunctionParameterTypeList";
    if (nodes.count(id) == 0 &&
        (record.kind == "SgFunctionParameterScope" || prototype_parameters)) {
      const uint64_t owner = requiredSingleEdgeTarget(record, "parent");
      if (ast.node(owner).kind == "SgFunctionParameterTypeList")
        construct_dependency(owner);
    }
    if (nodes.count(id) != 0 && isSgExpression(nodes.at(id)) == nullptr &&
        isSgOmpClauseList(nodes.at(id)) == nullptr && !declaration_pending &&
        !variable_template && !prototype_region)
      return;
    auto state = dependency_states.find(id);
    if (state != dependency_states.end()) {
      if (state->second == RestoreState::Visiting) {
        std::ostringstream message;
        message << "AST JSON template argument has cyclic construction "
                   "dependencies: ";
        for (uint64_t dependency : dependency_stack)
          message << ast.node(dependency).kind << '#' << dependency << " -> ";
        message << record.kind << '#' << id;
        throw std::runtime_error(message.str());
      }
      return;
    }
    dependency_states.emplace(id, RestoreState::Visiting);
    dependency_stack.push_back(id);
    if (variable_template) {
      ROSE_ASSERT(constructTemplateVariableDeclarator(
          ast, nodes, id, [&](uint64_t dependency) {
            construct_dependency(dependency);
            return true;
          }));
      auto *declaration = nodeByIdAs<SgTemplateVariableDeclaration>(nodes, id);
      for (const JsonValue &argument :
           record.properties.at("semantic_template_arguments").array)
        restore_argument(
            requiredPositiveId(argument, "semantic template argument"));
      declaration->get_semanticTemplateArguments() =
          templateArgumentListFromJson(
              record.properties.at("semantic_template_arguments"), nodes,
              declaration);
      dependency_stack.pop_back();
      dependency_states[id] = RestoreState::Complete;
      return;
    }
    for (const char *field :
         {"symbol_declaration", "c_implicit_declaration",
          "semantic_function_declaration",
          "semantic_member_function_declaration",
          "resolved_function_declaration", "resolved_variable_declaration"}) {
      if (const JsonValue *value = record.properties.find(field)) {
        const int64_t target = value->asInt();
        if (target < 0)
          throw std::runtime_error(
              "AST JSON reference dependency ID must be nonnegative");
        if (target != 0)
          construct_dependency(static_cast<uint64_t>(target));
      }
    }
    // A non-type parameter is constructed from its initialized name and
    // canonical parameter. Its constraint expressions can refer back to that
    // parameter and are owned after construction.
    if ((nodes.count(id) == 0 || declaration_pending) &&
        record.kind != "SgTemplateParameter" && record.kind != "SgNonrealDecl")
      restore_type_dependencies(record.properties);
    if (declaration_pending) {
      std::unordered_set<uint64_t> dependencies, symbol_dependencies,
          type_dependencies;
      collectDelayedDeclarationDependencies(ast, record, nodes, dependencies,
                                            symbol_dependencies,
                                            type_dependencies);
      for (uint64_t dependency : dependencies) {
        construct_dependency(dependency);
      }
      if (uint64_t scope = singleEdgeTarget(record, "scope")) {
        for (SgNode *owner = nodeById(nodes, scope); owner != nullptr;
             owner = owner->get_parent()) {
          if (isSgClassDeclaration(owner))
            restore_class_arguments(node_ids.at(owner));
        }
      }
      std::vector<const NodeRecord *> pending{&record};
      drainDelayedDeclarations(ast, bindings, pending, nodes,
                               construction_fields, true);
      delayed_declarations.erase(std::find(
          delayed_declarations.begin(), delayed_declarations.end(), &record));
    } else if (record.kind == "SgFunctionParameterTypeList") {
      const uint64_t scope =
          requiredSingleEdgeTarget(record, "source_parameter_scope");
      construct_dependency(requiredSingleEdgeTarget(
          ast.node(scope), "completed_semantic_scope"));
      constructRecordedNodes(ast, bindings, nodes, {&record}, node_ids);
    } else {
      if (record.kind == "SgFunctionParameterScope")
        construct_dependency(
            requiredSingleEdgeTarget(record, "completed_semantic_scope"));
      for (const EdgeRecord &edge : record.edges) {
        if (edge.field == "parent" ||
            (record.kind == "SgInitializedName" &&
             edge.field == "variable_definition") ||
            edge.target == 0)
          continue;
        const NodeRecord &child = ast.node(edge.target);
        if (singleEdgeTarget(child, "parent") == id ||
            (nodes.count(edge.target) == 0 && requiresDelayedRebuild(child)))
          construct_dependency(edge.target);
      }
      if (nodes.count(id) == 0 || prototype_parameters ||
          record.kind == "SgExprListExp" || record.kind == "SgOmpClauseList") {
        if (record.kind == "SgCompoundLiteralExp") {
          const NodeRecord &name = ast.node(
              requiredPositiveId(record.properties.at("symbol_declaration"),
                                 "compound literal variable"));
          const NodeRecord &owner =
              ast.node(requiredSingleEdgeTarget(name, "parent"));
          construct_dependency(requiredSingleEdgeTarget(name, "initptr"));
          constructRecordedNodes(ast, bindings, nodes, {&name, &owner, &record},
                                 node_ids);
        } else {
          constructRecordedNodes(ast, bindings, nodes, {&record}, node_ids);
        }
      }
    }
    node_ids.emplace(nodeById(nodes, id), id);
    dependency_stack.pop_back();
    dependency_states[id] = RestoreState::Complete;
  };

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
    for (const char *field :
         {"source_argument_list", "node", "template_parameter"}) {
      if (const JsonValue *dependency = value.find(field)) {
        const int64_t raw_id = dependency->asInt();
        if (raw_id < 0)
          throw std::runtime_error(
              "AST JSON type dependency has a negative node ID");
        if (raw_id > 0) {
          construct_dependency(static_cast<uint64_t>(raw_id));
          if (std::string_view(field) == "source_argument_list")
            construct_dependency(requiredSingleEdgeTarget(
                ast.node(static_cast<uint64_t>(raw_id)), "source_parameters"));
        }
      }
    }
    for (const char *field : {"substitution_arguments", "template_arguments"}) {
      if (const JsonValue *arguments = value.find(field)) {
        if (arguments->kind != JsonValue::Kind::Array)
          throw std::runtime_error(std::string("AST JSON ") + field +
                                   " is not an array");
        for (const JsonValue &argument : arguments->array) {
          const int64_t id = argument.asInt();
          if (id <= 0)
            throw std::runtime_error(
                "AST JSON template argument ID must be positive");
          restore_argument(static_cast<uint64_t>(id));
        }
      }
    }
    const JsonValue *kind = value.find("kind");
    if (kind != nullptr && kind->kind == JsonValue::Kind::String) {
      const std::string type_kind = kind->asString();
      if (type_kind == "SgClassType" || type_kind == "SgTypedefType" ||
          type_kind == "SgEnumType" || type_kind == "SgNonrealType") {
        const int64_t raw_id = value.requiredInt("declaration");
        if (raw_id <= 0) {
          throw std::runtime_error(
              "AST JSON named type construction dependency has no positive "
              "declaration ID");
        }
        const uint64_t id = static_cast<uint64_t>(raw_id);
        construct_dependency(id);
        if (type_kind == "SgClassType")
          restore_class_arguments(id);
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
    const NodeRecord &record = ast.node(id);
    if (record.kind != "SgTemplateArgument")
      throw std::runtime_error(
          "AST JSON template argument has a conflicting construction state");
    if (nodes.count(id) != 0) {
      (void)nodeByIdAs<SgTemplateArgument>(nodes, id);
      if (!declarationConstructionDependencyReady(ast, nodes, id, false, false))
        throw std::runtime_error(
            "AST JSON template argument has incomplete construction inputs");
      argument_states[id] = RestoreState::Complete;
      return;
    }
    restore_type_dependencies(record.properties.at("type"));
    restore_type_dependencies(record.properties.at("source_spelled_type"));
    restore_type_dependencies(record.properties.at("expression"));
    const auto &fields = construction_fields.at(id);
    const auto &argument_fields = fields.template_argument.value();
    if (argument_fields.template_id != 0)
      construct_dependency(argument_fields.template_id);
    auto argument = constructTemplateArgument(
        record, nodes, argument_fields, fields.flags, fields.positions.value());
    auto identity = node_ids.emplace(argument.get(), id);
    if (!identity.second)
      throw std::runtime_error(
          "AST JSON template argument was published twice");
    try {
      if (!nodes.emplace(id, argument.get()).second)
        throw std::runtime_error(
            "AST JSON template argument was published twice");
    } catch (...) {
      node_ids.erase(identity.first);
      throw;
    }
    argument.release();
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
    if (isSgTemplateClassDeclaration(class_declaration))
      constructTemplateClassIdentityInputs(ast, nodes, id,
                                           [&](uint64_t dependency) {
                                             construct_dependency(dependency);
                                             return true;
                                           });
    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(class_declaration);
    if (declaration == nullptr) {
      class_states[id] = RestoreState::Complete;
      return;
    }
    const NodeRecord &record = ast.node(id);
    for (const char *field :
         {"template_arguments", "semantic_template_arguments",
          "deduced_template_arguments"}) {
      const JsonValue &list = record.properties.at(field);
      if (list.kind != JsonValue::Kind::Array)
        throw std::runtime_error("AST JSON class template argument list is "
                                 "not an array");
      for (const JsonValue &entry : list.array)
        restore_argument(requiredPositiveId(entry, field));
    }
    declaration->get_templateArguments() = templateArgumentListFromJson(
        record.properties.at("template_arguments"), nodes, declaration);
    declaration->get_semanticTemplateArguments() = templateArgumentListFromJson(
        record.properties.at("semantic_template_arguments"), nodes,
        declaration);
    declaration->get_deducedTemplateArguments() = templateArgumentListFromJson(
        record.properties.at("deduced_template_arguments"), nodes, declaration);
    class_states[id] = RestoreState::Complete;
  };

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node != nodes.end() &&
        (isSgTemplateClassDeclaration(restored_node->second) != nullptr ||
         isSgTemplateInstantiationDecl(restored_node->second) != nullptr)) {
      restore_class_arguments(record.id);
    }
    if (record.kind == "SgTemplateVariableDeclaration")
      construct_dependency(record.id);
  }

  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgNonrealDecl") {
      construct_dependency(record.id);
    }
  }

  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgTemplateArgument") {
      restore_argument(record.id);
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgVarRefExp" || record.kind == "SgNonrealRefExp" ||
        record.kind == "SgMemberFunctionRefExp" ||
        record.kind == "SgTemplateMemberFunctionRefExp")
      construct_dependency(record.id);
  }
}

static void validateVariableInlineTypeOwnership(const AstFileRecord &ast,
                                                const NodeMap &nodes) {
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

    const NodeRecord &owned_record = ast.node(owned_id);
    if (requiredSingleEdgeTarget(owned_record, "parent") !=
        variable_record.id) {
      throw std::runtime_error(
          "AST JSON variable inline type does not name its exact serialized "
          "owner");
    }
    SgDeclarationStatement *owned_declaration =
        nodeByIdAs<SgDeclarationStatement>(nodes, owned_id);
    if (owned_record.properties.requiredBool("is_autonomous_declaration")) {
      throw std::runtime_error(
          "AST JSON variable inline type is serialized as an autonomous "
          "declaration");
    }
    if (SgClassDeclaration *class_declaration =
            isSgClassDeclaration(owned_declaration)) {
      if (class_declaration->get_isAutonomousDeclaration())
        throw std::runtime_error(
            "AST JSON inline class lost its constructed ownership role");
    } else if (SgEnumDeclaration *enum_declaration =
                   isSgEnumDeclaration(owned_declaration)) {
      if (enum_declaration->get_isAutonomousDeclaration())
        throw std::runtime_error(
            "AST JSON inline enum lost its constructed ownership role");
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
      const NodeRecord &declarator_record = ast.node(edge.target);
      if (declarator_record.kind != "SgInitializedName") {
        throw std::runtime_error(
            "AST JSON variable inline type references an unknown declarator");
      }
      SgInitializedName *declarator =
          nodeByIdAs<SgInitializedName>(nodes, edge.target);
      SgType *restored_type = declarator->get_type();
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

static void validateDesignatorFieldReferenceConstructionDependencies(
    const AstFileRecord &ast, const NodeMap &nodes,
    const RecordConstructionFieldMap &prepared) {
  for (const NodeRecord &designator_record : ast.nodes) {
    if (designator_record.kind != "SgDesignator") {
      continue;
    }
    SgDesignator *designator =
        nodeByIdAs<SgDesignator>(nodes, designator_record.id);
    const auto &input = prepared.at(designator_record.id).designator;
    ROSE_ASSERT(input.has_value());
    const auto kind = input->kind;
    if (designator->get_kind() != kind) {
      throw std::runtime_error(
          "AST JSON designator construction kind disagrees with its exact "
          "serialized kind");
    }
    if (kind != SgDesignator::e_designator_field) {
      continue;
    }
    const uint64_t reference_id =
        requiredSingleEdgeTarget(designator_record, "first_expression");
    const NodeRecord &reference_record = ast.node(reference_id);
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
        static_cast<int64_t>(basis_id)) {
      throw std::runtime_error(
          "AST JSON field designator symbol declaration fields disagree");
    }
    const NodeRecord &basis_record = ast.node(basis_id);
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
    const NodeRecord &owner_record = ast.node(owner_id);
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

    if (basis->get_definition() == nullptr) {
      throw std::runtime_error(
          "AST JSON field designator named basis has no typed variable "
          "definition");
    }

    const JsonValue *basis_type_json = basis_record.properties.find("type");
    const JsonValue *reference_type_json =
        reference_record.properties.find("type");
    if (basis_type_json == nullptr || reference_type_json == nullptr) {
      throw std::runtime_error(
          "AST JSON field designator variable dependency has no serialized "
          "semantic type");
    }
    if (!basis_type_json->exactlyEquals(*reference_type_json)) {
      throw std::runtime_error(
          "AST JSON field designator reference type record differs from its "
          "exact variable basis type record");
    }
    SgType *basis_type = typeFromJson(*basis_type_json, nodes);
    if (basis->get_type() != basis_type) {
      throw std::runtime_error(
          "AST JSON field designator reference type does not name its exact "
          "variable basis type");
    }

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
    if (reference->get_symbol() != symbol ||
        reference->get_type() != basis_type) {
      throw std::runtime_error(
          "AST JSON field designator reference failed exact typed symbol "
          "publication");
    }
  }
}

void validateReconstructedTypesAndSymbols(const AstFileRecord &ast,
                                          const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;

    if (auto *reference = isSgCxxDecompositionRefExp(node))
      reference->validate();

    if (SgInitializedName *name = isSgInitializedName(node)) {
      name->validate_structured_binding();
      const auto enum_roles = initializedNameRolesFromJson(p);
      if (name->get_enum_constant_source_ownership() != enum_roles.ownership ||
          name->get_enum_constant_value() != enum_roles.enum_value) {
        throw std::runtime_error(
            "AST JSON enum-constant identity changed after "
            "construction");
      }
    }

    if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
      if (decl->get_type() == nullptr) {
        throw std::runtime_error("AST JSON SgFunctionDeclaration " +
                                 decl->get_name().getString() +
                                 " has no exact semantic function type");
      }
      SgFunctionType *function_type_syntax = decl->get_type_syntax();
      if (decl->get_type_syntax_is_available() !=
              p.requiredBool("type_syntax_is_available") ||
          (function_type_syntax != nullptr) !=
              p.at("function_type_syntax").requiredBool("present") ||
          (function_type_syntax != nullptr &&
           (function_type_syntax == decl->get_type() ||
            function_type_syntax->get_parent() != decl))) {
        throw std::runtime_error(
            "AST JSON SgFunctionDeclaration " + decl->get_name().getString() +
            " lost its constructed source function type syntax");
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
      if (decl->get_source_declarator_uses_wrapped_function_type()) {
        decl->validate_source_declarator_form();
      }
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
      if (SageInterface::containsUnknownType(
              decl->get_type()->get_return_type())) {
        throw std::runtime_error(
            "AST JSON function declaration has no exact canonical return "
            "type edge");
      }
    }

    if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
      classTypeForDeclaration(decl);
    } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
      if (typeFromJson(p.at("type"), nodes) != decl->get_type()) {
        throw std::runtime_error(
            "AST JSON nonreal declaration lost its constructed type identity");
      }
    } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
      // Re-decoding could create a different non-interned type. Validate the
      // exact base and wrapper already bound during dependency construction.
      SgType *constructed = decl->get_base_type();
      if (SageInterface::containsUnknownType(constructed)) {
        throw std::runtime_error("AST JSON typedef has no exact base type");
      }
      if (decl->get_type() == nullptr ||
          decl->get_type()->get_declaration() != decl) {
        throw std::runtime_error(
            "AST JSON typedef canonical wrapper differs from construction");
      }
    } else if (SgInitializedName *name = isSgInitializedName(node)) {
      // Re-decoding a non-interned type could allocate a different object.
      SgType *constructed = name->get_typeptr();
      if (SageInterface::containsUnknownType(constructed)) {
        throw std::runtime_error("AST JSON initialized name has no exact type");
      }
    } else if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
      const auto fields = templateParameterSourceFieldsFromJson(p);
      if (parameter->get_parameterType() != fields.kind ||
          parameter->get_templateParameterKeyword() != fields.keyword ||
          parameter->get_isAbbreviatedFunctionTemplateParameter() !=
              fields.abbreviated ||
          parameter->get_is_parameter_pack() != fields.pack)
        throw std::runtime_error(
            "AST JSON template parameter metadata differs from construction");
      const uint64_t initialized_name_id =
          p.requiredNonnegativeNodeId("initialized_name");
      if (parameter->get_parameterType() ==
          SgTemplateParameter::nontype_parameter) {
        SgInitializedName *name =
            nodeByIdAs<SgInitializedName>(nodes, initialized_name_id);
        if (parameter->get_initializedName() != name ||
            name->get_parent() != parameter ||
            parameter->get_type() != name->get_type()) {
          throw std::runtime_error(
              "AST JSON non-type parameter differs from construction");
        }
      }
      validateTemplateParameterContract(parameter, "reconstruction");
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
      if (decl->get_scope() == nullptr &&
          (decl->variantT() != V_SgTemplateDeclaration ||
           singleEdgeTarget(record, "scope") != 0)) {
        throw std::runtime_error("AST JSON " + record.kind + " id " +
                                 std::to_string(record.id) +
                                 " has no exact semantic scope");
      }
    }
    if (SgInitializedName *name = isSgInitializedName(node)) {
      if (name->get_scope() == nullptr) {
        const auto *parameter = isSgTemplateParameter(name->get_parent());
        if (parameter == nullptr ||
            parameter->get_parameterType() !=
                SgTemplateParameter::nontype_parameter ||
            parameter->get_canonicalNonTypeParameter() != parameter ||
            parameter->get_parent() != nullptr ||
            parameter->get_initializedName() != name ||
            parameter->get_type() != name->get_type() ||
            name->get_declptr() != nullptr)
          throw std::runtime_error(
              "AST JSON initialized name has neither an exact scope nor a "
              "canonical parameter owner");
      }
    }
    if (SgVarRefExp *ref = isSgVarRefExp(node)) {
      if (ref->get_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON failed to resolve SgVarRefExp symbol");
      }
      validateAnonymousDataMemberReferenceQualification(ref);
    }
  }
}

// Collect construction dependencies, including references inside inline types.
void collectTypeDependencyIds(const JsonValue &json,
                              std::unordered_set<uint64_t> &ids,
                              std::unordered_set<uint64_t> &symbol_ids,
                              std::unordered_set<uint64_t> &type_ids) {
  auto harvest_id = [&ids, &symbol_ids](const JsonValue *value,
                                        bool symbol = false) {
    if (value != nullptr) {
      const int64_t id = value->asInt();
      if (id < 0)
        throw std::runtime_error(
            "AST JSON construction dependency ID must be nonnegative");
      if (id > 0) {
        ids.insert(static_cast<uint64_t>(id));
        if (symbol)
          symbol_ids.insert(static_cast<uint64_t>(id));
      }
    }
  };
  if (json.kind == JsonValue::Kind::Object) {
    if (const auto *kind = json.find("owned_kind")) {
      if (kind->asString() == "SgEnumVal") {
        const uint64_t declaration =
            requiredPositiveId(json.at("properties").at("declaration"),
                               "enum literal declaration");
        ids.insert(declaration);
        type_ids.insert(declaration);
      } else if (kind->asString() == "SgConstructorInitializer") {
        harvest_id(&json.at("properties").at("declaration"));
      }
    }
    if (const auto *function = json.find("external_function"))
      collectExternalFunctionConstructionDependencies(*function, ids);
    harvest_id(json.find("node"));
    harvest_id(json.find("binding_scope"));
    if (json.find("source_argument_list") != nullptr) {
      const uint64_t prototype =
          json.requiredNonnegativeNodeId("source_argument_list");
      if (prototype != 0) {
        ids.insert(prototype);
        type_ids.insert(prototype);
      }
    }
    harvest_id(json.find("symbol_declaration"), true);
    harvest_id(json.find("c_implicit_declaration"));
    harvest_id(json.find("semantic_function_declaration"));
    harvest_id(json.find("semantic_member_function_declaration"));
    harvest_id(json.find("bound_temporary_destructor"));
    harvest_id(json.find("materialized_lifetime_owner"));
    harvest_id(json.find("template_parameter"));
    harvest_id(json.find("resolved_function_declaration"));
    harvest_id(json.find("resolved_variable_declaration"));
    if (const auto *specification = json.find("exception_specification")) {
      harvest_id(&specification->at("source"));
      harvest_id(&specification->at("pattern"));
    }
    if (json.find("reference_kind") != nullptr &&
        json.find("value_category") != nullptr)
      harvest_id(json.find("binding"));
    if (const auto *reference = json.find("binding_reference"))
      harvest_id(reference->find("binding"));
    if (const auto *captures = json.find("captured_bindings")) {
      if (captures->kind != JsonValue::Kind::Array)
        throw std::runtime_error(
            "AST JSON pseudo-object captures are not an array");
      for (const auto &capture : captures->array)
        harvest_id(capture.find("binding"));
    }
    for (const char *field : {"substitution_arguments", "template_arguments"}) {
      if (const JsonValue *arguments = json.find(field)) {
        if (arguments->kind != JsonValue::Kind::Array)
          throw std::runtime_error(std::string("AST JSON ") + field +
                                   " is not an array");
        for (const JsonValue &argument : arguments->array)
          harvest_id(&argument);
      }
    }
    bool names_declaration = false;
    if (const JsonValue *kind = json.find("kind")) {
      names_declaration = kind->kind == JsonValue::Kind::String &&
                          (kind->asString() == "SgTypedefType" ||
                           kind->asString() == "SgClassType" ||
                           kind->asString() == "SgNonrealType" ||
                           kind->asString() == "SgEnumType");
    }
    if (names_declaration) {
      const JsonValue *declaration = json.find("declaration");
      harvest_id(declaration);
      if (declaration != nullptr && declaration->asInt() > 0)
        type_ids.insert(static_cast<uint64_t>(declaration->asInt()));
    }
    for (const auto &field : json.object) {
      collectTypeDependencyIds(field.second, ids, symbol_ids, type_ids);
    }
  } else if (json.kind == JsonValue::Kind::Array) {
    for (const JsonValue &entry : json.array) {
      collectTypeDependencyIds(entry, ids, symbol_ids, type_ids);
    }
  }
}

namespace {
void collectDelayedDeclarationDependencies(
    const AstFileRecord &ast, const NodeRecord &record, const NodeMap &nodes,
    std::unordered_set<uint64_t> &ids, std::unordered_set<uint64_t> &symbol_ids,
    std::unordered_set<uint64_t> &type_ids) {
  if (uint64_t scope = singleEdgeTarget(record, "nonreal_decl_scope"))
    ids.insert(scope);
  if (uint64_t scope = singleEdgeTarget(record, "source_declarator_scope")) {
    ids.insert(scope);
    for (const auto &edge : edgesFor(ast.node(scope), "declarations"))
      ids.insert(edge.target);
  }
  if (record.kind == "SgUsingDeclarationStatement") {
    for (uint64_t target :
         {exactSerializedPointerTarget(record, "declaration", "declaration"),
          exactSerializedPointerTarget(record, "initialized_name",
                                       "initializedName")})
      if (target != 0)
        ids.insert(target);
    if (uint64_t scope = singleEdgeTarget(record, "scope"))
      ids.insert(scope);
    return;
  }
  if (record.kind == "SgNamespaceAliasDeclarationStatement") {
    ids.insert(record.properties.requiredInt("namespace_declaration"));
    if (uint64_t alias =
            record.properties.requiredInt("namespace_alias_declaration")) {
      ids.insert(alias);
    }
    if (uint64_t scope = singleEdgeTarget(record, "scope")) {
      ids.insert(scope);
    }
    return;
  }
  if (record.kind == "SgTemplateParameter") {
    const auto fields =
        templateParameterSourceFieldsFromJson(record.properties);
    if (fields.kind == SgTemplateParameter::nontype_parameter) {
      ids.insert(
          record.properties.requiredNonnegativeNodeId("initialized_name"));
      if (static_cast<uint64_t>(fields.canonicalNonTypeParameter) != record.id)
        ids.insert(static_cast<uint64_t>(fields.canonicalNonTypeParameter));
      if (uint64_t expression_id =
              record.properties.at("default_expression_parameter")
                  .requiredNonnegativeNodeId("node"))
        ids.insert(expression_id);
    } else if (fields.kind == SgTemplateParameter::template_parameter) {
      ids.insert(
          record.properties.requiredNonnegativeNodeId("template_declaration"));
      if (uint64_t argument_id = record.properties.requiredNonnegativeNodeId(
              "default_template_argument"))
        ids.insert(argument_id);
      if (uint64_t source_id = record.properties.requiredNonnegativeNodeId(
              "source_spelled_template_declaration"))
        ids.insert(source_id);
    }
    collectTypeDependencyIds(record.properties.at("default_type_parameter"),
                             ids, symbol_ids, type_ids);
    return;
  }
  if (record.kind == "SgTemplateTypedefDeclaration")
    for (const auto &edge : edgesFor(record, "templateParameters"))
      ids.insert(edge.target);
  if (record.kind == "SgNonrealDecl") {
    if (uint64_t declaration = singleEdgeTarget(record, "templateDeclaration"))
      ids.insert(declaration);
    if (uint64_t scope = singleEdgeTarget(record, "scope"))
      ids.insert(scope);
    for (const auto &argument : edgesFor(record, "tpl_args"))
      ids.insert(argument.target);
    collectTypeDependencyIds(record.properties.at("aliased_type"), ids,
                             symbol_ids, type_ids);
    return;
  }
  const bool function = isFunctionDeclarationRecordKind(record.kind);
  if (record.kind == "SgInitializedName" ||
      record.kind == "SgProcedureHeaderStatement")
    collectTypeDependencyIds(
        record.properties.at("fortran_source_derived_type_symbol"), ids,
        symbol_ids, type_ids);
  const JsonValue &type =
      record.properties.at(function ? "function_type"
                           : record.kind == "SgInitializedName" ? "type"
                           : record.kind == "SgEnumDeclaration" ? "field_type"
                                                                : "base_type");
  collectTypeDependencyIds(type, ids, symbol_ids, type_ids);
  if (record.kind == "SgTemplateInstantiationTypedefDeclaration" ||
      record.kind == "SgTemplateInstantiationFunctionDecl" ||
      record.kind == "SgTemplateInstantiationMemberFunctionDecl")
    for (const char *field : {"templateArguments", "deducedTemplateArguments"})
      for (const auto &edge : edgesFor(record, field))
        ids.insert(edge.target);
  if (function) {
    collectTypeDependencyIds(
        record.properties.at("declared_exception_signature"), ids, symbol_ids,
        type_ids);
    if (uint64_t clause = singleEdgeTarget(record, "trailingRequiresClause"))
      ids.insert(clause);
  }
  if (record.kind == "SgEnumDeclaration") {
    collectTypeDependencyIds(record.properties.at("promotion_type"), ids,
                             symbol_ids, type_ids);
    if (uint64_t parent = singleEdgeTarget(record, "parent");
        parent != 0 && ast.node(parent).kind == "SgAuxiliaryDeclarationList")
      ids.insert(parent);
    for (uint64_t dependency :
         {requiredSingleEdgeTarget(record, "firstNondefiningDeclaration"),
          record.properties.requiredNonnegativeNodeId("type_owner")})
      if (dependency != record.id) {
        ids.insert(dependency);
        type_ids.insert(dependency);
      }
  }
  if (record.kind == "SgInitializedName") {
    for (const char *field : {"auto_decltype", "fortran_source_type",
                              "cxx_source_type", "fortran_procedure_interface"})
      collectTypeDependencyIds(record.properties.at(field), ids, symbol_ids,
                               type_ids);
    if (uint64_t declaration = singleEdgeTarget(record, "declptr")) {
      // The owner backlink needs declaration identity, not its signature.
      // A function's VLA signature can itself depend on this parameter.
      if (nodes.find(declaration) == nodes.end())
        ids.insert(declaration);
      else
        (void)nodeByIdAs<SgDeclarationStatement>(nodes, declaration);
    }
    if (uint64_t declaration = record.properties.requiredNonnegativeNodeId(
            "fortran_separate_shape_declaration")) {
      if (nodes.find(declaration) == nodes.end())
        ids.insert(declaration);
      else
        (void)nodeByIdAs<SgStatement>(nodes, declaration);
    }
    if (uint64_t previous = singleEdgeTarget(record, "prev_decl_item"))
      ids.insert(previous);
    for (const char *field :
         {"structured_binding_scope", "structured_binding_pattern"}) {
      if (uint64_t target =
              record.properties.requiredNonnegativeNodeId(field)) {
        if (nodes.count(target) == 0)
          ids.insert(target);
      }
    }
    for (const char *field :
         {"cray_pointer_pointee", "fortran_separate_pointer_declaration",
          "ctor_initializer_field_target", "structured_binding_owner",
          "structured_binding_value", "structured_binding_holding_variable"}) {
      if (uint64_t target = record.properties.requiredNonnegativeNodeId(field))
        ids.insert(target);
    }
  }
  if (record.kind == "SgTemplateInstantiationTypedefDeclaration")
    ids.insert(requiredSingleEdgeTarget(record, "templateDeclaration"));
  if (record.kind == "SgTypedefDeclaration" ||
      record.kind == "SgTemplateTypedefDeclaration" ||
      record.kind == "SgTemplateInstantiationTypedefDeclaration") {
    if (uint64_t base_declaration = singleEdgeTarget(record, "declaration")) {
      ids.insert(base_declaration);
      const uint64_t owner =
          singleEdgeTarget(ast.node(base_declaration), "parent");
      if (owner != 0 && owner != record.id)
        ids.insert(owner);
    }
  }
  if (uint64_t scope = singleEdgeTarget(record, "scope")) {
    ids.insert(scope);
    if (record.kind != "SgInitializedName" && !function)
      type_ids.insert(scope);
  }
}
} // namespace

void validateDelayedDeclarationRecord(const NodeRecord &record) {
  validateConstructionFlags(record);
  if (record.location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + record.kind +
                             " location must be an object");
  }
  (void)record.location.at("start").requiredBool("present");
  (void)record.location.at("end").requiredBool("present");
  if (record.kind == "SgUsingDeclarationStatement") {
    (void)record.properties.requiredName("source_terminal_name");
    (void)record.properties.requiredBool("is_inheriting_constructor");
    const uint64_t declaration =
        exactSerializedPointerTarget(record, "declaration", "declaration");
    const uint64_t name = exactSerializedPointerTarget(
        record, "initialized_name", "initializedName");
    if ((declaration == 0) == (name == 0))
      throw std::runtime_error(
          "AST JSON using declaration has no exact typed target");
  } else if (record.kind == "SgNamespaceAliasDeclarationStatement") {
    (void)record.properties.requiredName("name");
    const int64_t target =
        record.properties.requiredInt("namespace_declaration");
    const int64_t alias =
        record.properties.requiredInt("namespace_alias_declaration");
    if (target <= 0 || alias < 0 ||
        record.properties.requiredBool(
            "is_alias_for_another_namespace_alias") != (alias != 0)) {
      throw std::runtime_error("AST JSON namespace alias has invalid targets");
    }
  } else if (record.kind == "SgTemplateParameter") {
    const auto fields =
        templateParameterSourceFieldsFromJson(record.properties);
    if (fields.kind == SgTemplateParameter::nontype_parameter &&
        record.properties.requiredInt("initialized_name") <= 0)
      throw std::runtime_error("AST JSON non-type parameter has no declarator");
    if (fields.kind == SgTemplateParameter::nontype_parameter &&
        singleEdgeTarget(record, "parent") == 0 &&
        (singleEdgeTarget(record, "canonicalNonTypeParameter") != record.id ||
         singleEdgeTarget(record, "initializedName") !=
             record.properties.requiredNonnegativeNodeId("initialized_name")))
      (void)requiredSingleEdgeTarget(record, "parent");
    if (singleEdgeTarget(record, "parent") == 0 &&
        fields.kind == SgTemplateParameter::nontype_parameter &&
        static_cast<uint64_t>(fields.canonicalNonTypeParameter) != record.id)
      throw std::runtime_error(
          "AST JSON detached non-type parameter is not canonical");
    (void)record.properties.at("type");
  } else if (record.kind == "SgEnumDeclaration") {
    (void)record.properties.at("field_type").requiredBool("present");
    (void)record.properties.at("promotion_type").requiredBool("present");
    if (record.properties.requiredNonnegativeNodeId("type_owner") == 0)
      throw std::runtime_error("AST JSON enum has no typed type owner");
    (void)requiredSingleEdgeTarget(record, "scope");
  } else if (record.kind == "SgNonrealDecl") {
    (void)requiredSingleEdgeTarget(record, "templateDeclaration");
  } else if (record.kind == "SgInitializedName") {
    (void)record.properties.requiredName("name");
    (void)initializedNameRolesFromJson(record.properties);
    (void)exactSerializedPointerTarget(record, "ctor_initializer_field_target",
                                       "ctor_initializer_field_target");
    if (!record.properties.at("type").requiredBool("present"))
      throw std::runtime_error(
          "AST JSON initialized-name required type is absent");
    (void)record.properties.at("auto_decltype");
    (void)record.properties.at("fortran_source_type");
    (void)record.properties.at("cxx_source_type");
    (void)initializedNameMetadataFromJson(record.properties);
    (void)record.properties.requiredBool("shape_deferred");
    (void)record.properties.requiredNonnegativeNodeId("cray_pointer_pointee");
    (void)record.properties.requiredNonnegativeNodeId(
        "fortran_separate_shape_declaration");
    (void)record.properties.requiredNonnegativeNodeId(
        "fortran_separate_pointer_declaration");
    (void)singleEdgeTarget(record, "prev_decl_item");
  } else {
    (void)record.properties.requiredName("name");
    // Base shape first, so malformed base descriptions keep their exact
    // diagnostics instead of collapsing into metadata errors.
    validateTypedefBaseShape(record.properties);
    (void)typedefMetadataFromJson(record.properties);
    if (record.kind == "SgTemplateInstantiationTypedefDeclaration") {
      (void)requiredSingleEdgeTarget(record, "templateDeclaration");
      if (record.properties.requiredName("template_name").is_null()) {
        throw std::runtime_error("AST JSON " + record.kind +
                                 " has an empty template_name");
      }
      (void)record.properties.requiredString("template_header");
    }
  }
}

std::vector<const NodeRecord *>
collectDelayedDeclarations(const AstFileRecord &ast, const NodeMap &nodes) {
  std::vector<const NodeRecord *> pending;
  for (const NodeRecord &record : ast.nodes) {
    const auto existing = nodes.find(record.id);
    if (existing != nodes.end() &&
        isSgFunctionDeclaration(existing->second) != nullptr) {
      (void)record.properties.at("function_type");
      pending.push_back(&record);
      continue;
    }
    if (record.kind != "SgInitializedName" &&
        !(record.kind == "SgNonrealDecl" &&
          singleEdgeTarget(record, "templateDeclaration") != 0) &&
        record.kind != "SgEnumDeclaration" &&
        record.kind != "SgNamespaceAliasDeclarationStatement" &&
        record.kind != "SgUsingDeclarationStatement" &&
        record.kind != "SgTypedefDeclaration" &&
        record.kind != "SgTemplateTypedefDeclaration" &&
        record.kind != "SgTemplateInstantiationTypedefDeclaration" &&
        record.kind != "SgTemplateParameter") {
      continue;
    }
    if (nodes.find(record.id) != nodes.end()) {
      throw std::runtime_error("AST JSON duplicate delayed declaration id " +
                               std::to_string(record.id));
    }
    validateDelayedDeclarationRecord(record);
    pending.push_back(&record);
  }
  std::unordered_set<uint64_t> pending_ids;
  for (const NodeRecord *pending_record : pending) {
    if (!pending_ids.insert(pending_record->id).second) {
      throw std::runtime_error("AST JSON duplicate delayed declaration id " +
                               std::to_string(pending_record->id));
    }
  }
  return pending;
}

namespace {
struct ArrayConstruction {
  NodeMap &nodes;
  NodeIds &node_ids;
  SemanticArrayConstruction arrays;
  RecordNodeConstruction ownership;
  std::vector<uint64_t> provisional;
  std::vector<std::pair<SgSymbolTable *, SgSymbol *>> symbols;
  std::vector<SgInitializedName *> initialized_names;
  std::vector<SgFunctionParameterList *> parameter_lists;
  std::vector<SgExprListExp *> expression_lists;
  std::vector<SgOmpClauseList *> clause_lists;
  std::vector<SgVariableDeclaration *> variable_declarations;
  std::vector<SgFunctionDeclaration *> source_type_owners;
  std::unordered_map<SgNode *, SgNode *> parameter_parents;
  std::unordered_map<SgClassDefinition *, SgBaseClassPtrList> inheritances;
  std::unordered_map<SgClassDefinition *, SgDeclarationStatementPtrList>
      class_members;
  std::unordered_map<SgAuxiliaryDeclarationList *,
                     SgDeclarationStatementPtrList>
      auxiliary_declarations;
  std::unordered_map<SgScopeStatement *, SgAuxiliaryDeclarationList *>
      auxiliary_owners;
  bool committed = false;

  ArrayConstruction(NodeMap &nodes, NodeIds &node_ids,
                    const NodeIds &initial_nodes)
      : nodes(nodes), node_ids(node_ids), ownership(initial_nodes) {}
  ~ArrayConstruction() {
    if (committed)
      return;
    arrays.prepareRollback();
    for (auto *name : initialized_names)
      name->set_initializer(nullptr);
    for (auto *list : parameter_lists)
      list->get_args().clear();
    for (auto *list : expression_lists)
      list->get_expressions().clear();
    for (auto *list : clause_lists)
      while (!list->get_clauses().empty())
        list->remove_clause(list->get_clauses().back());
    for (auto *declaration : variable_declarations)
      declaration->get_variables().clear();
    for (auto *function : source_type_owners) {
      function->set_type_syntax(nullptr);
      function->set_type_syntax_is_available(false);
    }
    for (auto &[owner, declarations] : auxiliary_declarations)
      owner->get_declarations().swap(declarations);
    for (auto &[owner, declarations] : class_members)
      owner->get_members().swap(declarations);
    for (auto &[owner, bases] : inheritances)
      owner->get_inheritances().swap(bases);
    for (const auto &[scope, owner] : auxiliary_owners)
      scope->set_auxiliary_declarations(owner);
    for (const auto &[node, parent] : parameter_parents)
      node->set_parent(parent);
    for (const auto &[table, symbol] : symbols)
      table->remove(symbol);
    for (uint64_t id : provisional) {
      const auto found = nodes.find(id);
      if (found != nodes.end()) {
        node_ids.erase(found->second);
        nodes.erase(found);
      }
    }
  }
  void retain(uint64_t id, SgNode *node) {
    ownership.own(node);
    if (auto *declaration = isSgTypedefDeclaration(node))
      ownership.own(declaration->get_type());
    if (auto *declaration = isSgEnumDeclaration(node))
      if (declaration->get_type()->get_declaration() == declaration)
        ownership.own(declaration->get_type());
    provisional.push_back(id);
  }
  void retain(SgSymbolTable *table, SgSymbol *symbol) {
    ownership.own(symbol);
    symbols.emplace_back(table, symbol);
  }
  void commit(SourceTypeConstruction *source_types = nullptr) {
    arrays.publish();
    if (source_types != nullptr)
      source_types->publish();
    ownership.publish();
    committed = true;
  }
};

// Construct symbols after their exact basis, before dependent references.
// Alias/rename entries require complete tables and remain on their own path.
void restoreConstructedBasisSymbols(const SymbolBindingIndex &bindings,
                                    uint64_t built_id, NodeMap &nodes,
                                    ArrayConstruction *construction) {
  SgNode *basis = nodes.at(built_id);
  const auto found = bindings.by_basis.find(built_id);
  if (found == bindings.by_basis.end())
    return;
  for (const SymbolTableBinding &binding : found->second) {
    auto scope_it = nodes.find(binding.scope_id);
    if (scope_it == nodes.end()) {
      continue;
    }
    SgScopeStatement *scope =
        nodeByIdAs<SgScopeStatement>(nodes, binding.scope_id);
    SgSymbolTable *table = scope->get_symbol_table();
    if (table == nullptr || table->get_parent() != scope) {
      throw std::runtime_error(
          "AST JSON delayed declaration symbol scope has no table");
    }
    const SerializedSymbolTableEntry &decoded = *binding.entry;
    if (decoded.symbol_kind == V_SgNamelistSymbol)
      continue; // The full declaration family is constructed by pending
                // symbols.
    if (decoded.symbol_kind == V_SgNamespaceSymbol)
      validateNamespaceSymbolKey(decoded.entry_name, basis);
    auto construct = prepareSymbolForKindAndBasis(decoded.symbol_kind, basis,
                                                  decoded.label_fields);
    std::unique_ptr<SgSymbol> symbol(construct());
    if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
      throw std::runtime_error(
          std::string(
              "AST JSON cannot reconstruct delayed declaration symbol entry"
              " kind=") +
          getVariantName(decoded.symbol_kind));
    }
    validateExternalSymbolBasisOwnership(symbol.get());
    table->insert(decoded.entry_name, symbol.get());
    if (construction != nullptr)
      construction->retain(table, symbol.get());
    SgSymbol *published = symbol.release();
    if (isSgLabelSymbol(published) != nullptr &&
        symbolIsLookupPreferred(table, published->get_name(), published) !=
            decoded.lookup_preferred)
      throw std::runtime_error(
          "AST JSON constructed label differs from its exact table binding");
  }
}

void publishConstructedDeclaration(const AstFileRecord &ast,
                                   const NodeRecord &record,
                                   const NodeConstructionFlags &flags,
                                   SgNode *node, NodeMap &nodes,
                                   ArrayConstruction *construction) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON delayed declaration construction "
                             "produced no node for " +
                             record.kind);
  }
  SourceDeclaratorScopeConstruction source_scope(record, node, nodes);
  source_scope.populate(ast, record, nodes);
  flags.initialize(node);
  if (!nodes.emplace(record.id, node).second) {
    throw std::runtime_error("AST JSON duplicate delayed declaration id " +
                             std::to_string(record.id));
  }
  if (construction != nullptr) {
    construction->retain(record.id, node);
    construction->node_ids.emplace(node, record.id);
  }
  source_scope.publish();
}

void buildDelayedDeclaration(
    const AstFileRecord &ast, const NodeRecord &record, NodeMap &nodes,
    ArrayConstruction *construction, const RecordConstructionFields &fields,
    SgType *typedef_semantic_base = nullptr,
    const NodeRecord *constructing_function = nullptr) {
  if (nodes.count(record.id) != 0)
    throw std::runtime_error("AST JSON duplicate delayed declaration id " +
                             std::to_string(record.id));
  if (constructing_function != nullptr &&
      (construction == nullptr || record.kind != "SgInitializedName" ||
       !isFunctionDeclarationRecordKind(constructing_function->kind) ||
       requiredSingleEdgeTarget(record, "declptr") !=
           constructing_function->id ||
       (requiredSingleEdgeTarget(record, "parent") !=
            requiredSingleEdgeTarget(*constructing_function, "parameterList") &&
        requiredSingleEdgeTarget(record, "parent") !=
            singleEdgeTarget(*constructing_function, "parameterList_syntax"))))
    throw std::runtime_error(
        "AST JSON private parameter has no exact constructing function owner");
  const auto &flags = fields.flags;
  const auto &positions = fields.positions.value();
  if (record.kind == "SgEnumDeclaration") {
    std::unique_ptr<SgEnumDeclaration> declaration(
        isSgEnumDeclaration(createNodeFromRecord(record, nullptr, nodes)));
    std::unique_ptr<SgEnumType> wrapper(
        declaration->get_type()->get_declaration() == declaration.get()
            ? declaration->get_type()
            : nullptr);
    const uint64_t parent = singleEdgeTarget(record, "parent");
    if (parent != 0 && ast.node(parent).kind == "SgDeclarationScope") {
      const auto children = edgesFor(ast.node(parent), "declarations");
      if (std::count_if(children.begin(), children.end(),
                        [&](const EdgeRecord &edge) {
                          return edge.target == record.id;
                        }) != 1)
        throw std::runtime_error(
            "AST JSON enum tag has no exact declaration-scope owner");
      declaration->set_parent(nodeByIdAs<SgDeclarationScope>(nodes, parent));
    } else if (parent != 0 &&
               (parent == requiredSingleEdgeTarget(record, "scope") ||
                ast.node(parent).kind == "SgAuxiliaryDeclarationList"))
      declaration->set_parent(nodeById(nodes, parent));
    publishConstructedDeclaration(ast, record, flags, declaration.get(), nodes,
                                  construction);
    wrapper.release();
    declaration.release();
    return;
  }
  auto publish_scope = [&](auto *node) {
    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      node->set_scope(nodeByIdAs<SgScopeStatement>(nodes, target));
    }
    if (uint64_t target = singleEdgeTarget(record, "nonreal_decl_scope")) {
      auto *owner = isSgDeclarationStatement(node);
      auto *scope = nodeByIdAs<SgDeclarationScope>(nodes, target);
      if (owner == nullptr ||
          requiredSingleEdgeTarget(ast.node(target), "parent") != record.id)
        throw std::runtime_error("AST JSON delayed declaration has conflicting "
                                 "nonreal scope ownership");
      SageBuilder::setNonrealDeclarationScope(owner, scope);
    }
  };
  if (record.kind == "SgUsingDeclarationStatement") {
    const uint64_t name_id = exactSerializedPointerTarget(
        record, "initialized_name", "initializedName");
    auto *name =
        name_id != 0 ? nodeByIdAs<SgInitializedName>(nodes, name_id) : nullptr;
    RecordNodeConstruction ownership;
    if (name != nullptr && name->get_parent() == nullptr)
      ownership.borrow(name);
    auto *declaration = isSgUsingDeclarationStatement(
        ownership.own(createNodeFromRecord(record, nullptr, nodes)));
    publish_scope(declaration);
    publishConstructedDeclaration(ast, record, flags, declaration, nodes,
                                  construction);
    ownership.publish();
  } else if (record.kind == "SgNonrealDecl") {
    std::unique_ptr<SgNonrealDecl> declaration(
        isSgNonrealDecl(createNodeFromRecord(record, nullptr, nodes)));
    ASSERT_not_null(declaration);
    std::unique_ptr<SgNonrealType> type(declaration->get_type());
    publish_scope(declaration.get());
    if (uint64_t parent_id = singleEdgeTarget(record, "parent")) {
      auto parent = nodes.find(parent_id);
      if (parent != nodes.end())
        declaration->set_parent(parent->second);
    }
    publishConstructedDeclaration(ast, record, flags, declaration.get(), nodes,
                                  construction);
    type.release();
    declaration.release();
  } else if (record.kind == "SgNamespaceAliasDeclarationStatement") {
    auto *target = nodeByIdAs<SgNamespaceDeclarationStatement>(
        nodes, record.properties.requiredInt("namespace_declaration"));
    const uint64_t alias_id =
        record.properties.requiredInt("namespace_alias_declaration");
    auto *alias =
        alias_id == 0
            ? nullptr
            : nodeByIdAs<SgNamespaceAliasDeclarationStatement>(nodes, alias_id);
    if (alias != nullptr && alias->get_namespaceDeclaration() != target) {
      throw std::runtime_error("AST JSON namespace alias targets disagree");
    }
    const SgName name = record.properties.requiredName("name");
    const auto statement_fields =
        statementFieldsFromJson(record, V_SgNamespaceAliasDeclarationStatement);
    auto fields = declarationFieldsFromJson(
        record.properties, V_SgNamespaceAliasDeclarationStatement, record.kind);
    auto declaration =
        std::make_unique<SgNamespaceAliasDeclarationStatement>(name, target);
    installSourcePositions(declaration.get(),
                           constructSourcePositions(positions), record.kind);
    declaration->set_namespaceAliasDeclaration(alias);
    declaration->set_is_alias_for_another_namespace_alias(alias != nullptr);
    publish_scope(declaration.get());
    statement_fields.initialize(declaration.get());
    fields.initialize(declaration.get());
    publishConstructedDeclaration(ast, record, flags, declaration.get(), nodes,
                                  construction);
    declaration.release();
  } else if (record.kind == "SgTemplateParameter") {
    const auto fields =
        templateParameterSourceFieldsFromJson(record.properties);
    const uint64_t owner_id = singleEdgeTarget(record, "parent");
    static const VariantVector declarations(V_SgDeclarationStatement);
    if (owner_id != 0 &&
        (nodes.count(owner_id) == 0 || ast.index_by_id.count(owner_id) != 0) &&
        !kindInVariants(ast.node(owner_id).kind, declarations) &&
        ast.node(owner_id).kind != "SgTemplateParameterList")
      throw std::runtime_error("AST JSON template parameter has invalid owner");
    const auto owner_entry = nodes.find(owner_id);
    SgNode *owner = owner_entry != nodes.end() ? owner_entry->second : nullptr;
    if (owner != nullptr && isSgDeclarationStatement(owner) == nullptr &&
        isSgTemplateParameterList(owner) == nullptr)
      throw std::runtime_error("AST JSON template parameter has invalid owner");
    if (fields.kind != SgTemplateParameter::nontype_parameter) {
      std::unique_ptr<SgTemplateParameter> parameter(
          isSgTemplateParameter(createNodeFromRecord(record, nullptr, nodes)));
      if (owner != nullptr)
        parameter->set_parent(owner);
      publishConstructedDeclaration(ast, record, flags, parameter.get(), nodes,
                                    construction);
      parameter.release();
      return;
    }
    const uint64_t name_id =
        record.properties.requiredNonnegativeNodeId("initialized_name");
    auto *name = nodeByIdAs<SgInitializedName>(nodes, name_id);
    if (name->get_parent() != nullptr ||
        requiredSingleEdgeTarget(ast.node(name_id), "parent") != record.id ||
        !record.properties.at("type").exactlyEquals(
            ast.node(name_id).properties.at("type"))) {
      throw std::runtime_error("AST JSON non-type parameter has no exactly "
                               "owned and typed declarator");
    }
    SgTemplateParameter *canonical = nullptr;
    if (static_cast<uint64_t>(fields.canonicalNonTypeParameter) != record.id) {
      canonical = nodeByIdAs<SgTemplateParameter>(
          nodes, static_cast<uint64_t>(fields.canonicalNonTypeParameter));
      if (canonical->get_parameterType() !=
              SgTemplateParameter::nontype_parameter ||
          canonical->get_canonicalNonTypeParameter() != canonical)
        throw std::runtime_error(
            "AST JSON non-type parameter target is not canonical");
      if (!(canonical->get_canonical_source_identity() ==
            fields.sourceIdentity))
        throw std::runtime_error(
            "AST JSON non-type parameter source identity differs from its "
            "canonical parameter");
    }
    const auto operands = templateParameterOperandsFromRecord(record, nodes);
    SgNode::SourceStateConstruction source_construction;
    RecordNodeConstruction ownership;
    ownership.borrow(name);
    if (operands.expression != nullptr)
      ownership.borrow(operands.expression);
    auto *parameter = ownership.make<SgTemplateParameter>(
        fields.kind, name->get_type(), nullptr, operands.expression, nullptr,
        nullptr, name, canonical);
    installSourcePositions(parameter, constructSourcePositions(positions),
                           record.kind);
    if (fields.sourceIdentity.has_value())
      parameter->initialize_canonical_source_identity(*fields.sourceIdentity);
    parameter->set_templateParameterKeyword(fields.keyword);
    parameter->set_isAbbreviatedFunctionTemplateParameter(fields.abbreviated);
    parameter->set_is_parameter_pack(fields.pack);
    initializeTemplateParameterTypes(parameter, record, nodes);
    name->set_parent(parameter);
    if (operands.expression != nullptr)
      operands.expression->set_parent(parameter);
    if (owner != nullptr)
      parameter->set_parent(owner);
    publishConstructedDeclaration(ast, record, flags, parameter, nodes,
                                  construction);
    ownership.publish();
  } else if (record.kind == "SgInitializedName") {
    const SgName name = record.properties.requiredName("name");
    const InitializedNameRoles roles =
        initializedNameRolesFromJson(record.properties);
    const InitializedNameMetadata metadata =
        initializedNameMetadataFromJson(record.properties);
    const bool shape_deferred =
        record.properties.requiredBool("shape_deferred");
    SgType *type = exactInitializedNameType(record.properties, nodes);
    SgScopeStatement *semantic_scope =
        singleEdgeTarget(record, "scope") != 0
            ? nodeByIdAs<SgScopeStatement>(nodes,
                                           singleEdgeTarget(record, "scope"))
            : nullptr;
    if (semantic_scope == nullptr && constructing_function != nullptr)
      semantic_scope = nodeByIdAs<SgFunctionParameterScope>(
          nodes, requiredSingleEdgeTarget(*constructing_function,
                                          "functionParameterScope"));
    requireFortranProcedureComponentBinding(
        metadata.procedure_component_binding, type, semantic_scope);
    SgInitializedName *field_target =
        roles.ctor_field_target != 0
            ? nodeByIdAs<SgInitializedName>(nodes, roles.ctor_field_target)
            : nullptr;
    if (field_target != nullptr &&
        (singleEdgeTarget(record, "scope") == 0 ||
         field_target->get_scope() !=
             nodeByIdAs<SgClassDefinition>(nodes,
                                           singleEdgeTarget(record, "scope")) ||
         field_target->get_preinitialization() !=
             SgInitializedName::e_unknown_preinitialization ||
         !SageInterface::isEquivalentType(type, field_target->get_type())))
      throw std::runtime_error("AST JSON constructor member target has an "
                               "invalid field type or role");
    SgType *auto_decltype =
        nullableTypeFromJson(record.properties.at("auto_decltype"), nodes);
    SgType *fortran_source_type = nullableTypeFromJson(
        record.properties.at("fortran_source_type"), nodes);
    SgType *cxx_source_type = nullableCxxSourceTypeFromJson(
        record.properties.at("cxx_source_type"), nodes);
    const uint64_t source_declaration = singleEdgeTarget(record, "declptr");
    const bool object_declaration =
        source_declaration != 0 &&
        ast.node(source_declaration).kind == "SgVariableDeclaration";
    if (cxx_source_type != nullptr &&
        !(object_declaration ? SageInterface::cxxSourceTypeMatchesObjectType(
                                   cxx_source_type, type)
                             : SageInterface::cxxSourceTypeMatchesSemanticType(
                                   cxx_source_type, type)))
      throw std::runtime_error("AST JSON initializedName has contradictory "
                               "C++ source and semantic types");
    SgSymbol *source_symbol = exactBoundSymbolFromJson(
        record.properties.at("fortran_source_derived_type_symbol"), nodes);
    const uint64_t cray_pointee_id =
        record.properties.requiredNonnegativeNodeId("cray_pointer_pointee");
    SgInitializedName *cray_pointee =
        cray_pointee_id != 0
            ? nodeByIdAs<SgInitializedName>(nodes, cray_pointee_id)
            : nullptr;
    const uint64_t separate_shape_id =
        record.properties.requiredNonnegativeNodeId(
            "fortran_separate_shape_declaration");
    SgStatement *separate_shape =
        separate_shape_id != 0
            ? nodeByIdAs<SgStatement>(nodes, separate_shape_id)
            : nullptr;
    const uint64_t separate_pointer_id =
        record.properties.requiredNonnegativeNodeId(
            "fortran_separate_pointer_declaration");
    SgStatement *separate_pointer =
        separate_pointer_id != 0
            ? nodeByIdAs<SgStatement>(nodes, separate_pointer_id)
            : nullptr;
    const uint64_t previous_id = singleEdgeTarget(record, "prev_decl_item");
    SgInitializedName *previous =
        previous_id != 0 ? nodeByIdAs<SgInitializedName>(nodes, previous_id)
                         : nullptr;
    SourceTypeConstruction interface_types;
    InlineExpressionConstruction ownership;
    ownership.borrow(type);
    const uint64_t binding_scope_id =
        singleEdgeTarget(record, "structured_binding_scope");
    auto *binding_scope =
        binding_scope_id != 0
            ? nodeByIdAs<SgDeclarationScope>(nodes, binding_scope_id)
            : nullptr;
    if (binding_scope != nullptr) {
      if (binding_scope->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON structured-binding scope is already owned");
      ownership.borrow(binding_scope);
    }
    const uint64_t pattern_id =
        singleEdgeTarget(record, "structured_binding_pattern");
    auto *pattern = pattern_id != 0
                        ? nodeByIdAs<SgExprListExp>(nodes, pattern_id)
                        : nullptr;
    if (pattern != nullptr) {
      if (pattern->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON structured-binding pattern is already owned");
      ownership.borrow(pattern);
    }
    SgExpression *procedure_interface = expressionFromRef(
        record.properties.at("fortran_procedure_interface"), nodes, ownership);
    if (auto *source = isSgTypeExpression(procedure_interface))
      if (source->get_represented_type()->get_fortran_source_syntax())
        ownership.borrow(source->get_represented_type());
    requireFortranProcedureInterface(procedure_interface, type,
                                     metadata.fortran_type_spec, source_symbol);
    if (procedure_interface != nullptr &&
        procedure_interface->get_parent() != nullptr)
      throw std::runtime_error("AST JSON initializedName has no exact unowned "
                               "procedure interface");
    const auto binding_kind = structuredBindingKindFromJson(record.properties);
    SgInitializedName *binding_owner = nullptr, *binding_holding = nullptr;
    SgExpression *binding_value = nullptr;
    if (binding_kind != SgInitializedName::e_not_structured_binding) {
      binding_owner = nodeByIdAs<SgInitializedName>(
          nodes, requiredSingleEdgeTarget(record, "structured_binding_owner"));
      const auto value_id =
          singleEdgeTarget(record, "structured_binding_value");
      binding_value =
          value_id != 0 ? nodeByIdAs<SgExpression>(nodes, value_id) : nullptr;
      const auto holding_id =
          singleEdgeTarget(record, "structured_binding_holding_variable");
      binding_holding = holding_id != 0
                            ? nodeByIdAs<SgInitializedName>(nodes, holding_id)
                            : nullptr;
      if (!SgInitializedName::is_valid_structured_binding(
              name, type, binding_owner, binding_value, binding_holding,
              binding_kind, singleEdgeTarget(record, "initptr") != 0) ||
          (binding_value != nullptr && binding_value->get_parent() != nullptr))
        throw std::runtime_error(
            "AST JSON structured-binding has invalid construction inputs");
      if (binding_value != nullptr)
        ownership.borrow(binding_value);
    }
    auto *initialized_name = static_cast<SgInitializedName *>(
        ownership.own(constructInitializedName(name, type)));
    if (binding_kind != SgInitializedName::e_not_structured_binding) {
      publish_scope(initialized_name);
      initialized_name->initialize_structured_binding(
          binding_owner, binding_value, binding_holding, binding_kind);
    }
    installSourcePositions(initialized_name,
                           constructSourcePositions(positions), record.kind);
    metadata.initialize(initialized_name);
    if (binding_scope != nullptr)
      initialized_name->initialize_structured_binding_scope(binding_scope);
    initialized_name->set_structured_binding_pattern(pattern);
    initialized_name->set_auto_decltype(auto_decltype);
    initialized_name->set_fortran_source_type(fortran_source_type);
    initialized_name->set_fortran_source_derived_type_symbol(source_symbol);
    initialized_name->set_cxx_source_type(cxx_source_type);
    initialized_name->set_cray_pointer_pointee(cray_pointee);
    initialized_name->set_fortran_separate_shape_declaration(separate_shape);
    initialized_name->set_fortran_separate_pointer_declaration(
        separate_pointer);
    if (previous != nullptr)
      initialized_name->set_prev_decl_item(previous);
    initialized_name->set_fortran_procedure_interface(procedure_interface);
    initialized_name->set_shapeDeferred(shape_deferred);
    if (uint64_t declaration = singleEdgeTarget(record, "declptr")) {
      if (constructing_function == nullptr)
        initialized_name->set_declptr(
            nodeByIdAs<SgDeclarationStatement>(nodes, declaration));
    }
    initialized_name->set_enum_constant_source_ownership(roles.ownership);
    initialized_name->set_enum_constant_value(roles.enum_value);
    initialized_name->set_preinitialization(roles.preinitialization);
    initialized_name->set_ctor_initializer_source_order(
        roles.ctor_source_order);
    initialized_name->set_ctor_initializer_field_target(field_target);
    SgInitializedName *published = initialized_name;
    if (binding_kind == SgInitializedName::e_not_structured_binding)
      publish_scope(initialized_name);
    if (constructing_function != nullptr)
      initialized_name->set_scope(semantic_scope);
    if (singleEdgeTarget(record, "scope") == 0 &&
        initialized_name->get_declptr() != nullptr) {
      auto *function = isSgFunctionDeclaration(initialized_name->get_declptr());
      if (function == nullptr ||
          function->get_functionParameterScope() == nullptr)
        throw std::runtime_error("AST JSON initialized name has no "
                                 "construction-time semantic scope");
      initialized_name->set_scope(function->get_functionParameterScope());
    }
    if (const uint64_t parent = singleEdgeTarget(record, "parent");
        parent != 0 && ast.node(parent).kind == "SgFunctionParameterList") {
      const uint64_t owner =
          requiredSingleEdgeTarget(ast.node(parent), "parent");
      if (ast.node(owner).kind == "SgFunctionParameterTypeList") {
        auto *prototype = nodeByIdAs<SgFunctionParameterTypeList>(nodes, owner);
        auto *parameters = nodeByIdAs<SgFunctionParameterList>(nodes, parent);
        if (prototype->get_source_parameters() != parameters ||
            parameters->get_parent() != prototype ||
            initialized_name->get_scope() !=
                prototype->get_source_parameter_scope() ||
            initialized_name->get_declptr() != nullptr)
          throw std::runtime_error(
              "AST JSON prototype parameter has no exact construction owner");
        initialized_name->set_parent(parameters);
      }
    }
    interface_types.preparePublish();
    publishConstructedDeclaration(ast, record, flags, published, nodes,
                                  construction);
    interface_types.publish();
    ownership.publish();
  } else {
    const SgName name = record.properties.requiredName("name");
    const TypedefMetadata metadata = typedefMetadataFromJson(record.properties);
    const VariantT kind =
        record.kind == "SgTemplateTypedefDeclaration"
            ? V_SgTemplateTypedefDeclaration
        : record.kind == "SgTemplateInstantiationTypedefDeclaration"
            ? V_SgTemplateInstantiationTypedefDeclaration
            : V_SgTypedefDeclaration;
    const auto statement_fields = statementFieldsFromJson(record, kind);
    auto fields =
        declarationFieldsFromJson(record.properties, kind, record.kind);
    std::optional<std::pair<SgName, SgName>> template_spelling;
    if (kind == V_SgTemplateInstantiationTypedefDeclaration) {
      template_spelling.emplace(
          record.properties.requiredName("template_name"),
          record.properties.requiredName("template_header"));
    }
    SgType *base = typedef_semantic_base != nullptr
                       ? typedef_semantic_base
                       : exactTypedefBaseType(record.properties, nodes);
    SgTemplateTypedefDeclaration *template_declaration =
        kind == V_SgTemplateInstantiationTypedefDeclaration
            ? nodeByIdAs<SgTemplateTypedefDeclaration>(
                  nodes,
                  requiredSingleEdgeTarget(record, "templateDeclaration"))
            : nullptr;
    const uint64_t base_declaration_id =
        singleEdgeTarget(record, "declaration");
    SgDeclarationStatement *base_declaration =
        base_declaration_id != 0
            ? nodeByIdAs<SgDeclarationStatement>(nodes, base_declaration_id)
            : nullptr;
    const bool owns_base_declaration =
        base_declaration != nullptr &&
        singleEdgeTarget(ast.node(base_declaration_id), "parent") == record.id;
    SgTemplateParameterPtrList parameters;
    if (kind == V_SgTemplateTypedefDeclaration) {
      for (const auto &edge : edgesFor(record, "templateParameters")) {
        auto *parameter = nodeByIdAs<SgTemplateParameter>(nodes, edge.target);
        if (parameter->get_parent() != nullptr)
          throw std::runtime_error(
              "AST JSON alias parameter already has an owner");
        parameters.push_back(parameter);
      }
    }
    SgTemplateArgumentPtrList arguments, deduced_arguments;
    if (kind == V_SgTemplateInstantiationTypedefDeclaration) {
      for (const auto &edge : edgesFor(record, "templateArguments"))
        arguments.push_back(nodeByIdAs<SgTemplateArgument>(nodes, edge.target));
      for (const auto &edge : edgesFor(record, "deducedTemplateArguments"))
        deduced_arguments.push_back(
            nodeByIdAs<SgTemplateArgument>(nodes, edge.target));
    }
    std::unique_ptr<SgTypedefDeclaration> declaration(
        constructTypedefDeclaration(
            name, base, base_declaration, owns_base_declaration, kind,
            template_declaration, arguments, deduced_arguments, metadata));
    // Generated declaration destructors do not reclaim the canonical wrapper.
    std::unique_ptr<SgTypedefType> wrapper(declaration->get_type());
    SgTypedefDeclaration *published = declaration.get();
    const bool owns_source_type = isTypedefOwnedSourceType(base);
    ROSE_ASSERT(!owns_source_type || base->get_parent() == published);
    try {
      installSourcePositions(declaration.get(),
                             constructSourcePositions(positions), record.kind);
      if (auto *instantiation =
              isSgTemplateInstantiationTypedefDeclaration(declaration.get())) {
        ROSE_ASSERT(template_spelling.has_value());
        instantiation->set_templateName(template_spelling->first);
        instantiation->set_templateHeader(template_spelling->second);
      }
      publish_scope(declaration.get());
      if (auto *alias = isSgTemplateTypedefDeclaration(declaration.get())) {
        alias->get_templateParameters() = parameters;
        for (auto *parameter : parameters)
          parameter->set_parent(alias);
      }
      statement_fields.initialize(declaration.get());
      fields.initialize(declaration.get());
      publishConstructedDeclaration(ast, record, flags, published, nodes,
                                    construction);
    } catch (...) {
      for (const auto *list : {&arguments, &deduced_arguments})
        for (auto *argument : *list)
          if (argument->get_parent() == published)
            argument->set_parent(nullptr);
      for (auto *parameter : parameters)
        if (parameter->get_parent() == published)
          parameter->set_parent(nullptr);
      if (owns_source_type)
        base->set_parent(nullptr);
      if (owns_base_declaration)
        base_declaration->set_parent(nullptr);
      throw;
    }
    wrapper.release();
    declaration.release();
  }
}
} // namespace

static bool constructTemplateVariableDeclarator(
    const AstFileRecord &ast, NodeMap &nodes, uint64_t id,
    const std::function<bool(uint64_t)> &construct_dependency,
    ArrayConstruction *construction) {
  auto *declaration = nodeByIdAs<SgTemplateVariableDeclaration>(nodes, id);
  const auto &variables = edgesFor(ast.node(id), "variables");
  if (variables.size() != 1)
    throw std::runtime_error(
        "AST JSON variable template has no exact single declarator");
  const uint64_t name_id = variables.front().target;
  const NodeRecord &record = ast.node(name_id);
  if (record.kind != "SgInitializedName" ||
      requiredSingleEdgeTarget(record, "parent") != id ||
      requiredSingleEdgeTarget(record, "declptr") != id)
    throw std::runtime_error(
        "AST JSON variable template declarator has no exact typed owner");
  if (!construct_dependency(name_id))
    return false;
  auto *name = nodeByIdAs<SgInitializedName>(nodes, name_id);
  if (name->get_declptr() != declaration ||
      SageInterface::containsUnknownType(name->get_type()) ||
      (name->get_parent() != nullptr && name->get_parent() != declaration))
    throw std::runtime_error("AST JSON variable template declarator has "
                             "incomplete typed construction");
  if (declaration->get_variables().empty()) {
    if (construction != nullptr && !construction->committed) {
      construction->variable_declarations.push_back(declaration);
      construction->parameter_parents.emplace(name, name->get_parent());
    }
    declaration->get_variables().push_back(name);
    name->set_parent(declaration);
  } else if (declaration->get_variables().size() != 1 ||
             declaration->get_variables().front() != name) {
    throw std::runtime_error(
        "AST JSON variable template has conflicting declarator identity");
  }
  return true;
}

bool declarationConstructionDependencyReady(const AstFileRecord &ast,
                                            const NodeMap &nodes, uint64_t id,
                                            bool needs_symbol,
                                            bool needs_type_identity,
                                            VariantT symbol_kind) {
  auto found = nodes.find(id);
  if (found == nodes.end())
    return false;
  auto children_ready = [&](SgNode *list, const auto &elements,
                            const char *role) {
    const auto children = edgesFor(ast.node(id), role);
    if (elements.size() != children.size())
      return false;
    for (size_t index = 0; index < children.size(); ++index) {
      auto child = nodes.find(children[index].target);
      if (child == nodes.end() || child->second == nullptr ||
          elements[index] != child->second ||
          child->second->get_parent() != list)
        return false;
    }
    return true;
  };
  if (auto *list = isSgExprListExp(found->second)) {
    if (!children_ready(list, list->get_expressions(), "expressions"))
      return false;
  }
  if (auto *list = isSgOmpClauseList(found->second)) {
    if (!children_ready(list, list->get_clauses(), "clauses"))
      return false;
  }
  if (auto *variable = isSgTemplateVariableDeclaration(found->second)) {
    const auto &names = variable->get_variables();
    if (names.size() != 1 || names.front() == nullptr ||
        names.front()->get_parent() != variable ||
        names.front()->get_declptr() != variable ||
        SageInterface::containsUnknownType(names.front()->get_type()))
      return false;
  }
  if (auto *variable = isSgVariableDeclaration(found->second)) {
    if (!children_ready(variable, variable->get_variables(), "variables"))
      return false;
  }
  if (auto *prototype = isSgFunctionParameterTypeList(found->second)) {
    auto *list = prototype->get_source_parameters();
    auto *scope = prototype->get_source_parameter_scope();
    const uint64_t list_id =
        requiredSingleEdgeTarget(ast.node(id), "source_parameters");
    const auto parameters = edgesFor(ast.node(list_id), "args");
    if (list == nullptr || scope == nullptr ||
        list->get_args().size() != parameters.size())
      return false;
    for (size_t index = 0; index < parameters.size(); ++index) {
      auto parameter = nodes.find(parameters[index].target);
      if (parameter == nodes.end() ||
          list->get_args()[index] != parameter->second)
        return false;
      auto *name = list->get_args()[index];
      if (!name->get_name().getString().empty() &&
          scope->find_symbol_from_declaration(name) == nullptr)
        return false;
    }
  }
  if (auto *function = isSgFunctionDeclaration(found->second);
      function != nullptr && function->get_type() == nullptr)
    return false;
  if (auto *argument = isSgTemplateArgument(found->second)) {
    switch (argument->get_argumentType()) {
    case SgTemplateArgument::type_argument:
    case SgTemplateArgument::nontype_argument:
      if (argument->get_type() == nullptr ||
          (argument->get_argumentType() ==
               SgTemplateArgument::nontype_argument &&
           argument->get_expression() == nullptr))
        return false;
      break;
    case SgTemplateArgument::template_template_argument:
      if (argument->get_templateDeclaration() == nullptr)
        return false;
      break;
    case SgTemplateArgument::start_of_pack_expansion_argument:
      break;
    default:
      throw std::runtime_error("AST JSON invalid template argument kind");
    }
  }
  auto *nonreal = isSgNonrealDecl(found->second);
  if (needs_type_identity && nonreal != nullptr &&
      (nonreal->get_type() == nullptr ||
       nonreal->get_type()->get_declaration() != nonreal ||
       nonreal->get_type()->get_parent() != nonreal))
    return false;
  if (needs_type_identity && !isSgClassDeclaration(found->second) &&
      !isSgEnumDeclaration(found->second) && nonreal == nullptr) {
    // Class, enum and nonreal wrappers use their declaration identity.
    for (SgNode *owner = found->second; owner != nullptr;
         owner = owner->get_parent()) {
      if (auto *definition = isSgFunctionDefinition(owner)) {
        if (definition->get_declaration() == nullptr ||
            definition->get_declaration()->get_type() == nullptr)
          return false;
      }
      auto *specialization = isSgTemplateClassDeclaration(owner);
      if (specialization != nullptr &&
          specialization->get_specialization() !=
              SgDeclarationStatement::e_no_specialization &&
          specialization->get_templateSpecializationArguments().empty())
        return false;
    }
  }
  if (auto *declaration = isSgEnumDeclaration(found->second)) {
    if (needs_type_identity && declaration->get_type() == nullptr)
      return false;
  }
  if (auto *declaration = isSgClassDeclaration(found->second);
      needs_type_identity && declaration != nullptr) {
    auto *defining =
        isSgClassDeclaration(declaration->get_definingDeclaration());
    // A class declaration alone cannot supply its type while its defining
    // declaration still awaits the exact definition node.
    if (declaration->get_type() == nullptr ||
        (defining != nullptr && defining->get_definition() == nullptr))
      return false;
  }
  if (needs_symbol) {
    if (symbol_kind == V_SgLabelSymbol) {
      SgScopeStatement *scope = labelSymbolScopeForBasis(found->second);
      if (scope == nullptr)
        return false;
      SgSymbol *symbol = nullptr;
      if (auto *statement = isSgStatement(found->second))
        symbol = scope->find_symbol_from_declaration(statement);
      else if (auto *parameter = isSgInitializedName(found->second))
        symbol = scope->find_symbol_from_declaration(parameter);
      else
        throw std::runtime_error("AST JSON label has an invalid typed basis");
      return isSgLabelSymbol(symbol) != nullptr;
    }
    if (auto *name = isSgInitializedName(found->second)) {
      SgScopeStatement *scope = name->get_scope();
      return scope != nullptr &&
             scope->find_symbol_from_declaration(name) != nullptr;
    }
    if (auto *declaration = isSgDeclarationStatement(found->second)) {
      if (declaration->get_parent() == nullptr)
        return false;
      SgScopeStatement *scope = declaration->get_scope();
      return scope != nullptr &&
             scope->find_symbol_from_declaration(declaration) != nullptr;
    }
    if (auto *statement = isSgStatement(found->second)) {
      SgScopeStatement *scope = labelSymbolScopeForBasis(statement);
      return scope != nullptr &&
             isSgLabelSymbol(scope->find_symbol_from_declaration(statement)) !=
                 nullptr;
    }
    return false;
  }
  return true;
}
namespace {
using ArrayConstructionOwner = std::function<ArrayConstruction *(uint64_t)>;

std::string
missingDeclarationDependencies(const AstFileRecord &ast, const NodeMap &nodes,
                               const std::vector<const NodeRecord *> &pending,
                               std::string message) {
  std::vector<const NodeRecord *> records = pending;
  std::unordered_set<uint64_t> visited;
  for (const NodeRecord *record : records)
    visited.insert(record->id);
  for (size_t index = 0; index < records.size(); ++index) {
    const NodeRecord *record = records[index];
    std::unordered_set<uint64_t> dependencies, symbols, types;
    if (record->kind == "SgTemplateArgument")
      collectTypeDependencyIds(record->properties, dependencies, symbols,
                               types);
    else
      collectDelayedDeclarationDependencies(ast, *record, nodes, dependencies,
                                            symbols, types);
    if (isFunctionDeclarationRecordKind(record->kind)) {
      for (const char *field :
           {"associatedClassDeclaration", "templateDeclaration",
            "template_instantiation_pattern"})
        if (uint64_t id = singleEdgeTarget(*record, field))
          dependencies.insert(id);
      for (const EdgeRecord &edge :
           edgesFor(*record, "dependentTemplateCandidates"))
        dependencies.insert(edge.target);
    }
    message +=
        " " + record->kind + " " + std::to_string(record->id) + " waits for [";
    bool first = true;
    for (uint64_t id : dependencies) {
      if (declarationConstructionDependencyReady(
              ast, nodes, id, symbols.count(id) != 0, types.count(id) != 0))
        continue;
      if (!first)
        message += ",";
      message += std::to_string(id);
      message += nodes.count(id) == 0 ? "(absent)" : "(incomplete)";
      if (ast.index_by_id.count(id) != 0) {
        const NodeRecord &dependency = ast.node(id);
        if ((dependency.kind == "SgNonrealDecl" ||
             dependency.kind == "SgTemplateParameter" ||
             dependency.kind == "SgTemplateArgument") &&
            visited.insert(id).second)
          records.push_back(&dependency);
      }
      first = false;
    }
    message += "]";
    if (const auto *source =
            record->properties.find("fortran_source_derived_type_symbol");
        source != nullptr &&
        !exactSymbolBindingDependenciesReady(*source, nodes))
      message += " waits for one exact source symbol binding";
  }
  return message;
}

NodeMap publishedArrayConstructionNodes(const NodeMap &nodes,
                                        const ArrayConstructionOwner &owner) {
  NodeMap published;
  for (const auto &[id, node] : nodes) {
    auto *construction = owner(id);
    if (construction == nullptr || construction->committed)
      published.emplace(id, node);
  }
  return published;
}

const JsonValue *sourceAliasSemanticBase(const NodeRecord &record) {
  if (record.kind != "SgTypedefDeclaration" &&
      record.kind != "SgTemplateTypedefDeclaration" &&
      record.kind != "SgTemplateInstantiationTypedefDeclaration")
    return nullptr;
  const JsonValue &source = record.properties.at("base_type");
  if (source.requiredString("kind") != "SgDeclType")
    return nullptr;
  return &source.at("base_type");
}

std::vector<uint64_t>
constructReciprocalSourceAliases(const AstFileRecord &ast,
                                 const SymbolBindingIndex &bindings,
                                 const NodeRecord &seed, NodeMap &nodes,
                                 const ArrayConstructionOwner &array_owner,
                                 RecordConstructionFieldMap *prepared_fields) {
  if (sourceAliasSemanticBase(seed) == nullptr)
    return {};
  const VariantVector expression_variants(V_SgExpression);
  std::vector<const NodeRecord *> aliases{&seed}, expressions;
  std::vector<const NodeRecord *> records{&seed};
  std::unordered_set<uint64_t> group{seed.id};
  for (size_t index = 0; index < records.size(); ++index) {
    const NodeRecord &record = *records[index];
    std::unordered_set<uint64_t> dependencies, symbols, types;
    if (sourceAliasSemanticBase(record) != nullptr)
      collectDelayedDeclarationDependencies(ast, record, nodes, dependencies,
                                            symbols, types);
    else {
      collectTypeDependencyIds(record.properties, dependencies, symbols, types);
      for (const auto &edge : record.edges)
        if (edge.field != "parent" && edge.target != 0)
          dependencies.insert(edge.target);
    }
    for (uint64_t id : dependencies) {
      if (auto *owner = array_owner ? array_owner(id) : nullptr;
          owner != nullptr && !owner->committed)
        return {};
      if (nodes.count(id) != 0) {
        if (!declarationConstructionDependencyReady(
                ast, nodes, id, symbols.count(id) != 0, types.count(id) != 0))
          return {};
        if (isSgExpression(nodes.at(id)) != nullptr && group.insert(id).second)
          records.push_back(&ast.node(id));
        continue;
      }
      if (group.count(id) != 0)
        continue;
      if (ast.index_by_id.count(id) == 0)
        return {};
      const NodeRecord &dependency = ast.node(id);
      if (sourceAliasSemanticBase(dependency) != nullptr)
        aliases.push_back(&dependency);
      else if (kindInVariants(dependency.kind, expression_variants))
        expressions.push_back(&dependency);
      else
        return {};
      group.insert(id);
      records.push_back(&dependency);
    }
  }
  if (expressions.empty())
    return {};

  RecordConstructionFieldMap local_fields;
  if (prepared_fields == nullptr) {
    for (const NodeRecord *record : records) {
      if (nodes.count(record->id) != 0)
        continue;
      RecordConstructionFields fields{validateConstructionFlags(*record), {}};
      prepareRecordPublicationFields(*record, fields);
      fields.positions = validateSourcePositionRecord(*record);
      local_fields.emplace(record->id, std::move(fields));
    }
  }
  auto &construction_fields =
      prepared_fields != nullptr ? *prepared_fields : local_fields;

  // Canonical aliases use their recorded semantic bases. Their written
  // expressions can then reference those exact aliases inside this group.
  NodeMap private_nodes = nodes;
  const NodeIds borrowed = mappedNodePointers(nodes);
  NodeIds private_ids = borrowed;
  SourceTypeConstruction semantic_types;
  ArrayConstruction construction(private_nodes, private_ids, borrowed);
  std::vector<uint64_t> built;
  built.reserve(aliases.size());
  std::vector<const NodeRecord *> pending = aliases;
  while (!pending.empty()) {
    bool progress = false;
    for (size_t index = 0; index < pending.size();) {
      const NodeRecord &record = *pending[index];
      std::unordered_set<uint64_t> dependencies, symbols, types;
      collectTypeDependencyIds(*sourceAliasSemanticBase(record), dependencies,
                               symbols, types);
      if (std::any_of(dependencies.begin(), dependencies.end(),
                      [&](uint64_t id) {
                        return !declarationConstructionDependencyReady(
                            ast, private_nodes, id, symbols.count(id) != 0,
                            types.count(id) != 0);
                      })) {
        ++index;
        continue;
      }
      SgType *semantic = typeFromJson(
          *sourceAliasSemanticBase(record), private_nodes,
          "AST JSON reciprocal typedef requires an exact semantic base");
      if (isTypedefOwnedSourceType(semantic))
        throw std::runtime_error(
            "AST JSON reciprocal typedef semantic base is a lexical type");
      buildDelayedDeclaration(ast, record, private_nodes, &construction,
                              construction_fields.at(record.id), semantic);
      built.push_back(record.id);
      pending[index] = pending.back();
      pending.pop_back();
      progress = true;
    }
    if (!progress)
      throw std::runtime_error(
          "AST JSON reciprocal typedef semantic bases are missing or cyclic");
  }
  SourceTypeConstruction source_types;
  struct SourceBases {
    std::vector<std::pair<SgTypedefDeclaration *, SgType *>> original;
    bool published = false;
    ~SourceBases() {
      if (!published)
        for (const auto &[declaration, semantic] : original) {
          declaration->get_base_type()->set_parent(nullptr);
          declaration->set_base_type(semantic);
        }
    }
  } source_bases;
  source_bases.original.reserve(aliases.size());
  constructRecordedNodes(ast, bindings, private_nodes, expressions, private_ids,
                         nullptr, {}, &construction, &construction_fields);
  for (const NodeRecord *record : aliases) {
    auto *declaration =
        nodeByIdAs<SgTypedefDeclaration>(private_nodes, record->id);
    SgType *semantic = declaration->get_base_type();
    SgType *source = exactTypedefBaseType(record->properties, private_nodes);
    if (isSgDeclType(source) == nullptr ||
        isSgDeclType(source)->get_base_type() != semantic)
      throw std::runtime_error(
          "AST JSON reciprocal typedef disagrees with its semantic base");
    source_bases.original.emplace_back(declaration, semantic);
    source->set_parent(declaration);
    declaration->set_base_type(source);
  }
  for (uint64_t id : built)
    restoreConstructedBasisSymbols(bindings, id, private_nodes, &construction);
  nodes.reserve(private_nodes.size());
  source_types.preparePublish();
  semantic_types.preparePublish();
  construction.arrays.preparePublish();
  source_types.publish();
  construction.arrays.publish();
  semantic_types.publish();
  construction.commit();
  source_bases.published = true;
  nodes.merge(private_nodes);
  return built;
}

void drainDelayedDeclarationsWithArrays(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    std::vector<const NodeRecord *> &pending, NodeMap &nodes,
    bool require_complete, const ArrayConstructionOwner &array_owner = {},
    const SymbolConstructionAdvance &advance_symbols = {},
    RecordConstructionFieldMap *prepared_fields = nullptr) {
  RecordConstructionFieldMap local_fields;
  if (prepared_fields == nullptr) {
    for (const NodeRecord *record : pending) {
      RecordConstructionFields fields{validateConstructionFlags(*record), {}};
      fields.positions = validateSourcePositionRecord(*record);
      if (!local_fields.emplace(record->id, std::move(fields)).second)
        throw std::runtime_error("AST JSON duplicate delayed declaration id " +
                                 std::to_string(record->id));
    }
  }
  const auto &construction_fields =
      prepared_fields != nullptr ? *prepared_fields : local_fields;
  auto advance_dependencies = [&] {
    auto advance = [&](const NodeMap &available) {
      bool progress = advance_symbols && advance_symbols(available);
      return constructExternalFunctionParameterScopeSymbols(ast, available,
                                                            false) ||
             progress;
    };
    if (!array_owner)
      return advance(nodes);
    return advance(publishedArrayConstructionNodes(nodes, array_owner));
  };
  while (!pending.empty()) {
    bool made_progress = advance_dependencies();
    for (size_t index = 0; index < pending.size();) {
      const NodeRecord &record = *pending[index];
      auto *construction = array_owner ? array_owner(record.id) : nullptr;
      auto *arrays = construction == nullptr || construction->committed
                         ? SemanticArrayConstruction::current()
                         : &construction->arrays;
      if (!semanticArrayDependenciesReady(record.properties, arrays)) {
        ++index;
        continue;
      }
      std::unordered_set<uint64_t> dependencies;
      std::unordered_set<uint64_t> symbol_dependencies;
      std::unordered_set<uint64_t> type_dependencies;
      collectDelayedDeclarationDependencies(ast, record, nodes, dependencies,
                                            symbol_dependencies,
                                            type_dependencies);
      bool ready = true;
      for (uint64_t dependency : dependencies) {
        auto *owner = array_owner ? array_owner(dependency) : nullptr;
        if ((owner != nullptr && owner != construction && !owner->committed) ||
            !declarationConstructionDependencyReady(
                ast, nodes, dependency,
                symbol_dependencies.count(dependency) != 0,
                type_dependencies.count(dependency) != 0)) {
          ready = false;
          break;
        }
      }
      if (ready && record.kind == "SgInitializedName" &&
          singleEdgeTarget(record, "scope") == 0 &&
          singleEdgeTarget(record, "declptr") != 0) {
        auto *function = nodeByIdAs<SgFunctionDeclaration>(
            nodes, requiredSingleEdgeTarget(record, "declptr"));
        ready = function->get_functionParameterScope() != nullptr;
      }
      if (ready && record.kind == "SgInitializedName")
        ready = exactSymbolBindingDependenciesReady(
            record.properties.at("fortran_source_derived_type_symbol"), nodes);
      if (!ready) {
        if (construction == nullptr) {
          const auto built = constructReciprocalSourceAliases(
              ast, bindings, record, nodes, array_owner, prepared_fields);
          if (!built.empty()) {
            pending.erase(std::remove_if(pending.begin(), pending.end(),
                                         [&](const NodeRecord *declaration) {
                                           return std::find(built.begin(),
                                                            built.end(),
                                                            declaration->id) !=
                                                  built.end();
                                         }),
                          pending.end());
            index = 0;
            made_progress = true;
            continue;
          }
        }
        ++index;
        continue;
      }
      SemanticArrayConstructionScope array_scope(arrays);
      buildDelayedDeclaration(ast, record, nodes, construction,
                              construction_fields.at(record.id));
      // Publish canonical table entries for the new declaration before any
      // dependent inline expression decodes: type-owned references resolve
      // through restored tables, and this exact insertion is skipped (never
      // duplicated) by the main symbol phase.
      restoreConstructedBasisSymbols(bindings, record.id, nodes, construction);
      pending[index] = pending.back();
      pending.pop_back();
      made_progress = true;
    }
    if (advance_dependencies())
      made_progress = true;
    if (!made_progress) {
      if (!require_complete) {
        return;
      }
      throw std::runtime_error(
          missingDeclarationDependencies(ast, nodes, pending,
                                         "AST JSON delayed declarations have "
                                         "missing or cyclic dependencies:"));
    }
  }
}
} // namespace

static void drainDelayedDeclarations(const AstFileRecord &ast,
                                     const SymbolBindingIndex &bindings,
                                     std::vector<const NodeRecord *> &pending,
                                     NodeMap &nodes,
                                     RecordConstructionFieldMap &fields,
                                     bool require_complete,
                                     const SymbolConstructionAdvance &symbols) {
  drainDelayedDeclarationsWithArrays(ast, bindings, pending, nodes,
                                     require_complete, {}, symbols, &fields);
}

void drainDelayedDeclarations(const AstFileRecord &ast,
                              std::vector<const NodeRecord *> &pending,
                              NodeMap &nodes, bool require_complete,
                              const SymbolConstructionAdvance &symbols) {
  drainDelayedDeclarationsWithArrays(ast, indexSymbolBindings(ast), pending,
                                     nodes, require_complete, {}, symbols);
}

static void constructRecordedNodes(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    NodeMap &nodes, const std::vector<const NodeRecord *> &records,
    NodeIds &node_ids, std::vector<const NodeRecord *> *delayed_declarations,
    const SymbolConstructionAdvance &advance_symbols,
    ArrayConstruction *source_construction,
    RecordConstructionFieldMap *prepared_construction_fields) {
  RecordConstructionFieldMap local_construction_fields;
  static const VariantVector expression_variants(V_SgExpression);
  static const VariantVector omp_body_variants(V_SgOmpBodyStatement);
  static const VariantVector omp_clause_statements(V_SgOmpClauseStatement);
  static const VariantVector omp_clause_bodies(V_SgOmpClauseBodyStatement);
  static const VariantVector omp_clauses(V_SgOmpClause);
  static const VariantVector image_control_variants(V_SgImageControlStatement);
  if (prepared_construction_fields == nullptr) {
    for (const NodeRecord *record : records) {
      if (nodes.count(record->id) != 0)
        continue;
      RecordConstructionFields fields{validateConstructionFlags(*record), {}};
      prepareRecordPublicationFields(*record, fields);
      fields.positions = validateSourcePositionRecord(*record);
      if (!local_construction_fields.emplace(record->id, std::move(fields))
               .second)
        throw std::runtime_error("AST JSON duplicate delayed node " +
                                 record->kind);
    }
    for (const NodeRecord *record : records) {
      if (record->kind != "SgFunctionParameterTypeList" ||
          nodes.count(record->id) != 0)
        continue;
      for (const char *role : {"source_parameter_scope", "source_parameters"}) {
        const auto &child = ast.node(requiredSingleEdgeTarget(*record, role));
        if (local_construction_fields.count(child.id) != 0)
          continue;
        RecordConstructionFields fields{validateConstructionFlags(child), {}};
        prepareRecordPublicationFields(child, fields);
        fields.positions = validateSourcePositionRecord(child);
        local_construction_fields.emplace(child.id, std::move(fields));
      }
    }
  }
  auto &construction_fields = prepared_construction_fields != nullptr
                                  ? *prepared_construction_fields
                                  : local_construction_fields;
  const bool has_semantic_bindings =
      std::any_of(records.begin(), records.end(), [](const NodeRecord *record) {
        return record->kind == "SgCxxArrayInitLoop" ||
               record->kind == "SgPseudoObjectExp";
      });
  const NodeIds initial_nodes = has_semantic_bindings ? node_ids : NodeIds{};
  std::unordered_map<uint64_t, std::unique_ptr<ArrayConstruction>>
      array_construction;
  std::unordered_map<uint64_t, uint64_t> array_roots;
  if (has_semantic_bindings) {
    for (const NodeRecord &record : ast.nodes) {
      uint64_t root = 0;
      std::unordered_set<uint64_t> visited;
      for (const NodeRecord *owner = &record; owner != nullptr;) {
        if (!visited.insert(owner->id).second)
          throw std::runtime_error(
              "AST JSON array initialization has cyclic ownership");
        if (owner->kind == "SgCxxArrayInitLoop" ||
            owner->kind == "SgPseudoObjectExp")
          root = owner->id;
        const uint64_t parent = singleEdgeTarget(*owner, "parent");
        owner = parent == 0 ? nullptr : &ast.node(parent);
      }
      if (root != 0 && nodes.count(root) == 0) {
        array_roots.emplace(record.id, root);
        if (array_construction.count(root) == 0)
          array_construction.emplace(root, std::make_unique<ArrayConstruction>(
                                               nodes, node_ids, initial_nodes));
      }
    }
  }
  auto array_owner = [&](uint64_t id) -> ArrayConstruction * {
    const auto root = array_roots.find(id);
    return root == array_roots.end()
               ? nullptr
               : array_construction.at(root->second).get();
  };
  auto with_delayed_node_ownership = [&](const NodeRecord &record, SgNode *node,
                                         auto publish) {
    if (source_construction != nullptr) {
      source_construction->retain(record.id, node);
      publish();
      node_ids.emplace(node, record.id);
      return;
    }
    if (auto *owner = array_owner(record.id);
        owner != nullptr && !owner->committed) {
      auto &transaction = *owner;
      transaction.retain(record.id, node);
      const bool complete = array_roots.at(record.id) == record.id;
      if (complete) {
        if (auto *loop = isSgCxxArrayInitLoop(node))
          loop->validate_bindings();
        else
          isSgPseudoObjectExp(node)->validate();
      }
      publish();
      node_ids.emplace(node, record.id);
      if (complete)
        transaction.commit();
      return;
    }
    RecordNodeConstruction ownership(node_ids);
    ownership.own(node);
    const bool label_basis =
        isSgStatement(node) != nullptr &&
        (singleEdgeTarget(record, "numeric_label") != 0 ||
         singleEdgeTarget(record, "end_numeric_label") != 0 ||
         singleEdgeTarget(record, "else_numeric_label") != 0);
    if (label_basis) {
      SgNode *parent =
          nodeById(nodes, requiredSingleEdgeTarget(record, "parent"));
      if (node->get_parent() != nullptr && node->get_parent() != parent)
        throw std::runtime_error(
            "AST JSON labeled statement has a conflicting exact owner");
      node->set_parent(parent);
    }
    publish();
    if (label_basis)
      restoreConstructedBasisSymbols(bindings, record.id, nodes, nullptr);
    ownership.publish();
    node_ids.emplace(node, record.id);
  };
  auto publish_delayed_node = [&](const NodeRecord &record,
                                  SourcePositions positions, SgNode *node) {
    with_delayed_node_ownership(record, node, [&] {
      publishConstructorOnlyNode(record, node, nodes, std::move(positions),
                                 construction_fields.at(record.id));
    });
  };
  auto require_unowned_delayed_child = [&](uint64_t child_id,
                                           const NodeRecord &owner,
                                           const std::string &context) {
    auto child = nodes.find(child_id);
    if (child == nodes.end() || child->second == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " has no constructed child node");
    }
    const NodeRecord &child_record = ast.node(child_id);
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

  auto require_function_reference = [&](uint64_t id,
                                        const std::string &context) {
    SgExpression *reference = nodeByIdAs<SgExpression>(nodes, id);
    SgFunctionDeclaration *declaration = nullptr;
    if (auto *typed = isSgTemplateMemberFunctionRefExp(reference))
      declaration = typed->getAssociatedMemberFunctionDeclaration();
    else if (auto *typed = isSgTemplateFunctionRefExp(reference))
      declaration = typed->getAssociatedFunctionDeclaration();
    else if (auto *typed = isSgMemberFunctionRefExp(reference))
      declaration = typed->getAssociatedMemberFunctionDeclaration();
    else if (auto *typed = isSgFunctionRefExp(reference))
      declaration = typed->getAssociatedFunctionDeclaration();
    if (declaration == nullptr || declaration->get_type() == nullptr ||
        reference->get_parent() != nullptr)
      throw std::runtime_error("AST JSON " + context +
                               " requires an exact unowned function reference");
    return reference;
  };

  auto construct_direct_record = [&](const NodeRecord &record,
                                     SourcePositions positions) {
    if (record.kind == "SgCxxDecompositionRefExp") {
      auto *owner = nodeByIdAs<SgInitializedName>(
          nodes, requiredSingleEdgeTarget(record, "decomposition"));
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      if (!SgCxxDecompositionRefExp::is_valid_target(owner, type))
        throw std::runtime_error(
            "AST JSON decomposition reference has no exact typed storage");
      publish_delayed_node(record, std::move(positions),
                           new SgCxxDecompositionRefExp(owner, type));
    } else if (record.kind == "SgCxxSubobjectDestruction") {
      auto *callee = nodeByIdAs<SgMemberFunctionDeclaration>(
          nodes, requiredSingleEdgeTarget(record, "destructor_declaration"));
      const uint64_t member_id = singleEdgeTarget(record, "member_target");
      const uint64_t base_id = singleEdgeTarget(record, "base_target");
      auto *member = member_id != 0
                         ? nodeByIdAs<SgInitializedName>(nodes, member_id)
                         : nullptr;
      auto *base = base_id != 0 ? nodeByIdAs<SgClassDeclaration>(nodes, base_id)
                                : nullptr;
      const auto kind = subobjectDestructionKindFromJson(record.properties);
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      if (!SgCxxSubobjectDestruction::is_valid_target(callee, member, base,
                                                      kind, type))
        throw std::runtime_error(
            "AST JSON subobject destruction has no exact typed target");
      publish_delayed_node(
          record, std::move(positions),
          new SgCxxSubobjectDestruction(callee, member, base, kind, type));
    } else if (record.kind == "SgAllocateStatement" ||
               record.kind == "SgDeallocateStatement") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "expr_list");
      auto *list = isSgExprListExp(require_unowned_delayed_child(
          list_id, record, record.kind + " expr_list"));
      if (list == nullptr)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " expr_list is not an expression list");
      auto operand = [&](const char *field) -> SgExpression * {
        const uint64_t id = singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        auto *child = isSgExpression(require_unowned_delayed_child(
            id, record, record.kind + " " + field));
        if (child == nullptr)
          throw std::runtime_error("AST JSON " + record.kind + " " + field +
                                   " is not an expression");
        return child;
      };
      if (record.kind == "SgAllocateStatement") {
        publish_delayed_node(
            record, std::move(positions),
            new SgAllocateStatement(
                list, operand("stat_expression"), operand("errmsg_expression"),
                operand("source_expression"), operand("mold_expression"),
                operand("stream_expression"), operand("pinned_expression")));
      } else {
        publish_delayed_node(
            record, std::move(positions),
            new SgDeallocateStatement(list, operand("stat_expression"),
                                      operand("errmsg_expression")));
      }
    } else if (record.kind == "SgNullifyStatement") {
      auto *list = isSgExprListExp(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "pointer_list"), record,
          "SgNullifyStatement pointer_list"));
      if (list == nullptr)
        throw std::runtime_error(
            "AST JSON NULLIFY pointer_list is not an expression list");
      publish_delayed_node(record, std::move(positions),
                           new SgNullifyStatement(list));
    } else if (record.kind == "SgAssignStatement" ||
               record.kind == "SgAssignedGotoStatement" ||
               record.kind == "SgComputedGotoStatement") {
      auto operand = [&](const char *field) {
        return require_unowned_delayed_child(
            requiredSingleEdgeTarget(record, field), record,
            record.kind + "." + field);
      };
      if (record.kind == "SgAssignStatement") {
        auto *label = isSgLabelRefExp(operand("label"));
        auto *value = isSgVarRefExp(operand("value"));
        if (label == nullptr || value == nullptr)
          throw std::runtime_error(
              "AST JSON ASSIGN requires an exact label and variable reference");
        auto *statement = new SgAssignStatement(label, value);
        label->set_parent(statement);
        value->set_parent(statement);
        publish_delayed_node(record, std::move(positions), statement);
        return;
      }
      const char *field =
          record.kind == "SgAssignedGotoStatement" ? "targets" : "labelList";
      auto *list = isSgExprListExp(operand(field));
      if (list == nullptr)
        throw std::runtime_error(
            "AST JSON Fortran GOTO requires its exact operand list");
      const NodeRecord &list_record =
          ast.node(requiredSingleEdgeTarget(record, field));
      const auto edges = edgesFor(list_record, "expressions");
      if (edges.empty())
        throw std::runtime_error("AST JSON Fortran GOTO operand list is empty");
      for (size_t i = 0; i < edges.size(); ++i) {
        auto *child = nodeByIdAs<SgExpression>(nodes, edges[i].target);
        const bool selector =
            record.kind == "SgAssignedGotoStatement" && i == 0;
        if ((selector && isSgVarRefExp(child) == nullptr) ||
            (!selector && isSgLabelRefExp(child) == nullptr))
          throw std::runtime_error("AST JSON Fortran GOTO has an invalid typed "
                                   "selector or label reference");
      }
      if (record.kind == "SgAssignedGotoStatement") {
        auto *statement = new SgAssignedGotoStatement(list);
        list->set_parent(statement);
        publish_delayed_node(record, std::move(positions), statement);
      } else {
        auto *index = isSgExpression(operand("label_index"));
        if (index == nullptr)
          throw std::runtime_error(
              "AST JSON computed GOTO has no exact selector expression");
        auto *statement = new SgComputedGotoStatement(list, index);
        list->set_parent(statement);
        index->set_parent(statement);
        publish_delayed_node(record, std::move(positions), statement);
      }
    } else if (record.kind == "SgStaticAssertionDeclaration") {
      const uint64_t condition_id =
          requiredSingleEdgeTarget(record, "condition");
      auto *condition = isSgExpression(require_unowned_delayed_child(
          condition_id, record, "SgStaticAssertionDeclaration condition"));
      if (condition == nullptr)
        throw std::runtime_error(
            "AST JSON static assertion condition is not an expression");
      const uint64_t message_id = singleEdgeTarget(record, "message");
      SgExpression *message = nullptr;
      if (message_id != 0) {
        message = isSgExpression(require_unowned_delayed_child(
            message_id, record, "SgStaticAssertionDeclaration message"));
        if (message == nullptr)
          throw std::runtime_error(
              "AST JSON static assertion message is not an expression");
      }
      auto *declaration = new SgStaticAssertionDeclaration(condition, message);
      publish_delayed_node(record, std::move(positions), declaration);
    } else if (record.kind == "SgAccCollapseClause" ||
               record.kind == "SgAccNumGangsClause" ||
               record.kind == "SgAccNumWorkersClause" ||
               record.kind == "SgAccVectorLengthClause" ||
               record.kind == "SgAccAsyncClause" ||
               record.kind == "SgAccIfClause" ||
               record.kind == "SgAccVectorClause") {
      const bool optional = record.kind == "SgAccAsyncClause" ||
                            record.kind == "SgAccVectorClause";
      const uint64_t expression_id =
          optional ? singleEdgeTarget(record, "expression")
                   : requiredSingleEdgeTarget(record, "expression");
      SgExpression *expression = nullptr;
      if (expression_id != 0) {
        expression = isSgExpression(require_unowned_delayed_child(
            expression_id, record, record.kind + " expression"));
        if (expression == nullptr)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " operand is not an expression");
      }
      SgAccExpressionClause *clause = nullptr;
      if (record.kind == "SgAccCollapseClause")
        clause = new SgAccCollapseClause(expression);
      else if (record.kind == "SgAccNumGangsClause")
        clause = new SgAccNumGangsClause(expression);
      else if (record.kind == "SgAccNumWorkersClause")
        clause = new SgAccNumWorkersClause(expression);
      else if (record.kind == "SgAccVectorLengthClause")
        clause = new SgAccVectorLengthClause(expression);
      else if (record.kind == "SgAccAsyncClause")
        clause = new SgAccAsyncClause(expression);
      else if (record.kind == "SgAccIfClause")
        clause = new SgAccIfClause(expression);
      else if (record.kind == "SgAccVectorClause")
        clause = new SgAccVectorClause(expression);
      ROSE_ASSERT(clause != nullptr);
      if (expression != nullptr)
        expression->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgAccCopyClause" ||
               record.kind == "SgAccCopyinClause" ||
               record.kind == "SgAccCopyoutClause" ||
               record.kind == "SgAccCreateClause" ||
               record.kind == "SgAccPresentClause" ||
               record.kind == "SgAccPrivateClause" ||
               record.kind == "SgAccDeviceptrClause" ||
               record.kind == "SgAccDeleteClause" ||
               record.kind == "SgAccReductionClause") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "variables");
      auto *variables = isSgExprListExp(require_unowned_delayed_child(
          list_id, record, record.kind + " variables"));
      if (variables == nullptr)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " variables are not an expression list");
      SgAccVariablesClause *clause = nullptr;
      if (record.kind == "SgAccCopyClause")
        clause = new SgAccCopyClause(variables);
      else if (record.kind == "SgAccCopyinClause")
        clause = new SgAccCopyinClause(variables);
      else if (record.kind == "SgAccCopyoutClause")
        clause = new SgAccCopyoutClause(variables);
      else if (record.kind == "SgAccCreateClause")
        clause = new SgAccCreateClause(variables);
      else if (record.kind == "SgAccPresentClause")
        clause = new SgAccPresentClause(variables);
      else if (record.kind == "SgAccPrivateClause")
        clause = new SgAccPrivateClause(variables);
      else if (record.kind == "SgAccDeviceptrClause")
        clause = new SgAccDeviceptrClause(variables);
      else if (record.kind == "SgAccDeleteClause")
        clause = new SgAccDeleteClause(variables);
      else if (record.kind == "SgAccReductionClause")
        clause = new SgAccReductionClause(
            variables, static_cast<int>(record.properties.requiredInt(
                           "reduction_operator")));
      ROSE_ASSERT(clause != nullptr);
      variables->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgAccWaitStatement") {
      const uint64_t list_id = singleEdgeTarget(record, "wait_list");
      const uint64_t devnum_id = singleEdgeTarget(record, "devnum");
      SgExprListExp *wait_list = nullptr;
      SgExpression *devnum = nullptr;
      if (list_id != 0) {
        wait_list = isSgExprListExp(require_unowned_delayed_child(
            list_id, record, "SgAccWaitStatement wait list"));
        if (wait_list == nullptr)
          throw std::runtime_error("AST JSON OpenACC wait list is not a list");
      }
      if (devnum_id != 0) {
        devnum = isSgExpression(require_unowned_delayed_child(
            devnum_id, record, "SgAccWaitStatement device number"));
        if (devnum == nullptr)
          throw std::runtime_error(
              "AST JSON OpenACC wait device number is not an expression");
      }
      auto *statement = new SgAccWaitStatement(
          wait_list, devnum, record.properties.requiredBool("queues"));
      if (wait_list != nullptr)
        wait_list->set_parent(statement);
      if (devnum != nullptr)
        devnum->set_parent(statement);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgAccCacheStatement") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "variables");
      auto *variables = isSgExprListExp(require_unowned_delayed_child(
          list_id, record, "SgAccCacheStatement variables"));
      if (variables == nullptr)
        throw std::runtime_error(
            "AST JSON OpenACC cache variables are not a list");
      auto *statement = new SgAccCacheStatement(
          variables,
          static_cast<int>(record.properties.requiredInt("modifier")));
      variables->set_parent(statement);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgAccParallelStatement" ||
               record.kind == "SgAccParallelLoopStatement" ||
               record.kind == "SgAccDataStatement" ||
               record.kind == "SgAccKernelsStatement" ||
               record.kind == "SgAccAtomicStatement") {
      auto *body = isSgStatement(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "body"), record, "OpenACC body"));
      if (body == nullptr)
        throw std::runtime_error(
            "AST JSON OpenACC body has no unowned exact statement");
      SgAccBodyStatement *statement = nullptr;
      if (record.kind == "SgAccParallelStatement")
        statement = new SgAccParallelStatement(body);
      else if (record.kind == "SgAccParallelLoopStatement")
        statement = new SgAccParallelLoopStatement(body);
      else if (record.kind == "SgAccDataStatement")
        statement = new SgAccDataStatement(body);
      else if (record.kind == "SgAccKernelsStatement")
        statement = new SgAccKernelsStatement(body);
      else if (record.kind == "SgAccAtomicStatement")
        statement = new SgAccAtomicStatement(body);
      ROSE_ASSERT(statement != nullptr);
      body->set_parent(statement);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (kindInVariants(record.kind, image_control_variants)) {
      auto operand = [&](const char *field, bool required) -> SgExpression * {
        const uint64_t id = required ? requiredSingleEdgeTarget(record, field)
                                     : singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        auto *expression = isSgExpression(require_unowned_delayed_child(
            id, record, record.kind + " " + field));
        if (expression == nullptr)
          throw std::runtime_error("AST JSON " + record.kind + " " + field +
                                   " is not an expression");
        return expression;
      };
      auto *stat = operand("stat", false);
      auto *err_msg = operand("err_msg", false);
      SgImageControlStatement *statement = nullptr;
      if (record.kind == "SgSyncAllStatement")
        statement = new SgSyncAllStatement(stat, err_msg);
      else if (record.kind == "SgSyncMemoryStatement")
        statement = new SgSyncMemoryStatement(stat, err_msg);
      else if (record.kind == "SgSyncImagesStatement")
        statement = new SgSyncImagesStatement(
            operand("image_set", false),
            record.properties.requiredBool("all_images"), stat, err_msg);
      else if (record.kind == "SgSyncTeamStatement")
        statement =
            new SgSyncTeamStatement(operand("team_value", true), stat, err_msg);
      else if (record.kind == "SgLockStatement")
        statement =
            new SgLockStatement(operand("lock_variable", true),
                                operand("acquired_lock", false), stat, err_msg);
      else if (record.kind == "SgUnlockStatement")
        statement = new SgUnlockStatement(operand("lock_variable", true), stat,
                                          err_msg);
      else if (record.kind == "SgFormTeamStatement")
        statement = new SgFormTeamStatement(
            operand("team_number", true), operand("team_variable", true),
            operand("new_index", false), stat, err_msg);
      ROSE_ASSERT(statement != nullptr);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgProcessControlStatement") {
      const auto &prepared = construction_fields.at(record.id).process_control;
      ROSE_ASSERT(prepared.has_value());
      const auto &input = *prepared;
      auto operand = [&](uint64_t id, const char *field) -> SgExpression * {
        if (id == 0)
          return nullptr;
        auto *expression =
            isSgExpression(require_unowned_delayed_child(id, record, field));
        if (expression == nullptr)
          throw std::runtime_error("AST JSON process control operand is not an "
                                   "expression");
        return expression;
      };
      publish_delayed_node(
          record, std::move(positions),
          new SgProcessControlStatement(input.kind, operand(input.code, "code"),
                                        operand(input.quiet, "quiet")));
    } else if (record.kind == "SgOmpNowaitClause" ||
               record.kind == "SgOmpOrderedClause" ||
               record.kind == "SgOmpAlignClause" ||
               record.kind == "SgOmpMessageClause" ||
               record.kind == "SgOmpGraphIdClause" ||
               record.kind == "SgOmpGraphResetClause" ||
               record.kind == "SgOmpTransparentClause" ||
               record.kind == "SgOmpThreadsetClause" ||
               record.kind == "SgOmpSafesyncClause" ||
               record.kind == "SgOmpLooprangeClause" ||
               record.kind == "SgOmpNoOpenmpConstructsClause" ||
               record.kind == "SgOmpHoldsClause" ||
               record.kind == "SgOmpUseClause") {
      const uint64_t expression_id = singleEdgeTarget(record, "expression");
      SgExpression *expression = nullptr;
      if (expression_id != 0) {
        expression = isSgExpression(require_unowned_delayed_child(
            expression_id, record, record.kind + " expression"));
        if (expression == nullptr)
          throw std::runtime_error("AST JSON " + record.kind +
                                   " operand is not an expression");
      }
      SgOmpExpressionClause *clause = nullptr;
      if (record.kind == "SgOmpNowaitClause")
        clause = new SgOmpNowaitClause(expression);
      else if (record.kind == "SgOmpOrderedClause")
        clause = new SgOmpOrderedClause(expression);
      else if (record.kind == "SgOmpAlignClause")
        clause = new SgOmpAlignClause(expression);
      else if (record.kind == "SgOmpMessageClause")
        clause = new SgOmpMessageClause(expression);
      else if (record.kind == "SgOmpGraphIdClause")
        clause = new SgOmpGraphIdClause(expression);
      else if (record.kind == "SgOmpGraphResetClause")
        clause = new SgOmpGraphResetClause(expression);
      else if (record.kind == "SgOmpTransparentClause")
        clause = new SgOmpTransparentClause(expression);
      else if (record.kind == "SgOmpThreadsetClause")
        clause = new SgOmpThreadsetClause(expression);
      else if (record.kind == "SgOmpSafesyncClause")
        clause = new SgOmpSafesyncClause(expression);
      else if (record.kind == "SgOmpLooprangeClause")
        clause = new SgOmpLooprangeClause(expression);
      else if (record.kind == "SgOmpNoOpenmpConstructsClause")
        clause = new SgOmpNoOpenmpConstructsClause(expression);
      else if (record.kind == "SgOmpHoldsClause")
        clause = new SgOmpHoldsClause(expression);
      else if (record.kind == "SgOmpUseClause")
        clause = new SgOmpUseClause(expression);
      ROSE_ASSERT(clause != nullptr);
      if (expression != nullptr)
        expression->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpSizesClause") {
      const uint64_t list_id = requiredSingleEdgeTarget(record, "expression");
      auto *expressions = isSgExprListExp(require_unowned_delayed_child(
          list_id, record, "SgOmpSizesClause expression"));
      if (expressions == nullptr)
        throw std::runtime_error(
            "AST JSON OpenMP sizes operand is not an expression list");
      auto *clause = new SgOmpSizesClause(expressions);
      expressions->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpGrainsizeClause" ||
               record.kind == "SgOmpNumTasksClause" ||
               record.kind == "SgOmpDeviceClause") {
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expression");
      auto *expression = isSgExpression(require_unowned_delayed_child(
          expression_id, record, record.kind + " expression"));
      if (expression == nullptr)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " operand is not an expression");
      SgOmpExpressionClause *clause = nullptr;
      if (record.kind == "SgOmpGrainsizeClause") {
        const auto modifier =
            requiredEnum<SgOmpClause::omp_grainsize_modifier_enum>(
                record.properties, "modifier", record.kind,
                {SgOmpClause::e_omp_grainsize_modifier_unspecified,
                 SgOmpClause::e_omp_grainsize_modifier_strict});
        clause = new SgOmpGrainsizeClause(expression, modifier);
      } else if (record.kind == "SgOmpNumTasksClause") {
        const auto modifier =
            requiredEnum<SgOmpClause::omp_num_tasks_modifier_enum>(
                record.properties, "modifier", record.kind,
                {SgOmpClause::e_omp_num_tasks_modifier_unspecified,
                 SgOmpClause::e_omp_num_tasks_modifier_strict});
        clause = new SgOmpNumTasksClause(expression, modifier);
      } else if (record.kind == "SgOmpDeviceClause") {
        const auto modifier =
            requiredEnum<SgOmpClause::omp_device_modifier_enum>(
                record.properties, "modifier", record.kind,
                {SgOmpClause::e_omp_device_modifier_unspecified,
                 SgOmpClause::e_omp_device_modifier_ancestor,
                 SgOmpClause::e_omp_device_modifier_device_num});
        clause = new SgOmpDeviceClause(expression, modifier);
      }
      ROSE_ASSERT(clause != nullptr);
      expression->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpScheduleClause" ||
               record.kind == "SgOmpDistScheduleClause") {
      const uint64_t chunk_id = singleEdgeTarget(record, "chunk_size");
      SgExpression *chunk = nullptr;
      if (chunk_id != 0) {
        chunk = isSgExpression(
            require_unowned_delayed_child(chunk_id, record, "schedule chunk"));
        if (chunk == nullptr)
          throw std::runtime_error(
              "AST JSON schedule chunk is not an expression");
      }
      const auto &fields = construction_fields.at(record.id);
      SgOmpClause *clause;
      if (record.kind == "SgOmpScheduleClause") {
        ROSE_ASSERT(fields.schedule.has_value());
        const auto &schedule = *fields.schedule;
        clause = new SgOmpScheduleClause(schedule.modifier, schedule.modifier1,
                                         schedule.kind, chunk);
      } else {
        ROSE_ASSERT(fields.dist_schedule.has_value());
        clause = new SgOmpDistScheduleClause(*fields.dist_schedule, chunk);
      }
      if (chunk != nullptr)
        chunk->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpIfClause") {
      const auto modifier = requiredEnum<SgOmpClause::omp_if_modifier_enum>(
          record.properties, "modifier", "SgOmpIfClause",
          {SgOmpClause::e_omp_if_modifier_unknown,
           SgOmpClause::e_omp_if_parallel, SgOmpClause::e_omp_if_simd,
           SgOmpClause::e_omp_if_target, SgOmpClause::e_omp_if_cancel,
           SgOmpClause::e_omp_if_taskloop, SgOmpClause::e_omp_if_target_data,
           SgOmpClause::e_omp_if_target_enter_data,
           SgOmpClause::e_omp_if_target_exit_data, SgOmpClause::e_omp_if_task,
           SgOmpClause::e_omp_if_target_update});
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expression");
      auto *expression = isSgExpression(require_unowned_delayed_child(
          expression_id, record, "SgOmpIfClause expression"));
      if (expression == nullptr)
        throw std::runtime_error(
            "AST JSON OpenMP if operand is not an expression");
      auto *clause = new SgOmpIfClause(expression, modifier);
      expression->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpCollapseClause" ||
               record.kind == "SgOmpNumThreadsClause" ||
               record.kind == "SgOmpNumTeamsClause" ||
               record.kind == "SgOmpSafelenClause" ||
               record.kind == "SgOmpSimdlenClause" ||
               record.kind == "SgOmpDetachClause" ||
               record.kind == "SgOmpFinalClause" ||
               record.kind == "SgOmpPriorityClause" ||
               record.kind == "SgOmpHintClause" ||
               record.kind == "SgOmpThreadLimitClause" ||
               record.kind == "SgOmpNocontextClause" ||
               record.kind == "SgOmpNovariantsClause" ||
               record.kind == "SgOmpFilterClause" ||
               record.kind == "SgOmpPartialClause") {
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expression");
      auto *expression = isSgExpression(require_unowned_delayed_child(
          expression_id, record, record.kind + " expression"));
      if (expression == nullptr)
        throw std::runtime_error("AST JSON " + record.kind +
                                 " operand is not an expression");
      SgOmpExpressionClause *clause = nullptr;
      if (record.kind == "SgOmpCollapseClause")
        clause = new SgOmpCollapseClause(expression);
      else if (record.kind == "SgOmpNumThreadsClause")
        clause = new SgOmpNumThreadsClause(expression);
      else if (record.kind == "SgOmpNumTeamsClause")
        clause = new SgOmpNumTeamsClause(expression);
      else if (record.kind == "SgOmpSafelenClause")
        clause = new SgOmpSafelenClause(expression);
      else if (record.kind == "SgOmpSimdlenClause")
        clause = new SgOmpSimdlenClause(expression);
      else if (record.kind == "SgOmpDetachClause")
        clause = new SgOmpDetachClause(expression);
      else if (record.kind == "SgOmpFinalClause")
        clause = new SgOmpFinalClause(expression);
      else if (record.kind == "SgOmpPriorityClause")
        clause = new SgOmpPriorityClause(expression);
      else if (record.kind == "SgOmpHintClause")
        clause = new SgOmpHintClause(expression);
      else if (record.kind == "SgOmpThreadLimitClause")
        clause = new SgOmpThreadLimitClause(expression);
      else if (record.kind == "SgOmpNocontextClause")
        clause = new SgOmpNocontextClause(expression);
      else if (record.kind == "SgOmpNovariantsClause")
        clause = new SgOmpNovariantsClause(expression);
      else if (record.kind == "SgOmpFilterClause")
        clause = new SgOmpFilterClause(expression);
      else if (record.kind == "SgOmpPartialClause")
        clause = new SgOmpPartialClause(expression);
      ROSE_ASSERT(clause != nullptr);
      expression->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpDoacrossClause") {
      const uint64_t expression_id =
          requiredSingleEdgeTarget(record, "expressions");
      auto *expressions = isSgExprListExp(require_unowned_delayed_child(
          expression_id, record, "SgOmpDoacrossClause expressions"));
      if (expressions == nullptr)
        throw std::runtime_error(
            "AST JSON OpenMP doacross expressions are not a list");
      auto *clause = new SgOmpDoacrossClause(
          ompDoacrossKindFromJson(record.properties), expressions);
      expressions->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpDefaultClause" ||
               record.kind == "SgOmpOtherwiseClause") {
      const uint64_t directive_id =
          singleEdgeTarget(record, "variant_directive");
      SgStatement *directive = nullptr;
      if (directive_id != 0) {
        directive = isSgStatement(require_unowned_delayed_child(
            directive_id, record, record.kind + " variant_directive"));
        if (directive == nullptr)
          throw std::runtime_error(
              "AST JSON OpenMP variant is not a statement");
      }
      SgOmpClause *clause = nullptr;
      if (record.kind == "SgOmpDefaultClause") {
        clause = new SgOmpDefaultClause(
            ompDefaultOptionFromJson(record.properties), directive);
      } else {
        clause = new SgOmpOtherwiseClause(directive);
      }
      if (directive != nullptr)
        directive->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpInitModifier") {
      const uint64_t expression_id = singleEdgeTarget(record, "expression");
      SgExpression *expression = nullptr;
      if (expression_id != 0) {
        expression = isSgExpression(require_unowned_delayed_child(
            expression_id, record, record.kind + " expression"));
        if (expression == nullptr)
          throw std::runtime_error(
              "AST JSON OpenMP modifier expression has the wrong node kind");
      }
      const auto &kind = construction_fields.at(record.id).init_modifier;
      ROSE_ASSERT(kind.has_value());
      std::string detail;
      if (!Rose::OpenMP::Detail::validateInitModifierExpression(
              *kind, expression, &detail)) {
        static const VariantVector clauses(V_SgOmpClause);
        const NodeRecord *owner = &record;
        std::unordered_set<uint64_t> owners;
        while (!kindInVariants(owner->kind, clauses)) {
          if (!owners.insert(owner->id).second)
            throw std::runtime_error("AST JSON modifier has cyclic ownership");
          const uint64_t parent = singleEdgeTarget(*owner, "parent");
          if (parent == 0)
            break;
          owner = &ast.node(parent);
        }
        throw std::runtime_error("AST JSON reconstructed malformed " +
                                 owner->kind + ": " + detail);
      }
      auto *node = new SgOmpInitModifier(*kind, expression);
      if (expression != nullptr)
        expression->set_parent(node);
      publish_delayed_node(record, std::move(positions), node);
    } else if (record.kind == "SgOmpInitModifierList") {
      SgOmpInitModifierPtrList modifiers;
      for (const auto &edge : edgesFor(record, "modifiers")) {
        require_unowned_delayed_child(edge.target, record, "init modifier");
        modifiers.push_back(nodeByIdAs<SgOmpInitModifier>(nodes, edge.target));
      }
      auto *list = new SgOmpInitModifierList();
      list->get_modifiers() = std::move(modifiers);
      for (auto *modifier : list->get_modifiers())
        modifier->set_parent(list);
      publish_delayed_node(record, std::move(positions), list);
    } else if (record.kind == "SgOmpIteratorDefinition") {
      auto child = [&](uint64_t id, const char *field) -> SgExpression * {
        if (id == 0)
          return nullptr;
        return isSgExpression(require_unowned_delayed_child(
            id, record, record.kind + " " + field));
      };
      const uint64_t type_id = singleEdgeTarget(record, "iterator_type");
      const uint64_t step_id = singleEdgeTarget(record, "step");
      auto *type = isSgTypeExpression(child(type_id, "iterator_type"));
      auto *name = isSgOmpNameExpression(child(
          requiredSingleEdgeTarget(record, "iterator_name"), "iterator_name"));
      auto *begin = child(requiredSingleEdgeTarget(record, "begin"), "begin");
      auto *end = child(requiredSingleEdgeTarget(record, "end"), "end");
      auto *step = child(step_id, "step");
      if ((type_id != 0 && type == nullptr) || name == nullptr ||
          name->get_spelling().empty() || begin == nullptr || end == nullptr ||
          (step_id != 0 && step == nullptr))
        throw std::runtime_error(
            "AST JSON OpenMP iterator has incomplete typed fields");
      auto *definition =
          new SgOmpIteratorDefinition(type, name, begin, end, step);
      publish_delayed_node(record, std::move(positions), definition);
    } else if (record.kind == "SgOmpMapClause" ||
               record.kind == "SgOmpToClause" ||
               record.kind == "SgOmpFromClause" ||
               record.kind == "SgOmpDependClause" ||
               record.kind == "SgOmpAffinityClause") {
      auto variables = [&](const char *role) -> SgExprListExp * {
        const uint64_t id = singleEdgeTarget(record, role);
        if (id == 0)
          return nullptr;
        require_unowned_delayed_child(id, record, role);
        return nodeByIdAs<SgExprListExp>(nodes, id);
      };
      auto *list = variables("variables");
      auto *source_list = variables("source_variables");
      auto *sink_vectors = record.kind == "SgOmpDependClause"
                               ? variables("sink_vectors")
                               : nullptr;
      SgOmpNameExpression *mapper = nullptr;
      if (const uint64_t id = singleEdgeTarget(record, "mapper_identifier")) {
        require_unowned_delayed_child(id, record, "transfer mapper");
        mapper = nodeByIdAs<SgOmpNameExpression>(nodes, id);
      }
      SgOmpIteratorDefinitionPtrList iterators;
      for (const auto &edge : edgesFor(record, "iterator_definitions")) {
        require_unowned_delayed_child(edge.target, record, "transfer iterator");
        iterators.push_back(
            nodeByIdAs<SgOmpIteratorDefinition>(nodes, edge.target));
      }
      const auto &fields = construction_fields.at(record.id);
      ROSE_ASSERT(list != nullptr);
      auto initialize = [&](auto *clause, bool source_override) {
        clause->set_has_source_variables_override(source_override);
        clause->set_source_variables(source_list);
        clause->get_iterator_definitions() = std::move(iterators);
        list->set_parent(clause);
        if (source_list != nullptr)
          source_list->set_parent(clause);
        if (mapper != nullptr)
          mapper->set_parent(clause);
        for (auto *iterator : clause->get_iterator_definitions())
          iterator->set_parent(clause);
      };
      SgOmpVariablesClause *clause;
      if (record.kind == "SgOmpMapClause") {
        ROSE_ASSERT(fields.map.has_value());
        auto *map = new SgOmpMapClause(list, fields.map->operation);
        map->set_modifier1(fields.map->modifiers[0]);
        map->set_modifier2(fields.map->modifiers[1]);
        map->set_modifier3(fields.map->modifiers[2]);
        map->set_mapper_identifier(mapper);
        initialize(map, fields.map->source_variables_override);
        clause = map;
      } else if (record.kind == "SgOmpToClause") {
        ROSE_ASSERT(fields.to.has_value());
        auto *to = new SgOmpToClause(list, fields.to->kind);
        to->set_declare_target_extended_list(
            fields.to->declare_target_extended_list);
        to->set_mapper_identifier(mapper);
        initialize(to, fields.to->source_variables_override);
        clause = to;
      } else if (record.kind == "SgOmpFromClause") {
        ROSE_ASSERT(fields.from.has_value());
        auto *from = new SgOmpFromClause(list, fields.from->kind);
        from->set_mapper_identifier(mapper);
        initialize(from, fields.from->source_variables_override);
        clause = from;
      } else if (record.kind == "SgOmpDependClause") {
        ROSE_ASSERT(fields.depend.has_value());
        auto *depend = new SgOmpDependClause(list, fields.depend->modifier,
                                             fields.depend->type);
        depend->set_sink_vectors(sink_vectors);
        if (sink_vectors != nullptr)
          sink_vectors->set_parent(depend);
        initialize(depend, fields.depend->source_variables_override);
        clause = depend;
      } else {
        ROSE_ASSERT(fields.affinity.has_value());
        auto *affinity =
            new SgOmpAffinityClause(list, fields.affinity->modifier);
        initialize(affinity, fields.affinity->source_variables_override);
        clause = affinity;
      }
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpReductionClause" ||
               record.kind == "SgOmpInReductionClause" ||
               record.kind == "SgOmpTaskReductionClause") {
      const uint64_t variables_id =
          requiredSingleEdgeTarget(record, "variables");
      require_unowned_delayed_child(variables_id, record,
                                    "reduction variables");
      auto *variables = nodeByIdAs<SgExprListExp>(nodes, variables_id);
      SgExprListExp *source_variables = nullptr;
      if (const uint64_t id = singleEdgeTarget(record, "source_variables")) {
        require_unowned_delayed_child(id, record, "reduction source variables");
        source_variables = nodeByIdAs<SgExprListExp>(nodes, id);
      }
      SgOmpNameExpression *identifier = nullptr;
      if (const uint64_t id =
              singleEdgeTarget(record, "user_defined_identifier")) {
        require_unowned_delayed_child(id, record, "reduction identifier");
        identifier = nodeByIdAs<SgOmpNameExpression>(nodes, id);
      }
      const auto &fields = construction_fields.at(record.id);
      SgOmpVariablesClause *clause;
      bool source_override;
      if (record.kind == "SgOmpReductionClause") {
        ROSE_ASSERT(fields.reduction.has_value() &&
                    fields.reduction_modifier.has_value());
        clause =
            new SgOmpReductionClause(variables, *fields.reduction_modifier,
                                     fields.reduction->identifier, identifier);
        source_override = fields.reduction->source_variables_override;
      } else if (record.kind == "SgOmpInReductionClause") {
        ROSE_ASSERT(fields.in_reduction.has_value());
        clause = new SgOmpInReductionClause(
            variables, fields.in_reduction->identifier, identifier);
        source_override = fields.in_reduction->source_variables_override;
      } else {
        ROSE_ASSERT(fields.task_reduction.has_value());
        clause = new SgOmpTaskReductionClause(
            variables, fields.task_reduction->identifier, identifier);
        source_override = fields.task_reduction->source_variables_override;
      }
      clause->set_has_source_variables_override(source_override);
      clause->set_source_variables(source_variables);
      variables->set_parent(clause);
      if (source_variables != nullptr)
        source_variables->set_parent(clause);
      if (identifier != nullptr)
        identifier->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpAllocateClause" ||
               record.kind == "SgOmpAllocatorClause") {
      auto expression = [&](const char *role) -> SgExpression * {
        const uint64_t id = singleEdgeTarget(record, role);
        if (id == 0)
          return nullptr;
        require_unowned_delayed_child(id, record, role);
        return nodeByIdAs<SgExpression>(nodes, id);
      };
      auto *modifier = expression("user_defined_modifier");
      const auto &fields = construction_fields.at(record.id);
      SgOmpClause *clause;
      if (record.kind == "SgOmpAllocateClause") {
        const uint64_t id = requiredSingleEdgeTarget(record, "variables");
        require_unowned_delayed_child(id, record, "allocate variables");
        auto *variables = nodeByIdAs<SgExprListExp>(nodes, id);
        SgExprListExp *source = nullptr;
        if (const uint64_t source_id =
                singleEdgeTarget(record, "source_variables")) {
          require_unowned_delayed_child(source_id, record,
                                        "allocate source variables");
          source = nodeByIdAs<SgExprListExp>(nodes, source_id);
        }
        auto *alignment = expression("alignment");
        ROSE_ASSERT(fields.allocate.has_value());
        auto *allocate = new SgOmpAllocateClause(
            variables, fields.allocate->modifier, modifier);
        allocate->set_uses_allocator_modifier_syntax(
            fields.allocate->modifier_syntax);
        allocate->set_has_source_variables_override(
            fields.allocate->source_variables_override);
        allocate->set_source_variables(source);
        allocate->set_alignment(alignment);
        variables->set_parent(allocate);
        if (source != nullptr)
          source->set_parent(allocate);
        if (alignment != nullptr)
          alignment->set_parent(allocate);
        clause = allocate;
      } else {
        ROSE_ASSERT(fields.allocator.has_value());
        clause = new SgOmpAllocatorClause(*fields.allocator, modifier);
      }
      if (modifier != nullptr)
        modifier->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (ordinaryOmpVariablesClauseVariant(record.kind) ||
               record.kind == "SgOmpLinearClause" ||
               record.kind == "SgOmpAlignedClause") {
      const auto &fields = construction_fields.at(record.id);
      const uint64_t variables_id =
          requiredSingleEdgeTarget(record, "variables");
      require_unowned_delayed_child(variables_id, record,
                                    record.kind + " variables");
      auto *variables = nodeByIdAs<SgExprListExp>(nodes, variables_id);
      SgExprListExp *source = nullptr;
      if (const uint64_t id = singleEdgeTarget(record, "source_variables")) {
        require_unowned_delayed_child(id, record,
                                      record.kind + " source variables");
        source = nodeByIdAs<SgExprListExp>(nodes, id);
      }
      SgExpression *operand = nullptr;
      if (!fields.variables) {
        const char *role =
            record.kind == "SgOmpLinearClause" ? "step" : "alignment";
        if (const uint64_t id = singleEdgeTarget(record, role)) {
          require_unowned_delayed_child(id, record, role);
          operand = nodeByIdAs<SgExpression>(nodes, id);
        }
      }
      SgOmpVariablesClause *clause;
      bool source_override;
      if (fields.variables) {
        source_override = fields.variables->source_variables_override;
        switch (fields.variables->kind) {
        case V_SgOmpLinkClause:
          clause = new SgOmpLinkClause(variables);
          break;
        case V_SgOmpEnterClause:
          clause = new SgOmpEnterClause(variables);
          break;
        case V_SgOmpLocalClause:
          clause = new SgOmpLocalClause(variables);
          break;
        case V_SgOmpPrivateClause:
          clause = new SgOmpPrivateClause(variables);
          break;
        case V_SgOmpFirstprivateClause: {
          ROSE_ASSERT(fields.firstprivate_saved.has_value());
          auto *firstprivate = new SgOmpFirstprivateClause(variables);
          firstprivate->set_saved(*fields.firstprivate_saved);
          clause = firstprivate;
          break;
        }
        case V_SgOmpCopyinClause:
          clause = new SgOmpCopyinClause(variables);
          break;
        case V_SgOmpLastprivateClause:
          ROSE_ASSERT(fields.lastprivate_modifier.has_value());
          clause = new SgOmpLastprivateClause(variables,
                                              *fields.lastprivate_modifier);
          break;
        case V_SgOmpSharedClause:
          clause = new SgOmpSharedClause(variables);
          break;
        case V_SgOmpCopyprivateClause:
          clause = new SgOmpCopyprivateClause(variables);
          break;
        case V_SgOmpUniformClause:
          clause = new SgOmpUniformClause(variables);
          break;
        case V_SgOmpNontemporalClause:
          clause = new SgOmpNontemporalClause(variables);
          break;
        case V_SgOmpIsDevicePtrClause:
          clause = new SgOmpIsDevicePtrClause(variables);
          break;
        case V_SgOmpUseDevicePtrClause:
          clause = new SgOmpUseDevicePtrClause(variables);
          break;
        case V_SgOmpUseDeviceAddrClause:
          clause = new SgOmpUseDeviceAddrClause(variables);
          break;
        case V_SgOmpHasDeviceAddrClause:
          clause = new SgOmpHasDeviceAddrClause(variables);
          break;
        case V_SgOmpExclusiveClause:
          clause = new SgOmpExclusiveClause(variables);
          break;
        case V_SgOmpInclusiveClause:
          clause = new SgOmpInclusiveClause(variables);
          break;
        default:
          throw std::runtime_error(
              "AST JSON has an unsupported variable clause kind");
        }
      } else if (record.kind == "SgOmpLinearClause") {
        ROSE_ASSERT(fields.linear.has_value());
        clause =
            new SgOmpLinearClause(variables, operand, fields.linear->modifier);
        source_override = fields.linear->source_variables_override;
      } else {
        ROSE_ASSERT(fields.aligned_source_variables_override.has_value());
        clause = new SgOmpAlignedClause(variables, operand);
        source_override = *fields.aligned_source_variables_override;
      }
      clause->set_has_source_variables_override(source_override);
      clause->set_source_variables(source);
      variables->set_parent(clause);
      if (source != nullptr)
        source->set_parent(clause);
      if (operand != nullptr)
        operand->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpAdjustArgsClause") {
      auto *arguments = isSgExprListExp(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "arguments"), record,
          "adjust_args arguments"));
      ROSE_ASSERT(arguments != nullptr &&
                  !arguments->get_expressions().empty());
      const auto &modifier = construction_fields.at(record.id).adjust_args;
      ROSE_ASSERT(modifier.has_value());
      auto *clause = new SgOmpAdjustArgsClause(arguments, *modifier);
      arguments->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpInitClause" ||
               record.kind == "SgOmpAppendArgsOperation") {
      const uint64_t modifier_id =
          requiredSingleEdgeTarget(record, "modifier_list");
      auto *modifiers = isSgOmpInitModifierList(require_unowned_delayed_child(
          modifier_id, record, record.kind + " modifier list"));
      if (modifiers == nullptr)
        throw std::runtime_error(
            "AST JSON OpenMP modifier list has the wrong node kind");
      SgNode *node = nullptr;
      if (record.kind == "SgOmpInitClause") {
        const uint64_t operand_id = requiredSingleEdgeTarget(record, "operand");
        auto *operand = isSgExpression(require_unowned_delayed_child(
            operand_id, record, "SgOmpInitClause operand"));
        if (operand == nullptr)
          throw std::runtime_error(
              "AST JSON OpenMP init operand is not an expression");
        node = new SgOmpInitClause(modifiers, operand);
        operand->set_parent(node);
      } else {
        std::string detail;
        if (!Rose::OpenMP::Detail::validateInitModifierList(
                modifiers, nullptr,
                Rose::OpenMP::Detail::InitModifierContext::AppendArgs, &detail))
          throw std::runtime_error(
              "AST JSON reconstructed malformed SgOmpAppendArgsClause: " +
              detail);
        node = new SgOmpAppendArgsOperation(modifiers);
      }
      modifiers->set_parent(node);
      publish_delayed_node(record, std::move(positions), node);
    } else if (record.kind == "SgOmpAppendArgsClause") {
      SgOmpAppendArgsOperationPtrList operations;
      for (const auto &edge : edgesFor(record, "interop_operations")) {
        require_unowned_delayed_child(edge.target, record,
                                      "append_args operation");
        operations.push_back(
            nodeByIdAs<SgOmpAppendArgsOperation>(nodes, edge.target));
      }
      auto *clause = new SgOmpAppendArgsClause();
      clause->get_interop_operations() = std::move(operations);
      for (auto *operation : clause->get_interop_operations())
        operation->set_parent(clause);
      publish_delayed_node(record, std::move(positions), clause);
    } else if (record.kind == "SgOmpDestroyClause") {
      const uint64_t expression_id = singleEdgeTarget(record, "expression");
      SgExpression *expression = nullptr;
      if (expression_id != 0) {
        expression = isSgExpression(require_unowned_delayed_child(
            expression_id, record, "SgOmpDestroyClause expression"));
        if (expression == nullptr)
          throw std::runtime_error(
              "AST JSON OpenMP destroy operand is not an expression");
      }
      publish_delayed_node(record, std::move(positions),
                           new SgOmpDestroyClause(expression));
    } else if (record.kind == "SgOmpContextSelectorProperty") {
      const auto &fields = construction_fields.at(record.id).context_property;
      ROSE_ASSERT(fields.has_value());
      const auto expression = [&](const char *role) -> SgExpression * {
        const uint64_t id = singleEdgeTarget(record, role);
        if (id == 0)
          return nullptr;
        require_unowned_delayed_child(id, record, role);
        return nodeByIdAs<SgExpression>(nodes, id);
      };
      auto *operand = expression("expression");
      auto *requires_operand = expression("requires_expression");
      auto *property = new SgOmpContextSelectorProperty;
      with_delayed_node_ownership(record, property, [&] {
        fields->initialize(property, operand, requires_operand);
        if (operand != nullptr)
          operand->set_parent(property);
        if (requires_operand != nullptr)
          requires_operand->set_parent(property);
        publishConstructorOnlyNode(record, property, nodes,
                                   std::move(positions),
                                   construction_fields.at(record.id));
      });
    } else if (record.kind == "SgOmpContextSelector") {
      const auto &fields = construction_fields.at(record.id).context_selector;
      ROSE_ASSERT(fields.has_value());
      SgExpression *score = nullptr;
      if (const uint64_t id = singleEdgeTarget(record, "score")) {
        require_unowned_delayed_child(id, record, "context score");
        score = nodeByIdAs<SgExpression>(nodes, id);
      }
      SgStatement *directive = nullptr;
      if (const uint64_t id = singleEdgeTarget(record, "construct_directive")) {
        require_unowned_delayed_child(id, record, "context construct");
        directive = nodeByIdAs<SgStatement>(nodes, id);
      }
      SgOmpContextSelectorPropertyPtrList properties;
      for (const auto &edge : edgesFor(record, "properties")) {
        require_unowned_delayed_child(edge.target, record, "context property");
        properties.push_back(
            nodeByIdAs<SgOmpContextSelectorProperty>(nodes, edge.target));
      }
      validateOmpContextSelectorPayload(fields->kind,
                                        fields->implementation_name, score,
                                        directive, properties);
      auto *selector = new SgOmpContextSelector(fields->kind);
      with_delayed_node_ownership(record, selector, [&] {
        selector->set_implementation_defined_name(fields->implementation_name);
        selector->set_score(score);
        selector->set_construct_directive(directive);
        selector->get_properties() = std::move(properties);
        for (auto *property : selector->get_properties())
          property->set_parent(selector);
        if (score != nullptr)
          score->set_parent(selector);
        if (directive != nullptr)
          directive->set_parent(selector);
        publishConstructorOnlyNode(record, selector, nodes,
                                   std::move(positions),
                                   construction_fields.at(record.id));
      });
    } else if (record.kind == "SgOmpContextSelectorSet") {
      const auto &kind = construction_fields.at(record.id).context_set_kind;
      ROSE_ASSERT(kind.has_value());
      SgOmpContextSelectorPtrList selectors;
      for (const auto &edge : edgesFor(record, "selectors")) {
        require_unowned_delayed_child(edge.target, record, "context selector");
        selectors.push_back(
            nodeByIdAs<SgOmpContextSelector>(nodes, edge.target));
      }
      validateOmpContextSelectorSetPayload(*kind, selectors);
      auto *set = new SgOmpContextSelectorSet(*kind);
      with_delayed_node_ownership(record, set, [&] {
        set->get_selectors() = std::move(selectors);
        for (auto *selector : set->get_selectors())
          selector->set_parent(set);
        publishConstructorOnlyNode(record, set, nodes, std::move(positions),
                                   construction_fields.at(record.id));
      });
    } else if (record.kind == "SgOmpWhenClause" ||
               record.kind == "SgOmpMatchClause") {
      SgOmpContextSelectorSetPtrList sets;
      for (const auto &edge : edgesFor(record, "context_selector_sets")) {
        require_unowned_delayed_child(edge.target, record, "context set");
        sets.push_back(nodeByIdAs<SgOmpContextSelectorSet>(nodes, edge.target));
      }
      validateOmpContextSelectorSetsPayload(sets);
      SgStatement *directive = nullptr;
      if (record.kind == "SgOmpWhenClause")
        if (const uint64_t id = singleEdgeTarget(record, "variant_directive")) {
          require_unowned_delayed_child(id, record, "when variant");
          directive = nodeByIdAs<SgStatement>(nodes, id);
        }
      const auto publish_clause = [&](auto *clause) {
        with_delayed_node_ownership(record, clause, [&] {
          clause->get_context_selector_sets() = std::move(sets);
          for (auto *set : clause->get_context_selector_sets())
            set->set_parent(clause);
          if (directive != nullptr)
            directive->set_parent(clause);
          publishConstructorOnlyNode(record, clause, nodes,
                                     std::move(positions),
                                     construction_fields.at(record.id));
        });
      };
      if (record.kind == "SgOmpWhenClause")
        publish_clause(new SgOmpWhenClause(directive));
      else
        publish_clause(new SgOmpMatchClause);
    } else if (kindInVariants(record.kind, omp_body_variants) ||
               kindInVariants(record.kind, omp_clause_statements) ||
               record.kind == "SgOmpGroupprivateStatement" ||
               isJsonScalarLiteralKind(record.kind) ||
               record.kind == "SgFunctionParameterScope" ||
               record.kind == "SgFormatItem" ||
               record.kind == "SgFormatItemList" ||
               record.kind == "SgFormatStatement") {
      if (kindInVariants(record.kind, omp_body_variants))
        if (uint64_t body = singleEdgeTarget(record, "body"))
          require_unowned_delayed_child(body, record, "OpenMP body");
      auto *node =
          createNodeFromRecord(record, nullptr, nodes, std::move(positions));
      with_delayed_node_ownership(record, node, [&] {
        publishRecordedNode(record, node, nodes,
                            construction_fields.at(record.id));
      });
    } else if (record.kind == "SgStatementFunctionStatement") {
      auto *function = isSgFunctionDeclaration(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "function"), record,
          "statement function declaration"));
      auto *expression = isSgExpression(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "expression"), record,
          "statement function expression"));
      if (function == nullptr || expression == nullptr)
        throw std::runtime_error(
            "AST JSON statement function requires its exact typed children");
      auto *statement = new SgStatementFunctionStatement(function, expression);
      function->set_parent(statement);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgLambdaCapture") {
      auto operand = [&](const char *field, bool required) -> SgExpression * {
        const uint64_t id = required ? requiredSingleEdgeTarget(record, field)
                                     : singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        auto *expression = isSgExpression(require_unowned_delayed_child(
            id, record, "lambda capture operand"));
        if (expression == nullptr)
          throw std::runtime_error(
              "AST JSON lambda capture operand is not an expression");
        return expression;
      };
      auto *variable = operand("capture_variable", true);
      auto *initializer = operand("capture_initializer", true);
      auto *closure = operand("closure_variable", false);
      if (variable == nullptr || initializer == nullptr)
        throw std::runtime_error(
            "AST JSON lambda capture lacks typed operands");
      const JsonValue &p = record.properties;
      publish_delayed_node(
          record, std::move(positions),
          new SgLambdaCapture(variable, initializer, closure,
                              p.requiredBool("capture_by_reference"),
                              p.requiredBool("implicit"),
                              p.requiredBool("pack_expansion"),
                              p.requiredBool("is_init_capture")));
    } else if (record.kind == "SgLambdaExp") {
      const uint64_t captures_id =
          requiredSingleEdgeTarget(record, "lambda_capture_list");
      auto *captures = isSgLambdaCaptureList(require_unowned_delayed_child(
          captures_id, record, "lambda capture list"));
      if (captures == nullptr || !captures->get_capture_list().empty())
        throw std::runtime_error(
            "AST JSON lambda requires a fresh capture list");
      for (const EdgeRecord &edge :
           edgesFor(ast.node(captures_id), "capture_list")) {
        auto *capture = isSgLambdaCapture(require_unowned_delayed_child(
            edge.target, ast.node(captures_id), "lambda capture"));
        if (capture == nullptr)
          throw std::runtime_error(
              "AST JSON lambda capture has the wrong kind");
        captures->get_capture_list().push_back(capture);
        capture->set_parent(captures);
      }
      auto *lambda = new SgLambdaExp(
          captures,
          nodeByIdAs<SgClassDeclaration>(
              nodes, requiredSingleEdgeTarget(record, "lambda_closure_class")),
          nodeByIdAs<SgFunctionDeclaration>(
              nodes, requiredSingleEdgeTarget(record, "lambda_function")),
          singleEdgeTarget(record, "closure_scope") != 0
              ? nodeByIdAs<SgDeclarationScope>(
                    nodes, singleEdgeTarget(record, "closure_scope"))
              : nullptr);
      const JsonValue &p = record.properties;
      lambda->set_is_mutable(p.requiredBool("is_mutable"));
      lambda->set_capture_default(p.requiredBool("capture_default"));
      lambda->set_default_is_by_reference(
          p.requiredBool("default_is_by_reference"));
      lambda->set_explicit_return_type(p.requiredBool("explicit_return_type"));
      lambda->set_has_parameter_decl(p.requiredBool("has_parameter_decl"));
      lambda->set_is_device(p.requiredBool("is_device"));
      publish_delayed_node(record, std::move(positions), lambda);
    } else if (record.kind == "SgFortranDo") {
      const bool has_end = record.properties.requiredBool("has_end_statement");
      const std::string name = record.properties.requiredString("string_label");
      const bool insensitive =
          record.properties.requiredBool("case_insensitive");
      auto control = [&](const char *field) -> SgExpression * {
        const uint64_t id = singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        auto *child = isSgExpression(require_unowned_delayed_child(
            id, record, std::string("SgFortranDo ") + field));
        if (child == nullptr)
          throw std::runtime_error(
              "AST JSON SgFortranDo has a non-expression " +
              std::string(field));
        return child;
      };
      auto *body = isSgBasicBlock(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "body"), record,
          "SgFortranDo body"));
      if (body == nullptr)
        throw std::runtime_error("AST JSON SgFortranDo body is not a block");
      auto *statement =
          new SgFortranDo(control("initialization"), control("bound"),
                          control("increment"), body);
      statement->set_has_end_statement(has_end);
      statement->set_string_label(name);
      statement->setCaseInsensitive(insensitive);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgCaseOptionStmt" ||
               record.kind == "SgDefaultOptionStmt") {
      const bool is_case = record.kind == "SgCaseOptionStmt";
      const auto name = record.properties.requiredString(
          is_case ? "case_construct_name" : "default_construct_name");
      auto *body = isSgStatement(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "body"), record,
          "CASE/DEFAULT body"));
      if (body == nullptr)
        throw std::runtime_error("AST JSON CASE/DEFAULT requires a typed body");
      if (is_case) {
        const uint64_t key_id = requiredSingleEdgeTarget(record, "key");
        auto *key = isSgExpression(
            require_unowned_delayed_child(key_id, record, "CASE key"));
        const uint64_t end_id = singleEdgeTarget(record, "key_range_end");
        auto *end = end_id != 0 ? isSgExpression(require_unowned_delayed_child(
                                      end_id, record, "CASE range end"))
                                : nullptr;
        if (key == nullptr || (end_id != 0 && end == nullptr))
          throw std::runtime_error("AST JSON CASE requires exact typed keys");
        auto *statement = new SgCaseOptionStmt(key, body);
        statement->set_key_range_end(end);
        if (end != nullptr)
          end->set_parent(statement);
        statement->set_case_construct_name(name);
        publish_delayed_node(record, std::move(positions), statement);
      } else {
        auto *statement = new SgDefaultOptionStmt(body);
        statement->set_default_construct_name(name);
        publish_delayed_node(record, std::move(positions), statement);
      }
    } else if (record.kind == "SgWhereStatement" ||
               record.kind == "SgElseWhereStatement") {
      const bool where = record.kind == "SgWhereStatement";
      auto *condition = isSgExpression(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "condition"), record,
          "WHERE condition"));
      auto *body = isSgBasicBlock(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "body"), record, "WHERE body"));
      const uint64_t next_id = singleEdgeTarget(record, "elsewhere");
      auto *next = next_id != 0
                       ? isSgElseWhereStatement(require_unowned_delayed_child(
                             next_id, record, "ELSEWHERE clause"))
                       : nullptr;
      if (condition == nullptr || body == nullptr ||
          (next_id != 0 && next == nullptr))
        throw std::runtime_error(
            "AST JSON WHERE requires exact typed children");
      if (where) {
        const bool has_end =
            record.properties.requiredBool("has_end_statement");
        const auto name = record.properties.requiredString("string_label");
        SgNode *parent =
            nodeById(nodes, requiredSingleEdgeTarget(record, "parent"));
        auto *statement = new SgWhereStatement(condition, body, next);
        statement->set_has_end_statement(has_end);
        statement->set_string_label(name);
        statement->set_parent(parent);
        publish_delayed_node(record, std::move(positions), statement);
        for (auto *clause = next; clause != nullptr;
             clause = clause->get_elsewhere())
          restoreConstructedBasisSymbols(bindings, node_ids.at(clause), nodes,
                                         nullptr);
      } else {
        // The owning WHERE attaches the complete chain before binding its
        // program-unit label symbols; requiring that owner here creates a
        // cycle.
        RecordNodeConstruction ownership(node_ids);
        auto *statement = new SgElseWhereStatement(condition, body, next);
        ownership.own(statement);
        publishConstructorOnlyNode(record, statement, nodes,
                                   std::move(positions),
                                   construction_fields.at(record.id));
        ownership.publish();
        node_ids.emplace(statement, record.id);
      }
    } else if (record.kind == "SgForAllStatement") {
      const auto kind =
          requiredEnum<SgForAllStatement::forall_statement_kind_enum>(
              record.properties, "forall_statement_kind", "SgForAllStatement",
              {SgForAllStatement::e_forall_statement,
               SgForAllStatement::e_do_concurrent_statement});
      const bool has_end = record.properties.requiredBool("has_end_statement");
      const std::string name = record.properties.requiredString("string_label");
      const bool insensitive =
          record.properties.requiredBool("case_insensitive");
      const uint64_t header_id =
          requiredSingleEdgeTarget(record, "forall_header");
      auto *header = isSgExprListExp(
          require_unowned_delayed_child(header_id, record, "FORALL header"));
      auto *body = isSgBasicBlock(require_unowned_delayed_child(
          requiredSingleEdgeTarget(record, "body"), record, "FORALL body"));
      if (header == nullptr || body == nullptr)
        throw std::runtime_error(
            "AST JSON FORALL requires fresh typed children");
      SgNode *parent =
          nodeById(nodes, requiredSingleEdgeTarget(record, "parent"));
      auto *statement = new SgForAllStatement(header, body);
      statement->set_forall_statement_kind(kind);
      statement->set_has_end_statement(has_end);
      statement->set_string_label(name);
      statement->setCaseInsensitive(insensitive);
      statement->set_parent(parent);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgTemplateParameterVal") {
      publish_delayed_node(
          record, std::move(positions),
          templateParameterValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgEnumVal") {
      publish_delayed_node(record, std::move(positions),
                           enumValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgNullptrValExp") {
      publish_delayed_node(
          record, std::move(positions),
          nullptrValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgVarRefExp") {
      publish_delayed_node(
          record, std::move(positions),
          variableReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgCompoundLiteralExp") {
      publish_delayed_node(
          record, std::move(positions),
          compoundLiteralFromProperties(record.properties, nodes));
    } else if (record.kind == "SgFunctionParameterRefExp") {
      publish_delayed_node(
          record, std::move(positions),
          functionParameterReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgLabelRefExp") {
      SgStatement *owner = nullptr;
      auto role = SgLabelSymbol::e_unknown_label_type;
      const uint64_t parent = singleEdgeTarget(record, "parent");
      if (parent != 0) {
        for (const auto &[field, field_role] :
             {std::pair{"numeric_label", SgLabelSymbol::e_start_label_type},
              std::pair{"else_numeric_label", SgLabelSymbol::e_else_label_type},
              std::pair{"end_numeric_label",
                        SgLabelSymbol::e_end_label_type}}) {
          if (singleEdgeTarget(ast.node(parent), field) != record.id)
            continue;
          if (owner != nullptr)
            throw std::runtime_error(
                "AST JSON label has multiple owning roles");
          owner = nodeByIdAs<SgStatement>(nodes, parent);
          role = field_role;
          SgLabelRefExp *existing = nullptr;
          if (role == SgLabelSymbol::e_start_label_type) {
            existing = owner->get_numeric_label();
          } else if (role == SgLabelSymbol::e_else_label_type) {
            auto *statement = isSgIfStmt(owner);
            if (statement == nullptr || !statement->get_use_then_keyword() ||
                singleEdgeTarget(ast.node(parent), "false_body") == 0)
              throw std::runtime_error(
                  "AST JSON ELSE label has no exact IF owner");
            existing = statement->get_else_numeric_label();
          } else {
            if (!owner->has_end_numeric_label())
              throw std::runtime_error(
                  "AST JSON end label has no construct owner");
            existing = owner->get_end_numeric_label();
          }
          if (existing != nullptr)
            throw std::runtime_error(
                "AST JSON label owner already has a reference");
        }
      }
      std::unique_ptr<SgLabelRefExp> pending_label(
          labelReferenceFromProperties(record.properties, nodes));
      if (owner != nullptr) {
        SgLabelSymbol *symbol = pending_label->get_symbol();
        if (symbol->get_fortran_statement() == nullptr ||
            (role != SgLabelSymbol::e_end_label_type &&
             symbol->get_fortran_statement() != owner))
          throw std::runtime_error(
              "AST JSON label has a contradictory statement basis");
        if (role == SgLabelSymbol::e_start_label_type) {
          if (symbol->get_label_type() != SgLabelSymbol::e_start_label_type &&
              symbol->get_label_type() != SgLabelSymbol::e_end_label_type)
            throw std::runtime_error(
                "AST JSON numeric label has an invalid typed role");
        } else if (symbol->get_label_type() != role) {
          throw std::runtime_error(
              "AST JSON boundary label has an invalid typed role");
        }
      }
      auto *label = pending_label.release();
      publish_delayed_node(record, std::move(positions), label);
      if (owner != nullptr) {
        if (role == SgLabelSymbol::e_start_label_type)
          owner->set_numeric_label(label);
        else if (role == SgLabelSymbol::e_else_label_type)
          isSgIfStmt(owner)->set_else_numeric_label(label);
        else
          owner->set_end_numeric_label(label);
        label->set_parent(owner);
      }
    } else if (record.kind == "SgNonrealRefExp") {
      if (record.properties.requiredInt("name_kind") ==
              SgNonrealRefExp::e_destructor_id &&
          (!edgesFor(record, "templateArguments").empty() ||
           singleEdgeTarget(record, "conversion_type_id") != 0))
        throw std::runtime_error(
            "AST JSON dependent destructor has invalid name edges");
      const uint64_t target = singleEdgeTarget(record, "conversion_type_id");
      auto *type_id =
          target == 0 ? nullptr : nodeByIdAs<SgTypeExpression>(nodes, target);
      publish_delayed_node(
          record, std::move(positions),
          nonrealReferenceFromProperties(record.properties, nodes, type_id));
    } else if (record.kind == "SgFunctionRefExp") {
      publish_delayed_node(
          record, std::move(positions),
          functionReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgMemberFunctionRefExp") {
      const uint64_t target = singleEdgeTarget(record, "conversion_type_id");
      auto *type_id =
          target == 0 ? nullptr : nodeByIdAs<SgTypeExpression>(nodes, target);
      publish_delayed_node(record, std::move(positions),
                           memberFunctionReferenceFromProperties(
                               record.properties, nodes, type_id));
    } else if (record.kind == "SgTemplateFunctionRefExp") {
      publish_delayed_node(
          record, std::move(positions),
          templateFunctionReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTemplateMemberFunctionRefExp") {
      const uint64_t target = singleEdgeTarget(record, "conversion_type_id");
      auto *type_id =
          target == 0 ? nullptr : nodeByIdAs<SgTypeExpression>(nodes, target);
      publish_delayed_node(record, std::move(positions),
                           templateMemberFunctionReferenceFromProperties(
                               record.properties, nodes, type_id));
    } else if (record.kind == "SgThisExp") {
      publish_delayed_node(
          record, std::move(positions),
          thisExpressionFromProperties(record.properties, nodes));
    } else if (record.kind == "SgSourceLocationBuiltinExp") {
      publish_delayed_node(
          record, std::move(positions),
          sourceLocationBuiltinFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTypeRequirement") {
      publish_delayed_node(
          record, std::move(positions),
          typeRequirementFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTypeExpression") {
      SgType *represented_type =
          typeFromJson(record.properties.at("represented_type"), nodes);
      if (represented_type->has_source_prototype() &&
          represented_type->get_parent() != nullptr)
        throw std::runtime_error("AST JSON SgTypeExpression has no exact "
                                 "construction-time represented type");
      publish_delayed_node(record, std::move(positions),
                           new SgTypeExpression(represented_type));
    } else if (record.kind == "SgStatementExpression") {
      const uint64_t body_id = requiredSingleEdgeTarget(record, "statement");
      auto *body = isSgBasicBlock(require_unowned_delayed_child(
          body_id, record, "SgStatementExpression body"));
      const uint64_t scope_id = requiredSingleEdgeTarget(
          ast.node(body_id), "statement_expression_semantic_scope");
      auto *scope = nodeByIdAs<SgScopeStatement>(nodes, scope_id);
      if (body == nullptr || scope == nullptr || scope == body ||
          body->get_statement_expression_construction_scope() != nullptr ||
          body->get_statement_expression_semantic_scope() != scope)
        throw std::runtime_error(
            "AST JSON SgStatementExpression has invalid construction-time body "
            "and semantic scope");
      SgType *result_type =
          typeFromJson(record.properties.at("type"), nodes,
                       "AST JSON " + record.kind +
                           " requires an exact stored destination type");
      auto *expression = new SgStatementExpression(body, result_type);
      ROSE_ASSERT(body->get_scope() == scope);
      ROSE_ASSERT(expression->get_type() == result_type);
      publish_delayed_node(record, std::move(positions), expression);
    } else if (record.kind == "SgOmpDeclareMapperStatement") {
      const int64_t identifier = record.properties.requiredInt("identifier");
      if (identifier <
              SgOmpClause::e_omp_declare_mapper_identifier_unspecified ||
          identifier > SgOmpClause::e_omp_declare_mapper_identifier_user)
        throw std::runtime_error(
            "AST JSON declare mapper identifier kind is invalid");
      const bool explicit_identifier =
          record.properties.requiredBool("identifier_is_explicit");
      auto operand = [&](const char *field) -> SgExpression * {
        const uint64_t id = singleEdgeTarget(record, field);
        if (id == 0)
          return nullptr;
        auto *expression = isSgExpression(require_unowned_delayed_child(
            id, record, "declare mapper operand"));
        if (expression == nullptr)
          throw std::runtime_error(
              "AST JSON declare mapper operand is not an expression");
        return expression;
      };
      auto *user_identifier = operand("user_defined_identifier");
      auto *type = operand("mapper_type");
      auto *variable = operand("mapper_variable");
      auto *statement = new SgOmpDeclareMapperStatement();
      statement->set_identifier(
          static_cast<SgOmpClause::omp_declare_mapper_identifier_enum>(
              identifier));
      statement->set_identifier_is_explicit(explicit_identifier);
      statement->set_user_defined_identifier(user_identifier);
      statement->set_mapper_type(type);
      statement->set_mapper_variable(variable);
      for (SgExpression *child : {user_identifier, type, variable})
        if (child != nullptr)
          child->set_parent(statement);
      publish_delayed_node(record, std::move(positions), statement);
    } else if (record.kind == "SgOmpDeclareSimdStatement") {
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
      publish_delayed_node(
          record, std::move(positions),
          new SgOmpDeclareSimdStatement(
              reference,
              record.properties.requiredBool("function_ref_is_explicit"),
              static_cast<std::size_t>(ordinal)));
    } else if (record.kind == "SgOmpDeclareVariantStatement") {
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
      publish_delayed_node(
          record, std::move(positions),
          new SgOmpDeclareVariantStatement(
              variant_reference, base_reference,
              record.properties.requiredBool("base_function_ref_is_explicit"),
              static_cast<std::size_t>(ordinal)));
    } else if (record.kind == "SgPseudoDestructorRefExp") {
      SgType *object_type =
          typeFromJson(record.properties.at("object_type"), nodes);
      SgType *expression_type =
          typeFromJson(record.properties.at("type"), nodes);
      SgMemberFunctionType *callable_type =
          isSgMemberFunctionType(expression_type);
      if (SageInterface::containsUnknownType(object_type) ||
          callable_type == nullptr ||
          SageInterface::containsUnknownType(callable_type) ||
          callable_type->get_class_type() != object_type ||
          callable_type->get_has_ellipses() ||
          !callable_type->get_arguments().empty() ||
          isSgTypeVoid(callable_type->get_return_type()) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgPseudoDestructorRefExp has incomplete exact type "
            "identity");
      }
      publish_delayed_node(
          record, std::move(positions),
          new SgPseudoDestructorRefExp(object_type, expression_type));
    } else if (record.kind == "SgBracedInitializer") {
      const bool braced =
          record.properties.requiredBool("is_braced_initialized");
      const uint64_t initializers_id =
          requiredSingleEdgeTarget(record, "initializers");
      SgExprListExp *initializers =
          isSgExprListExp(require_unowned_delayed_child(
              initializers_id, record, "SgBracedInitializer initializers"));
      if (initializers == nullptr) {
        throw std::runtime_error(
            "AST JSON SgBracedInitializer child is not an expression list");
      }
      SgType *destination_type =
          typeFromJson(record.properties.at("type"), nodes,
                       "AST JSON " + record.kind +
                           " requires an exact stored destination type");
      SgBracedInitializer *initializer =
          new SgBracedInitializer(initializers, destination_type);
      initializer->set_is_braced_initialized(braced);
      if (initializers->get_parent() != initializer) {
        throw std::runtime_error(
            "AST JSON SgBracedInitializer did not claim its exact list");
      }
      publish_delayed_node(record, std::move(positions), initializer);
    } else {
      throw std::runtime_error("AST JSON unsupported direct constructor kind " +
                               record.kind);
    }
  };

  struct DirectInput {};
  struct AssignmentInput {
    uint64_t operand;
    AssignmentInitializerSourceFields source;
  };
  struct ActualArgumentInput {
    uint64_t expression;
    SgName name;
  };
  struct MacroInput {
    uint64_t expanded;
  };
  struct DefaultInitializerInput {
    uint64_t expression, parameter;
    SgCxxDefaultInitExp::source_kind_enum kind;
  };
  struct ArrayBindingInput {
    uint64_t source;
    SgCastExp::value_category_enum category;
  };
  struct ArrayLoopInput {
    uint64_t binding, element, count;
  };
  struct ArrayReferenceInput {
    uint64_t binding;
    ArrayInitReferenceFields fields;
  };
  struct OpaqueBindingInput {
    uint64_t source;
    OpaqueValueFields fields;
  };
  struct OpaqueReferenceInput {
    uint64_t binding;
  };
  struct PseudoObjectInput {
    uint64_t written, evaluations, result;
    std::vector<uint64_t> captures;
    OpaqueValueFields fields;
  };
  struct AsmOperandInput {
    uint64_t expression;
    AsmOperandConstructionFields fields;
  };
  struct AsmStatementInput {
    std::vector<uint64_t> operands;
    AsmStatementConstructionFields fields;
  };
  struct DesignatedInput {
    uint64_t designators;
    uint64_t member;
    bool braced;
  };
  struct CastInput {
    uint64_t operand;
    uint64_t type_defining_declaration;
    CastConstructionFields fields;
  };
  struct FoldInput {
    uint64_t operands;
    FoldConstructionFields fields;
  };
  struct AwaitInput {
    uint64_t operand;
    SgAwaitExpression::coroutine_keyword_kind_enum keyword;
  };
  struct PackInput {
    uint64_t pattern;
  };
  struct OperandInput {
    uint64_t operand;
  };
  struct OmpItemInput {
    uint64_t operand;
    std::vector<uint64_t> policies;
  };
  struct OmpMapPolicyInput {
    uint64_t expression;
  };
  struct OmpListClauseInput {
    std::vector<uint64_t> children;
    std::optional<std::string> apply_label;
  };
  struct OmpApplyInput {
    uint64_t argument, nested;
  };
  struct NewInput {
    uint64_t bound, placement, constructor, builtin, declaration;
  };
  struct DeleteInput {
    uint64_t operand, declaration;
    short is_array, global;
  };
  struct BinaryInput {
    uint64_t lhs, rhs;
    std::optional<SgArrowExp::emission_role_enum> arrow_role;
    std::optional<long long> result_storage_size_bytes;
    std::vector<std::optional<long long>> subscript_strides;
  };
  struct UnaryInput {
    uint64_t operand;
  };
  struct BoundsInput {
    uint64_t lower, upper, stride;
    VariantT kind = V_SgSubscriptExpression;
  };
  struct AggregateInput {
    uint64_t list;
    SgUnsignedCharList source_roles;
  };
  struct ConstructorInput {
    uint64_t list, declaration;
  };
  struct CallInput {
    uint64_t function;
    uint64_t arguments;
    uint64_t lexical;
    std::vector<uint64_t> tokens;
    FunctionCallSourceFields fields;
  };
  struct ConditionalInput {
    SgConditionalExp::conditional_operator_kind_enum kind;
    uint64_t condition;
    uint64_t true_expression;
    uint64_t false_expression;
  };
  struct ChooseInput {
    SgChooseExpression::selection_kind_enum selection;
    uint64_t condition, true_expression, false_expression;
  };
  struct BuiltinInput {
    std::vector<uint64_t> arguments;
  };
  struct ComplexInput {
    std::vector<uint64_t> components;
  };
  struct UnaryTraitInput {
    SgExpression::unary_type_trait_kind_enum kind;
    uint64_t operand;
    uint64_t type_defining_declaration;
    bool objectless_member;
    bool operand_is_evaluated;
    std::optional<UnaryTraitSourceFields> source;
  };
  struct RequirementInput {
    OperandRequirementKind kind;
    uint64_t operand, type_constraint;
    bool noexcept_required;
  };
  struct RequiresInput {
    uint64_t parameters, scope, requirements, syntax;
    std::vector<uint64_t> arguments;
    std::vector<uint64_t> written_arguments;
  };
  struct AttributedStatementInput {
    uint64_t statement, list;
  };
  struct StatementAttributeInput {
    uint64_t operand;
  };
  struct StatementAttributeListInput {
    std::vector<uint64_t> attributes;
  };
  struct OperandStatementInput {
    uint64_t expression;
    std::optional<ReturnStatementFields> return_statement;
  };
  struct AlignmentInput {
    uint64_t operand;
  };
  struct DeclarationAttributesInput {
    std::vector<uint64_t> alignments;
    uint64_t launch_bounds;
    uint64_t explicit_condition;
  };
  struct FunctionSyntaxInput {
    bool available;
  };
  struct ParameterListInput {
    uint64_t owner;
    std::vector<uint64_t> parameters;
  };
  struct PrototypeInput {
    uint64_t scope, parameters, enclosing;
  };
  struct InitializedNameInput {
    uint64_t initializer;
  };
  struct VariableListInput {
    std::vector<uint64_t> variables;
  };
  struct OwnedListInput {
    std::vector<uint64_t> children;
  };
  struct ImplicitStatementInput {
    SgImplicitStatement::implicit_spec_enum spec;
    std::vector<uint64_t> ranges;
  };
  struct DataValueInput {
    uint64_t constant, repeat;
  };
  struct DataObjectInput {
    uint64_t list;
  };
  struct NamelistGroupInput {
    std::string name;
    std::vector<uint64_t> objects;
  };
  struct NamelistInput {
    std::vector<uint64_t> groups;
  };
  struct NamelistIoInput {
    const JsonValue *binding;
  };
  struct DataGroupInput {
    std::vector<uint64_t> objects, values;
  };
  struct DataOwnerInput {
    std::vector<uint64_t> children;
    std::optional<bool> braced;
  };
  struct ExpressionInput {
    const NodeRecord *record;
    std::variant<
        DirectInput, MacroInput, DefaultInitializerInput, ArrayBindingInput,
        ArrayLoopInput, ArrayReferenceInput, OpaqueBindingInput,
        OpaqueReferenceInput, PseudoObjectInput, AsmOperandInput,
        AsmStatementInput, ActualArgumentInput, AssignmentInput,
        DesignatorInput, DesignatedInput, CastInput, FoldInput, PackInput,
        AwaitInput, ComplexInput, UnaryTraitInput, BuiltinInput,
        ConditionalInput, ChooseInput, CallInput, AggregateInput,
        ConstructorInput, NewInput, DeleteInput, BinaryInput, UnaryInput,
        BoundsInput, RequirementInput, RequiresInput, OperandInput,
        OmpItemInput, OmpMapPolicyInput, OmpListClauseInput, OmpApplyInput,
        AttributedStatementInput, StatementAttributeInput,
        StatementAttributeListInput, OperandStatementInput, AlignmentInput,
        DeclarationAttributesInput, FunctionSyntaxInput, ParameterListInput,
        PrototypeInput, InitializedNameInput, VariableListInput, OwnedListInput,
        FortranImplicitRangeInput, ImplicitStatementInput, DataValueInput,
        DataObjectInput, DataGroupInput, DataOwnerInput, NamelistGroupInput,
        NamelistInput, NamelistIoInput>
        input;
    SourcePositions positions;
    bool completed = false;

    bool completesExistingNode() const {
      return std::holds_alternative<DeclarationAttributesInput>(input) ||
             std::holds_alternative<FunctionSyntaxInput>(input) ||
             std::holds_alternative<ParameterListInput>(input) ||
             std::holds_alternative<VariableListInput>(input) ||
             std::holds_alternative<OwnedListInput>(input) ||
             std::holds_alternative<InitializedNameInput>(input);
    }
  };
  std::vector<ExpressionInput> expressions;
  for (const NodeRecord *record_ptr : records) {
    const NodeRecord &record = *record_ptr;
    if (auto node = nodes.find(record.id);
        node != nodes.end() && isSgVariableDeclaration(node->second)) {
      VariableListInput input;
      for (const auto &edge : edgesFor(record, "variables"))
        input.variables.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    }
    if (isFunctionDeclarationRecordKind(record.kind)) {
      if (const auto *syntax = record.properties.find("function_type_syntax")) {
        const bool available =
            record.properties.requiredBool("type_syntax_is_available");
        if (available != syntax->requiredBool("present"))
          throw std::runtime_error(
              "AST JSON function has inconsistent function type syntax state");
        expressions.push_back({&record, FunctionSyntaxInput{available}});
      }
    }
    if (record.kind == "SgExprListExp" || record.kind == "SgOmpClauseList") {
      OwnedListInput input;
      const char *role =
          record.kind == "SgExprListExp" ? "expressions" : "clauses";
      for (const auto &edge : edgesFor(record, role))
        input.children.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgInitializedName") {
      const uint64_t initializer = singleEdgeTarget(record, "initptr");
      if (initializer != 0)
        expressions.push_back({&record, InitializedNameInput{initializer}});
    } else if (record.kind == "SgFunctionParameterTypeList") {
      const uint64_t scope =
          requiredSingleEdgeTarget(record, "source_parameter_scope");
      expressions.push_back(
          {&record,
           PrototypeInput{scope,
                          requiredSingleEdgeTarget(record, "source_parameters"),
                          requiredSingleEdgeTarget(
                              ast.node(scope), "completed_semantic_scope")}});
    } else if (record.kind == "SgFunctionParameterList") {
      const uint64_t owner = requiredSingleEdgeTarget(record, "parent");
      if (isFunctionDeclarationRecordKind(ast.node(owner).kind) ||
          ast.node(owner).kind == "SgFunctionParameterTypeList") {
        ParameterListInput input{owner, {}};
        for (const EdgeRecord &edge : edgesFor(record, "args"))
          input.parameters.push_back(edge.target);
        expressions.push_back({&record, std::move(input)});
      }
    }
    if (record.kind == "SgTemplateInstantiationDefn") {
      auto *definition =
          nodeByIdAs<SgTemplateInstantiationDefn>(nodes, record.id);
      auto *declaration = nodeByIdAs<SgTemplateInstantiationDecl>(
          nodes, requiredSingleEdgeTarget(record, "parent"));
      if (definition->get_parent() != declaration ||
          definition->get_declaration() != declaration ||
          declaration->get_definition() != definition)
        throw std::runtime_error(
            "AST JSON template-instantiation definition lost its exact "
            "declaration owner");
    }
    if (record.kind == "SgAllocateStatement" ||
        record.kind == "SgDeallocateStatement" ||
        record.kind == "SgNullifyStatement" ||
        record.kind == "SgAssignStatement" ||
        record.kind == "SgAssignedGotoStatement" ||
        record.kind == "SgComputedGotoStatement" ||
        record.kind == "SgStaticAssertionDeclaration" ||
        record.kind == "SgAccCollapseClause" ||
        record.kind == "SgAccNumGangsClause" ||
        record.kind == "SgAccNumWorkersClause" ||
        record.kind == "SgAccVectorLengthClause" ||
        record.kind == "SgAccAsyncClause" || record.kind == "SgAccIfClause" ||
        record.kind == "SgAccVectorClause" ||
        record.kind == "SgAccCopyClause" ||
        record.kind == "SgAccCopyinClause" ||
        record.kind == "SgAccCopyoutClause" ||
        record.kind == "SgAccCreateClause" ||
        record.kind == "SgAccPresentClause" ||
        record.kind == "SgAccPrivateClause" ||
        record.kind == "SgAccDeviceptrClause" ||
        record.kind == "SgAccDeleteClause" ||
        record.kind == "SgAccReductionClause" ||
        record.kind == "SgAccWaitStatement" ||
        record.kind == "SgAccCacheStatement" ||
        record.kind == "SgAccParallelStatement" ||
        record.kind == "SgAccParallelLoopStatement" ||
        record.kind == "SgAccDataStatement" ||
        record.kind == "SgAccKernelsStatement" ||
        record.kind == "SgAccAtomicStatement" ||
        kindInVariants(record.kind, image_control_variants) ||
        record.kind == "SgProcessControlStatement" ||
        (kindInVariants(record.kind, omp_clauses) &&
         record.kind != "SgOmpInductionClause" &&
         record.kind != "SgOmpApplyClause") ||
        record.kind == "SgOmpInitModifier" ||
        record.kind == "SgOmpInitModifierList" ||
        record.kind == "SgOmpAppendArgsOperation" ||
        record.kind == "SgOmpIteratorDefinition" ||
        kindInVariants(record.kind, omp_body_variants) ||
        kindInVariants(record.kind, omp_clause_statements) ||
        record.kind == "SgOmpGroupprivateStatement" ||
        ompContextHierarchyVariant(record.kind) ||
        isJsonScalarLiteralKind(record.kind) || record.kind == "SgFortranDo" ||
        record.kind == "SgLambdaExp" || record.kind == "SgLambdaCapture" ||
        record.kind == "SgForAllStatement" ||
        record.kind == "SgWhereStatement" ||
        record.kind == "SgElseWhereStatement" ||
        record.kind == "SgCaseOptionStmt" ||
        record.kind == "SgDefaultOptionStmt" ||
        record.kind == "SgTemplateParameterVal" || record.kind == "SgEnumVal" ||
        record.kind == "SgNullptrValExp" || record.kind == "SgVarRefExp" ||
        record.kind == "SgFunctionParameterRefExp" ||
        record.kind == "SgCompoundLiteralExp" ||
        record.kind == "SgLabelRefExp" || record.kind == "SgNonrealRefExp" ||
        record.kind == "SgFunctionRefExp" ||
        record.kind == "SgMemberFunctionRefExp" ||
        record.kind == "SgTemplateFunctionRefExp" ||
        record.kind == "SgTemplateMemberFunctionRefExp" ||
        record.kind == "SgThisExp" ||
        record.kind == "SgSourceLocationBuiltinExp" ||
        record.kind == "SgTypeRequirement" ||
        record.kind == "SgTypeExpression" ||
        record.kind == "SgStatementExpression" ||
        record.kind == "SgOmpDeclareSimdStatement" ||
        record.kind == "SgOmpDeclareMapperStatement" ||
        record.kind == "SgOmpDeclareVariantStatement" ||
        record.kind == "SgPseudoDestructorRefExp" ||
        record.kind == "SgCxxSubobjectDestruction" ||
        record.kind == "SgCxxDecompositionRefExp" ||
        record.kind == "SgBracedInitializer" ||
        record.kind == "SgStatementFunctionStatement" ||
        record.kind == "SgFormatItem" || record.kind == "SgFormatItemList" ||
        record.kind == "SgFormatStatement" ||
        (record.kind == "SgFunctionParameterScope" &&
         ast.node(requiredSingleEdgeTarget(record, "parent")).kind ==
             "SgRequiresExpr")) {
      expressions.push_back({&record, DirectInput{}});
    } else if (record.kind == "SgOmpInductionClause" ||
               record.kind == "SgOmpApplyClause") {
      OmpListClauseInput input{{},
                               construction_fields.at(record.id).apply_label};
      const char *role = input.apply_label ? "transformations" : "items";
      for (const auto &edge : edgesFor(record, role))
        input.children.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgOmpApplyTransformation") {
      expressions.push_back(
          {&record, OmpApplyInput{singleEdgeTarget(record, "argument"),
                                  singleEdgeTarget(record, "nested_apply")}});
    } else if (!edgesFor(record, "alignment_specifiers").empty() ||
               singleEdgeTarget(record, "cuda_launch_bounds_expression") != 0 ||
               singleEdgeTarget(record, "explicit_specifier_expression") != 0) {
      DeclarationAttributesInput input{
          {},
          singleEdgeTarget(record, "cuda_launch_bounds_expression"),
          singleEdgeTarget(record, "explicit_specifier_expression")};
      for (const auto &edge : edgesFor(record, "alignment_specifiers"))
        input.alignments.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgDataStatementValue") {
      expressions.push_back(
          {&record, DataValueInput{
                        requiredSingleEdgeTarget(record, "constant_expression"),
                        singleEdgeTarget(record, "repeat_expression")}});
    } else if (record.kind == "SgReadStatement" ||
               record.kind == "SgWriteStatement") {
      expressions.push_back(
          {&record, NamelistIoInput{&record.properties.at("namelist")}});
    } else if (record.kind == "SgNameGroup") {
      NamelistGroupInput input{record.properties.requiredString("group_name"),
                               {}};
      for (const auto &edge : edgesFor(record, "objects"))
        input.objects.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgNamelistStatement") {
      NamelistInput input;
      for (const auto &edge : edgesFor(record, "group_list"))
        input.groups.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgDataStatementObject") {
      DataObjectInput input{
          requiredSingleEdgeTarget(record, "variableReference_list")};
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgDataStatementGroup") {
      DataGroupInput input;
      for (const auto &edge : edgesFor(record, "object_list"))
        input.objects.push_back(edge.target);
      for (const auto &edge : edgesFor(record, "value_list"))
        input.values.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgFortranDataInitializer" ||
               (record.kind == "SgAttributeSpecificationStatement" &&
                record.properties.requiredInt("attribute_kind") ==
                    SgAttributeSpecificationStatement::e_dataStatement)) {
      DataOwnerInput input;
      if (record.kind == "SgFortranDataInitializer")
        input.braced = record.properties.requiredBool("is_braced_initialized");
      for (const auto &edge :
           edgesFor(record, record.kind == "SgFortranDataInitializer"
                                ? "values"
                                : "data_statement_group_list"))
        input.children.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgFortranImplicitRange") {
      expressions.push_back(
          {&record, fortranImplicitRangeInput(record.properties)});
    } else if (record.kind == "SgImplicitStatement") {
      ImplicitStatementInput input{implicitStatementSpec(record.properties),
                                   {}};
      for (const auto &edge : edgesFor(record, "ranges"))
        input.ranges.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgAlignmentSpecifier") {
      expressions.push_back(
          {&record, AlignmentInput{singleEdgeTarget(record, "operand")}});
    } else if (record.kind == "SgAsmOp") {
      expressions.push_back(
          {&record,
           AsmOperandInput{requiredSingleEdgeTarget(record, "expression"),
                           asmOperandFieldsFromJson(record.properties)}});
    } else if (record.kind == "SgAsmStmt") {
      AsmStatementInput input{{},
                              asmStatementFieldsFromJson(record.properties)};
      for (const auto &edge : edgesFor(record, "operands"))
        input.operands.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgGotoStatement" &&
               singleEdgeTarget(record, "selector_expression") != 0) {
      expressions.push_back(
          {&record, OperandStatementInput{
                        requiredSingleEdgeTarget(record, "selector_expression"),
                        std::nullopt}});
    } else if (record.kind == "SgExprStatement" ||
               record.kind == "SgReturnStmt") {
      expressions.push_back(
          {&record, OperandStatementInput{
                        singleEdgeTarget(record, "expression"),
                        record.kind == "SgReturnStmt"
                            ? construction_fields.at(record.id).return_statement
                            : std::nullopt}});
    } else if (record.kind == "SgAttributedStatement") {
      AttributedStatementInput input{
          requiredSingleEdgeTarget(record, "statement"),
          requiredSingleEdgeTarget(record, "attribute_list")};
      expressions.push_back({&record, input});
    } else if (record.kind == "SgStatementAttribute") {
      expressions.push_back({&record, StatementAttributeInput{singleEdgeTarget(
                                          record, "expression_argument")}});
    } else if (record.kind == "SgStatementAttributeList") {
      StatementAttributeListInput input;
      for (const auto &edge : edgesFor(record, "attributes"))
        input.attributes.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgRequiresExpr") {
      RequiresInput input{
          singleEdgeTarget(record, "local_parameter_list"),
          singleEdgeTarget(record, "local_parameter_scope"),
          requiredSingleEdgeTarget(record, "requirements"),
          singleEdgeTarget(record, "local_parameter_list_syntax"),
          {},
          {}};
      if (input.parameters != 0) {
        for (const EdgeRecord &edge :
             edgesFor(ast.node(input.parameters), "args"))
          input.arguments.push_back(edge.target);
      }
      if (input.syntax != 0)
        for (const auto &edge : edgesFor(ast.node(input.syntax), "args"))
          input.written_arguments.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgSimpleRequirement" ||
               record.kind == "SgCompoundRequirement" ||
               record.kind == "SgNestedRequirement") {
      const bool compound = record.kind == "SgCompoundRequirement";
      const bool nested = record.kind == "SgNestedRequirement";
      expressions.push_back(
          {&record,
           RequirementInput{
               compound ? OperandRequirementKind::compound
               : nested ? OperandRequirementKind::nested
                        : OperandRequirementKind::simple,
               requiredSingleEdgeTarget(record,
                                        nested ? "constraint" : "expression"),
               compound ? singleEdgeTarget(record, "type_constraint") : 0,
               compound &&
                   record.properties.requiredBool("noexcept_required")}});
    } else if (record.kind == "SgCxxDefaultInitExp") {
      expressions.push_back(
          {&record, DefaultInitializerInput{
                        requiredSingleEdgeTarget(record, "expression"),
                        defaultInitializerDeclarationNodeFromJson(
                            record.properties.at("source_declaration")),
                        defaultInitializerKindFromJson(record.properties)}});
    } else if (record.kind == "SgOpaqueValueBinding") {
      expressions.push_back(
          {&record, OpaqueBindingInput{
                        requiredSingleEdgeTarget(record, "source_expression"),
                        opaqueValueFieldsFromJson(record.properties)}});
    } else if (record.kind == "SgOpaqueValueReference") {
      expressions.push_back(
          {&record,
           OpaqueReferenceInput{requiredSingleEdgeTarget(record, "binding")}});
    } else if (record.kind == "SgPseudoObjectExp") {
      PseudoObjectInput input{
          requiredSingleEdgeTarget(record, "written_expression"),
          requiredSingleEdgeTarget(record, "semantic_evaluations"),
          singleEdgeTarget(record, "result_expression"),
          {},
          opaqueValueFieldsFromJson(record.properties)};
      for (const auto &edge : edgesFor(record, "captured_bindings"))
        input.captures.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgCxxArrayInitBinding") {
      expressions.push_back(
          {&record, ArrayBindingInput{
                        requiredSingleEdgeTarget(record, "common_source"),
                        arrayInitBindingCategoryFromJson(record.properties)}});
    } else if (record.kind == "SgCxxArrayInitLoop") {
      expressions.push_back(
          {&record, ArrayLoopInput{
                        requiredSingleEdgeTarget(record, "binding"),
                        requiredSingleEdgeTarget(record, "element_initializer"),
                        unsigned64ValueFromJson(
                            record.properties.at("element_count"))}});
    } else if (record.kind == "SgCxxArrayInitReference") {
      expressions.push_back(
          {&record, ArrayReferenceInput{
                        requiredSingleEdgeTarget(record, "binding"),
                        arrayInitReferenceFieldsFromJson(record.properties)}});
    } else if (record.kind == "SgMacroExpansionExp") {
      expressions.push_back({&record, MacroInput{requiredSingleEdgeTarget(
                                          record, "expanded_expression")}});
    } else if (record.kind == "SgActualArgumentExpression") {
      const SgName name = record.properties.requiredName("argument_name");
      if (name.is_null())
        throw std::runtime_error(
            "AST JSON named argument has an empty argument name");
      expressions.push_back(
          {&record, ActualArgumentInput{
                        requiredSingleEdgeTarget(record, "expression"), name}});
    } else if (record.kind == "SgAssignInitializer") {
      expressions.push_back(
          {&record,
           AssignmentInput{
               requiredSingleEdgeTarget(record, "operand_i"),
               assignmentInitializerSourceFieldsFromJson(record.properties)}});
    } else if (record.kind == "SgSizeOfOp" || record.kind == "SgAlignOfOp" ||
               record.kind == "SgTypeIdOp") {
      const uint64_t operand = singleEdgeTarget(record, "operand_expr");
      const bool type_operand =
          record.properties.at("operand_type").requiredBool("present");
      if ((operand != 0) == type_operand) {
        throw std::runtime_error(
            "AST JSON unary trait must have exactly one operand");
      }
      const uint64_t declaration =
          singleEdgeTarget(record, "type_defining_declaration");
      if (declaration != 0 && !type_operand)
        throw std::runtime_error("AST JSON unary trait defining declaration "
                                 "requires a type operand");
      const bool size_of = record.kind == "SgSizeOfOp";
      const bool type_id = record.kind == "SgTypeIdOp";
      if (type_id && declaration != 0)
        throw std::runtime_error(
            "AST JSON typeid cannot own a type-defining declaration");
      const bool evaluated =
          type_id && record.properties.requiredBool("operand_is_evaluated");
      if (evaluated && operand == 0)
        throw std::runtime_error(
            "AST JSON evaluated typeid requires an expression operand");
      expressions.push_back(
          {&record,
           UnaryTraitInput{
               unaryTraitKindFromProperties(record), operand, declaration,
               size_of && record.properties.requiredBool(
                              "is_objectless_nonstatic_data_member_reference"),
               evaluated,
               type_id ? std::nullopt
                       : std::make_optional(unaryTraitSourceFieldsFromJson(
                             record.properties, record.kind))}});
    } else if (record.kind == "SgNewExp") {
      NewInput input{singleEdgeTarget(record, "array_bound"),
                     singleEdgeTarget(record, "placement_args"),
                     singleEdgeTarget(record, "constructor_args"),
                     singleEdgeTarget(record, "builtin_args"),
                     singleEdgeTarget(record, "newOperatorDeclaration")};
      if (input.placement != 0) {
        const auto &list = ast.node(input.placement);
        if (list.kind != "SgExprListExp")
          throw std::runtime_error("AST JSON new-expression placement_args is "
                                   "not an expression list");
      }
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgDeleteExp") {
      const int64_t is_array = record.properties.requiredInt("is_array");
      const int64_t global =
          record.properties.requiredInt("need_global_specifier");
      if ((is_array != 0 && is_array != 1) || (global != 0 && global != 1))
        throw std::runtime_error("AST JSON delete-expression flags must be "
                                 "zero or one");
      expressions.push_back(
          {&record,
           DeleteInput{requiredSingleEdgeTarget(record, "variable"),
                       singleEdgeTarget(record, "deleteOperatorDeclaration"),
                       static_cast<short>(is_array),
                       static_cast<short>(global)}});
    } else if (isJsonBinaryOpKind(record.kind)) {
      const auto arrow_role =
          binaryEmissionRoleFromJson(record.properties, record.kind);
      expressions.push_back(
          {&record,
           BinaryInput{
               requiredSingleEdgeTarget(record, "lhs_operand_i"),
               requiredSingleEdgeTarget(record, "rhs_operand_i"), arrow_role,
               binaryStorageSizeFromJson(record.properties, record.kind),
               binarySubscriptStridesFromJson(record.properties,
                                              record.kind)}});
    } else if (isJsonUnaryOpKind(record.kind)) {
      expressions.push_back(
          {&record, UnaryInput{singleEdgeTarget(record, "operand_i")}});
    } else if (record.kind == "SgSubscriptExpression") {
      expressions.push_back(
          {&record, BoundsInput{singleEdgeTarget(record, "lowerBound"),
                                singleEdgeTarget(record, "upperBound"),
                                singleEdgeTarget(record, "stride")}});
    } else if (record.kind == "SgRangeExp") {
      expressions.push_back(
          {&record, BoundsInput{requiredSingleEdgeTarget(record, "start"),
                                requiredSingleEdgeTarget(record, "end"),
                                requiredSingleEdgeTarget(record, "stride"),
                                V_SgRangeExp}});
    } else if (record.kind == "SgAggregateInitializer") {
      AggregateInput input{
          requiredSingleEdgeTarget(record, "initializers"),
          aggregateSourceElementRolesFromJson(record.properties)};
      const auto &list = ast.node(input.list);
      if (list.kind != "SgExprListExp")
        throw std::runtime_error(
            "AST JSON aggregate initializers are not an expression list");
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgConstructorInitializer") {
      ConstructorInput input{requiredSingleEdgeTarget(record, "args"),
                             singleEdgeTarget(record, "declaration")};
      const auto &list = ast.node(input.list);
      if (list.kind != "SgExprListExp")
        throw std::runtime_error(
            "AST JSON constructor arguments are not an expression list");
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgFunctionCallExp") {
      CallInput input{
          requiredSingleEdgeTarget(record, "function"),
          requiredSingleEdgeTarget(record, "args"),
          singleEdgeTarget(record, "source_user_defined_literal_operands"),
          {},
          functionCallSourceFieldsFromJson(record.properties)};
      const auto &arguments = ast.node(input.arguments);
      if (arguments.kind != "SgExprListExp")
        throw std::runtime_error(
            "AST JSON call arguments are not an expression list");
      if (input.lexical != 0) {
        const auto &lexical = ast.node(input.lexical);
        if (lexical.kind != "SgCxxLiteralTokenList")
          throw std::runtime_error(
              "AST JSON call literal operands are not a token list");
        for (const auto &edge : edgesFor(lexical, "tokens"))
          input.tokens.push_back(edge.target);
      }
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgChooseExpression") {
      expressions.push_back(
          {&record,
           ChooseInput{chooseSelectionFromJson(record.properties),
                       requiredSingleEdgeTarget(record, "condition"),
                       requiredSingleEdgeTarget(record, "true_expression"),
                       requiredSingleEdgeTarget(record, "false_expression")}});
    } else if (record.kind == "SgConditionalExp") {
      const auto kind = conditionalOperatorKindFromJson(record.properties);
      const uint64_t true_expression = singleEdgeTarget(record, "true_exp");
      if ((kind == SgConditionalExp::e_conditional_operator_standard) !=
          (true_expression != 0))
        throw std::runtime_error(
            "AST JSON conditional has invalid operand arity");
      expressions.push_back(
          {&record,
           ConditionalInput{kind,
                            requiredSingleEdgeTarget(record, "conditional_exp"),
                            true_expression,
                            requiredSingleEdgeTarget(record, "false_exp")}});
    } else if (record.kind == "SgTypeTraitBuiltinOperator") {
      BuiltinInput input;
      for (const EdgeRecord &edge : edgesFor(record, "args"))
        input.arguments.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgOmpInductionItem" ||
               record.kind == "SgOmpMapItem") {
      OmpItemInput input{requiredSingleEdgeTarget(record, "expression"), {}};
      if (record.kind == "SgOmpMapItem")
        for (const auto &edge : edgesFor(record, "policies"))
          input.policies.push_back(edge.target);
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgOmpMapDistDataPolicy") {
      expressions.push_back(
          {&record, OmpMapPolicyInput{singleEdgeTarget(record, "expression")}});
    } else if (record.kind == "SgNoexceptOp" || record.kind == "SgVarArgOp") {
      expressions.push_back({&record, OperandInput{requiredSingleEdgeTarget(
                                          record, "operand_expr")}});
    } else if (record.kind == "SgPackExpansionExpr") {
      expressions.push_back({&record, PackInput{requiredSingleEdgeTarget(
                                          record, "pattern_expression")}});
    } else if (record.kind == "SgAwaitExpression") {
      expressions.push_back(
          {&record,
           AwaitInput{requiredSingleEdgeTarget(record, "value"),
                      coroutineKeywordKindFromJson(record.properties)}});
    } else if (record.kind == "SgFoldExpression") {
      expressions.push_back(
          {&record,
           FoldInput{requiredSingleEdgeTarget(record, "operands"),
                     foldConstructionFieldsFromJson(record.properties)}});
    } else if (record.kind == "SgCastExp") {
      const auto fields = castConstructionFieldsFromJson(record.properties);
      const uint64_t declaration =
          singleEdgeTarget(record, "type_defining_declaration");
      if (declaration != 0 && fields.cast_type == SgCastExp::e_implicit_cast)
        throw std::runtime_error(
            "AST JSON implicit cast cannot own a written type declaration");
      expressions.push_back(
          {&record, CastInput{requiredSingleEdgeTarget(record, "operand_i"),
                              declaration, fields}});
    } else if (record.kind == "SgComplexVal") {
      ComplexInput input;
      for (const char *field : {"real_value", "imaginary_value"}) {
        const int64_t id = record.properties.at(field).requiredInt("node");
        if (id < 0 ||
            static_cast<uint64_t>(id) != singleEdgeTarget(record, field)) {
          throw std::runtime_error("AST JSON SgComplexVal component property "
                                   "conflicts with its edge");
        }
        if (id != 0)
          input.components.push_back(static_cast<uint64_t>(id));
      }
      expressions.push_back({&record, std::move(input)});
    } else if (record.kind == "SgDesignator") {
      const auto &input = construction_fields.at(record.id).designator;
      ROSE_ASSERT(input.has_value());
      expressions.push_back({&record, *input});
    } else if (record.kind == "SgDesignatedInitializer") {
      expressions.push_back(
          {&record,
           DesignatedInput{
               requiredSingleEdgeTarget(record, "designatorList"),
               requiredSingleEdgeTarget(record, "memberInit"),
               record.properties.requiredBool("is_braced_initialized")}});
    }
  }

  for (ExpressionInput &pending : expressions)
    if (!pending.completesExistingNode() &&
        nodes.count(pending.record->id) == 0)
      pending.positions = constructSourcePositions(
          construction_fields.at(pending.record->id).positions.value());

  // Casts, complex components and initializer operands can depend on each
  // other. Construct by available dependencies, independent of kind and record
  // order.
  size_t remaining = expressions.size();
  std::unordered_set<uint64_t> constructed_parameter_lists;
  std::unordered_set<uint64_t> constructed_variable_lists;
  std::unordered_set<uint64_t> constructed_initializers;
  while (remaining != 0) {
    bool made_progress =
        advance_symbols &&
        advance_symbols(publishedArrayConstructionNodes(nodes, array_owner));
    if (delayed_declarations != nullptr && !delayed_declarations->empty()) {
      const auto pending = *delayed_declarations;
      drainDelayedDeclarationsWithArrays(
          ast, bindings, *delayed_declarations, nodes, false, array_owner,
          advance_symbols, prepared_construction_fields);
      made_progress =
          pending.size() != delayed_declarations->size() || made_progress;
      for (const NodeRecord *record : pending)
        if (auto found = nodes.find(record->id); found != nodes.end())
          node_ids.emplace(found->second, record->id);
    }
    for (ExpressionInput &pending : expressions) {
      const NodeRecord &record = *pending.record;
      const auto *attributes =
          std::get_if<DeclarationAttributesInput>(&pending.input);
      const auto *function_syntax =
          std::get_if<FunctionSyntaxInput>(&pending.input);
      if (pending.completed)
        continue;
      if (!pending.completesExistingNode() && nodes.count(record.id) != 0) {
        pending.completed = true;
        --remaining;
        made_progress = true;
        continue;
      }
      const auto *expression_fields =
          !pending.completesExistingNode() &&
                  kindInVariants(record.kind, expression_variants)
              ? &construction_fields.at(record.id).expression.value()
              : nullptr;
      auto &positions = pending.positions;
      auto *construction = source_construction != nullptr
                               ? source_construction
                               : array_owner(record.id);
      auto *arrays = construction == nullptr ? nullptr : &construction->arrays;
      if (!semanticArrayDependenciesReady(record.properties, arrays))
        continue;
      if (construction != nullptr && construction != source_construction &&
          array_roots.at(record.id) == record.id) {
        const bool pending_declaration =
            delayed_declarations != nullptr &&
            std::any_of(delayed_declarations->begin(),
                        delayed_declarations->end(),
                        [&](const NodeRecord *declaration) {
                          return array_owner(declaration->id) == construction;
                        });
        const bool pending_operand =
            std::any_of(expressions.begin(), expressions.end(),
                        [&](const ExpressionInput &operand) {
                          if (operand.record == &record ||
                              array_owner(operand.record->id) != construction)
                            return false;
                          return operand.completesExistingNode()
                                     ? !operand.completed
                                     : nodes.count(operand.record->id) == 0;
                        });
        if (pending_declaration || pending_operand)
          continue;
      }
      SemanticArrayConstructionScope array_scope(arrays);
      std::unordered_set<uint64_t> dependencies, symbol_dependencies,
          type_dependencies;
      std::unordered_set<uint64_t> lifetime_declarations;
      const bool direct = std::holds_alternative<DirectInput>(pending.input);
      if (expression_fields != nullptr) {
        if (expression_fields->bound_temporary_destructor != 0)
          dependencies.insert(expression_fields->bound_temporary_destructor);
        if (expression_fields->materialized_lifetime_owner != 0) {
          for (uint64_t owner = expression_fields->materialized_lifetime_owner;
               owner != 0;
               owner = singleEdgeTarget(ast.node(owner), "prev_decl_item")) {
            if (!dependencies.insert(owner).second)
              throw std::runtime_error(
                  "AST JSON materialized temporary has cyclic lifetime owners");
            const uint64_t declaration =
                requiredSingleEdgeTarget(ast.node(owner), "parent");
            lifetime_declarations.insert(declaration);
            dependencies.insert(declaration);
          }
        }
      }
      if (function_syntax != nullptr) {
        dependencies.insert(record.id);
        collectTypeDependencyIds(record.properties.at("function_type_syntax"),
                                 dependencies, symbol_dependencies,
                                 type_dependencies);
      } else if (const auto *input =
                     std::get_if<PrototypeInput>(&pending.input)) {
        dependencies.insert(input->enclosing);
      } else if (const auto *input =
                     std::get_if<ParameterListInput>(&pending.input)) {
        dependencies.insert(input->owner);
        dependencies.insert(input->parameters.begin(), input->parameters.end());
      } else if (const auto *input =
                     std::get_if<InitializedNameInput>(&pending.input)) {
        dependencies.insert(record.id);
        if (input->initializer != 0)
          dependencies.insert(input->initializer);
      } else if (const auto *input =
                     std::get_if<OwnedListInput>(&pending.input)) {
        dependencies.insert(record.id);
        dependencies.insert(input->children.begin(), input->children.end());
      } else if (const auto *input =
                     std::get_if<NamelistGroupInput>(&pending.input)) {
        dependencies.insert(input->objects.begin(), input->objects.end());
      } else if (const auto *input =
                     std::get_if<NamelistInput>(&pending.input)) {
        dependencies.insert(input->groups.begin(), input->groups.end());
        dependencies.insert(requiredSingleEdgeTarget(record, "parent"));
      } else if (const auto *input =
                     std::get_if<NamelistIoInput>(&pending.input)) {
        if (input->binding->kind != JsonValue::Kind::Null) {
          dependencies.insert(
              input->binding->requiredNonnegativeNodeId("binding_scope"));
          dependencies.insert(
              input->binding->at("symbol").requiredNonnegativeNodeId(
                  "symbol_declaration"));
        }
      } else if (const auto *input =
                     std::get_if<VariableListInput>(&pending.input)) {
        dependencies.insert(record.id);
        dependencies.insert(input->variables.begin(), input->variables.end());
        if (uint64_t scope =
                singleEdgeTarget(record, "source_declarator_scope")) {
          dependencies.insert(scope);
          for (const auto &edge : edgesFor(ast.node(scope), "declarations"))
            dependencies.insert(edge.target);
        }
      } else if (const auto *input =
                     std::get_if<OperandStatementInput>(&pending.input)) {
        if (input->return_statement &&
            input->return_statement->nrvo_candidate != 0) {
          const uint64_t candidate = input->return_statement->nrvo_candidate;
          dependencies.insert(candidate);
          dependencies.insert(
              requiredSingleEdgeTarget(ast.node(candidate), "scope"));
          const uint64_t declaration =
              requiredSingleEdgeTarget(ast.node(candidate), "parent");
          lifetime_declarations.insert(declaration);
          dependencies.insert(declaration);
        }
      }
      if (direct) {
        if (record.kind == "SgCxxDecompositionRefExp")
          dependencies.insert(
              requiredSingleEdgeTarget(record, "decomposition"));
        if (record.kind == "SgCxxSubobjectDestruction") {
          for (const char *field :
               {"destructor_declaration", "member_target", "base_target"})
            if (uint64_t id = singleEdgeTarget(record, field))
              dependencies.insert(id);
          if (uint64_t member = singleEdgeTarget(record, "member_target"))
            dependencies.insert(
                requiredSingleEdgeTarget(ast.node(member), "parent"));
        }
        for (const auto &edge : record.edges)
          if (edge.field != "parent" && edge.field != "numeric_label" &&
              edge.field != "else_numeric_label" &&
              edge.field != "end_numeric_label" &&
              singleEdgeTarget(ast.node(edge.target), "parent") == record.id)
            dependencies.insert(edge.target);
        if (record.kind != "SgElseWhereStatement" &&
            (singleEdgeTarget(record, "numeric_label") != 0 ||
             singleEdgeTarget(record, "else_numeric_label") != 0 ||
             singleEdgeTarget(record, "end_numeric_label") != 0))
          dependencies.insert(requiredSingleEdgeTarget(record, "parent"));
        if (ompContextHierarchyVariant(record.kind)) {
          for (const char *role : {"expression", "requires_expression", "score",
                                   "construct_directive", "variant_directive"})
            if (const uint64_t id = singleEdgeTarget(record, role))
              dependencies.insert(id);
          for (const char *role :
               {"properties", "selectors", "context_selector_sets"})
            for (const auto &edge : edgesFor(record, role))
              dependencies.insert(edge.target);
        }
        if (kindInVariants(record.kind, omp_body_variants))
          if (uint64_t body = singleEdgeTarget(record, "body"))
            dependencies.insert(body);
        if (record.kind == "SgOmpTaskStatement")
          for (const char *role : {"semantic_firstprivate_variables",
                                   "semantic_private_variables"})
            for (const auto &edge : edgesFor(record, role))
              dependencies.insert(edge.target);
        if (kindInVariants(record.kind, omp_clause_statements) ||
            kindInVariants(record.kind, omp_clause_bodies) ||
            record.kind == "SgOmpGroupprivateStatement")
          dependencies.insert(requiredSingleEdgeTarget(record, "clause_list"));
        if (record.kind == "SgOmpFlushStatement" ||
            record.kind == "SgOmpAllocateStatement")
          dependencies.insert(
              requiredSingleEdgeTarget(record, "variable_list"));
        if (record.kind == "SgOmpDepobjStatement")
          dependencies.insert(requiredSingleEdgeTarget(record, "depobj"));
        if (record.kind == "SgOmpGroupprivateStatement")
          dependencies.insert(requiredSingleEdgeTarget(record, "variables"));
        if (record.kind == "SgStatementFunctionStatement") {
          dependencies.insert(requiredSingleEdgeTarget(record, "function"));
          dependencies.insert(requiredSingleEdgeTarget(record, "expression"));
        }
        if (record.kind == "SgFormatItem" ||
            record.kind == "SgFormatItemList" ||
            record.kind == "SgFormatStatement")
          for (const auto &edge : edgesFor(record, "format_item_list"))
            dependencies.insert(edge.target);
        if (record.kind == "SgFunctionParameterScope")
          dependencies.insert(
              requiredSingleEdgeTarget(record, "completed_semantic_scope"));
        else
          collectTypeDependencyIds(record.properties, dependencies,
                                   symbol_dependencies, type_dependencies);
        if (record.kind == "SgLambdaExp") {
          for (const char *field : {"lambda_capture_list",
                                    "lambda_closure_class", "lambda_function"})
            dependencies.insert(requiredSingleEdgeTarget(record, field));
          if (const uint64_t scope_id =
                  singleEdgeTarget(record, "closure_scope"))
            dependencies.insert(scope_id);
          else {
            const auto &closure = ast.node(
                requiredSingleEdgeTarget(record, "lambda_closure_class"));
            const auto &context_record =
                ast.node(requiredSingleEdgeTarget(closure, "scope"));
            dependencies.insert(
                requiredSingleEdgeTarget(context_record, "parent"));
          }
          const auto &captures =
              ast.node(requiredSingleEdgeTarget(record, "lambda_capture_list"));
          for (const EdgeRecord &edge : edgesFor(captures, "capture_list"))
            dependencies.insert(edge.target);
        }
        if (record.kind == "SgLambdaCapture") {
          dependencies.insert(
              requiredSingleEdgeTarget(record, "capture_variable"));
          dependencies.insert(
              requiredSingleEdgeTarget(record, "capture_initializer"));
          if (const uint64_t closure =
                  singleEdgeTarget(record, "closure_variable"))
            dependencies.insert(closure);
        }
        if (record.kind == "SgCompoundLiteralExp") {
          const uint64_t name =
              requiredPositiveId(record.properties.at("symbol_declaration"),
                                 "compound literal variable");
          const uint64_t owner =
              requiredSingleEdgeTarget(ast.node(name), "parent");
          const uint64_t initializer =
              requiredSingleEdgeTarget(ast.node(name), "initptr");
          if (nodes.count(name) == 0 || nodes.count(owner) == 0 ||
              nodes.count(initializer) == 0)
            continue;
          auto *variable = nodeByIdAs<SgInitializedName>(nodes, name);
          if (variable->get_initializer() == nullptr ||
              variable->get_parent() == nullptr)
            continue;
          dependencies.insert(name);
          dependencies.insert(owner);
          dependencies.insert(initializer);
        }
        if (record.kind == "SgEnumVal") {
          const uint64_t declaration = requiredPositiveId(
              record.properties.at("declaration"), "enum literal declaration");
          dependencies.insert(declaration);
          type_dependencies.insert(declaration);
        }
        for (const char *field :
             {"conversion_type_id", "statement", "function_ref",
              "variant_function_ref", "base_function_ref", "initializers",
              "user_defined_identifier", "mapper_type", "mapper_variable"})
          if (const uint64_t target = singleEdgeTarget(record, field))
            dependencies.insert(target);
        if (record.kind == "SgStatementExpression")
          dependencies.insert(requiredSingleEdgeTarget(
              ast.node(requiredSingleEdgeTarget(record, "statement")),
              "statement_expression_semantic_scope"));
        if (record.kind == "SgForAllStatement") {
          const uint64_t header =
              requiredSingleEdgeTarget(record, "forall_header");
          dependencies.insert(header);
          dependencies.insert(requiredSingleEdgeTarget(record, "body"));
          dependencies.insert(requiredSingleEdgeTarget(record, "parent"));
          for (const auto &edge : edgesFor(ast.node(header), "expressions"))
            dependencies.insert(edge.target);
        }
        if (record.kind == "SgCaseOptionStmt" ||
            record.kind == "SgDefaultOptionStmt") {
          dependencies.insert(requiredSingleEdgeTarget(record, "body"));
          if (record.kind == "SgCaseOptionStmt") {
            const uint64_t key = requiredSingleEdgeTarget(record, "key");
            dependencies.insert(key);
            if (const uint64_t end = singleEdgeTarget(record, "key_range_end"))
              dependencies.insert(end);
            if (ast.node(key).kind == "SgExprListExp")
              for (const auto &edge : edgesFor(ast.node(key), "expressions"))
                dependencies.insert(edge.target);
          }
          if (singleEdgeTarget(record, "numeric_label") != 0)
            dependencies.insert(requiredSingleEdgeTarget(record, "parent"));
        }
        if (record.kind == "SgWhereStatement" ||
            record.kind == "SgElseWhereStatement") {
          dependencies.insert(requiredSingleEdgeTarget(record, "condition"));
          dependencies.insert(requiredSingleEdgeTarget(record, "body"));
          if (const uint64_t next = singleEdgeTarget(record, "elsewhere"))
            dependencies.insert(next);
          if (record.kind == "SgWhereStatement")
            dependencies.insert(requiredSingleEdgeTarget(record, "parent"));
        }
        if (record.kind == "SgFortranDo") {
          dependencies.insert(requiredSingleEdgeTarget(record, "body"));
          for (const char *field : {"initialization", "bound", "increment"})
            if (uint64_t id = singleEdgeTarget(record, field))
              dependencies.insert(id);
        }
        if (record.kind == "SgLabelRefExp") {
          const uint64_t parent = singleEdgeTarget(record, "parent");
          if (parent != 0)
            for (const char *field :
                 {"numeric_label", "else_numeric_label", "end_numeric_label"})
              if (singleEdgeTarget(ast.node(parent), field) == record.id)
                dependencies.insert(parent);
        }
      }
      for (const char *field :
           {"type", "source_type", "operand_type", "conversion_base_path",
            "source_derived_type_symbol", "source_declaration"}) {
        if (const JsonValue *type = record.properties.find(field)) {
          collectTypeDependencyIds(*type, dependencies, symbol_dependencies,
                                   type_dependencies);
        }
      }
      if (record.kind == "SgFortranImplicitRange") {
        const auto &symbol = record.properties.at("source_derived_type_symbol");
        if (symbol.kind != JsonValue::Kind::Null)
          dependencies.insert(requiredPositiveId(
              symbol.at("binding_scope"), "implicit range binding scope"));
      }
      if (function_syntax == nullptr &&
          !std::holds_alternative<DesignatorInput>(pending.input))
        for (const auto &edge : record.edges)
          if (edge.field != "parent" &&
              ast.node(edge.target).kind == "SgExprListExp")
            dependencies.insert(edge.target);
      if (std::any_of(
              dependencies.begin(), dependencies.end(), [&](uint64_t id) {
                auto *owner = array_owner(id);
                if (owner != nullptr && owner != construction &&
                    !owner->committed)
                  return true;
                if (direct || function_syntax != nullptr ||
                    lifetime_declarations.count(id) != 0 ||
                    type_dependencies.count(id) != 0 ||
                    record.kind == "SgFortranImplicitRange" ||
                    (id != record.id &&
                     !std::holds_alternative<DesignatorInput>(pending.input) &&
                     ast.node(id).kind == "SgExprListExp"))
                  return !declarationConstructionDependencyReady(
                      ast, nodes, id, symbol_dependencies.count(id) != 0,
                      type_dependencies.count(id) != 0,
                      record.kind == "SgLabelRefExp" ? V_SgLabelSymbol
                                                     : V_SgSymbol);
                return nodes.count(id) == 0;
              }))
        continue;
      auto all_constructed = [&](const std::vector<uint64_t> &ids) {
        return std::all_of(ids.begin(), ids.end(),
                           [&](uint64_t id) { return nodes.count(id) != 0; });
      };
      if (expression_fields != nullptr)
        validateExpressionLifetime(*expression_fields, record.properties,
                                   nodes);
      if (direct) {
        construct_direct_record(record, std::move(positions));
      } else if (const auto *input =
                     std::get_if<PrototypeInput>(&pending.input)) {
        auto *enclosing = nodeByIdAs<SgScopeStatement>(nodes, input->enclosing);
        if (nodes.count(input->scope) != 0 ||
            nodes.count(input->parameters) != 0)
          throw std::runtime_error(
              "AST JSON prototype region already published a child");
        auto &scope_fields = construction_fields.at(input->scope);
        auto &parameter_fields = construction_fields.at(input->parameters);
        auto scope_positions =
            constructSourcePositions(scope_fields.positions.value());
        auto parameter_positions =
            constructSourcePositions(parameter_fields.positions.value());
        RecordNodeConstruction local_ownership;
        RecordNodeConstruction &ownership = source_construction != nullptr
                                                ? source_construction->ownership
                                                : local_ownership;
        auto *prototype = ownership.make<SgFunctionParameterTypeList>();
        auto *scope = ownership.make<SgFunctionParameterScope>();
        auto *parameters = ownership.make<SgFunctionParameterList>();
        prototype->initialize_source_prototype(scope, parameters, enclosing);
        NodeMap group;
        publishConstructorOnlyNode(ast.node(input->scope), scope, group,
                                   std::move(scope_positions), scope_fields);
        publishConstructorOnlyNode(ast.node(input->parameters), parameters,
                                   group, std::move(parameter_positions),
                                   parameter_fields);
        publishConstructorOnlyNode(record, prototype, group,
                                   std::move(positions),
                                   construction_fields.at(record.id));
        nodes.reserve(nodes.size() + group.size());
        node_ids.reserve(node_ids.size() + group.size());
        for (const auto &[id, node] : group) {
          if (source_construction != nullptr)
            source_construction->retain(id, node);
          node_ids.emplace(node, id);
        }
        nodes.merge(group);
        ROSE_ASSERT(group.empty());
        local_ownership.publish();
      } else if (function_syntax != nullptr) {
        auto *function = nodeByIdAs<SgFunctionDeclaration>(nodes, record.id);
        if (function->get_type_syntax() != nullptr ||
            function->get_type_syntax_is_available())
          throw std::runtime_error(
              "AST JSON function source syntax was constructed more than once");
        SgFunctionType *syntax = nullableFunctionSyntaxTypeFromJson(
            record.properties.at("function_type_syntax"), nodes);
        if (syntax != nullptr &&
            (syntax == function->get_type() || syntax->get_parent() != nullptr))
          throw std::runtime_error(
              "AST JSON function does not own one distinct source function "
              "type syntax node");
        if (construction != nullptr && !construction->committed &&
            syntax != nullptr) {
          construction->ownership.own(syntax);
          construction->source_type_owners.push_back(function);
        }
        function->set_type_syntax(syntax);
        function->set_type_syntax_is_available(function_syntax->available);
        if (syntax != nullptr)
          syntax->set_parent(function);
      } else if (const auto *input =
                     std::get_if<ParameterListInput>(&pending.input)) {
        const NodeRecord &owner_record = ast.node(input->owner);
        if (owner_record.kind == "SgFunctionParameterTypeList") {
          if (nodes.count(record.id) == 0 || nodes.count(input->owner) == 0 ||
              !all_constructed(input->parameters))
            continue;
          auto *prototype =
              nodeByIdAs<SgFunctionParameterTypeList>(nodes, input->owner);
          auto *list = nodeByIdAs<SgFunctionParameterList>(nodes, record.id);
          auto *scope = prototype->get_source_parameter_scope();
          if (prototype->get_source_parameters() != list || scope == nullptr ||
              list->get_parent() != prototype ||
              scope->get_parent() != prototype)
            throw std::runtime_error(
                "AST JSON prototype has no exact constructed parameter region");
          SgInitializedNamePtrList parameters;
          for (uint64_t id : input->parameters) {
            auto *parameter = nodeByIdAs<SgInitializedName>(nodes, id);
            if (parameter->get_parent() != list ||
                parameter->get_scope() != scope ||
                parameter->get_declptr() != nullptr ||
                SageInterface::containsUnknownType(parameter->get_type()))
              throw std::runtime_error(
                  "AST JSON prototype parameter has conflicting ownership");
            parameters.push_back(parameter);
          }
          if (!list->get_args().empty() && list->get_args() != parameters)
            throw std::runtime_error("AST JSON prototype parameter order "
                                     "changed during construction");
          if (list->get_args().empty() && !parameters.empty()) {
            if (construction != nullptr && !construction->committed)
              construction->parameter_lists.push_back(list);
            for (auto *parameter : parameters)
              list->append_arg(parameter);
          }
          constructed_parameter_lists.insert(record.id);
          pending.completed = true;
          --remaining;
          made_progress = true;
          continue;
        }
        if (nodes.count(record.id) == 0 ||
            !declarationConstructionDependencyReady(ast, nodes, input->owner,
                                                    false, false) ||
            !all_constructed(input->parameters))
          continue;
        auto *list = nodeByIdAs<SgFunctionParameterList>(nodes, record.id);
        auto *function = nodeByIdAs<SgFunctionDeclaration>(nodes, input->owner);
        const bool semantic = requiredSingleEdgeTarget(
                                  owner_record, "parameterList") == record.id;
        const bool source =
            singleEdgeTarget(owner_record, "parameterList_syntax") == record.id;
        if ((!semantic && !source) ||
            (semantic && function->get_parameterList() != list) ||
            (source && function->get_parameterList_syntax() != list) ||
            list->get_parent() != function)
          throw std::runtime_error(
              "AST JSON function parameter list has conflicting construction "
              "ownership");
        SgInitializedNamePtrList parameters;
        for (uint64_t id : input->parameters) {
          auto *parameter = nodeByIdAs<SgInitializedName>(nodes, id);
          if (requiredSingleEdgeTarget(ast.node(id), "parent") != record.id ||
              requiredSingleEdgeTarget(ast.node(id), "declptr") !=
                  input->owner ||
              parameter->get_declptr() != function ||
              parameter->get_scope() == nullptr ||
              SageInterface::containsUnknownType(parameter->get_type()) ||
              (parameter->get_parent() != nullptr &&
               parameter->get_parent() != list) ||
              (!list->get_args().empty() && parameter->get_parent() != list))
            throw std::runtime_error(
                "AST JSON function parameter has no exact constructed "
                "identity, type and owner");
          parameters.push_back(parameter);
        }
        if (!list->get_args().empty() && list->get_args() != parameters)
          throw std::runtime_error(
              "AST JSON function parameter list differs from its constructed "
              "ordered parameters");
        if (list->get_args().empty() && !parameters.empty()) {
          if (construction != nullptr && !construction->committed) {
            construction->parameter_lists.push_back(list);
            for (auto *parameter : parameters)
              construction->parameter_parents.emplace(parameter,
                                                      parameter->get_parent());
          }
          for (auto *parameter : parameters)
            list->append_arg(parameter);
        }
        constructed_parameter_lists.insert(record.id);
      } else if (const auto *input =
                     std::get_if<VariableListInput>(&pending.input)) {
        auto *declaration = nodeByIdAs<SgVariableDeclaration>(nodes, record.id);
        SourceDeclaratorScopeConstruction source_scope(record, declaration,
                                                       nodes);
        source_scope.populate(ast, record, nodes);
        SgInitializedNamePtrList variables;
        for (uint64_t id : input->variables) {
          auto *name = nodeByIdAs<SgInitializedName>(nodes, id);
          if (requiredSingleEdgeTarget(ast.node(id), "parent") != record.id ||
              (name->get_parent() != nullptr &&
               name->get_parent() != declaration))
            throw std::runtime_error(
                "AST JSON variable has conflicting construction ownership");
          variables.push_back(name);
        }
        if (!declaration->get_variables().empty() &&
            declaration->get_variables() != variables)
          throw std::runtime_error(
              "AST JSON variable declaration differs from constructed names: "
              "id=" +
              std::to_string(record.id) + " existing=" +
              std::to_string(declaration->get_variables().size()) +
              " expected=" + std::to_string(variables.size()));
        if (declaration->get_variables().empty() && !variables.empty()) {
          if (construction != nullptr && !construction->committed) {
            construction->variable_declarations.push_back(declaration);
            for (auto *name : variables)
              construction->parameter_parents.emplace(name, name->get_parent());
          }
          for (auto *name : variables)
            name->set_parent(declaration);
          declaration->get_variables() = std::move(variables);
        }
        constructed_variable_lists.insert(record.id);
        source_scope.publish();
      } else if (const auto *input =
                     std::get_if<InitializedNameInput>(&pending.input)) {
        if (nodes.count(record.id) == 0 ||
            (input->initializer != 0 && nodes.count(input->initializer) == 0))
          continue;
        if (uint64_t parent = singleEdgeTarget(record, "parent");
            parent != 0 && ast.node(parent).kind == "SgFunctionParameterList") {
          const NodeRecord &owner =
              ast.node(requiredSingleEdgeTarget(ast.node(parent), "parent"));
          if (isFunctionDeclarationRecordKind(owner.kind) &&
              (constructed_parameter_lists.count(
                   requiredSingleEdgeTarget(owner, "parameterList")) == 0 ||
               (singleEdgeTarget(owner, "parameterList_syntax") != 0 &&
                constructed_parameter_lists.count(
                    singleEdgeTarget(owner, "parameterList_syntax")) == 0)))
            continue;
        }
        auto *name = nodeByIdAs<SgInitializedName>(nodes, record.id);
        if (input->initializer != 0) {
          auto *initializer =
              nodeByIdAs<SgInitializer>(nodes, input->initializer);
          if (requiredSingleEdgeTarget(ast.node(input->initializer),
                                       "parent") != record.id ||
              (name->get_initializer() != nullptr &&
               name->get_initializer() != initializer) ||
              (initializer->get_parent() != nullptr &&
               initializer->get_parent() != name) ||
              ((name->get_initializer() == nullptr) !=
               (initializer->get_parent() == nullptr)))
            throw std::runtime_error(
                "AST JSON initialized-name has conflicting initializer "
                "construction ownership");
          if (auto *assignment = isSgAssignInitializer(initializer)) {
            if (auto *value = isSgEnumVal(assignment->get_operand_i());
                value != nullptr &&
                name->get_enum_constant_value().has_value() &&
                value->get_value() != *name->get_enum_constant_value())
              throw std::runtime_error("AST JSON enum initializer disagrees "
                                       "with its evaluated value");
          }
          if (name->get_initializer() == nullptr) {
            if ((name->get_structured_binding_scope() != nullptr ||
                 name->get_structured_binding_kind() ==
                     SgInitializedName::
                         e_structured_binding_holding_variable) &&
                !SgInitializedName::is_valid_structured_binding_initializer(
                    name->get_type(), initializer))
              throw std::runtime_error("AST JSON structured-binding storage "
                                       "initializer has incompatible type");
            if (construction != nullptr && !construction->committed)
              construction->initialized_names.push_back(name);
            name->set_initializer(initializer);
          }
          if (initializer->get_parent() != name)
            throw std::runtime_error(
                "AST JSON initialized-name failed exact initializer ownership "
                "publication");
        }
        constructed_initializers.insert(record.id);
      } else if (const auto *input =
                     std::get_if<DataValueInput>(&pending.input)) {
        if (nodes.count(input->constant) == 0 ||
            (input->repeat != 0 && nodes.count(input->repeat) == 0))
          continue;
        auto *constant = isSgExpression(require_unowned_delayed_child(
            input->constant, record, "DATA constant"));
        auto *repeat = input->repeat == 0
                           ? nullptr
                           : isSgExpression(require_unowned_delayed_child(
                                 input->repeat, record, "DATA repeat"));
        if (!SgDataStatementValue::is_valid_value(constant, repeat))
          throw std::runtime_error(
              "AST JSON DATA value has invalid typed expressions");
        publish_delayed_node(record, std::move(positions),
                             new SgDataStatementValue(constant, repeat));
      } else if (const auto *input =
                     std::get_if<OwnedListInput>(&pending.input)) {
        if (auto *clause_list = isSgOmpClauseList(nodeById(nodes, record.id))) {
          SgOmpClausePtrList children;
          for (uint64_t id : input->children) {
            auto *child = nodeByIdAs<SgOmpClause>(nodes, id);
            if (child->get_parent() != nullptr &&
                child->get_parent() != clause_list)
              throw std::runtime_error(
                  "AST JSON OpenMP clause already has a different owner");
            children.push_back(child);
          }
          if (!clause_list->get_clauses().empty() &&
              clause_list->get_clauses() != children)
            throw std::runtime_error(
                "AST JSON OpenMP clause list differs from construction inputs");
          if (clause_list->get_clauses().empty()) {
            if (construction != nullptr && !construction->committed &&
                !children.empty()) {
              construction->clause_lists.push_back(clause_list);
              for (auto *child : children)
                construction->parameter_parents.emplace(child,
                                                        child->get_parent());
            }
            for (auto *child : children)
              clause_list->append_clause(child);
          }
        } else {
          auto *list = nodeByIdAs<SgExprListExp>(nodes, record.id);
          SgExpressionPtrList children;
          std::unordered_set<SgExpression *> seen;
          for (uint64_t id : input->children) {
            auto *child = nodeByIdAs<SgExpression>(nodes, id);
            if (requiredSingleEdgeTarget(ast.node(id), "parent") != record.id ||
                (child->get_parent() != nullptr &&
                 child->get_parent() != list) ||
                !seen.insert(child).second)
              throw std::runtime_error(
                  "AST JSON expression list has conflicting "
                  "construction ownership");
            children.push_back(child);
          }
          if (!list->get_expressions().empty() &&
              list->get_expressions() != children)
            throw std::runtime_error(
                "AST JSON expression list differs from construction inputs");
          if (list->get_expressions().empty()) {
            if (construction != nullptr && !construction->committed &&
                !children.empty()) {
              construction->expression_lists.push_back(list);
              for (auto *child : children)
                construction->parameter_parents.emplace(child,
                                                        child->get_parent());
            }
            for (auto *child : children)
              list->append_expression(child);
          }
          for (auto *child : children)
            if (child->get_parent() != list)
              throw std::runtime_error(
                  "AST JSON expression list lost its constructed child owner");
        }
      } else if (const auto *input =
                     std::get_if<NamelistIoInput>(&pending.input)) {
        if (!exactSymbolBindingDependenciesReady(*input->binding, nodes))
          continue;
        auto *symbol = exactBoundSymbolFromJson(*input->binding, nodes);
        SgIOStatement *statement;
        if (record.kind == "SgReadStatement")
          statement = new SgReadStatement(symbol);
        else
          statement = new SgWriteStatement(symbol);
        publish_delayed_node(record, std::move(positions), statement);
      } else if (const auto *input =
                     std::get_if<NamelistGroupInput>(&pending.input)) {
        SgVarRefExpPtrList objects;
        for (uint64_t id : input->objects) {
          auto *reference = isSgVarRefExp(
              require_unowned_delayed_child(id, record, "NAMELIST object"));
          if (reference == nullptr)
            throw std::runtime_error(
                "AST JSON NAMELIST object is not a variable reference");
          objects.push_back(reference);
        }
        if (!SgNameGroup::is_valid_group(input->name, objects))
          throw std::runtime_error(
              "AST JSON NAMELIST has invalid typed objects");
        publish_delayed_node(record, std::move(positions),
                             new SgNameGroup(input->name, objects));
      } else if (const auto *input =
                     std::get_if<NamelistInput>(&pending.input)) {
        if (!all_constructed(input->groups))
          continue;
        SgNameGroupPtrList groups;
        for (uint64_t id : input->groups) {
          auto *group = isSgNameGroup(
              require_unowned_delayed_child(id, record, "NAMELIST group"));
          if (group == nullptr)
            throw std::runtime_error("AST JSON NAMELIST has an invalid group");
          group->validate_group();
          groups.push_back(group);
        }
        auto *statement = new SgNamelistStatement(groups);
        statement->set_parent(nodeByIdAs<SgScopeStatement>(
            nodes, requiredSingleEdgeTarget(record, "parent")));
        publish_delayed_node(record, std::move(positions), statement);
      } else if (const auto *input =
                     std::get_if<DataObjectInput>(&pending.input)) {
        if (nodes.count(input->list) == 0)
          continue;
        auto *list = isSgExprListExp(require_unowned_delayed_child(
            input->list, record, "DATA object list"));
        if (list == nullptr)
          throw std::runtime_error(
              "AST JSON DATA object requires a typed expression list");
        publish_delayed_node(record, std::move(positions),
                             new SgDataStatementObject(list));
      } else if (const auto *input =
                     std::get_if<DataGroupInput>(&pending.input)) {
        if (!all_constructed(input->objects) || !all_constructed(input->values))
          continue;
        SgDataStatementObjectPtrList objects;
        SgDataStatementValuePtrList values;
        for (uint64_t id : input->objects)
          objects.push_back(isSgDataStatementObject(
              require_unowned_delayed_child(id, record, "DATA object")));
        for (uint64_t id : input->values)
          values.push_back(isSgDataStatementValue(
              require_unowned_delayed_child(id, record, "DATA value")));
        if (!SgDataStatementGroup::is_valid_group(objects, values))
          throw std::runtime_error(
              "AST JSON DATA group has invalid typed children");
        publish_delayed_node(record, std::move(positions),
                             new SgDataStatementGroup(objects, values));
      } else if (const auto *input =
                     std::get_if<DataOwnerInput>(&pending.input)) {
        if (!all_constructed(input->children))
          continue;
        if (record.kind == "SgFortranDataInitializer") {
          const bool braced = input->braced.value();
          const uint64_t owner = requiredSingleEdgeTarget(record, "parent");
          if (nodes.count(owner) == 0)
            continue;
          SgType *type =
              nodeByIdAs<SgInitializedName>(nodes, owner)->get_type();
          SgDataStatementValuePtrList values;
          for (uint64_t id : input->children)
            values.push_back(
                isSgDataStatementValue(require_unowned_delayed_child(
                    id, record, "DATA initializer value")));
          if (!SgFortranDataInitializer::is_valid_initializer(values, type))
            throw std::runtime_error("AST JSON DATA initializer has invalid "
                                     "destination type or values");
          auto *initializer = new SgFortranDataInitializer(values, type);
          initializer->set_is_braced_initialized(braced);
          publish_delayed_node(record, std::move(positions), initializer);
        } else {
          SgDataStatementGroupPtrList groups;
          for (uint64_t id : input->children)
            groups.push_back(isSgDataStatementGroup(
                require_unowned_delayed_child(id, record, "DATA group")));
          if (!SgAttributeSpecificationStatement::is_valid_data_groups(
                  SgAttributeSpecificationStatement::e_dataStatement, groups))
            throw std::runtime_error(
                "AST JSON DATA statement has invalid typed groups");
          publish_delayed_node(
              record, std::move(positions),
              new SgAttributeSpecificationStatement(
                  SgAttributeSpecificationStatement::e_dataStatement, groups));
        }
      } else if (attributes != nullptr) {
        if (nodes.count(record.id) == 0 ||
            (attributes->explicit_condition != 0 &&
             nodes.count(attributes->explicit_condition) == 0) ||
            (attributes->launch_bounds != 0 &&
             nodes.count(attributes->launch_bounds) == 0) ||
            std::any_of(attributes->alignments.begin(),
                        attributes->alignments.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        SgNode *owner = nodeById(nodes, record.id);
        auto *declaration = isSgDeclarationStatement(owner);
        auto *name = isSgInitializedName(owner);
        if (declaration == nullptr && name == nullptr)
          throw std::runtime_error(
              "AST JSON alignment has no typed declaration owner");
        auto &list = declaration != nullptr
                         ? declaration->get_alignment_specifiers()
                         : name->get_alignment_specifiers();
        SgAlignmentSpecifierPtrList specifiers;
        for (uint64_t id : attributes->alignments) {
          auto *specifier = nodeByIdAs<SgAlignmentSpecifier>(nodes, id);
          if (specifier->get_parent() != nullptr &&
              specifier->get_parent() != owner)
            throw std::runtime_error(
                "AST JSON alignment has conflicting construction ownership");
          specifiers.push_back(specifier);
        }
        auto *function = isSgFunctionDeclaration(owner);
        auto *explicit_condition =
            attributes->explicit_condition != 0
                ? nodeByIdAs<SgExpression>(nodes,
                                           attributes->explicit_condition)
                : nullptr;
        if (explicit_condition != nullptr &&
            (function == nullptr ||
             function->get_explicit_specifier_expression() != nullptr ||
             (explicit_condition->get_parent() != nullptr &&
              explicit_condition->get_parent() != owner)))
          throw std::runtime_error("AST JSON explicit condition conflicts with "
                                   "construction ownership");
        auto *launch_bounds =
            attributes->launch_bounds != 0
                ? nodeByIdAs<SgExpression>(nodes, attributes->launch_bounds)
                : nullptr;
        if (!list.empty() ||
            (launch_bounds != nullptr &&
             (function == nullptr || (launch_bounds->get_parent() != nullptr &&
                                      launch_bounds->get_parent() != owner))) ||
            (function != nullptr &&
             function->get_cuda_launch_bounds_expression() != nullptr))
          throw std::runtime_error(
              "AST JSON declaration attributes conflict with construction");
        if (!SgFunctionDeclaration::cuda_launch_bounds_operands_are_valid(
                launch_bounds))
          throw std::runtime_error(
              "AST JSON CUDA launch bounds have no constructed semantic type");
        list.swap(specifiers);
        if (explicit_condition != nullptr) {
          function->set_explicit_specifier_expression(explicit_condition);
          explicit_condition->set_parent(function);
        }
        for (auto *specifier : list)
          specifier->set_parent(owner);
        if (launch_bounds != nullptr) {
          function->set_cuda_launch_bounds_expression(launch_bounds);
          launch_bounds->set_parent(function);
          function->validate_cuda_launch_bounds_expression();
        }
      } else if (const auto *input =
                     std::get_if<FortranImplicitRangeInput>(&pending.input)) {
        publish_delayed_node(record, std::move(positions),
                             constructFortranImplicitRange(
                                 input->first, input->last, input->type_spec,
                                 record.properties, nodes));
      } else if (const auto *input =
                     std::get_if<ImplicitStatementInput>(&pending.input)) {
        if (std::any_of(input->ranges.begin(), input->ranges.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        SgFortranImplicitRangePtrList ranges;
        for (uint64_t id : input->ranges) {
          auto *range = isSgFortranImplicitRange(
              require_unowned_delayed_child(id, record, "implicit range"));
          if (range == nullptr)
            throw std::runtime_error(
                "AST JSON implicit child is not a typed range");
          ranges.push_back(range);
        }
        if (!SgImplicitStatement::is_valid_implicit_statement(input->spec,
                                                              ranges))
          throw std::runtime_error(
              "AST JSON implicit statement has invalid typed ranges");
        publish_delayed_node(record, std::move(positions),
                             new SgImplicitStatement(input->spec, ranges));
      } else if (const auto *input =
                     std::get_if<AlignmentInput>(&pending.input)) {
        if (input->operand != 0 && nodes.count(input->operand) == 0)
          continue;
        auto *operand = input->operand != 0
                            ? isSgExpression(require_unowned_delayed_child(
                                  input->operand, record, "alignment operand"))
                            : nullptr;
        if (input->operand != 0 && operand == nullptr)
          throw std::runtime_error(
              "AST JSON alignment operand is not an expression");
        publish_delayed_node(
            record, std::move(positions),
            alignmentSpecifierFromProperties(record.properties, operand));
      } else if (const auto *input =
                     std::get_if<AsmOperandInput>(&pending.input)) {
        if (nodes.count(input->expression) == 0)
          continue;
        auto *expression = isSgExpression(require_unowned_delayed_child(
            input->expression, record, "asm operand expression"));
        publish_delayed_node(record, std::move(positions),
                             constructAsmOperand(input->fields, expression));
      } else if (const auto *input =
                     std::get_if<AsmStatementInput>(&pending.input)) {
        if (std::any_of(input->operands.begin(), input->operands.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        SgExpressionPtrList operands;
        for (uint64_t id : input->operands) {
          auto *operand = isSgAsmOp(require_unowned_delayed_child(
              id, record, "asm statement operand"));
          if (operand == nullptr)
            throw std::runtime_error("AST JSON asm child is not an operand");
          operands.push_back(operand);
        }
        publish_delayed_node(record, std::move(positions),
                             constructAsmStatement(input->fields, operands));
      } else if (const auto *input =
                     std::get_if<OperandStatementInput>(&pending.input)) {
        if (input->expression != 0 && nodes.count(input->expression) == 0)
          continue;
        SgExpression *expression = nullptr;
        if (input->expression != 0) {
          expression = isSgExpression(require_unowned_delayed_child(
              input->expression, record, record.kind + " expression"));
          if (expression == nullptr)
            throw std::runtime_error(
                "AST JSON statement operand is not an expression");
        }
        if (input->return_statement) {
          const auto &fields = *input->return_statement;
          auto *candidate =
              fields.nrvo_candidate != 0
                  ? nodeByIdAs<SgInitializedName>(nodes, fields.nrvo_candidate)
                  : nullptr;
          if (!SgReturnStmt::is_valid_nrvo_binding(expression, candidate) ||
              (candidate != nullptr &&
               (candidate->get_scope() !=
                    nodeByIdAs<SgScopeStatement>(
                        nodes, requiredSingleEdgeTarget(
                                   ast.node(fields.nrvo_candidate), "scope")) ||
                candidate->get_declptr() !=
                    nodeByIdAs<SgVariableDeclaration>(
                        nodes,
                        requiredSingleEdgeTarget(
                            ast.node(fields.nrvo_candidate), "parent")))))
            throw std::runtime_error(
                "AST JSON return has no exact NRVO binding");
          auto *statement = new SgReturnStmt(expression, candidate);
          statement->set_return_keyword_kind(fields.keyword);
          publish_delayed_node(record, std::move(positions), statement);
        } else if (record.kind == "SgGotoStatement") {
          publish_delayed_node(record, std::move(positions),
                               new SgGotoStatement(nullptr, expression));
        } else {
          publish_delayed_node(record, std::move(positions),
                               new SgExprStatement(expression));
        }
      } else if (const auto *input =
                     std::get_if<AttributedStatementInput>(&pending.input)) {
        if (nodes.count(input->statement) == 0 || nodes.count(input->list) == 0)
          continue;
        auto *wrapped = isSgStatement(require_unowned_delayed_child(
            input->statement, record, "attributed statement child"));
        auto *list = isSgStatementAttributeList(require_unowned_delayed_child(
            input->list, record, "attributed statement list"));
        if (wrapped == nullptr || list == nullptr ||
            list->get_attributes().empty())
          throw std::runtime_error(
              "AST JSON attributed statement has invalid children");
        publish_delayed_node(record, std::move(positions),
                             new SgAttributedStatement(wrapped, list));
      } else if (const auto *input =
                     std::get_if<StatementAttributeInput>(&pending.input)) {
        if (input->operand != 0 && nodes.count(input->operand) == 0)
          continue;
        SgExpression *operand = nullptr;
        if (input->operand != 0) {
          operand = isSgExpression(require_unowned_delayed_child(
              input->operand, record, "statement attribute operand"));
          if (operand == nullptr)
            throw std::runtime_error(
                "AST JSON statement attribute operand has wrong kind");
        }
        const auto &fields =
            construction_fields.at(record.id).statement_attribute;
        ROSE_ASSERT(fields.has_value());
        auto *attribute = new SgStatementAttribute(
            fields->kind, fields->spelling, operand, fields->integral,
            fields->option, fields->state);
        attribute->validate();
        publish_delayed_node(record, std::move(positions), attribute);
      } else if (const auto *input =
                     std::get_if<StatementAttributeListInput>(&pending.input)) {
        if (std::any_of(input->attributes.begin(), input->attributes.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        SgStatementAttributePtrList attributes;
        for (uint64_t id : input->attributes) {
          auto *attribute = isSgStatementAttribute(
              require_unowned_delayed_child(id, record, "statement attribute"));
          if (attribute == nullptr)
            throw std::runtime_error(
                "AST JSON statement attribute has wrong kind");
          attributes.push_back(attribute);
        }
        auto list = std::make_unique<SgStatementAttributeList>();
        for (auto *attribute : attributes)
          list->append_attribute(attribute);
        publish_delayed_node(record, std::move(positions), list.release());
      } else if (const auto *input =
                     std::get_if<RequiresInput>(&pending.input)) {
        if (nodes.count(input->requirements) == 0 ||
            (input->parameters != 0 && (nodes.count(input->parameters) == 0 ||
                                        nodes.count(input->scope) == 0)) ||
            (input->syntax != 0 && nodes.count(input->syntax) == 0) ||
            std::any_of(input->written_arguments.begin(),
                        input->written_arguments.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }) ||
            std::any_of(input->arguments.begin(), input->arguments.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        auto *requirements = isSgExprListExp(require_unowned_delayed_child(
            input->requirements, record, "requires-expression requirements"));
        if (requirements == nullptr)
          throw std::runtime_error("AST JSON requires-expression needs a "
                                   "typed requirement list");
        SgFunctionParameterList *parameters = nullptr;
        SgFunctionParameterList *syntax = nullptr;
        SgFunctionParameterScope *scope = nullptr;
        if (input->parameters != 0) {
          parameters = isSgFunctionParameterList(require_unowned_delayed_child(
              input->parameters, record, "requires-expression parameters"));
          scope = isSgFunctionParameterScope(require_unowned_delayed_child(
              input->scope, record, "requires-expression parameter scope"));
          if (parameters == nullptr || scope == nullptr ||
              !parameters->get_args().empty())
            throw std::runtime_error("AST JSON requires-expression needs a "
                                     "fresh typed parameter region");
          for (uint64_t id : input->arguments) {
            auto *parameter = isSgInitializedName(
                require_unowned_delayed_child(id, ast.node(input->parameters),
                                              "requires-expression parameter"));
            if (parameter == nullptr || parameter->get_scope() != scope)
              throw std::runtime_error("AST JSON requires-expression parameter "
                                       "has conflicting ownership or scope");
            parameter->set_parent(parameters);
            parameters->get_args().push_back(parameter);
          }
          if (input->syntax != 0) {
            syntax = isSgFunctionParameterList(require_unowned_delayed_child(
                input->syntax, record,
                "requires-expression written parameters"));
            if (syntax == nullptr || !syntax->get_args().empty())
              throw std::runtime_error("AST JSON requires-expression needs a "
                                       "fresh written parameter list");
            for (uint64_t id : input->written_arguments) {
              auto *parameter =
                  isSgInitializedName(require_unowned_delayed_child(
                      id, ast.node(input->syntax),
                      "requires-expression written parameter"));
              if (parameter == nullptr || parameter->get_scope() != scope)
                throw std::runtime_error("AST JSON requires-expression written "
                                         "parameter has conflicting scope");
              auto *object = parameters->get_args()[syntax->get_args().size()];
              if (!SageInterface::cxxParameterTypeMatchesSemanticType(
                      parameter->get_type(), object->get_type()))
                throw std::runtime_error(
                    "AST JSON requires-expression written parameter has a "
                    "conflicting object type");
              parameter->set_parent(syntax);
              syntax->get_args().push_back(parameter);
            }
          }
        }
        publish_delayed_node(record, std::move(positions),
                             new SgRequiresExpr(nullptr, parameters, scope,
                                                requirements, syntax));
      } else if (const auto *input =
                     std::get_if<RequirementInput>(&pending.input)) {
        if (nodes.count(input->operand) == 0 ||
            (input->type_constraint != 0 &&
             nodes.count(input->type_constraint) == 0))
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, "requirement operand"));
        SgExpression *constraint = nullptr;
        if (input->type_constraint != 0) {
          constraint = isSgExpression(require_unowned_delayed_child(
              input->type_constraint, record, "requirement type constraint"));
          if (constraint == nullptr)
            throw std::runtime_error(
                "AST JSON requirement constraint is not an expression");
        }
        publish_delayed_node(
            record, std::move(positions),
            constructOperandRequirement(input->kind, operand, constraint,
                                        input->noexcept_required));
      } else if (const auto *input =
                     std::get_if<DefaultInitializerInput>(&pending.input)) {
        if (nodes.count(input->expression) == 0)
          continue;
        if (input->kind == SgCxxDefaultInitExp::e_member) {
          if (nodes.count(input->parameter) == 0)
            continue;
          const auto &source = ast.node(input->parameter);
          const uint64_t declaration =
              requiredSingleEdgeTarget(source, "parent");
          const uint64_t initializer =
              requiredSingleEdgeTarget(source, "initptr");
          if (nodes.count(initializer) == 0 ||
              constructed_variable_lists.count(declaration) == 0 ||
              constructed_initializers.count(input->parameter) == 0)
            continue;
        } else if (input->parameter != 0) {
          if (nodes.count(input->parameter) == 0)
            continue;
          const uint64_t list_id =
              requiredSingleEdgeTarget(ast.node(input->parameter), "parent");
          if (nodes.count(list_id) == 0)
            continue;
          auto *list = nodeByIdAs<SgFunctionParameterList>(nodes, list_id);
          const auto parameters = edgesFor(ast.node(list_id), "args");
          if (list->get_args().size() != parameters.size() ||
              list->get_parent() == nullptr)
            continue;
          bool complete = true;
          for (size_t index = 0; index < parameters.size(); ++index) {
            auto parameter = nodes.find(parameters[index].target);
            complete &= parameter != nodes.end() &&
                        list->get_args()[index] == parameter->second &&
                        parameter->second->get_parent() == list;
          }
          if (!complete)
            continue;
        }
        auto *parameter = defaultInitializerDeclarationFromJson(
            record.properties.at("source_declaration"), nodes);
        auto *expression = isSgExpression(require_unowned_delayed_child(
            input->expression, record, "default argument expansion"));
        auto *type = typeFromJson(record.properties.at("type"), nodes);
        if (!SgCxxDefaultInitExp::is_valid_use(parameter, expression,
                                               input->kind) ||
            !SageInterface::isEquivalentType(expression->get_type(), type))
          throw std::runtime_error(
              "AST JSON default argument has invalid exact typed input");
        publish_delayed_node(
            record, std::move(positions),
            new SgCxxDefaultInitExp(parameter, expression, input->kind));
      } else if (const auto *input =
                     std::get_if<OpaqueBindingInput>(&pending.input)) {
        if (nodes.count(input->source) == 0)
          continue;
        auto *source = isSgExpression(require_unowned_delayed_child(
            input->source, record, "opaque source"));
        auto *type = typeFromJson(record.properties.at("type"), nodes);
        if (!SgOpaqueValueBinding::is_valid_binding(source, type,
                                                    input->fields.category,
                                                    input->fields.object_kind))
          throw std::runtime_error(
              "AST JSON opaque binding has invalid typed source");
        publish_delayed_node(
            record, std::move(positions),
            new SgOpaqueValueBinding(source, type, input->fields.category,
                                     input->fields.object_kind));
      } else if (const auto *input =
                     std::get_if<OpaqueReferenceInput>(&pending.input)) {
        if (nodes.count(input->binding) == 0)
          continue;
        auto *binding = nodeByIdAs<SgOpaqueValueBinding>(nodes, input->binding);
        if (!SgOpaqueValueReference::is_valid_reference(binding) ||
            record.properties.requiredBool("lvalue") != binding->get_lvalue())
          throw std::runtime_error(
              "AST JSON opaque reference has invalid binding");
        if (!SageInterface::isEquivalentType(
                binding->get_type(),
                typeFromJson(record.properties.at("type"), nodes)))
          throw std::runtime_error(
              "AST JSON opaque reference has inconsistent type");
        publish_delayed_node(record, std::move(positions),
                             new SgOpaqueValueReference(binding));
      } else if (const auto *input =
                     std::get_if<PseudoObjectInput>(&pending.input)) {
        if (nodes.count(input->written) == 0 ||
            nodes.count(input->evaluations) == 0 ||
            (input->result != 0 && nodes.count(input->result) == 0) ||
            !all_constructed(input->captures))
          continue;
        auto *evaluations =
            nodeByIdAs<SgExprListExp>(nodes, input->evaluations);
        const auto steps =
            edgesFor(ast.node(input->evaluations), "expressions");
        if (evaluations->get_expressions().size() != steps.size())
          continue;
        auto *written = isSgExpression(require_unowned_delayed_child(
            input->written, record, "pseudo-object written expression"));
        require_unowned_delayed_child(input->evaluations, record,
                                      "pseudo-object evaluations");
        auto *result = input->result == 0
                           ? nullptr
                           : nodeByIdAs<SgExpression>(nodes, input->result);
        SgOpaqueValueBindingPtrList captures;
        for (uint64_t id : input->captures)
          captures.push_back(nodeByIdAs<SgOpaqueValueBinding>(nodes, id));
        auto *type = typeFromJson(record.properties.at("type"), nodes);
        if (!SgPseudoObjectExp::is_valid_sequence(
                written, evaluations, result, captures, type,
                input->fields.category, input->fields.object_kind))
          throw std::runtime_error(
              "AST JSON pseudo-object has invalid evaluation sequence");
        publish_delayed_node(record, std::move(positions),
                             new SgPseudoObjectExp(written, evaluations, result,
                                                   captures, type,
                                                   input->fields.category,
                                                   input->fields.object_kind));
      } else if (const auto *input =
                     std::get_if<ArrayBindingInput>(&pending.input)) {
        if (nodes.count(input->source) == 0)
          continue;
        auto *source = isSgExpression(require_unowned_delayed_child(
            input->source, record, "array common source"));
        if (!SgCxxArrayInitBinding::is_valid_source(source, input->category))
          throw std::runtime_error("AST JSON array initialization binding has "
                                   "invalid common source");
        publish_delayed_node(
            record, std::move(positions),
            new SgCxxArrayInitBinding(source, input->category));
      } else if (const auto *input =
                     std::get_if<ArrayReferenceInput>(&pending.input)) {
        if (nodes.count(input->binding) == 0)
          continue;
        auto *binding =
            nodeByIdAs<SgCxxArrayInitBinding>(nodes, input->binding);
        auto *type = typeFromJson(record.properties.at("type"), nodes);
        if (!SgCxxArrayInitReference::is_valid_reference(
                binding, input->fields.kind, type, input->fields.category))
          throw std::runtime_error("AST JSON array initialization reference "
                                   "has invalid typed input");
        publish_delayed_node(
            record, std::move(positions),
            new SgCxxArrayInitReference(binding, input->fields.kind, type,
                                        input->fields.category));
      } else if (const auto *input =
                     std::get_if<ArrayLoopInput>(&pending.input)) {
        if (nodes.count(input->binding) == 0 ||
            nodes.count(input->element) == 0)
          continue;
        auto *binding = isSgCxxArrayInitBinding(require_unowned_delayed_child(
            input->binding, record, "array binding"));
        auto *element = isSgExpression(require_unowned_delayed_child(
            input->element, record, "array element initializer"));
        auto *type = typeFromJson(record.properties.at("type"), nodes);
        if (!SgCxxArrayInitLoop::is_valid_loop(binding, element, type,
                                               input->count))
          throw std::runtime_error(
              "AST JSON array initialization loop has invalid typed operands");
        publish_delayed_node(
            record, std::move(positions),
            new SgCxxArrayInitLoop(binding, element, type, input->count));
      } else if (const auto *input = std::get_if<MacroInput>(&pending.input)) {
        if (nodes.count(input->expanded) == 0)
          continue;
        auto *expanded = isSgExpression(require_unowned_delayed_child(
            input->expanded, record,
            "SgMacroExpansionExp expanded expression"));
        if (expanded == nullptr)
          throw std::runtime_error(
              "AST JSON macro expansion child is not an expression");
        const std::string spelling =
            record.properties.requiredString("spelling");
        const bool modified =
            record.properties.requiredBool("expansion_is_modified");
        auto *macro = new SgMacroExpansionExp(spelling, expanded, modified);
        publish_delayed_node(record, std::move(positions), macro);
      } else if (const auto *input =
                     std::get_if<ActualArgumentInput>(&pending.input)) {
        if (nodes.count(input->expression) == 0)
          continue;
        auto *expression = isSgExpression(require_unowned_delayed_child(
            input->expression, record, "named argument expression"));
        if (expression == nullptr)
          throw std::runtime_error(
              "AST JSON named argument child is not an expression");
        auto *argument =
            new SgActualArgumentExpression(input->name, expression);
        expression->set_parent(argument);
        publish_delayed_node(record, std::move(positions), argument);
      } else if (const auto *input =
                     std::get_if<AssignmentInput>(&pending.input)) {
        if (nodes.find(input->operand) == nodes.end()) {
          continue;
        }
        SgExpression *operand_expression =
            isSgExpression(require_unowned_delayed_child(
                input->operand, record, "SgAssignInitializer operand"));
        if (operand_expression == nullptr) {
          throw std::runtime_error(
              "AST JSON SgAssignInitializer operand is not an expression");
        }
        SgType *destination_type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        auto *initializer =
            new SgAssignInitializer(operand_expression, destination_type);
        initializer->set_source_form(input->source.source_form);
        initializer->set_is_braced_initialized(input->source.braced);
        if (operand_expression->get_parent() != initializer) {
          throw std::runtime_error(
              "AST JSON SgAssignInitializer did not claim its exact operand");
        }
        publish_delayed_node(record, std::move(positions), initializer);
      } else if (const auto *input =
                     std::get_if<UnaryTraitInput>(&pending.input)) {
        if ((input->operand != 0 && nodes.count(input->operand) == 0) ||
            (input->type_defining_declaration != 0 &&
             nodes.count(input->type_defining_declaration) == 0))
          continue;
        SgExpression *operand = nullptr;
        if (input->operand != 0) {
          operand = isSgExpression(require_unowned_delayed_child(
              input->operand, record, "unary trait operand"));
          if (operand == nullptr) {
            throw std::runtime_error(
                "AST JSON unary trait operand is not an expression");
          }
        }
        SgDeclarationStatement *type_defining_declaration = nullptr;
        if (input->type_defining_declaration != 0) {
          type_defining_declaration =
              isSgDeclarationStatement(require_unowned_delayed_child(
                  input->type_defining_declaration, record,
                  "unary trait type-defining declaration"));
          if (type_defining_declaration == nullptr)
            throw std::runtime_error("AST JSON unary trait type-defining "
                                     "declaration is not a declaration");
        }
        SgType *operand_type =
            nullableTypeFromJson(record.properties.at("operand_type"), nodes);
        if ((operand == nullptr) == (operand_type == nullptr) ||
            (operand_type != nullptr &&
             SageInterface::containsUnknownType(operand_type))) {
          throw std::runtime_error(
              "AST JSON unary trait requires exactly one typed operand");
        }
        SgType *result_type = nullptr;
        if (input->kind != SgExpression::e_typeid)
          result_type = requireUnaryTraitResultType(
              typeFromJson(record.properties.at("type"), nodes), record.kind);
        SgExpression *trait = nullptr;
        switch (input->kind) {
        case SgExpression::e_sizeof:
        case SgExpression::e_sizeof_pack:
        case SgExpression::e_datasizeof: {
          const auto &source = input->source.value();
          auto *size_of = new SgSizeOfOp(operand, operand_type, result_type,
                                         input->kind, source.evaluated_value);
          size_of->set_type_elaboration_required(source.elaboration);
          size_of->set_source_type_elaboration_kind(source.kind);
          size_of->set_is_objectless_nonstatic_data_member_reference(
              input->objectless_member);
          if (type_defining_declaration != nullptr)
            size_of->set_type_defining_declaration(type_defining_declaration);
          trait = size_of;
          break;
        }
        case SgExpression::e_alignof:
        case SgExpression::e_preferred_alignof: {
          const auto &source = input->source.value();
          auto *align_of = new SgAlignOfOp(operand, operand_type, result_type,
                                           input->kind, source.evaluated_value);
          align_of->set_type_elaboration_required(source.elaboration);
          align_of->set_source_type_elaboration_kind(source.kind);
          if (type_defining_declaration != nullptr)
            align_of->set_type_defining_declaration(type_defining_declaration);
          trait = align_of;
          break;
        }
        case SgExpression::e_typeid:
          trait =
              typeIdFromProperties(record.properties, nodes, operand,
                                   operand_type, input->operand_is_evaluated);
          break;
        }
        if (operand != nullptr && input->kind != SgExpression::e_typeid)
          operand->set_parent(trait);
        if (type_defining_declaration != nullptr)
          type_defining_declaration->set_parent(trait);
        if (auto *size_of = isSgSizeOfOp(trait))
          (void)size_of->get_type();
        publish_delayed_node(record, std::move(positions), trait);
      } else if (const auto *input = std::get_if<NewInput>(&pending.input)) {
        auto missing = [&](uint64_t id) {
          return id != 0 && nodes.count(id) == 0;
        };
        if (missing(input->bound) || missing(input->placement) ||
            missing(input->constructor) || missing(input->builtin) ||
            missing(input->declaration))
          continue;
        auto child = [&](uint64_t id) -> SgExpression * {
          return id == 0 ? nullptr
                         : isSgExpression(require_unowned_delayed_child(
                               id, record, "new-expression operand"));
        };
        auto *placement = isSgExprListExp(child(input->placement));
        auto *constructor =
            isSgConstructorInitializer(child(input->constructor));
        auto *builtin = child(input->builtin);
        auto *bound = child(input->bound);
        if ((input->placement != 0 && placement == nullptr) ||
            (input->constructor != 0 && constructor == nullptr) ||
            (input->builtin != 0 && builtin == nullptr) ||
            (input->bound != 0 && bound == nullptr))
          throw std::runtime_error(
              "AST JSON new-expression has invalid typed operands");
        auto *declaration =
            input->declaration == 0
                ? nullptr
                : nodeByIdAs<SgFunctionDeclaration>(nodes, input->declaration);
        const auto fields =
            newConstructionFieldsFromJson(record.properties, nodes);
        SgExpressionPtrList elements;
        if (placement != nullptr)
          elements = placement->get_expressions();
        publish_delayed_node(record, std::move(positions),
                             constructNewExpression(fields, bound, placement,
                                                    constructor, builtin,
                                                    declaration, elements));
      } else if (const auto *input = std::get_if<DeleteInput>(&pending.input)) {
        if (nodes.count(input->operand) == 0 ||
            (input->declaration != 0 && nodes.count(input->declaration) == 0))
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, "delete operand"));
        if (operand == nullptr)
          throw std::runtime_error(
              "AST JSON delete operand is not an expression");
        auto *declaration =
            input->declaration == 0
                ? nullptr
                : nodeByIdAs<SgFunctionDeclaration>(nodes, input->declaration);
        auto *expression = new SgDeleteExp(operand, input->is_array,
                                           input->global, declaration);
        operand->set_parent(expression);
        publish_delayed_node(record, std::move(positions), expression);
      } else if (const auto *input = std::get_if<BinaryInput>(&pending.input)) {
        if (nodes.count(input->lhs) == 0 || nodes.count(input->rhs) == 0)
          continue;
        auto *lhs = isSgExpression(require_unowned_delayed_child(
            input->lhs, record, "binary left operand"));
        auto *rhs = isSgExpression(require_unowned_delayed_child(
            input->rhs, record, "binary right operand"));
        if (lhs == nullptr || rhs == nullptr)
          throw std::runtime_error(
              "AST JSON binary operands are not expressions");
        publish_delayed_node(
            record, std::move(positions),
            constructBinaryOp(record.kind, lhs, rhs,
                              typeFromJson(record.properties.at("type"), nodes),
                              input->arrow_role,
                              input->result_storage_size_bytes,
                              input->subscript_strides));
      } else if (const auto *input = std::get_if<UnaryInput>(&pending.input)) {
        if (input->operand != 0 && nodes.count(input->operand) == 0)
          continue;
        const auto fields = unaryConstructionFieldsFromJson(record.properties,
                                                            record.kind, nodes);
        SgExpression *operand =
            input->operand == 0 ? nullptr
                                : isSgExpression(require_unowned_delayed_child(
                                      input->operand, record, "unary operand"));
        if (input->operand != 0 && operand == nullptr)
          throw std::runtime_error(
              "AST JSON unary operand is not an expression");
        publish_delayed_node(
            record, std::move(positions),
            constructUnaryOp(
                record.kind, operand, fields,
                typeFromJson(record.properties.at("type"), nodes)));
      } else if (const auto *input = std::get_if<BoundsInput>(&pending.input)) {
        bool missing_child = false;
        for (uint64_t id : {input->lower, input->upper, input->stride})
          missing_child |= id != 0 && nodes.count(id) == 0;
        if (missing_child)
          continue;
        auto operand = [&](uint64_t id) -> SgExpression * {
          if (id == 0)
            return nullptr;
          auto *child = isSgExpression(
              require_unowned_delayed_child(id, record, "expression bound"));
          if (child == nullptr)
            throw std::runtime_error("AST JSON bound is not an expression");
          return child;
        };
        SgExpression *lower = operand(input->lower);
        SgExpression *upper = operand(input->upper);
        SgExpression *stride = operand(input->stride);
        if ((lower != nullptr && (lower == upper || lower == stride)) ||
            (upper != nullptr && upper == stride))
          throw std::runtime_error("AST JSON bounds are not distinct children");
        SgExpression *bounds = nullptr;
        switch (input->kind) {
        case V_SgRangeExp:
          bounds = new SgRangeExp(lower, upper, stride);
          break;
        case V_SgSubscriptExpression:
          bounds = new SgSubscriptExpression(lower, upper, stride);
          break;
        default:
          throw std::runtime_error(
              "AST JSON has an invalid bounded expression kind");
        }
        for (SgExpression *child : {lower, upper, stride})
          if (child != nullptr)
            child->set_parent(bounds);
        publish_delayed_node(record, std::move(positions), bounds);
      } else if (const auto *input =
                     std::get_if<AggregateInput>(&pending.input)) {
        if (nodes.count(input->list) == 0)
          continue;
        auto *list = isSgExprListExp(require_unowned_delayed_child(
            input->list, record, "aggregate initializer list"));
        if (list == nullptr)
          throw std::runtime_error(
              "AST JSON aggregate requires a typed initializer list");
        const auto fields = aggregateConstructionFieldsFromJson(
            record.properties, nodes, input->source_roles);
        publish_delayed_node(record, std::move(positions),
                             constructAggregateInitializer(
                                 fields, list, list->get_expressions()));
      } else if (const auto *input =
                     std::get_if<ConstructorInput>(&pending.input)) {
        if (nodes.count(input->list) == 0 ||
            (input->declaration != 0 && nodes.count(input->declaration) == 0))
          continue;
        auto *list = isSgExprListExp(require_unowned_delayed_child(
            input->list, record, "constructor argument list"));
        auto *declaration = input->declaration != 0
                                ? nodeByIdAs<SgMemberFunctionDeclaration>(
                                      nodes, input->declaration)
                                : nullptr;
        SgType *type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        publish_delayed_node(record, std::move(positions),
                             constructorInitializerFromProperties(
                                 record.properties, declaration, list,
                                 list->get_expressions(), type));
      } else if (const auto *input = std::get_if<CallInput>(&pending.input)) {
        auto missing = [&](uint64_t id) { return nodes.count(id) == 0; };
        if (missing(input->function) || missing(input->arguments) ||
            (input->lexical != 0 && missing(input->lexical)) ||
            std::any_of(input->tokens.begin(), input->tokens.end(), missing))
          continue;
        auto *function = isSgExpression(require_unowned_delayed_child(
            input->function, record, "call callee"));
        auto *arguments = isSgExprListExp(require_unowned_delayed_child(
            input->arguments, record, "call arguments"));
        auto *lexical =
            input->lexical
                ? isSgCxxLiteralTokenList(require_unowned_delayed_child(
                      input->lexical, record, "call literal operands"))
                : nullptr;
        if (function == nullptr || arguments == nullptr ||
            (input->lexical != 0 &&
             (lexical == nullptr || !lexical->get_tokens().empty())))
          throw std::runtime_error(
              "AST JSON call requires fresh typed operand containers");
        SgType *type = typeFromJson(
            record.properties.at("type"), nodes,
            "AST JSON call requires an exact non-callable result type");
        validateFunctionCallInputs(input->fields, function, arguments, type,
                                   lexical, arguments->get_expressions().size(),
                                   input->tokens.size());
        SgCxxLiteralTokenPtrList tokens;
        std::unordered_set<SgNode *> seen;
        auto require_child = [&](uint64_t id, uint64_t owner_id,
                                 SgNode *owner) {
          SgNode *child = nodeById(nodes, id);
          if (requiredSingleEdgeTarget(ast.node(id), "parent") != owner_id ||
              (child->get_parent() != nullptr &&
               child->get_parent() != owner) ||
              !seen.insert(child).second)
            throw std::runtime_error(
                "AST JSON call child has conflicting ownership");
          return child;
        };
        for (uint64_t id : input->tokens) {
          auto *token =
              isSgCxxLiteralToken(require_child(id, input->lexical, lexical));
          if (token == nullptr)
            throw std::runtime_error(
                "AST JSON call literal child is not a token");
          tokens.push_back(token);
        }
        if (lexical != nullptr) {
          lexical->get_tokens().swap(tokens);
          for (SgCxxLiteralToken *token : lexical->get_tokens())
            token->set_parent(lexical);
        }
        publish_delayed_node(record, std::move(positions),
                             constructFunctionCall(input->fields, function,
                                                   arguments, type, lexical));
      } else if (const auto *input = std::get_if<ChooseInput>(&pending.input)) {
        if (nodes.count(input->condition) == 0 ||
            nodes.count(input->true_expression) == 0 ||
            nodes.count(input->false_expression) == 0)
          continue;
        auto operand = [&](uint64_t id) {
          auto *expression = isSgExpression(require_unowned_delayed_child(
              id, record, "choose expression operand"));
          if (expression == nullptr)
            throw std::runtime_error(
                "AST JSON choose operand is not an expression");
          return expression;
        };
        SgType *type = typeFromJson(record.properties.at("type"), nodes);
        publish_delayed_node(
            record, std::move(positions),
            constructChooseExpression(type, input->selection,
                                      operand(input->condition),
                                      operand(input->true_expression),
                                      operand(input->false_expression)));
      } else if (const auto *input =
                     std::get_if<ConditionalInput>(&pending.input)) {
        if (nodes.count(input->condition) == 0 ||
            nodes.count(input->false_expression) == 0 ||
            (input->true_expression != 0 &&
             nodes.count(input->true_expression) == 0))
          continue;
        auto operand = [&](uint64_t id) -> SgExpression * {
          if (id == 0)
            return nullptr;
          auto *expression = isSgExpression(
              require_unowned_delayed_child(id, record, "conditional operand"));
          if (expression == nullptr)
            throw std::runtime_error(
                "AST JSON conditional operand is not an expression");
          return expression;
        };
        publish_delayed_node(
            record, std::move(positions),
            conditionalFromProperties(record.properties, nodes, input->kind,
                                      operand(input->condition),
                                      operand(input->true_expression),
                                      operand(input->false_expression)));
      } else if (const auto *input =
                     std::get_if<BuiltinInput>(&pending.input)) {
        if (std::any_of(input->arguments.begin(), input->arguments.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; }))
          continue;
        SgExpressionPtrList arguments;
        for (uint64_t id : input->arguments)
          arguments.push_back(isSgExpression(require_unowned_delayed_child(
              id, record, "type-trait builtin operand")));
        publish_delayed_node(record, std::move(positions),
                             typeTraitBuiltinFromProperties(record.properties,
                                                            nodes, arguments));
      } else if (const auto *input =
                     std::get_if<OperandInput>(&pending.input)) {
        if (nodes.count(input->operand) == 0)
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, record.kind + " operand"));
        if (record.kind == "SgNoexceptOp")
          publish_delayed_node(
              record, std::move(positions),
              noexceptFromProperties(record.properties, nodes, operand));
        else
          publish_delayed_node(
              record, std::move(positions),
              varArgFromProperties(record.properties, nodes, operand));
      } else if (const auto *input =
                     std::get_if<OmpMapPolicyInput>(&pending.input)) {
        if (input->expression != 0 && nodes.count(input->expression) == 0)
          continue;
        SgExpression *expression = nullptr;
        if (input->expression != 0) {
          expression = isSgExpression(require_unowned_delayed_child(
              input->expression, record, "map policy expression"));
          if (expression == nullptr)
            throw std::runtime_error(
                "AST JSON map policy operand is not an expression");
        }
        const auto &policy = construction_fields.at(record.id).dist_data_policy;
        ROSE_ASSERT(policy.has_value());
        auto *node = new SgOmpMapDistDataPolicy(*policy, expression);
        if (expression != nullptr)
          expression->set_parent(node);
        publish_delayed_node(record, std::move(positions), node);
      } else if (const auto *input =
                     std::get_if<OmpItemInput>(&pending.input)) {
        if (nodes.count(input->operand) == 0 ||
            !all_constructed(input->policies))
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, record.kind + " expression"));
        if (operand == nullptr)
          throw std::runtime_error(
              "AST JSON OpenMP item operand is not an expression");
        SgExpression *item;
        const auto &fields = construction_fields.at(record.id).induction_item;
        ROSE_ASSERT(fields.has_value() ==
                    (record.kind == "SgOmpInductionItem"));
        if (fields)
          item = new SgOmpInductionItem(fields->kind, fields->label, operand);
        else {
          SgOmpMapDistDataPolicyPtrList policies;
          for (uint64_t id : input->policies) {
            auto *policy = isSgOmpMapDistDataPolicy(
                require_unowned_delayed_child(id, record, "map policy"));
            if (policy == nullptr)
              throw std::runtime_error(
                  "AST JSON map item child is not a policy");
            policies.push_back(policy);
          }
          auto *map = new SgOmpMapItem(operand);
          map->get_policies() = std::move(policies);
          for (auto *policy : map->get_policies())
            policy->set_parent(map);
          item = map;
        }
        operand->set_parent(item);
        publish_delayed_node(record, std::move(positions), item);
      } else if (const auto *input =
                     std::get_if<OmpListClauseInput>(&pending.input)) {
        if (!all_constructed(input->children))
          continue;
        SgOmpClause *clause;
        if (input->apply_label) {
          SgOmpApplyTransformationPtrList children;
          for (uint64_t id : input->children) {
            auto *child =
                isSgOmpApplyTransformation(require_unowned_delayed_child(
                    id, record, "apply transformation"));
            if (child == nullptr)
              throw std::runtime_error(
                  "AST JSON apply child is not a transformation");
            children.push_back(child);
          }
          auto *apply = new SgOmpApplyClause(*input->apply_label);
          apply->get_transformations() = std::move(children);
          clause = apply;
        } else {
          SgOmpInductionItemPtrList children;
          for (uint64_t id : input->children) {
            auto *child = isSgOmpInductionItem(
                require_unowned_delayed_child(id, record, "induction item"));
            if (child == nullptr)
              throw std::runtime_error(
                  "AST JSON induction child is not an item");
            children.push_back(child);
          }
          auto *induction = new SgOmpInductionClause();
          induction->get_items() = std::move(children);
          clause = induction;
        }
        for (uint64_t id : input->children)
          nodeById(nodes, id)->set_parent(clause);
        publish_delayed_node(record, std::move(positions), clause);
      } else if (const auto *input =
                     std::get_if<OmpApplyInput>(&pending.input)) {
        if ((input->argument != 0 && nodes.count(input->argument) == 0) ||
            (input->nested != 0 && nodes.count(input->nested) == 0))
          continue;
        SgExpression *argument = nullptr;
        if (input->argument != 0) {
          argument = isSgExpression(require_unowned_delayed_child(
              input->argument, record, "OpenMP apply argument"));
          if (argument == nullptr)
            throw std::runtime_error(
                "AST JSON OpenMP apply argument is not an expression");
        }
        SgOmpApplyClause *nested = nullptr;
        if (input->nested != 0) {
          nested = isSgOmpApplyClause(require_unowned_delayed_child(
              input->nested, record, "OpenMP nested apply"));
          if (nested == nullptr)
            throw std::runtime_error(
                "AST JSON OpenMP nested apply has the wrong node kind");
        }
        const auto &fields =
            construction_fields.at(record.id).apply_transformation;
        ROSE_ASSERT(fields.has_value());
        auto *item = new SgOmpApplyTransformation(
            fields->kind, fields->separator, fields->name, argument, nested);
        if (argument != nullptr)
          argument->set_parent(item);
        if (nested != nullptr)
          nested->set_parent(item);
        publish_delayed_node(record, std::move(positions), item);
      } else if (const auto *input = std::get_if<PackInput>(&pending.input)) {
        if (nodes.count(input->pattern) == 0)
          continue;
        auto *pattern = isSgExpression(require_unowned_delayed_child(
            input->pattern, record, "SgPackExpansionExpr pattern"));
        if (pattern == nullptr) {
          throw std::runtime_error(
              "AST JSON SgPackExpansionExpr pattern is not an expression");
        }
        SgType *type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        auto *pack = new SgPackExpansionExpr(pattern, type);
        publish_delayed_node(record, std::move(positions), pack);
      } else if (const auto *input = std::get_if<AwaitInput>(&pending.input)) {
        if (nodes.count(input->operand) == 0)
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, "SgAwaitExpression operand"));
        if (operand == nullptr) {
          throw std::runtime_error(
              "AST JSON SgAwaitExpression operand is not an expression");
        }
        SgType *type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        auto *await_expression =
            new SgAwaitExpression(operand, type, input->keyword);
        publish_delayed_node(record, std::move(positions), await_expression);
      } else if (const auto *input = std::get_if<FoldInput>(&pending.input)) {
        if (nodes.count(input->operands) == 0)
          continue;
        auto *operands = isSgExpression(require_unowned_delayed_child(
            input->operands, record, "SgFoldExpression operands"));
        if (operands == nullptr) {
          throw std::runtime_error(
              "AST JSON SgFoldExpression operands are not an expression");
        }
        SgType *type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        auto *fold =
            new SgFoldExpression(operands, input->fields.operator_kind,
                                 input->fields.is_left_associative, type);
        publish_delayed_node(record, std::move(positions), fold);
      } else if (const auto *input = std::get_if<CastInput>(&pending.input)) {
        if (nodes.find(input->operand) == nodes.end() ||
            (input->type_defining_declaration != 0 &&
             nodes.find(input->type_defining_declaration) == nodes.end()))
          continue;
        auto *operand = isSgExpression(require_unowned_delayed_child(
            input->operand, record, "SgCastExp operand"));
        if (operand == nullptr) {
          throw std::runtime_error(
              "AST JSON SgCastExp operand is not an expression");
        }
        SgDeclarationStatement *type_defining_declaration = nullptr;
        if (input->type_defining_declaration != 0) {
          type_defining_declaration =
              isSgDeclarationStatement(require_unowned_delayed_child(
                  input->type_defining_declaration, record,
                  "SgCastExp type-defining declaration"));
          if (type_defining_declaration == nullptr)
            throw std::runtime_error("AST JSON SgCastExp type-defining "
                                     "declaration is not a declaration");
        }
        SgType *type =
            typeFromJson(record.properties.at("type"), nodes,
                         "AST JSON " + record.kind +
                             " requires an exact stored destination type");
        auto *cast = castExpressionFromProperties(record.properties, nodes,
                                                  operand, type, input->fields);
        if (type_defining_declaration != nullptr) {
          cast->set_type_defining_declaration(type_defining_declaration);
          type_defining_declaration->set_parent(cast);
        }
        operand->set_parent(cast);
        publish_delayed_node(record, std::move(positions), cast);
      } else if (const auto *input =
                     std::get_if<ComplexInput>(&pending.input)) {
        if (std::any_of(input->components.begin(), input->components.end(),
                        [&](uint64_t id) { return nodes.count(id) == 0; })) {
          continue;
        }
        for (uint64_t id : input->components) {
          if (isSgExpression(require_unowned_delayed_child(
                  id, record, "SgComplexVal component")) == nullptr) {
            throw std::runtime_error(
                "AST JSON SgComplexVal component is not an expression");
          }
        }
        publish_delayed_node(
            record, std::move(positions),
            complexValueFromProperties(record.properties, nodes));
      } else if (const auto *input =
                     std::get_if<DesignatorInput>(&pending.input)) {
        if (nodes.count(input->first) == 0 ||
            (input->second != 0 && nodes.count(input->second) == 0) ||
            nodes.count(input->list) == 0)
          continue;
        auto *list = nodeByIdAs<SgExprListExp>(nodes, input->list);
        auto *first = isSgExpression(require_unowned_delayed_child(
            input->first, record, "designator first expression"));
        auto *second =
            input->second != 0
                ? isSgExpression(require_unowned_delayed_child(
                      input->second, record, "designator second expression"))
                : nullptr;
        if (first == nullptr || (input->second != 0 && second == nullptr) ||
            (input->kind == SgDesignator::e_designator_field &&
             isSgVarRefExp(first) == nullptr))
          throw std::runtime_error(
              "AST JSON designator has invalid typed expressions");
        auto *designator = new SgDesignator(input->kind, first, second);
        first->set_parent(designator);
        if (second != nullptr)
          second->set_parent(designator);
        designator->set_parent(list);
        designator->validate_designator();
        publish_delayed_node(record, std::move(positions), designator);
      } else {
        const auto &designated_input = std::get<DesignatedInput>(pending.input);
        if (nodes.find(designated_input.designators) == nodes.end() ||
            nodes.find(designated_input.member) == nodes.end()) {
          continue;
        }
        SgExprListExp *designators =
            isSgExprListExp(require_unowned_delayed_child(
                designated_input.designators, record,
                "SgDesignatedInitializer designatorList"));
        SgInitializer *member = isSgInitializer(require_unowned_delayed_child(
            designated_input.member, record,
            "SgDesignatedInitializer memberInit"));
        if (designators == nullptr || member == nullptr) {
          throw std::runtime_error(
              "AST JSON SgDesignatedInitializer has invalid typed children");
        }
        if (designators->get_expressions().empty())
          throw std::runtime_error(
              "AST JSON SgDesignatedInitializer has an empty designator list");
        auto *initializer = new SgDesignatedInitializer(designators, member);
        initializer->set_is_braced_initialized(designated_input.braced);
        if (designators->get_parent() != initializer ||
            member->get_parent() != initializer) {
          throw std::runtime_error("AST JSON SgDesignatedInitializer did not "
                                   "claim its exact children");
        }
        publish_delayed_node(record, std::move(positions), initializer);
      }
      pending.completed = true;
      --remaining;
      made_progress = true;
    }
    if (!made_progress) {
      std::string pending_node;
      for (const ExpressionInput &pending : expressions) {
        if (pending.completesExistingNode()
                ? !pending.completed
                : nodes.count(pending.record->id) == 0) {
          pending_node = std::to_string(pending.record->id) + " (" +
                         pending.record->kind + ")";
          break;
        }
      }
      throw std::runtime_error("AST JSON expression construction dependencies "
                               "are missing or cyclic for node " +
                               pending_node);
    }
  }
}

static void constructConstructorOnlyNodes(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    NodeMap &nodes, RecordConstructionFieldMap &construction_fields,
    std::vector<const NodeRecord *> *delayed_declarations = nullptr,
    const SymbolConstructionAdvance &advance_symbols = {}) {
  std::vector<const NodeRecord *> records;
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgVariableDefinition")
      continue;
    if (nodes.count(record.id) == 0 ||
        record.kind == "SgTemplateInstantiationDefn" ||
        isSgVariableDeclaration(nodes.at(record.id)) != nullptr ||
        record.kind == "SgFunctionParameterList" ||
        record.kind == "SgExprListExp" || record.kind == "SgOmpClauseList" ||
        record.kind == "SgInitializedName" ||
        isFunctionDeclarationRecordKind(record.kind) ||
        !edgesFor(record, "alignment_specifiers").empty() ||
        singleEdgeTarget(record, "cuda_launch_bounds_expression") != 0) {
      records.push_back(&record);
    }
  }
  auto node_ids = mappedNodePointers(nodes);
  constructRecordedNodes(ast, bindings, nodes, records, node_ids,
                         delayed_declarations, advance_symbols, nullptr,
                         &construction_fields);
}

static void validateSourceFileCommands(const AstFileRecord &ast) {
  for (const NodeRecord &record : ast.nodes)
    if (record.kind == "SgSourceFile")
      (void)commandLineFromJson(record.properties);
}

void rebuildConstructorOnlyNodes(const AstFileRecord &ast, NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  auto construction_fields = validateConstructorOnlyRecords(ast, nodes);
  validateSourceFileCommands(ast);
  const auto bindings = indexSymbolBindings(ast);
  for (const NodeRecord &record : ast.nodes)
    if (nodes.count(record.id) == 0 &&
        construction_fields.count(record.id) == 0) {
      auto inserted = construction_fields.emplace(
          record.id,
          RecordConstructionFields{validateConstructionFlags(record), {}});
      prepareRecordPublicationFields(record, inserted.first->second);
    }
  for (auto &[id, fields] : construction_fields)
    fields.positions = validateSourcePositionRecord(ast.node(id));
  SgNode::SourceStateConstruction source_construction;
  constructConstructorOnlyNodes(ast, bindings, nodes, construction_fields);
}

static void constructVariableDefinitions(const AstFileRecord &ast,
                                         NodeMap &nodes,
                                         RecordConstructionFieldMap &prepared) {
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgVariableDefinition")
      continue;
    if (nodes.count(record.id) != 0)
      throw std::runtime_error("AST JSON duplicate variable definition");

    const uint64_t name_id = requiredSingleEdgeTarget(record, "vardefn");
    const NodeRecord &name_record = ast.node(name_id);
    if (name_record.kind != "SgInitializedName" ||
        requiredSingleEdgeTarget(record, "parent") != name_id ||
        requiredSingleEdgeTarget(name_record, "variable_definition") !=
            record.id) {
      throw std::runtime_error(
          "AST JSON variable definition has no exact initialized-name owner");
    }
    SgInitializedName *name = nodeByIdAs<SgInitializedName>(nodes, name_id);
    if (name->get_definition() != nullptr)
      throw std::runtime_error(
          "AST JSON initialized name already owns a variable definition");

    const uint64_t bitfield_id = singleEdgeTarget(record, "bitfield");
    SgExpression *bitfield = bitfield_id != 0
                                 ? nodeByIdAs<SgExpression>(nodes, bitfield_id)
                                 : nullptr;
    if (bitfield != nullptr &&
        (requiredSingleEdgeTarget(ast.node(bitfield_id), "parent") !=
             record.id ||
         bitfield->get_parent() != nullptr)) {
      throw std::runtime_error(
          "AST JSON variable definition has no unowned exact bitfield");
    }
    const bool zero_width =
        record.properties.requiredBool("is_zero_width_bitfield");
    const bool no_unique_address =
        record.properties.requiredBool("has_no_unique_address");
    if (zero_width && bitfield == nullptr)
      throw std::runtime_error(
          "AST JSON zero-width bitfield has no bitfield expression");

    auto &fields = prepared.at(record.id);
    ROSE_ASSERT(fields.statement.has_value() && fields.declaration.has_value());
    auto positions = constructSourcePositions(fields.positions.value());
    SgNode::SourceStateConstruction construction;
    std::unique_ptr<SgVariableDefinition> definition(new SgVariableDefinition(
        static_cast<Sg_File_Info *>(nullptr), name, bitfield));
    try {
      fields.declaration->initialize(definition.get());
      definition->set_is_zero_width_bitfield(zero_width);
      definition->set_has_no_unique_address(no_unique_address);
      fields.statement->initialize(definition.get());
      installSourcePositions(definition.get(), std::move(positions),
                             record.kind);
      fields.flags.initialize(definition.get());
      definition->set_parent(name);
      name->set_definition(definition.get());
      nodes.emplace(record.id, definition.get());
    } catch (...) {
      if (name->get_definition() == definition.get())
        name->set_definition(nullptr);
      if (bitfield != nullptr && bitfield->get_parent() == definition.get())
        bitfield->set_parent(nullptr);
      throw;
    }
    definition.release();
  }
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

static void
constructBaseClassRecord(const NodeRecord &record, NodeMap &nodes,
                         RecordConstructionFields &fields,
                         ArrayConstruction *construction = nullptr) {
  const bool plain = record.kind == "SgBaseClass";
  const bool expression = record.kind == "SgExpBaseClass";
  const bool nonreal = record.kind == "SgNonrealBaseClass";
  ROSE_ASSERT(plain || expression || nonreal);

  const JsonValue &p = record.properties;
  const uint64_t class_id =
      exactSerializedPointerTarget(record, "base_class", "base_class");
  const uint64_t nonreal_id =
      nonreal ? exactSerializedPointerTarget(record, "base_class_nonreal",
                                             "base_class_nonreal")
              : 0;
  if ((plain && class_id == 0) || (!plain && class_id != 0) ||
      (nonreal && nonreal_id == 0)) {
    throw std::runtime_error(
        "AST JSON base class has no exact semantic declaration");
  }
  const bool direct = p.requiredBool("is_direct_base_class");
  const bool pack = p.requiredBool("pack_expansion");
  const auto modifier =
      requiredEnum<SgBaseClassModifier::baseclass_modifier_enum>(
          p, "base_class_modifier", "SgBaseClass",
          {SgBaseClassModifier::e_default, SgBaseClassModifier::e_virtual});
  const auto access = requiredBaseClassAccessModifier(
      p, "base_class_access_modifier", "SgBaseClass");
  const bool explicit_access = p.requiredBool("base_class_access_is_explicit");
  const int64_t qualification_length =
      p.requiredInt("name_qualification_length");
  if (qualification_length < 0 ||
      qualification_length > std::numeric_limits<int>::max()) {
    throw std::runtime_error(
        "AST JSON base class has an invalid qualification length");
  }
  const bool type_elaboration = p.requiredBool("type_elaboration_required");
  const bool global_qualification =
      p.requiredBool("global_qualification_required");
  const bool source_present =
      p.requiredBool("source_type_qualification_present");
  const bool source_global = p.requiredBool("source_type_global_qualification");
  const SgStringList source_tokens =
      stringListFromJson(p.at("source_type_qualification_tokens"),
                         "source_type_qualification_tokens");
  const bool owns_terminal =
      p.requiredBool("source_type_qualification_owns_terminal_name");
  const bool owns_arguments =
      p.requiredBool("source_type_qualification_owns_template_arguments");
  if (!source_present && (source_global || !source_tokens.empty())) {
    throw std::runtime_error(
        "AST JSON SgBaseClass has source qualifier components without "
        "source qualification provenance");
  }
  if ((owns_terminal && (!source_present || source_tokens.empty())) ||
      (owns_arguments && !owns_terminal)) {
    throw std::runtime_error(
        "AST JSON SgBaseClass has inconsistent source qualifier terminal "
        "ownership");
  }

  auto positions = constructSourcePositions(fields.positions.value());
  SgType *source_type = nullableTypeFromJson(p.at("source_type"), nodes);
  if (source_type == nullptr) {
    throw std::runtime_error("AST JSON SgBaseClass has no exact source_type");
  }
  SgSymbol *source_symbol = exactBoundSymbolFromJson(
      p.at("fortran_source_derived_type_symbol"), nodes);
  SgClassDeclaration *declaration =
      plain ? nodeByIdAs<SgClassDeclaration>(nodes, class_id) : nullptr;
  if ((source_symbol != nullptr ||
       isSgDerivedTypeStatement(declaration) != nullptr) &&
      (declaration == nullptr || source_type != declaration->get_type() ||
       !exactFortranSourceDerivedTypeBinding(source_symbol,
                                             declaration->get_type())))
    throw std::runtime_error(
        "AST JSON Fortran base binding disagrees with its semantic type");
  if (expression) {
    SgDeclType *decl_type = isSgDeclType(source_type);
    if (decl_type == nullptr || decl_type->get_base_expression() == nullptr) {
      throw std::runtime_error(
          "AST JSON SgExpBaseClass requires an exact decltype source "
          "without a class declaration");
    }
  }

  const auto mapped_nodes = mappedNodePointers(nodes);
  RecordNodeConstruction ordinary_ownership(mapped_nodes);
  auto &ownership =
      construction != nullptr ? construction->ownership : ordinary_ownership;
  SgBaseClass *base = nullptr;
  if (plain) {
    base = ownership.make<SgBaseClass>(declaration, source_type, direct);
  } else if (expression) {
    base = ownership.make<SgExpBaseClass>(nullptr, source_type, direct);
  } else {
    base = ownership.make<SgNonrealBaseClass>(
        nullptr, source_type, direct,
        nodeByIdAs<SgNonrealDecl>(nodes, nonreal_id));
  }
  ROSE_ASSERT(base->get_baseClassModifier() != nullptr);
  base->set_fortran_source_derived_type_symbol(source_symbol);
  base->get_baseClassModifier()->set_modifier(modifier);
  base->get_baseClassModifier()->get_accessModifier().set_modifier(access);
  base->get_baseClassModifier()->get_accessModifier().set_is_explicit(
      explicit_access);
  base->set_pack_expansion(pack);
  base->set_name_qualification_length(static_cast<int>(qualification_length));
  base->set_type_elaboration_required(type_elaboration);
  base->set_global_qualification_required(global_qualification);
  base->set_source_type_global_qualification(source_global);
  base->get_source_type_qualification_tokens() = source_tokens;
  base->set_source_type_qualification_present(source_present);
  base->set_source_type_qualification_owns_terminal_name(owns_terminal);
  base->set_source_type_qualification_owns_template_arguments(owns_arguments);
  requireRestoredKind(base, record);
  publishConstructorOnlyNode(record, base, nodes, std::move(positions), fields);
  if (construction != nullptr) {
    construction->retain(record.id, base);
    construction->node_ids.emplace(base, record.id);
  } else
    ownership.publish();
}

void restoreDeclarationIdentityEdges(
    const AstFileRecord &ast, const NodeMap &nodes, SgProject *project,
    std::unordered_set<uint64_t> &restored_declarations,
    bool require_complete) {
  auto external_peer_dependencies_are_ready = [&](const JsonValue &reference) {
    const JsonValue *function = reference.find("external_function");
    if (function == nullptr)
      return true;
    std::unordered_set<uint64_t> ids, symbols, types;
    collectExternalFunctionConstructionDependencies(*function, ids);
    for (const char *field :
         {"function_type", "function_type_syntax",
          "declared_exception_signature", "parameters", "alignment_specifiers",
          "cuda_launch_bounds_expression", "explicit_specifier_expression"})
      collectTypeDependencyIds(function->at(field), ids, symbols, types);
    if (const auto *syntax = function->find("syntax_parameters"))
      collectTypeDependencyIds(*syntax, ids, symbols, types);
    const auto &scope = function->at("function_parameter_scope");
    if (scope.requiredBool("present")) {
      collectTypeDependencyIds(scope.at("declarations"), ids, symbols, types);
    }
    for (uint64_t id : ids)
      if (!declarationConstructionDependencyReady(
              ast, nodes, id, symbols.count(id) != 0, types.count(id) != 0))
        return false;
    return true;
  };
  auto external_peer_context = [&](SgDeclarationStatement *owner,
                                   const JsonValue &json,
                                   const std::string &context) {
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
    return std::pair<SgScopeStatement *, SgNode *>(scope, parent);
  };
  auto restore_external_peer_context = [&](SgDeclarationStatement *owner,
                                           SgDeclarationStatement *peer,
                                           SgScopeStatement *scope,
                                           SgNode *parent,
                                           const std::string &context) {
    if (owner == nullptr || peer == nullptr) {
      throw std::runtime_error("AST JSON " + context +
                               " reconstructed a null declaration peer");
    }
    bool same_identity = owner->variantT() == peer->variantT();
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(owner)) {
      SgFunctionDeclaration *other = isSgFunctionDeclaration(peer);
      same_identity =
          same_identity && other != nullptr &&
          function->get_name() == other->get_name() &&
          function->get_symbol_table_key() == other->get_symbol_table_key() &&
          function->get_type() != nullptr && other->get_type() != nullptr &&
          (SageInterface::isEquivalentFunctionType(function->get_type(),
                                                   other->get_type()) ||
           SageInterface::isFunctionPrototypeRefinement(function->get_type(),
                                                        other->get_type()) ||
           SageInterface::isFunctionPrototypeRefinement(other->get_type(),
                                                        function->get_type()));
    } else if (SgClassDeclaration *declaration = isSgClassDeclaration(owner)) {
      SgClassDeclaration *other = isSgClassDeclaration(peer);
      same_identity = same_identity && other != nullptr &&
                      declaration->get_name() == other->get_name() &&
                      declaration->get_class_type() == other->get_class_type();
    } else {
      same_identity = false;
    }
    if (!same_identity) {
      throw std::runtime_error("AST JSON " + context +
                               " is not the same declaration identity");
    }
    if (isSgFunctionDeclaration(peer) != nullptr ||
        isSgProject(peer->get_parent()) != nullptr ||
        SageInterface::getEnclosingSourceFile(peer, true) != nullptr) {
      if (peer->get_scope() != scope || peer->get_parent() != parent) {
        throw std::runtime_error("AST JSON " + context +
                                 " would move an existing declaration peer");
      }
    } else {
      peer->set_scope(scope);
      peer->set_parent(parent);
    }

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
    for (const char *field :
         {"scope", "firstNondefiningDeclaration", "definingDeclaration"}) {
      dependencies_available &=
          dependency_is_available(singleEdgeTarget(record, field));
    }
    for (const char *field : {"external_first_nondefining_declaration",
                              "external_defining_declaration"}) {
      const JsonValue *external_json = record.properties.find(field);
      if (external_json == nullptr) {
        continue;
      }
      if (!external_json->requiredBool("present")) {
        if (external_json->object.size() != 1) {
          throw std::runtime_error("AST JSON " + record.kind + "." + field +
                                   " absent external declaration has payload");
        }
        continue;
      }
      if (auto *function = isSgFunctionDeclaration(decl))
        dependencies_available &= function->get_type() != nullptr;
      dependencies_available &=
          external_peer_dependencies_are_ready(*external_json);
      if (external_json->find("external_function") != nullptr)
        continue;
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

    if (uint64_t target = singleEdgeTarget(record, "scope")) {
      SgScopeStatement *scope = nodeByIdAs<SgScopeStatement>(nodes, target);
      if (isSgFunctionDeclaration(decl) != nullptr ||
          isSgEnumDeclaration(decl) != nullptr) {
        if (decl->get_scope() != scope)
          throw std::runtime_error(
              "AST JSON declaration has no constructed semantic scope");
      } else if (decl->hasExplicitScope()) {
        decl->set_scope(scope);
      }
    }
    if (isSgFunctionParameterList(decl) != nullptr) {
      if (decl->get_firstNondefiningDeclaration() != decl ||
          decl->get_definingDeclaration() != nullptr)
        throw std::runtime_error(
            "AST JSON function parameter list lost its constructor-owned "
            "declaration identity");
    } else if (uint64_t target =
                   singleEdgeTarget(record, "firstNondefiningDeclaration")) {
      SgDeclarationStatement *first =
          nodeByIdAs<SgDeclarationStatement>(nodes, target);
      if (isSgEnumDeclaration(decl) != nullptr) {
        if (decl->get_firstNondefiningDeclaration() != first)
          throw std::runtime_error(
              "AST JSON enum declaration lost its constructed family");
      } else if (isSgFunctionDeclaration(decl) != nullptr) {
        if (decl->get_firstNondefiningDeclaration() != first)
          throw std::runtime_error(
              "AST JSON function declaration lost its constructed family");
      } else {
        decl->set_firstNondefiningDeclaration(first);
      }
    } else if (const JsonValue *external_json = record.properties.find(
                   "external_first_nondefining_declaration");
               external_json != nullptr &&
               external_json->requiredBool("present")) {
      const std::string context = record.kind + ".firstNondefiningDeclaration";
      auto [scope, parent] =
          external_peer_context(decl, *external_json, context);
      SgDeclarationStatement *external =
          externalDeclarationReferenceFromJson(*external_json, nodes, context);
      restore_external_peer_context(decl, external, scope, parent, context);
      decl->set_firstNondefiningDeclaration(external);
    } else {
      // Declaration constructors commonly install self as a provisional
      // canonical identity.  An absent serialized edge is an exact null
      // identity, not permission to retain that constructor default.
      decl->set_firstNondefiningDeclaration(nullptr);
    }
    if (isSgFunctionParameterList(decl) != nullptr) {
      ROSE_ASSERT(decl->get_definingDeclaration() == nullptr);
    } else if (uint64_t target =
                   singleEdgeTarget(record, "definingDeclaration")) {
      decl->set_definingDeclaration(
          nodeByIdAs<SgDeclarationStatement>(nodes, target));
    } else if (const JsonValue *external_json =
                   record.properties.find("external_defining_declaration");
               external_json != nullptr &&
               external_json->requiredBool("present")) {
      const std::string context = record.kind + ".definingDeclaration";
      auto [scope, parent] =
          external_peer_context(decl, *external_json, context);
      SgDeclarationStatement *external =
          externalDeclarationReferenceFromJson(*external_json, nodes, context);
      restore_external_peer_context(decl, external, scope, parent, context);
      decl->set_definingDeclaration(external);
    } else {
      decl->set_definingDeclaration(nullptr);
    }
    if (auto *function = isSgFunctionDeclaration(decl);
        function != nullptr &&
        record.properties.find("external_function_parameter_scope_owner")) {
      const auto owner = requiredEnum<ExternalFunctionParameterScopeOwner>(
          record.properties, "external_function_parameter_scope_owner",
          record.kind,
          {ExternalFunctionParameterScopeOwner::FirstNondefining,
           ExternalFunctionParameterScopeOwner::Defining});
      auto *peer = isSgFunctionDeclaration(
          owner == ExternalFunctionParameterScopeOwner::FirstNondefining
              ? function->get_firstNondefiningDeclaration()
              : function->get_definingDeclaration());
      if (peer == nullptr || peer->get_functionParameterScope() == nullptr ||
          peer->get_functionParameterScope()->get_parent() != peer)
        throw std::runtime_error(
            "AST JSON function has no exact external parameter-scope owner");
      function->set_functionParameterScope(peer->get_functionParameterScope());
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
      SgNode *node = nodeById(nodes, record.id);
      SgNode *parent = nodeById(nodes, target);
      SgFunctionDefinition *function_owner = isSgFunctionDefinition(parent);
      SgBasicBlock *body = isSgBasicBlock(node);
      if (function_owner != nullptr && body != nullptr &&
          function_owner->get_body() != body)
        throw std::runtime_error(
            "AST JSON function body disagrees with its constructor owner");
      if (node->get_parent() != parent)
        throw std::runtime_error(
            "AST JSON node lost its recorded parent after edge linking");
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
      const SgNodePtrList sourceChildren =
          owner->get_traversalSuccessorContainer();
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
        if (std::find(sourceChildren.begin(), sourceChildren.end(),
                      declaration) != sourceChildren.end()) {
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

void validateFortranAggregateConstruction(
    const AggregateConstructionFields &fields) {
  const bool is_array_constructor =
      fields.source_form ==
      SgAggregateInitializer::e_aggregate_initializer_source_fortran;
  const bool is_structure_constructor =
      fields.source_form ==
      SgAggregateInitializer::e_aggregate_initializer_source_fortran_structure;
  if (SageInterface::containsUnknownType(fields.type) ||
      fields.has_explicit_type != (fields.explicit_type != nullptr) ||
      ((!is_array_constructor && !is_structure_constructor) &&
       (fields.has_explicit_type || fields.source_symbol != nullptr))) {
    throw std::runtime_error(
        "AST JSON aggregate has contradictory Fortran source type-spec state");
  }
  if (fields.has_explicit_type) {
    if (SageInterface::containsUnknownType(fields.explicit_type))
      throw std::runtime_error(
          "AST JSON Fortran constructor has no exact explicit source type");
    const FortranTypeContractSurface source_surface =
        fortranTypeContractSurface(fields.explicit_type);
    const FortranTypeContractSurface semantic_surface =
        fortranTypeContractSurface(fields.type);
    const bool rank_contract =
        is_array_constructor
            ? (source_surface.ordinary_rank == 0 &&
               semantic_surface.ordinary_rank != 0)
            : (is_structure_constructor && source_surface.ordinary_rank == 0 &&
               semantic_surface.ordinary_rank == 0);
    if (source_surface.base == nullptr || semantic_surface.base == nullptr ||
        !rank_contract ||
        !SageInterface::fortranSourceTypeMatchesSemanticType(
            source_surface.base, semantic_surface.base)) {
      throw std::runtime_error(
          "AST JSON Fortran constructor has a contradictory explicit "
          "source/semantic type contract");
    }
    if (!exactFortranSourceDerivedTypeBinding(
            fields.source_symbol, isSgClassType(source_surface.base))) {
      throw std::runtime_error(
          "AST JSON Fortran constructor has a contradictory exact "
          "source derived-type binding");
    }
    if (is_structure_constructor && fields.explicit_type != fields.type) {
      throw std::runtime_error(
          "AST JSON Fortran structure constructor split its exact "
          "source and semantic derived type identity");
    }
  } else if (fields.source_symbol != nullptr) {
    throw std::runtime_error(
        "AST JSON Fortran constructor without an explicit type "
        "owns a source derived-type binding");
  } else if (is_structure_constructor) {
    throw std::runtime_error(
        "AST JSON Fortran structure constructor has no explicit source "
        "type identity");
  }
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
  auto *declaration = isSgVariableDeclaration(name->get_parent());
  auto *auxiliary =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  auto *scope = name->get_scope();
  const bool implicitDeclaration =
      name->get_fortran_source_type() == nullptr && declaration != nullptr &&
      declaration->get_fortran_declaration_origin() ==
          SgVariableDeclaration::e_fortran_semantic_only_declaration &&
      scope != nullptr && declaration->get_scope() == scope &&
      auxiliary != nullptr && auxiliary->get_parent() == scope &&
      scope->get_auxiliary_declarations() == auxiliary &&
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) == 1 &&
      std::count(declaration->get_variables().begin(),
                 declaration->get_variables().end(), name) == 1 &&
      name->get_fortran_source_derived_type_symbol() == nullptr;
  if (pointer == nullptr ||
      pointer->get_attribute_kind() !=
          SgAttributeSpecificationStatement::e_pointerStatement ||
      pointer->get_scope() == nullptr || pointer->get_scope() != scope ||
      pointer->get_parent() != scope ||
      !scope->statementExistsInScope(pointer) || parameters == nullptr ||
      parameters->get_parent() != pointer ||
      parameters->get_expressions().empty() ||
      (source.base == nullptr && !implicitDeclaration) ||
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
  FortranTypeContractSurface source_surface =
      fortranTypeContractSurface(source);
  FortranTypeContractSurface semantic_surface =
      fortranTypeContractSurface(semantic);
  if (auto *interface =
          isSgTypeExpression(name->get_fortran_procedure_interface())) {
    auto *procedure = isSgFunctionType(semantic_surface.base);
    if (procedure == nullptr ||
        !SageInterface::fortranSourceTypeMatchesSemanticType(
            source_surface.base, semantic_surface.base))
      return false;
    source_surface.base =
        fortranTypeContractSurface(interface->get_represented_type()).base;
    semantic_surface.base =
        fortranTypeContractSurface(procedure->get_return_type()).base;
  } else if (auto *procedure = isSgFunctionType(semantic_surface.base);
             procedure != nullptr &&
             isSgFunctionType(source_surface.base) == nullptr) {
    auto *declaration = isSgVariableDeclaration(name->get_parent());
    if (name->get_fortran_procedure_interface() != nullptr ||
        declaration == nullptr ||
        !declaration->get_declarationModifier()
             .get_storageModifier()
             .isExtern())
      return false;
    semantic_surface.base =
        fortranTypeContractSurface(procedure->get_return_type()).base;
  }
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
  for (const NodeRecord &record : ast.nodes) {
    SgSourceFile *file = nullptr;
    for (SgNode *owner = nodeById(nodes, record.id); owner != nullptr;
         owner = owner->get_parent()) {
      if ((file = isSgSourceFile(owner)) != nullptr)
        break;
    }
    if (file == nullptr ||
        file->get_inputLanguage() != SgFile::e_Fortran_language)
      continue;
    if (SgInitializedName *parameter =
            isSgInitializedName(nodeById(nodes, record.id));
        parameter != nullptr &&
        isSgFunctionParameterList(parameter->get_parent()) != nullptr &&
        (parameter->get_fortran_source_type() != nullptr ||
         parameter->get_fortran_source_derived_type_symbol() != nullptr ||
         parameter->get_fortran_type_spec() !=
             SgInitializedName::e_fortran_type_spec_default ||
         parameter->get_fortran_procedure_interface() != nullptr ||
         parameter->get_fortran_separate_shape_declaration() != nullptr ||
         parameter->get_fortran_separate_pointer_declaration() != nullptr ||
         parameter->get_cray_pointer_pointee() != nullptr ||
         parameter->get_fortran_cray_pointer_pointee_shape() != nullptr ||
         parameter->get_shapeDeferred())) {
      throw std::runtime_error(
          "AST JSON Fortran procedure parameter owns declaration-statement "
          "source syntax");
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
            !validFortranSeparatePointerOwnership(name) ||
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

    if (!outer->get_source_form_is_combined())
      continue;

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

static void
validateDeclarationSourceFields(const AstFileRecord &ast,
                                const RecordConstructionFieldMap &prepared) {
  for (const NodeRecord &record : ast.nodes) {
    const auto &fields = prepared.at(record.id).declaration;
    if (!fields)
      continue;
    const auto &source_order = fields->source.translation_unit_order;
    if (!source_order.has_value()) {
      continue;
    }

    const NodeRecord *position_owner = &record;
    if (record.kind == "SgNamespaceDeclarationStatement") {
      const uint64_t opening =
          singleEdgeTarget(record, "opening_source_fragment");
      if (opening == 0 ||
          ast.node(opening).kind != "SgNamespaceSourceFragment" ||
          ast.node(opening).properties.requiredInt(
              "namespace_source_fragment_form") !=
              SgNamespaceSourceFragment::
                  e_namespace_source_fragment_source_spelled) {
        throw std::runtime_error(
            "AST JSON namespace declaration source order has no exact "
            "source-spelled opening fragment");
      }
      const uint64_t introducer =
          singleEdgeTarget(record, "opening_introducer_source_fragment");
      position_owner = &ast.node(introducer != 0 ? introducer : opening);
    }

    const JsonValue &position = position_owner->location.at("start");
    const bool present = position.requiredBool("present");
    const int64_t position_order =
        present ? position.requiredInt("source_sequence") : 0;
    if (!present || position_order != *source_order) {
      throw std::runtime_error(
          "AST JSON declaration translation-unit source order disagrees "
          "with its exact source-position occurrence: " +
          record.kind + " node " + std::to_string(record.id) + " has order " +
          std::to_string(*source_order) + " and position order " +
          (present ? std::to_string(position_order) : std::string("<null>")));
    }
  }
}

static void constructDestructorEpilogue(
    const NodeRecord &record, SgFunctionDefinition *definition,
    const NodeMap &nodes, const RecordConstructionFieldMap &fields) {
  auto *owner = isSgMemberFunctionDeclaration(definition->get_declaration());
  const bool destructor =
      owner != nullptr && owner->get_specialFunctionModifier().isDestructor();
  const bool dependent = destructor && record.properties.requiredBool(
                                           "destructor_epilogue_dependent");
  const uint64_t epilogue_id = singleEdgeTarget(record, "destructor_epilogue");
  if (!dependent && epilogue_id == 0)
    return;
  if (!destructor)
    throw std::runtime_error(
        "AST JSON destructor epilogue has no exact member-function owner");
  if (dependent) {
    if (!definition->has_dependent_destructor_context())
      throw std::runtime_error(
          "AST JSON dependent destructor has no typed template context");
    definition->defer_destructor_epilogue();
  } else {
    auto *epilogue = nodeByIdAs<SgExprListExp>(nodes, epilogue_id);
    if (epilogue->get_parent() != nullptr)
      throw std::runtime_error(
          "AST JSON destructor epilogue already has an owner");
    auto members =
        destructorMemberInputs(fields.at(record.id).destructor_members, nodes);
    if (!members)
      throw std::runtime_error(
          "AST JSON destructor has unavailable typed member dependencies");
    if (!SgFunctionDefinition::has_complete_member_destruction(
            epilogue->get_expressions(), *members))
      throw std::runtime_error(
          "AST JSON destructor epilogue omits a required member destructor");
    definition->adopt_destructor_epilogue(epilogue, *members);
  }
}

static bool constructFunctionDeclaration(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    const NodeRecord &record, NodeMap &nodes, SgProject *project,
    RecordConstructionFieldMap &construction_fields,
    std::vector<const NodeRecord *> &delayed_declarations,
    std::unordered_set<uint64_t> &constructed_signature_symbol_bases,
    std::unordered_set<uint64_t> &visiting, ArrayConstruction *construction);

static bool constructSignatureDependency(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    NodeMap &nodes, uint64_t id, bool needs_symbol, bool needs_type,
    RecordConstructionFieldMap &construction_fields,
    std::unordered_set<uint64_t> &constructed_symbol_bases,
    std::unordered_set<uint64_t> &visiting,
    std::vector<const NodeRecord *> &delayed_declarations,
    ArrayConstruction *construction = nullptr) {
  if (declarationConstructionDependencyReady(ast, nodes, id, needs_symbol,
                                             needs_type))
    return true;
  if (auto node = nodes.find(id);
      node != nodes.end() && isSgTemplateVariableDeclaration(node->second)) {
    if (!visiting.insert(id).second)
      throw std::runtime_error(
          "AST JSON function signature has cyclic operands: " +
          std::to_string(id));
    const bool ready = constructTemplateVariableDeclarator(
        ast, nodes, id,
        [&](uint64_t dependency) {
          return constructSignatureDependency(
              ast, bindings, nodes, dependency, false, false,
              construction_fields, constructed_symbol_bases, visiting,
              delayed_declarations, construction);
        },
        construction);
    visiting.erase(id);
    return ready && declarationConstructionDependencyReady(
                        ast, nodes, id, needs_symbol, needs_type);
  }
  if (auto node = nodes.find(id);
      node != nodes.end() && isSgVariableDeclaration(node->second)) {
    if (!visiting.insert(id).second)
      throw std::runtime_error(
          "AST JSON function signature has cyclic operands: " +
          std::to_string(id));
    for (const auto &edge : edgesFor(ast.node(id), "variables")) {
      if (!constructSignatureDependency(ast, bindings, nodes, edge.target,
                                        false, false, construction_fields,
                                        constructed_symbol_bases, visiting,
                                        delayed_declarations, construction)) {
        visiting.erase(id);
        return false;
      }
    }
    auto node_ids = mappedNodePointers(nodes);
    constructRecordedNodes(ast, bindings, nodes, {&ast.node(id)}, node_ids,
                           nullptr, {}, construction, &construction_fields);
    visiting.erase(id);
    return declarationConstructionDependencyReady(ast, nodes, id, needs_symbol,
                                                  needs_type);
  }
  const NodeRecord &record = ast.node(id);
  if (record.kind == "SgFunctionParameterTypeList") {
    if (!visiting.insert(id).second)
      throw std::runtime_error(
          "AST JSON function signature has cyclic operands: " +
          std::to_string(id));
    auto construct_dependency = [&](uint64_t dependency) {
      return constructSignatureDependency(ast, bindings, nodes, dependency,
                                          false, false, construction_fields,
                                          constructed_symbol_bases, visiting,
                                          delayed_declarations, construction);
    };
    const uint64_t scope =
        requiredSingleEdgeTarget(record, "source_parameter_scope");
    const NodeRecord &parameters =
        ast.node(requiredSingleEdgeTarget(record, "source_parameters"));
    if (!construct_dependency(requiredSingleEdgeTarget(
            ast.node(scope), "completed_semantic_scope"))) {
      visiting.erase(id);
      return false;
    }
    auto node_ids = mappedNodePointers(nodes);
    if (nodes.count(id) == 0)
      constructRecordedNodes(ast, bindings, nodes, {&record}, node_ids, nullptr,
                             {}, construction, &construction_fields);
    bool ready = true;
    for (const auto &edge : edgesFor(parameters, "args"))
      if (!construct_dependency(edge.target))
        ready = false;
    if (ready) {
      node_ids = mappedNodePointers(nodes);
      constructRecordedNodes(ast, bindings, nodes, {&parameters}, node_ids,
                             nullptr, {}, construction, &construction_fields);
    }
    visiting.erase(id);
    return ready && declarationConstructionDependencyReady(
                        ast, nodes, id, needs_symbol, needs_type);
  }
  if (nodes.count(id) != 0 && isSgExprListExp(nodes.at(id)) == nullptr) {
    SgNode *basis = nodes.at(id);
    auto *name = isSgInitializedName(basis);
    auto *declaration = isSgDeclarationStatement(basis);
    SgScopeStatement *scope = nullptr;
    if (name != nullptr)
      scope = name->get_scope();
    else if (declaration != nullptr)
      scope = declaration->get_scope();
    if (needs_symbol && scope != nullptr && basis->get_parent() != nullptr &&
        declarationConstructionDependencyReady(ast, nodes, id, false,
                                               needs_type) &&
        constructed_symbol_bases.insert(id).second)
      restoreConstructedBasisSymbols(bindings, id, nodes, construction);
    return declarationConstructionDependencyReady(ast, nodes, id, needs_symbol,
                                                  needs_type);
  }
  if (isFunctionDeclarationRecordKind(record.kind)) {
    if (!visiting.insert(id).second)
      throw std::runtime_error(
          "AST JSON function signature has cyclic operands: " +
          std::to_string(id));
    const bool ready = constructFunctionDeclaration(
        ast, bindings, record, nodes, nullptr, construction_fields,
        delayed_declarations, constructed_symbol_bases, visiting, construction);
    visiting.erase(id);
    return ready && constructSignatureDependency(
                        ast, bindings, nodes, id, needs_symbol, needs_type,
                        construction_fields, constructed_symbol_bases, visiting,
                        delayed_declarations, construction);
  }
  static const VariantVector expression_variants(V_SgExpression);
  const bool template_argument = record.kind == "SgTemplateArgument";
  const bool base_class = record.kind == "SgBaseClass" ||
                          record.kind == "SgExpBaseClass" ||
                          record.kind == "SgNonrealBaseClass";
  const bool declaration =
      record.kind == "SgNonrealDecl" || record.kind == "SgTemplateParameter" ||
      record.kind == "SgInitializedName" ||
      record.kind == "SgTypedefDeclaration" ||
      record.kind == "SgTemplateTypedefDeclaration" ||
      record.kind == "SgTemplateInstantiationTypedefDeclaration";
  if (!kindInVariants(record.kind, expression_variants) && !template_argument &&
      !declaration && !base_class)
    return false;
  if (!visiting.insert(id).second)
    throw std::runtime_error(
        "AST JSON function signature has cyclic operands: " +
        std::to_string(id));
  std::unordered_set<uint64_t> dependencies, symbols, types;
  if (declaration)
    collectDelayedDeclarationDependencies(ast, record, nodes, dependencies,
                                          symbols, types);
  else {
    collectTypeDependencyIds(record.properties, dependencies, symbols, types);
    for (const EdgeRecord &edge : record.edges)
      if (edge.field != "parent" && edge.target != 0)
        dependencies.insert(edge.target);
    if (record.kind == "SgCxxSubobjectDestruction")
      if (uint64_t member = singleEdgeTarget(record, "member_target"))
        dependencies.insert(
            requiredSingleEdgeTarget(ast.node(member), "parent"));
  }
  bool ready = true;
  for (uint64_t dependency : dependencies)
    if (!constructSignatureDependency(
            ast, bindings, nodes, dependency, symbols.count(dependency) != 0,
            types.count(dependency) != 0, construction_fields,
            constructed_symbol_bases, visiting, delayed_declarations,
            construction))
      ready = false;
  visiting.erase(id);
  if (!ready)
    return false;
  if (template_argument) {
    const auto &fields = construction_fields.at(id);
    auto argument = constructTemplateArgument(
        record, nodes, fields.template_argument.value(), fields.flags,
        fields.positions.value());
    auto *input = argument.get();
    if (construction != nullptr)
      construction->retain(id, argument.release());
    if (!nodes.emplace(id, input).second)
      throw std::runtime_error(
          "AST JSON template argument was published twice");
    argument.release();
  } else if (base_class) {
    constructBaseClassRecord(record, nodes, construction_fields.at(id),
                             construction);
  } else if (declaration) {
    auto pending = std::find(delayed_declarations.begin(),
                             delayed_declarations.end(), &record);
    if (pending == delayed_declarations.end())
      throw std::runtime_error(
          "AST JSON declaration signature dependency has no "
          "construction record");
    buildDelayedDeclaration(ast, record, nodes, construction,
                            construction_fields.at(id));
    delayed_declarations.erase(pending);
    restoreConstructedBasisSymbols(bindings, id, nodes, construction);
    constructed_symbol_bases.insert(id);
  } else {
    auto node_ids = mappedNodePointers(nodes);
    constructRecordedNodes(ast, bindings, nodes, {&record}, node_ids, nullptr,
                           {}, construction, &construction_fields);
  }
  return declarationConstructionDependencyReady(ast, nodes, id, needs_symbol,
                                                needs_type);
}

static bool signatureReferencesOwnParameters(
    const AstFileRecord &ast, const NodeRecord &function,
    const std::unordered_set<uint64_t> &dependencies) {
  static const VariantVector expressions(V_SgExpression);
  const uint64_t parameter_scope =
      singleEdgeTarget(function, "functionParameterScope");
  std::vector<uint64_t> pending(dependencies.begin(), dependencies.end());
  std::unordered_set<uint64_t> visited;
  for (size_t index = 0; index < pending.size(); ++index) {
    const uint64_t id = pending[index];
    if (id == 0 || !visited.insert(id).second)
      continue;
    const NodeRecord &record = ast.node(id);
    if ((record.kind == "SgInitializedName" &&
         singleEdgeTarget(record, "declptr") == function.id) ||
        (parameter_scope != 0 &&
         singleEdgeTarget(record, "scope") == parameter_scope))
      return true;
    if (!kindInVariants(record.kind, expressions) &&
        record.kind != "SgTemplateArgument" && record.kind != "SgNonrealDecl")
      continue;
    std::unordered_set<uint64_t> ids, symbols, types;
    collectTypeDependencyIds(record.properties, ids, symbols, types);
    for (const EdgeRecord &edge : record.edges)
      if (edge.field != "parent" && edge.target != 0)
        ids.insert(edge.target);
    pending.insert(pending.end(), ids.begin(), ids.end());
  }
  return false;
}

struct FunctionParameterInputs {
  std::vector<uint64_t> parameters;
  SgFunctionDefinition *definition = nullptr;

  bool prepare(const AstFileRecord &ast, const SymbolBindingIndex &bindings,
               const NodeRecord &function, SgProject *project,
               RecordConstructionFieldMap &fields, NodeMap &nodes,
               ArrayConstruction &construction,
               std::vector<const NodeRecord *> &delayed,
               std::unordered_set<uint64_t> &symbols,
               std::unordered_set<uint64_t> &visiting) {
    const uint64_t scope_id =
        singleEdgeTarget(function, "functionParameterScope");
    if (scope_id != 0 && nodes.count(scope_id) == 0) {
      if (requiredSingleEdgeTarget(ast.node(scope_id), "parent") != function.id)
        return false;
      const uint64_t enclosing_id = requiredSingleEdgeTarget(function, "scope");
      if (nodes.count(enclosing_id) == 0)
        return false;
      auto *enclosing = nodeByIdAs<SgScopeStatement>(nodes, enclosing_id);
      auto *scope = isSgFunctionParameterScope(
          createNodeFromRecord(ast.node(scope_id), project, nodes));
      construction.retain(scope_id, scope);
      SageInterface::beginDetachedFunctionParameterScopeConstruction(
          scope, enclosing, enclosing);
      nodes.emplace(scope_id, scope);
    }
    if (scope_id != 0) {
      const auto &scope_record = ast.node(scope_id);
      if (const uint64_t auxiliary_id =
              singleEdgeTarget(scope_record, "auxiliary_declarations")) {
        auto *scope = nodeByIdAs<SgFunctionParameterScope>(nodes, scope_id);
        auto *owner =
            nodeByIdAs<SgAuxiliaryDeclarationList>(nodes, auxiliary_id);
        if (requiredSingleEdgeTarget(ast.node(auxiliary_id), "parent") !=
                scope_id ||
            (owner->get_parent() != nullptr && owner->get_parent() != scope) ||
            (scope->get_auxiliary_declarations() != nullptr &&
             scope->get_auxiliary_declarations() != owner))
          throw std::runtime_error(
              "AST JSON parameter scope has conflicting auxiliary ownership");
        owner->validate_semantic_non_output_role();
        construction.parameter_parents.emplace(owner, owner->get_parent());
        construction.auxiliary_owners.emplace(
            scope, scope->get_auxiliary_declarations());
        construction.auxiliary_declarations.emplace(owner,
                                                    owner->get_declarations());
        scope->set_auxiliary_declarations(owner);
        owner->set_parent(scope);
        SgDeclarationStatementPtrList declarations;
        for (const auto &edge :
             edgesFor(ast.node(auxiliary_id), "declarations")) {
          if (requiredSingleEdgeTarget(ast.node(edge.target), "parent") !=
              auxiliary_id)
            throw std::runtime_error(
                "AST JSON parameter-scope declaration names a different owner");
          if (!constructSignatureDependency(ast, bindings, nodes, edge.target,
                                            false, false, fields, symbols,
                                            visiting, delayed, &construction))
            return false;
          auto *declaration =
              nodeByIdAs<SgDeclarationStatement>(nodes, edge.target);
          if ((declaration->get_parent() != nullptr &&
               declaration->get_parent() != owner) ||
              std::find(declarations.begin(), declarations.end(),
                        declaration) != declarations.end())
            throw std::runtime_error("AST JSON parameter-scope declaration has "
                                     "conflicting membership");
          construction.parameter_parents.emplace(declaration,
                                                 declaration->get_parent());
          declaration->set_parent(owner);
          declarations.push_back(declaration);
        }
        owner->get_declarations().swap(declarations);
      }
    }
    if (const uint64_t id = singleEdgeTarget(function, "definition")) {
      const NodeRecord &record = ast.node(id);
      const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
      if (nodes.count(body_id) == 0)
        return false;
      auto *body = nodeByIdAs<SgBasicBlock>(nodes, body_id);
      if (body->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON private function body already has an owner");
      definition =
          record.kind == "SgTemplateFunctionDefinition"
              ? new SgTemplateFunctionDefinition(
                    static_cast<SgTemplateFunctionDeclaration *>(nullptr), body)
              : new SgFunctionDefinition(
                    static_cast<SgFunctionDeclaration *>(nullptr), body);
      construction.retain(id, definition);
      publishConstructorOnlyNode(
          record, definition, nodes,
          constructSourcePositions(fields.at(id).positions.value()),
          fields.at(id));
    }
    std::unordered_set<uint64_t> lists;
    for (const char *field : {"parameterList", "parameterList_syntax"}) {
      const uint64_t list_id = singleEdgeTarget(function, field);
      if (list_id == 0 || !lists.insert(list_id).second)
        continue;
      for (const EdgeRecord &edge : edgesFor(ast.node(list_id), "args")) {
        const NodeRecord &record = ast.node(edge.target);
        std::unordered_set<uint64_t> dependencies, symbol_ids, type_ids;
        collectDelayedDeclarationDependencies(ast, record, nodes, dependencies,
                                              symbol_ids, type_ids);
        for (uint64_t id : dependencies) {
          if (id == function.id)
            continue;
          if (!constructSignatureDependency(
                  ast, bindings, nodes, id, symbol_ids.count(id) != 0,
                  type_ids.count(id) != 0, fields, symbols, visiting, delayed,
                  &construction))
            return false;
        }
        buildDelayedDeclaration(ast, record, nodes, &construction,
                                fields.at(record.id), nullptr, &function);
        restoreConstructedBasisSymbols(bindings, record.id, nodes,
                                       &construction);
        symbols.insert(record.id);
        parameters.push_back(record.id);
        auto pending = std::find(delayed.begin(), delayed.end(), &record);
        if (pending == delayed.end())
          throw std::runtime_error(
              "AST JSON private function parameter has no construction record");
        delayed.erase(pending);
      }
    }
    return true;
  }

  void bind(const AstFileRecord &ast, const NodeRecord &record,
            SgFunctionDeclaration *function, NodeMap &nodes,
            ArrayConstruction &construction) {
    if (definition != nullptr) {
      definition->set_declaration(function);
      function->set_definition(definition);
    }
    if (const uint64_t id =
            singleEdgeTarget(record, "function_declarator_scope")) {
      auto *scope = nodeByIdAs<SgDeclarationScope>(nodes, id);
      construction.ownership.borrow(scope);
      SageBuilder::adoptFunctionDeclaratorScope(function, scope);
    }
    const uint64_t semantic_list =
        requiredSingleEdgeTarget(record, "parameterList");
    for (uint64_t id : parameters) {
      auto *parameter = nodeByIdAs<SgInitializedName>(nodes, id);
      if (parameter->get_declptr() != nullptr ||
          parameter->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON private function parameter already has an owner");
      parameter->set_declptr(function);
      const bool semantic =
          requiredSingleEdgeTarget(ast.node(id), "parent") == semantic_list;
      auto *list = semantic ? function->get_parameterList()
                            : function->get_parameterList_syntax();
      ASSERT_not_null(list);
      list->append_arg(parameter);
    }
  }
};

struct FunctionParameterConstruction {
  SgNode::SourceStateConstruction source_state;
  NodeMap nodes;
  NodeIds borrowed;
  NodeIds node_ids;
  SourceTypeConstruction types;
  ArrayConstruction construction;
  std::vector<const NodeRecord *> delayed;
  std::unordered_set<uint64_t> symbols;

  FunctionParameterConstruction(
      const NodeMap &available,
      const std::vector<const NodeRecord *> &delayed_declarations,
      const std::unordered_set<uint64_t> &constructed_symbols)
      : nodes(available), borrowed(mappedNodePointers(available)),
        node_ids(borrowed), construction(nodes, node_ids, borrowed),
        delayed(delayed_declarations), symbols(constructed_symbols) {}

  void publish(NodeMap &available, NodeMap &group,
               std::vector<const NodeRecord *> &delayed_declarations,
               std::unordered_set<uint64_t> &constructed_symbols) {
    nodes.merge(group);
    available.reserve(nodes.size());
    construction.commit(&types);
    available.merge(nodes);
    delayed_declarations = std::move(delayed);
    constructed_symbols = std::move(symbols);
  }
};

static size_t declarationConstructionPosition(
    const NodeRecord &owner, const char *field, uint64_t id,
    const SgDeclarationStatementPtrList &declarations, const NodeMap &nodes,
    const char *context) {
  auto next = declarations.begin();
  std::optional<size_t> position;
  for (const auto &edge : edgesFor(owner, field)) {
    if (edge.target == id) {
      if (position)
        throw std::runtime_error(std::string("AST JSON ") + context +
                                 " has duplicate membership");
      position = std::distance(declarations.begin(), next);
    } else if (next != declarations.end() && nodes.count(edge.target) != 0 &&
               nodes.at(edge.target) == *next) {
      ++next;
    }
  }
  if (!position || next != declarations.end())
    throw std::runtime_error(std::string("AST JSON ") + context +
                             " has invalid declaration order");
  return *position;
}

static bool constructFunctionDeclaration(
    const AstFileRecord &ast, const SymbolBindingIndex &bindings,
    const NodeRecord &record, NodeMap &nodes, SgProject *project,
    RecordConstructionFieldMap &construction_fields,
    std::vector<const NodeRecord *> &delayed_declarations,
    std::unordered_set<uint64_t> &constructed_signature_symbol_bases,
    std::unordered_set<uint64_t> &visiting, ArrayConstruction *construction) {
  std::unordered_set<uint64_t> signature_dependencies;
  std::unordered_set<uint64_t> signature_symbols;
  std::unordered_set<uint64_t> signature_types;
  collectDelayedDeclarationDependencies(ast, record, nodes,
                                        signature_dependencies,
                                        signature_symbols, signature_types);
  const uint64_t first_id =
      singleEdgeTarget(record, "firstNondefiningDeclaration");
  if (first_id != 0 && first_id != record.id) {
    // A redeclaration can share a canonical signature expression whose name
    // references belong to the first declaration's parameters. Construct that
    // owner and its bindings before traversing the shared expression; an
    // unordered dependency set can otherwise visit the use before its owner.
    if (!constructSignatureDependency(
            ast, bindings, nodes, first_id, false, false, construction_fields,
            constructed_signature_symbol_bases, visiting, delayed_declarations,
            construction))
      return false;
    signature_dependencies.insert(first_id);
  }
  const uint64_t definition_id = singleEdgeTarget(record, "definition");
  const uint64_t epilogue_id =
      definition_id != 0
          ? singleEdgeTarget(ast.node(definition_id), "destructor_epilogue")
          : 0;
  if (epilogue_id != 0) {
    signature_dependencies.insert(epilogue_id);
    for (uint64_t id :
         construction_fields.at(definition_id).destructor_base_declarations)
      signature_dependencies.insert(id);
    for (uint64_t id :
         construction_fields.at(definition_id).destructor_members) {
      signature_dependencies.insert(id);
      signature_dependencies.insert(
          requiredSingleEdgeTarget(ast.node(id), "parent"));
    }
  }
  const bool dependent_epilogue =
      definition_id != 0 &&
      ast.node(definition_id)
              .properties.find("destructor_epilogue_dependent") != nullptr &&
      ast.node(definition_id)
          .properties.requiredBool("destructor_epilogue_dependent");
  if (dependent_epilogue) {
    // Template-context validation needs constructed lexical owners, including
    // the enclosing function definition of a local class.
    for (uint64_t id = requiredSingleEdgeTarget(record, "scope"); id != 0;) {
      auto node = nodes.find(id);
      if (node == nodes.end())
        return false;
      if (isSgGlobal(node->second))
        break;
      const uint64_t parent_id =
          requiredSingleEdgeTarget(ast.node(id), "parent");
      auto parent = nodes.find(parent_id);
      if (parent == nodes.end() || node->second->get_parent() == nullptr)
        return false;
      if (node->second->get_parent() != parent->second)
        throw std::runtime_error(
            "AST JSON destructor context has conflicting lexical ownership");
      if (isSgTemplateClassDeclaration(parent->second) ||
          isSgTemplateFunctionDeclaration(parent->second) ||
          isSgTemplateMemberFunctionDeclaration(parent->second))
        break;
      id = parent_id;
    }
  }
  std::unique_ptr<FunctionParameterConstruction> parameters;
  const bool needs_parameters =
      construction != nullptr || epilogue_id != 0 || dependent_epilogue ||
      signatureReferencesOwnParameters(ast, record, signature_dependencies);
  if (needs_parameters && construction == nullptr) {
    parameters = std::make_unique<FunctionParameterConstruction>(
        nodes, delayed_declarations, constructed_signature_symbol_bases);
  }
  NodeMap &inputs = parameters != nullptr ? parameters->nodes : nodes;
  auto &pending_declarations =
      parameters != nullptr ? parameters->delayed : delayed_declarations;
  auto &constructed_symbols = parameters != nullptr
                                  ? parameters->symbols
                                  : constructed_signature_symbol_bases;
  auto *transaction =
      parameters != nullptr ? &parameters->construction : construction;
  SemanticArrayConstructionScope array_scope(
      transaction != nullptr ? &transaction->arrays
                             : SemanticArrayConstruction::current());
  FunctionParameterInputs parameter_inputs;
  if (needs_parameters &&
      !parameter_inputs.prepare(
          ast, bindings, record, project, construction_fields, inputs,
          *transaction, pending_declarations, constructed_symbols, visiting))
    return false;
  NodeMap dependencies;
  bool ready = true;
  auto require_dependency = [&](uint64_t id) {
    if (id == 0)
      return;
    auto node = inputs.find(id);
    if (node == inputs.end())
      ready = false;
    else
      dependencies.emplace(id, node->second);
  };
  for (uint64_t id : signature_dependencies) {
    if (!constructSignatureDependency(
            ast, bindings, inputs, id, signature_symbols.count(id) != 0,
            signature_types.count(id) != 0, construction_fields,
            constructed_symbols, visiting, pending_declarations, transaction))
      ready = false;
    require_dependency(id);
  }
  if (epilogue_id != 0 && ready) {
    for (uint64_t id :
         construction_fields.at(definition_id).destructor_members) {
      auto *member = nodeByIdAs<SgInitializedName>(inputs, id);
      auto *object = SgCxxSubobjectDestruction::get_member_class(member);
      if (object == nullptr ||
          SageInterface::getDefaultDestructor(
              isSgClassDeclaration(object->get_declaration())) != nullptr)
        continue;
      auto *defining = isSgClassDeclaration(
          object->get_declaration()->get_definingDeclaration());
      if (defining == nullptr || defining->get_definition() == nullptr)
        throw std::runtime_error(
            "AST JSON destructor member has no complete class definition");
      const auto scope = transaction->node_ids.find(defining->get_definition());
      if (scope == transaction->node_ids.end())
        throw std::runtime_error(
            "AST JSON destructor member has no selected destructor identity");
      const auto selected =
          selectedDestructorRecord(ast, ast.node(scope->second));
      if (!selected)
        throw std::runtime_error(
            "AST JSON destructor member has no selected destructor identity");
      if (!constructSignatureDependency(ast, bindings, inputs, selected->first,
                                        false, false, construction_fields,
                                        constructed_symbols, visiting,
                                        pending_declarations, transaction))
        ready = false;
    }
  }
  if (epilogue_id != 0 && ready) {
    ASSERT_not_null(transaction);
    std::unordered_set<uint64_t> visited;
    std::function<bool(uint64_t)> construct_bases = [&](uint64_t scope_id) {
      if (!visited.insert(scope_id).second)
        return true;
      if (inputs.count(scope_id) == 0)
        return false;
      auto *scope = nodeByIdAs<SgClassDefinition>(inputs, scope_id);
      SgBaseClassPtrList bases;
      for (const auto &edge : edgesFor(ast.node(scope_id), "inheritances")) {
        if (!constructSignatureDependency(ast, bindings, inputs, edge.target,
                                          false, false, construction_fields,
                                          constructed_symbols, visiting,
                                          pending_declarations, transaction))
          return false;
        auto *base = nodeByIdAs<SgBaseClass>(inputs, edge.target);
        if (base->get_parent() != nullptr && base->get_parent() != scope)
          throw std::runtime_error(
              "AST JSON destructor base has a foreign owner");
        bases.push_back(base);
        const uint64_t declaration_id = exactSerializedPointerTarget(
            ast.node(edge.target), "base_class", "base_class");
        const uint64_t definition_id = requiredSingleEdgeTarget(
            ast.node(declaration_id), "definingDeclaration");
        if (!construct_bases(requiredSingleEdgeTarget(ast.node(definition_id),
                                                      "definition")))
          return false;
      }
      auto &owned = scope->get_inheritances();
      if (!owned.empty() && owned != bases)
        throw std::runtime_error(
            "AST JSON destructor inheritance order conflicts");
      if (owned != bases) {
        transaction->inheritances.emplace(scope, owned);
        for (auto *base : bases) {
          transaction->parameter_parents.emplace(base, base->get_parent());
          base->set_parent(scope);
        }
        owned.swap(bases);
      }
      return true;
    };
    ready = construct_bases(requiredSingleEdgeTarget(record, "scope"));
  }
  for (const char *field : {"associatedClassDeclaration", "templateDeclaration",
                            "template_instantiation_pattern"})
    require_dependency(singleEdgeTarget(record, field));
  for (const EdgeRecord &edge : edgesFor(record, "dependentTemplateCandidates"))
    require_dependency(edge.target);
  const uint64_t parent_id = singleEdgeTarget(record, "parent");
  const uint64_t semantic_scope_id = singleEdgeTarget(record, "scope");
  const bool auxiliary = parent_id != 0 && ast.node(parent_id).kind ==
                                               "SgAuxiliaryDeclarationList";
  static const VariantVector class_definitions(V_SgClassDefinition);
  const bool class_member =
      parent_id != 0 &&
      kindInVariants(ast.node(parent_id).kind, class_definitions);
  if (auxiliary && requiredSingleEdgeTarget(ast.node(parent_id), "parent") !=
                       semantic_scope_id)
    throw std::runtime_error(
        "AST JSON auxiliary declaration has inconsistent lexical scope");
  if (semantic_scope_id == 0)
    throw std::runtime_error(
        "AST JSON function declaration has no exact semantic scope");
  require_dependency(semantic_scope_id);
  if (auxiliary || class_member)
    require_dependency(parent_id);
  const uint64_t scope_id = singleEdgeTarget(record, "functionParameterScope");
  const bool owns_scope =
      scope_id != 0 &&
      requiredSingleEdgeTarget(ast.node(scope_id), "parent") == record.id;
  if (scope_id != 0 && !owns_scope) {
    require_dependency(scope_id);
    require_dependency(requiredSingleEdgeTarget(ast.node(scope_id), "parent"));
  }
  if (!ready) {
    return false;
  }
  SgAuxiliaryDeclarationList *auxiliary_owner = nullptr;
  SgScopeStatement *auxiliary_scope = nullptr;
  SgDeclarationStatementPtrList auxiliary_declarations;
  size_t auxiliary_position = 0;
  if (auxiliary) {
    auxiliary_owner = nodeByIdAs<SgAuxiliaryDeclarationList>(inputs, parent_id);
    const uint64_t owner_id = semantic_scope_id;
    auxiliary_scope = nodeByIdAs<SgScopeStatement>(inputs, owner_id);
    if (requiredSingleEdgeTarget(ast.node(owner_id),
                                 "auxiliary_declarations") != parent_id ||
        auxiliary_owner->get_parent() != auxiliary_scope ||
        (auxiliary_scope->get_auxiliary_declarations() != nullptr &&
         auxiliary_scope->get_auxiliary_declarations() != auxiliary_owner))
      throw std::runtime_error(
          "AST JSON function has conflicting auxiliary scope ownership");
    auxiliary_owner->validate_semantic_non_output_role();
    auxiliary_declarations = auxiliary_owner->get_declarations();
    if (transaction != nullptr) {
      transaction->auxiliary_declarations.emplace(auxiliary_owner,
                                                  auxiliary_declarations);
      transaction->auxiliary_owners.emplace(
          auxiliary_scope, auxiliary_scope->get_auxiliary_declarations());
    }
    auxiliary_position = declarationConstructionPosition(
        ast.node(parent_id), "declarations", record.id, auxiliary_declarations,
        inputs, "auxiliary function");
  }
  SgClassDefinition *member_owner = nullptr;
  SgDeclarationStatementPtrList member_declarations;
  size_t member_position = 0;
  if (class_member) {
    member_owner = nodeByIdAs<SgClassDefinition>(inputs, parent_id);
    member_declarations = member_owner->get_members();
    if (transaction != nullptr)
      transaction->class_members.emplace(member_owner, member_declarations);
    member_position = declarationConstructionPosition(
        ast.node(parent_id), "members", record.id, member_declarations, inputs,
        "class member function");
  }
  const uint64_t list_id = requiredSingleEdgeTarget(record, "parameterList");
  const uint64_t source_list_id =
      singleEdgeTarget(record, "parameterList_syntax");
  const uint64_t ctor_list_id = singleEdgeTarget(record, "CtorInitializerList");
  auto function_positions = constructSourcePositions(
      construction_fields.at(record.id).positions.value());
  auto list_positions = constructSourcePositions(
      construction_fields.at(list_id).positions.value());
  SourcePositions source_list_positions, scope_positions, ctor_positions;
  if (source_list_id != 0 && source_list_id != list_id)
    source_list_positions = constructSourcePositions(
        construction_fields.at(source_list_id).positions.value());
  if (owns_scope)
    scope_positions = constructSourcePositions(
        construction_fields.at(scope_id).positions.value());
  if (ctor_list_id != 0)
    ctor_positions = constructSourcePositions(
        construction_fields.at(ctor_list_id).positions.value());
  RecordNodeConstruction ordinary_ownership;
  auto &ownership =
      transaction != nullptr ? transaction->ownership : ordinary_ownership;
  if (owns_scope && inputs.count(scope_id) == 0) {
    auto *scope = isSgFunctionParameterScope(ownership.own(
        createNodeFromRecord(ast.node(scope_id), project, dependencies,
                             std::move(scope_positions))));
    dependencies.emplace(scope_id, scope);
  } else if (owns_scope)
    require_dependency(scope_id);
  if (source_list_id != 0 && source_list_id != list_id)
    dependencies.emplace(source_list_id,
                         ownership.make<SgFunctionParameterList>());
  auto *function = isSgFunctionDeclaration(ownership.own(createNodeFromRecord(
      record, project, dependencies, std::move(function_positions))));
  ASSERT_not_null(function);
  requireRestoredKind(function, record);
  if (first_id == record.id)
    function->set_firstNondefiningDeclaration(function);
  else if (first_id != 0) {
    auto *first = nodeByIdAs<SgFunctionDeclaration>(inputs, first_id);
    if (first->get_firstNondefiningDeclaration() != first)
      throw std::runtime_error(
          "AST JSON function has no complete canonical declaration");
    function->set_firstNondefiningDeclaration(first);
  }
  if (auxiliary_owner != nullptr) {
    function->set_parent(auxiliary_owner);
    function->set_scope(auxiliary_scope);
    auxiliary_declarations.insert(
        auxiliary_declarations.begin() + auxiliary_position, function);
  } else if (member_owner != nullptr) {
    function->set_parent(member_owner);
    member_declarations.insert(member_declarations.begin() + member_position,
                               function);
  }
  if (needs_parameters)
    parameter_inputs.bind(ast, record, function, inputs, *transaction);
  if (SgFunctionDefinition *definition = function->get_definition()) {
    if (epilogue_id != 0 && transaction != nullptr) {
      auto *epilogue = nodeByIdAs<SgExprListExp>(inputs, epilogue_id);
      transaction->parameter_parents.emplace(epilogue, epilogue->get_parent());
    }
    constructDestructorEpilogue(ast.node(definition_id), definition, inputs,
                                construction_fields);
  }
  auto *list = function->get_parameterList();
  if (list == nullptr || list->get_parent() != function ||
      list->get_firstNondefiningDeclaration() != list ||
      list->get_definingDeclaration() != nullptr)
    throw std::runtime_error(
        "AST JSON function constructor has no owned parameter list");
  NodeMap group;
  publishConstructorOnlyNode(ast.node(list_id), list, group,
                             std::move(list_positions),
                             construction_fields.at(list_id));
  if (source_list_id != 0 && source_list_id != list_id)
    publishConstructorOnlyNode(ast.node(source_list_id),
                               function->get_parameterList_syntax(), group,
                               std::move(source_list_positions),
                               construction_fields.at(source_list_id));
  if (owns_scope)
    publishRecordedNode(ast.node(scope_id), dependencies.at(scope_id), group,
                        construction_fields.at(scope_id));
  if (ctor_list_id != 0) {
    auto *owner = isSgMemberFunctionDeclaration(function);
    ASSERT_not_null(owner);
    auto *ctor_list = owner->get_CtorInitializerList();
    if (ctor_list == nullptr || ctor_list->get_parent() != owner ||
        !ctor_list->get_ctors().empty())
      throw std::runtime_error(
          "AST JSON member function has no fresh owned initializer list");
    requireRestoredKind(ctor_list, ast.node(ctor_list_id));
    publishConstructorOnlyNode(ast.node(ctor_list_id), ctor_list, group,
                               std::move(ctor_positions),
                               construction_fields.at(ctor_list_id));
  }
  group.emplace(record.id, function);
  if (parameters != nullptr)
    parameters->publish(nodes, group, delayed_declarations,
                        constructed_signature_symbol_bases);
  else {
    if (transaction != nullptr) {
      for (const auto &[id, node] : group) {
        if (nodes.count(id) == 0)
          transaction->retain(id, node);
        transaction->node_ids.emplace(node, id);
      }
    } else
      ownership.publish();
    nodes.merge(group);
  }
  if (auxiliary_owner != nullptr) {
    auxiliary_scope->set_auxiliary_declarations(auxiliary_owner);
    auxiliary_owner->get_declarations().swap(auxiliary_declarations);
  }
  if (member_owner != nullptr)
    member_owner->get_members().swap(member_declarations);
  return true;
}

static void constructFunctionDefinitions(
    const AstFileRecord &ast, NodeMap &nodes, SgProject *project,
    const RecordConstructionFieldMap &fields, bool require_complete) {
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgFunctionDefinition" &&
        record.kind != "SgTemplateFunctionDefinition")
      continue;
    if (const auto existing = nodes.find(record.id); existing != nodes.end()) {
      requireRestoredKind(existing->second, record);
      continue;
    }
    const uint64_t declaration_id = requiredSingleEdgeTarget(record, "parent");
    const uint64_t body_id = requiredSingleEdgeTarget(record, "body");
    const uint64_t epilogue_id =
        singleEdgeTarget(record, "destructor_epilogue");
    const bool bases_ready =
        epilogue_id == 0 ||
        std::all_of(fields.at(record.id).destructor_base_declarations.begin(),
                    fields.at(record.id).destructor_base_declarations.end(),
                    [&](uint64_t id) {
                      return declarationConstructionDependencyReady(
                          ast, nodes, id, false, false);
                    });
    const bool members_ready =
        epilogue_id == 0 ||
        destructorMemberInputs(fields.at(record.id).destructor_members, nodes)
            .has_value();
    if (nodes.count(declaration_id) == 0 || nodes.count(body_id) == 0 ||
        (epilogue_id != 0 && nodes.count(epilogue_id) == 0) || !bases_ready ||
        !members_ready) {
      if (require_complete)
        throw std::runtime_error("AST JSON function definition has unavailable "
                                 "constructor inputs: " +
                                 std::to_string(record.id));
      continue;
    }
    auto *declaration =
        nodeByIdAs<SgFunctionDeclaration>(nodes, declaration_id);
    auto *body = nodeByIdAs<SgBasicBlock>(nodes, body_id);
    if (declaration->get_definition() != nullptr)
      throw std::runtime_error(
          "AST JSON function definition was published twice");
    SgNode *definition =
        createNodeFromRecord(record, project, declaration, body);
    constructDestructorEpilogue(record, isSgFunctionDefinition(definition),
                                nodes, fields);
    requireRestoredKind(definition, record);
    if (!nodes.emplace(record.id, definition).second)
      throw std::runtime_error(
          "AST JSON function definition ID was published twice");
  }
}

static void
constructClassDefinitions(const AstFileRecord &ast, NodeMap &nodes,
                          SgProject *project,
                          std::unordered_set<uint64_t> &restored_declarations,
                          bool require_complete) {
  bool constructed = false;
  do {
    constructed = false;
    for (const NodeRecord &record : ast.nodes) {
      if (record.kind != "SgDeclarationScope" || nodes.count(record.id) != 0)
        continue;
      const uint64_t outer = singleEdgeTarget(record, "semantic_outer_scope");
      if (outer == 0 || nodes.count(outer) == 0)
        continue;
      SgNode *node = createNodeFromRecord(record, project, nodes);
      requireRestoredKind(node, record);
      nodes.emplace(record.id, node);
      constructed = true;
    }
    restoreAvailableParentsAndScopes(ast, nodes);
    restoreDeclarationIdentityEdges(ast, nodes, project, restored_declarations,
                                    false);
    for (const NodeRecord &record : ast.nodes) {
      if (record.kind != "SgClassDefinition" &&
          record.kind != "SgTemplateClassDefinition" &&
          record.kind != "SgTemplateInstantiationDefn")
        continue;
      if (nodes.count(record.id) != 0)
        continue;

      const uint64_t owner_id = requiredSingleEdgeTarget(record, "parent");
      const NodeRecord &owner_record = ast.node(owner_id);
      if (restored_declarations.count(owner_id) == 0)
        continue;
      const uint64_t first_id =
          singleEdgeTarget(owner_record, "firstNondefiningDeclaration");
      if (first_id != 0 && restored_declarations.count(first_id) == 0)
        continue;

      auto *declaration = nodeByIdAs<SgClassDeclaration>(nodes, owner_id);
      if (declaration->get_firstNondefiningDeclaration() == nullptr ||
          declaration->get_definition() != nullptr) {
        throw std::runtime_error(
            "AST JSON class definition owner has incomplete canonical "
            "identity");
      }
      SgNode *definition =
          createNodeFromRecord(record, project, nullptr, nullptr, declaration);
      requireRestoredKind(definition, record);
      if (definition->get_parent() != declaration ||
          declaration->get_definition() != definition)
        throw std::runtime_error(
            "AST JSON class definition lost its constructor owner");
      if (!nodes.emplace(record.id, definition).second)
        throw std::runtime_error(
            "AST JSON class definition was published twice");
      constructed = true;
    }
  } while (constructed);

  if (require_complete) {
    for (const NodeRecord &record : ast.nodes) {
      if ((record.kind == "SgClassDefinition" ||
           record.kind == "SgTemplateClassDefinition" ||
           record.kind == "SgTemplateInstantiationDefn") &&
          nodes.count(record.id) == 0) {
        throw std::runtime_error(
            "AST JSON class definition has an unavailable typed owner: " +
            std::to_string(record.id));
      }
    }
  }
  for (const auto &[id, node] : nodes) {
    auto *declaration = isSgClassDeclaration(node);
    if (declaration == nullptr)
      continue;
    if (!require_complete) {
      const NodeRecord &record = ast.node(id);
      const uint64_t first =
          singleEdgeTarget(record, "firstNondefiningDeclaration");
      const uint64_t defining = singleEdgeTarget(record, "definingDeclaration");
      if (restored_declarations.count(id) == 0 ||
          (first != 0 && restored_declarations.count(first) == 0) ||
          (defining != 0 && restored_declarations.count(defining) == 0))
        continue;
      auto *definition_owner =
          isSgClassDeclaration(declaration->get_definingDeclaration());
      if (definition_owner != nullptr &&
          definition_owner->get_definition() == nullptr)
        continue;
    }
    constructClassTypeForDeclaration(declaration);
  }
}

static void validateIncludeOwnershipRecords(const AstFileRecord &ast) {
  auto target = [&](const NodeRecord &record, const char *field,
                    const char *kind) -> const NodeRecord & {
    const NodeRecord &node = ast.node(requiredSingleEdgeTarget(record, field));
    if (node.kind != kind)
      throw std::runtime_error("AST JSON include ownership has an invalid " +
                               std::string(field) + " target");
    return node;
  };
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgIncludeFile") {
      const auto &source = target(record, "source_file", "SgSourceFile");
      const auto &unit =
          target(record, "source_file_of_translation_unit", "SgSourceFile");
      const auto &including =
          target(record, "including_source_file", "SgSourceFile");
      const uint64_t parent = singleEdgeTarget(record, "parent_include_file");
      if (parent == 0) {
        if (!record.properties.requiredBool("isRootSourceFile") ||
            singleEdgeTarget(record, "parent") != source.id ||
            unit.id != source.id || including.id != source.id ||
            singleEdgeTarget(source, "associated_include_file") != record.id)
          throw std::runtime_error(
              "AST JSON include root has invalid ownership");
      } else {
        const auto &owner =
            target(record, "parent_include_file", "SgIncludeFile");
        const auto siblings = edgesFor(owner, "include_file_list");
        if (record.properties.requiredBool("isRootSourceFile") ||
            singleEdgeTarget(record, "parent") != parent ||
            singleEdgeTarget(owner, "source_file_of_translation_unit") !=
                unit.id ||
            singleEdgeTarget(owner, "source_file") != including.id ||
            std::count_if(siblings.begin(), siblings.end(),
                          [&](const EdgeRecord &edge) {
                            return edge.target == record.id;
                          }) != 1)
          throw std::runtime_error(
              "AST JSON include child has invalid ownership");
      }
      std::unordered_set<uint64_t> children;
      for (const EdgeRecord &edge : edgesFor(record, "include_file_list")) {
        const auto &child = ast.node(edge.target);
        if (child.kind != "SgIncludeFile" ||
            singleEdgeTarget(child, "parent_include_file") != record.id ||
            singleEdgeTarget(child, "parent") != record.id ||
            !children.insert(child.id).second)
          throw std::runtime_error(
              "AST JSON include list has invalid ownership");
      }
    } else if (record.kind == "SgSourceFile") {
      const uint64_t include_id =
          singleEdgeTarget(record, "associated_include_file");
      if (include_id != 0) {
        const auto &include =
            target(record, "associated_include_file", "SgIncludeFile");
        if (singleEdgeTarget(include, "source_file") != record.id)
          throw std::runtime_error(
              "AST JSON source has a foreign include association");
      }
      const uint64_t global_id = singleEdgeTarget(record, "globalScope");
      if (global_id == 0)
        continue;
      const auto &global = target(record, "globalScope", "SgGlobal");
      if (singleEdgeTarget(global, "parent") == record.id)
        continue;
      if (!record.properties.requiredBool("is_header_file") || include_id == 0)
        throw std::runtime_error("AST JSON source has a foreign global scope");
      const auto &include = ast.node(include_id);
      const auto &unit =
          target(include, "source_file_of_translation_unit", "SgSourceFile");
      if (singleEdgeTarget(unit, "globalScope") != global_id ||
          singleEdgeTarget(global, "parent") != unit.id)
        throw std::runtime_error(
            "AST JSON header has an invalid borrowed global scope");
    }
  }
}

SgSourceFile *reconstructSourceFile(const AstFileRecord &ast,
                                    SgSourceFile *old_file,
                                    NodeMap *restored_nodes) {
  auto construction_fields = validateConstructorOnlyRecords(ast);
  const auto bindings = indexSymbolBindings(ast);
  const auto ctor_initializer_list_owners =
      validateCtorInitializerListOwners(ast);
  validateParentChains(ast);
  validateIncludeOwnershipRecords(ast);
  validateSourceFileCommands(ast);
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgSourceFile")
      continue;
    const auto abi = targetAbiFromJson(record.properties.at("target_abi"));
    if (abi.available &&
        !record.properties.at("target_size_type").requiredBool("present"))
      throw std::runtime_error(
          "AST JSON available target ABI requires its exact size_t type");
  }
  for (const NodeRecord &record : ast.nodes) {
    if (!isFunctionDeclarationRecordKind(record.kind))
      continue;
    (void)functionReturnTypeSourceFieldsFromJson(record.properties);
    if (record.properties.requiredBool("type_syntax_is_available") !=
        record.properties.at("function_type_syntax").requiredBool("present"))
      throw std::runtime_error("AST JSON SgFunctionDeclaration " +
                               record.properties.requiredString("name") +
                               " has inconsistent function type syntax state");
    const JsonValue &function_type = record.properties.at("function_type");
    (void)semanticFunctionTypeKindFromJson(function_type);
    if (!function_type.at("return_type")
             .exactlyEquals(record.properties.at("return_type")))
      throw std::runtime_error(
          "AST JSON function_type and return_type disagree");
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgNonrealRefExp")
      continue;
    const JsonValue &properties = record.properties;
    const bool non_odr_use = properties.requiredBool("is_non_odr_use");
    const int64_t function_id =
        properties.requiredInt("resolved_function_declaration");
    const int64_t variable_id =
        properties.requiredInt("resolved_variable_declaration");
    if (function_id < 0 || variable_id < 0 ||
        (function_id != 0 && variable_id != 0))
      throw std::runtime_error("AST JSON SgNonrealRefExp requires nonnegative, "
                               "exclusive resolved targets");
    if (non_odr_use && variable_id == 0)
      throw std::runtime_error(
          "AST JSON non-ODR reference requires an exact variable template");
    if (function_id == 0 && variable_id == 0)
      continue;
    if (function_id != 0 &&
        !isFunctionDeclarationRecordKind(ast.node(function_id).kind))
      throw std::runtime_error("AST JSON node id has unexpected Sage type " +
                               std::to_string(function_id) + ": expected " +
                               typeid(SgFunctionDeclaration).name() + ", got " +
                               ast.node(function_id).kind);
    if (variable_id != 0 &&
        ast.node(variable_id).kind != "SgTemplateVariableDeclaration")
      throw std::runtime_error("AST JSON node id has unexpected Sage type " +
                               std::to_string(variable_id) + ": expected " +
                               typeid(SgTemplateVariableDeclaration).name() +
                               ", got " + ast.node(variable_id).kind);
    const int64_t declaration_id = properties.requiredInt("symbol_declaration");
    if (declaration_id <= 0)
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp requires a positive symbol declaration");
    const NodeRecord &declaration = ast.node(declaration_id);
    if (declaration.kind != "SgNonrealDecl")
      throw std::runtime_error("AST JSON node id has unexpected Sage type " +
                               std::to_string(declaration_id) + ": expected " +
                               typeid(SgNonrealDecl).name() + ", got " +
                               declaration.kind);
    const JsonValue &symbol = properties.at("symbol");
    if (symbol.requiredNonnegativeNodeId("symbol_declaration") !=
            static_cast<uint64_t>(declaration_id) ||
        symbol.requiredString("symbol_kind") != "SgNonrealSymbol")
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp symbol cross-edge is inconsistent");
    validateNonrealResolvedTargetIdentity<uint64_t>(
        function_id, variable_id,
        singleEdgeTarget(declaration, "templateDeclaration"));
  }
  auto delayed_declarations = collectDelayedDeclarations(ast, {});
  for (const NodeRecord &record : ast.nodes) {
    if (construction_fields.count(record.id) == 0) {
      auto inserted = construction_fields.emplace(
          record.id,
          RecordConstructionFields{validateConstructionFlags(record), {}});
      prepareRecordPublicationFields(record, inserted.first->second);
    }
  }
  validateDeclarationSourceFields(ast, construction_fields);
  struct FileIdMapRollback {
    std::map<int, std::string> id_to_name =
        Sg_File_Info::get_fileidtoname_map();
    std::map<std::string, int> name_to_id =
        Sg_File_Info::get_nametofileid_map();
    bool committed = false;

    ~FileIdMapRollback() {
      if (!committed) {
        Sg_File_Info::get_fileidtoname_map().swap(id_to_name);
        Sg_File_Info::get_nametofileid_map().swap(name_to_id);
      }
    }
  } file_id_maps;
  SgProject *project = owningProject(old_file);
  ROSE_ASSERT(project != nullptr);
  DeserializationProjectGuard project_guard(project, old_file,
                                            ast.project_files_id != 0);
  PointerMemberTypeDeserializationIdentityGuard
      pointer_member_type_identity_guard;
  FullTypeDeserializationIdentityGuard full_type_identity_guard;
  TemplateTypeDeserializationIdentityGuard template_type_identity_guard;
  FunctionTypeDeserializationIdentityGuard function_type_identity_guard;
  ExternalFunctionDeserializationIdentityGuard external_function_identity_guard(
      ast);
  ExternalClassDeserializationIdentityGuard external_class_identity_guard;
  restoreFileIdMapFromMetadata(ast.metadata);

  for (const NodeRecord &record : ast.nodes)
    construction_fields.at(record.id).positions =
        validateSourcePositionRecord(record);
  for (const NodeRecord &record : ast.nodes) {
    auto &fields = construction_fields.at(record.id);
    fields.attributes = prepareAstAttributes(record);
    fields.preprocessing = preprocessingInfoFieldsFromJson(record);
    static const VariantVector located(V_SgLocatedNode);
    if (!fields.preprocessing.empty() && !kindInVariants(record.kind, located))
      throw std::runtime_error(
          "AST JSON non-located node owns preprocessing entries");
  }
  SgNode::SourceStateConstruction source_construction;
  NodeMap nodes;
  nodes.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgFileList" && record.kind != "SgForInitStatement" &&
        record.kind != "SgDeclarationScope")
      continue;
    if (record.kind == "SgDeclarationScope" &&
        singleEdgeTarget(record, "semantic_outer_scope") != 0)
      continue;
    SgNode *node = createNodeFromRecord(record, project, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  SgFileList *boundary_owner = nullptr;
  if (ast.project_files_id != 0) {
    nodeByIdAs<SgFileList>(nodes, ast.project_files_id)->set_parent(project);
  } else {
    boundary_owner = isSgFileList(old_file->get_parent());
    ASSERT_not_null(boundary_owner);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateDeclaration")
      continue;
    SgNode *node = createNodeFromRecord(record, project, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  std::vector<const NodeRecord *> pending_templates;
  for (const NodeRecord &record : ast.nodes)
    if (record.kind == "SgTemplateClassDeclaration" ||
        record.kind == "SgTemplateVariableDeclaration")
      pending_templates.push_back(&record);
  while (!pending_templates.empty()) {
    const size_t previous_size = pending_templates.size();
    for (size_t index = 0; index < pending_templates.size();) {
      const NodeRecord &record = *pending_templates[index];
      const uint64_t primary =
          singleEdgeTarget(record, "specializedTemplateDeclaration");
      const uint64_t member_pattern =
          singleEdgeTarget(record, "instantiatedFromMemberTemplate");
      if ((primary != 0 && nodes.count(primary) == 0) ||
          (member_pattern != 0 && nodes.count(member_pattern) == 0)) {
        ++index;
        continue;
      }
      SgDeclarationStatement *selected =
          primary != 0 ? nodeByIdAs<SgDeclarationStatement>(nodes, primary)
                       : nullptr;
      SgNode *node =
          createNodeFromRecord(record, project, nullptr, nullptr, nullptr,
                               nullptr, selected, nullptr, nullptr, nodes);
      requireRestoredKind(node, record);
      nodes.emplace(record.id, node);
      pending_templates[index] = pending_templates.back();
      pending_templates.pop_back();
    }
    if (pending_templates.size() == previous_size)
      throw std::runtime_error(
          "AST JSON template declarations have missing or cyclic primary "
          "dependencies");
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind == "SgFunctionParameterTypeList")
      continue;
    if (record.kind == "SgFunctionParameterScope" ||
        record.kind == "SgFunctionParameterList") {
      const uint64_t parent = requiredSingleEdgeTarget(record, "parent");
      if (ast.node(parent).kind == "SgFunctionParameterTypeList" ||
          isFunctionDeclarationRecordKind(ast.node(parent).kind) ||
          (record.kind == "SgFunctionParameterScope" &&
           ast.node(parent).kind == "SgRequiresExpr"))
        continue;
    }
    if (record.kind == "SgFileList" || record.kind == "SgSourceFile" ||
        record.kind == "SgTemplateDeclaration" ||
        record.kind == "SgDeclarationScope" ||
        record.kind == "SgForInitStatement" ||
        record.kind == "SgTemplateClassDeclaration" ||
        record.kind == "SgTemplateVariableDeclaration" ||
        (record.kind == "SgNonrealDecl" &&
         singleEdgeTarget(record, "templateDeclaration") != 0) ||
        ctor_initializer_list_owners.count(record.id) != 0 ||
        requiresDelayedRebuild(record) || record.kind == "SgRequiresExpr" ||
        record.kind == "SgVariableDefinition" ||
        record.kind == "SgEnumDeclaration" || record.kind == "SgUseStatement" ||
        record.kind == "SgPragmaDeclaration" ||
        record.kind == "SgTemplateInstantiationDecl" ||
        isFunctionDeclarationRecordKind(record.kind) ||
        record.kind == "SgFunctionDefinition" ||
        record.kind == "SgTemplateFunctionDefinition" ||
        record.kind == "SgClassDefinition" ||
        record.kind == "SgTemplateClassDefinition" ||
        record.kind == "SgTemplateArgument" ||
        record.kind == "SgNamespaceDeclarationStatement" ||
        record.kind == "SgNamespaceDefinitionStatement" ||
        record.kind == "SgUsingDirectiveStatement" ||
        record.kind == "SgUsingDeclarationStatement" ||
        record.kind == "SgTemplateInstantiationDirectiveStatement" ||
        record.kind == "SgLambdaExp" || record.kind == "SgLambdaCapture") {
      continue;
    }
    SgNode *node = nullptr;
    if (record.kind == "SgForStatement") {
      auto positions = constructSourcePositions(
          construction_fields.at(record.id).positions.value());
      SgNode::SourceStateConstruction construction;
      auto *initializer = nodeByIdAs<SgForInitStatement>(
          nodes, requiredSingleEdgeTarget(record, "for_init_stmt"));
      RecordNodeConstruction ownership;
      ownership.borrow(initializer);
      node = ownership.make<SgForStatement>(
          initializer, static_cast<SgStatement *>(nullptr),
          static_cast<SgExpression *>(nullptr),
          static_cast<SgStatement *>(nullptr));
      publishConstructorOnlyNode(record, node, nodes, std::move(positions),
                                 construction_fields.at(record.id));
      ownership.publish();
      continue;
    } else {
      node = createNodeFromRecord(record, project, nodes);
    }
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateInstantiationDecl")
      continue;
    auto *primary = nodeByIdAs<SgTemplateClassDeclaration>(
        nodes, requiredSingleEdgeTarget(record, "templateDeclaration"));
    auto *selected = nodeByIdAs<SgTemplateClassDeclaration>(
        nodes,
        requiredSingleEdgeTarget(record, "specializedTemplateDeclaration"));
    SgNode *node =
        createNodeFromRecord(record, project, nullptr, primary, nullptr,
                             nullptr, selected, nullptr, nullptr, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgNamespaceDeclarationStatement")
      continue;
    const NodeRecord &definition_record =
        ast.node(requiredSingleEdgeTarget(record, "definition"));
    SgNode *declaration_node = createNodeFromRecord(record, project, nodes);
    requireRestoredKind(declaration_node, record);
    auto *declaration = isSgNamespaceDeclarationStatement(declaration_node);
    SgNode *definition_node = createNodeFromRecord(
        definition_record, project, nullptr, nullptr, nullptr, declaration);
    requireRestoredKind(definition_node, definition_record);
    auto *definition = isSgNamespaceDefinitionStatement(definition_node);
    if (declaration->get_definition() != definition ||
        definition->get_parent() != declaration)
      throw std::runtime_error(
          "AST JSON namespace definition lost its constructor owner");
    nodes.emplace(record.id, declaration);
    nodes.emplace(definition_record.id, definition);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgNamespaceDefinitionStatement")
      continue;
    auto *definition =
        nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, record.id);
    definition->set_global_definition(
        nodeByIdAs<SgNamespaceDefinitionStatement>(
            nodes, requiredSingleEdgeTarget(record, "global_definition")));
    const uint64_t previous_id =
        singleEdgeTarget(record, "previousNamespaceDefinition");
    const uint64_t next_id =
        singleEdgeTarget(record, "nextNamespaceDefinition");
    definition->set_previousNamespaceDefinition(
        previous_id != 0
            ? nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, previous_id)
            : nullptr);
    definition->set_nextNamespaceDefinition(
        next_id != 0
            ? nodeByIdAs<SgNamespaceDefinitionStatement>(nodes, next_id)
            : nullptr);
  }
  std::unordered_set<uint64_t> restored_declaration_identities;
  restoreAvailableParentsAndScopes(ast, nodes);

  std::vector<const NodeRecord *> pending_functions;
  std::unordered_set<uint64_t> constructed_signature_symbol_bases;
  for (const NodeRecord &record : ast.nodes)
    if (isFunctionDeclarationRecordKind(record.kind))
      pending_functions.push_back(&record);
  while (!pending_functions.empty()) {
    const size_t previous_size = pending_functions.size();
    const size_t previous_nodes = nodes.size();
    constructClassDefinitions(ast, nodes, project,
                              restored_declaration_identities, false);
    constructFunctionDefinitions(ast, nodes, project, construction_fields,
                                 false);
    restoreAvailableParentsAndScopes(ast, nodes);
    drainDelayedDeclarations(ast, bindings, delayed_declarations, nodes,
                             construction_fields, false);
    for (const NodeRecord &record : ast.nodes) {
      if (record.kind != "SgTemplateClassDeclaration")
        continue;
      std::unordered_set<uint64_t> visiting;
      constructTemplateClassIdentityInputs(
          ast, nodes, record.id, [&](uint64_t dependency) {
            return constructSignatureDependency(
                ast, bindings, nodes, dependency, false, false,
                construction_fields, constructed_signature_symbol_bases,
                visiting, delayed_declarations);
          });
    }
    for (size_t index = 0; index < pending_functions.size();) {
      const NodeRecord &record = *pending_functions[index];
      std::unordered_set<uint64_t> visiting{record.id};
      if (nodes.count(record.id) == 0 &&
          !constructFunctionDeclaration(
              ast, bindings, record, nodes, project, construction_fields,
              delayed_declarations, constructed_signature_symbol_bases,
              visiting, nullptr)) {
        ++index;
        continue;
      }
      pending_functions[index] = pending_functions.back();
      pending_functions.pop_back();
    }
    if (pending_functions.size() == previous_size &&
        nodes.size() == previous_nodes)
      throw std::runtime_error(missingDeclarationDependencies(
          ast, nodes, pending_functions,
          "AST JSON functions have missing or cyclic class/template/scope "
          "dependencies:"));
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgUseStatement")
      continue;
    const uint64_t module_id = singleEdgeTarget(record, "module");
    const JsonValue &external_module = record.properties.at("external_module");
    const bool external_present = external_module.requiredBool("present");
    if (module_id != 0 && external_present)
      throw std::runtime_error(
          "AST JSON USE statement has two module identities");
    SgModuleStatement *module =
        module_id != 0 ? nodeByIdAs<SgModuleStatement>(nodes, module_id)
                       : externalModuleFromJson(external_module);
    SgNode *node = createUseStatementFromRecord(record, project, module);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgUsingDirectiveStatement")
      continue;
    auto *declaration = nodeByIdAs<SgNamespaceDeclarationStatement>(
        nodes, exactSerializedPointerTarget(record, "namespace_declaration",
                                            "namespaceDeclaration"));
    SgNode *node =
        createNodeFromRecord(record, project, nullptr, nullptr, nullptr,
                             declaration, nullptr, nullptr, nullptr, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  constructFunctionDefinitions(ast, nodes, project, construction_fields, true);
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateInstantiationDirectiveStatement")
      continue;
    auto *declaration = nodeByIdAs<SgDeclarationStatement>(
        nodes, requiredSingleEdgeTarget(record, "declaration"));
    SgNode *node =
        createNodeFromRecord(record, project, nullptr, nullptr, nullptr,
                             nullptr, declaration, nullptr, nullptr, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgPragmaDeclaration")
      continue;
    SgPragma *pragma =
        nodeByIdAs<SgPragma>(nodes, requiredSingleEdgeTarget(record, "pragma"));
    SgNode *node =
        createNodeFromRecord(record, project, pragma, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nodes);
    requireRestoredKind(node, record);
    nodes.emplace(record.id, node);
  }
  constructClassDefinitions(ast, nodes, project,
                            restored_declaration_identities, false);
  validateDefinitionOwners(ast, nodes);
  restoreAvailableConstructionScopes(ast, nodes);
  restoreAvailableAuxiliaryDeclarationOwnership(ast, nodes);
  validateDeclarationSpecializationFamilies(nodes);
  publishAnonymousFortranProgramUnitSymbolKeys(ast, nodes);
  validateNamespaceCanonicalDefinitionChains(ast, nodes);
  constructClassDefinitions(ast, nodes, project,
                            restored_declaration_identities, true);
  validateDefinitionOwners(ast, nodes);
  restoreAvailableParentsAndScopes(ast, nodes);
  restoreAvailableAuxiliaryDeclarationOwnership(ast, nodes);
  // Classes in these newly constructed scopes also need their canonical
  // declaration families before a dependent declarator can construct a type.
  restoreDeclarationIdentityEdges(ast, nodes, project,
                                  restored_declaration_identities, false);
  drainDelayedDeclarations(ast, bindings, delayed_declarations, nodes,
                           construction_fields, false);
  // References need the existing lexical tables before operand construction.
  // Operand-owned scopes acquire their tables when their exact children exist.
  std::unordered_set<uint64_t> ready_symbol_scopes;
  std::unordered_set<uint64_t> delayed_symbol_scopes;
  for (const NodeRecord &record : ast.nodes) {
    if (record.properties.find("symbol_table") != nullptr) {
      if (nodes.count(record.id) != 0)
        ready_symbol_scopes.insert(record.id);
      else
        delayed_symbol_scopes.insert(record.id);
    }
  }
  SerializedSymbolTableConstruction symbol_construction;
  SymbolConstructionAdvance advance_symbols = [&](const NodeMap &available) {
    const auto before = restored_declaration_identities.size();
    restoreDeclarationIdentityEdges(ast, available, project,
                                    restored_declaration_identities, false);
    const bool identities_ready =
        restored_declaration_identities.size() != before;
    return constructPendingSerializedSymbols(symbol_construction, available,
                                             false) ||
           identities_ready;
  };
  restoreSerializedSymbolTables(ast, nodes, ready_symbol_scopes,
                                bindings.tables, &symbol_construction);
  drainDelayedDeclarations(ast, bindings, delayed_declarations, nodes,
                           construction_fields, false, advance_symbols);
  restoreClassTypeConstructionInputs(ast, bindings, nodes, delayed_declarations,
                                     construction_fields);
  drainDelayedDeclarations(ast, bindings, delayed_declarations, nodes,
                           construction_fields, false, advance_symbols);
  constructExternalFunctionParameterScopeSymbols(ast, nodes, false);
  constructConstructorOnlyNodes(ast, bindings, nodes, construction_fields,
                                &delayed_declarations, advance_symbols);
  restoreSerializedSymbolTables(ast, nodes, delayed_symbol_scopes,
                                bindings.tables, &symbol_construction);
  // Complete declarations whose types referenced delayed operands. Anything
  // still pending is malformed or cyclic and is reported with the leftover
  // identifiers and their missing dependencies.
  drainDelayedDeclarations(ast, bindings, delayed_declarations, nodes,
                           construction_fields, true, advance_symbols);
  constructPendingSerializedSymbols(symbol_construction, nodes, true);
  constructExternalFunctionParameterScopeSymbols(ast, nodes, true);
  for (const auto &[list_id, owner_id] : ctor_initializer_list_owners) {
    SgCtorInitializerList *list =
        nodeByIdAs<SgCtorInitializerList>(nodes, list_id);
    if (list->get_parent() !=
        nodeByIdAs<SgMemberFunctionDeclaration>(nodes, owner_id))
      throw std::runtime_error(
          "AST JSON constructor initializer list lost its typed owner");
    for (const EdgeRecord &edge : edgesFor(ast.node(list_id), "ctors")) {
      auto *child = nodeByIdAs<SgInitializedName>(nodes, edge.target);
      if (child->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON constructor initializer child is already owned");
      list->append_ctor_initializer(child);
    }
  }
  constructVariableDefinitions(ast, nodes, construction_fields);
  restoreDeclarationIdentityEdges(ast, nodes, project,
                                  restored_declaration_identities, true);
  for (const NodeRecord &record : ast.nodes)
    if ((record.kind == "SgBaseClass" || record.kind == "SgExpBaseClass" ||
         record.kind == "SgNonrealBaseClass") &&
        nodes.count(record.id) == 0)
      constructBaseClassRecord(record, nodes,
                               construction_fields.at(record.id));
  validateDeclarationSpecializationFamilies(nodes);
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgSourceFile")
      continue;
    SgFileList *owner = nullptr;
    if (record.id == ast.root_id && ast.project_files_id == 0) {
      if (singleEdgeTarget(record, "parent") != 0)
        throw std::runtime_error(
            "AST JSON file boundary has a serialized parent");
      owner = boundary_owner;
    } else {
      owner = nodeByIdAs<SgFileList>(
          nodes, requiredSingleEdgeTarget(record, "parent"));
    }
    SgSourceFile *file =
        createSourceFileFromRecord(record, project, owner, nodes);
    requireRestoredKind(file, record);
    nodes.emplace(record.id, file);
  }
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    requireRestoredKind(node, record);
    attachJsonNodeId(node, record.id);
  }
  restoreAvailableParentsAndScopes(ast, nodes);

  validateVariableInlineTypeOwnership(ast, nodes);
  // Field designators already own typed operands. Check their exact declaration
  // and symbol identities before edge linking.
  validateDesignatorFieldReferenceConstructionDependencies(ast, nodes,
                                                           construction_fields);
  for (const NodeRecord &record : ast.nodes) {
    linkNodeEdges(record, nodes);
  }
  linkDeclarationGroupEdges(ast, nodes);
  finalizeAuxiliaryDeclarationContainers(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    auto &fields = construction_fields.at(record.id);
    attachPreprocessingInfo(node,
                            constructPreprocessingInfo(fields.preprocessing));
    attachAstAttributes(node, std::move(fields.attributes));
  }
  restoreRecordedParentEdges(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (auto *source = isSgSourceFile(node))
      restoreResolvedIncludeDirectives(
          source, record.properties.at("frontend_resolved_include_directives"),
          nodes);
    if (auto *include = isSgIncludeFile(node))
      include->validateIncludeOwnership();
  }
  validateReconstructedTypesAndSymbols(ast, nodes);
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
      SgCxxLiteralTokenList *lexical =
          call->get_source_user_defined_literal_operands();
      if (literal_surface) {
        if (lexical == nullptr || lexical->get_parent() != call ||
            lexical->get_tokens().empty() ||
            lexical->get_tokens().size() !=
                call->get_source_user_defined_literal_suffix_roles().size()) {
          throw std::runtime_error(
              "AST JSON SgFunctionCallExp has malformed UDL lexical ownership");
        }
        for (SgCxxLiteralToken *operand : lexical->get_tokens()) {
          if (operand == nullptr || operand->get_parent() != lexical) {
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
      const uint64_t clause_id =
          singleEdgeTarget(record, "trailingRequiresClause");
      auto *clause =
          clause_id == 0 ? nullptr : nodeByIdAs<SgExpression>(nodes, clause_id);
      if (declaration->get_trailingRequiresClause() != clause ||
          (clause != nullptr && clause->get_parent() != declaration))
        throw std::runtime_error(
            "AST JSON trailing constraint differs from construction");
      const uint64_t condition_id =
          singleEdgeTarget(record, "explicit_specifier_expression");
      auto *condition = condition_id == 0
                            ? nullptr
                            : nodeByIdAs<SgExpression>(nodes, condition_id);
      if (declaration->get_explicit_specifier_expression() != condition ||
          (condition != nullptr && condition->get_parent() != declaration))
        throw std::runtime_error(
            "AST JSON explicit condition differs from construction");
      declaration->validate_explicit_specifier();
      SgExpression *launch_bounds =
          declaration->get_cuda_launch_bounds_expression();
      if (launch_bounds != nullptr &&
          (launch_bounds->get_parent() != declaration ||
           !SgFunctionDeclaration::cuda_launch_bounds_operands_are_valid(
               launch_bounds))) {
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
    } else if (SgTypeIdOp *type_id = isSgTypeIdOp(node)) {
      (void)type_id->get_type();
    }
  }
  for (const NodeRecord &record : ast.nodes) {
    SgNonrealRefExp *reference = isSgNonrealRefExp(nodeById(nodes, record.id));
    if (reference == nullptr)
      continue;
    SgNonrealDecl *spelling = reference->get_symbol() != nullptr
                                  ? reference->get_symbol()->get_declaration()
                                  : nullptr;
    validateNonrealResolvedTargetIdentity<SgDeclarationStatement *>(
        reference->get_resolved_function_declaration(),
        reference->get_resolved_variable_declaration(),
        spelling != nullptr ? spelling->get_templateDeclaration() : nullptr);
    if (reference->get_resolved_function_declaration() != nullptr) {
      SageInterface::requireResolvedFunctionTemplateReference(
          reference, "AST JSON deserialization");
    } else if (reference->get_resolved_variable_declaration() != nullptr) {
      SageInterface::requireResolvedVariableTemplateReference(
          reference, "AST JSON deserialization");
    }
  }
  validateFortranTypeContracts(ast, nodes);
  validateRestoredCombinedClauseSourceOrders(ast, nodes);
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    if (auto *task = isSgOmpTaskStatement(node)) {
      bool selector_metadata = false;
      for (SgNode *owner = task->get_parent(); owner != nullptr;
           owner = owner->get_parent()) {
        if (isSgOmpContextSelector(owner)) {
          selector_metadata = true;
          break;
        }
      }
      if (!selector_metadata)
        task->validate_semantic_data_environment();
    }
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
  for (const NodeRecord &record : ast.nodes) {
    if (SgSourceFile *source = isSgSourceFile(nodeById(nodes, record.id))) {
      restoreTokenMappings(source, record.properties.at("token_mappings"),
                           nodes);
    }
  }
  restoreFileIdMapFromMetadata(ast.metadata);
  restoreOpenMPProducerSemanticRecords(ast, nodes);
  if (restored_nodes != nullptr)
    *restored_nodes = std::move(nodes);
  file_id_maps.committed = true;
  return file;
}

} // namespace AstJson
} // namespace Rose
