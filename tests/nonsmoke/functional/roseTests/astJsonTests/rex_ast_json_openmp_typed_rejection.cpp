#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

Rose::AstJson::NodeRecord &uniqueRecord(Rose::AstJson::AstFileRecord &ast,
                                        const std::string &kind) {
  Rose::AstJson::NodeRecord *result = nullptr;
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.kind != kind) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error("AST JSON rejection fixture has duplicate " +
                               kind + " records");
    }
    result = &record;
  }
  if (result == nullptr) {
    throw std::runtime_error("AST JSON rejection fixture has no " + kind +
                             " record");
  }
  return *result;
}

Rose::AstJson::NodeRecord &firstRecord(Rose::AstJson::AstFileRecord &ast,
                                       const std::string &kind) {
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.kind == kind) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON rejection fixture has no " + kind +
                           " record");
}

Rose::AstJson::NodeRecord &
firstRecordWithIntProperty(Rose::AstJson::AstFileRecord &ast,
                           const std::string &kind, const std::string &property,
                           int64_t value) {
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.kind == kind &&
        record.properties.at(property).asInt() == value) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON rejection fixture has no " + kind +
                           " record with " + property + "=" +
                           std::to_string(value));
}

Rose::AstJson::NodeRecord &recordById(Rose::AstJson::AstFileRecord &ast,
                                      uint64_t id) {
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.id == id) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON rejection fixture has no record id " +
                           std::to_string(id));
}

Rose::AstJson::NodeRecord &
selectorSet(Rose::AstJson::AstFileRecord &ast,
            SgOmpClause::omp_context_selector_set_kind_enum setKind) {
  return firstRecordWithIntProperty(ast, "SgOmpContextSelectorSet", "set_kind",
                                    setKind);
}

Rose::AstJson::NodeRecord &
selectorSetWithCount(Rose::AstJson::AstFileRecord &ast,
                     SgOmpClause::omp_context_selector_set_kind_enum setKind,
                     std::size_t selectorCount) {
  Rose::AstJson::NodeRecord *result = nullptr;
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.kind != "SgOmpContextSelectorSet" ||
        record.properties.requiredInt("set_kind") != setKind ||
        Rose::AstJson::edgesFor(record, "selectors").size() != selectorCount) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error(
          "AST JSON rejection fixture has multiple matching selector sets");
    }
    result = &record;
  }
  if (result == nullptr) {
    throw std::runtime_error(
        "AST JSON rejection fixture has no selector set with the required "
        "cardinality");
  }
  return *result;
}

Rose::AstJson::NodeRecord &
selectorInSet(Rose::AstJson::AstFileRecord &ast, Rose::AstJson::NodeRecord &set,
              SgOmpClause::omp_context_trait_selector_kind_enum selectorKind,
              size_t ordinal = 0) {
  size_t found = 0;
  for (const Rose::AstJson::EdgeRecord &edge :
       Rose::AstJson::edgesFor(set, "selectors")) {
    Rose::AstJson::NodeRecord &selector = recordById(ast, edge.target);
    if (selector.properties.requiredInt("selector_kind") != selectorKind) {
      continue;
    }
    if (found++ == ordinal) {
      return selector;
    }
  }
  throw std::runtime_error(
      "AST JSON rejection fixture has no requested selector in set");
}

Rose::AstJson::NodeRecord &propertyAt(Rose::AstJson::AstFileRecord &ast,
                                      Rose::AstJson::NodeRecord &selector,
                                      size_t index) {
  const std::vector<Rose::AstJson::EdgeRecord> properties =
      Rose::AstJson::edgesFor(selector, "properties");
  if (index >= properties.size()) {
    throw std::runtime_error(
        "AST JSON rejection fixture has no requested selector property");
  }
  return recordById(ast, properties[index].target);
}

Rose::AstJson::NodeRecord &
propertyExpression(Rose::AstJson::AstFileRecord &ast,
                   Rose::AstJson::NodeRecord &property) {
  const std::vector<Rose::AstJson::EdgeRecord> expressions =
      Rose::AstJson::edgesFor(property, "expression");
  if (expressions.size() != 1) {
    throw std::runtime_error(
        "AST JSON rejection fixture property has no single expression");
  }
  return recordById(ast, expressions.front().target);
}

Rose::AstJson::NodeRecord &selectorScore(Rose::AstJson::AstFileRecord &ast,
                                         Rose::AstJson::NodeRecord &selector) {
  const std::vector<Rose::AstJson::EdgeRecord> scores =
      Rose::AstJson::edgesFor(selector, "score");
  if (scores.size() != 1) {
    throw std::runtime_error(
        "AST JSON rejection fixture selector has no single score");
  }
  return recordById(ast, scores.front().target);
}

void replaceWithOmpNameExpression(Rose::AstJson::NodeRecord &record,
                                  const std::string &spelling,
                                  bool preserveSerializedType = false) {
  record.kind = "SgOmpNameExpression";
  record.properties.object["spelling"] =
      Rose::AstJson::JsonValue::string(spelling);
  if (!preserveSerializedType) {
    record.properties.object.erase("type");
  }
}

Rose::AstJson::NodeRecord &
firstRecordWithSingleEdge(Rose::AstJson::AstFileRecord &ast,
                          const std::string &field) {
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (Rose::AstJson::edgesFor(record, field).size() == 1) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON rejection fixture has no record with "
                           "one '" +
                           field + "' edge");
}

Rose::AstJson::NodeRecord &
firstRecordWithNonemptyWrapper(Rose::AstJson::AstFileRecord &ast,
                               const std::string &wrapperField,
                               const std::string &elementField) {
  for (Rose::AstJson::NodeRecord &record : ast.nodes) {
    const std::vector<Rose::AstJson::EdgeRecord> wrapperEdges =
        Rose::AstJson::edgesFor(record, wrapperField);
    if (wrapperEdges.size() != 1) {
      continue;
    }
    Rose::AstJson::NodeRecord &wrapper =
        recordById(ast, wrapperEdges.front().target);
    if (!Rose::AstJson::edgesFor(wrapper, elementField).empty()) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON rejection fixture has no nonempty '" +
                           wrapperField + "' wrapper");
}

void removeEdges(Rose::AstJson::NodeRecord &record, const std::string &field) {
  record.edges.erase(std::remove_if(record.edges.begin(), record.edges.end(),
                                    [&](const Rose::AstJson::EdgeRecord &edge) {
                                      return edge.field == field;
                                    }),
                     record.edges.end());
}

std::string oversizedSizeTJsonInteger() {
  if constexpr (std::numeric_limits<std::size_t>::max() >=
                static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(
        "AST JSON rejection fixture has no int64 value above size_t");
  } else {
    return std::to_string(
        static_cast<uint64_t>(std::numeric_limits<std::size_t>::max()) + 1);
  }
}

SgSourceFile *sourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    throw std::runtime_error(
        "AST JSON rejection fixture requires exactly one source file");
  }
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON rejection fixture input is not a source file");
  }
  return file;
}

} // namespace

int main(int argc, char **argv) {
  std::string mode;
  std::vector<char *> frontendArguments;
  frontendArguments.push_back(argv[0]);
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument.rfind("--malformed-json=", 0) == 0) {
      if (!mode.empty()) {
        throw std::runtime_error(
            "AST JSON rejection fixture received multiple mutation modes");
      }
      mode = argument.substr(std::string("--malformed-json=").size());
    } else {
      frontendArguments.push_back(argv[index]);
    }
  }
  if (mode.empty()) {
    throw std::runtime_error(
        "AST JSON rejection fixture requires a mutation mode");
  }

  SgProject *project = frontend(static_cast<int>(frontendArguments.size()),
                                frontendArguments.data());
  project->skipfinalCompileStep(true);
  SgSourceFile *file = sourceFile(project);
  const std::string json = Rose::AstJson::buildJson(
      file, Rose::AstJson::Checkpoint::PostOmpConstruction, file);
  Rose::AstJson::AstFileRecord ast = Rose::AstJson::parseAstFileJson(
      json, Rose::AstJson::checkpointName(
                Rose::AstJson::Checkpoint::PostOmpConstruction));

  std::string expectedMessage;
  if (mode == "invalid-enum") {
    Rose::AstJson::NodeRecord &item = firstRecord(ast, "SgOmpInductionItem");
    item.properties.object["kind"] = Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpInductionItem has an invalid kind";
  } else if (mode == "invalid-at-enum") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpAtClause");
    clause.properties.object["kind"] = Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpAtClause has an invalid kind";
  } else if (mode == "invalid-severity-enum") {
    Rose::AstJson::NodeRecord &clause =
        uniqueRecord(ast, "SgOmpSeverityClause");
    clause.properties.object["kind"] = Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpSeverityClause has an invalid kind";
  } else if (mode == "invalid-doacross-enum") {
    Rose::AstJson::NodeRecord &clause =
        uniqueRecord(ast, "SgOmpDoacrossClause");
    clause.properties.object["kind"] = Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpDoacrossClause has an invalid kind";
  } else if (mode == "invalid-declare-target-device-type") {
    Rose::AstJson::NodeRecord &statement =
        uniqueRecord(ast, "SgOmpDeclareTargetStatement");
    statement.properties.object["device_type_kind"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage =
        "SgOmpDeclareTargetStatement has an invalid device_type kind";
  } else if (mode == "invalid-groupprivate-device-type") {
    Rose::AstJson::NodeRecord &statement =
        uniqueRecord(ast, "SgOmpGroupprivateStatement");
    statement.properties.object["device_type_kind"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage =
        "SgOmpGroupprivateStatement has an invalid device_type kind";
  } else if (mode == "duplicate-singleton") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpInitClause");
    const std::vector<Rose::AstJson::EdgeRecord> operandEdges =
        Rose::AstJson::edgesFor(clause, "operand");
    if (operandEdges.size() != 1) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires one init operand edge");
    }
    clause.edges.push_back(operandEdges.front());
    expectedMessage = "duplicate singleton edge 'operand'";
  } else if (mode == "missing-induction-expression") {
    Rose::AstJson::NodeRecord &item = firstRecord(ast, "SgOmpInductionItem");
    removeEdges(item, "expression");
    expectedMessage = "missing required edge 'expression'";
  } else if (mode == "missing-init-operand") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpInitClause");
    removeEdges(clause, "operand");
    expectedMessage = "missing required edge 'operand'";
  } else if (mode == "missing-init-modifier-list") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpInitClause");
    removeEdges(clause, "modifier_list");
    expectedMessage = "missing required edge 'modifier_list'";
  } else if (mode == "syntax-init-operand") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpInitClause");
    Rose::AstJson::NodeRecord &operand = recordById(
        ast, Rose::AstJson::edgesFor(clause, "operand").front().target);
    replaceWithOmpNameExpression(operand, "rex_syntax_operand");
    expectedMessage =
        "reconstructed malformed SgOmpInitClause: operand has no semantic "
        "value-expression role";
  } else if (mode == "invalid-init-modifier-enum") {
    firstRecord(ast, "SgOmpInitModifier").properties.object["kind"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpInitModifier has an invalid kind";
  } else if (mode == "invalid-adjust-enum") {
    uniqueRecord(ast, "SgOmpAdjustArgsClause").properties.object["modifier"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpAdjustArgsClause has an invalid modifier";
  } else if (mode == "missing-adjust-arguments") {
    removeEdges(uniqueRecord(ast, "SgOmpAdjustArgsClause"), "arguments");
    expectedMessage =
        "reconstructed malformed SgOmpAdjustArgsClause: adjust_args argument "
        "list is missing, empty, or misowned";
  } else if (mode == "syntax-adjust-argument") {
    Rose::AstJson::NodeRecord &clause =
        uniqueRecord(ast, "SgOmpAdjustArgsClause");
    Rose::AstJson::NodeRecord &arguments = recordById(
        ast, Rose::AstJson::edgesFor(clause, "arguments").front().target);
    Rose::AstJson::NodeRecord &argument = recordById(
        ast, Rose::AstJson::edgesFor(arguments, "expressions").front().target);
    replaceWithOmpNameExpression(argument, "rex_syntax_argument");
    expectedMessage =
        "reconstructed malformed SgOmpAdjustArgsClause: adjust_args argument "
        "has no semantic value-expression role";
  } else if (mode == "missing-append-operations") {
    removeEdges(uniqueRecord(ast, "SgOmpAppendArgsClause"),
                "interop_operations");
    expectedMessage = "SgOmpAppendArgsClause has no interop operations";
  } else if (mode == "missing-append-modifier-list") {
    removeEdges(firstRecord(ast, "SgOmpAppendArgsOperation"), "modifier_list");
    expectedMessage = "missing required edge 'modifier_list'";
  } else if (mode == "append-missing-interop-type") {
    Rose::AstJson::NodeRecord &operation =
        firstRecord(ast, "SgOmpAppendArgsOperation");
    Rose::AstJson::NodeRecord &modifierList = recordById(
        ast,
        Rose::AstJson::edgesFor(operation, "modifier_list").front().target);
    removeEdges(modifierList, "modifiers");
    expectedMessage =
        "reconstructed malformed SgOmpAppendArgsClause: interop type is "
        "missing";
  } else if (mode == "append-invalid-depinfo") {
    Rose::AstJson::NodeRecord &preferType = firstRecordWithIntProperty(
        ast, "SgOmpInitModifier", "kind",
        SgOmpClause::e_omp_init_modifier_prefer_type);
    preferType.properties.object["kind"] = Rose::AstJson::JsonValue::number(
        std::to_string(SgOmpClause::e_omp_init_modifier_depinfo_in));
    expectedMessage =
        "reconstructed malformed SgOmpAppendArgsClause: depinfo modifier has "
        "no semantic locator payload";
  } else if (mode == "missing-clause-list") {
    Rose::AstJson::NodeRecord &statement =
        firstRecordWithSingleEdge(ast, "clause_list");
    removeEdges(statement, "clause_list");
    expectedMessage = "missing required edge 'clause_list'";
  } else if (mode == "duplicate-clause-list") {
    Rose::AstJson::NodeRecord &statement =
        firstRecordWithSingleEdge(ast, "clause_list");
    const Rose::AstJson::EdgeRecord clauseListEdge =
        Rose::AstJson::edgesFor(statement, "clause_list").front();
    statement.edges.push_back(clauseListEdge);
    expectedMessage = "duplicate singleton edge 'clause_list'";
  } else if (mode == "missing-variable-list") {
    Rose::AstJson::NodeRecord &statement =
        uniqueRecord(ast, "SgOmpFlushStatement");
    removeEdges(statement, "variable_list");
    expectedMessage = "missing required edge 'variable_list'";
  } else if (mode == "flattened-clauses") {
    Rose::AstJson::NodeRecord &statement =
        firstRecordWithNonemptyWrapper(ast, "clause_list", "clauses");
    Rose::AstJson::NodeRecord &wrapper = recordById(
        ast, Rose::AstJson::edgesFor(statement, "clause_list").front().target);
    for (Rose::AstJson::EdgeRecord edge :
         Rose::AstJson::edgesFor(wrapper, "clauses")) {
      edge.field = "clauses";
      statement.edges.push_back(edge);
    }
    expectedMessage = "flattened 'clauses' edges";
  } else if (mode == "flattened-variables") {
    Rose::AstJson::NodeRecord &statement =
        uniqueRecord(ast, "SgOmpFlushStatement");
    Rose::AstJson::NodeRecord &wrapper = recordById(
        ast,
        Rose::AstJson::edgesFor(statement, "variable_list").front().target);
    for (Rose::AstJson::EdgeRecord edge :
         Rose::AstJson::edgesFor(wrapper, "expressions")) {
      edge.field = "variables";
      statement.edges.push_back(edge);
    }
    expectedMessage = "flattened 'variables' edges";
  } else if (mode == "invalid-context-set-enum") {
    Rose::AstJson::NodeRecord &set =
        firstRecord(ast, "SgOmpContextSelectorSet");
    set.properties.object["set_kind"] = Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpContextSelectorSet has an invalid set kind";
  } else if (mode == "duplicate-context-set") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpWhenClause");
    const std::vector<Rose::AstJson::EdgeRecord> setEdges =
        Rose::AstJson::edgesFor(clause, "context_selector_sets");
    if (setEdges.size() != 4) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires four context selector sets");
    }
    clause.edges.push_back(setEdges.front());
    expectedMessage = "variant clause has a duplicate selector set";
  } else if (mode == "duplicate-context-trait") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    const std::vector<Rose::AstJson::EdgeRecord> selectorEdges =
        Rose::AstJson::edgesFor(set, "selectors");
    if (selectorEdges.size() != 5) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires five target_device selectors");
    }
    set.edges.push_back(selectorEdges.front());
    expectedMessage = "context selector set has a duplicate selector";
  } else if (mode == "duplicate-context-property") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_arch);
    const std::vector<Rose::AstJson::EdgeRecord> properties =
        Rose::AstJson::edgesFor(selector, "properties");
    if (properties.size() != 2) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires two arch properties");
    }
    selector.edges.push_back(properties.front());
    expectedMessage = "context selector has a duplicate property";
  } else if (mode == "duplicate-context-kind-property") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_kind);
    Rose::AstJson::NodeRecord &second = propertyAt(ast, selector, 1);
    second.properties.object["context_kind"] = Rose::AstJson::JsonValue::number(
        std::to_string(SgOmpClause::e_omp_when_context_kind_cpu));
    expectedMessage = "context selector has a duplicate property";
  } else if (mode == "duplicate-equivalent-name-property") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_extension);
    Rose::AstJson::NodeRecord &quotedName =
        propertyExpression(ast, propertyAt(ast, selector, 1));
    if (quotedName.kind != "SgOmpSourceExpression") {
      throw std::runtime_error(
          "AST JSON rejection fixture requires an exact quoted syntax "
          "property");
    }
    quotedName.properties.object["spelling"] =
        Rose::AstJson::JsonValue::string("\"rex_ext_a\"");
    expectedMessage = "context selector has a duplicate property";
  } else if (mode == "duplicate-context-construct") {
    Rose::AstJson::NodeRecord &set = selectorSetWithCount(
        ast, SgOmpClause::e_omp_context_selector_set_construct, 2);
    const std::vector<Rose::AstJson::EdgeRecord> selectorEdges =
        Rose::AstJson::edgesFor(set, "selectors");
    if (selectorEdges.size() != 2) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires two distinct construct "
          "selectors");
    }
    set.edges.push_back(selectorEdges.front());
    expectedMessage = "context selector set has a duplicate selector";
  } else if (mode == "duplicate-context-custom") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    std::vector<Rose::AstJson::EdgeRecord> customSelectorEdges;
    for (const Rose::AstJson::EdgeRecord &edge :
         Rose::AstJson::edgesFor(set, "selectors")) {
      Rose::AstJson::NodeRecord &selector = recordById(ast, edge.target);
      if (selector.properties.requiredInt("selector_kind") ==
          SgOmpClause::e_omp_context_trait_implementation_user) {
        customSelectorEdges.push_back(edge);
      }
    }
    if (customSelectorEdges.size() != 2) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires two distinct custom "
          "selectors");
    }
    set.edges.push_back(customSelectorEdges.front());
    expectedMessage = "context selector set has a duplicate selector";
  } else if (mode == "illegal-context-trait") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_device);
    set.properties.object["set_kind"] = Rose::AstJson::JsonValue::number(
        std::to_string(SgOmpClause::e_omp_context_selector_set_user));
    expectedMessage = "trait selector is illegal in its selector set";
  } else if (mode == "missing-context-expression") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_arch);
    removeEdges(propertyAt(ast, selector, 0), "expression");
    expectedMessage =
        "context selector property does not own exactly one typed payload";
  } else if (mode == "missing-context-properties") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_arch);
    removeEdges(selector, "properties");
    expectedMessage = "context selector has invalid property cardinality";
  } else if (mode == "wrong-context-property-payload") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_arch);
    Rose::AstJson::NodeRecord &property = propertyAt(ast, selector, 0);
    property.properties.object["context_kind"] =
        Rose::AstJson::JsonValue::number(
            std::to_string(SgOmpClause::e_omp_when_context_kind_cpu));
    expectedMessage =
        "context selector property does not own exactly one typed payload";
  } else if (mode == "syntax-expression-with-type") {
    replaceWithOmpNameExpression(firstRecord(ast, "SgIntVal"),
                                 "syntax_with_semantic_type", true);
    expectedMessage = "syntax expression has a serialized semantic type";
  } else if (mode == "semantic-expression-missing-type") {
    firstRecord(ast, "SgIntVal").properties.object.erase("type");
    expectedMessage = "semantic expression is missing its semantic type";
  } else if (mode == "syntax-context-semantic-property") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_device_num);
    Rose::AstJson::NodeRecord &expression =
        propertyExpression(ast, propertyAt(ast, selector, 0));
    replaceWithOmpNameExpression(expression, "not_a_semantic_expression");
    expectedMessage = "semantic context property is not a semantic expression";
  } else if (mode == "syntax-context-score") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_vendor);
    replaceWithOmpNameExpression(selectorScore(ast, selector),
                                 "not_a_semantic_score");
    expectedMessage = "context selector score is not a semantic expression";
  } else if (mode == "noninteger-context-score") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_vendor);
    Rose::AstJson::NodeRecord &score = selectorScore(ast, selector);
    score.kind = "SgFloatVal";
    score.properties.object["value"] = Rose::AstJson::JsonValue::string("7.5");
    expectedMessage =
        "context selector score is not an integer or enum expression";
  } else if (mode == "negative-context-score") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_vendor);
    Rose::AstJson::NodeRecord &score = selectorScore(ast, selector);
    if (score.kind != "SgIntVal") {
      throw std::runtime_error(
          "AST JSON rejection fixture score is not an exact integer value");
    }
    score.properties.object["value"] = Rose::AstJson::JsonValue::number("-1");
    score.properties.object["value_string"] =
        Rose::AstJson::JsonValue::string("-1");
    expectedMessage =
        "context selector score is not a nonnegative constant integer "
        "expression";
  } else if (mode == "invalid-context-property-enum") {
    Rose::AstJson::NodeRecord &property =
        firstRecord(ast, "SgOmpContextSelectorProperty");
    property.properties.object["context_kind"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage =
        "SgOmpContextSelectorProperty has an invalid context kind";
  } else if (mode == "invalid-context-atomic-enum") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector = selectorInSet(
        ast, set, SgOmpClause::e_omp_context_trait_atomic_default_mem_order);
    propertyAt(ast, selector, 0).properties.object["atomic_default_mem_order"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpContextSelectorProperty has an invalid "
                      "atomic_default_mem_order kind";
  } else if (mode == "invalid-context-requires-enum") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_requires);
    propertyAt(ast, selector, 0).properties.object["requires_kind"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage =
        "SgOmpContextSelectorProperty has an invalid requires kind";
  } else if (mode == "invalid-context-requires-atomic-enum") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_requires);
    propertyAt(ast, selector, 0)
        .properties.object["requires_atomic_default_mem_order"] =
        Rose::AstJson::JsonValue::number("999");
    expectedMessage = "SgOmpContextSelectorProperty has an invalid requires "
                      "atomic_default_mem_order kind";
  } else if (mode == "wrong-context-requires-payload") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_requires);
    propertyAt(ast, selector, 0)
        .properties.object["requires_atomic_default_mem_order"] =
        Rose::AstJson::JsonValue::number(std::to_string(
            SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire));
    expectedMessage = "ordinary requires property owns a mismatched payload";
  } else if (mode == "missing-context-custom-name") {
    Rose::AstJson::NodeRecord &set = selectorSet(
        ast, SgOmpClause::e_omp_context_selector_set_implementation);
    Rose::AstJson::NodeRecord &selector = selectorInSet(
        ast, set, SgOmpClause::e_omp_context_trait_implementation_user);
    selector.properties.object["implementation_defined_name"] =
        Rose::AstJson::JsonValue::string("");
    expectedMessage = "implementation-defined selector has no exact name";
  } else if (mode == "kind-any-with-another-property") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_kind);
    propertyAt(ast, selector, 0).properties.object["context_kind"] =
        Rose::AstJson::JsonValue::number(
            std::to_string(SgOmpClause::e_omp_when_context_kind_any));
    expectedMessage = "kind(any) is not the only property in its selector";
  } else if (mode == "kind-any-with-another-trait") {
    Rose::AstJson::NodeRecord &set =
        selectorSet(ast, SgOmpClause::e_omp_context_selector_set_target_device);
    Rose::AstJson::NodeRecord &selector =
        selectorInSet(ast, set, SgOmpClause::e_omp_context_trait_kind);
    std::vector<Rose::AstJson::EdgeRecord> properties =
        Rose::AstJson::edgesFor(selector, "properties");
    if (properties.size() != 2) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires two kind properties");
    }
    propertyAt(ast, selector, 0).properties.object["context_kind"] =
        Rose::AstJson::JsonValue::number(
            std::to_string(SgOmpClause::e_omp_when_context_kind_any));
    removeEdges(selector, "properties");
    selector.edges.push_back(properties.front());
    expectedMessage = "kind(any) selector is not the only selector in its set";
  } else if (mode == "combined-order-absent" ||
             mode == "combined-order-duplicate" ||
             mode == "combined-order-out-of-range") {
    std::vector<Rose::AstJson::NodeRecord *> combinedClauses;
    for (Rose::AstJson::NodeRecord &record : ast.nodes) {
      const Rose::AstJson::JsonValue *order =
          record.properties.find("combined_source_order");
      if (order != nullptr &&
          order->kind == Rose::AstJson::JsonValue::Kind::Number) {
        combinedClauses.push_back(&record);
      }
    }
    if (combinedClauses.size() < 2) {
      throw std::runtime_error(
          "AST JSON rejection fixture requires multiple combined clauses");
    }
    if (mode == "combined-order-absent") {
      combinedClauses.front()->properties.object["combined_source_order"] =
          Rose::AstJson::JsonValue::null();
    } else if (mode == "combined-order-duplicate") {
      combinedClauses[1]->properties.object["combined_source_order"] =
          combinedClauses.front()->properties.at("combined_source_order");
    } else {
      combinedClauses.front()->properties.object["combined_source_order"] =
          Rose::AstJson::JsonValue::number(
              std::to_string(combinedClauses.size()));
    }
    expectedMessage =
        "AST JSON combined OpenMP clause source order is absent, duplicate, "
        "or out of range";
  } else if (mode == "noncombined-order-present") {
    Rose::AstJson::NodeRecord &clause = uniqueRecord(ast, "SgOmpAtClause");
    clause.properties.object["combined_source_order"] =
        Rose::AstJson::JsonValue::number("0");
    expectedMessage =
        "AST JSON non-combined OpenMP directive owns combined clause source "
        "order";
  } else if (mode == "missing-combined-order-property") {
    uniqueRecord(ast, "SgOmpAtClause")
        .properties.object.erase("combined_source_order");
    expectedMessage = "AST JSON object is missing key: combined_source_order";
  } else if (mode == "missing-declare-simd-target") {
    removeEdges(uniqueRecord(ast, "SgOmpDeclareSimdStatement"), "function_ref");
    expectedMessage = "is missing required edge 'function_ref'";
  } else if (mode == "negative-declare-simd-ordinal" ||
             mode == "oversized-declare-simd-ordinal") {
    const std::string ordinal = mode == "negative-declare-simd-ordinal"
                                    ? "-1"
                                    : oversizedSizeTJsonInteger();
    uniqueRecord(ast, "SgOmpDeclareSimdStatement")
        .properties.object["semantic_variant_ordinal"] =
        Rose::AstJson::JsonValue::number(ordinal);
    expectedMessage =
        "AST JSON declare simd semantic variant ordinal is out of range";
  } else if (mode == "missing-declare-variant-base") {
    removeEdges(uniqueRecord(ast, "SgOmpDeclareVariantStatement"),
                "base_function_ref");
    expectedMessage = "is missing required edge 'base_function_ref'";
  } else if (mode == "negative-declare-variant-ordinal" ||
             mode == "oversized-declare-variant-ordinal") {
    const std::string ordinal = mode == "negative-declare-variant-ordinal"
                                    ? "-1"
                                    : oversizedSizeTJsonInteger();
    uniqueRecord(ast, "SgOmpDeclareVariantStatement")
        .properties.object["semantic_variant_ordinal"] =
        Rose::AstJson::JsonValue::number(ordinal);
    expectedMessage =
        "AST JSON declare variant semantic variant ordinal is out of range";
  } else {
    throw std::runtime_error("unknown AST JSON mutation mode: " + mode);
  }

  try {
    (void)Rose::AstJson::reconstructSourceFile(ast, file);
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(expectedMessage) == std::string::npos) {
      std::fprintf(stderr, "unexpected AST JSON rejection for %s: %s\n",
                   mode.c_str(), error.what());
      return 3;
    }
    std::fprintf(stderr, "REX_AST_JSON_REJECTION_DETAIL[%s]: %s\n",
                 mode.c_str(), error.what());
    std::fprintf(stderr, "REX_AST_JSON_REJECTION[%s]: %s\n", mode.c_str(),
                 expectedMessage.c_str());
    return 2;
  }

  std::fprintf(stderr, "malformed AST JSON was accepted for %s\n",
               mode.c_str());
  return 0;
}
