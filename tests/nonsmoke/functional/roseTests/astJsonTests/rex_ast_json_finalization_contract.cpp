#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Rose::AstJson::AstFileRecord;
using Rose::AstJson::EdgeRecord;
using Rose::AstJson::JsonValue;
using Rose::AstJson::NodeRecord;

class SerializerSemanticTypeProbe final : public SgCastExp {
public:
  explicit SerializerSemanticTypeProbe(SgType *semanticType)
      : SgCastExp(static_cast<SgExpression *>(nullptr),
                  SageBuilder::buildIntType(), SgCastExp::e_implicit_cast,
                  SgCastExp::e_semantic_conversion_unclassified,
                  SgCastExp::e_value_category_prvalue),
        semanticType_(semanticType) {}

  SgType *get_type() const override { return semanticType_; }

private:
  SgType *semanticType_;
};

std::string takeMode(int &argc, char **argv) {
  constexpr const char prefix[] = "--finalization-contract=";
  std::string mode;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument.rfind(prefix, 0) == 0) {
      if (!mode.empty()) {
        throw std::runtime_error(
            "AST JSON finalization fixture received multiple modes");
      }
      mode = argument.substr(sizeof(prefix) - 1);
    } else {
      filtered.push_back(argv[index]);
    }
  }
  if (mode.empty()) {
    throw std::runtime_error("AST JSON finalization fixture received no mode");
  }
  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return mode;
}

SgSourceFile *sourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    throw std::runtime_error(
        "AST JSON finalization fixture requires one source file");
  }
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON finalization fixture input is not a source file");
  }
  return file;
}

NodeRecord &recordById(AstFileRecord &ast, uint64_t id) {
  for (NodeRecord &record : ast.nodes) {
    if (record.id == id) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON finalization fixture has no record id " +
                           std::to_string(id));
}

NodeRecord &firstRecord(AstFileRecord &ast, const std::string &kind) {
  for (NodeRecord &record : ast.nodes) {
    if (record.kind == kind) {
      return record;
    }
  }
  throw std::runtime_error("AST JSON finalization fixture has no " + kind);
}

NodeRecord &resolvedVariableNonrealReference(AstFileRecord &ast) {
  for (NodeRecord &record : ast.nodes) {
    if (record.kind == "SgNonrealRefExp" &&
        record.properties.requiredInt("resolved_variable_declaration") > 0) {
      return record;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no resolved variable-template "
      "nonreal reference");
}

NodeRecord &functionRecord(AstFileRecord &ast, const std::string &name) {
  for (NodeRecord &record : ast.nodes) {
    if (record.kind == "SgFunctionDeclaration" &&
        record.properties.requiredString("name") == name) {
      return record;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no requested function");
}

NodeRecord &partialSpecializationFirstDeclaration(AstFileRecord &ast) {
  for (NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateClassDeclaration" ||
        record.properties.requiredString("template_name") !=
            "rex_ast_json_partial_specialization" ||
        record.properties.requiredInt("specialization") !=
            SgDeclarationStatement::e_partial_specialization) {
      continue;
    }
    const uint64_t first = Rose::AstJson::requiredSingleEdgeTarget(
        record, "firstNondefiningDeclaration");
    const uint64_t defining =
        Rose::AstJson::requiredSingleEdgeTarget(record, "definingDeclaration");
    if (first == record.id && defining != 0 && defining != record.id) {
      return record;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no partial-specialization family");
}

JsonValue &typeOwnedArrayIndexProperties(AstFileRecord &ast) {
  for (NodeRecord &record : ast.nodes) {
    if (record.kind != "SgInitializedName" ||
        record.properties.requiredString("name") !=
            "rex_ast_json_type_owned_array") {
      continue;
    }
    JsonValue &arrayType = record.properties.object.at("type");
    if (arrayType.requiredString("kind") != "SgArrayType") {
      throw std::runtime_error(
          "AST JSON finalization fixture array has no exact array type");
    }
    JsonValue &index = arrayType.object.at("index");
    if (index.requiredInt("node") != 0 ||
        index.requiredString("owned_kind") != "SgIntVal") {
      throw std::runtime_error(
          "AST JSON finalization fixture array index is not an inline "
          "type-owned SgIntVal");
    }
    JsonValue &properties = index.object.at("properties");
    if (properties.find("type") == nullptr) {
      throw std::runtime_error(
          "AST JSON finalization fixture type-owned index has no semantic "
          "type");
    }
    return properties;
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no type-owned array index");
}

void removeEdges(NodeRecord &record, const std::string &field) {
  record.edges.erase(std::remove_if(record.edges.begin(), record.edges.end(),
                                    [&](const EdgeRecord &edge) {
                                      return edge.field == field;
                                    }),
                     record.edges.end());
}

uint64_t singleTarget(const NodeRecord &record, const std::string &field) {
  return Rose::AstJson::requiredSingleEdgeTarget(record, field);
}

NodeRecord &firstAuxiliaryDeclaration(AstFileRecord &ast,
                                      NodeRecord **containerOut = nullptr) {
  for (NodeRecord &container : ast.nodes) {
    if (container.kind != "SgAuxiliaryDeclarationList") {
      continue;
    }
    for (const EdgeRecord &edge :
         Rose::AstJson::edgesFor(container, "declarations")) {
      NodeRecord &declaration = recordById(ast, edge.target);
      if (declaration.kind != "SgFunctionDeclaration" ||
          declaration.properties.requiredString("name") !=
              "rex_ast_json_contract_function") {
        continue;
      }
      if (containerOut != nullptr) {
        *containerOut = &container;
      }
      return declaration;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no exact auxiliary function");
}

SgFunctionDeclaration *findFunction(SgSourceFile *file,
                                    const std::string &name) {
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name().getString() == name &&
        isSgGlobal(declaration->get_parent()) != nullptr) {
      return declaration;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture has no lexical external target");
}

SgTypedefDeclaration *findTypedef(SgSourceFile *file, const std::string &name) {
  SgTypedefDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error(
          "AST JSON finalization fixture has duplicate typedef targets");
    }
    result = declaration;
  }
  if (result == nullptr) {
    throw std::runtime_error(
        "AST JSON finalization fixture has no requested typedef");
  }
  return result;
}

void installExternalPeer(SgProject *project, SgSourceFile *file) {
  SgFunctionDeclaration *target =
      findFunction(file, "rex_ast_json_external_contract");
  SgFunctionDeclaration *peer =
      SageInterface::deepCopy<SgFunctionDeclaration>(target);
  if (peer == nullptr || target->get_scope() == nullptr) {
    throw std::runtime_error(
        "AST JSON finalization fixture could not clone an external peer");
  }
  peer->set_parent(project);
  peer->set_scope(target->get_scope());
  peer->set_firstNondefiningDeclaration(peer);
  peer->set_definingDeclaration(nullptr);
  target->set_firstNondefiningDeclaration(peer);
}

JsonValue &externalPeerRecord(AstFileRecord &ast) {
  for (NodeRecord &target : ast.nodes) {
    if (target.kind != "SgFunctionDeclaration" ||
        target.properties.requiredString("name") !=
            "rex_ast_json_external_contract") {
      continue;
    }
    JsonValue &external =
        target.properties.object.at("external_first_nondefining_declaration");
    if (external.requiredBool("present")) {
      return external;
    }
  }
  throw std::runtime_error(
      "AST JSON finalization fixture did not serialize its external peer");
}

JsonValue unresolvedSemanticType(const std::string &kind) {
  return JsonValue::objectValue(
      {{"present", JsonValue::boolean(true)},
       {"kind", JsonValue::string(kind)},
       {"fortran_source_syntax", JsonValue::boolean(false)}});
}

std::string expectedFailure(const std::string &mode) {
  if (mode == "missing-parameter-list") {
    return "missing required edge 'parameterList'";
  }
  if (mode == "invalid-function-type") {
    return "semantic function type has non-function kind SgTypeInt";
  }
  if (mode == "mismatched-return-type") {
    return "function_type and return_type disagree";
  }
  if (mode == "missing-declaration-scope") {
    return "auxiliary declaration has inconsistent lexical scope";
  }
  if (mode == "detached-declaration-scope") {
    return "has no exact semantic scope";
  }
  if (mode == "missing-auxiliary-parent") {
    return "SgAuxiliaryDeclarationList has malformed ownership";
  }
  if (mode == "wrong-auxiliary-scope") {
    return "auxiliary declaration has inconsistent lexical scope";
  }
  if (mode == "duplicate-emission-owner") {
    return "auxiliary declaration also has source-emission ownership";
  }
  if (mode == "missing-this-symbol") {
    return "SgThisExp must name exactly one class or nonreal symbol";
  }
  if (mode == "missing-nonreal-symbol") {
    return "SgNonrealRefExp symbol declaration must be a positive node ID";
  }
  if (mode == "inconsistent-nonreal-symbol") {
    return "AST JSON early SgNonrealRefExp symbol dependency is inconsistent";
  }
  if (mode == "inconsistent-nonreal-variable-identity") {
    return "resolved variable identity disagrees with its synthetic "
           "specialization edge";
  }
  if (mode == "inconsistent-specialization-family") {
    return "declaration specialization family is malformed";
  }
  if (mode == "null-template-argument") {
    return "template argument ID must be positive";
  }
  if (mode == "missing-external-context") {
    return "AST JSON object is missing key: scope_source";
  }
  if (mode == "invalid-external-scope") {
    return "has no exact serialized scope";
  }
  if (mode == "inconsistent-external-kind") {
    return "kind disagrees with external_function";
  }
  if (mode == "mismatched-source-order") {
    return "declaration translation-unit source order disagrees with its "
           "exact source-position occurrence";
  }
  if (mode == "cast-unknown-type" || mode == "cast-default-type") {
    return "AST JSON SgCastExp has no exact semantic value type during "
           "deserialization";
  }
  if (mode == "call-unknown-type" || mode == "call-default-type") {
    return "AST JSON SgFunctionCallExp has no exact semantic value type during "
           "deserialization";
  }
  if (mode == "inline-unknown-type" || mode == "inline-default-type") {
    return "AST JSON SgIntVal has no exact semantic value type during "
           "deserialization";
  }
  if (mode == "typedef-unknown-source-form") {
    return "AST JSON SgTypedefDeclaration has an invalid typedef_type enum "
           "value";
  }
  if (mode == "missing-noexcept-operand") {
    return "missing required edge 'operand_expr'";
  }
  if (mode == "noexcept-nonbool-type") {
    return "AST JSON SgNoexceptOp result type is not exact bool";
  }
  throw std::runtime_error("unknown AST JSON finalization mode: " + mode);
}

bool requiresExactFailure(const std::string &mode) {
  return mode == "cast-unknown-type" || mode == "cast-default-type" ||
         mode == "call-unknown-type" || mode == "call-default-type" ||
         mode == "inline-unknown-type" || mode == "inline-default-type";
}

SgCastExp *contractExplicitCast(SgSourceFile *file) {
  SgFunctionDeclaration *owner =
      findFunction(file, "rex_ast_json_contract_cast");
  SgCastExp *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(owner, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    if (cast == nullptr || cast->get_cast_type() != SgCastExp::e_static_cast) {
      continue;
    }
    if (result != nullptr) {
      throw std::runtime_error(
          "AST JSON finalization fixture has multiple explicit static casts");
    }
    result = cast;
  }
  if (result == nullptr) {
    throw std::runtime_error(
        "AST JSON finalization fixture has no explicit static cast");
  }
  return result;
}

void requireSerializerTypeRejection(const std::string &mode,
                                    SgSourceFile *file) {
  if (mode != "serializer-null-type" && mode != "serializer-unknown-type" &&
      mode != "serializer-default-type" &&
      mode != "serializer-unknown-typedef-source-form") {
    throw std::runtime_error(
        "unknown AST JSON serializer type-rejection mode: " + mode);
  }

  const std::string expected =
      mode == "serializer-unknown-typedef-source-form"
          ? "AST JSON SgTypedefDeclaration has no exact typedef/using source "
            "form during serialization"
          : "AST JSON SgCastExp has no exact semantic value type during "
            "serialization";
  try {
    if (mode == "serializer-null-type") {
      SerializerSemanticTypeProbe *expression =
          new SerializerSemanticTypeProbe(nullptr);
      std::vector<std::string> fields;
      const std::unordered_map<const SgNode *, uint64_t> ids;
      Rose::AstJson::addExpressionType(fields, expression, ids);
    } else if (mode == "serializer-unknown-typedef-source-form") {
      SgTypedefDeclaration *declaration =
          findTypedef(file, "RexAstJsonSourceForm");
      declaration->set_typedef_type(SgTypedefDeclaration::e_unknown);
      constexpr Rose::AstJson::Checkpoint checkpoint =
          Rose::AstJson::Checkpoint::PreOmpConstruction;
      (void)Rose::AstJson::buildJson(file, checkpoint, file);
    } else {
      SgCastExp *expression = contractExplicitCast(file);
      SgType *invalid_type = nullptr;
      if (mode == "serializer-unknown-type") {
        invalid_type = SageBuilder::buildUnknownType();
      } else {
        invalid_type = SgTypeDefault::createType();
      }
      expression->set_type(invalid_type);
      constexpr Rose::AstJson::Checkpoint checkpoint =
          Rose::AstJson::Checkpoint::PreOmpConstruction;
      (void)Rose::AstJson::buildJson(file, checkpoint, file);
    }
  } catch (const std::runtime_error &error) {
    if (error.what() == expected) {
      return;
    }
    throw std::runtime_error("unexpected AST JSON serializer rejection for " +
                             mode + ": " + error.what());
  }
  throw std::runtime_error(
      "malformed semantic expression was accepted by AST JSON serialization "
      "for " +
      mode);
}

void mutate(AstFileRecord &ast, const std::string &mode) {
  if (mode == "missing-parameter-list") {
    removeEdges(functionRecord(ast, "rex_ast_json_contract_function"),
                "parameterList");
    return;
  }
  if (mode == "invalid-function-type") {
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    function.properties.object["function_type"] =
        function.properties.at("return_type");
    return;
  }
  if (mode == "mismatched-return-type") {
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    function.properties.object["return_type"] =
        JsonValue::objectValue({{"present", JsonValue::boolean(true)},
                                {"kind", JsonValue::string("SgTypeFloat")}});
    return;
  }
  if (mode == "missing-declaration-scope") {
    removeEdges(firstAuxiliaryDeclaration(ast), "scope");
    return;
  }
  if (mode == "detached-declaration-scope") {
    NodeRecord *container = nullptr;
    NodeRecord &declaration = firstAuxiliaryDeclaration(ast, &container);
    container->edges.erase(
        std::remove_if(container->edges.begin(), container->edges.end(),
                       [&](const EdgeRecord &edge) {
                         return edge.field == "declarations" &&
                                edge.target == declaration.id;
                       }),
        container->edges.end());
    removeEdges(declaration, "parent");
    removeEdges(declaration, "scope");
    return;
  }
  if (mode == "missing-auxiliary-parent") {
    removeEdges(firstAuxiliaryDeclaration(ast), "parent");
    return;
  }
  if (mode == "wrong-auxiliary-scope") {
    NodeRecord &declaration = firstAuxiliaryDeclaration(ast);
    removeEdges(declaration, "scope");
    declaration.edges.push_back(
        EdgeRecord{"scope", firstRecord(ast, "SgFunctionDefinition").id, 0});
    return;
  }
  if (mode == "duplicate-emission-owner") {
    NodeRecord *container = nullptr;
    NodeRecord &declaration = firstAuxiliaryDeclaration(ast, &container);
    NodeRecord &owner = recordById(ast, singleTarget(*container, "parent"));
    owner.edges.push_back(
        EdgeRecord{"declarations", declaration.id, owner.edges.size()});
    return;
  }
  if (mode == "missing-this-symbol") {
    NodeRecord &expression = firstRecord(ast, "SgThisExp");
    expression.properties.object["class_symbol_declaration"] =
        JsonValue::number("0");
    expression.properties.object["nonreal_symbol_declaration"] =
        JsonValue::number("0");
    return;
  }
  if (mode == "missing-nonreal-symbol") {
    firstRecord(ast, "SgNonrealRefExp")
        .properties.object["symbol_declaration"] = JsonValue::number("0");
    return;
  }
  if (mode == "inconsistent-nonreal-symbol") {
    firstRecord(ast, "SgNonrealRefExp").properties.object["symbol_name"] =
        JsonValue::string("rex_inconsistent_nonreal_symbol");
    return;
  }
  if (mode == "inconsistent-nonreal-variable-identity") {
    NodeRecord &reference = resolvedVariableNonrealReference(ast);
    const uint64_t specialization_id = static_cast<uint64_t>(
        reference.properties.requiredInt("resolved_variable_declaration"));
    NodeRecord &specialization = recordById(ast, specialization_id);
    const uint64_t primary_id =
        singleTarget(specialization, "specializedTemplateDeclaration");
    reference.properties.object["resolved_variable_declaration"] =
        JsonValue::number(std::to_string(primary_id));
    return;
  }
  if (mode == "inconsistent-specialization-family") {
    partialSpecializationFirstDeclaration(ast)
        .properties.object["specialization"] = JsonValue::number(
        std::to_string(SgDeclarationStatement::e_no_specialization));
    return;
  }
  if (mode == "null-template-argument") {
    NodeRecord &reference = resolvedVariableNonrealReference(ast);
    JsonValue &arguments = reference.properties.object["template_arguments"];
    if (arguments.kind != JsonValue::Kind::Array || arguments.array.empty()) {
      throw std::runtime_error(
          "AST JSON finalization fixture has no reference template argument");
    }
    arguments.array.front() = JsonValue::number("0");
    return;
  }
  if (mode == "mismatched-source-order") {
    for (NodeRecord &record : ast.nodes) {
      if (record.properties.kind != JsonValue::Kind::Object) {
        continue;
      }
      auto found =
          record.properties.object.find("translation_unit_source_order");
      if (found == record.properties.object.end() ||
          found->second.kind != JsonValue::Kind::Number) {
        continue;
      }
      const int64_t original = found->second.asInt();
      if (original <= 0 ||
          original >= std::numeric_limits<unsigned int>::max()) {
        throw std::runtime_error(
            "AST JSON finalization fixture found no incrementable source "
            "order");
      }
      found->second = JsonValue::number(std::to_string(original + 1));
      return;
    }
    throw std::runtime_error(
        "AST JSON finalization fixture found no ordered declaration");
  }
  if (mode == "cast-unknown-type" || mode == "cast-default-type") {
    firstRecord(ast, "SgCastExp").properties.object["type"] =
        unresolvedSemanticType(mode == "cast-unknown-type" ? "SgTypeUnknown"
                                                           : "SgTypeDefault");
    return;
  }
  if (mode == "call-unknown-type" || mode == "call-default-type") {
    firstRecord(ast, "SgFunctionCallExp").properties.object["type"] =
        unresolvedSemanticType(mode == "call-unknown-type" ? "SgTypeUnknown"
                                                           : "SgTypeDefault");
    return;
  }
  if (mode == "inline-unknown-type" || mode == "inline-default-type") {
    typeOwnedArrayIndexProperties(ast).object["type"] = unresolvedSemanticType(
        mode == "inline-unknown-type" ? "SgTypeUnknown" : "SgTypeDefault");
    return;
  }
  if (mode == "typedef-unknown-source-form") {
    for (NodeRecord &record : ast.nodes) {
      if (record.kind == "SgTypedefDeclaration" &&
          record.properties.requiredString("name") == "RexAstJsonSourceForm") {
        record.properties.object["typedef_type"] =
            JsonValue::number(std::to_string(SgTypedefDeclaration::e_unknown));
        return;
      }
    }
    throw std::runtime_error(
        "AST JSON finalization fixture has no source-form typedef record");
  }
  if (mode == "missing-noexcept-operand") {
    removeEdges(firstRecord(ast, "SgNoexceptOp"), "operand_expr");
    return;
  }
  if (mode == "noexcept-nonbool-type") {
    JsonValue &type =
        firstRecord(ast, "SgNoexceptOp").properties.object["type"];
    type.object["kind"] = JsonValue::string("SgTypeInt");
    return;
  }
  JsonValue &external = externalPeerRecord(ast);
  if (mode == "missing-external-context") {
    external.object.erase("scope_source");
    return;
  }
  if (mode == "invalid-external-scope") {
    external.object["scope_source"] = JsonValue::string("node");
    external.object["scope"] = JsonValue::number("0");
    return;
  }
  if (mode == "inconsistent-external-kind") {
    external.object["kind"] = JsonValue::string("SgClassDeclaration");
    return;
  }
  throw std::runtime_error("unknown AST JSON finalization mode: " + mode);
}

} // namespace

int main(int argc, char **argv) {
  using namespace Rose::AstJson;

  const std::string mode = takeMode(argc, argv);
  SgProject *project = frontend(argc, argv);
  if (project == nullptr || frontendExitStatus(project) != 0) {
    throw std::runtime_error("AST JSON finalization fixture frontend failed");
  }
  SgSourceFile *file = sourceFile(project);
  if (mode == "serializer-null-type" || mode == "serializer-unknown-type" ||
      mode == "serializer-default-type" ||
      mode == "serializer-unknown-typedef-source-form") {
    requireSerializerTypeRejection(mode, file);
    return 0;
  }
  if (mode.find("external") != std::string::npos) {
    installExternalPeer(project, file);
  }

  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(file, checkpoint, file),
                                       checkpointName(checkpoint));
  if (mode == "positive") {
    SgSourceFile *copy = reconstructSourceFile(ast, file);
    if (copy == nullptr) {
      throw std::runtime_error(
          "valid AST JSON finalization contract reconstructed as null");
    }
    if (findTypedef(copy, "RexAstJsonSourceForm")->get_typedef_type() !=
        SgTypedefDeclaration::e_using) {
      throw std::runtime_error(
          "valid AST JSON did not preserve the exact using source form");
    }
    size_t resolved_variable_references = 0;
    for (SgNode *node : NodeQuery::querySubTree(copy, V_SgNonrealRefExp)) {
      SgNonrealRefExp *reference = isSgNonrealRefExp(node);
      if (reference == nullptr ||
          reference->get_resolved_variable_declaration() == nullptr) {
        continue;
      }
      ++resolved_variable_references;
      SgInitializedName *name =
          SageInterface::requireResolvedVariableTemplateReference(
              reference, "AST JSON positive edge regression");
      if (name->get_name() != "rex_ast_json_variable_template" ||
          reference->get_symbol()
                  ->get_declaration()
                  ->get_templateDeclaration() !=
              reference->get_resolved_variable_declaration()) {
        throw std::runtime_error(
            "valid AST JSON variable-template identity edge was not "
            "reconstructed exactly");
      }
    }
    if (resolved_variable_references != 1) {
      throw std::runtime_error(
          "valid AST JSON did not reconstruct exactly one resolved "
          "variable-template reference");
    }
    return 0;
  }

  mutate(ast, mode);
  const std::string expected = expectedFailure(mode);
  try {
    (void)reconstructSourceFile(ast, file);
  } catch (const std::runtime_error &error) {
    if ((requiresExactFailure(mode) && error.what() == expected) ||
        (!requiresExactFailure(mode) &&
         std::string(error.what()).find(expected) != std::string::npos)) {
      return 0;
    }
    throw;
  }
  throw std::runtime_error("malformed AST JSON finalization contract was "
                           "accepted for " +
                           mode);
}
