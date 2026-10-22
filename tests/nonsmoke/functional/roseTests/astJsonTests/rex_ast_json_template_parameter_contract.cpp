#include "sageAstJsonPrivate.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct DeleteAst {
  void operator()(SgNode *node) const { SageInterface::deleteAST(node); }
};

void requireFailure(const std::function<void()> &operation,
                    const std::string &expected) {
  try {
    operation();
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()).find(expected) != std::string::npos) {
      return;
    }
    throw;
  }
  throw std::runtime_error("malformed template parameter was accepted");
}

void requireTemplateArgumentPayloadPreflight() {
  using namespace Rose::AstJson;
  std::unique_ptr<SgNullStatement, DeleteAst> preceding(
      SageBuilder::buildNullStatement());
  auto argument =
      std::make_unique<SgTemplateArgument>(SageBuilder::buildIntType(), true);
  std::unique_ptr<SgIntVal> value(SageBuilder::buildIntVal(7));
  const std::unordered_map<const SgNode *, uint64_t> ids = {
      {preceding.get(), 1}, {argument.get(), 2}, {value.get(), 3}};
  AstFileRecord graph;
  for (SgNode *node : {static_cast<SgNode *>(preceding.get()),
                       static_cast<SgNode *>(argument.get()),
                       static_cast<SgNode *>(value.get())}) {
    NodeRecord record;
    record.id = ids.at(node);
    record.kind = node->class_name();
    record.flags = parseJson(rawNodeFlagsJson(node));
    record.location = parseJson(rawLocationJson(node));
    record.properties = parseJson(rawNodeProperties(node, ids));
    graph.index_by_id.emplace(record.id, graph.nodes.size());
    graph.nodes.push_back(std::move(record));
  }
  graph.nodes[2].edges.push_back({"parent", 2, 0});
  const auto arguments = SgTemplateArgument::numberOfNodes();
  const auto statements = SgNullStatement::numberOfNodes();
  const auto positions = Sg_File_Info::numberOfNodes();
  const auto files = SgSourceFile::numberOfNodes();
  const NodeMap borrowed = {{3, value.get()}};
  const auto require_unchanged = [&](const NodeMap &nodes) {
    ROSE_ASSERT(nodes == borrowed);
    ROSE_ASSERT(value->get_parent() == nullptr);
    ROSE_ASSERT(SgTemplateArgument::numberOfNodes() == arguments);
    ROSE_ASSERT(SgNullStatement::numberOfNodes() == statements);
    ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions);
    ROSE_ASSERT(SgSourceFile::numberOfNodes() == files);
  };
  const auto require_rejection = [&](const std::string &expected) {
    NodeMap nodes = borrowed;
    requireFailure([&] { rebuildConstructorOnlyNodes(graph, nodes); },
                   expected);
    requireFailure([&] { (void)reconstructSourceFile(graph, nullptr); },
                   expected);
    require_unchanged(nodes);
  };
  auto &properties = graph.nodes[1].properties.object;
  properties.at("argument_type") =
      parseJson(std::to_string(SgTemplateArgument::nontype_argument));
  properties.at("expression") = parseJson(rawExpressionRef(value.get(), ids));
  graph.nodes[1].edges = {{"expression", 3, 0}};
  for (const char *field : {"is_modified", "contains_transformation",
                            "contains_transformation_to_surrounding_whitespace",
                            "source_range_ends_in_macro_expansion",
                            "source_range_is_macro_expansion_fragment"}) {
    graph.nodes[1].flags = parseJson(rawNodeFlagsJson(argument.get()));
    graph.nodes[1].flags.object.erase(field);
    require_rejection(std::string("missing key: ") + field);
  }
  graph.nodes[1].flags = parseJson(rawNodeFlagsJson(argument.get()));
  properties.at("argument_type") =
      parseJson(std::to_string(SgTemplateArgument::type_argument));
  properties.at("expression") = parseJson(rawExpressionRef(nullptr, {}));
  graph.nodes[1].edges.clear();
  for (const int legacy_id : {0, 3}) {
    properties.insert_or_assign("initialized_name",
                                parseJson(std::to_string(legacy_id)));
    require_rejection("removed initialized-name template argument payload");
  }
  properties.erase("initialized_name");
  graph.nodes[1].edges = {{"initializedName", 3, 0}};
  require_rejection("removed initialized-name template argument payload");
  properties.at("expression") = parseJson(rawExpressionRef(value.get(), ids));
  graph.nodes[1].edges = {{"expression", 3, 0}};
  for (const auto kind :
       {SgTemplateArgument::type_argument, SgTemplateArgument::nontype_argument,
        SgTemplateArgument::start_of_pack_expansion_argument}) {
    graph.nodes[1].properties.object.at("argument_type") =
        parseJson(std::to_string(kind));
    if (kind != SgTemplateArgument::type_argument)
      graph.nodes[1].properties.object.at("type") =
          parseJson(R"({"present":false})");
    require_rejection("incompatible semantic payloads");
  }
  properties.at("argument_type") =
      parseJson(std::to_string(SgTemplateArgument::nontype_argument));
  properties.at("type") =
      parseJson(rawTypeJson(SageBuilder::buildIntType(), {}));
  properties.at("expression") = parseJson(rawExpressionRef(value.get(), ids));
  properties.at("expression").object.at("node") = parseJson("1");
  graph.nodes[1].edges = {{"expression", 1, 0}};
  graph.nodes[0].edges = {{"parent", 2, 0}};
  require_rejection("expression has no exact owner");
  properties.at("argument_type") =
      parseJson(std::to_string(SgTemplateArgument::template_template_argument));
  properties.at("type") = parseJson(R"({"present":false})");
  properties.at("expression") = parseJson(rawExpressionRef(nullptr, {}));
  properties.at("template_declaration") = parseJson("1");
  graph.nodes[1].edges = {{"templateDeclaration", 1, 0}};
  require_rejection("declaration is not a template-bearing AST category");
}

void requireSharedDeclaratorType() {
  using namespace Rose::AstJson;
  std::unique_ptr<SgDeclarationScope, DeleteAst> scope(
      SageBuilder::buildDeclarationScope());
  auto type = std::make_unique<SgTemplateType>(SgName("T"));
  auto parameter = std::make_unique<SgTemplateParameter>(
      SgTemplateParameter::nontype_parameter, type.get());
  auto location = std::make_unique<Sg_File_Info>(
      "rex_ast_json_template_parameter_contract.cpp", 1, 1);
  const int file_id = location->get_file_id();
  parameter->initialize_canonical_source_identity({file_id, 11, file_id, 11});
  std::unique_ptr<SgInitializedName> name(
      constructInitializedName(SgName("N"), type.get()));
  // The declaration worklist publishes the lexical scope before this phase.
  name->set_scope(scope.get());
  name->set_parent(parameter.get());
  parameter->set_initializedName(name.get());
  const std::unordered_map<const SgNode *, uint64_t> ids = {
      {parameter.get(), 1}, {name.get(), 2}};
  NodeRecord parameter_record;
  parameter_record.id = 1;
  parameter_record.kind = "SgTemplateParameter";
  parameter_record.flags = parseJson(rawNodeFlagsJson(parameter.get()));
  parameter_record.location = parseJson(rawLocationJson(parameter.get()));
  parameter_record.properties =
      parseJson(rawNodeProperties(parameter.get(), ids));
  {
    std::unique_ptr<SgIntVal> value(SageBuilder::buildIntVal(7));
    NodeRecord conflicting = parameter_record;
    conflicting.edges.push_back({"defaultExpressionParameter", 3, 0});
    const NodeMap nodes = {
        {1, parameter.get()}, {2, name.get()}, {3, value.get()}};
    requireFailure([&] { linkNodeEdges(conflicting, nodes); },
                   "conflicting default_expression_parameter");
    ROSE_ASSERT(parameter->get_defaultExpressionParameter() == nullptr);
    ROSE_ASSERT(value->get_parent() == nullptr);
    conflicting.properties.object.at("default_expression_parameter") =
        parseJson(rawExpressionRef(value.get(), {{value.get(), 3}}));
    value->set_parent(scope.get());
    requireFailure([&] { linkNodeEdges(conflicting, nodes); },
                   "foreign default_expression_parameter");
    ROSE_ASSERT(parameter->get_defaultExpressionParameter() == nullptr);
    ROSE_ASSERT(value->get_parent() == scope.get());
    value->set_parent(nullptr);
    requireFailure([&] { linkNodeEdges(conflicting, nodes); },
                   "defaults differ from construction");
    ROSE_ASSERT(parameter->get_defaultExpressionParameter() == nullptr);
    ROSE_ASSERT(value->get_parent() == nullptr);
  }
  for (const auto &[edge, property] :
       {std::pair{"templateDeclaration", "template_declaration"},
        std::pair{"defaultTemplateArgument", "default_template_argument"},
        std::pair{"initializedName", "initialized_name"},
        std::pair{"sourceSpelledTemplateDeclaration",
                  "source_spelled_template_declaration"}}) {
    NodeRecord conflicting = parameter_record;
    conflicting.edges.push_back({edge, 99, 0});
    requireFailure(
        [&] {
          linkNodeEdges(conflicting, {{1, parameter.get()}, {2, name.get()}});
        },
        std::string("conflicting ") + property);
    ROSE_ASSERT(parameter->get_initializedName() == name.get());
    ROSE_ASSERT(name->get_parent() == parameter.get());
  }
  const auto parameter_count = SgTemplateParameter::numberOfNodes();
  {
    NodeRecord obsolete = parameter_record;
    obsolete.properties.object["expression"] = parseJson("{\"node\":3}");
    requireFailure(
        [&] {
          std::unique_ptr<SgNode> unexpected(
              createNodeFromRecord(obsolete, nullptr));
        },
        "cannot own an expression payload");
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
  }
  requireFailure(
      [&] {
        std::unique_ptr<SgNode> unexpected(
            createNodeFromRecord(parameter_record, nullptr));
      },
      "requires typed declarator construction");
  ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
  for (const char *field :
       {"parameter_type", "template_parameter_keyword", "is_parameter_pack",
        "is_abbreviated_function_template_parameter",
        "canonical_source_identity"}) {
    NodeRecord missing = parameter_record;
    missing.properties.object.erase(field);
    requireFailure(
        [&]() {
          std::unique_ptr<SgNode> unexpected(
              createNodeFromRecord(missing, nullptr));
        },
        std::string("missing key: ") + field);
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
  }
  {
    NodeRecord incompatible = parameter_record;
    incompatible.properties.object.at("template_parameter_keyword") =
        parseJson(std::to_string(SgTemplateParameter::keyword_class));
    requireFailure(
        [&]() {
          std::unique_ptr<SgNode> unexpected(
              createNodeFromRecord(incompatible, nullptr));
        },
        "kind and source keyword are incompatible");
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
  }
  AstFileRecord missing_owner;
  missing_owner.nodes.push_back(parameter_record);
  requireFailure([&] { collectDelayedDeclarations(missing_owner, {}); },
                 "missing required edge 'parent'");
  for (int id : {0, -1}) {
    AstFileRecord missing_declarator;
    missing_declarator.nodes.push_back(parameter_record);
    missing_declarator.nodes.front().properties.object.at("initialized_name") =
        parseJson(std::to_string(id));
    ROSE_ASSERT(requiresDelayedRebuild(missing_declarator.nodes.front()));
    requireFailure([&] { collectDelayedDeclarations(missing_declarator, {}); },
                   "non-type parameter has no declarator");
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
  }
  auto owner = std::make_unique<SgTemplateParameterList>();
  parameter_record.edges.push_back({"parent", 4, 0});
  NodeRecord name_record;
  name_record.id = 2;
  name_record.kind = "SgInitializedName";
  name_record.properties = parseJson(rawNodeProperties(name.get(), ids));
  name_record.edges.push_back({"parent", 1, 0});
  const auto type_count = SgTemplateType::numberOfNodes();
  for (bool parameter_first : {false, true}) {
    parameter_record.properties.object.at("is_parameter_pack") =
        parseJson(parameter_first ? "true" : "false");
    AstFileRecord ast;
    ast.nodes = parameter_first
                    ? std::vector<NodeRecord>{parameter_record, name_record}
                    : std::vector<NodeRecord>{name_record, parameter_record};
    NodeRecord source_parameter_record = parameter_record;
    source_parameter_record.id = 5;
    source_parameter_record.properties.object.at("initialized_name") =
        parseJson("6");
    NodeRecord source_name_record = name_record;
    source_name_record.id = 6;
    source_name_record.edges = {{"parent", 5, 0}};
    ast.nodes.push_back(source_parameter_record);
    ast.nodes.push_back(source_name_record);
    std::unique_ptr<SgIntVal> default_value(SageBuilder::buildIntVal(7));
    if (!parameter_first) {
      ast.nodes[1].properties.object.at("default_expression_parameter") =
          parseJson(rawExpressionRef(default_value.get(),
                                     {{default_value.get(), 7}}));
      ast.nodes[1].edges.push_back({"defaultExpressionParameter", 7, 0});
      NodeRecord value_record;
      value_record.id = 7;
      value_record.kind = "SgIntVal";
      value_record.edges.push_back({"parent", 1, 0});
      ast.nodes.push_back(value_record);
    }
    for (size_t i = 0; i < ast.nodes.size(); ++i)
      ast.index_by_id.emplace(ast.nodes[i].id, i);
    name->set_parent(nullptr);
    parameter.reset();
    std::unique_ptr<SgInitializedName> source_name(
        constructInitializedName(SgName("N"), type.get()));
    source_name->set_scope(scope.get());
    NodeMap nodes = {{2, name.get()}, {4, owner.get()}, {6, source_name.get()}};
    if (!parameter_first)
      nodes.emplace(7, default_value.get());
    std::vector<const NodeRecord *> pending = {&ast.node(5), &ast.node(1)};
    drainDelayedDeclarations(ast, pending, nodes, true);
    parameter.reset(nodeByIdAs<SgTemplateParameter>(nodes, 1));
    std::unique_ptr<SgTemplateParameter> source_parameter(
        nodeByIdAs<SgTemplateParameter>(nodes, 5));
    ROSE_ASSERT(parameter->get_canonicalNonTypeParameter() == parameter.get());
    ROSE_ASSERT(source_parameter->get_canonicalNonTypeParameter() ==
                parameter.get());
    ROSE_ASSERT(parameter->get_canonical_source_identity().has_value());
    ROSE_ASSERT(source_parameter->get_canonical_source_identity() ==
                parameter->get_canonical_source_identity());
    ROSE_ASSERT(pending.empty());
    if (!parameter_first) {
      ROSE_ASSERT(parameter->get_defaultExpressionParameter() ==
                  default_value.get());
      ROSE_ASSERT(default_value->get_parent() == parameter.get());
    }
    validateReconstructedTypesAndSymbols(ast, nodes);
    ROSE_ASSERT(parameter->get_type() == type.get());
    ROSE_ASSERT(parameter->get_parameterType() ==
                SgTemplateParameter::nontype_parameter);
    ROSE_ASSERT(parameter->get_templateParameterKeyword() ==
                SgTemplateParameter::keyword_unspecified);
    ROSE_ASSERT(parameter->get_is_parameter_pack() == parameter_first);
    ROSE_ASSERT(parameter->get_initializedName() == name.get());
    ROSE_ASSERT(parameter->get_parent() == owner.get());
    ROSE_ASSERT(name->get_type() == type.get());
    ROSE_ASSERT(SgTemplateType::numberOfNodes() == type_count);

    source_name->set_parent(nullptr);
    source_parameter.reset();
    nodes.erase(5);
    ast.nodes[ast.index_by_id.at(1)].properties.object.at("type") =
        parseJson(rawTypeJson(SageBuilder::buildIntType(), {}));
    name->set_parent(nullptr);
    if (!parameter_first) {
      parameter->set_defaultExpressionParameter(nullptr);
      default_value->set_parent(nullptr);
    }
    parameter.reset();
    nodes.erase(1);
    pending = {&ast.node(1)};
    requireFailure(
        [&]() { drainDelayedDeclarations(ast, pending, nodes, true); },
        "no exactly owned and typed declarator");
    ROSE_ASSERT(nodes.count(1) == 0);
    ROSE_ASSERT(name->get_parent() == nullptr);
    ROSE_ASSERT(name->get_type() == type.get());
    ROSE_ASSERT(SgTemplateType::numberOfNodes() == type_count);
  }
  name->set_parent(nullptr);
}

} // namespace

namespace {
void requireTemplateTypeUseSpelling() {
  using namespace Rose::AstJson;
  auto canonical = std::make_unique<SgTemplateType>(SgName("T"));
  auto location = std::make_unique<Sg_File_Info>(
      "rex_ast_json_template_parameter_contract.cpp", 1, 1);
  const int file = location->get_file_id();
  canonical->initialize_canonical_source_identity({file, 17, file, 17});
  canonical->set_template_parameter_depth(0);
  canonical->set_template_parameter_position(0);
  std::unique_ptr<SgTemplateParameter> parameter(
      SageBuilder::buildTemplateParameter(SgTemplateParameter::type_parameter,
                                          canonical.get(),
                                          SgTemplateParameter::keyword_class));
  SgTemplateType use(SgName("Renamed"));
  use.initialize_canonical_source_identity(
      *canonical->get_canonical_source_identity());
  use.set_template_parameter_depth(0);
  use.set_template_parameter_position(0);
  use.set_template_parameter(parameter.get());
  ROSE_ASSERT(SageInterface::isEquivalentType(canonical.get(), &use));
  ROSE_ASSERT(
      SageInterface::cxxSourceTypeMatchesSemanticType(&use, canonical.get()));
  TemplateTypeDeserializationIdentityGuard identities;
  JsonValue json = parseJson(rawTypeJson(&use, {{parameter.get(), 1}}));
  std::unique_ptr<SgTemplateType> restored(
      isSgTemplateType(typeFromJson(json, {{1, parameter.get()}})));
  ROSE_ASSERT(restored != nullptr && restored.get() != canonical.get());
  ROSE_ASSERT(restored->get_name() == use.get_name());
  ROSE_ASSERT(restored->get_template_parameter() == parameter.get());
  ROSE_ASSERT(SageInterface::isEquivalentType(restored.get(), canonical.get()));
  ROSE_ASSERT(typeFromJson(json, {{1, parameter.get()}}) == restored.get());
  ROSE_ASSERT(typeFromJson(parseJson(rawTypeJson(canonical.get(),
                                                 {{parameter.get(), 1}})),
                           {{1, parameter.get()}}) == canonical.get());
  const auto types = SgTemplateType::numberOfNodes();
  for (const char *coordinate :
       {"template_parameter_depth", "template_parameter_position"}) {
    JsonValue malformed = json;
    malformed.object.at(coordinate) = parseJson("1");
    requireFailure(
        [&] { (void)typeFromJson(malformed, {{1, parameter.get()}}); },
        "exact template parameter coordinates");
    ROSE_ASSERT(SgTemplateType::numberOfNodes() == types);
  }
  JsonValue unrelated = json;
  unrelated.object.at("canonical_source_identity")
      .object.at("expansion_file_offset") = parseJson("18");
  requireFailure([&] { (void)typeFromJson(unrelated, {{1, parameter.get()}}); },
                 "exact template parameter coordinates");
  ROSE_ASSERT(SgTemplateType::numberOfNodes() == types);
  ROSE_ASSERT(parameter->get_type() == canonical.get());
  ROSE_ASSERT(canonical->get_name() == "T");
}

void requireParameterKinds() {
  using Rose::AstJson::validateTemplateParameterContract;

  std::unique_ptr<SgTemplateType> type_parameter_type(
      SageBuilder::buildTemplateType(SgName("T")));
  auto type_parameter = std::make_unique<SgTemplateParameter>(
      SgTemplateParameter::type_parameter, type_parameter_type.get());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(type_parameter.get(), "test");
      },
      "no exact source keyword");
  type_parameter->set_templateParameterKeyword(
      SgTemplateParameter::keyword_typename);
  validateTemplateParameterContract(type_parameter.get(), "test");
  type_parameter_type->set_template_parameter(type_parameter.get());
  {
    using namespace Rose::AstJson;
    NodeRecord record;
    record.id = 1;
    record.kind = "SgTemplateParameter";
    record.flags = parseJson(rawNodeFlagsJson(type_parameter.get()));
    record.location = parseJson(rawLocationJson(type_parameter.get()));
    record.properties = parseJson(
        rawNodeProperties(type_parameter.get(), {{type_parameter.get(), 1}}));
    record.properties.object.at("default_type_parameter") =
        parseJson(rawTypeJson(SageBuilder::buildIntType(), {}));
    {
      std::unique_ptr<SgTemplateParameter> restored(
          isSgTemplateParameter(createNodeFromRecord(record, nullptr)));
      std::unique_ptr<SgTemplateType> restored_type(
          isSgTemplateType(restored->get_type()));
      ROSE_ASSERT(restored_type != nullptr);
      ROSE_ASSERT(restored_type->get_template_parameter() == restored.get());
      ROSE_ASSERT(restored_type->get_name() == type_parameter_type->get_name());
      ROSE_ASSERT(restored->get_defaultTypeParameter() ==
                  SageBuilder::buildIntType());
    }
    const auto parameter_count = SgTemplateParameter::numberOfNodes();
    const auto type_count = SgTemplateType::numberOfNodes();
    {
      NodeRecord missing = record;
      missing.properties.object.erase("default_type_parameter");
      requireFailure([&] { (void)createNodeFromRecord(missing, nullptr); },
                     "missing key: default_type_parameter");
      ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
      ROSE_ASSERT(SgTemplateType::numberOfNodes() == type_count);
    }
    {
      SgPointerType pointer(SageBuilder::buildIntType());
      const auto pointers = SgPointerType::numberOfNodes();
      NodeRecord malformed = record;
      malformed.properties.object.at("default_type_parameter") =
          parseJson(rawTypeJson(&pointer, {}));
      malformed.properties.object.at("type").object.at("template_parameter") =
          parseJson("99");
      for (int attempt = 0; attempt < 2; ++attempt) {
        requireFailure([&] { (void)createNodeFromRecord(malformed, nullptr); },
                       "no exact canonical type identity");
        ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
        ROSE_ASSERT(SgTemplateType::numberOfNodes() == type_count);
        ROSE_ASSERT(SgPointerType::numberOfNodes() == pointers);
      }
    }
    {
      auto owner = std::make_unique<SgTemplateParameterList>();
      NodeRecord first = record;
      first.edges.push_back({"parent", 4, 0});
      NodeRecord second = first;
      second.id = 2;
      second.properties.object.at("type").object.at("template_parameter") =
          parseJson("2");
      second.properties.object.at("type").object.at("name") =
          parseJson("\"U\"");
      second.properties.object.at("default_type_parameter") =
          first.properties.at("type");
      AstFileRecord ast;
      ast.nodes = {second, first};
      ast.index_by_id = {{2, 0}, {1, 1}};
      NodeMap nodes = {{4, owner.get()}};
      auto pending = collectDelayedDeclarations(ast, nodes);
      drainDelayedDeclarations(ast, pending, nodes, true);
      std::unique_ptr<SgTemplateParameter> first_parameter(
          nodeByIdAs<SgTemplateParameter>(nodes, 1));
      std::unique_ptr<SgTemplateParameter> second_parameter(
          nodeByIdAs<SgTemplateParameter>(nodes, 2));
      std::unique_ptr<SgTemplateType> first_type(
          isSgTemplateType(first_parameter->get_type()));
      std::unique_ptr<SgTemplateType> second_type(
          isSgTemplateType(second_parameter->get_type()));
      ROSE_ASSERT(pending.empty());
      ROSE_ASSERT(second_parameter->get_defaultTypeParameter() ==
                  first_type.get());
      ROSE_ASSERT(first_type->get_template_parameter() ==
                  first_parameter.get());
      ROSE_ASSERT(second_type->get_template_parameter() ==
                  second_parameter.get());
      ROSE_ASSERT(first_parameter->get_parent() == owner.get());
      ROSE_ASSERT(second_parameter->get_parent() == owner.get());
      ast.nodes.front().properties.object.at("default_type_parameter") =
          second.properties.at("type");
      nodes.erase(2);
      pending = {&ast.node(2)};
      requireFailure(
          [&] { drainDelayedDeclarations(ast, pending, nodes, true); },
          "missing or cyclic dependencies");
      ROSE_ASSERT(nodes.count(2) == 0);
    }
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameter_count);
    ROSE_ASSERT(SgTemplateType::numberOfNodes() == type_count);
    record.properties.object.at("type").object.at("kind") =
        parseJson("\"SgNonrealType\"");
    requireFailure(
        [&] { (void)templateParameterSourceFieldsFromJson(record.properties); },
        "type parameter requires an exact template-parameter type");
    const auto count = SgTemplateParameter::numberOfNodes();
    for (auto kind : {SgTemplateParameter::type_parameter,
                      SgTemplateParameter::template_parameter}) {
      record.properties.object.at("parameter_type") =
          parseJson(std::to_string(kind));
      for (const char *type_json :
           {R"({"present":false})", R"({"present":true,"kind":"SgTypeInt"})"}) {
        record.properties.object.at("type") = parseJson(type_json);
        requireFailure(
            [&] {
              std::unique_ptr<SgNode> unexpected(
                  createNodeFromRecord(record, nullptr));
            },
            "requires an exact template-parameter type");
        ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == count);
      }
    }
  }
  auto invalid_type_owner =
      std::make_unique<SgTemplateDeclaration>(SgName("invalid"));
  type_parameter->set_templateDeclaration(invalid_type_owner.get());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(type_parameter.get(), "test");
      },
      "type parameter owns a template declaration");

  SgType *int_type = SageBuilder::buildIntType();
  auto non_type_parameter = std::make_unique<SgTemplateParameter>(
      SgTemplateParameter::nontype_parameter, int_type);
  std::unique_ptr<SgInitializedName, DeleteAst> initialized_name(
      SageBuilder::buildInitializedName(SgName("N"), int_type));
  initialized_name->set_parent(non_type_parameter.get());
  non_type_parameter->set_initializedName(initialized_name.get());
  validateTemplateParameterContract(non_type_parameter.get(), "test");
  non_type_parameter->set_type(SageBuilder::buildUnknownType());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(non_type_parameter.get(), "test");
      },
      "no exact semantic type");
  non_type_parameter->set_type(int_type);
  non_type_parameter->set_initializedName(nullptr);
  requireFailure(
      [&]() {
        validateTemplateParameterContract(non_type_parameter.get(), "test");
      },
      "must own its declaration");
  non_type_parameter->set_initializedName(initialized_name.get());
  initialized_name->set_parent(type_parameter.get());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(non_type_parameter.get(), "test");
      },
      "must own its declaration");
  initialized_name->set_parent(non_type_parameter.get());
  std::unique_ptr<SgInitializedName, DeleteAst> wrong_type_name(
      SageBuilder::buildInitializedName(SgName("N"),
                                        SageBuilder::buildFloatType()));
  wrong_type_name->set_parent(non_type_parameter.get());
  non_type_parameter->set_initializedName(wrong_type_name.get());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(non_type_parameter.get(), "test");
      },
      "different semantic type");

  std::unique_ptr<SgTemplateType> template_parameter_type(
      SageBuilder::buildTemplateType(SgName("C")));
  auto template_parameter = std::make_unique<SgTemplateParameter>(
      SgTemplateParameter::template_parameter, template_parameter_type.get());
  template_parameter->set_templateParameterKeyword(
      SgTemplateParameter::keyword_class);
  auto template_identity =
      std::make_unique<SgTemplateDeclaration>(SgName("identity"));
  template_parameter->set_templateDeclaration(template_identity.get());
  validateTemplateParameterContract(template_parameter.get(), "test");
  template_parameter_type->set_template_parameter(template_parameter.get());
  {
    using namespace Rose::AstJson;
    std::unique_ptr<SgDeclarationScope, DeleteAst> source_scope(
        SageBuilder::buildDeclarationScope());
    auto target =
        std::make_unique<SgTemplateDeclaration>(SgName("default_target"));
    auto argument = std::make_unique<SgTemplateArgument>(target.get(), false);
    auto source =
        std::make_unique<SgTemplateDeclaration>(SgName("source_template"));
    source->set_scope(source_scope.get());
    template_parameter->set_defaultTemplateArgument(argument.get());
    argument->set_parent(template_parameter.get());
    template_parameter->set_sourceSpelledTemplateDeclaration(source.get());
    source->set_parent(template_parameter.get());
    validateTemplateParameterContract(template_parameter.get(), "test");
    NodeRecord record;
    record.id = 1;
    record.kind = "SgTemplateParameter";
    record.flags = parseJson(rawNodeFlagsJson(template_parameter.get()));
    record.location = parseJson(rawLocationJson(template_parameter.get()));
    record.properties = parseJson(rawNodeProperties(
        template_parameter.get(), {{template_parameter.get(), 1},
                                   {template_identity.get(), 2},
                                   {argument.get(), 3},
                                   {source.get(), 4}}));
    record.edges = {{"templateDeclaration", 2, 0},
                    {"defaultTemplateArgument", 3, 0},
                    {"sourceSpelledTemplateDeclaration", 4, 0}};
    template_parameter->set_defaultTemplateArgument(nullptr);
    argument->set_parent(nullptr);
    template_parameter->set_sourceSpelledTemplateDeclaration(nullptr);
    source->set_parent(nullptr);
    const NodeMap nodes = {
        {2, template_identity.get()}, {3, argument.get()}, {4, source.get()}};
    {
      std::unique_ptr<SgTemplateParameter> restored(
          isSgTemplateParameter(createNodeFromRecord(record, nullptr, nodes)));
      std::unique_ptr<SgTemplateType> type(
          isSgTemplateType(restored->get_type()));
      ROSE_ASSERT(restored->get_defaultTemplateArgument() == argument.get());
      ROSE_ASSERT(argument->get_parent() == restored.get());
      ROSE_ASSERT(restored->get_sourceSpelledTemplateDeclaration() ==
                  source.get());
      ROSE_ASSERT(source->get_parent() == restored.get());
      validateTemplateParameterContract(restored.get(), "test");
      NodeMap linked = nodes;
      linked.emplace(1, restored.get());
      linkNodeEdges(record, linked);
      restored->set_defaultTemplateArgument(nullptr);
      argument->set_parent(nullptr);
      restored->set_sourceSpelledTemplateDeclaration(nullptr);
      source->set_parent(nullptr);
    }
    const auto parameters = SgTemplateParameter::numberOfNodes();
    source->set_parent(type_parameter.get());
    requireFailure([&] { (void)createNodeFromRecord(record, nullptr, nodes); },
                   "source template declaration has a foreign owner");
    ROSE_ASSERT(argument->get_parent() == nullptr);
    ROSE_ASSERT(source->get_parent() == type_parameter.get());
    source->set_parent(nullptr);
    source->set_scope(nullptr);
    requireFailure([&] { (void)createNodeFromRecord(record, nullptr, nodes); },
                   "malformed source-spelled template declaration");
    ROSE_ASSERT(argument->get_parent() == nullptr);
    ROSE_ASSERT(source->get_parent() == nullptr);
    source->set_scope(source_scope.get());
    {
      NodeRecord alias = record;
      alias.properties.object.at("source_spelled_template_declaration") =
          parseJson("2");
      alias.edges.back().target = 2;
      requireFailure([&] { (void)createNodeFromRecord(alias, nullptr, nodes); },
                     "malformed source-spelled template declaration");
      ROSE_ASSERT(template_identity->get_parent() == nullptr);
    }
    argument->set_parent(type_parameter.get());
    requireFailure([&] { (void)createNodeFromRecord(record, nullptr, nodes); },
                   "template default has a foreign owner");
    ROSE_ASSERT(argument->get_parent() == type_parameter.get());
    argument->set_parent(nullptr);
    argument->set_argumentType(SgTemplateArgument::type_argument);
    requireFailure([&] { (void)createNodeFromRecord(record, nullptr, nodes); },
                   "no exact template argument");
    ROSE_ASSERT(argument->get_parent() == nullptr);
    argument->set_argumentType(SgTemplateArgument::template_template_argument);
    record.properties.object.at("type").object.at("template_parameter") =
        parseJson("99");
    for (bool modified : {false, true}) {
      argument->set_isModified(modified);
      const auto sequence = argument->get_lastMutationSequence();
      source->set_isModified(modified);
      const auto source_sequence = source->get_lastMutationSequence();
      requireFailure(
          [&] { (void)createNodeFromRecord(record, nullptr, nodes); },
          "no exact canonical type identity");
      ROSE_ASSERT(argument->get_parent() == nullptr);
      ROSE_ASSERT(argument->get_isModified() == modified);
      ROSE_ASSERT(argument->get_lastMutationSequence() == sequence);
      ROSE_ASSERT(source->get_parent() == nullptr);
      ROSE_ASSERT(source->get_isModified() == modified);
      ROSE_ASSERT(source->get_lastMutationSequence() == source_sequence);
    }
    ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameters);
  }

  // Only the declaration variant is under test; no symbol table is needed.
  auto invalid_template_identity =
      std::make_unique<SgNonrealDecl>(SgName("invalid"), nullptr);
  template_parameter->set_templateDeclaration(invalid_template_identity.get());
  requireFailure(
      [&]() {
        validateTemplateParameterContract(template_parameter.get(), "test");
      },
      "exact SgTemplateDeclaration");
  template_parameter->set_templateDeclaration(nullptr);
  type_parameter->set_templateDeclaration(nullptr);
  non_type_parameter->set_initializedName(nullptr);
  initialized_name->set_parent(nullptr);
  wrong_type_name->set_parent(nullptr);
}
} // namespace

int main() {
  (void)SageBuilder::buildIntType();
  (void)SageBuilder::buildFloatType();
  const auto parameters = SgTemplateParameter::numberOfNodes();
  const auto names = SgInitializedName::numberOfNodes();
  const auto types = SgTemplateType::numberOfNodes();
  const auto declarations = SgTemplateDeclaration::numberOfNodes();
  const auto nonreal = SgNonrealDecl::numberOfNodes();
  const auto positions = Sg_File_Info::numberOfNodes();
  const auto scopes = SgDeclarationScope::numberOfNodes();
  requireTemplateArgumentPayloadPreflight();
  requireSharedDeclaratorType();
  requireTemplateTypeUseSpelling();
  requireParameterKinds();
  ROSE_ASSERT(SgTemplateParameter::numberOfNodes() == parameters);
  ROSE_ASSERT(SgInitializedName::numberOfNodes() == names);
  ROSE_ASSERT(SgTemplateType::numberOfNodes() == types);
  ROSE_ASSERT(SgTemplateDeclaration::numberOfNodes() == declarations);
  ROSE_ASSERT(SgNonrealDecl::numberOfNodes() == nonreal);
  ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions);
  ROSE_ASSERT(SgDeclarationScope::numberOfNodes() == scopes);
  return 0;
}
