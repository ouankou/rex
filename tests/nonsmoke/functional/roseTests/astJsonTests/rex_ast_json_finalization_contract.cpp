#include "ompAstConstruction.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <limits>
#include <memory>
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
                  SageBuilder::buildIntType(), SgUnaryOp::prefix,
                  SgCastExp::e_implicit_cast,
                  SgCastExp::e_semantic_conversion_unclassified,
                  SgCastExp::e_value_category_prvalue, {}, nullptr),
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
            "rex_ast_json_inline_type_owned_array") {
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

void installExternalPeer(SgProject *project, SgSourceFile *file,
                         bool owns_symbol) {
  SgFunctionDeclaration *target =
      findFunction(file, "rex_ast_json_external_contract");
  SgFunctionDeclaration *peer =
      SageInterface::deepCopy<SgFunctionDeclaration>(target);
  if (peer == nullptr || target->get_scope() == nullptr) {
    throw std::runtime_error(
        "AST JSON finalization fixture could not clone an external peer");
  }
  peer->set_parent(project);
  peer->initialize_ast_json_external_provenance(
      SgDeclarationStatement::e_ast_json_external_function,
      file->get_sourceFileNameWithPath());
  peer->set_scope(target->get_scope());
  peer->set_firstNondefiningDeclaration(peer);
  peer->set_definingDeclaration(nullptr);
  auto *launch_bounds = SageBuilder::buildIntVal(256);
  peer->set_cuda_launch_bounds_expression(launch_bounds);
  launch_bounds->set_parent(peer);
  target->set_firstNondefiningDeclaration(peer);
  if (owns_symbol) {
    auto *scope = target->get_scope();
    auto *symbol =
        isSgFunctionSymbol(scope->find_symbol_from_declaration(target));
    ROSE_ASSERT(symbol != nullptr && symbol->get_symbol_basis() == target);
    symbol->set_declaration(peer);
    scope->get_symbol_table()->reindex_symbol_basis(symbol, target);
  }
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
  if (mode == "invalid-variable-inline")
    return "variable_inline_kind enum value";
  if (mode == "non-variable-inline")
    return "has inline variable state on a non-variable declaration";
  if (mode == "cyclic-global-parent")
    return "AST JSON contains a parent cycle at node ";
  if (mode == "missing-parameter-list") {
    return "missing required edge 'parameterList'";
  }
  if (mode == "misowned-parameter-list") {
    return "AST JSON function parameter list has no unique typed owner";
  }
  if (mode == "missing-parameter-scope" || mode == "wrong-parameter-scope") {
    return "AST JSON function parameter has no exact construction-time "
           "semantic scope";
  }
  if (mode == "missing-parameter-declaration" ||
      mode == "wrong-parameter-declaration") {
    return "AST JSON function parameter has no exact declaration owner";
  }
  if (mode == "legacy-external-parameter-scope") {
    return "AST JSON function retains removed external parameter-scope state";
  }
  if (mode == "invalid-external-parameter-scope-owner")
    return "external_function_parameter_scope_owner enum value";
  if (mode == "conflicting-external-parameter-scope-owner")
    return "AST JSON function has no unique external parameter-scope owner";
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
    // Owned edges are checked during construction, before finalization.
    return "AST JSON SgGlobal.declarations does not match its recorded child "
           "owner";
  }
  if (mode == "missing-this-symbol") {
    return "SgThisExp must name exactly one class or nonreal symbol";
  }
  if (mode == "missing-nonreal-symbol") {
    return "AST JSON SgNonrealRefExp requires a positive symbol declaration";
  }
  if (mode == "inconsistent-nonreal-symbol") {
    return "AST JSON SgNonrealRefExp symbol cross-edge is inconsistent";
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
  if (mode == "mismatched-template-argument-expression") {
    return "template argument expression disagrees with its typed edge";
  }
  if (mode == "incompatible-template-argument-type") {
    return "non-type template argument has incompatible value type";
  }
  if (mode == "mismatched-template-argument-name") {
    return "removed initialized-name template argument payload";
  }
  if (mode == "mismatched-template-argument-declaration") {
    return "has conflicting pointer identity";
  }
  if (mode == "mismatched-class-template-arguments") {
    return "SgTemplateInstantiationDecl argument property and edges disagree";
  }
  if (mode == "wrong-class-template-argument-kind") {
    return "SgTemplateInstantiationDecl template argument has the wrong kind";
  }
  if (mode == "missing-class-template-declaration") {
    return "missing required edge 'templateDeclaration'";
  }
  if (mode == "missing-class-definition-declaration") {
    return "missing required edge 'declaration'";
  }
  if (mode == "wrong-class-definition-owner-kind") {
    return "AST JSON class definition has no exact declaration owner";
  }
  if (mode == "wrong-function-definition-owner-kind") {
    return "AST JSON function definition has no exact declaration and "
           "body ownership";
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
  if (mode == "conflicting-external-context")
    return "external function has conflicting construction owners";
  if (mode == "mismatched-source-order") {
    return "declaration translation-unit source order disagrees with its "
           "exact source-position occurrence";
  }
  if (mode == "cast-unknown-type" || mode == "cast-default-type") {
    return "AST JSON SgCastExp requires an exact stored destination type";
  }
  if (mode == "call-unknown-type" || mode == "call-default-type") {
    return "AST JSON call requires an exact non-callable result type";
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
  return mode == "missing-nonreal-symbol" ||
         mode == "inconsistent-nonreal-symbol" || mode == "cast-unknown-type" ||
         mode == "cast-default-type" || mode == "call-unknown-type" ||
         mode == "call-default-type" || mode == "inline-unknown-type" ||
         mode == "inline-default-type";
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
      auto expression = std::make_unique<SerializerSemanticTypeProbe>(nullptr);
      std::vector<std::string> fields;
      const std::unordered_map<const SgNode *, uint64_t> ids;
      Rose::AstJson::addExpressionType(fields, expression.get(), ids);
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
  if (mode == "invalid-variable-inline" || mode == "non-variable-inline") {
    NodeRecord &record = firstRecord(ast, mode == "invalid-variable-inline"
                                              ? "SgVariableDeclaration"
                                              : "SgFunctionDeclaration");
    record.properties.object["variable_inline_kind"] = JsonValue::number("99");
    return;
  }
  if (mode == "cyclic-global-parent") {
    NodeRecord &global = firstRecord(ast, "SgGlobal");
    removeEdges(global, "parent");
    global.edges.push_back({"parent", global.id, 0});
    return;
  }
  if (mode == "missing-parameter-declaration" ||
      mode == "wrong-parameter-declaration") {
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    NodeRecord &list = recordById(ast, singleTarget(function, "parameterList"));
    const auto parameters = Rose::AstJson::edgesFor(list, "args");
    ROSE_ASSERT(parameters.size() == 1);
    NodeRecord &parameter = recordById(ast, parameters.front().target);
    ROSE_ASSERT(singleTarget(parameter, "declptr") == function.id);
    removeEdges(parameter, "declptr");
    if (mode == "wrong-parameter-declaration") {
      parameter.edges.push_back(
          {"declptr", functionRecord(ast, "rex_ast_json_contract_cast").id, 0});
    }
    return;
  }
  if (mode == "missing-parameter-scope" || mode == "wrong-parameter-scope") {
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    NodeRecord &list = recordById(ast, singleTarget(function, "parameterList"));
    const auto parameters = Rose::AstJson::edgesFor(list, "args");
    ROSE_ASSERT(parameters.size() == 1);
    NodeRecord &parameter = recordById(ast, parameters.front().target);
    ROSE_ASSERT(singleTarget(function, "functionParameterScope") != 0);
    ROSE_ASSERT(singleTarget(parameter, "scope") != 0);
    removeEdges(parameter, "scope");
    if (mode == "wrong-parameter-scope") {
      parameter.edges.push_back({"scope", singleTarget(function, "scope"), 0});
    }
    return;
  }
  if (mode == "missing-parameter-list") {
    removeEdges(functionRecord(ast, "rex_ast_json_contract_function"),
                "parameterList");
    return;
  }
  if (mode == "misowned-parameter-list") {
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    NodeRecord &list = recordById(ast, singleTarget(function, "parameterList"));
    for (EdgeRecord &edge : list.edges) {
      if (edge.field == "parent") {
        edge.target = list.id;
        return;
      }
    }
    throw std::runtime_error("function parameter list has no parent edge");
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
    NodeRecord &reference = firstRecord(ast, "SgNonrealRefExp");
    const uint64_t declaration_id =
        reference.properties.requiredInt("symbol_declaration");
    for (const NodeRecord &declaration : ast.nodes) {
      if (declaration.kind == "SgNonrealDecl" &&
          declaration.id != declaration_id) {
        reference.properties.object.at("symbol_declaration") =
            JsonValue::number(std::to_string(declaration.id));
        return;
      }
    }
    throw std::runtime_error(
        "AST JSON finalization fixture has no distinct nonreal declaration");
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
  if (mode == "incompatible-template-argument-type") {
    for (NodeRecord &argument : ast.nodes) {
      if (argument.kind == "SgTemplateArgument" &&
          argument.properties.requiredInt("argument_type") ==
              SgTemplateArgument::nontype_argument) {
        argument.properties.object.at("type") = Rose::AstJson::parseJson(
            Rose::AstJson::rawTypeJson(SageBuilder::buildFloatType(), {}));
        return;
      }
    }
    throw std::runtime_error("fixture has no non-type template argument");
  }
  if (mode == "mismatched-template-argument-expression" ||
      mode == "mismatched-template-argument-declaration" ||
      mode == "mismatched-template-argument-name") {
    NodeRecord &argument = firstRecord(ast, "SgTemplateArgument");
    const JsonValue wrong_id = JsonValue::number(std::to_string(argument.id));
    if (mode == "mismatched-template-argument-expression") {
      argument.properties.object["expression"].object["node"] = wrong_id;
    } else {
      const char *field = mode == "mismatched-template-argument-name"
                              ? "initialized_name"
                              : "template_declaration";
      argument.properties.object[field] = wrong_id;
    }
    return;
  }
  if (mode == "mismatched-class-template-arguments") {
    for (NodeRecord &record : ast.nodes) {
      if (record.kind == "SgTemplateInstantiationDecl" &&
          !Rose::AstJson::edgesFor(record, "semanticTemplateArguments")
               .empty()) {
        record.properties.object["semantic_template_arguments"] =
            Rose::AstJson::parseJson("[]");
        return;
      }
    }
    throw std::runtime_error(
        "AST JSON finalization fixture has no class template arguments");
  }
  if (mode == "wrong-class-template-argument-kind") {
    for (NodeRecord &record : ast.nodes) {
      if (record.kind != "SgTemplateInstantiationDecl") {
        continue;
      }
      for (EdgeRecord &edge : record.edges) {
        if (edge.field == "semanticTemplateArguments") {
          edge.target = record.id;
          record.properties.object["semantic_template_arguments"]
              .array.front() = JsonValue::number(std::to_string(record.id));
          return;
        }
      }
    }
    throw std::runtime_error(
        "AST JSON finalization fixture has no class template arguments");
  }
  if (mode == "missing-class-template-declaration") {
    NodeRecord &record = firstRecord(ast, "SgTemplateInstantiationDecl");
    record.edges.erase(std::remove_if(record.edges.begin(), record.edges.end(),
                                      [](const EdgeRecord &edge) {
                                        return edge.field ==
                                               "templateDeclaration";
                                      }),
                       record.edges.end());
    return;
  }
  if (mode == "missing-class-definition-declaration") {
    removeEdges(firstRecord(ast, "SgClassDefinition"), "declaration");
    return;
  }
  if (mode == "wrong-class-definition-owner-kind") {
    NodeRecord &definition = firstRecord(ast, "SgClassDefinition");
    recordById(ast, singleTarget(definition, "parent")).kind =
        "SgFunctionDeclaration";
    return;
  }
  if (mode == "wrong-function-definition-owner-kind") {
    NodeRecord &definition = firstRecord(ast, "SgFunctionDefinition");
    recordById(ast, singleTarget(definition, "parent")).kind =
        "SgClassDeclaration";
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
      JsonValue &file_ids = ast.metadata.object.at("file_id_map");
      ROSE_ASSERT(file_ids.kind == JsonValue::Kind::Array);
      const std::string filename = "rex_ast_json_rejected_checkpoint_only.hpp";
      ROSE_ASSERT(Sg_File_Info::get_nametofileid_map().count(filename) == 0);
      ROSE_ASSERT(std::none_of(file_ids.array.begin(), file_ids.array.end(),
                               [&](const JsonValue &entry) {
                                 return entry.requiredString("filename") ==
                                        filename;
                               }));
      file_ids.array.push_back(JsonValue::objectValue(
          {{"id", JsonValue::number(std::to_string(file_ids.array.size()))},
           {"filename", JsonValue::string(filename)}}));
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
  if (mode == "legacy-external-parameter-scope") {
    auto &properties =
        functionRecord(ast, "rex_ast_json_external_contract").properties;
    properties.object["external_function_parameter_scope"] =
        JsonValue::objectValue({{"present", JsonValue::boolean(true)}});
    properties.object["external_function_parameter_scope_source"] =
        JsonValue::string("firstNondefiningDeclaration");
    return;
  }
  if (mode == "invalid-external-parameter-scope-owner" ||
      mode == "conflicting-external-parameter-scope-owner") {
    functionRecord(ast, "rex_ast_json_external_contract")
        .properties.object["external_function_parameter_scope_owner"] =
        JsonValue::number(
            mode == "invalid-external-parameter-scope-owner" ? "42" : "1");
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
  if (mode == "conflicting-external-context") {
    JsonValue conflicting = externalPeerRecord(ast);
    conflicting.object["parent_source"] =
        JsonValue::string("redeclaration_owner_parent");
    functionRecord(ast, "rex_ast_json_external_contract")
        .properties.object["external_defining_declaration"] =
        std::move(conflicting);
    return;
  }
  if (mode == "inconsistent-external-kind") {
    external.object["kind"] = JsonValue::string("SgClassDeclaration");
    return;
  }
  throw std::runtime_error("unknown AST JSON finalization mode: " + mode);
}

void requireProducerRegistrationContract(SgSourceFile *file,
                                         bool invalid_owner) {
  using namespace Rose::AstJson;
  auto *function = findFunction(file, "rex_ast_json_contract_function");
  ROSE_ASSERT(function->get_definition() != nullptr);
  auto *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr &&
              body->get_parent() == function->get_definition());
  const auto &parameters = function->get_parameterList()->get_args();
  ROSE_ASSERT(parameters.size() == 1);
  auto *value = parameters.front();
  auto *symbol = isSgVariableSymbol(value->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == value);
  auto *later_scope = SageBuilder::buildBasicBlock();
  const std::vector<SgPragmaDeclaration *> pragmas{
      SageBuilder::buildPragmaDeclaration("acc wait(value + 1)", body),
      SageBuilder::buildPragmaDeclaration("acc wait(value + 1)", later_scope)};
  SageInterface::appendStatement(pragmas[0], body);
  SageInterface::appendStatement(later_scope, body);
  SageInterface::appendStatement(pragmas[1], later_scope);
  for (auto *pragma : pragmas) {
    OpenACCCxxExactSemanticBindings::BindingSequence expressions;
    expressions.emplace_back(
        OMP_EXPR_PARSE_expression, "value + 1",
        std::vector<OpenACCCxxExactSemanticBindings::Binding>{
            {"value", OpenACCCxxExactSemanticBindings::BindingKind::value,
             value, symbol}},
        std::vector<OmpExactSubexpressionType>{
            {OmpExactSubexpressionKind::add, value->get_type()}});
    OmpSupport::registerOpenACCCxxExactSemanticBindings(
        file, pragma, OpenACCCxxExactSemanticBindings(std::move(expressions)));
  }
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  NodeMap originals;
  AstFileRecord ast =
      parseAstFileJson(buildJson(file, checkpoint, file, nullptr, &originals),
                       checkpointName(checkpoint));
  std::unordered_map<const SgNode *, uint64_t> ids;
  for (const auto &entry : originals)
    ids.emplace(entry.second, entry.first);
  if (!invalid_owner) {
    NodeMap restored;
    auto *copy = reconstructSourceFile(ast, file, &restored);
    std::unordered_map<const SgNode *, uint64_t> restored_ids;
    for (const auto &entry : restored)
      restored_ids.emplace(entry.second, entry.first);
    for (auto *pragma : pragmas) {
      auto *restored_pragma =
          nodeByIdAs<SgPragmaDeclaration>(restored, ids.at(pragma));
      const JsonValue properties =
          parseJson(rawNodeProperties(restored_pragma, restored_ids));
      ROSE_ASSERT(
          properties.at("openmp_producer_semantic_records")
              .exactlyEquals(
                  ast.node(ids.at(pragma))
                      .properties.at("openmp_producer_semantic_records")));
    }
    OmpSupport::discardOpenMPProducerSemanticRecords(copy);
    OmpSupport::discardOpenMPProducerSemanticRecords(file);
    return;
  }
  const uint64_t first_id = ids.at(pragmas[0]);
  ROSE_ASSERT(ast.index_by_id.at(first_id) <
              ast.index_by_id.at(ids.at(pragmas[1])));
  NodeRecord &scope_record = recordById(ast, ids.at(later_scope));
  ROSE_ASSERT(singleTarget(scope_record, "parent") == ids.at(body));
  removeEdges(scope_record, "parent");
  auto &body_edges = recordById(ast, ids.at(body)).edges;
  const size_t original_edges = body_edges.size();
  body_edges.erase(std::remove_if(body_edges.begin(), body_edges.end(),
                                  [&](const EdgeRecord &edge) {
                                    return edge.field == "statements" &&
                                           edge.target == scope_record.id;
                                  }),
                   body_edges.end());
  ROSE_ASSERT(body_edges.size() + 1 == original_edges);
  try {
    (void)reconstructSourceFile(ast, file);
  } catch (const std::runtime_error &error) {
    ROSE_ASSERT(
        std::string(error.what()) ==
        "AST JSON OpenMP producer record has no exact source-file owner");
    for (auto *pragma : pragmas) {
      const JsonValue restored = parseJson(rawNodeProperties(pragma, ids));
      ROSE_ASSERT(
          restored.at("openmp_producer_semantic_records")
              .exactlyEquals(
                  ast.node(ids.at(pragma))
                      .properties.at("openmp_producer_semantic_records")));
    }
    VariantVector variants;
    variants.push_back(V_SgPragmaDeclaration);
    size_t matched = 0;
    for (SgNode *node : NodeQuery::queryMemoryPool(variants)) {
      if (node == pragmas[0] || node == pragmas[1] ||
          preservedJsonNodeId(node) != first_id)
        continue;
      auto *pragma = isSgPragmaDeclaration(node);
      auto *source = SageInterface::getEnclosingSourceFile(pragma);
      ROSE_ASSERT(source != nullptr && source != file);
      ROSE_ASSERT(
          OmpSupport::snapshotOpenMPProducerSemanticRecords(pragma).empty());
      ++matched;
    }
    ROSE_ASSERT(matched <= 1);
    OmpSupport::discardOpenMPProducerSemanticRecords(file);
    return;
  }
  throw std::runtime_error(
      "AST JSON accepted a producer record without its source owner");
}

struct VariableTemplateReferenceCounts {
  size_t dependent = 0;
  size_t resolved = 0;
};

VariableTemplateReferenceCounts
variableTemplateReferenceCounts(SgSourceFile *file) {
  VariableTemplateReferenceCounts counts;
  Rose::AstJson::CollectionBoundaryGuard boundary(file);
  for (auto *node : Rose::AstJson::collectNodes(file)) {
    auto *reference = isSgNonrealRefExp(node);
    if (reference == nullptr)
      continue;
    auto *symbol = reference->get_symbol();
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
    auto *target = isSgTemplateVariableDeclaration(
        symbol->get_declaration()->get_templateDeclaration());
    if (target == nullptr)
      continue;
    auto *primary = isSgTemplateVariableDeclaration(
        target->get_firstNondefiningDeclaration());
    if (target->get_specializedTemplateDeclaration() != nullptr)
      primary = SageInterface::requireCanonicalVariableTemplatePrimary(
          target, "AST JSON positive edge regression");
    ROSE_ASSERT(primary != nullptr &&
                primary->get_firstNondefiningDeclaration() == primary &&
                primary->get_specializedTemplateDeclaration() == nullptr &&
                !primary->get_templateParameters().empty() &&
                primary->get_variables().size() == 1 &&
                primary->get_variables().front()->get_name() ==
                    "rex_ast_json_variable_template");
    if (reference->get_resolved_variable_declaration() != nullptr) {
      auto *name = SageInterface::requireResolvedVariableTemplateReference(
          reference, "AST JSON positive edge regression");
      ROSE_ASSERT(name->get_declptr() == target);
      ++counts.resolved;
    } else {
      auto *type = isSgNonrealType(reference->get_type());
      ROSE_ASSERT(target == primary &&
                  reference->get_resolved_function_declaration() == nullptr &&
                  reference->get_explicit_template_argument_list() &&
                  type != nullptr &&
                  type->get_declaration() == symbol->get_declaration());
      ++counts.dependent;
    }
  }
  return counts;
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
  if (mode == "inline-unknown-type" || mode == "inline-default-type") {
    auto *type = SageBuilder::buildArrayType(SageBuilder::buildIntType(),
                                             SageBuilder::buildIntVal(3));
    auto *declaration = SageBuilder::buildVariableDeclaration(
        "rex_ast_json_inline_type_owned_array", type, nullptr,
        file->get_globalScope());
    SageInterface::appendStatement(declaration, file->get_globalScope());
  }
  if (mode == "invalid-late-producer-owner" ||
      mode == "positive-producer-records") {
    requireProducerRegistrationContract(file,
                                        mode == "invalid-late-producer-owner");
    return 0;
  }
  if (mode == "serializer-null-type" || mode == "serializer-unknown-type" ||
      mode == "serializer-default-type" ||
      mode == "serializer-unknown-typedef-source-form") {
    requireSerializerTypeRejection(mode, file);
    return 0;
  }
  if (mode.find("external") != std::string::npos &&
      mode != "legacy-external-parameter-scope") {
    installExternalPeer(project, file, mode == "positive-external-symbol");
  }

  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  const auto expected_variable_references =
      variableTemplateReferenceCounts(file);
  AstFileRecord ast = parseAstFileJson(buildJson(file, checkpoint, file),
                                       checkpointName(checkpoint));
  auto reject = [&](const AstFileRecord &invalid, const std::string &expected,
                    bool exact = false) {
    const auto nodes_before = numberOfNodes();
    const auto file_ids_before = Sg_File_Info::get_fileidtoname_map();
    const auto file_names_before = Sg_File_Info::get_nametofileid_map();
    const auto type_entries_before =
        *SgNode::get_globalTypeTable()->get_type_table()->get_table();
    const auto function_types_before =
        SgNode::get_globalFunctionTypeTable()->get_types();
    bool rejected = false;
    try {
      (void)reconstructSourceFile(invalid, file);
    } catch (const std::runtime_error &error) {
      const std::string message = error.what();
      ASSERT_require2(exact ? message == expected
                            : message.find(expected) != std::string::npos,
                      "expected rejection: %s; actual rejection: %s",
                      expected.c_str(), message.c_str());
      rejected = true;
    }
    ROSE_ASSERT(rejected && numberOfNodes() == nodes_before);
    ROSE_ASSERT(Sg_File_Info::get_fileidtoname_map() == file_ids_before);
    ROSE_ASSERT(Sg_File_Info::get_nametofileid_map() == file_names_before);
    ROSE_ASSERT(*SgNode::get_globalTypeTable()->get_type_table()->get_table() ==
                type_entries_before);
    const auto &functions = SgNode::get_globalFunctionTypeTable()->get_types();
    ROSE_ASSERT(functions.size() == function_types_before.size());
    for (const auto &entry : function_types_before) {
      const auto found = functions.find(entry.first);
      ROSE_ASSERT(found != functions.end() && found->second == entry.second);
    }
  };
  if (mode == "lambda-constructor-preflight") {
    for (const char *field :
         {"is_mutable", "capture_default", "default_is_by_reference",
          "explicit_return_type", "has_parameter_decl", "is_device"}) {
      auto invalid = ast;
      firstRecord(invalid, "SgLambdaExp").properties.object.erase(field);
      reject(invalid, std::string("AST JSON object is missing key: ") + field);
      invalid = ast;
      firstRecord(invalid, "SgLambdaExp").properties.object.at(field) =
          JsonValue::number("0");
      reject(invalid, "AST JSON value is not a bool");
    }
    for (const char *field : {"capture_by_reference", "implicit",
                              "pack_expansion", "is_init_capture"}) {
      auto invalid = ast;
      firstRecord(invalid, "SgLambdaCapture").properties.object.erase(field);
      reject(invalid, std::string("AST JSON object is missing key: ") + field);
      invalid = ast;
      firstRecord(invalid, "SgLambdaCapture").properties.object.at(field) =
          JsonValue::number("0");
      reject(invalid, "AST JSON value is not a bool");
    }
    for (const char *field :
         {"capture_variable", "capture_initializer", "closure_variable"}) {
      auto invalid = ast;
      auto &capture = firstRecord(invalid, "SgLambdaCapture");
      removeEdges(capture, field);
      capture.edges.push_back({field, ast.root_id, 0});
      reject(invalid, "AST JSON lambda capture operand is not an expression");
    }
    for (int scenario = 0; scenario != 3; ++scenario) {
      auto invalid = ast;
      auto &captures = firstRecord(invalid, "SgLambdaCaptureList");
      const auto children = edgesFor(captures, "capture_list");
      ROSE_ASSERT(!children.empty());
      if (scenario == 0) {
        captures.edges.push_back(children.front());
      } else if (scenario == 1) {
        removeEdges(captures, "capture_list");
        captures.edges.push_back({"capture_list", ast.root_id, 0});
      } else {
        auto &capture = recordById(invalid, children.front().target);
        removeEdges(capture, "parent");
        capture.edges.push_back({"parent", ast.root_id, 0});
      }
      reject(invalid,
             "AST JSON lambda capture list has invalid child ownership");
    }
    for (const char *field :
         {"lambda_capture_list", "lambda_closure_class", "lambda_function"}) {
      auto invalid = ast;
      auto &lambda = firstRecord(invalid, "SgLambdaExp");
      removeEdges(lambda, field);
      reject(invalid, std::string("missing required edge '") + field + "'");
      invalid = ast;
      auto &wrong = firstRecord(invalid, "SgLambdaExp");
      const auto other = singleTarget(
          wrong, std::string(field) == "lambda_function" ? "lambda_capture_list"
                                                         : "lambda_function");
      removeEdges(wrong, field);
      wrong.edges.push_back({field, other, 0});
      reject(invalid, "AST JSON lambda children have invalid kinds");
      invalid = ast;
      auto &child = recordById(
          invalid, singleTarget(firstRecord(invalid, "SgLambdaExp"), field));
      removeEdges(child, "parent");
      child.edges.push_back({"parent", ast.root_id, 0});
      reject(invalid, "AST JSON lambda children have a different owner");
    }
    auto invalid = ast;
    auto &properties = firstRecord(invalid, "SgLambdaExp").properties;
    properties.object.at("capture_default") = JsonValue::boolean(false);
    properties.object.at("default_is_by_reference") = JsonValue::boolean(true);
    reject(invalid,
           "AST JSON lambda has reference default without capture default");
    return 0;
  }
  if (mode == "positive") {
    SgClassDeclaration *forward = nullptr;
    for (SgNode *node :
         NodeQuery::querySubTree(file, V_SgTemplateClassDeclaration)) {
      auto *declaration = isSgClassDeclaration(node);
      if (declaration->get_firstNondefiningDeclaration() == declaration &&
          declaration->get_definingDeclaration() == nullptr) {
        forward = declaration;
        break;
      }
    }
    ROSE_ASSERT(forward != nullptr && forward->get_type() != nullptr);
    const auto nodes_before = numberOfNodes();
    const bool modified = forward->get_isModified();
    SgClassType *type = forward->get_type();
    const NodeMap nodes{{1, forward}};
    const auto class_type = parseJson(
        R"({"present":true,"kind":"SgClassType","fortran_source_syntax":false,"declaration":1})");
    forward->set_type(nullptr);
    bool rejected = false;
    try {
      (void)typeFromJson(class_type, nodes);
    } catch (const std::runtime_error &error) {
      ROSE_ASSERT(
          std::string(error.what()) ==
          "AST JSON class declaration has no constructed canonical class type");
      rejected = true;
    }
    forward->set_type(type);
    forward->set_isModified(modified);
    ROSE_ASSERT(rejected && numberOfNodes() == nodes_before);

    const std::array kinds{"SgInitializedName",     "SgInitializedName",
                           "SgFunctionDeclaration", "SgFunctionDeclaration",
                           "SgFunctionDeclaration", "SgFunctionDeclaration",
                           "SgClassDeclaration",    "SgTypedefDeclaration",
                           "SgFunctionDeclaration", "SgClassDeclaration",
                           "SgTypedefDeclaration",  "SgVariableDeclaration",
                           "SgFunctionDeclaration", "SgFunctionDeclaration",
                           "SgFunctionDeclaration", "SgFunctionDeclaration"};
    for (size_t scenario = 0; scenario < kinds.size(); ++scenario) {
      auto invalid = ast;
      auto &properties = firstRecord(invalid, kinds[scenario]).properties;
      std::string expected;
      if (scenario < 2) {
        const bool name_qualification = scenario == 1;
        properties.object.at(name_qualification
                                 ? "source_name_qualification_present"
                                 : "source_type_qualification_present") =
            parseJson("false");
        properties.object.at(name_qualification
                                 ? "source_name_global_qualification"
                                 : "source_type_global_qualification") =
            parseJson("true");
        expected = name_qualification
                       ? "AST JSON initialized-name source name qualifier "
                         "payload is present without its presence bit"
                       : "AST JSON initialized-name source type qualifier "
                         "payload is present without its presence bit";
      } else if (scenario == 2) {
        properties.object.at("source_return_type_qualification_present") =
            parseJson("false");
        properties.object.at("source_return_type_global_qualification") =
            parseJson("true");
        expected = "AST JSON function return-type source qualifier payload is "
                   "present without its presence bit";
      } else if (scenario == 3) {
        properties.object.at("source_return_type_qualification_present") =
            parseJson("true");
        properties.object.at("source_return_type_elaboration_kind") = parseJson(
            std::to_string(SgFunctionDeclaration::
                               e_source_return_type_elaboration_unspecified));
        expected = "AST JSON source function return type has no exact "
                   "elaboration kind";
      } else if (scenario == 4) {
        properties.object.at("source_return_type_elaboration_kind") =
            parseJson(std::to_string(
                SgFunctionDeclaration::e_source_return_type_elaboration_class));
        properties.object.at("type_elaboration_required_for_return_type") =
            parseJson("false");
        expected = "AST JSON exact function return-type elaboration disagrees "
                   "with its typed boolean projection";
      } else if (scenario < 8) {
        properties.object.at("source_name_qualification_present") =
            parseJson("false");
        properties.object.at("source_name_global_qualification") =
            parseJson("true");
        expected = "AST JSON declaration source qualifier payload is present "
                   "without its presence bit";
      } else if (scenario < 12) {
        properties.object.at("name_qualification_length") = parseJson("-1");
        expected =
            "AST JSON name_qualification_length is outside its int range";
      } else if (scenario == 12) {
        properties.object.at("name_qualification_length_for_return_type") =
            parseJson("-1");
        expected = "AST JSON name_qualification_length_for_return_type is "
                   "outside its int range";
      } else if (scenario < 15) {
        properties.object.at(
            scenario == 13 ? "global_qualification_required_for_return_type"
                           : "requires_name_qualification_on_return_type") =
            parseJson("0");
        expected = "AST JSON value is not a bool";
      } else {
        const bool syntax_present =
            properties.at("function_type_syntax").requiredBool("present");
        properties.object.at("type_syntax_is_available") =
            parseJson(syntax_present ? "false" : "true");
        expected = "AST JSON SgFunctionDeclaration " +
                   properties.requiredString("name") +
                   " has inconsistent function type syntax state";
      }
      reject(invalid, expected, true);
    }
  }
  if (mode == "positive" || mode == "positive-external" ||
      mode == "positive-external-symbol") {
    ROSE_ASSERT(expected_variable_references.dependent != 0 &&
                expected_variable_references.resolved != 0);
    if (mode == "positive") {
      for (const char *field : {"attributes", "preprocessing"}) {
        auto invalid = ast;
        auto &record = firstRecord(invalid, "SgCastExp");
        record.properties.object.at(field) = JsonValue::number("0");
        if (std::string(field) == "preprocessing")
          record.preprocessing = record.properties.at(field);
        const auto sequence = SgNode::get_globalAstModificationSequence();
        reject(invalid, field);
        ROSE_ASSERT(SgNode::get_globalAstModificationSequence() == sequence);
      }
      for (const char *field :
           {"explicitly_specified", "is_array_bound_unknown_type",
            "type_elaboration_required", "source_type_qualification_present",
            "source_type_global_qualification", "is_pack_element",
            "source_type_qualification_tokens"}) {
        auto invalid = ast;
        firstRecord(invalid, "SgTemplateArgument").properties.object.at(field) =
            JsonValue::number("0");
        const auto sequence = SgNode::get_globalAstModificationSequence();
        reject(invalid, std::string(field) == "source_type_qualification_tokens"
                            ? field
                            : "not a bool");
        ROSE_ASSERT(SgNode::get_globalAstModificationSequence() == sequence);
      }
      for (int scenario = 0; scenario < 6; ++scenario) {
        auto invalid = ast;
        auto &entries = firstRecord(invalid, "SgGlobal")
                            .properties.object.at("symbol_table")
                            .array;
        ROSE_ASSERT(!entries.empty());
        auto selected = entries.begin();
        if (scenario >= 4)
          selected = std::find_if(
              entries.begin(), entries.end(), [](const JsonValue &candidate) {
                return candidate.requiredBool("lookup_preferred");
              });
        ROSE_ASSERT(selected != entries.end());
        auto &entry = *selected;
        std::string expected;
        if (scenario == 0) {
          entry.object.erase("lookup_preferred");
          expected = "AST JSON object is missing key: lookup_preferred";
        } else if (scenario == 1) {
          entry.object.at("lookup_preferred") = JsonValue::number("0");
          expected = "AST JSON value is not a bool";
        } else if (scenario == 2) {
          entry.object.erase("entry_name");
          expected = "AST JSON object is missing key: entry_name";
        } else if (scenario == 3) {
          entry.object.at("symbol").object.at("symbol_kind") =
              JsonValue::string(entry.requiredString("symbol_kind") ==
                                        "SgVariableSymbol"
                                    ? "SgFunctionSymbol"
                                    : "SgVariableSymbol");
          expected =
              "AST JSON symbol table entry kind disagrees with its symbol";
        } else {
          expected = "AST JSON symbol preference group " +
                     entry.requiredString("entry_name") + "/" +
                     std::to_string(symbolLookupPreferenceCategory(
                         symbolVariantFromJsonKind(
                             entry.requiredString("symbol_kind")))) +
                     " does not identify one exact preferred entry";
          if (scenario == 4)
            entry.object.at("lookup_preferred") = JsonValue::boolean(false);
          else
            entries.push_back(entry);
        }
        reject(invalid, expected, true);
      }
      {
        auto invalid = ast;
        firstRecord(invalid, "SgIntVal").properties.object["symbol_table"] =
            firstRecord(invalid, "SgBasicBlock").properties.at("symbol_table");
        reject(invalid, "AST JSON symbol table owner is not a scope: SgIntVal",
               true);
      }
      {
        auto invalid = ast;
        firstRecord(invalid, "SgBasicBlock")
            .properties.object.at("symbol_table") = JsonValue::null();
        reject(invalid,
               "AST JSON symbol_table field is not an array for "
               "SgBasicBlock",
               true);
      }
      for (const char *kind : {"SgFunctionDeclaration", "SgSourceFile",
                               "SgFunctionParameterScope"}) {
        for (const char *field :
             {"is_modified", "contains_transformation",
              "contains_transformation_to_surrounding_whitespace",
              "source_range_ends_in_macro_expansion",
              "source_range_is_macro_expansion_fragment"}) {
          auto invalid = ast;
          firstRecord(invalid, kind).flags.object.erase(field);
          reject(invalid,
                 std::string("AST JSON object is missing key: ") + field, true);
        }
      }
      for (const auto &[field, context] :
           {std::pair{"nonreal_decl_scope", "nonreal declaration scope"},
            std::pair{"function_declarator_scope",
                      "function declarator scope"}}) {
        auto invalid = ast;
        auto &function =
            functionRecord(invalid, "rex_ast_json_contract_function");
        removeEdges(function, field);
        function.edges.push_back({field, invalid.root_id, 0});
        reject(invalid,
               std::string("AST JSON ") + context +
                   " has invalid typed endpoints",
               true);
      }
      {
        auto invalid = ast;
        auto &block = firstRecord(invalid, "SgBasicBlock");
        const uint64_t global = firstRecord(invalid, "SgGlobal").id;
        block.edges.push_back(
            {"statement_expression_semantic_scope", global, 0});
        block.edges.push_back({"implied_do_semantic_scope", global, 0});
        reject(invalid,
               "AST JSON SgBasicBlock has multiple typed expression "
               "semantic roles",
               true);
      }
      for (const char *kind :
           {"SgCastExp", "SgFunctionParameterList", "SgCtorInitializerList"}) {
        auto invalid = ast;
        firstRecord(invalid, kind).location.object.erase("start");
        reject(invalid, "AST JSON object is missing key: start", true);
      }
      {
        auto invalid = ast;
        auto &start =
            firstRecord(invalid, "SgCastExp").location.object.at("start");
        start.object.at("raw_line") = JsonValue::number("2147483648");
        reject(invalid, "AST JSON file info raw_line is out of range", true);
      }
      {
        auto invalid = ast;
        auto &start =
            firstRecord(invalid, "SgCastExp").location.object.at("start");
        start.object.at("raw_column") = JsonValue::number("-1");
        reject(invalid, "AST JSON file info raw_column is out of range", true);
      }
      {
        auto invalid = ast;
        firstRecord(invalid, "SgSourceFile").location.object.at("end") =
            firstRecord(invalid, "SgCastExp").location.at("start");
        reject(invalid,
               "AST JSON SgSourceFile file has no endOfConstruct position",
               true);
      }
    }
    std::reverse(ast.nodes.begin(), ast.nodes.end());
    for (size_t index = 0; index < ast.nodes.size(); ++index)
      ast.index_by_id.at(ast.nodes[index].id) = index;
    const auto globals_before = SgGlobal::numberOfNodes();
    const auto serialized_globals = std::count_if(
        ast.nodes.begin(), ast.nodes.end(),
        [](const NodeRecord &record) { return record.kind == "SgGlobal"; });
    NodeMap restored_nodes;
    SgSourceFile *copy = reconstructSourceFile(ast, file, &restored_nodes);
    if (copy == nullptr) {
      throw std::runtime_error(
          "valid AST JSON finalization contract reconstructed as null");
    }
    if (SgGlobal::numberOfNodes() != globals_before + serialized_globals)
      throw std::runtime_error(
          "AST JSON reconstruction retained an unrecorded semantic global");
    const auto nodes_before_validation = numberOfNodes();
    const auto sequence_before_validation =
        SgNode::get_globalAstModificationSequence();
    validateReconstructedTypesAndSymbols(ast, restored_nodes);
    ROSE_ASSERT(numberOfNodes() == nodes_before_validation);
    ROSE_ASSERT(SgNode::get_globalAstModificationSequence() ==
                sequence_before_validation);
    for (const NodeRecord &record : ast.nodes) {
      SgNode *node = restored_nodes.at(record.id);
      const std::string flags = rawNodeFlagsJson(node);
      const bool flags_match = parseJson(flags).exactlyEquals(record.flags);
      if (!flags_match)
        std::cerr << "AST JSON flag mismatch for " << record.kind
                  << " id=" << record.id << ": " << flags << '\n';
      ROSE_ASSERT(flags_match);
      if (isJsonScalarLiteralKind(record.kind) ||
          isSgFunctionParameterScope(node) != nullptr ||
          isSgDeclarationStatement(node) != nullptr ||
          isSgInitializedName(node) != nullptr ||
          isSgTemplateParameter(node) != nullptr)
        ROSE_ASSERT(
            parseJson(rawLocationJson(node)).exactlyEquals(record.location));
    }
    NodeRecord &function =
        functionRecord(ast, "rex_ast_json_contract_function");
    auto *restored_function =
        isSgFunctionDeclaration(restored_nodes.at(function.id));
    auto *restored_parameters = isSgFunctionParameterList(
        restored_nodes.at(singleTarget(function, "parameterList")));
    if (restored_function == nullptr || restored_parameters == nullptr ||
        restored_function->get_parameterList() != restored_parameters ||
        restored_parameters->get_parent() != restored_function) {
      throw std::runtime_error(
          "valid AST JSON function lost its constructor-owned parameter list");
    }
    if (mode == "positive-external" || mode == "positive-external-symbol") {
      auto *target = findFunction(copy, "rex_ast_json_external_contract");
      auto *peer =
          isSgFunctionDeclaration(target->get_firstNondefiningDeclaration());
      ROSE_ASSERT(peer != nullptr && peer != target &&
                  peer->get_scope() == target->get_scope());
      ROSE_ASSERT(SageInterface::isEquivalentFunctionType(target->get_type(),
                                                          peer->get_type()));
      auto *launch_bounds =
          isSgIntVal(peer->get_cuda_launch_bounds_expression());
      ROSE_ASSERT(launch_bounds != nullptr &&
                  launch_bounds->get_value() == 256 &&
                  launch_bounds->get_parent() == peer);
      if (mode == "positive-external-symbol") {
        auto *symbol = peer->get_scope()->find_symbol_from_declaration(peer);
        ROSE_ASSERT(symbol != nullptr && symbol->get_symbol_basis() == peer &&
                    symbol->get_parent() ==
                        peer->get_scope()->get_symbol_table());
      }
      for (auto *parameter : peer->get_parameterList()->get_args())
        ROSE_ASSERT(parameter->get_declptr() == peer &&
                    parameter->get_parent() == peer->get_parameterList() &&
                    parameter->get_scope() ==
                        peer->get_functionParameterScope());
    }
    if (findTypedef(copy, "RexAstJsonSourceForm")->get_typedef_type() !=
        SgTypedefDeclaration::e_using) {
      throw std::runtime_error(
          "valid AST JSON did not preserve the exact using source form");
    }
    const auto actual_variable_references =
        variableTemplateReferenceCounts(copy);
    if (actual_variable_references.dependent !=
            expected_variable_references.dependent ||
        actual_variable_references.resolved !=
            expected_variable_references.resolved) {
      throw std::runtime_error(
          "valid AST JSON changed dependent or resolved variable-template "
          "reference counts");
    }
    return 0;
  }

  const JsonValue original_file_ids = ast.metadata.at("file_id_map");
  mutate(ast, mode);
  if (mode == "mismatched-source-order")
    ROSE_ASSERT(
        !ast.metadata.at("file_id_map").exactlyEquals(original_file_ids));
  const std::string expected = expectedFailure(mode);
  const auto file_ids_before = Sg_File_Info::get_fileidtoname_map();
  const auto file_names_before = Sg_File_Info::get_nametofileid_map();
  const auto node_counts = [] {
    return std::array{numberOfNodes(),
                      SgGlobal::numberOfNodes(),
                      SgSourceFile::numberOfNodes(),
                      SgFunctionDeclaration::numberOfNodes(),
                      SgNonrealRefExp::numberOfNodes(),
                      SgNonrealSymbol::numberOfNodes(),
                      Sg_File_Info::numberOfNodes()};
  };
  const auto nodes_before = node_counts();
  const auto type_entries_before =
      *SgNode::get_globalTypeTable()->get_type_table()->get_table();
  const auto function_types_before =
      SgNode::get_globalFunctionTypeTable()->get_types();
  try {
    (void)reconstructSourceFile(ast, file);
  } catch (const std::runtime_error &error) {
    if ((requiresExactFailure(mode) && error.what() == expected) ||
        (!requiresExactFailure(mode) &&
         std::string(error.what()).find(expected) != std::string::npos)) {
      const bool rejects_before_construction =
          mode == "invalid-variable-inline" || mode == "non-variable-inline" ||
          mode == "cyclic-global-parent" ||
          mode == "conflicting-external-context" ||
          mode == "mismatched-return-type" || mode == "invalid-function-type" ||
          mode == "inconsistent-nonreal-variable-identity" ||
          mode == "misowned-parameter-list" ||
          mode == "missing-auxiliary-parent" ||
          mode == "mismatched-source-order" ||
          mode == "typedef-unknown-source-form";
      if (rejects_before_construction) {
        ROSE_ASSERT(Sg_File_Info::get_fileidtoname_map() == file_ids_before);
        ROSE_ASSERT(Sg_File_Info::get_nametofileid_map() == file_names_before);
        ROSE_ASSERT(node_counts() == nodes_before);
        ROSE_ASSERT(
            *SgNode::get_globalTypeTable()->get_type_table()->get_table() ==
            type_entries_before);
        const auto &function_types =
            SgNode::get_globalFunctionTypeTable()->get_types();
        ROSE_ASSERT(function_types.size() == function_types_before.size());
        for (const auto &entry : function_types_before) {
          const auto restored = function_types.find(entry.first);
          ROSE_ASSERT(restored != function_types.end() &&
                      restored->second == entry.second);
        }
      }
      return 0;
    }
    throw;
  }
  throw std::runtime_error("malformed AST JSON finalization contract was "
                           "accepted for " +
                           mode);
}
