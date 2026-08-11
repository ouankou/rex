#include "ompAstConstruction.h"
#include "sageAstJsonPrivate.h"
#include "tokenStreamMapping.h"

namespace Rose {
namespace AstJson {

namespace {

std::string fortranAnonymousProgramUnitSymbolKeyForJson(
    SgFunctionDeclaration *declaration) {
  if (declaration == nullptr) {
    throw std::runtime_error(
        "AST JSON function symbol-key validation received null declaration");
  }
  const bool is_fortran_program_unit =
      isSgProgramHeaderStatement(declaration) != nullptr ||
      isSgProcedureHeaderStatement(declaration) != nullptr;
  if (is_fortran_program_unit &&
      SageInterface::isFortranProgramUnitWithoutSourceName(declaration)) {
    // Validate the producer-published identity, but never serialize its
    // implementation-only symbol-table spelling. Deserialization rebuilds
    // the key from the exact typed source form and source anchor.
    SageInterface::getFortranProgramUnitSymbolTableKey(declaration);
    return std::string();
  }
  if (!declaration->get_fortran_anonymous_program_unit_symbol_key().is_null()) {
    throw std::runtime_error(
        "AST JSON named or non-Fortran function owns an anonymous program-unit "
        "symbol key");
  }
  return std::string();
}

std::optional<unsigned int>
ompDeclareVariantRegionOrdinalForJson(SgFunctionDeclaration *declaration) {
  if (declaration == nullptr) {
    throw std::runtime_error(
        "AST JSON declare-variant validation received null declaration");
  }
  const std::string source_name =
      declaration->get_omp_declare_variant_source_name().getString();
  const std::optional<unsigned int> region_ordinal =
      declaration->get_omp_declare_variant_region_ordinal();
  if (source_name.empty() != !region_ordinal.has_value()) {
    throw std::runtime_error(
        "AST JSON function has incomplete OpenMP declare-variant source "
        "identity: " +
        declaration->get_name().getString());
  }
  if (!source_name.empty() &&
      source_name == declaration->get_name().getString()) {
    throw std::runtime_error(
        "AST JSON function has a declare-variant source name equal to its "
        "semantic identity: " +
        declaration->get_name().getString());
  }
  return region_ordinal;
}

std::string rawOpenMPProducerObject(const std::vector<std::string> &fields) {
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawOpenMPProducerArray(const std::vector<std::string> &elements) {
  std::ostringstream out;
  out << '[';
  for (size_t index = 0; index < elements.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << elements[index];
  }
  out << ']';
  return out.str();
}

int64_t openMPProducerSize(std::size_t value, const char *context) {
  if (value > static_cast<std::size_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(std::string("AST JSON OpenMP ") + context +
                             " is out of range");
  }
  return static_cast<int64_t>(value);
}

uint64_t requiredOpenMPProducerNodeId(
    const std::unordered_map<const SgNode *, uint64_t> &ids, const SgNode *node,
    const char *context) {
  if (node == nullptr) {
    return 0;
  }
  const uint64_t id = idFor(ids, node);
  if (id == 0) {
    throw std::runtime_error(std::string("AST JSON OpenMP ") + context +
                             " is outside the serialized node identity set");
  }
  return id;
}

std::string rawOpenMPSubexpressionTypes(
    const std::vector<OmpExactSubexpressionType> &subexpressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> elements;
  elements.reserve(subexpressions.size());
  for (const OmpExactSubexpressionType &subexpression : subexpressions) {
    elements.push_back(rawOpenMPProducerObject(
        {rawIntegerField("kind", static_cast<int>(subexpression.kind())),
         jsonString("result_type") + ": " +
             rawTypeJson(subexpression.resultType(), ids)}));
  }
  return rawOpenMPProducerArray(elements);
}

std::string rawOpenACCCxxExactSemanticBindings(
    const OpenACCCxxExactSemanticBindings &record,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> expressions;
  expressions.reserve(record.bindings().size());
  for (const OpenACCCxxExactSemanticBindings::ExpressionBindings &expression :
       record.bindings()) {
    std::vector<std::string> identifiers;
    identifiers.reserve(expression.identifiers().size());
    for (const OpenACCCxxExactSemanticBindings::Binding &binding :
         expression.identifiers()) {
      identifiers.push_back(rawOpenMPProducerObject(
          {rawStringField("spelling", binding.spelling()),
           rawIntegerField("kind", static_cast<int>(binding.kind())),
           rawIntegerField("semantic_node",
                           requiredOpenMPProducerNodeId(
                               ids, binding.semanticNode(),
                               "OpenACC C/C++ semantic binding node")),
           jsonString("symbol") + ": " +
               rawExactBoundSymbolRef(binding.symbol(), ids)}));
    }
    expressions.push_back(rawOpenMPProducerObject(
        {rawIntegerField("parse_mode",
                         static_cast<int>(expression.parseMode())),
         rawStringField("expression", expression.expression()),
         jsonString("identifiers") + ": " + rawOpenMPProducerArray(identifiers),
         jsonString("subexpressions") + ": " +
             rawOpenMPSubexpressionTypes(expression.subexpressions(), ids)}));
  }
  return rawOpenMPProducerObject(
      {jsonString("expressions") + ": " + rawOpenMPProducerArray(expressions)});
}

std::string rawOpenMPFortranExactSemanticBindings(
    const OmpFortranExactSemanticBindings &record,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> bindings;
  bindings.reserve(record.bindings().size());
  for (const OmpFortranExactSemanticBindings::Binding &binding :
       record.bindings()) {
    const bool value_binding =
        binding.kind() == OmpFortranExactSemanticBindings::BindingKind::value;
    const SgNode *serialized_semantic_node =
        value_binding ? nullptr : binding.semanticNode();
    bindings.push_back(rawOpenMPProducerObject(
        {rawIntegerField("source_offset",
                         openMPProducerSize(binding.sourceOffset(),
                                            "Fortran binding source offset")),
         rawIntegerField("source_size",
                         openMPProducerSize(binding.sourceSize(),
                                            "Fortran binding source size")),
         rawStringField("spelling", binding.spelling()),
         rawStringField("source_spelling", binding.sourceSpelling()),
         rawIntegerField("kind", static_cast<int>(binding.kind())),
         rawIntegerField("semantic_node",
                         requiredOpenMPProducerNodeId(
                             ids, serialized_semantic_node,
                             "Fortran exact semantic binding node")),
         jsonString("symbol") + ": " +
             rawExactBoundSymbolRef(binding.symbol(), ids),
         jsonString("directive_local_type") + ": " +
             rawTypeJson(binding.directiveLocalType(), ids)}));
  }

  std::vector<std::string> expressions;
  expressions.reserve(record.expressions().size());
  for (const OmpFortranExactSemanticBindings::ExpressionTypes &expression :
       record.expressions()) {
    expressions.push_back(rawOpenMPProducerObject(
        {rawIntegerField(
             "source_offset",
             openMPProducerSize(expression.sourceOffset(),
                                "Fortran expression source offset")),
         rawIntegerField("source_size",
                         openMPProducerSize(expression.sourceSize(),
                                            "Fortran expression source size")),
         rawStringField("expression", expression.expression()),
         jsonString("subexpressions") + ": " +
             rawOpenMPSubexpressionTypes(expression.subexpressions(), ids)}));
  }
  return rawOpenMPProducerObject(
      {rawIntegerField("producer", static_cast<int>(record.producer())),
       rawStringField("directive_source", record.directiveSource()),
       jsonString("default_integer_type") + ": " +
           rawTypeJson(record.defaultIntegerType(), ids),
       jsonString("bindings") + ": " + rawOpenMPProducerArray(bindings),
       jsonString("expressions") + ": " + rawOpenMPProducerArray(expressions)});
}

std::string rawOpenMPProducerSemanticRecords(
    SgPragmaDeclaration *pragma,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  OpenMPProducerSemanticRecords records =
      OmpSupport::snapshotOpenMPProducerSemanticRecords(pragma);
  const std::string exact_bindings =
      records.openacc_cxx_semantic_bindings.has_value()
          ? rawOpenACCCxxExactSemanticBindings(
                *records.openacc_cxx_semantic_bindings, ids)
          : "null";
  const std::string fortran_bindings =
      records.fortran_exact_semantic_bindings.has_value()
          ? rawOpenMPFortranExactSemanticBindings(
                *records.fortran_exact_semantic_bindings, ids)
          : "null";
  return rawOpenMPProducerObject(
      {jsonString("openacc_cxx_semantic_bindings") + ": " + exact_bindings,
       jsonString("fortran_exact_semantic_bindings") + ": " +
           fortran_bindings});
}

std::string rawSourceOperatorOperandRolesJson(const SgUnsignedCharList &roles) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < roles.size(); ++index) {
    const unsigned char role = roles[index];
    if (role != SgFunctionCallExp::e_source_operator_operand &&
        role != SgFunctionCallExp::e_semantic_operator_operand) {
      throw std::runtime_error(
          "AST JSON SgFunctionCallExp has an invalid source operator operand "
          "role");
    }
    if (index != 0) {
      out << ", ";
    }
    out << static_cast<unsigned int>(role);
  }
  out << "]";
  return out.str();
}

std::string rawUdlSuffixRolesJson(const SgUnsignedCharList &roles) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < roles.size(); ++index) {
    const unsigned char role = roles[index];
    if (role !=
            SgFunctionCallExp::e_user_defined_literal_token_without_suffix &&
        role != SgFunctionCallExp::e_user_defined_literal_token_with_suffix) {
      throw std::runtime_error(
          "AST JSON SgFunctionCallExp has an invalid UDL suffix role");
    }
    if (index != 0) {
      out << ", ";
    }
    out << static_cast<unsigned int>(role);
  }
  out << "]";
  return out.str();
}

std::string
rawOmpDirectiveKindsJson(const SgOmpClause::omp_directive_kind_list &kinds) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < kinds.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << static_cast<int>(kinds[index]);
  }
  out << "]";
  return out.str();
}

uint64_t requiredTokenMappingNodeId(
    const std::unordered_map<const SgNode *, uint64_t> &ids, const SgNode *node,
    const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON token mapping has a null " + context);
  }
  const uint64_t id = idFor(ids, node);
  if (id == 0) {
    throw std::runtime_error("AST JSON token mapping " + context + " " +
                             node->sage_class_name() +
                             " is outside the serialized source file");
  }
  return id;
}

std::string rawTokenMappingNodeIdsJson(
    const std::vector<SgNode *> &nodes,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < nodes.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << requiredTokenMappingNodeId(ids, nodes[index], "node_vector entry");
  }
  out << "]";
  return out.str();
}

std::string
rawTokenMappingsJson(SgSourceFile *file,
                     const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON token mapping serialization requires a source file");
  }

  struct Entry {
    uint64_t node_id;
    TokenStreamSequenceToNodeMapping *mapping;
  };
  std::vector<Entry> entries;
  for (const auto &entry : file->get_tokenSubsequenceMap()) {
    if (entry.second == nullptr) {
      throw std::runtime_error("AST JSON token map contains a null mapping");
    }
    entries.push_back({requiredTokenMappingNodeId(ids, entry.first, "map key"),
                       entry.second});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry &lhs, const Entry &rhs) {
              return lhs.node_id < rhs.node_id;
            });
  for (size_t index = 1; index < entries.size(); ++index) {
    if (entries[index - 1].node_id == entries[index].node_id) {
      throw std::runtime_error(
          "AST JSON token map contains duplicate serialized node IDs");
    }
  }

  std::unordered_map<TokenStreamSequenceToNodeMapping *, uint64_t> mapping_ids;
  std::vector<TokenStreamSequenceToNodeMapping *> mappings;
  for (const Entry &entry : entries) {
    if (mapping_ids.find(entry.mapping) == mapping_ids.end()) {
      const uint64_t mapping_id = mappings.size() + 1;
      mapping_ids.emplace(entry.mapping, mapping_id);
      mappings.push_back(entry.mapping);
    }
  }

  std::ostringstream entries_json;
  entries_json << "[";
  if (!entries.empty()) {
    entries_json << '\n';
    for (size_t index = 0; index < entries.size(); ++index) {
      const Entry &entry = entries[index];
      std::vector<std::string> fields;
      fields.push_back(rawIntegerField("node", entry.node_id));
      fields.push_back(
          rawIntegerField("mapping", mapping_ids.at(entry.mapping)));
      indent(entries_json, 4);
      writeRawObject(entries_json, 0, fields, false);
      if (index + 1 != entries.size()) {
        entries_json << ',';
      }
      entries_json << '\n';
    }
    indent(entries_json, 2);
  }
  entries_json << "]";

  std::ostringstream mappings_json;
  mappings_json << "[";
  if (!mappings.empty()) {
    mappings_json << '\n';
    for (size_t index = 0; index < mappings.size(); ++index) {
      TokenStreamSequenceToNodeMapping *mapping = mappings[index];
      std::vector<std::string> fields;
      fields.push_back(rawIntegerField("id", index + 1));
      fields.push_back(
          rawIntegerField("node", requiredTokenMappingNodeId(ids, mapping->node,
                                                             "mapping owner")));
      const TokenStreamHalfOpenInterval &leading = mapping->halfOpenInterval(
          TokenStreamIntervalKind::leading_whitespace);
      const TokenStreamHalfOpenInterval &core =
          mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
      const TokenStreamHalfOpenInterval &trailing = mapping->halfOpenInterval(
          TokenStreamIntervalKind::trailing_whitespace);
      const TokenStreamHalfOpenInterval &else_interval =
          mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
      fields.push_back(
          rawIntegerField("leading_whitespace_begin", leading.begin));
      fields.push_back(rawIntegerField("leading_whitespace_end", leading.end));
      fields.push_back(rawIntegerField("token_subsequence_begin", core.begin));
      fields.push_back(rawIntegerField("token_subsequence_end", core.end));
      fields.push_back(
          rawIntegerField("trailing_whitespace_begin", trailing.begin));
      fields.push_back(
          rawIntegerField("trailing_whitespace_end", trailing.end));
      fields.push_back(
          rawIntegerField("else_whitespace_begin", else_interval.begin));
      fields.push_back(
          rawIntegerField("else_whitespace_end", else_interval.end));
      fields.push_back(rawBoolField("shared", mapping->shared));
      fields.push_back(
          rawBoolField("constructed_in_synthesized_attribute",
                       mapping->constructedInEvaluationOfSynthesizedAttribute));
      fields.push_back(jsonString("node_vector") + ": " +
                       rawTokenMappingNodeIdsJson(mapping->nodeVector, ids));
      indent(mappings_json, 4);
      writeRawObject(mappings_json, 0, fields, false);
      if (index + 1 != mappings.size()) {
        mappings_json << ',';
      }
      mappings_json << '\n';
    }
    indent(mappings_json, 2);
  }
  mappings_json << "]";

  std::vector<std::string> fields;
  fields.push_back(jsonString("entries") + ": " + entries_json.str());
  fields.push_back(jsonString("mappings") + ": " + mappings_json.str());
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

} // namespace

std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", decl != nullptr));
  if (decl != nullptr) {
    fields.push_back(rawOptionalUnsignedIntegerField(
        "translation_unit_source_order", std::nullopt));
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", decl->get_class_type()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForNode(decl)));
    fields.push_back(rawStringField("module_name", moduleNameForNode(decl)));
    fields.push_back(
        rawBoolField("has_definition", classDeclarationHasDefinition(decl)));
    fields.push_back(rawBoolField("is_first_nondefining",
                                  classDeclarationIsFirstNondefining(decl)));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  const bool external = module != nullptr && (isAstJsonExternalModule(module) ||
                                              idFor(ids, module) == 0);
  fields.push_back(rawBoolField("present", external));
  if (external) {
    fields.push_back(rawOptionalUnsignedIntegerField(
        "translation_unit_source_order", std::nullopt));
    fields.push_back(rawStringField("name", module->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", module->get_class_type()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForNode(module)));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalFunctionParameterScopeJson(
    SgFunctionDeclaration *function, SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
std::string rawExternalInitializedNameJson(
    SgInitializedName *name,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);

std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external) {
  std::vector<std::string> fields;
  const bool external =
      decl != nullptr &&
      (force_external || isAstJsonExternalFunction(decl) ||
       (idFor(ids, decl) == 0 && !insideCollectionBoundary(decl)));
  fields.push_back(rawBoolField("present", external));
  if (external) {
    if (decl->get_source_declarator_uses_wrapped_function_type()) {
      decl->validate_source_declarator_form();
    }
    fields.push_back(rawOptionalUnsignedIntegerField(
        "translation_unit_source_order", std::nullopt));
    if (isSgProcedureHeaderStatement(decl) != nullptr) {
      SageInterface::isFortranProgramUnitWithoutSourceName(decl);
    } else if (decl->get_name().is_null()) {
      throw std::runtime_error(
          "AST JSON external non-Fortran function has an empty name");
    }
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawStringField(
        "omp_declare_variant_source_name",
        decl->get_omp_declare_variant_source_name().getString()));
    fields.push_back(rawOptionalUnsignedIntegerField(
        "omp_declare_variant_region_ordinal",
        ompDeclareVariantRegionOrdinalForJson(decl)));
    fields.push_back(
        rawStringField("fortran_anonymous_program_unit_symbol_key",
                       fortranAnonymousProgramUnitSymbolKeyForJson(decl)));
    fields.push_back(
        rawBoolField("source_name_parenthesized_for_macro",
                     decl->get_source_name_parenthesized_for_macro()));
    fields.push_back(
        rawBoolField("source_declarator_uses_wrapped_function_type",
                     decl->get_source_declarator_uses_wrapped_function_type()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForExternalFunction(decl)));
    fields.push_back(
        jsonString("location") + ": " +
        rawRequiredLocationJson(decl, "external_function " +
                                          decl->get_name().getString()));
    SgFunctionParameterList *parameter_list = decl->get_parameterList();
    if (parameter_list == nullptr) {
      throw std::runtime_error("AST JSON external_function " +
                               decl->get_name().getString() +
                               " requires a parameterList");
    }
    fields.push_back(jsonString("parameter_list_location") + ": " +
                     rawRequiredLocationJson(
                         parameter_list, "external_function parameterList " +
                                             decl->get_name().getString()));
    SgFunctionParameterList *syntax_list = decl->get_parameterList_syntax();
    if (syntax_list != nullptr && syntax_list != parameter_list) {
      throw std::runtime_error(
          "AST JSON external_function " + decl->get_name().getString() +
          " has a distinct parameterList_syntax that is not yet serialized");
    }
    fields.push_back(
        rawBoolField("parameter_list_syntax_aliases_parameter_list",
                     syntax_list == parameter_list));
    std::ostringstream parameters;
    parameters << jsonString("parameters") << ": [";
    const SgInitializedNamePtrList &args = parameter_list->get_args();
    if (!args.empty()) {
      parameters << '\n';
      for (size_t i = 0; i < args.size(); ++i) {
        SgInitializedName *parameter = args[i];
        if (parameter == nullptr || parameter->get_parent() != parameter_list ||
            parameter->get_declptr() != decl ||
            std::count(args.begin(), args.end(), parameter) != 1) {
          throw std::runtime_error("AST JSON external_function " +
                                   decl->get_name().getString() +
                                   " has malformed exact parameter ownership");
        }
        indent(parameters, 4);
        parameters << rawExternalInitializedNameJson(
            parameter, ids, decl->get_name().getString());
        if (i + 1 != args.size()) {
          parameters << ',';
        }
        parameters << '\n';
      }
      indent(parameters, 2);
    }
    parameters << "]";
    fields.push_back(parameters.str());
    fields.push_back(jsonString("function_parameter_scope") + ": " +
                     rawExternalFunctionParameterScopeJson(
                         decl, decl->get_functionParameterScope(), ids,
                         decl->get_name().getString()));
    fields.push_back(jsonString("function_type") + ": " +
                     rawTypeJson(decl->get_type(), ids));
    if (decl->get_type_syntax_is_available() !=
            (decl->get_type_syntax() != nullptr) ||
        (decl->get_type_syntax() != nullptr &&
         (decl->get_type_syntax() == decl->get_type() ||
          decl->get_type_syntax()->get_parent() != decl))) {
      throw std::runtime_error("AST JSON external_function " +
                               decl->get_name().getString() +
                               " has inconsistent or unowned function type "
                               "syntax state");
    }
    fields.push_back(jsonString("function_type_syntax") + ": " +
                     rawTypeJson(decl->get_type_syntax(), ids));
    fields.push_back(rawBoolField("type_syntax_is_available",
                                  decl->get_type_syntax_is_available()));
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(decl)) {
      SageInterface::isFortranProgramUnitWithoutSourceName(procedure);
      fields.push_back(
          rawIntegerField("subprogram_kind", procedure->get_subprogram_kind()));
      fields.push_back(rawIntegerField("block_data_name_kind",
                                       procedure->get_block_data_name_kind()));
      fields.push_back(
          rawIntegerField("fortran_procedure_source_form",
                          procedure->get_fortran_procedure_source_form()));
      fields.push_back(
          rawIntegerField("fortran_result_type_spec",
                          procedure->get_fortran_result_type_spec()));
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool declarationNeedsExternalReferenceRecord(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return decl != nullptr && idFor(ids, decl) == 0 &&
         (hasExternalMarkerAncestor(decl) ||
          ((isSgFunctionDeclaration(decl) != nullptr ||
            isSgModuleStatement(decl) != nullptr ||
            isSgClassDeclaration(decl) != nullptr) &&
           !isStructuralAstChildOfParent(decl)));
}

std::string externalFunctionParameterScopeSource(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (decl == nullptr || decl->get_functionParameterScope() == nullptr ||
      idFor(ids, decl->get_functionParameterScope()) != 0) {
    return "";
  }

  auto has_external_scope = [&](SgDeclarationStatement *candidate) -> bool {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(candidate);
    return function != nullptr &&
           function->get_functionParameterScope() ==
               decl->get_functionParameterScope() &&
           declarationNeedsExternalReferenceRecord(function, ids);
  };

  if (has_external_scope(decl->get_firstNondefiningDeclaration())) {
    return "firstNondefiningDeclaration";
  }
  if (has_external_scope(decl->get_definingDeclaration())) {
    return "definingDeclaration";
  }
  return "";
}

bool functionParameterScopeNeedsExternalReferenceRecord(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return !externalFunctionParameterScopeSource(decl, ids).empty();
}

std::string rawExternalDeclarationReferenceJson(
    SgDeclarationStatement *decl, SgDeclarationStatement *owner,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  const bool present = declarationNeedsExternalReferenceRecord(decl, ids);
  fields.push_back(rawBoolField("present", present));
  if (present) {
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    SgScopeStatement *scope = decl->get_scope();
    if (scope == nullptr) {
      throw std::runtime_error(
          std::string("AST JSON external redeclaration peer has no semantic ") +
          "scope: " + decl->sage_class_name());
    }
    const uint64_t scope_id = idFor(ids, scope);
    std::string scope_source;
    if (scope_id != 0) {
      scope_source = "node";
    } else if (owner != nullptr && scope == owner->get_scope()) {
      scope_source = "redeclaration_owner";
    } else {
      throw std::runtime_error(
          std::string("AST JSON external redeclaration peer has an "
                      "unrepresentable semantic scope: ") +
          decl->sage_class_name());
    }
    fields.push_back(rawStringField("scope_source", scope_source));
    fields.push_back(rawIntegerField("scope", scope_id));

    SgNode *parent = decl->get_parent();
    if (parent == nullptr) {
      throw std::runtime_error(
          std::string(
              "AST JSON external redeclaration peer has no structural ") +
          "parent: " + decl->sage_class_name());
    }
    const uint64_t parent_id = idFor(ids, parent);
    std::string parent_source;
    if (parent_id != 0) {
      parent_source = "node";
    } else if (owner != nullptr && parent == owner->get_parent()) {
      parent_source = "redeclaration_owner_parent";
    } else if (SgProject *project = isSgProject(parent)) {
      SgSourceFile *source_file =
          owner != nullptr ? SageInterface::getEnclosingSourceFile(owner, true)
                           : nullptr;
      if (source_file == nullptr || owningProject(source_file) != project) {
        throw std::runtime_error(
            std::string("AST JSON external redeclaration peer belongs to a "
                        "different project: ") +
            decl->sage_class_name());
      }
      parent_source = "project";
    } else {
      throw std::runtime_error(
          std::string("AST JSON external redeclaration peer has an "
                      "unrepresentable structural parent: ") +
          decl->sage_class_name());
    }
    fields.push_back(rawStringField("parent_source", parent_source));
    fields.push_back(rawIntegerField("parent", parent_id));
    if (SgFunctionDeclaration *function_decl = isSgFunctionDeclaration(decl)) {
      fields.push_back(jsonString("external_function") + ": " +
                       rawExternalFunctionJson(function_decl, ids, true));
    } else if (SgModuleStatement *module = isSgModuleStatement(decl)) {
      fields.push_back(jsonString("external_module") + ": " +
                       rawExternalModuleJson(module, ids));
    } else if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      fields.push_back(jsonString("external_class") + ": " +
                       rawExternalClassDeclarationJson(class_decl));
    } else {
      throw std::runtime_error(
          std::string("AST JSON external declaration reference has unsupported "
                      "kind: ") +
          decl->sage_class_name());
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalInitializedNameJson(
    SgInitializedName *name,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (name == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null initialized name");
  }
  if (name->get_initptr() != nullptr) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " has an initialized name with an initializer that is not yet "
        "serialized: " +
        name->get_name().getString());
  }
  if (name->get_preinitialization() !=
      SgInitializedName::e_unknown_preinitialization) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " has a parameter initialized name with a constructor "
        "preinitialization role: " +
        name->get_name().getString());
  }
  if (name->get_cray_pointer_pointee() != nullptr ||
      name->get_fortran_cray_pointer_pointee_shape() != nullptr ||
      name->get_fortran_separate_shape_declaration() != nullptr ||
      name->get_fortran_separate_pointer_declaration() != nullptr ||
      name->get_shapeDeferred()) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " has a parameter initialized name with declaration-statement "
        "shape or Cray pointer state: " +
        name->get_name().getString());
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("name", name->get_name().getString()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(name, "external_function initializedName " +
                                        function_name +
                                        "::" + name->get_name().getString()));
  fields.push_back(jsonString("type") + ": " +
                   rawTypeJson(name->get_typeptr(), ids));
  fields.push_back(jsonString("fortran_source_type") + ": " +
                   rawTypeJson(name->get_fortran_source_type(), ids));
  fields.push_back(jsonString("cxx_source_type") + ": " +
                   rawTypeJson(name->get_cxx_source_type(), ids));
  fields.push_back(rawIntegerField(
      "fortran_type_spec", static_cast<int>(name->get_fortran_type_spec())));
  fields.push_back(
      rawStringField("fortran_procedure_interface",
                     name->get_fortran_procedure_interface().getString()));
  fields.push_back(rawBoolField("is_predefined_identifier",
                                name->get_is_predefined_identifier()));
  fields.push_back(
      rawIntegerField("generated_variable_role",
                      static_cast<int>(name->get_generated_variable_role())));
  fields.push_back(rawIntegerField(
      "preinitialization", static_cast<int>(name->get_preinitialization())));
  fields.push_back(rawIntegerField(
      "storage_modifier",
      static_cast<int>(name->get_storageModifier().get_modifier())));
  fields.push_back(rawStringField("gnu_attribute_section_name",
                                  name->get_gnu_attribute_section_name()));
  fields.push_back(rawIntegerField("name_qualification_length",
                                   name->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                name->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                name->get_global_qualification_required()));
  fields.push_back(
      rawIntegerField("name_qualification_length_for_type",
                      name->get_name_qualification_length_for_type()));
  fields.push_back(
      rawBoolField("type_elaboration_required_for_type",
                   name->get_type_elaboration_required_for_type()));
  fields.push_back(
      rawBoolField("global_qualification_required_for_type",
                   name->get_global_qualification_required_for_type()));
  fields.push_back(rawBoolField("source_type_qualification_present",
                                name->get_source_type_qualification_present()));
  fields.push_back(rawBoolField("source_type_global_qualification",
                                name->get_source_type_global_qualification()));
  fields.push_back(
      jsonString("source_type_qualification_tokens") + ": " +
      rawStringListJson(name->get_source_type_qualification_tokens()));
  fields.push_back(rawBoolField("source_name_qualification_present",
                                name->get_source_name_qualification_present()));
  fields.push_back(rawBoolField("source_name_global_qualification",
                                name->get_source_name_global_qualification()));
  fields.push_back(
      jsonString("source_name_qualification_tokens") + ": " +
      rawStringListJson(name->get_source_name_qualification_tokens()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

void appendRawDeclarationTypeModifierFields(std::vector<std::string> &fields,
                                            const SgTypeModifier &modifier) {
  fields.push_back(jsonString("declaration_type_modifier_vector") + ": " +
                   rawBitVectorJson(modifier.get_modifierVector()));
  fields.push_back(rawIntegerField(
      "declaration_type_const_volatile_modifier",
      static_cast<int>(modifier.get_constVolatileModifier().get_modifier())));
  fields.push_back(rawIntegerField(
      "declaration_type_elaborated_modifier",
      static_cast<int>(modifier.get_elaboratedTypeModifier().get_modifier())));
  fields.push_back(rawIntegerField("declaration_type_gnu_attribute_alignment",
                                   modifier.get_gnu_attribute_alignment()));
  fields.push_back(rawIntegerField("declaration_type_gnu_attribute_sentinel",
                                   modifier.get_gnu_attribute_sentinel()));
  fields.push_back(rawIntegerField("declaration_type_address_space_value",
                                   modifier.get_address_space_value()));
  fields.push_back(rawIntegerField("declaration_type_vector_size",
                                   modifier.get_vector_size()));
}

void appendRawExternalDeclarationStatementFields(
    std::vector<std::string> &fields, SgDeclarationStatement *decl,
    const std::string &context) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a declaration statement");
  }
  SgDeclarationStatement *first_nondefining =
      decl->get_firstNondefiningDeclaration();
  SgDeclarationStatement *defining = decl->get_definingDeclaration();
  if (first_nondefining != nullptr && first_nondefining != decl) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a firstNondefiningDeclaration that is not self or null");
  }
  if (defining != nullptr && defining != decl) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a definingDeclaration that is not self or null");
  }
  if (decl->get_declarationScope() != nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " has a declarationScope that is not yet "
                             "serialized for nested external declarations");
  }
  if (decl->get_nonreal_decl_scope() != nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " has a nonreal_decl_scope that is not yet "
                             "serialized for nested external declarations");
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(decl);
      function != nullptr &&
      function->get_function_declarator_scope() != nullptr) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a function_declarator_scope that is not yet serialized for "
        "nested external declarations");
  }

  const SgStorageModifier &storage =
      decl->get_declarationModifier().get_storageModifier();
  fields.push_back(rawOptionalUnsignedIntegerField(
      "translation_unit_source_order",
      decl->get_translation_unit_source_order()));
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(decl)) {
    fields.push_back(rawIntegerField(
        "frontend_source_ownership",
        static_cast<int>(function->get_frontend_source_ownership())));
    fields.push_back(rawIntegerField(
        "frontend_declaration_origin",
        static_cast<int>(function->get_frontend_declaration_origin())));
  }
  fields.push_back(
      rawIntegerField("decl_attributes", decl->get_decl_attributes()));
  fields.push_back(rawStringField("linkage", decl->get_linkage()));
  fields.push_back(
      jsonString("declaration_modifier_vector") + ": " +
      rawBitVectorJson(decl->get_declarationModifier().get_modifierVector()));
  appendRawDeclarationTypeModifierFields(
      fields, decl->get_declarationModifier().get_typeModifier());
  fields.push_back(rawIntegerField("declaration_storage_modifier",
                                   static_cast<int>(storage.get_modifier())));
  fields.push_back(rawBoolField("declaration_thread_local_storage",
                                storage.get_thread_local_storage()));
  fields.push_back(
      rawIntegerField("declaration_access_modifier",
                      static_cast<int>(decl->get_declarationModifier()
                                           .get_accessModifier()
                                           .get_modifier())));
  fields.push_back(rawBoolField(
      "declaration_access_is_explicit",
      decl->get_declarationModifier().get_accessModifier().get_is_explicit()));
  fields.push_back(rawBoolField("name_only", decl->get_nameOnly()));
  fields.push_back(rawBoolField("forward", decl->get_forward()));
  fields.push_back(rawBoolField("extern_brace", decl->get_externBrace()));
  fields.push_back(
      rawBoolField("skip_elaborate_type", decl->get_skipElaborateType()));
  fields.push_back(rawStringField("binding_label", decl->get_binding_label()));
  fields.push_back(
      rawBoolField("binding_cdefined", decl->get_binding_cdefined()));
  fields.push_back(
      rawBoolField("unparse_template_ast", decl->get_unparse_template_ast()));
  fields.push_back(rawBoolField("source_name_qualification_present",
                                decl->get_source_name_qualification_present()));
  fields.push_back(rawBoolField("source_name_global_qualification",
                                decl->get_source_name_global_qualification()));
  fields.push_back(
      jsonString("source_name_qualification_tokens") + ": " +
      rawStringListJson(decl->get_source_name_qualification_tokens()));
  fields.push_back(rawIntegerField(
      "declaration_gnu_attribute_visibility",
      static_cast<int>(
          decl->get_declarationModifier().get_gnu_attribute_visibility())));
  fields.push_back(rawIntegerField(
      "declaration_gnu_type_visibility",
      static_cast<int>(
          decl->get_declarationModifier().get_gnu_type_visibility())));
  fields.push_back(
      rawBoolField("first_nondefining_is_self", first_nondefining == decl));
  fields.push_back(
      rawBoolField("first_nondefining_is_null", first_nondefining == nullptr));
  fields.push_back(
      rawBoolField("defining_declaration_is_self", defining == decl));
  fields.push_back(
      rawBoolField("defining_declaration_is_null", defining == nullptr));
}

std::string rawExternalVariableDeclarationJson(
    SgVariableDeclaration *variable,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (variable == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null parameter-scope variable "
                             "declaration");
  }
  if (variable->get_baseTypeNondefiningDeclaration() != nullptr ||
      variable->get_baseTypeDefiningDeclaration() != nullptr) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " parameter-scope variable declaration has an owned base tag "
        "declaration that is not yet serialized");
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", variable->sage_class_name()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(
          variable, "external_function variableDeclaration " + function_name));
  appendRawExternalDeclarationStatementFields(
      fields, variable,
      "external_function " + function_name + " variableDeclaration");
  fields.push_back(
      rawBoolField("requires_global_name_qualification_on_type",
                   variable->get_requiresGlobalNameQualificationOnType()));
  fields.push_back(rawIntegerField("name_qualification_length",
                                   variable->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                variable->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                variable->get_global_qualification_required()));
  std::ostringstream variables;
  variables << jsonString("variables") << ": [";
  const SgInitializedNamePtrList &names = variable->get_variables();
  if (!names.empty()) {
    variables << '\n';
    for (size_t i = 0; i < names.size(); ++i) {
      indent(variables, 4);
      variables << rawExternalInitializedNameJson(names[i], ids, function_name);
      if (i + 1 != names.size()) {
        variables << ',';
      }
      variables << '\n';
    }
    indent(variables, 2);
  }
  variables << "]";
  fields.push_back(variables.str());
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalRenamePairJson(SgRenamePair *rename,
                                      const std::string &function_name) {
  if (rename == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null SgUseStatement rename pair");
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", rename->sage_class_name()));
  fields.push_back(jsonString("location") + ": " + rawLocationJson(rename));
  fields.push_back(
      rawStringField("local_name", rename->get_local_name().getString()));
  fields.push_back(
      rawStringField("use_name", rename->get_use_name().getString()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalUseStatementJson(
    SgUseStatement *stmt,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (stmt == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null parameter-scope use statement");
  }

  const uint64_t module_id = idFor(ids, stmt->get_module());
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", stmt->sage_class_name()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(stmt, "external_function useStatement " +
                                        function_name));
  appendRawExternalDeclarationStatementFields(
      fields, stmt, "external_function " + function_name + " useStatement");
  fields.push_back(rawStringField("name", stmt->get_name().getString()));
  fields.push_back(rawBoolField("only_option", stmt->get_only_option()));
  fields.push_back(rawStringField("module_nature", stmt->get_module_nature()));
  fields.push_back(rawIntegerField("module", module_id));
  fields.push_back(jsonString("external_module") + ": " +
                   rawExternalModuleJson(stmt->get_module(), ids));
  std::ostringstream rename_list;
  rename_list << jsonString("rename_list") << ": [";
  const SgRenamePairPtrList &renames = stmt->get_rename_list();
  if (!renames.empty()) {
    rename_list << '\n';
    for (size_t i = 0; i < renames.size(); ++i) {
      indent(rename_list, 4);
      rename_list << rawExternalRenamePairJson(renames[i], function_name);
      if (i + 1 != renames.size()) {
        rename_list << ',';
      }
      rename_list << '\n';
    }
    indent(rename_list, 2);
  }
  rename_list << "]";
  fields.push_back(rename_list.str());

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalParameterScopeDeclarationJson(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
    return rawExternalVariableDeclarationJson(variable, ids, function_name);
  }
  if (SgUseStatement *stmt = isSgUseStatement(decl)) {
    return rawExternalUseStatementJson(stmt, ids, function_name);
  }
  throw std::runtime_error("AST JSON external_function " + function_name +
                           " functionParameterScope declaration kind is not "
                           "supported: " +
                           (decl != nullptr
                                ? std::string(decl->sage_class_name())
                                : std::string("<null>")));
}

std::string rawExternalFunctionParameterScopeJson(
    SgFunctionDeclaration *function, SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", scope != nullptr));
  if (scope != nullptr) {
    if (function == nullptr || scope->get_parent() != function ||
        function->get_functionParameterScope() != scope ||
        function->get_parameterList() == nullptr) {
      throw std::runtime_error("AST JSON external_function " + function_name +
                               " has malformed parameter-scope ownership");
    }
    std::map<SgInitializedName *, size_t> parameter_indices;
    const SgInitializedNamePtrList &parameters =
        function->get_parameterList()->get_args();
    for (size_t i = 0; i < parameters.size(); ++i) {
      SgInitializedName *parameter = parameters[i];
      if (parameter == nullptr ||
          parameter->get_parent() != function->get_parameterList() ||
          parameter->get_declptr() != function ||
          parameter->get_scope() != scope ||
          !parameter_indices.emplace(parameter, i).second) {
        std::ostringstream message;
        message << "AST JSON external_function " << function_name
                << " parameter scope has malformed exact parameter ownership"
                << " function=" << function << " scope=" << scope
                << " parameter-list=" << function->get_parameterList()
                << " index=" << i << " parameter=" << parameter;
        if (parameter != nullptr) {
          SgScopeStatement *actual_parameter_scope = parameter->get_scope();
          message
              << " parent=" << parameter->get_parent()
              << " declaration=" << parameter->get_declptr()
              << " parameter-scope=" << actual_parameter_scope << "/"
              << (actual_parameter_scope != nullptr
                      ? actual_parameter_scope->sage_class_name()
                      : "<null>")
              << " parameter-scope-parent="
              << (actual_parameter_scope != nullptr
                      ? actual_parameter_scope->get_parent()
                      : nullptr)
              << "/"
              << (actual_parameter_scope != nullptr &&
                          actual_parameter_scope->get_parent() != nullptr
                      ? actual_parameter_scope->get_parent()->sage_class_name()
                      : "<null>")
              << " function-semantic-scope=" << function->get_scope()
              << " function-first="
              << function->get_firstNondefiningDeclaration()
              << " function-defining=" << function->get_definingDeclaration()
              << " name=" << parameter->get_name().getString();
        }
        throw std::runtime_error(message.str());
      }
    }
    std::map<SgInitializedName *, std::pair<size_t, size_t>> variable_indices;
    std::map<SgDeclarationStatement *, size_t> declaration_indices;
    std::ostringstream declarations;
    declarations << jsonString("declarations") << ": [";
    const SgDeclarationStatementPtrList &decls = scope->get_declarations();
    if (!decls.empty()) {
      declarations << '\n';
      for (size_t i = 0; i < decls.size(); ++i) {
        if (decls[i] != nullptr) {
          declaration_indices[decls[i]] = i;
        }
        SgVariableDeclaration *variable = isSgVariableDeclaration(decls[i]);
        if (variable != nullptr) {
          const SgInitializedNamePtrList &names = variable->get_variables();
          for (size_t j = 0; j < names.size(); ++j) {
            if (names[j] != nullptr) {
              variable_indices[names[j]] = std::make_pair(i, j);
            }
          }
        }
        indent(declarations, 4);
        declarations << rawExternalParameterScopeDeclarationJson(decls[i], ids,
                                                                 function_name);
        if (i + 1 != decls.size()) {
          declarations << ',';
        }
        declarations << '\n';
      }
      indent(declarations, 2);
    }
    declarations << "]";
    SgSymbolTable *table = scope->get_symbol_table();
    const size_t table_size = table != nullptr && table->get_table() != nullptr
                                  ? table->get_table()->size()
                                  : 0;
    fields.push_back(jsonString("location") + ": " +
                     rawRequiredLocationJson(
                         scope, "external_function functionParameterScope " +
                                    function_name));
    fields.push_back(declarations.str());
    fields.push_back(
        rawIntegerField("symbol_table_size", static_cast<int64_t>(table_size)));
    fields.push_back(rawBoolField("symbol_table_present", table != nullptr));
    if (table != nullptr) {
      fields.push_back(rawBoolField("symbol_table_case_insensitive",
                                    table->isCaseInsensitive()));
      std::ostringstream entries;
      entries << jsonString("symbol_table") << ": [";
      if (table->get_table() != nullptr && !table->get_table()->empty()) {
        std::vector<SymbolTableEntryJson> serialized_entries;
        for (const std::pair<const SgName, SgSymbol *> &entry :
             *table->get_table()) {
          SgSymbol *symbol = entry.second;
          if (symbol == nullptr) {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope symbol table has a null entry: " +
                entry.first.getString());
          }
          SgVariableSymbol *variable_symbol = isSgVariableSymbol(symbol);
          SgInitializedName *declaration =
              variable_symbol != nullptr ? variable_symbol->get_declaration()
                                         : nullptr;
          auto parameter_index = parameter_indices.find(declaration);
          auto index = variable_indices.find(declaration);
          std::vector<std::string> entry_fields;
          entry_fields.push_back(
              rawStringField("entry_name", entry.first.getString()));
          entry_fields.push_back(
              rawStringField("symbol_kind", symbol->class_name()));
          entry_fields.push_back(rawBoolField(
              "lookup_preferred",
              symbolIsLookupPreferred(table, entry.first, symbol)));
          if (variable_symbol != nullptr &&
              parameter_index != parameter_indices.end()) {
            entry_fields.push_back(
                rawIntegerField("parameter_index",
                                static_cast<int64_t>(parameter_index->second)));
          } else if (variable_symbol != nullptr &&
                     index != variable_indices.end()) {
            entry_fields.push_back(
                rawIntegerField("declaration_index",
                                static_cast<int64_t>(index->second.first)));
            entry_fields.push_back(rawIntegerField(
                "variable_index", static_cast<int64_t>(index->second.second)));
          } else {
            entry_fields.push_back(jsonString("symbol") + ": " +
                                   rawSymbolRef(symbol, ids));
          }
          if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
            if (alias->get_alias() == nullptr) {
              throw std::runtime_error("AST JSON external_function " +
                                       function_name +
                                       " functionParameterScope SgAliasSymbol "
                                       "has no alias target: " +
                                       entry.first.getString());
            }
            entry_fields.push_back(jsonString("alias_target") + ": " +
                                   rawSymbolRef(alias->get_alias(), ids));
            entry_fields.push_back(
                rawBoolField("alias_is_renamed", alias->get_isRenamed()));
            entry_fields.push_back(rawStringField(
                "alias_new_name", alias->get_new_name().getString()));
            std::ostringstream causal_nodes;
            causal_nodes << jsonString("alias_causal_nodes") << ": [";
            const SgNodePtrList &nodes = alias->get_causal_nodes();
            if (nodes.empty()) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope SgAliasSymbol has no causal "
                  "provenance: " +
                  entry.first.getString());
            }
            if (!nodes.empty()) {
              causal_nodes << '\n';
              for (size_t i = 0; i < nodes.size(); ++i) {
                SgNode *causal_node = nodes[i];
                const uint64_t causal_id = idFor(ids, causal_node);
                auto declaration_index = declaration_indices.find(
                    isSgDeclarationStatement(causal_node));
                if (causal_node == nullptr ||
                    (causal_id == 0 &&
                     declaration_index == declaration_indices.end())) {
                  throw std::runtime_error(
                      "AST JSON external_function " + function_name +
                      " functionParameterScope SgAliasSymbol causal node is "
                      "null, uncollected, and not a nested declaration");
                }
                std::vector<std::string> causal_fields;
                causal_fields.push_back(
                    rawIntegerField("node", static_cast<int64_t>(causal_id)));
                causal_fields.push_back(rawIntegerField(
                    "declaration_index",
                    declaration_index != declaration_indices.end()
                        ? static_cast<int64_t>(declaration_index->second)
                        : static_cast<int64_t>(-1)));
                indent(causal_nodes, 6);
                writeRawObject(causal_nodes, 6, causal_fields, false);
                if (i + 1 != nodes.size()) {
                  causal_nodes << ',';
                }
                causal_nodes << '\n';
              }
              indent(causal_nodes, 4);
            }
            causal_nodes << "]";
            entry_fields.push_back(causal_nodes.str());
          }
          if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
            if (rename->get_original_symbol() == nullptr) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope "
                  "SgRenameSymbol has no original symbol: " +
                  entry.first.getString());
            }
            entry_fields.push_back(
                jsonString("original_symbol") + ": " +
                rawSymbolRef(rename->get_original_symbol(), ids));
            entry_fields.push_back(rawStringField(
                "rename_new_name", rename->get_new_name().getString()));
          }
          std::ostringstream entry_out;
          writeRawObject(entry_out, 0, entry_fields, false);
          std::string entry_json = entry_out.str();
          if (!entry_json.empty() && entry_json.back() == '\n') {
            entry_json.pop_back();
          }

          SymbolTableEntryJson serialized;
          serialized.entry_name = entry.first.getString();
          serialized.symbol_kind = symbol->class_name();
          serialized.basis_id = idFor(ids, symbolBasis(symbol));
          serialized.lookup_preferred =
              symbolIsLookupPreferred(table, entry.first, symbol);
          serialized.json = std::move(entry_json);
          serialized_entries.push_back(std::move(serialized));
        }

        std::stable_sort(serialized_entries.begin(), serialized_entries.end(),
                         [](const SymbolTableEntryJson &lhs,
                            const SymbolTableEntryJson &rhs) {
                           if (lhs.basis_id != rhs.basis_id) {
                             if (lhs.basis_id == 0) {
                               return false;
                             }
                             if (rhs.basis_id == 0) {
                               return true;
                             }
                             return lhs.basis_id < rhs.basis_id;
                           }
                           if (lhs.lookup_preferred != rhs.lookup_preferred) {
                             return lhs.lookup_preferred &&
                                    !rhs.lookup_preferred;
                           }
                           if (lhs.entry_name != rhs.entry_name) {
                             return lhs.entry_name < rhs.entry_name;
                           }
                           return lhs.symbol_kind < rhs.symbol_kind;
                         });

        entries << '\n';
        for (size_t i = 0; i < serialized_entries.size(); ++i) {
          indent(entries, 4);
          entries << serialized_entries[i].json;
          if (i + 1 != serialized_entries.size()) {
            entries << ',';
          }
          entries << '\n';
        }
        indent(entries, 2);
      }
      entries << "]";
      fields.push_back(entries.str());
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool isExternalUseModuleEdge(
    SgNode *source, SgNode *target, const std::string &field,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return field == "module" && isSgUseStatement(source) != nullptr &&
         isSgModuleStatement(target) != nullptr && idFor(ids, target) == 0;
}

std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  auto validate_iterator_definitions =
      [](SgOmpClause *owner, const SgOmpIteratorDefinitionPtrList &definitions,
         bool required) {
        if (required != !definitions.empty()) {
          throw std::runtime_error(
              "AST JSON OpenMP iterator discriminator and structural "
              "definitions disagree");
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
            throw std::runtime_error(
                "AST JSON OpenMP iterator definition has invalid structural "
                "ownership");
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
                "AST JSON OpenMP iterator definition aliases one syntax "
                "node across roles");
          }
        }
      };
  fields.push_back(jsonString("attributes") + ": " +
                   rawAstAttributesJson(node));
  if (SgOmpClause *clause = isSgOmpClause(node)) {
    const std::optional<std::size_t> &source_order =
        clause->get_combined_source_order();
    if (!source_order.has_value()) {
      fields.push_back(jsonString("combined_source_order") + ": null");
    } else {
      if (*source_order >
          static_cast<std::size_t>(std::numeric_limits<int64_t>::max())) {
        throw std::runtime_error(
            "AST JSON OpenMP combined clause source order is out of range");
      }
      fields.push_back(rawIntegerField("combined_source_order",
                                       static_cast<int64_t>(*source_order)));
    }
  }
  if (SgOmpBodyStatement *statement = isSgOmpBodyStatement(node)) {
    fields.push_back(rawBoolField("source_form_is_combined",
                                  statement->get_source_form_is_combined()));
  }
  if (SgScopeStatement *scope = isSgScopeStatement(node)) {
    fields.push_back(
        rawBoolField("case_insensitive", scope->isCaseInsensitive()));
    fields.push_back(jsonString("symbol_table") + ": " +
                     rawSymbolTableJson(scope, ids));
  }
  if (SgBasicBlock *block = isSgBasicBlock(node)) {
    if (block->get_statement_expression_construction_scope() != nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize an active statement-expression "
          "semantic-scope construction transaction");
    }
    SgScopeStatement *semantic_scope =
        block->get_statement_expression_semantic_scope();
    SgStatementExpression *owner = isSgStatementExpression(block->get_parent());
    if ((semantic_scope == nullptr) != (owner == nullptr) ||
        (owner != nullptr && (owner->get_statement() != block ||
                              block->get_scope() != semantic_scope))) {
      throw std::runtime_error(
          "AST JSON SgBasicBlock has malformed statement-expression "
          "semantic ownership");
    }
    const uint64_t semantic_scope_id = idFor(ids, semantic_scope);
    if (semantic_scope != nullptr && semantic_scope_id == 0) {
      throw std::runtime_error(
          "AST JSON statement-expression semantic scope is outside the "
          "serialized node identity set");
    }
    if (block->get_implied_do_construction_scope() != nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize an active implied-DO semantic-scope "
          "construction transaction");
    }
    if (block->get_forall_construction_scope() != nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize an active FORALL semantic-scope "
          "construction transaction");
    }
    SgScopeStatement *implied_semantic_scope =
        block->get_implied_do_semantic_scope();
    SgImpliedDo *implied_owner = isSgImpliedDo(block->get_parent());
    if ((implied_semantic_scope == nullptr) != (implied_owner == nullptr) ||
        (implied_owner != nullptr &&
         (implied_owner->get_implied_do_scope() != block ||
          block->get_scope() != implied_semantic_scope)) ||
        (semantic_scope != nullptr && implied_semantic_scope != nullptr)) {
      throw std::runtime_error(
          "AST JSON SgBasicBlock has malformed implied-DO semantic "
          "ownership");
    }
    const uint64_t implied_semantic_scope_id =
        idFor(ids, implied_semantic_scope);
    if (implied_semantic_scope != nullptr && implied_semantic_scope_id == 0) {
      throw std::runtime_error(
          "AST JSON implied-DO semantic scope is outside the serialized node "
          "identity set");
    }
  }
  if (SgDeclarationScope *scope = isSgDeclarationScope(node)) {
    fields.push_back(rawBoolField("is_default_nonreal_scope",
                                  scope->get_is_default_nonreal_scope()));
  }
  if (SgAttributedStatement *statement = isSgAttributedStatement(node)) {
    statement->validate();
  }
  if (SgStatementAttributeList *attribute_list =
          isSgStatementAttributeList(node)) {
    attribute_list->validate();
  }
  if (SgStatementAttribute *attribute = isSgStatementAttribute(node)) {
    attribute->validate();
    fields.push_back(rawIntegerField("statement_attribute_kind",
                                     static_cast<int>(attribute->get_kind())));
    fields.push_back(
        rawIntegerField("statement_attribute_spelling",
                        static_cast<int>(attribute->get_spelling())));
    fields.push_back(rawIntegerField("statement_attribute_integral_argument",
                                     attribute->get_integral_argument()));
    fields.push_back(
        rawIntegerField("statement_attribute_loop_hint_option",
                        static_cast<int>(attribute->get_loop_hint_option())));
    fields.push_back(
        rawIntegerField("statement_attribute_loop_hint_state",
                        static_cast<int>(attribute->get_loop_hint_state())));
  }
  if (SgNamespaceSourceFragment *fragment = isSgNamespaceSourceFragment(node)) {
    fragment->validate();
    fields.push_back(rawIntegerField("namespace_source_fragment_kind",
                                     static_cast<int>(fragment->get_kind())));
    fields.push_back(
        rawIntegerField("namespace_source_fragment_form",
                        static_cast<int>(fragment->get_source_form())));
  }
  if (SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node)) {
    const auto fortranFamily = pragma->get_fortran_directive_family();
    const bool hasFortranDirective =
        fortranFamily != SgPragmaDeclaration::e_fortran_directive_none;
    const bool validFortranFamily =
        fortranFamily == SgPragmaDeclaration::e_fortran_directive_none ||
        fortranFamily == SgPragmaDeclaration::e_fortran_directive_openmp ||
        fortranFamily == SgPragmaDeclaration::e_fortran_directive_ompx ||
        fortranFamily == SgPragmaDeclaration::e_fortran_directive_openacc ||
        fortranFamily == SgPragmaDeclaration::e_fortran_directive_cuda;
    const bool hasValidFortranOwnership =
        hasFortranDirective &&
        !pragma->get_fortran_directive_group_id().empty() &&
        pragma->get_fortran_directive_member_count() > 0 &&
        pragma->get_fortran_directive_member_index() <
            pragma->get_fortran_directive_member_count() &&
        !pragma->get_fortran_directive_raw_text().empty() &&
        (pragma->get_fortran_directive_primary()
             ? !pragma->get_fortran_directive_logical_text().empty() &&
                   !pragma->get_fortran_directive_semantic_text().empty() &&
                   pragma->get_fortran_directive_member_index() == 0
             : pragma->get_fortran_directive_logical_text().empty() &&
                   pragma->get_fortran_directive_semantic_text().empty());
    const bool hasEmptyFortranOwnership =
        !hasFortranDirective &&
        pragma->get_fortran_directive_group_id().empty() &&
        pragma->get_fortran_directive_member_index() == 0 &&
        pragma->get_fortran_directive_member_count() == 0 &&
        !pragma->get_fortran_directive_primary() &&
        pragma->get_fortran_directive_raw_text().empty() &&
        pragma->get_fortran_directive_logical_text().empty() &&
        pragma->get_fortran_directive_semantic_text().empty();
    if (!validFortranFamily ||
        (!hasValidFortranOwnership && !hasEmptyFortranOwnership)) {
      throw std::runtime_error(
          "AST JSON SgPragmaDeclaration has malformed Fortran directive "
          "ownership");
    }
    fields.push_back(rawIntegerField("fortran_directive_family",
                                     static_cast<int>(fortranFamily)));
    fields.push_back(rawStringField("fortran_directive_group_id",
                                    pragma->get_fortran_directive_group_id()));
    fields.push_back(
        rawIntegerField("fortran_directive_member_index",
                        pragma->get_fortran_directive_member_index()));
    fields.push_back(
        rawIntegerField("fortran_directive_member_count",
                        pragma->get_fortran_directive_member_count()));
    fields.push_back(rawBoolField("fortran_directive_primary",
                                  pragma->get_fortran_directive_primary()));
    fields.push_back(rawStringField("fortran_directive_raw_text",
                                    pragma->get_fortran_directive_raw_text()));
    fields.push_back(
        rawStringField("fortran_directive_logical_text",
                       pragma->get_fortran_directive_logical_text()));
    fields.push_back(
        rawStringField("fortran_directive_semantic_text",
                       pragma->get_fortran_directive_semantic_text()));
    const auto payloadKind = pragma->get_cxx_pragma_payload_kind();
    if (payloadKind != SgPragmaDeclaration::e_cxx_pragma_payload_none &&
        payloadKind != SgPragmaDeclaration::e_cxx_pragma_source_spelled &&
        payloadKind != SgPragmaDeclaration::e_cxx_pragma_source_file_only &&
        payloadKind != SgPragmaDeclaration::e_cxx_pragma_generated_semantic) {
      throw std::runtime_error(
          "AST JSON SgPragmaDeclaration has invalid cxx pragma payload kind");
    }
    fields.push_back(rawIntegerField("cxx_pragma_payload_kind",
                                     static_cast<int>(payloadKind)));
    fields.push_back(
        rawStringField("cxx_source_text", pragma->get_cxx_source_text()));
    fields.push_back(rawBoolField("cxx_top_level_macro_expansion",
                                  pragma->get_cxx_top_level_macro_expansion()));
    fields.push_back(jsonString("openmp_producer_semantic_records") + ": " +
                     rawOpenMPProducerSemanticRecords(pragma, ids));
  }
  if (SgAccessLabelStatement *label = isSgAccessLabelStatement(node)) {
    label->validate();
    fields.push_back(rawIntegerField(
        "access_label_kind", static_cast<int>(label->get_label_kind())));
  }
  if (SgEmptyDeclaration *empty = isSgEmptyDeclaration(node)) {
    empty->validate_lexical_role();
    fields.push_back(
        rawIntegerField("empty_declaration_lexical_role",
                        static_cast<int>(empty->get_lexical_role())));
  }
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(node)) {
    group->validate();
    fields.push_back(
        rawIntegerField("declaration_group_source_terminator",
                        static_cast<int>(group->get_source_terminator())));
  }

  if (SgSourceFile *file = isSgSourceFile(node)) {
    SgType *target_size_type = file->get_target_size_type();
    if (target_size_type != nullptr) {
      SageInterface::requireTargetSizeType(file);
    }
    fields.push_back(jsonString("target_size_type") + ": " +
                     rawTypeJson(target_size_type, ids));
    fields.push_back(jsonString("token_mappings") + ": " +
                     rawTokenMappingsJson(file, ids));
    fields.push_back(rawStringField("source_filename_with_path",
                                    file->get_sourceFileNameWithPath()));
    fields.push_back(rawStringField("source_filename_without_path",
                                    file->get_sourceFileNameWithoutPath()));
    fields.push_back(rawStringField("unparse_output_filename",
                                    file->get_unparse_output_filename()));
    fields.push_back(rawBoolField("skip_unparse", file->get_skip_unparse()));
    fields.push_back(
        rawBoolField("is_generated_source", file->get_isGeneratedSource()));
    fields.push_back(rawBoolField("C_only", file->get_C_only()));
    fields.push_back(rawBoolField("Cxx_only", file->get_Cxx_only()));
    fields.push_back(rawBoolField("Fortran_only", file->get_Fortran_only()));
    fields.push_back(
        rawBoolField("CoArrayFortran_only", file->get_CoArrayFortran_only()));
    fields.push_back(rawBoolField("Cuda_only", file->get_Cuda_only()));
    fields.push_back(rawBoolField("OpenCL_only", file->get_OpenCL_only()));
    fields.push_back(rawBoolField("requires_C_preprocessor",
                                  file->get_requires_C_preprocessor()));
    fields.push_back(rawIntegerField("input_format", file->get_inputFormat()));
    fields.push_back(
        rawIntegerField("output_format", file->get_outputFormat()));
    fields.push_back(rawIntegerField("backend_compile_format",
                                     file->get_backendCompileFormat()));
    fields.push_back(rawBoolField("fortran_implicit_none",
                                  file->get_fortran_implicit_none()));
    fields.push_back(
        rawIntegerField("input_language", file->get_inputLanguage()));
    fields.push_back(
        rawIntegerField("output_language", file->get_outputLanguage()));
    fields.push_back(rawBoolField("strict_language_handling",
                                  file->get_strict_language_handling()));
    fields.push_back(rawBoolField("source_uses_cpp_extension",
                                  file->get_sourceFileUsesCppFileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran_extension",
                     file->get_sourceFileUsesFortranFileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran77_extension",
                     file->get_sourceFileUsesFortran77FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran90_extension",
                     file->get_sourceFileUsesFortran90FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran95_extension",
                     file->get_sourceFileUsesFortran95FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran2003_extension",
                     file->get_sourceFileUsesFortran2003FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran2008_extension",
                     file->get_sourceFileUsesFortran2008FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_coarray_fortran_extension",
                     file->get_sourceFileUsesCoArrayFortranFileExtension()));
    fields.push_back(rawBoolField("source_file_type_is_unknown",
                                  file->get_sourceFileTypeIsUnknown()));
    fields.push_back(rawBoolField("experimental_flang_frontend",
                                  file->get_experimental_flang_frontend()));
  } else if (SgToken *token = isSgToken(node)) {
    fields.push_back(
        rawStringField("lexeme_string", token->get_lexeme_string()));
    fields.push_back(rawIntegerField(
        "classification_code",
        static_cast<int64_t>(token->get_classification_code())));
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    switch (name->get_generated_variable_role()) {
    case SgInitializedName::e_generated_variable_none:
    case SgInitializedName::e_generated_loop_tiling_index:
    case SgInitializedName::e_generated_loop_tiling_increment:
      break;
    case SgInitializedName::e_last_generated_variable_role:
    default:
      throw std::runtime_error(
          "AST JSON SgInitializedName has an invalid generated variable "
          "role");
    }
    switch (name->get_enum_constant_source_ownership()) {
    case SgInitializedName::e_enum_constant_source_unclassified:
    case SgInitializedName::e_enum_constant_source_body:
    case SgInitializedName::e_enum_constant_source_external:
    case SgInitializedName::e_enum_constant_semantic_only:
      break;
    case SgInitializedName::e_last_enum_constant_source_ownership:
    default:
      throw std::runtime_error(
          "AST JSON SgInitializedName has an invalid enum-constant source "
          "ownership role");
    }
    switch (name->get_preinitialization()) {
    case SgInitializedName::e_unknown_preinitialization:
    case SgInitializedName::e_virtual_base_class:
    case SgInitializedName::e_nonvirtual_base_class:
    case SgInitializedName::e_data_member:
    case SgInitializedName::e_delegation_constructor:
      break;
    case SgInitializedName::e_last_preinitialization:
    default:
      throw std::runtime_error(
          "AST JSON SgInitializedName has an invalid preinitialization role");
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
    const bool is_enum_constant =
        isSgEnumDeclaration(name->get_parent()) != nullptr;
    const bool has_enum_source_role =
        name->get_enum_constant_source_ownership() !=
        SgInitializedName::e_enum_constant_source_unclassified;
    if (is_enum_constant != has_enum_source_role) {
      throw std::runtime_error(
          "AST JSON SgInitializedName has inconsistent enum-constant source "
          "ownership");
    }
    fields.push_back(rawStringField("name", name->get_name().getString()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(name->get_typeptr(), ids));
    fields.push_back(jsonString("fortran_source_type") + ": " +
                     rawTypeJson(name->get_fortran_source_type(), ids));
    fields.push_back(jsonString("cxx_source_type") + ": " +
                     rawTypeJson(name->get_cxx_source_type(), ids));
    fields.push_back(jsonString("fortran_source_derived_type_symbol") + ": " +
                     rawExactBoundSymbolRef(
                         name->get_fortran_source_derived_type_symbol(), ids));
    fields.push_back(rawIntegerField(
        "cray_pointer_pointee", idFor(ids, name->get_cray_pointer_pointee())));
    fields.push_back(rawIntegerField(
        "fortran_cray_pointer_pointee_shape",
        idFor(ids, name->get_fortran_cray_pointer_pointee_shape())));
    fields.push_back(rawIntegerField(
        "fortran_type_spec", static_cast<int>(name->get_fortran_type_spec())));
    fields.push_back(
        rawStringField("fortran_procedure_interface",
                       name->get_fortran_procedure_interface().getString()));
    fields.push_back(rawIntegerField(
        "fortran_separate_shape_declaration",
        idFor(ids, name->get_fortran_separate_shape_declaration())));
    fields.push_back(rawIntegerField(
        "fortran_separate_pointer_declaration",
        idFor(ids, name->get_fortran_separate_pointer_declaration())));
    fields.push_back(rawBoolField("shape_deferred", name->get_shapeDeferred()));
    fields.push_back(rawBoolField("is_predefined_identifier",
                                  name->get_is_predefined_identifier()));
    fields.push_back(
        rawIntegerField("generated_variable_role",
                        static_cast<int>(name->get_generated_variable_role())));
    fields.push_back(rawIntegerField(
        "enum_constant_source_ownership",
        static_cast<int>(name->get_enum_constant_source_ownership())));
    fields.push_back(rawIntegerField(
        "preinitialization", static_cast<int>(name->get_preinitialization())));
    fields.push_back(rawIntegerField(
        "storage_modifier",
        static_cast<int>(name->get_storageModifier().get_modifier())));
    fields.push_back(rawStringField("gnu_attribute_section_name",
                                    name->get_gnu_attribute_section_name()));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     name->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  name->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  name->get_global_qualification_required()));
    fields.push_back(
        rawIntegerField("name_qualification_length_for_type",
                        name->get_name_qualification_length_for_type()));
    fields.push_back(
        rawBoolField("type_elaboration_required_for_type",
                     name->get_type_elaboration_required_for_type()));
    fields.push_back(
        rawBoolField("global_qualification_required_for_type",
                     name->get_global_qualification_required_for_type()));
    fields.push_back(
        rawBoolField("source_type_qualification_present",
                     name->get_source_type_qualification_present()));
    fields.push_back(
        rawBoolField("source_type_global_qualification",
                     name->get_source_type_global_qualification()));
    fields.push_back(
        jsonString("source_type_qualification_tokens") + ": " +
        rawStringListJson(name->get_source_type_qualification_tokens()));
    fields.push_back(
        rawBoolField("source_name_qualification_present",
                     name->get_source_name_qualification_present()));
    fields.push_back(
        rawBoolField("source_name_global_qualification",
                     name->get_source_name_global_qualification()));
    fields.push_back(
        jsonString("source_name_qualification_tokens") + ": " +
        rawStringListJson(name->get_source_name_qualification_tokens()));
  } else if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    if (decl->get_source_declarator_uses_wrapped_function_type()) {
      decl->validate_source_declarator_form();
    }
    SgFunctionParameterList *parameter_list = decl->get_parameterList();
    SgFunctionType *function_type = decl->get_type();
    if (parameter_list == nullptr || parameter_list->get_parent() != decl ||
        function_type == nullptr ||
        function_type->get_return_type() == nullptr ||
        decl->get_scope() == nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize a function declaration without exact "
          "parameter-list, type, and scope contracts: " +
          decl->get_name().getString());
    }
    if (decl->get_type_syntax_is_available() !=
            (decl->get_type_syntax() != nullptr) ||
        (decl->get_type_syntax() != nullptr &&
         (decl->get_type_syntax() == decl->get_type() ||
          decl->get_type_syntax()->get_parent() != decl))) {
      throw std::runtime_error("AST JSON SgFunctionDeclaration " +
                               decl->get_name().getString() +
                               " has inconsistent or unowned function type "
                               "syntax state");
    }
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawStringField(
        "omp_declare_variant_source_name",
        decl->get_omp_declare_variant_source_name().getString()));
    fields.push_back(rawOptionalUnsignedIntegerField(
        "omp_declare_variant_region_ordinal",
        ompDeclareVariantRegionOrdinalForJson(decl)));
    fields.push_back(
        rawStringField("fortran_anonymous_program_unit_symbol_key",
                       fortranAnonymousProgramUnitSymbolKeyForJson(decl)));
    fields.push_back(
        rawBoolField("source_name_parenthesized_for_macro",
                     decl->get_source_name_parenthesized_for_macro()));
    fields.push_back(
        rawBoolField("source_declarator_uses_wrapped_function_type",
                     decl->get_source_declarator_uses_wrapped_function_type()));
    fields.push_back(jsonString("function_type") + ": " +
                     rawTypeJson(function_type, ids));
    fields.push_back(jsonString("function_type_syntax") + ": " +
                     rawTypeJson(decl->get_type_syntax(), ids));
    if (functionParameterScopeNeedsExternalReferenceRecord(decl, ids)) {
      fields.push_back(
          rawStringField("external_function_parameter_scope_source",
                         externalFunctionParameterScopeSource(decl, ids)));
      SgFunctionParameterScope *external_scope =
          decl->get_functionParameterScope();
      SgFunctionDeclaration *external_owner =
          external_scope != nullptr
              ? isSgFunctionDeclaration(external_scope->get_parent())
              : nullptr;
      if (external_owner == nullptr || external_owner == decl) {
        throw std::runtime_error(
            "AST JSON external shared function parameter scope has no exact "
            "external function owner");
      }
      fields.push_back(jsonString("external_function_parameter_scope") + ": " +
                       rawExternalFunctionParameterScopeJson(
                           external_owner, external_scope, ids,
                           decl->get_name().getString()));
    }
    fields.push_back(jsonString("return_type") + ": " +
                     rawTypeJson(function_type->get_return_type(), ids));
    fields.push_back(
        jsonString("function_modifier_vector") + ": " +
        rawBitVectorJson(decl->get_functionModifier().get_modifierVector()));
    fields.push_back(
        jsonString("special_function_modifier_vector") + ": " +
        rawBitVectorJson(
            decl->get_specialFunctionModifier().get_modifierVector()));
    fields.push_back(rawBoolField("named_in_end_statement",
                                  decl->get_named_in_end_statement()));
    fields.push_back(rawStringField("asm_name", decl->get_asm_name()));
    fields.push_back(
        rawBoolField("old_style_definition", decl->get_oldStyleDefinition()));
    fields.push_back(
        rawBoolField("requires_name_qualification_on_return_type",
                     decl->get_requiresNameQualificationOnReturnType()));
    fields.push_back(rawStringField("gnu_extension_section",
                                    decl->get_gnu_extension_section()));
    fields.push_back(
        rawStringField("gnu_extension_alias", decl->get_gnu_extension_alias()));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     decl->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  decl->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  decl->get_global_qualification_required()));
    fields.push_back(
        rawIntegerField("name_qualification_length_for_return_type",
                        decl->get_name_qualification_length_for_return_type()));
    fields.push_back(
        rawBoolField("type_elaboration_required_for_return_type",
                     decl->get_type_elaboration_required_for_return_type()));
    fields.push_back(rawBoolField(
        "global_qualification_required_for_return_type",
        decl->get_global_qualification_required_for_return_type()));
    const auto source_return_type_elaboration =
        decl->get_source_return_type_elaboration_kind();
    if (decl->get_source_return_type_qualification_present() &&
        source_return_type_elaboration ==
            SgFunctionDeclaration::
                e_source_return_type_elaboration_unspecified) {
      throw std::runtime_error(
          "AST JSON source function return type has no exact elaboration "
          "kind: name=" +
          decl->get_name().getString() + " ownership=" +
          std::to_string(
              static_cast<int>(decl->get_frontend_source_ownership())));
    }
    if (source_return_type_elaboration !=
            SgFunctionDeclaration::
                e_source_return_type_elaboration_unspecified &&
        decl->get_type_elaboration_required_for_return_type() !=
            (source_return_type_elaboration !=
             SgFunctionDeclaration::e_source_return_type_elaboration_none)) {
      throw std::runtime_error(
          "AST JSON exact function return-type elaboration disagrees with "
          "its typed boolean projection");
    }
    fields.push_back(
        rawBoolField("source_return_type_qualification_present",
                     decl->get_source_return_type_qualification_present()));
    fields.push_back(
        rawBoolField("source_return_type_global_qualification",
                     decl->get_source_return_type_global_qualification()));
    fields.push_back(
        jsonString("source_return_type_qualification_tokens") + ": " +
        rawStringListJson(decl->get_source_return_type_qualification_tokens()));
    fields.push_back(
        rawIntegerField("source_return_type_elaboration_kind",
                        static_cast<int>(source_return_type_elaboration)));
    fields.push_back(rawBoolField("prototype_is_without_parameters",
                                  decl->get_prototypeIsWithoutParameters()));
    fields.push_back(rawIntegerField("gnu_regparm_attribute",
                                     decl->get_gnu_regparm_attribute()));
    fields.push_back(rawBoolField("type_syntax_is_available",
                                  decl->get_type_syntax_is_available()));
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(decl)) {
      fields.push_back(
          rawIntegerField("fortran_result_type_spec",
                          procedure->get_fortran_result_type_spec()));
      fields.push_back(
          jsonString("fortran_source_derived_type_symbol") + ": " +
          rawExactBoundSymbolRef(
              procedure->get_fortran_source_derived_type_symbol(), ids));
    }
    fields.push_back(rawBoolField("using_c11_noreturn_keyword",
                                  decl->get_using_C11_Noreturn_keyword()));
    fields.push_back(rawBoolField("is_constexpr", decl->get_is_constexpr()));
    fields.push_back(
        rawBoolField("using_new_function_return_type_syntax",
                     decl->get_using_new_function_return_type_syntax()));
    fields.push_back(
        rawBoolField("is_deduction_guide", decl->get_is_deduction_guide()));
    fields.push_back(
        rawBoolField("marked_as_frontend_normalization",
                     decl->get_marked_as_frontend_normalization()));
    fields.push_back(
        rawBoolField("is_implicit_function", decl->get_is_implicit_function()));
    fields.push_back(rawBoolField(
        "template_instantiation_pattern_is_unpublished",
        decl->get_template_instantiation_pattern_is_unpublished()));
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(decl)) {
      SageInterface::isFortranProgramUnitWithoutSourceName(procedure);
      fields.push_back(
          rawIntegerField("subprogram_kind", procedure->get_subprogram_kind()));
      fields.push_back(rawIntegerField("block_data_name_kind",
                                       procedure->get_block_data_name_kind()));
      fields.push_back(
          rawIntegerField("fortran_procedure_source_form",
                          procedure->get_fortran_procedure_source_form()));
    }
    if (SgProgramHeaderStatement *program = isSgProgramHeaderStatement(decl)) {
      const std::string end_statement_name =
          program->get_end_statement_name().getString();
      validateFortranProgramNameMetadata(program->get_name().getString(),
                                         program->get_program_statement_kind(),
                                         program->get_named_in_end_statement(),
                                         end_statement_name);
      fields.push_back(rawIntegerField("program_statement_kind",
                                       program->get_program_statement_kind()));
      fields.push_back(
          rawStringField("end_statement_name", end_statement_name));
    }
    if (SgTemplateInstantiationFunctionDecl *tmpl =
            isSgTemplateInstantiationFunctionDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(
          rawBoolField("template_argument_list_is_explicit",
                       tmpl->get_template_argument_list_is_explicit()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
    if (SgTemplateInstantiationMemberFunctionDecl *tmpl =
            isSgTemplateInstantiationMemberFunctionDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(
          rawBoolField("template_argument_list_is_explicit",
                       tmpl->get_template_argument_list_is_explicit()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    if (decl->get_typedef_type() != SgTypedefDeclaration::e_typedef &&
        decl->get_typedef_type() != SgTypedefDeclaration::e_using) {
      throw std::runtime_error(
          "AST JSON SgTypedefDeclaration has no exact typedef/using source "
          "form during serialization");
    }
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(jsonString("base_type") + ": " +
                     rawTypeJson(decl->get_base_type(), ids));
    fields.push_back(rawIntegerField("typedef_type", decl->get_typedef_type()));
    fields.push_back(
        rawBoolField("typedef_base_type_contains_defining_declaration",
                     decl->get_typedefBaseTypeContainsDefiningDeclaration()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    fields.push_back(
        rawBoolField("source_base_type_qualification_present",
                     decl->get_source_base_type_qualification_present()));
    fields.push_back(
        rawBoolField("source_base_type_global_qualification",
                     decl->get_source_base_type_global_qualification()));
    fields.push_back(
        jsonString("source_base_type_qualification_tokens") + ": " +
        rawStringListJson(decl->get_source_base_type_qualification_tokens()));
    if (SgTemplateInstantiationTypedefDeclaration *tmpl =
            isSgTemplateInstantiationTypedefDeclaration(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawStringField("template_header",
                                      tmpl->get_templateHeader().getString()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", decl->get_class_type()));
    fields.push_back(rawBoolField("is_unnamed", decl->get_isUnNamed()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    if (SgTemplateClassDeclaration *tmpl = isSgTemplateClassDeclaration(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
    }
    if (SgTemplateInstantiationDecl *tmpl =
            isSgTemplateInstantiationDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawStringField("template_header",
                                      tmpl->get_templateHeader().getString()));
      fields.push_back(rawBoolField("source_spells_injected_class_name",
                                    tmpl->get_sourceSpellsInjectedClassName()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_evaluated",
                       tmpl->get_constraintSatisfactionEvaluated()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_satisfied",
                       tmpl->get_constraintSatisfactionSatisfied()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_contains_errors",
                       tmpl->get_constraintSatisfactionContainsErrors()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_substitution_failure",
                       tmpl->get_constraintSatisfactionSubstitutionFailure()));
      fields.push_back(
          rawStringField("constraint_satisfaction_summary",
                         tmpl->get_constraintSatisfactionSummary()));
      fields.push_back(
          rawBoolField("sfinae_evaluated", tmpl->get_sfinaeEvaluated()));
      fields.push_back(rawBoolField("sfinae_substitution_failure",
                                    tmpl->get_sfinaeSubstitutionFailure()));
      fields.push_back(
          rawStringField("sfinae_summary", tmpl->get_sfinaeSummary()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("semantic_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_semanticTemplateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    decl->validate_enumerator_source_ownership();
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawBoolField("embedded", decl->get_embedded()));
    fields.push_back(rawBoolField("is_unnamed", decl->get_isUnNamed()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    fields.push_back(rawBoolField("is_scoped_enum", decl->get_isScopedEnum()));
    fields.push_back(rawBoolField("underlying_type_source_spelled",
                                  decl->get_underlying_type_source_spelled()));
    fields.push_back(jsonString("field_type") + ": " +
                     rawTypeJson(decl->get_field_type(), ids));
  } else if (SgNamespaceDeclarationStatement *decl =
                 isSgNamespaceDeclarationStatement(node)) {
    decl->validate_source_fragments();
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(
        rawBoolField("is_unnamed_namespace", decl->get_isUnnamedNamespace()));
    fields.push_back(
        rawBoolField("is_inlined_namespace", decl->get_isInlinedNamespace()));
  } else if (SgNamespaceDefinitionStatement *def =
                 isSgNamespaceDefinitionStatement(node)) {
    fields.push_back(
        rawBoolField("is_union_of_reentrant_namespace_definitions",
                     def->get_isUnionOfReentrantNamespaceDefinitions()));
  } else if (SgUsingDirectiveStatement *stmt =
                 isSgUsingDirectiveStatement(node)) {
    SgNamespaceDeclarationStatement *decl = stmt->get_namespaceDeclaration();
    fields.push_back(
        rawIntegerField("namespace_declaration", idFor(ids, decl)));
    fields.push_back(rawStringField(
        "namespace_name", decl != nullptr ? decl->get_name().getString() : ""));
    fields.push_back(
        rawBoolField("namespace_is_unnamed",
                     decl != nullptr ? decl->get_isUnnamedNamespace() : false));
  } else if (SgUsingDeclarationStatement *stmt =
                 isSgUsingDeclarationStatement(node)) {
    SgDeclarationStatement *decl = stmt->get_declaration();
    SgInitializedName *name = stmt->get_initializedName();
    fields.push_back(rawIntegerField("declaration", idFor(ids, decl)));
    fields.push_back(
        rawStringField("declaration_name",
                       decl != nullptr ? SageInterface::get_name(decl) : ""));
    fields.push_back(rawIntegerField("initialized_name", idFor(ids, name)));
    fields.push_back(
        rawStringField("initialized_name_name",
                       name != nullptr ? name->get_name().getString() : ""));
    fields.push_back(
        jsonString("initialized_name_type") + ": " +
        rawTypeJson(name != nullptr ? name->get_typeptr() : nullptr, ids));
    fields.push_back(rawStringField(
        "source_terminal_name", stmt->get_source_terminal_name().getString()));
    fields.push_back(rawBoolField("is_inheriting_constructor",
                                  stmt->get_is_inheriting_constructor()));
  } else if (SgUseStatement *stmt = isSgUseStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
    fields.push_back(rawBoolField("only_option", stmt->get_only_option()));
    fields.push_back(
        rawStringField("module_nature", stmt->get_module_nature()));
    fields.push_back(rawIntegerField("module", idFor(ids, stmt->get_module())));
    fields.push_back(jsonString("external_module") + ": " +
                     rawExternalModuleJson(stmt->get_module(), ids));
  } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(
        rawStringField("semantic_name", decl->get_semantic_name().getString()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(decl->get_type(), ids));
    fields.push_back(rawIntegerField("template_parameter_position",
                                     decl->get_template_parameter_position()));
    fields.push_back(rawIntegerField("template_parameter_depth",
                                     decl->get_template_parameter_depth()));
    fields.push_back(
        rawBoolField("is_class_member", decl->get_is_class_member()));
    fields.push_back(
        rawBoolField("is_template_param", decl->get_is_template_param()));
    fields.push_back(rawBoolField("is_template_template_param",
                                  decl->get_is_template_template_param()));
    fields.push_back(
        rawBoolField("has_template_keyword", decl->get_has_template_keyword()));
    fields.push_back(
        rawBoolField("has_global_qualifier", decl->get_has_global_qualifier()));
    fields.push_back(
        rawBoolField("suppress_typename", decl->get_suppress_typename()));
    fields.push_back(rawIntegerField("source_elaboration_kind",
                                     decl->get_source_elaboration_kind()));
    fields.push_back(rawIntegerField("nonreal_template_role",
                                     decl->get_nonreal_template_role()));
    fields.push_back(rawBoolField("is_concept", decl->get_is_concept()));
    fields.push_back(
        rawBoolField("is_nonreal_function", decl->get_is_nonreal_function()));
    fields.push_back(jsonString("tpl_args") + ": " +
                     rawTemplateArgumentListJson(decl->get_tpl_args(), ids));
  } else if (SgBaseClass *base = isSgBaseClass(node)) {
    fields.push_back(
        rawIntegerField("base_class", idFor(ids, base->get_base_class())));
    fields.push_back(jsonString("source_type") + ": " +
                     rawTypeJson(base->get_source_type(), ids));
    fields.push_back(
        rawBoolField("is_direct_base_class", base->get_isDirectBaseClass()));
    fields.push_back(
        rawBoolField("pack_expansion", base->get_pack_expansion()));
    fields.push_back(rawIntegerField(
        "base_class_modifier",
        base->get_baseClassModifier() != nullptr
            ? static_cast<int>(base->get_baseClassModifier()->get_modifier())
            : static_cast<int>(SgBaseClassModifier::e_unknown)));
    fields.push_back(
        rawIntegerField("base_class_access_modifier",
                        base->get_baseClassModifier() != nullptr
                            ? static_cast<int>(base->get_baseClassModifier()
                                                   ->get_accessModifier()
                                                   .get_modifier())
                            : static_cast<int>(SgAccessModifier::e_unknown)));
    fields.push_back(rawBoolField("base_class_access_is_explicit",
                                  base->get_baseClassModifier() != nullptr
                                      ? base->get_baseClassModifier()
                                            ->get_accessModifier()
                                            .get_is_explicit()
                                      : false));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     base->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  base->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  base->get_global_qualification_required()));
    fields.push_back(
        rawBoolField("source_type_qualification_present",
                     base->get_source_type_qualification_present()));
    fields.push_back(
        rawBoolField("source_type_global_qualification",
                     base->get_source_type_global_qualification()));
    fields.push_back(
        jsonString("source_type_qualification_tokens") + ": " +
        rawStringListJson(base->get_source_type_qualification_tokens()));
    fields.push_back(
        rawBoolField("source_type_qualification_owns_terminal_name",
                     base->get_source_type_qualification_owns_terminal_name()));
    fields.push_back(rawBoolField(
        "source_type_qualification_owns_template_arguments",
        base->get_source_type_qualification_owns_template_arguments()));
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      fields.push_back(jsonString("base_class_exp") + ": " +
                       rawExpressionRef(expr_base->get_base_class_exp(), ids));
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      fields.push_back(
          rawIntegerField("base_class_nonreal",
                          idFor(ids, nonreal_base->get_base_class_nonreal())));
    }
  } else if (SgPragma *pragma = isSgPragma(node)) {
    fields.push_back(rawStringField("name", pragma->get_name()));
  } else if (SgTypeTraitBuiltinOperator *op =
                 isSgTypeTraitBuiltinOperator(node)) {
    if (op->get_builtin_operator_kind() !=
            SgTypeTraitBuiltinOperator::e_type_trait_builtin &&
        op->get_builtin_operator_kind() !=
            SgTypeTraitBuiltinOperator::e_offsetof_builtin &&
        op->get_builtin_operator_kind() !=
            SgTypeTraitBuiltinOperator::e_convert_vector_builtin) {
      throw std::runtime_error(
          "AST JSON SgTypeTraitBuiltinOperator has invalid builtin kind");
    }
    fields.push_back(rawStringField("name", op->get_name().getString()));
    fields.push_back(
        rawIntegerField("builtin_operator_kind",
                        static_cast<int>(op->get_builtin_operator_kind())));
    if (serializingTypeOwnedExpression) {
      fields.push_back(jsonString("args") + ": " +
                       rawTypeOwnedExpressionListJson(op->get_args(), ids));
    }
  } else if (SgRenamePair *rename = isSgRenamePair(node)) {
    fields.push_back(
        rawStringField("local_name", rename->get_local_name().getString()));
    fields.push_back(
        rawStringField("use_name", rename->get_use_name().getString()));
  } else if (SgCommonBlockObject *object = isSgCommonBlockObject(node)) {
    fields.push_back(rawStringField("block_name", object->get_block_name()));
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    const SgStorageModifier &storage =
        decl->get_declarationModifier().get_storageModifier();
    fields.push_back(rawOptionalUnsignedIntegerField(
        "translation_unit_source_order",
        decl->get_translation_unit_source_order()));
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(decl)) {
      fields.push_back(rawIntegerField(
          "frontend_source_ownership",
          static_cast<int>(function->get_frontend_source_ownership())));
      fields.push_back(rawIntegerField(
          "frontend_declaration_origin",
          static_cast<int>(function->get_frontend_declaration_origin())));
    }
    fields.push_back(
        rawIntegerField("decl_attributes", decl->get_decl_attributes()));
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
      fields.push_back(rawIntegerField(
          "specialization", static_cast<int>(variable->get_specialization())));
    } else if (SgClassDeclaration *class_declaration =
                   isSgClassDeclaration(decl)) {
      fields.push_back(rawIntegerField(
          "specialization",
          static_cast<int>(class_declaration->get_specialization())));
    } else if (SgFunctionDeclaration *function =
                   isSgFunctionDeclaration(decl)) {
      fields.push_back(rawIntegerField(
          "specialization", static_cast<int>(function->get_specialization())));
    }
    fields.push_back(rawStringField("linkage", decl->get_linkage()));
    fields.push_back(
        jsonString("declaration_modifier_vector") + ": " +
        rawBitVectorJson(decl->get_declarationModifier().get_modifierVector()));
    appendRawDeclarationTypeModifierFields(
        fields, decl->get_declarationModifier().get_typeModifier());
    fields.push_back(rawIntegerField("declaration_storage_modifier",
                                     static_cast<int>(storage.get_modifier())));
    fields.push_back(rawBoolField("declaration_thread_local_storage",
                                  storage.get_thread_local_storage()));
    fields.push_back(
        rawIntegerField("declaration_access_modifier",
                        static_cast<int>(decl->get_declarationModifier()
                                             .get_accessModifier()
                                             .get_modifier())));
    fields.push_back(rawBoolField("declaration_access_is_explicit",
                                  decl->get_declarationModifier()
                                      .get_accessModifier()
                                      .get_is_explicit()));
    fields.push_back(rawBoolField("name_only", decl->get_nameOnly()));
    fields.push_back(rawBoolField("forward", decl->get_forward()));
    fields.push_back(rawBoolField("extern_brace", decl->get_externBrace()));
    fields.push_back(
        rawBoolField("skip_elaborate_type", decl->get_skipElaborateType()));
    fields.push_back(
        rawStringField("binding_label", decl->get_binding_label()));
    fields.push_back(
        rawBoolField("binding_cdefined", decl->get_binding_cdefined()));
    fields.push_back(
        rawBoolField("unparse_template_ast", decl->get_unparse_template_ast()));
    fields.push_back(
        rawBoolField("source_name_qualification_present",
                     decl->get_source_name_qualification_present()));
    fields.push_back(
        rawBoolField("source_name_global_qualification",
                     decl->get_source_name_global_qualification()));
    fields.push_back(
        jsonString("source_name_qualification_tokens") + ": " +
        rawStringListJson(decl->get_source_name_qualification_tokens()));
    fields.push_back(rawIntegerField(
        "declaration_gnu_attribute_visibility",
        static_cast<int>(
            decl->get_declarationModifier().get_gnu_attribute_visibility())));
    fields.push_back(rawIntegerField(
        "declaration_gnu_type_visibility",
        static_cast<int>(
            decl->get_declarationModifier().get_gnu_type_visibility())));
    fields.push_back(jsonString("external_first_nondefining_declaration") +
                     ": " +
                     rawExternalDeclarationReferenceJson(
                         decl->get_firstNondefiningDeclaration(), decl, ids));
    fields.push_back(jsonString("external_defining_declaration") + ": " +
                     rawExternalDeclarationReferenceJson(
                         decl->get_definingDeclaration(), decl, ids));
  }

  if (SgReturnStmt *return_statement = isSgReturnStmt(node)) {
    switch (return_statement->get_return_keyword_kind()) {
    case SgReturnStmt::e_return_keyword_return:
    case SgReturnStmt::e_return_keyword_co_return:
      break;
    default:
      throw std::runtime_error(
          "AST JSON SgReturnStmt has an invalid return keyword kind");
    }
    fields.push_back(rawIntegerField(
        "return_keyword_kind",
        static_cast<int>(return_statement->get_return_keyword_kind())));
  }

  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    fields.push_back(rawBoolField("has_source_variables_override",
                                  clause->get_has_source_variables_override()));
  }
  if (SgOmpFirstprivateClause *clause = isSgOmpFirstprivateClause(node)) {
    fields.push_back(rawBoolField("saved", clause->get_saved()));
  }

  if (SgExpression *expr = isSgExpression(node)) {
    if (SgFortranCommonBlockRefExp *common =
            isSgFortranCommonBlockRefExp(expr)) {
      SageInterface::validateFortranCommonBlockRef(common);
      fields.push_back(
          rawStringField("use_name", common->get_use_name().getString()));
    } else {
      addExpressionType(fields, expr, ids);
    }
    if (SgTypeExpression *type_expression = isSgTypeExpression(expr)) {
      SgType *represented_type = type_expression->get_represented_type();
      if (represented_type == nullptr ||
          isSgTypeUnknown(represented_type) != nullptr ||
          isSgTypeDefault(represented_type) != nullptr) {
        throw std::runtime_error(
            "AST JSON SgTypeExpression has no exact represented type");
      }
      fields.push_back(jsonString("represented_type") + ": " +
                       rawTypeJson(represented_type, ids));
    }
    if (SgConditionalExp *conditional = isSgConditionalExp(expr)) {
      conditional->validate();
      fields.push_back(
          rawIntegerField("conditional_operator_kind",
                          static_cast<int>(conditional->get_operator_kind())));
    }
    fields.push_back(rawBoolField("lvalue", expr->get_lvalue()));
    fields.push_back(rawBoolField("need_paren", expr->get_need_paren()));
    fields.push_back(rawBoolField("global_qualified_name",
                                  expr->get_global_qualified_name()));
    fields.push_back(
        rawIntegerField("semantic_wrapper_mask",
                        static_cast<int>(expr->get_semantic_wrapper_mask())));
    const bool fortranIntegerValueAvailable =
        expr->get_fortran_integer_constant_value_is_available();
    const std::int64_t fortranIntegerValue =
        expr->get_fortran_integer_constant_value();
    if (!fortranIntegerValueAvailable && fortranIntegerValue != 0) {
      throw std::runtime_error(
          "AST JSON expression has a Fortran folded selector value without "
          "availability");
    }
    fields.push_back(rawBoolField("fortran_integer_constant_value_is_available",
                                  fortranIntegerValueAvailable));
    fields.push_back(
        rawIntegerField("fortran_integer_constant_value", fortranIntegerValue));
    if (SgValueExp *value = isSgValueExp(expr)) {
      fields.push_back(rawBoolField("has_literal_semantic_type",
                                    value->get_literal_type() != nullptr));
      if (value->get_literal_spelling_form() !=
              SgValueExp::e_literal_source_spelled &&
          value->get_literal_spelling_form() !=
              SgValueExp::e_literal_canonical_generated) {
        const Sg_File_Info *start = value->get_startOfConstruct();
        const SgNode *parent = value->get_parent();
        throw std::runtime_error(
            "AST JSON " + value->class_name() +
            " has invalid literal_spelling_form=" +
            std::to_string(
                static_cast<int>(value->get_literal_spelling_form())) +
            ", parent=" + (parent != nullptr ? parent->class_name() : "null") +
            ", source=" +
            (start != nullptr ? start->get_filenameString() + ":" +
                                    std::to_string(start->get_line()) + ":" +
                                    std::to_string(start->get_col())
                              : "null"));
      }
      fields.push_back(rawIntegerField(
          "literal_spelling_form",
          static_cast<int>(value->get_literal_spelling_form())));
    }
    if (SgArrowExp *arrow = isSgArrowExp(expr)) {
      fields.push_back(rawIntegerField(
          "arrow_emission_role", static_cast<int>(arrow->get_emission_role())));
    }
    if (SgAwaitExpression *await_expression = isSgAwaitExpression(expr)) {
      if (await_expression->get_value() == nullptr ||
          await_expression->get_value()->get_parent() != await_expression) {
        throw std::runtime_error(
            "AST JSON SgAwaitExpression has no exactly owned operand");
      }
      switch (await_expression->get_coroutine_keyword_kind()) {
      case SgAwaitExpression::e_coroutine_keyword_co_await:
      case SgAwaitExpression::e_coroutine_keyword_co_yield:
        break;
      case SgAwaitExpression::e_coroutine_keyword_unspecified:
      default:
        throw std::runtime_error(
            "AST JSON SgAwaitExpression has an invalid coroutine keyword "
            "kind");
      }
      fields.push_back(rawIntegerField(
          "coroutine_keyword_kind",
          static_cast<int>(await_expression->get_coroutine_keyword_kind())));
    }
    if (SgFoldExpression *fold_expression = isSgFoldExpression(expr)) {
      if (fold_expression->get_operands() == nullptr ||
          fold_expression->get_operands()->get_parent() != fold_expression ||
          fold_expression->get_operator_token().empty()) {
        throw std::runtime_error(
            "AST JSON SgFoldExpression has malformed operands or operator");
      }
      fields.push_back(rawStringField("operator_token",
                                      fold_expression->get_operator_token()));
      fields.push_back(rawBoolField(
          "is_left_associative", fold_expression->get_is_left_associative()));
    }
    if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(expr)) {
      if (pack->get_pattern_expression() == nullptr ||
          pack->get_pattern_expression()->get_parent() != pack) {
        throw std::runtime_error(
            "AST JSON SgPackExpansionExpr has no exclusively owned pattern");
      }
    }
    if (SgStatementExpression *statement_expression =
            isSgStatementExpression(expr)) {
      SgBasicBlock *body =
          isSgBasicBlock(statement_expression->get_statement());
      if (body == nullptr || body->get_parent() != statement_expression ||
          body->get_statement_expression_construction_scope() != nullptr ||
          body->get_statement_expression_semantic_scope() == nullptr ||
          body->get_scope() !=
              body->get_statement_expression_semantic_scope()) {
        throw std::runtime_error(
            "AST JSON SgStatementExpression has no exactly owned body and "
            "semantic scope");
      }
    }
    if (SgInitializer *initializer = isSgInitializer(expr)) {
      fields.push_back(rawBoolField("is_braced_initialized",
                                    initializer->get_is_braced_initialized()));
    }
    if (SgAssignInitializer *initializer = isSgAssignInitializer(expr)) {
      fields.push_back(
          rawIntegerField("assignment_initializer_source_form",
                          static_cast<int>(initializer->get_source_form())));
    }
    addExpressionQualificationFields(fields, expr);
    if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expr)) {
      macro->get_expanded_expression_checked();
      fields.push_back(rawStringField("spelling", macro->get_spelling()));
    }
    if (SgSourceLocationBuiltinExp *builtin =
            isSgSourceLocationBuiltinExp(expr)) {
      fields.push_back(rawIntegerField("source_location_builtin_kind",
                                       static_cast<int>(builtin->get_kind())));
    }
    if (SgNullExpression *null_expression = isSgNullExpression(expr)) {
      if (null_expression->get_role() ==
          SgNullExpression::e_null_expression_unclassified) {
        throw std::runtime_error("AST JSON SgNullExpression has no exact role");
      }
      fields.push_back(rawIntegerField(
          "role", static_cast<int>(null_expression->get_role())));
    }
    if (SgAggregateInitializer *aggregate = isSgAggregateInitializer(expr)) {
      if (aggregate->get_source_form() ==
          SgAggregateInitializer::e_aggregate_initializer_source_unclassified) {
        throw std::runtime_error(
            "AST JSON SgAggregateInitializer has no exact source form");
      }
      fields.push_back(
          rawIntegerField("aggregate_source_form",
                          static_cast<int>(aggregate->get_source_form())));
      const bool hasExplicitFortranType =
          aggregate->get_fortran_has_source_explicit_type();
      SgType *explicitFortranType =
          aggregate->get_fortran_source_explicit_type();
      const bool typedFortranConstructor =
          aggregate->get_source_form() ==
              SgAggregateInitializer::e_aggregate_initializer_source_fortran ||
          aggregate->get_source_form() ==
              SgAggregateInitializer::
                  e_aggregate_initializer_source_fortran_structure;
      if (hasExplicitFortranType != (explicitFortranType != nullptr) ||
          (!typedFortranConstructor && hasExplicitFortranType)) {
        throw std::runtime_error(
            "AST JSON SgAggregateInitializer has contradictory Fortran "
            "source type-spec state");
      }
      fields.push_back(rawBoolField("fortran_has_source_explicit_type",
                                    hasExplicitFortranType));
      fields.push_back(jsonString("fortran_source_explicit_type") + ": " +
                       rawTypeJson(explicitFortranType, ids));
      fields.push_back(
          jsonString("fortran_source_derived_type_symbol") + ": " +
          rawExactBoundSymbolRef(
              aggregate->get_fortran_source_derived_type_symbol(), ids));
    }
    if (SgDesignator *designator = isSgDesignator(expr)) {
      designator->validate_designator();
      fields.push_back(rawIntegerField(
          "designator_kind", static_cast<int>(designator->get_kind())));
    }
    if (SgFunctionCallExp *call = isSgFunctionCallExp(expr)) {
      if (call->get_source_syntax() !=
              SgFunctionCallExp::e_source_function_call &&
          call->get_source_syntax() !=
              SgFunctionCallExp::e_implicit_conversion) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp has invalid source syntax");
      }
      const int source_operator_surface =
          static_cast<int>(call->get_source_operator_surface());
      if (source_operator_surface < SgFunctionCallExp::e_no_operator_surface ||
          source_operator_surface >
              SgFunctionCallExp::e_user_defined_literal_surface) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp has an invalid source operator "
            "surface");
      }
      const int source_operator_callee_form =
          static_cast<int>(call->get_source_operator_callee_form());
      if (source_operator_callee_form <
              SgFunctionCallExp::e_no_operator_callee_form ||
          source_operator_callee_form >
              SgFunctionCallExp::e_nonmember_operator_callee) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp has an invalid source operator "
            "callee form");
      }
      const SgUnsignedCharList &source_operator_operand_roles =
          call->get_source_operator_operand_roles();
      const std::string source_user_defined_literal_suffix =
          call->get_source_user_defined_literal_suffix().getString();
      SgExprListExp *source_user_defined_literal_operands =
          call->get_source_user_defined_literal_operands();
      const SgUnsignedCharList &source_user_defined_literal_suffix_roles =
          call->get_source_user_defined_literal_suffix_roles();
      const bool user_defined_literal =
          source_operator_surface ==
          SgFunctionCallExp::e_user_defined_literal_surface;
      const bool has_operator_surface =
          source_operator_surface != SgFunctionCallExp::e_no_operator_surface;
      if (has_operator_surface != call->get_uses_operator_syntax() ||
          (!has_operator_surface &&
           (source_operator_callee_form !=
                SgFunctionCallExp::e_no_operator_callee_form ||
            !source_operator_operand_roles.empty())) ||
          (has_operator_surface &&
           source_operator_callee_form ==
               SgFunctionCallExp::e_no_operator_callee_form) ||
          (call->get_source_syntax() ==
               SgFunctionCallExp::e_implicit_conversion &&
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
            source_user_defined_literal_operands == nullptr ||
            source_user_defined_literal_operands->get_parent() != call ||
            source_user_defined_literal_operands->get_expressions().empty() ||
            source_user_defined_literal_operands->get_expressions().size() !=
                source_user_defined_literal_suffix_roles.size() ||
            std::none_of(source_user_defined_literal_suffix_roles.begin(),
                         source_user_defined_literal_suffix_roles.end(),
                         [](unsigned char role) {
                           return role ==
                                  SgFunctionCallExp::
                                      e_user_defined_literal_token_with_suffix;
                         }))) ||
          (!user_defined_literal &&
           (source_user_defined_literal_operands != nullptr ||
            !source_user_defined_literal_suffix_roles.empty()))) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp has inconsistent operator source "
            "metadata");
      }
      if (has_operator_surface && (call->get_args() == nullptr ||
                                   call->get_args()->get_expressions().size() !=
                                       source_operator_operand_roles.size())) {
        throw std::runtime_error(
            "AST JSON SgFunctionCallExp operator roles do not match semantic "
            "arguments");
      }
      if (user_defined_literal) {
        for (SgExpression *lexical :
             source_user_defined_literal_operands->get_expressions()) {
          if (lexical == nullptr ||
              lexical->get_parent() != source_user_defined_literal_operands ||
              isSgValueExp(lexical) == nullptr ||
              isSgValueExp(lexical)->get_literal_spelling_form() !=
                  SgValueExp::e_literal_source_spelled) {
            throw std::runtime_error(
                "AST JSON SgFunctionCallExp has malformed UDL lexical "
                "ownership");
          }
        }
      }
      fields.push_back(rawBoolField("uses_operator_syntax",
                                    call->get_uses_operator_syntax()));
      fields.push_back(rawIntegerField(
          "source_syntax", static_cast<int>(call->get_source_syntax())));
      fields.push_back(
          rawIntegerField("source_operator_surface", source_operator_surface));
      fields.push_back(rawIntegerField("source_operator_callee_form",
                                       source_operator_callee_form));
      fields.push_back(
          jsonString("source_operator_operand_roles") + ": " +
          rawSourceOperatorOperandRolesJson(source_operator_operand_roles));
      fields.push_back(rawStringField("source_user_defined_literal_suffix",
                                      source_user_defined_literal_suffix));
      fields.push_back(
          jsonString("source_user_defined_literal_suffix_roles") + ": " +
          rawUdlSuffixRolesJson(source_user_defined_literal_suffix_roles));
      if (serializingTypeOwnedExpression) {
        if (call->get_function() == nullptr || call->get_args() == nullptr ||
            call->get_function()->get_parent() != call ||
            call->get_args()->get_parent() != call) {
          throw std::runtime_error(
              "AST JSON type-owned SgFunctionCallExp has malformed callee "
              "or argument-list ownership");
        }
        fields.push_back(jsonString("function") + ": " +
                         rawTypeOwnedExpressionRef(call->get_function(), ids));
        fields.push_back(jsonString("args") + ": " +
                         rawTypeOwnedExprListExpJson(call->get_args(), ids));
      }
    }
    if (SgSubscriptExpression *subscript = isSgSubscriptExpression(expr)) {
      if (serializingTypeOwnedExpression) {
        fields.push_back(
            jsonString("lower_bound") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_lowerBound(), ids));
        fields.push_back(
            jsonString("upper_bound") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_upperBound(), ids));
        fields.push_back(
            jsonString("stride") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_stride(), ids));
      }
    }
    if (serializingTypeOwnedExpression) {
      if (SgNewExp *new_expr = isSgNewExp(expr)) {
        auto require_owned_child = [&](SgExpression *child,
                                       const std::string &field) {
          if (child != nullptr && child->get_parent() != new_expr) {
            throw std::runtime_error(
                "AST JSON type-owned SgNewExp has a foreign " + field);
          }
        };
        require_owned_child(new_expr->get_placement_args(), "placement_args");
        require_owned_child(new_expr->get_constructor_args(),
                            "constructor_args");
        require_owned_child(new_expr->get_builtin_args(), "builtin_args");
        fields.push_back(
            jsonString("placement_args") + ": " +
            rawTypeOwnedExprListExpJson(new_expr->get_placement_args(), ids));
        fields.push_back(
            jsonString("constructor_args") + ": " +
            rawTypeOwnedExpressionRef(new_expr->get_constructor_args(), ids));
        fields.push_back(
            jsonString("builtin_args") + ": " +
            rawTypeOwnedExpressionRef(new_expr->get_builtin_args(), ids));
        if (new_expr->get_newOperatorDeclaration() != nullptr &&
            idFor(ids, new_expr->get_newOperatorDeclaration()) == 0) {
          throw std::runtime_error(
              "AST JSON type-owned SgNewExp operator declaration was not "
              "collected");
        }
        fields.push_back(rawIntegerField(
            "new_operator_declaration",
            idFor(ids, new_expr->get_newOperatorDeclaration())));
      }
      if (SgConstructorInitializer *initializer =
              isSgConstructorInitializer(expr)) {
        if (initializer->get_args() == nullptr ||
            initializer->get_args()->get_parent() != initializer) {
          throw std::runtime_error(
              "AST JSON type-owned SgConstructorInitializer has no exact "
              "owned argument list");
        }
        fields.push_back(
            jsonString("args") + ": " +
            rawTypeOwnedExprListExpJson(initializer->get_args(), ids));
        if (initializer->get_declaration() != nullptr &&
            idFor(ids, initializer->get_declaration()) == 0) {
          throw std::runtime_error(
              "AST JSON type-owned SgConstructorInitializer declaration was "
              "not collected");
        }
        fields.push_back(rawIntegerField(
            "declaration", idFor(ids, initializer->get_declaration())));
      }
      if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
        fields.push_back(
            jsonString("operand") + ": " +
            rawTypeOwnedExpressionRef(unary->get_operand_i(), ids));
      }
      if (SgBinaryOp *binary = isSgBinaryOp(expr)) {
        fields.push_back(
            jsonString("lhs_operand") + ": " +
            rawTypeOwnedExpressionRef(binary->get_lhs_operand_i(), ids));
        fields.push_back(
            jsonString("rhs_operand") + ": " +
            rawTypeOwnedExpressionRef(binary->get_rhs_operand_i(), ids));
      }
      if (SgAwaitExpression *await_expression = isSgAwaitExpression(expr)) {
        fields.push_back(
            jsonString("value") + ": " +
            rawTypeOwnedExpressionRef(await_expression->get_value(), ids));
      }
      if (SgFoldExpression *fold = isSgFoldExpression(expr)) {
        fields.push_back(jsonString("operands") + ": " +
                         rawTypeOwnedExpressionRef(fold->get_operands(), ids));
      }
      if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(expr)) {
        fields.push_back(
            jsonString("pattern_expression") + ": " +
            rawTypeOwnedExpressionRef(pack->get_pattern_expression(), ids));
      }
      if (SgNoexceptOp *noexcept_op = isSgNoexceptOp(expr)) {
        if (noexcept_op->get_operand_expr() == nullptr ||
            noexcept_op->get_operand_expr()->get_parent() != noexcept_op) {
          throw std::runtime_error(
              "AST JSON type-owned SgNoexceptOp has no exact owned operand");
        }
        fields.push_back(
            jsonString("operand_expr") + ": " +
            rawTypeOwnedExpressionRef(noexcept_op->get_operand_expr(), ids));
      }
      if (SgSizeOfOp *size_of = isSgSizeOfOp(expr)) {
        if (size_of->get_type_defining_declaration() != nullptr) {
          throw std::runtime_error(
              "AST JSON type-owned SgSizeOfOp cannot contain an inline "
              "defining declaration");
        }
        fields.push_back(
            jsonString("operand_expr") + ": " +
            rawTypeOwnedExpressionRef(size_of->get_operand_expr(), ids));
      }
      if (SgAlignOfOp *align_of = isSgAlignOfOp(expr)) {
        if (align_of->get_type_defining_declaration() != nullptr) {
          throw std::runtime_error(
              "AST JSON type-owned SgAlignOfOp cannot contain an inline "
              "defining declaration");
        }
        fields.push_back(
            jsonString("operand_expr") + ": " +
            rawTypeOwnedExpressionRef(align_of->get_operand_expr(), ids));
      }
    }
  }

  if (SgClinkageDeclarationStatement *linkage =
          isSgClinkageDeclarationStatement(node)) {
    const std::string &language = linkage->get_languageSpecifier();
    if (language != "C" && language != "C++") {
      throw std::runtime_error(
          "AST JSON C linkage marker has invalid language specifier");
    }
    fields.push_back(rawStringField("language_specifier", language));
  }

  if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    fields.push_back(
        rawIntegerField("argument_type", argument->get_argumentType()));
    fields.push_back(rawBoolField("is_array_bound_unknown_type",
                                  argument->get_isArrayBoundUnknownType()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(argument->get_type(), ids));
    fields.push_back(jsonString("source_spelled_type") + ": " +
                     rawTypeJson(argument->get_sourceSpelledType(), ids));
    fields.push_back(
        rawBoolField("source_type_qualification_present",
                     argument->get_source_type_qualification_present()));
    fields.push_back(
        rawBoolField("source_type_global_qualification",
                     argument->get_source_type_global_qualification()));
    fields.push_back(
        jsonString("source_type_qualification_tokens") + ": " +
        rawStringListJson(argument->get_source_type_qualification_tokens()));
    fields.push_back(jsonString("expression") + ": " +
                     rawExpressionRef(argument->get_expression(), ids));
    fields.push_back(
        rawIntegerField("template_declaration",
                        idFor(ids, argument->get_templateDeclaration())));
    fields.push_back(rawIntegerField(
        "initialized_name", idFor(ids, argument->get_initializedName())));
    fields.push_back(rawBoolField("explicitly_specified",
                                  argument->get_explicitlySpecified()));
    fields.push_back(
        rawBoolField("is_pack_element", argument->get_is_pack_element()));
  }
  if (SgTemplateParameterList *list = isSgTemplateParameterList(node)) {
    switch (list->get_source_header_separator()) {
    case SgTemplateParameterList::e_source_header_separator_space:
    case SgTemplateParameterList::e_source_header_separator_newline:
      break;
    case SgTemplateParameterList::e_source_header_separator_unset:
    default:
      throw std::runtime_error(
          "AST JSON SgTemplateParameterList has no exact source header "
          "separator");
    }
    fields.push_back(rawIntegerField("source_header_separator",
                                     list->get_source_header_separator()));
  }
  if (SgTemplateDeclaration *declaration = isSgTemplateDeclaration(node)) {
    switch (declaration->get_template_kind()) {
    case SgTemplateDeclaration::e_template_none:
    case SgTemplateDeclaration::e_template_class:
    case SgTemplateDeclaration::e_template_m_class:
    case SgTemplateDeclaration::e_template_function:
    case SgTemplateDeclaration::e_template_m_function:
    case SgTemplateDeclaration::e_template_m_data:
    case SgTemplateDeclaration::e_template_variable:
      break;
    default:
      throw std::runtime_error(
          "AST JSON SgTemplateDeclaration has an invalid template kind");
    }
    fields.push_back(
        rawStringField("name", declaration->get_name().getString()));
    fields.push_back(
        rawIntegerField("template_kind", declaration->get_template_kind()));
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    validateTemplateParameterContract(parameter, "serialization");
    fields.push_back(
        rawIntegerField("parameter_type", parameter->get_parameterType()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(parameter->get_type(), ids));
    fields.push_back(jsonString("default_type_parameter") + ": " +
                     rawTypeJson(parameter->get_defaultTypeParameter(), ids));
    fields.push_back(jsonString("expression") + ": " +
                     rawExpressionRef(parameter->get_expression(), ids));
    fields.push_back(jsonString("type_constraint") + ": " +
                     rawExpressionRef(parameter->get_typeConstraint(), ids));
    fields.push_back(
        jsonString("source_type_constraint") + ": " +
        rawExpressionRef(parameter->get_sourceTypeConstraint(), ids));
    fields.push_back(
        jsonString("default_expression_parameter") + ": " +
        rawExpressionRef(parameter->get_defaultExpressionParameter(), ids));
    fields.push_back(
        rawIntegerField("template_declaration",
                        idFor(ids, parameter->get_templateDeclaration())));
    fields.push_back(rawIntegerField(
        "source_spelled_template_declaration",
        idFor(ids, parameter->get_sourceSpelledTemplateDeclaration())));
    fields.push_back(rawIntegerField(
        "default_template_declaration_parameter",
        idFor(ids, parameter->get_defaultTemplateDeclarationParameter())));
    fields.push_back(rawIntegerField(
        "initialized_name", idFor(ids, parameter->get_initializedName())));
    fields.push_back(
        rawIntegerField("template_parameter_keyword",
                        parameter->get_templateParameterKeyword()));
    fields.push_back(
        rawBoolField("is_abbreviated_function_template_parameter",
                     parameter->get_isAbbreviatedFunctionTemplateParameter()));
    fields.push_back(
        rawBoolField("is_parameter_pack", parameter->get_is_parameter_pack()));
  }

  if (SgActualArgumentExpression *actual = isSgActualArgumentExpression(node)) {
    fields.push_back(rawStringField("argument_name",
                                    actual->get_argument_name().getString()));
  }

  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    fields.push_back(rawIntegerField("symbol_declaration",
                                     varRefSymbolDeclarationId(ref, ids)));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
  } else if (SgLabelRefExp *ref = isSgLabelRefExp(node)) {
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error("AST JSON SgLabelRefExp has no symbol");
    }
    fields.push_back(rawIntegerField(
        "symbol_declaration", idFor(ids, symbolBasis(ref->get_symbol()))));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
    SgFunctionSymbol *symbol = ref->get_symbol();
    SgFunctionDeclaration *decl =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    const uint64_t decl_id = idFor(ids, decl);
    const bool external_decl =
        decl != nullptr && (isAstJsonExternalFunction(decl) ||
                            (decl_id == 0 && !insideCollectionBoundary(decl)));
    if (symbol == nullptr || (decl_id == 0 && !external_decl)) {
      std::ostringstream message;
      message
          << "AST JSON SgFunctionRefExp symbol declaration was not collected";
      if (symbol != nullptr) {
        message << ": " << symbol->get_name().getString();
        if (decl != nullptr) {
          message << " declaration=" << decl->sage_class_name() << "("
                  << decl->get_name().getString() << ")";
          if (SgNode *parent = decl->get_parent()) {
            message << " declaration_parent=" << parent->sage_class_name();
          }
          std::ostringstream declaration_chain;
          for (SgNode *current = decl; current != nullptr;
               current = current->get_parent()) {
            if (declaration_chain.tellp() > 0) {
              declaration_chain << " <- ";
            }
            declaration_chain << current->sage_class_name();
          }
          message << " declaration_chain=[" << declaration_chain.str() << "]";
        }
      }
      std::ostringstream parent_chain;
      for (SgNode *current = ref; current != nullptr;
           current = current->get_parent()) {
        if (parent_chain.tellp() > 0) {
          parent_chain << " <- ";
        }
        parent_chain << current->sage_class_name();
      }
      message << " parent_chain=[" << parent_chain.str() << "]";
      throw std::runtime_error(message.str());
    }
    fields.push_back(
        rawIntegerField("symbol_declaration", external_decl ? 0 : decl_id));
    fields.push_back(
        rawStringField("symbol_name", symbol->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));
    fields.push_back(jsonString("external_function") + ": " +
                     rawExternalFunctionJson(decl, ids));
    fields.push_back(
        jsonString("fortran_source_visible_symbol") + ": " +
        rawExactBoundSymbolRef(ref->get_fortran_source_visible_symbol(), ids));
    fields.push_back(
        rawIntegerField("fortran_source_visible_binding_kind",
                        ref->get_fortran_source_visible_binding_kind()));
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(node)) {
    if (ref->get_symbol() == nullptr ||
        idFor(ids, ref->get_symbol()->get_declaration()) == 0) {
      throw std::runtime_error("AST JSON SgTemplateFunctionRefExp symbol "
                               "declaration was not collected");
    }
    fields.push_back(
        rawIntegerField("symbol_declaration",
                        idFor(ids, ref->get_symbol()->get_declaration())));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
    SgFunctionDeclaration *semantic_function =
        ref->get_semantic_function_declaration();
    if (semantic_function == nullptr || idFor(ids, semantic_function) == 0 ||
        ref->getAssociatedFunctionDeclaration() != semantic_function) {
      throw std::runtime_error(
          "AST JSON SgTemplateFunctionRefExp semantic function declaration "
          "was not collected or is inconsistent");
    }
    fields.push_back(rawIntegerField("semantic_function_declaration",
                                     idFor(ids, semantic_function)));
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
    if (ref->get_symbol_i() == nullptr ||
        idFor(ids, ref->get_symbol_i()->get_declaration()) == 0) {
      throw std::runtime_error("AST JSON SgMemberFunctionRefExp symbol "
                               "declaration was not collected");
    }
    fields.push_back(
        rawIntegerField("symbol_declaration",
                        idFor(ids, ref->get_symbol_i()->get_declaration())));
    fields.push_back(rawStringField(
        "symbol_name", ref->get_symbol_i()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol_i(), ids));
    fields.push_back(rawIntegerField("virtual_call", ref->get_virtual_call()));
    fields.push_back(
        rawIntegerField("need_qualifier", ref->get_need_qualifier()));
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(node)) {
    if (ref->get_symbol() == nullptr ||
        idFor(ids, ref->get_symbol()->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgTemplateMemberFunctionRefExp symbol declaration was "
          "not collected");
    }
    fields.push_back(
        rawIntegerField("symbol_declaration",
                        idFor(ids, ref->get_symbol()->get_declaration())));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
    fields.push_back(rawIntegerField("virtual_call", ref->get_virtual_call()));
    fields.push_back(
        rawIntegerField("need_qualifier", ref->get_need_qualifier()));
    SgMemberFunctionDeclaration *semantic_function =
        ref->get_semantic_member_function_declaration();
    if (semantic_function == nullptr || idFor(ids, semantic_function) == 0 ||
        ref->getAssociatedMemberFunctionDeclaration() != semantic_function) {
      throw std::runtime_error(
          "AST JSON SgTemplateMemberFunctionRefExp semantic member-function "
          "declaration was not collected or is inconsistent");
    }
    fields.push_back(rawIntegerField("semantic_member_function_declaration",
                                     idFor(ids, semantic_function)));
  } else if (SgThisExp *expr = isSgThisExp(node)) {
    SgClassSymbol *class_symbol = expr->get_class_symbol();
    SgNonrealSymbol *nonreal_symbol = expr->get_nonreal_symbol();
    if ((class_symbol == nullptr) == (nonreal_symbol == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgThisExp must own exactly one class or nonreal symbol");
    }
    if (class_symbol != nullptr &&
        idFor(ids, class_symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgThisExp class symbol declaration was not collected");
    }
    if (nonreal_symbol != nullptr &&
        idFor(ids, nonreal_symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgThisExp nonreal symbol declaration was not collected");
    }
    fields.push_back(rawIntegerField(
        "class_symbol_declaration",
        idFor(ids, class_symbol != nullptr ? class_symbol->get_declaration()
                                           : nullptr)));
    fields.push_back(rawStringField(
        "class_symbol_name",
        class_symbol != nullptr ? class_symbol->get_name().getString() : ""));
    fields.push_back(rawIntegerField(
        "nonreal_symbol_declaration",
        idFor(ids, nonreal_symbol != nullptr ? nonreal_symbol->get_declaration()
                                             : nullptr)));
    fields.push_back(rawStringField("nonreal_symbol_name",
                                    nonreal_symbol != nullptr
                                        ? nonreal_symbol->get_name().getString()
                                        : ""));
    fields.push_back(rawIntegerField("pobj_this", expr->get_pobj_this()));
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    SgNonrealSymbol *symbol = ref->get_symbol();
    if (symbol == nullptr || idFor(ids, symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp symbol declaration was not collected");
    }
    fields.push_back(rawIntegerField("symbol_declaration",
                                     idFor(ids, symbol->get_declaration())));
    fields.push_back(
        rawStringField("symbol_name", symbol->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));
    SgFunctionDeclaration *resolved_function =
        ref->get_resolved_function_declaration();
    SgTemplateVariableDeclaration *resolved_variable =
        ref->get_resolved_variable_declaration();
    if (resolved_function != nullptr && resolved_variable != nullptr) {
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp resolves to both a function and a "
          "variable template");
    }
    if (resolved_function != nullptr && idFor(ids, resolved_function) == 0) {
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp resolved function declaration was not "
          "collected");
    }
    if (resolved_function != nullptr) {
      SageInterface::requireResolvedFunctionTemplateReference(
          ref, "AST JSON serialization");
    }
    fields.push_back(rawIntegerField("resolved_function_declaration",
                                     idFor(ids, resolved_function)));
    if (resolved_variable != nullptr && idFor(ids, resolved_variable) == 0) {
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp resolved variable declaration was not "
          "collected");
    }
    if (resolved_variable != nullptr) {
      SageInterface::requireResolvedVariableTemplateReference(
          ref, "AST JSON serialization");
    }
    fields.push_back(rawIntegerField("resolved_variable_declaration",
                                     idFor(ids, resolved_variable)));
    fields.push_back(rawIntegerField(
        "semantic_role", static_cast<int>(ref->get_semantic_role())));
    fields.push_back(
        jsonString("template_arguments") + ": " +
        rawTemplateArgumentListJson(ref->get_templateArguments(), ids));
    fields.push_back(rawBoolField("explicit_template_argument_list",
                                  ref->get_explicit_template_argument_list()));
    fields.push_back(rawBoolField("constraint_satisfaction_evaluated",
                                  ref->get_constraintSatisfactionEvaluated()));
    fields.push_back(rawBoolField("constraint_satisfaction_satisfied",
                                  ref->get_constraintSatisfactionSatisfied()));
    fields.push_back(
        rawBoolField("constraint_satisfaction_contains_errors",
                     ref->get_constraintSatisfactionContainsErrors()));
    fields.push_back(
        rawBoolField("constraint_satisfaction_substitution_failure",
                     ref->get_constraintSatisfactionSubstitutionFailure()));
    fields.push_back(rawStringField("constraint_satisfaction_summary",
                                    ref->get_constraintSatisfactionSummary()));
    fields.push_back(
        rawBoolField("sfinae_evaluated", ref->get_sfinaeEvaluated()));
    fields.push_back(rawBoolField("sfinae_substitution_failure",
                                  ref->get_sfinaeSubstitutionFailure()));
    fields.push_back(
        rawStringField("sfinae_summary", ref->get_sfinaeSummary()));
  } else if (SgOmpNameExpression *name = isSgOmpNameExpression(node)) {
    fields.push_back(rawStringField("spelling", name->get_spelling()));
  } else if (SgOmpSourceExpression *source = isSgOmpSourceExpression(node)) {
    fields.push_back(rawStringField("spelling", source->get_spelling()));
  } else if (SgOmpIteratorDefinition *definition =
                 isSgOmpIteratorDefinition(node)) {
    if (definition->get_iterator_name() == nullptr ||
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
      throw std::runtime_error(
          "AST JSON SgOmpIteratorDefinition has invalid typed child "
          "ownership");
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
          "AST JSON SgOmpIteratorDefinition aliases one syntax node across "
          "roles");
    }
  } else if (SgOmpInductionItem *item = isSgOmpInductionItem(node)) {
    fields.push_back(rawIntegerField("kind", item->get_kind()));
    fields.push_back(rawStringField("label", item->get_label()));
  } else if (SgOmpApplyTransformation *item =
                 isSgOmpApplyTransformation(node)) {
    fields.push_back(rawIntegerField("kind", item->get_kind()));
    fields.push_back(rawIntegerField("separator", item->get_separator()));
    fields.push_back(
        rawStringField("transformation_name", item->get_transformation_name()));
  } else if (SgOmpInitModifier *modifier = isSgOmpInitModifier(node)) {
    fields.push_back(rawIntegerField("kind", modifier->get_kind()));
  } else if (SgOmpMapDistDataPolicy *policy = isSgOmpMapDistDataPolicy(node)) {
    fields.push_back(rawIntegerField("policy", policy->get_policy()));
  } else if (SgBoolValExp *value = isSgBoolValExp(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
  } else if (SgShortVal *value = isSgShortVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedShortVal *value = isSgUnsignedShortVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgIntVal *value = isSgIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedIntVal *value = isSgUnsignedIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgLongIntVal *value = isSgLongIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedLongVal *value = isSgUnsignedLongVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgLongLongIntVal *value = isSgLongLongIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedLongLongIntVal *value =
                 isSgUnsignedLongLongIntVal(node)) {
    fields.push_back(
        rawIntegerField("value", static_cast<int64_t>(value->get_value())));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgCharVal *value = isSgCharVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedCharVal *value = isSgUnsignedCharVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgFloatVal *value = isSgFloatVal(node)) {
    fields.push_back(rawStringField("value", value->get_valueString()));
  } else if (SgDoubleVal *value = isSgDoubleVal(node)) {
    fields.push_back(rawStringField("value", value->get_valueString()));
  } else if (SgComplexVal *value = isSgComplexVal(node)) {
    fields.push_back(jsonString("precision_type") + ": " +
                     rawTypeJson(value->get_precisionType(), ids));
    fields.push_back(jsonString("real_value") + ": " +
                     rawTypeOwnedExpressionRef(value->get_real_value(), ids));
    fields.push_back(
        jsonString("imaginary_value") + ": " +
        rawTypeOwnedExpressionRef(value->get_imaginary_value(), ids));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgStringVal *value = isSgStringVal(node)) {
    fields.push_back(rawStringField("value", value->get_value()));
    fields.push_back(rawIntegerField(
        "literal_encoding", static_cast<int>(value->get_literal_encoding())));
    fields.push_back(
        rawBoolField("cxx_unevaluated", value->get_cxx_unevaluated()));
    fields.push_back(
        rawIntegerField("string_delimiter", value->get_stringDelimiter()));
    fields.push_back(rawBoolField("is_raw_string", value->get_isRawString()));
    fields.push_back(rawStringField("raw_string_delimiter",
                                    value->get_raw_string_delimiter()));
    fields.push_back(
        rawStringField("raw_string_payload", value->get_raw_string_payload()));
  } else if (SgEnumVal *value = isSgEnumVal(node)) {
    if (value->get_declaration() == nullptr ||
        idFor(ids, value->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgEnumVal has no collected enum declaration");
    }
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("name", value->get_name().getString()));
    fields.push_back(
        rawIntegerField("declaration", idFor(ids, value->get_declaration())));
    fields.push_back(rawBoolField("requires_name_qualification",
                                  value->get_requiresNameQualification()));
  } else if (SgTemplateParameterVal *value = isSgTemplateParameterVal(node)) {
    fields.push_back(rawIntegerField("template_parameter_position",
                                     value->get_template_parameter_position()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgCastExp *cast = isSgCastExp(node)) {
    cast->validate_semantic_conversion();
    if (serializingTypeOwnedExpression &&
        cast->get_type_defining_declaration() != nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgCastExp cannot contain an inline defining "
          "declaration");
    }
    const bool explicit_cast =
        cast->get_cast_type() != SgCastExp::e_implicit_cast;
    if (explicit_cast != (cast->get_source_type() != nullptr) ||
        (!cast->get_explicit_name_qualification_present() &&
         (cast->get_explicit_global_qualification() ||
          !cast->get_explicit_name_qualification_tokens().empty())) ||
        (cast->get_explicit_name_qualification_present() !=
         (cast->get_source_type_elaboration_kind() !=
          SgNonrealDecl::e_source_elaboration_unspecified))) {
      std::ostringstream message;
      message << "AST JSON SgCastExp has inconsistent written type provenance"
              << " cast=" << cast
              << " cast_type=" << static_cast<int>(cast->get_cast_type())
              << " source_type=" << cast->get_source_type()
              << " result_type=" << cast->get_type() << " qualifier_present="
              << cast->get_explicit_name_qualification_present()
              << " global=" << cast->get_explicit_global_qualification()
              << " qualifier_tokens="
              << cast->get_explicit_name_qualification_tokens().size()
              << " parent=" << cast->get_parent() << "/"
              << (cast->get_parent() != nullptr
                      ? cast->get_parent()->class_name()
                      : std::string("<null>"))
              << " file="
              << (cast->get_file_info() != nullptr
                      ? cast->get_file_info()->get_filenameString()
                      : std::string("<null>"))
              << ":"
              << (cast->get_file_info() != nullptr
                      ? cast->get_file_info()->get_line()
                      : -1)
              << ":"
              << (cast->get_file_info() != nullptr
                      ? cast->get_file_info()->get_col()
                      : -1)
              << " compiler_generated="
              << (cast->get_file_info() != nullptr &&
                  cast->get_file_info()->isCompilerGenerated())
              << " transformation="
              << (cast->get_file_info() != nullptr &&
                  cast->get_file_info()->isTransformation());
      throw std::runtime_error(message.str());
    }
    if (cast->get_type_defining_declaration() != nullptr &&
        cast->get_type_defining_declaration()->get_parent() != cast) {
      throw std::runtime_error(
          "AST JSON SgCastExp has malformed inline type ownership");
    }
    fields.push_back(rawIntegerField("cast_type", cast->cast_type()));
    fields.push_back(jsonString("source_type") + ": " +
                     rawTypeJson(cast->get_source_type(), ids));
    fields.push_back(
        rawBoolField("explicit_name_qualification_present",
                     cast->get_explicit_name_qualification_present()));
    fields.push_back(rawBoolField("explicit_global_qualification",
                                  cast->get_explicit_global_qualification()));
    fields.push_back(
        jsonString("explicit_name_qualification_tokens") + ": " +
        rawStringListJson(cast->get_explicit_name_qualification_tokens()));
    fields.push_back(rawIntegerField("source_type_elaboration_kind",
                                     cast->get_source_type_elaboration_kind()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  cast->get_type_elaboration_required()));
    fields.push_back(rawIntegerField(
        "semantic_conversion_kind",
        static_cast<int>(cast->get_semantic_conversion_kind())));
    fields.push_back(rawIntegerField(
        "value_category", static_cast<int>(cast->get_value_category())));
    std::ostringstream base_path;
    base_path << jsonString("conversion_base_path") << ": [";
    const SgTypePtrList &base_types = cast->get_conversion_base_path();
    if (!base_types.empty()) {
      base_path << '\n';
      for (size_t i = 0; i < base_types.size(); ++i) {
        indent(base_path, 8);
        base_path << rawTypeJson(base_types[i], ids);
        if (i + 1 != base_types.size()) {
          base_path << ',';
        }
        base_path << '\n';
      }
      indent(base_path, 6);
    }
    base_path << ']';
    fields.push_back(base_path.str());
  } else if (SgConstructorInitializer *init =
                 isSgConstructorInitializer(node)) {
    if ((!init->get_explicit_name_qualification_present() &&
         (init->get_explicit_global_qualification() ||
          !init->get_explicit_name_qualification_tokens().empty())) ||
        (init->get_explicit_name_qualification_present() !=
         (init->get_source_type_elaboration_kind() !=
          SgNonrealDecl::e_source_elaboration_unspecified))) {
      throw std::runtime_error(
          "AST JSON SgConstructorInitializer has inconsistent written type "
          "provenance");
    }
    fields.push_back(rawBoolField("need_name", init->get_need_name()));
    fields.push_back(
        rawBoolField("need_qualifier", init->get_need_qualifier()));
    fields.push_back(rawBoolField("need_parenthesis_after_name",
                                  init->get_need_parenthesis_after_name()));
    fields.push_back(rawBoolField("associated_class_unknown",
                                  init->get_associated_class_unknown()));
    fields.push_back(
        rawBoolField("explicit_name_qualification_present",
                     init->get_explicit_name_qualification_present()));
    fields.push_back(rawBoolField("explicit_global_qualification",
                                  init->get_explicit_global_qualification()));
    fields.push_back(
        jsonString("explicit_name_qualification_tokens") + ": " +
        rawStringListJson(init->get_explicit_name_qualification_tokens()));
    fields.push_back(rawIntegerField("source_type_elaboration_kind",
                                     init->get_source_type_elaboration_kind()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  init->get_type_elaboration_required()));
  } else if (SgBreakStmt *stmt = isSgBreakStmt(node)) {
    fields.push_back(
        rawStringField("do_string_label", stmt->get_do_string_label()));
  } else if (SgContinueStmt *stmt = isSgContinueStmt(node)) {
    fields.push_back(
        rawStringField("do_string_label", stmt->get_do_string_label()));
  } else if (SgProcessControlStatement *stmt =
                 isSgProcessControlStatement(node)) {
    fields.push_back(rawIntegerField("control_kind", stmt->get_control_kind()));
  } else if (SgAttributeSpecificationStatement *stmt =
                 isSgAttributeSpecificationStatement(node)) {
    fields.push_back(
        rawIntegerField("attribute_kind", stmt->get_attribute_kind()));
    fields.push_back(rawIntegerField("intent", stmt->get_intent()));
    fields.push_back(jsonString("name_list") + ": " +
                     rawStringListJson(stmt->get_name_list()));
  } else if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
    fields.push_back(rawIntegerField("generic_spec", stmt->get_generic_spec()));
  } else if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    fields.push_back(
        rawStringField("function_name", body->get_function_name().getString()));
    fields.push_back(
        rawBoolField("use_function_name", body->get_use_function_name()));
    fields.push_back(rawIntegerField(
        "function_declaration", idFor(ids, body->get_functionDeclaration())));
  } else if (SgCaseOptionStmt *stmt = isSgCaseOptionStmt(node)) {
    fields.push_back(
        rawStringField("case_construct_name", stmt->get_case_construct_name()));
  } else if (SgDefaultOptionStmt *stmt = isSgDefaultOptionStmt(node)) {
    fields.push_back(rawStringField("default_construct_name",
                                    stmt->get_default_construct_name()));
  } else if (SgPseudoDestructorRefExp *pseudo =
                 isSgPseudoDestructorRefExp(node)) {
    if (pseudo->get_object_type() == nullptr || pseudo->get_type() == nullptr) {
      throw std::runtime_error(
          "AST JSON SgPseudoDestructorRefExp has incomplete type identity");
    }
    fields.push_back(jsonString("object_type") + ": " +
                     rawTypeJson(pseudo->get_object_type(), ids));
  } else if (SgSizeOfOp *size_of = isSgSizeOfOp(node)) {
    if ((size_of->get_operand_expr() == nullptr) ==
        (size_of->get_operand_type() == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgSizeOfOp must have exactly one operand");
    }
    if (size_of->get_type_defining_declaration() != nullptr &&
        (size_of->get_operand_type() == nullptr ||
         size_of->get_type_defining_declaration()->get_parent() != size_of)) {
      throw std::runtime_error(
          "AST JSON SgSizeOfOp has malformed inline type ownership");
    }
    fields.push_back(jsonString("operand_type") + ": " +
                     rawTypeJson(size_of->get_operand_type(), ids));
    fields.push_back(rawBoolField(
        "is_objectless_nonstatic_data_member_reference",
        size_of->get_is_objectless_nonstatic_data_member_reference()));
    fields.push_back(
        rawBoolField("is_sizeof_pack", size_of->get_is_sizeof_pack()));
  } else if (SgAlignOfOp *align_of = isSgAlignOfOp(node)) {
    if ((align_of->get_operand_expr() == nullptr) ==
        (align_of->get_operand_type() == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgAlignOfOp must have exactly one operand");
    }
    if (align_of->get_type_defining_declaration() != nullptr &&
        (align_of->get_operand_type() == nullptr ||
         align_of->get_type_defining_declaration()->get_parent() != align_of)) {
      throw std::runtime_error(
          "AST JSON SgAlignOfOp has malformed inline type ownership");
    }
    fields.push_back(jsonString("operand_type") + ": " +
                     rawTypeJson(align_of->get_operand_type(), ids));
  } else if (SgRequiresExpr *requires_expr = isSgRequiresExpr(node)) {
    if (requires_expr->get_requirements() == nullptr ||
        requires_expr->get_requirements()->get_parent() != requires_expr ||
        requires_expr->get_requirements()->get_expressions().empty() ||
        (requires_expr->get_local_parameter_list() != nullptr &&
         requires_expr->get_local_parameter_list()->get_parent() !=
             requires_expr)) {
      throw std::runtime_error(
          "AST JSON SgRequiresExpr has malformed typed ownership");
    }
  } else if (SgSimpleRequirement *requirement = isSgSimpleRequirement(node)) {
    if (requirement->get_expression() == nullptr ||
        requirement->get_expression()->get_parent() != requirement) {
      throw std::runtime_error(
          "AST JSON SgSimpleRequirement has no exact owned expression");
    }
  } else if (SgRequirementSubstitutionFailure *failure =
                 isSgRequirementSubstitutionFailure(node)) {
    if (failure->get_failure_kind() <
            SgRequirementSubstitutionFailure::e_simple_expression_failure ||
        failure->get_failure_kind() >
            SgRequirementSubstitutionFailure::e_compound_return_type_failure ||
        failure->get_substituted_entity().empty()) {
      throw std::runtime_error(
          "AST JSON SgRequirementSubstitutionFailure has malformed exact "
          "semantic state");
    }
    fields.push_back(
        rawIntegerField("failure_kind", failure->get_failure_kind()));
    fields.push_back(rawStringField("substituted_entity",
                                    failure->get_substituted_entity()));
    fields.push_back(rawStringField("diagnostic_message",
                                    failure->get_diagnostic_message()));
  } else if (SgTypeRequirement *requirement = isSgTypeRequirement(node)) {
    if (requirement->get_required_type() == nullptr) {
      throw std::runtime_error(
          "AST JSON SgTypeRequirement has no exact required type");
    }
    fields.push_back(jsonString("required_type") + ": " +
                     rawTypeJson(requirement->get_required_type(), ids));
  } else if (SgCompoundRequirement *requirement =
                 isSgCompoundRequirement(node)) {
    if (requirement->get_expression() == nullptr ||
        requirement->get_expression()->get_parent() != requirement ||
        (requirement->get_type_constraint() != nullptr &&
         requirement->get_type_constraint()->get_parent() != requirement)) {
      throw std::runtime_error(
          "AST JSON SgCompoundRequirement has malformed typed ownership");
    }
    fields.push_back(rawBoolField("noexcept_required",
                                  requirement->get_noexcept_required()));
  } else if (SgNestedRequirement *requirement = isSgNestedRequirement(node)) {
    if (requirement->get_constraint() == nullptr ||
        requirement->get_constraint()->get_parent() != requirement) {
      throw std::runtime_error(
          "AST JSON SgNestedRequirement has no exact owned constraint");
    }
  } else if (SgOmpClause *clause = isSgOmpClause(node)) {
    fields.push_back(rawIntegerField("directive_name_modifier",
                                     clause->get_directive_name_modifier()));
  }

  if (SgUnaryOp *unary = isSgUnaryOp(node)) {
    fields.push_back(rawIntegerField("mode", unary->get_mode()));
  }

  if (SgThrowOp *throw_op = isSgThrowOp(node)) {
    if (throw_op->get_throwKind() != SgThrowOp::throw_expression &&
        throw_op->get_throwKind() != SgThrowOp::rethrow) {
      throw std::runtime_error(
          "AST JSON cannot serialize SgThrowOp with unknown throw kind");
    }
    fields.push_back(rawIntegerField("throw_kind", throw_op->get_throwKind()));
  }

  if (SgOmpDirectiveKindClause *clause = isSgOmpDirectiveKindClause(node)) {
    fields.push_back(jsonString("directive_kinds") + ": " +
                     rawOmpDirectiveKindsJson(clause->get_directive_kinds()));
  } else if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    switch (clause->get_operation()) {
    case SgOmpClause::e_omp_map_unknown:
    case SgOmpClause::e_omp_map_alloc:
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
    case SgOmpClause::e_omp_map_storage:
    case SgOmpClause::e_omp_map_release:
    case SgOmpClause::e_omp_map_delete:
    case SgOmpClause::e_omp_map_present:
    case SgOmpClause::e_omp_map_self:
      break;
    default:
      throw std::runtime_error(
          "AST JSON OpenMP map clause has an invalid typed operation");
    }
    fields.push_back(rawIntegerField("operation", clause->get_operation()));
    fields.push_back(rawIntegerField("modifier1", clause->get_modifier1()));
    fields.push_back(rawIntegerField("modifier2", clause->get_modifier2()));
    fields.push_back(rawIntegerField("modifier3", clause->get_modifier3()));
    const SgOmpClause::omp_map_modifier_enum modifiers[] = {
        clause->get_modifier1(), clause->get_modifier2(),
        clause->get_modifier3()};
    bool saw_unspecified_modifier = false;
    std::set<int> unique_modifiers;
    size_t mapper_modifier_count = 0;
    size_t iterator_modifier_count = 0;
    for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
      switch (modifier) {
      case SgOmpClause::e_omp_map_modifier_unspecified:
        saw_unspecified_modifier = true;
        continue;
      case SgOmpClause::e_omp_map_modifier_always:
      case SgOmpClause::e_omp_map_modifier_close:
      case SgOmpClause::e_omp_map_modifier_present:
      case SgOmpClause::e_omp_map_modifier_self:
      case SgOmpClause::e_omp_map_modifier_mapper:
      case SgOmpClause::e_omp_map_modifier_iterator:
        break;
      default:
        throw std::runtime_error(
            "AST JSON OpenMP map clause has an invalid modifier");
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
    if (mapper_modifier_count > 1 || iterator_modifier_count > 1) {
      throw std::runtime_error(
          "AST JSON OpenMP map clause has duplicate mapper or iterator "
          "modifiers");
    }
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if ((mapper_modifier_count == 1) != (mapper_identifier != nullptr) ||
        (mapper_identifier != nullptr &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP map mapper modifier and structural identifier "
          "disagree");
    }
    validate_iterator_definitions(clause, clause->get_iterator_definitions(),
                                  iterator_modifier_count == 1);
  } else if (SgOmpDeviceClause *clause = isSgOmpDeviceClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpDefaultClause *clause = isSgOmpDefaultClause(node)) {
    fields.push_back(
        rawIntegerField("data_sharing", clause->get_data_sharing()));
  } else if (SgOmpProcBindClause *clause = isSgOmpProcBindClause(node)) {
    fields.push_back(rawIntegerField("policy", clause->get_policy()));
  } else if (SgOmpIfClause *clause = isSgOmpIfClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpLastprivateClause *clause = isSgOmpLastprivateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         (clause->get_user_defined_identifier()->get_spelling().empty() ||
          clause->get_user_defined_identifier()->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP reduction kind and structural user identifier "
          "disagree");
    }
  } else if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpScheduleClause *clause = isSgOmpScheduleClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(rawIntegerField("modifier1", clause->get_modifier1()));
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpDistScheduleClause *clause =
                 isSgOmpDistScheduleClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpOrderClause *clause = isSgOmpOrderClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpAtomicDefaultMemOrderClause *clause =
                 isSgOmpAtomicDefaultMemOrderClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpDefaultmapClause *clause = isSgOmpDefaultmapClause(node)) {
    fields.push_back(rawIntegerField("behavior", clause->get_behavior()));
    fields.push_back(rawIntegerField("category", clause->get_category()));
  } else if (SgOmpBindClause *clause = isSgOmpBindClause(node)) {
    fields.push_back(rawIntegerField("binding", clause->get_binding()));
  } else if (SgOmpFailClause *clause = isSgOmpFailClause(node)) {
    fields.push_back(
        rawIntegerField("memory_order", clause->get_memory_order()));
  } else if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(
        rawBoolField("uses_allocator_modifier_syntax",
                     clause->get_uses_allocator_modifier_syntax()));
    fields.push_back(
        jsonString("user_defined_modifier") + ": " +
        rawExpressionRef(clause->get_user_defined_modifier(), ids));
    fields.push_back(jsonString("alignment") + ": " +
                     rawExpressionRef(clause->get_alignment(), ids));
  } else if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(
        jsonString("user_defined_modifier") + ": " +
        rawExpressionRef(clause->get_user_defined_modifier(), ids));
  } else if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         (clause->get_user_defined_identifier()->get_spelling().empty() ||
          clause->get_user_defined_identifier()->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP in_reduction kind and structural user identifier "
          "disagree");
    }
  } else if (SgOmpTaskReductionClause *clause =
                 isSgOmpTaskReductionClause(node)) {
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
    const bool requires_user_identifier =
        clause->get_identifier() ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier;
    if (requires_user_identifier !=
            (clause->get_user_defined_identifier() != nullptr) ||
        (clause->get_user_defined_identifier() != nullptr &&
         (clause->get_user_defined_identifier()->get_spelling().empty() ||
          clause->get_user_defined_identifier()->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP task_reduction kind and structural user identifier "
          "disagree");
    }
  } else if (SgOmpDepobjUpdateClause *clause =
                 isSgOmpDepobjUpdateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpNumTasksClause *clause = isSgOmpNumTasksClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpGrainsizeClause *clause = isSgOmpGrainsizeClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpContextSelectorSet *set = isSgOmpContextSelectorSet(node)) {
    fields.push_back(rawIntegerField("set_kind", set->get_set_kind()));
  } else if (SgOmpContextSelectorProperty *property =
                 isSgOmpContextSelectorProperty(node)) {
    fields.push_back(
        rawIntegerField("context_kind", property->get_context_kind()));
    fields.push_back(
        rawIntegerField("context_vendor", property->get_context_vendor()));
    fields.push_back(rawIntegerField("atomic_default_mem_order",
                                     property->get_atomic_default_mem_order()));
    fields.push_back(
        rawIntegerField("requires_kind", property->get_requires_kind()));
    fields.push_back(
        rawIntegerField("requires_atomic_default_mem_order",
                        property->get_requires_atomic_default_mem_order()));
    fields.push_back(rawStringField(
        "requires_extension", property->get_requires_extension().getString()));
  } else if (SgOmpContextSelector *selector = isSgOmpContextSelector(node)) {
    fields.push_back(
        rawIntegerField("selector_kind", selector->get_selector_kind()));
    fields.push_back(rawStringField(
        "implementation_defined_name",
        selector->get_implementation_defined_name().getString()));
  } else if (SgOmpUsesAllocatorsDefination *definition =
                 isSgOmpUsesAllocatorsDefination(node)) {
    fields.push_back(rawIntegerField("allocator", definition->get_allocator()));
    fields.push_back(
        jsonString("user_defined_allocator") + ": " +
        rawExpressionRef(definition->get_user_defined_allocator(), ids));
    fields.push_back(
        jsonString("allocator_traits_array") + ": " +
        rawExpressionRef(definition->get_allocator_traits_array(), ids));
  } else if (SgOmpUsesAllocatorsClause *clause =
                 isSgOmpUsesAllocatorsClause(node)) {
    fields.push_back(jsonString("uses_allocators_definitions") + ": " +
                     rawOmpUsesAllocatorsDefinitionsJson(
                         clause->get_uses_allocators_defination(), ids));
  } else if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    fields.push_back(
        rawIntegerField("depend_modifier", clause->get_depend_modifier()));
    fields.push_back(
        rawIntegerField("dependence_type", clause->get_dependence_type()));
    const bool requires_iterator = clause->get_depend_modifier() ==
                                   SgOmpClause::e_omp_depend_modifier_iterator;
    if (clause->get_depend_modifier() !=
            SgOmpClause::e_omp_depend_modifier_unspecified &&
        !requires_iterator) {
      throw std::runtime_error(
          "AST JSON OpenMP depend clause has an invalid typed modifier");
    }
    switch (clause->get_dependence_type()) {
    case SgOmpClause::e_omp_depend_in:
    case SgOmpClause::e_omp_depend_out:
    case SgOmpClause::e_omp_depend_inout:
    case SgOmpClause::e_omp_depend_inoutset:
    case SgOmpClause::e_omp_depend_mutexinoutset:
    case SgOmpClause::e_omp_depend_depobj:
    case SgOmpClause::e_omp_depend_source:
    case SgOmpClause::e_omp_depend_sink:
      break;
    default:
      throw std::runtime_error(
          "AST JSON OpenMP depend clause has an invalid typed dependence "
          "kind");
    }
    validate_iterator_definitions(clause, clause->get_iterator_definitions(),
                                  requires_iterator);
    const bool requires_sink_vectors =
        clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
    if (requires_sink_vectors != (clause->get_sink_vectors() != nullptr) ||
        (requires_sink_vectors &&
         (clause->get_sink_vectors()->get_parent() != clause ||
          clause->get_sink_vectors()->get_expressions().empty()))) {
      throw std::runtime_error(
          "AST JSON OpenMP depend kind and structural sink vectors disagree");
    }
    if (requires_sink_vectors) {
      for (SgExpression *vector :
           clause->get_sink_vectors()->get_expressions()) {
        if (vector == nullptr ||
            vector->get_parent() != clause->get_sink_vectors() ||
            std::count(clause->get_sink_vectors()->get_expressions().begin(),
                       clause->get_sink_vectors()->get_expressions().end(),
                       vector) != 1) {
          throw std::runtime_error(
              "AST JSON OpenMP sink vector has invalid structural "
              "ownership");
        }
      }
    }
  } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    fields.push_back(
        rawIntegerField("affinity_modifier", clause->get_affinity_modifier()));
    if (clause->get_affinity_modifier() !=
            SgOmpClause::e_omp_affinity_modifier_unspecified &&
        clause->get_affinity_modifier() !=
            SgOmpClause::e_omp_affinity_modifier_iterator) {
      throw std::runtime_error(
          "AST JSON OpenMP affinity clause has an invalid typed modifier");
    }
    validate_iterator_definitions(
        clause, clause->get_iterator_definitions(),
        clause->get_affinity_modifier() ==
            SgOmpClause::e_omp_affinity_modifier_iterator);
  } else if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    if (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_mapper &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_iterator &&
        clause->get_kind() != SgOmpClause::e_omp_to_kind_present) {
      throw std::runtime_error(
          "AST JSON OpenMP to clause has an invalid typed kind");
    }
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    fields.push_back(rawBoolField("declare_target_extended_list",
                                  clause->get_declare_target_extended_list()));
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if (requires_mapper != (mapper_identifier != nullptr) ||
        (mapper_identifier != nullptr &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP to kind and structural mapper identifier "
          "disagree");
    }
    validate_iterator_definitions(clause, clause->get_iterator_definitions(),
                                  clause->get_kind() ==
                                      SgOmpClause::e_omp_to_kind_iterator);
    if (clause->get_declare_target_extended_list() &&
        (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
         mapper_identifier != nullptr ||
         !clause->get_iterator_definitions().empty())) {
      throw std::runtime_error(
          "AST JSON OpenMP declare-target extended list has incompatible "
          "to-clause state");
    }
  } else if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    if (clause->get_kind() != SgOmpClause::e_omp_from_kind_unknown &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_mapper &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_iterator &&
        clause->get_kind() != SgOmpClause::e_omp_from_kind_present) {
      throw std::runtime_error(
          "AST JSON OpenMP from clause has an invalid typed kind");
    }
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    const bool requires_mapper =
        clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
    SgOmpNameExpression *mapper_identifier = clause->get_mapper_identifier();
    if (requires_mapper != (mapper_identifier != nullptr) ||
        (mapper_identifier != nullptr &&
         (mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != clause))) {
      throw std::runtime_error(
          "AST JSON OpenMP from kind and structural mapper identifier "
          "disagree");
    }
    validate_iterator_definitions(clause, clause->get_iterator_definitions(),
                                  clause->get_kind() ==
                                      SgOmpClause::e_omp_from_kind_iterator);
  }

  auto validate_acc_expression_list = [](SgNode *owner, SgExprListExp *list,
                                         const std::string &context) {
    if (list == nullptr || list->get_parent() != owner ||
        list->get_expressions().empty()) {
      throw std::runtime_error("AST JSON " + context +
                               " requires one non-empty exactly owned "
                               "expression list");
    }
    for (SgExpression *expression : list->get_expressions()) {
      if (expression == nullptr || expression->get_parent() != list) {
        throw std::runtime_error("AST JSON " + context +
                                 " has a null or foreign list expression");
      }
    }
  };
  auto validate_acc_clauses = [](SgStatement *owner,
                                 const SgAccClausePtrList &clauses) {
    std::set<SgAccClause *> unique;
    for (SgAccClause *clause : clauses) {
      if (clause == nullptr || clause->get_parent() != owner ||
          !unique.insert(clause).second) {
        throw std::runtime_error(
            "AST JSON OpenACC statement has a null, foreign, or duplicate "
            "clause");
      }
    }
  };
  if (SgAccExpressionClause *clause = isSgAccExpressionClause(node)) {
    SgExpression *expression = clause->get_expression();
    const bool expression_is_optional = isSgAccAsyncClause(clause) != nullptr ||
                                        isSgAccVectorClause(clause) != nullptr;
    if ((expression == nullptr && !expression_is_optional) ||
        (expression != nullptr && expression->get_parent() != clause)) {
      throw std::runtime_error(
          "AST JSON OpenACC expression clause has missing or foreign syntax");
    }
  }
  if (SgAccVariablesClause *clause = isSgAccVariablesClause(node)) {
    validate_acc_expression_list(clause, clause->get_variables(),
                                 clause->sage_class_name());
  }
  if (SgAccDefaultClause *clause = isSgAccDefaultClause(node)) {
    const int default_kind = clause->get_default_kind();
    if (default_kind != 0 && default_kind != 1) {
      throw std::runtime_error(
          "AST JSON SgAccDefaultClause has an invalid default kind");
    }
    fields.push_back(rawIntegerField("default_kind", default_kind));
  }
  if (SgAccReductionClause *clause = isSgAccReductionClause(node)) {
    const int reduction_operator = clause->get_reduction_operator();
    if (reduction_operator < 0 || reduction_operator > 16) {
      throw std::runtime_error(
          "AST JSON SgAccReductionClause has an invalid reduction operator");
    }
    fields.push_back(rawIntegerField("reduction_operator", reduction_operator));
  }
  if (SgAccBodyStatement *stmt = isSgAccBodyStatement(node)) {
    if (stmt->get_body() == nullptr || stmt->get_body()->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON OpenACC body statement has no exactly owned body");
    }
  }
  if (SgAccClauseBodyStatement *stmt = isSgAccClauseBodyStatement(node)) {
    validate_acc_clauses(stmt, stmt->get_clauses());
  } else if (SgAccClauseStatement *stmt = isSgAccClauseStatement(node)) {
    validate_acc_clauses(stmt, stmt->get_clauses());
  }
  if (SgAccRoutineStatement *stmt = isSgAccRoutineStatement(node)) {
    fields.push_back(
        rawStringField("routine_name", stmt->get_routine_name().getString()));
  }
  if (SgAccWaitStatement *stmt = isSgAccWaitStatement(node)) {
    SgExprListExp *wait_list = stmt->get_wait_list();
    SgExpression *device_number = stmt->get_devnum();
    if (wait_list != nullptr) {
      validate_acc_expression_list(stmt, wait_list, "SgAccWaitStatement");
    }
    if (device_number != nullptr && device_number->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON SgAccWaitStatement has a foreign device-number "
          "expression");
    }
    if (stmt->get_queues() && wait_list == nullptr) {
      throw std::runtime_error(
          "AST JSON SgAccWaitStatement queues syntax has no wait list");
    }
    fields.push_back(rawBoolField("queues", stmt->get_queues()));
  }
  if (SgAccCacheStatement *stmt = isSgAccCacheStatement(node)) {
    validate_acc_expression_list(stmt, stmt->get_variables(),
                                 "SgAccCacheStatement");
    const int modifier = stmt->get_modifier();
    if (modifier != 0 && modifier != 1) {
      throw std::runtime_error(
          "AST JSON SgAccCacheStatement has an invalid modifier");
    }
    fields.push_back(rawIntegerField("modifier", modifier));
  }

  if (SgOmpAtClause *clause = isSgOmpAtClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  }
  if (SgOmpSeverityClause *clause = isSgOmpSeverityClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  }
  if (SgOmpDoacrossClause *clause = isSgOmpDoacrossClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  }
  if (SgOmpApplyClause *clause = isSgOmpApplyClause(node)) {
    fields.push_back(rawStringField("label", clause->get_label()));
  }

  if (SgOmpCriticalStatement *stmt = isSgOmpCriticalStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
  }
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    if (stmt->get_semantic_variant_ordinal() >
        static_cast<std::size_t>(std::numeric_limits<int64_t>::max())) {
      throw std::runtime_error(
          "AST JSON declare simd producer ordinal is out of range");
    }
    fields.push_back(rawIntegerField(
        "semantic_variant_ordinal",
        static_cast<int64_t>(stmt->get_semantic_variant_ordinal())));
    fields.push_back(rawBoolField("function_ref_is_explicit",
                                  stmt->get_function_ref_is_explicit()));
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    if (stmt->get_semantic_variant_ordinal() >
        static_cast<std::size_t>(std::numeric_limits<int64_t>::max())) {
      throw std::runtime_error(
          "AST JSON declare variant producer ordinal is out of range");
    }
    fields.push_back(rawIntegerField(
        "semantic_variant_ordinal",
        static_cast<int64_t>(stmt->get_semantic_variant_ordinal())));
    fields.push_back(rawBoolField("base_function_ref_is_explicit",
                                  stmt->get_base_function_ref_is_explicit()));
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    fields.push_back(rawIntegerField("identifier", stmt->get_identifier()));
    fields.push_back(rawBoolField("identifier_is_explicit",
                                  stmt->get_identifier_is_explicit()));
  }
  if (SgOmpDeclareTargetStatement *stmt = isSgOmpDeclareTargetStatement(node)) {
    fields.push_back(
        rawIntegerField("device_type_kind", stmt->get_device_type_kind()));
    fields.push_back(rawBoolField("use_underscore_spelling",
                                  stmt->get_use_underscore_spelling()));
  }
  if (SgOmpBeginDeclareTargetStatement *stmt =
          isSgOmpBeginDeclareTargetStatement(node)) {
    fields.push_back(rawBoolField("use_underscore_spelling",
                                  stmt->get_use_underscore_spelling()));
  }
  if (SgOmpEndDeclareTargetStatement *stmt =
          isSgOmpEndDeclareTargetStatement(node)) {
    fields.push_back(rawBoolField("use_underscore_spelling",
                                  stmt->get_use_underscore_spelling()));
  }
  if (SgOmpGroupprivateStatement *stmt = isSgOmpGroupprivateStatement(node)) {
    fields.push_back(
        rawIntegerField("device_type_kind", stmt->get_device_type_kind()));
  }
  if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
    fields.push_back(rawBoolField("implicit_none", stmt->get_implicit_none()));
    fields.push_back(
        rawIntegerField("implicit_spec", stmt->get_implicit_spec()));
  }
  if (SgFortranIncludeLine *stmt = isSgFortranIncludeLine(node)) {
    fields.push_back(rawStringField("filename", stmt->get_filename()));
  }
  if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
    fields.push_back(rawStringField("label", stmt->get_label().getString()));
    fields.push_back(
        rawBoolField("gnu_extension_unused", stmt->get_gnu_extension_unused()));
  }
  if (SgBasicBlock *stmt = isSgBasicBlock(node)) {
    fields.push_back(rawBoolField("is_fortran_block_construct",
                                  stmt->get_is_fortran_block_construct()));
    fields.push_back(rawStringField("fortran_block_construct_name",
                                    stmt->get_fortran_block_construct_name()));
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
    fields.push_back(
        rawBoolField("use_then_keyword", stmt->get_use_then_keyword()));
    fields.push_back(
        rawBoolField("is_else_if_statement", stmt->get_is_else_if_statement()));
  }
  if (SgTryStmt *stmt = isSgTryStmt(node)) {
    fields.push_back(rawBoolField("is_function_try_block",
                                  stmt->get_is_function_try_block()));
  }
  if (SgLambdaExp *lambda = isSgLambdaExp(node)) {
    if (lambda->get_lambda_capture_list() == nullptr ||
        lambda->get_lambda_closure_class() == nullptr ||
        lambda->get_lambda_function() == nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize incomplete SgLambdaExp");
    }
    fields.push_back(rawBoolField("is_mutable", lambda->get_is_mutable()));
    fields.push_back(
        rawBoolField("capture_default", lambda->get_capture_default()));
    fields.push_back(rawBoolField("default_is_by_reference",
                                  lambda->get_default_is_by_reference()));
    fields.push_back(rawBoolField("explicit_return_type",
                                  lambda->get_explicit_return_type()));
    fields.push_back(
        rawBoolField("has_parameter_decl", lambda->get_has_parameter_decl()));
    fields.push_back(rawBoolField("is_device", lambda->get_is_device()));
  }
  if (SgLambdaCapture *capture = isSgLambdaCapture(node)) {
    if (capture->get_capture_variable() == nullptr) {
      throw std::runtime_error(
          "AST JSON cannot serialize SgLambdaCapture without a variable");
    }
    fields.push_back(rawBoolField("capture_by_reference",
                                  capture->get_capture_by_reference()));
    fields.push_back(rawBoolField("implicit", capture->get_implicit()));
    fields.push_back(
        rawBoolField("pack_expansion", capture->get_pack_expansion()));
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
  }
  if (SgFortranDo *stmt = isSgFortranDo(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(rawBoolField("old_style", stmt->get_old_style()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
  }
  if (SgIOStatement *stmt = isSgIOStatement(node)) {
    switch (stmt->get_io_statement()) {
    case SgIOStatement::e_read:
    case SgIOStatement::e_print:
    case SgIOStatement::e_write:
    case SgIOStatement::e_open:
    case SgIOStatement::e_close:
    case SgIOStatement::e_inquire:
    case SgIOStatement::e_backspace:
    case SgIOStatement::e_endfile:
    case SgIOStatement::e_rewind:
    case SgIOStatement::e_flush:
    case SgIOStatement::e_wait:
      break;
    case SgIOStatement::e_unknown:
    case SgIOStatement::e_last_io_statment_kind:
    default:
      throw std::runtime_error(
          "AST JSON SgIOStatement has no exact concrete statement kind");
    }
    fields.push_back(rawIntegerField("io_statement", stmt->get_io_statement()));
  }
  if (SgNewExp *expr = isSgNewExp(node)) {
    fields.push_back(jsonString("specified_type") + ": " +
                     rawTypeJson(expr->get_specified_type(), ids));
    fields.push_back(rawIntegerField(
        "need_global_specifier",
        static_cast<int64_t>(expr->get_need_global_specifier())));
    fields.push_back(rawBoolField("type_id_is_parenthesized",
                                  expr->get_type_id_is_parenthesized()));
  }
  if (SgDeleteExp *expr = isSgDeleteExp(node)) {
    fields.push_back(rawIntegerField(
        "is_array", static_cast<int64_t>(expr->get_is_array())));
    fields.push_back(rawIntegerField(
        "need_global_specifier",
        static_cast<int64_t>(expr->get_need_global_specifier())));
  }

  if (SgStatement *statement = isSgStatement(node)) {
    fields.push_back(rawIntegerField("source_sequence_value",
                                     statement->get_source_sequence_value()));
    switch (statement->get_directive_end_kind()) {
    case SgStatement::e_directive_end_not_applicable:
    case SgStatement::e_directive_end_implicit:
    case SgStatement::e_directive_end_explicit:
      break;
    default:
      throw std::runtime_error(
          "AST JSON SgStatement has an invalid directive-end kind");
    }
    fields.push_back(rawIntegerField("directive_end_kind",
                                     statement->get_directive_end_kind()));
    switch (statement->get_omp_fortran_spelling()) {
    case SgStatement::e_omp_fortran_spelling_not_applicable:
    case SgStatement::e_omp_fortran_spelling_do:
      break;
    default:
      throw std::runtime_error(
          "AST JSON SgStatement has an invalid OpenMP Fortran spelling");
    }
    fields.push_back(rawIntegerField("omp_fortran_spelling",
                                     statement->get_omp_fortran_spelling()));
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    auto append_declaration_qualification = [&](auto *qualified_decl) {
      fields.push_back(
          rawIntegerField("name_qualification_length",
                          qualified_decl->get_name_qualification_length()));
      fields.push_back(
          rawBoolField("type_elaboration_required",
                       qualified_decl->get_type_elaboration_required()));
      fields.push_back(
          rawBoolField("global_qualification_required",
                       qualified_decl->get_global_qualification_required()));
    };
    if (isSgFunctionDeclaration(decl) == nullptr) {
      if (SgVariableDeclaration *qualified = isSgVariableDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgEnumDeclaration *qualified = isSgEnumDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgTypedefDeclaration *qualified =
                     isSgTypedefDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgUsingDirectiveStatement *qualified =
                     isSgUsingDirectiveStatement(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgUsingDeclarationStatement *qualified =
                     isSgUsingDeclarationStatement(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgClassDeclaration *qualified = isSgClassDeclaration(decl)) {
        append_declaration_qualification(qualified);
      }
    }
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
      fields.push_back(rawIntegerField(
          "fortran_declaration_origin",
          static_cast<int>(variable->get_fortran_declaration_origin())));
      fields.push_back(
          rawBoolField("requires_global_name_qualification_on_type",
                       variable->get_requiresGlobalNameQualificationOnType()));
      fields.push_back(
          jsonString("source_spelled_template_owner_type") + ": " +
          rawTypeJson(variable->get_sourceSpelledTemplateOwnerType(), ids));
    }
  }

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    addLocatedPreprocessing(fields, located);
  } else {
    fields.push_back(jsonString("preprocessing") + ": []");
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

} // namespace AstJson
} // namespace Rose
