#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "FortranIntegerSelector.h"
#include "FortranProcedureBinding.h"
#include "roseInternal.h"
#include "sageAstJsonPrivate.h"
#include <functional>
#include <type_traits>
namespace Rose {
namespace AstJson {

std::optional<SgInitializedName::FortranProcedureComponentBinding>
fortranProcedureComponentBindingFromJson(const JsonValue &properties) {
  const JsonValue &value = properties.at("fortran_procedure_component_binding");
  if (value.kind == JsonValue::Kind::Null)
    return std::nullopt;
  using Kind = SgInitializedName::FortranProcedurePassKind;
  const Kind kind =
      requiredEnum<Kind>(value, "kind", "Fortran procedure component",
                         {Kind::implicit_pass, Kind::explicit_pass,
                          Kind::named_pass, Kind::no_pass});
  const auto index = requiredOptionalUnsignedInteger(
      value, "passed_object_index", "Fortran procedure component");
  const SgName name = value.requiredName("written_dummy_name");
  if ((kind == Kind::no_pass && (index || !name.is_null())) ||
      ((kind == Kind::implicit_pass || kind == Kind::explicit_pass) &&
       (index != 0 || !name.is_null())) ||
      (kind == Kind::named_pass && (!index || name.is_null())))
    throw std::runtime_error("AST JSON Fortran procedure component has an "
                             "inconsistent PASS role");
  return SgInitializedName::FortranProcedureComponentBinding{kind, index, name};
}

void requireFortranProcedureComponentBinding(
    const std::optional<SgInitializedName::FortranProcedureComponentBinding>
        &binding,
    SgType *type, SgScopeStatement *scope) {
  SgClassDefinition *owner = isSgClassDefinition(scope);
  const bool derived =
      owner != nullptr &&
      isSgDerivedTypeStatement(owner->get_declaration()) != nullptr;
  SgPointerType *pointer = isSgPointerType(type);
  const bool component = derived && pointer != nullptr &&
                         isSgFunctionType(pointer->get_base_type()) != nullptr;
  if (binding.has_value() != component ||
      (binding && !binding->isValidForType(type)))
    throw std::runtime_error("AST JSON Fortran procedure component has no "
                             "valid typed PASS binding");
}

std::optional<SgTemplateParameter::canonical_source_identity>
canonicalSourceIdentityFromJson(const JsonValue &serialized,
                                const char *owner) {
  const JsonValue &identity = serialized.at("canonical_source_identity");
  if (identity.kind == JsonValue::Kind::Null)
    return std::nullopt;
  if (identity.kind != JsonValue::Kind::Object)
    throw std::runtime_error(std::string("AST JSON ") + owner +
                             " canonical_source_identity is neither an "
                             "object nor null");

  const int64_t expansion_file_id = identity.requiredInt("expansion_file_id");
  const int64_t expansion_file_offset =
      identity.requiredInt("expansion_file_offset");
  const int64_t spelling_file_id = identity.requiredInt("spelling_file_id");
  const int64_t spelling_file_offset =
      identity.requiredInt("spelling_file_offset");
  if (expansion_file_id < 0 ||
      expansion_file_id > std::numeric_limits<int>::max() ||
      spelling_file_id < 0 ||
      spelling_file_id > std::numeric_limits<int>::max() ||
      expansion_file_offset < 0 ||
      static_cast<uint64_t>(expansion_file_offset) >
          std::numeric_limits<unsigned int>::max() ||
      spelling_file_offset < 0 ||
      static_cast<uint64_t>(spelling_file_offset) >
          std::numeric_limits<unsigned int>::max()) {
    throw std::runtime_error(std::string("AST JSON ") + owner +
                             " canonical_source_identity is out of range");
  }
  filenameForFileId(static_cast<int>(expansion_file_id),
                    std::string(owner) + " expansion identity");
  filenameForFileId(static_cast<int>(spelling_file_id),
                    std::string(owner) + " spelling identity");
  return SgTemplateParameter::canonical_source_identity{
      static_cast<int>(expansion_file_id),
      static_cast<unsigned int>(expansion_file_offset),
      static_cast<int>(spelling_file_id),
      static_cast<unsigned int>(spelling_file_offset)};
}

LiteralTokenFields literalTokenFieldsFromJson(const JsonValue &properties) {
  const auto kind = requiredEnum<SgCxxLiteralToken::token_kind_enum>(
      properties, "token_kind", "SgCxxLiteralToken",
      {SgCxxLiteralToken::e_numeric_token, SgCxxLiteralToken::e_character_token,
       SgCxxLiteralToken::e_string_token});
  auto spelling = properties.requiredString("spelling");
  if (spelling.empty())
    throw std::runtime_error("AST JSON SgCxxLiteralToken has empty spelling");
  return {kind, std::move(spelling)};
}

namespace {

SgCxxLiteralTokenList *typeOwnedLiteralTokensFromJson(const JsonValue &json) {
  SgNode::SourceStateConstruction construction;
  auto recordFor = [](const JsonValue &value, VariantT expected_variant) {
    NodeRecord record;
    record.kind = value.requiredString("kind");
    if (record.kind != getVariantName(expected_variant))
      throw std::runtime_error(
          "AST JSON type-owned UDL has an invalid lexical node kind");
    record.flags = value.at("flags");
    record.location = value.at("location");
    record.properties = value.at("properties");
    record.preprocessing = record.properties.at("preprocessing");
    return record;
  };
  auto record = recordFor(json, V_SgCxxLiteralTokenList);
  const JsonValue &tokens = json.at("tokens");
  if (tokens.kind != JsonValue::Kind::Array || tokens.array.empty())
    throw std::runtime_error(
        "AST JSON type-owned UDL requires a nonempty token array");
  struct PreparedLiteral {
    NodeConstructionFlags flags;
    SourcePositionFields positions;
    PreparedAstAttributes attributes;
    PreparedPreprocessingInfo preprocessing;
    std::optional<LiteralTokenFields> token;
  };
  auto prepare = [](NodeRecord record, bool token) {
    const auto flags = validateConstructionFlags(record);
    auto positions = validateSourcePositionRecord(record);
    auto attributes = prepareAstAttributes(record);
    auto preprocessing = preparePreprocessingInfo(record);
    std::optional<LiteralTokenFields> fields;
    if (token)
      fields = literalTokenFieldsFromJson(record.properties);
    return PreparedLiteral{flags, std::move(positions), std::move(attributes),
                           std::move(preprocessing), std::move(fields)};
  };
  auto prepared_list = prepare(std::move(record), false);
  std::vector<PreparedLiteral> prepared;
  prepared.reserve(tokens.array.size());
  for (const JsonValue &entry : tokens.array) {
    prepared.push_back(prepare(recordFor(entry, V_SgCxxLiteralToken), true));
  }
  auto delete_tree = [](SgCxxLiteralTokenList *node) {
    SageInterface::deleteAST(node);
  };
  std::unique_ptr<SgCxxLiteralTokenList, decltype(delete_tree)> list(
      new SgCxxLiteralTokenList, delete_tree);
  const auto initialize = [&](SgNode *node, PreparedLiteral &input) {
    input.flags.initialize(node);
    installSourcePositions(node, constructSourcePositions(input.positions),
                           node->sage_class_name());
    attachPreprocessingInfo(node, std::move(input.preprocessing));
    attachAstAttributes(node, std::move(input.attributes));
  };
  initialize(list.get(), prepared_list);
  for (auto &input : prepared) {
    ROSE_ASSERT(input.token.has_value());
    std::unique_ptr<SgCxxLiteralToken> token(
        new SgCxxLiteralToken(input.token->kind, input.token->spelling));
    initialize(token.get(), input);
    token->set_parent(list.get());
    list->get_tokens().push_back(token.get());
    token.release();
  }
  return list.release();
}

unsigned int memberFunctionSpecifierFromJson(const JsonValue &json,
                                             bool hasClassType) {
  const int64_t raw = json.requiredInt("mfunc_specifier");
  constexpr unsigned int refMask =
      SgMemberFunctionType::e_ref_qualifier_lvalue |
      SgMemberFunctionType::e_ref_qualifier_rvalue;
  constexpr unsigned int allowed = SgMemberFunctionType::e_const |
                                   SgMemberFunctionType::e_volatile |
                                   SgMemberFunctionType::e_restrict | refMask;
  if (raw < 0 || static_cast<uint64_t>(raw) > allowed ||
      (static_cast<unsigned int>(raw) & refMask) == refMask ||
      (!hasClassType && raw == 0)) {
    throw std::runtime_error(
        "AST JSON member function type has invalid qualifiers or no bound "
        "class for an unqualified signature");
  }
  return static_cast<unsigned int>(raw);
}

struct FunctionArgumentMetadata {
  bool pack;
  bool qualification_present;
  bool global;
  SgStringList tokens;
  bool elaborated;
};

struct FunctionExceptionFields {
  SgFunctionType::exception_specification_kind_enum kind;
  const JsonValue &expression;
  const JsonValue &types;
  uint64_t source;
  uint64_t pattern;
  bool relative_source;
  bool written;
};

FunctionExceptionFields functionExceptionFieldsFromJson(const JsonValue &json) {
  const int64_t kind = json.requiredInt("kind");
  if (kind < SgFunctionType::e_exception_none ||
      kind > SgFunctionType::e_exception_uninstantiated)
    throw std::runtime_error("AST JSON function exception kind is invalid");
  FunctionExceptionFields fields{
      static_cast<SgFunctionType::exception_specification_kind_enum>(kind),
      json.at("expression"),
      json.at("types"),
      json.requiredNonnegativeNodeId("source"),
      json.requiredNonnegativeNodeId("pattern"),
      json.requiredBool("source_is_self"),
      json.requiredBool("is_written")};
  const bool computed =
      kind == SgFunctionType::e_exception_noexcept_dependent ||
      kind == SgFunctionType::e_exception_noexcept_false ||
      kind == SgFunctionType::e_exception_noexcept_true;
  const bool delayed = kind == SgFunctionType::e_exception_unevaluated ||
                       kind == SgFunctionType::e_exception_uninstantiated;
  const bool expression =
      fields.expression.requiredNonnegativeNodeId("node") != 0 ||
      fields.expression.find("owned_kind") != nullptr;
  if (fields.types.kind != JsonValue::Kind::Array || computed != expression ||
      delayed != (fields.source != 0 || fields.relative_source) ||
      (fields.relative_source && fields.source != 0) ||
      (fields.written && kind == SgFunctionType::e_exception_none) ||
      ((kind == SgFunctionType::e_exception_uninstantiated) !=
       (fields.pattern != 0)) ||
      ((kind == SgFunctionType::e_exception_dynamic) !=
       !fields.types.array.empty()))
    throw std::runtime_error(
        "AST JSON function exception operands are contradictory");
  for (const JsonValue &entry : fields.types.array) {
    const JsonValue &type = entry.at("type");
    entry.requiredBool("is_pack_expansion");
    if (!type.requiredBool("present") ||
        type.requiredString("kind") == "SgTypeEllipse" ||
        type.requiredString("kind") == "SgTypeUnknown")
      throw std::runtime_error(
          "AST JSON exception type is absent or unresolved");
  }
  return fields;
}

struct FunctionTypeFields {
  const JsonValue &return_type;
  const JsonValue &arguments;
  const JsonValue *class_type;
  bool has_ellipses;
  bool has_prototype;
  unsigned int specifier;
  bool canonical;
  uint64_t source_argument_list;
  bool trailing_return;
  const JsonValue &semantic_signature;
  FunctionExceptionFields exception;
  std::vector<FunctionArgumentMetadata> positions;
};

FunctionTypeFields functionTypeFieldsFromJson(const JsonValue &json,
                                              bool member) {
  FunctionTypeFields fields{
      json.at("return_type"),
      json.at("arguments"),
      member ? &json.at("class_type") : nullptr,
      json.requiredBool("has_ellipses"),
      json.requiredBool("has_prototype"),
      0,
      json.requiredBool("semantic_canonical"),
      json.requiredNonnegativeNodeId("source_argument_list"),
      json.requiredBool("source_trailing_return"),
      json.at("semantic_signature"),
      functionExceptionFieldsFromJson(json.at("exception_specification")),
      {}};
  const bool semantic_signature =
      fields.semantic_signature.requiredBool("present");
  if (semantic_signature &&
      (fields.canonical || json.requiredBool("fortran_source_syntax") ||
       fields.semantic_signature.requiredBool("has_prototype") !=
           fields.has_prototype ||
       fields.semantic_signature.requiredString("kind") !=
           json.requiredString("kind") ||
       !fields.semantic_signature.requiredBool("semantic_canonical") ||
       fields.semantic_signature.at("semantic_signature")
           .requiredBool("present")))
    throw std::runtime_error(
        "AST JSON function semantic signature is not a canonical peer");
  if (!semantic_signature &&
      (fields.exception.written || fields.exception.relative_source))
    throw std::runtime_error(
        "AST JSON source function type lacks its semantic signature");
  if ((fields.canonical &&
       (fields.source_argument_list != 0 || fields.trailing_return ||
        fields.exception.relative_source || fields.exception.written)) ||
      (fields.trailing_return && fields.source_argument_list == 0) ||
      (fields.source_argument_list != 0 &&
       json.requiredBool("fortran_source_syntax")))
    throw std::runtime_error(
        "AST JSON function type has contradictory prototype ownership");
  if (fields.canonical && json.requiredBool("fortran_source_syntax"))
    throw std::runtime_error(
        "AST JSON source function type claims canonical identity");
  if (!fields.return_type.requiredBool("present"))
    throw std::runtime_error("AST JSON function return type is absent");
  if (member)
    fields.specifier = memberFunctionSpecifierFromJson(
        json, fields.class_type->requiredBool("present"));
  if (fields.arguments.kind != JsonValue::Kind::Array)
    throw std::runtime_error("AST JSON function arguments are not an array");
  bool ellipsis = false;
  for (const JsonValue &argument : fields.arguments.array) {
    const JsonValue &type = argument.at("type");
    if (!type.requiredBool("present"))
      throw std::runtime_error("AST JSON function argument type is absent");
    if (ellipsis)
      throw std::runtime_error("AST JSON function ellipsis is not final");
    ellipsis = type.requiredString("kind") == "SgTypeEllipse";
    FunctionArgumentMetadata metadata{
        argument.requiredBool("is_pack_expansion"),
        argument.requiredBool("source_type_qualification_present"),
        argument.requiredBool("source_type_global_qualification"),
        stringListFromJson(argument.at("source_type_qualification_tokens"),
                           "function argument qualification"),
        argument.requiredBool("source_type_elaboration_required")};
    if ((ellipsis && metadata.pack) ||
        (!metadata.qualification_present &&
         (metadata.global || !metadata.tokens.empty())))
      throw std::runtime_error(
          "AST JSON function argument has contradictory source metadata");
    fields.positions.push_back(std::move(metadata));
  }
  if (ellipsis != fields.has_ellipses)
    throw std::runtime_error(
        "AST JSON function ellipsis flag disagrees with its typed arguments");
  if (!fields.has_prototype &&
      (member || !fields.arguments.array.empty() ||
       json.requiredBool("fortran_source_syntax") || fields.trailing_return ||
       fields.exception.kind != SgFunctionType::e_exception_none))
    throw std::runtime_error(
        "AST JSON unprototyped function has conflicting signature metadata");
  return fields;
}

void installFunctionArguments(
    SgFunctionParameterTypeList *owned, const SgTypePtrList &arguments,
    const std::vector<FunctionArgumentMetadata> &positions) {
  ROSE_ASSERT(owned != nullptr && owned->get_arguments().empty() &&
              arguments.size() == positions.size());
  for (size_t index = 0; index < arguments.size(); ++index) {
    owned->append_argument(arguments[index]);
    SgFunctionTypeArgument *position =
        owned->get_argument_qualification_use_sites().back();
    const auto &metadata = positions[index];
    position->set_is_pack_expansion(metadata.pack);
    position->set_source_type_qualification_present(
        metadata.qualification_present);
    position->set_source_type_global_qualification(metadata.global);
    position->get_source_type_qualification_tokens() = metadata.tokens;
    position->set_source_type_elaboration_required(metadata.elaborated);
  }
}

thread_local bool pointerMemberTypeDeserializationIdentityActive = false;
struct PointerMemberTypeDeserializationIdentity {
  SgPointerMemberType *type = nullptr;
  SgType *base_type = nullptr;
  SgType *class_type = nullptr;
  JsonValue serialized_type;
};
thread_local std::unordered_map<uint64_t,
                                PointerMemberTypeDeserializationIdentity>
    pointerMemberTypeDeserializationIdentities;
thread_local std::unordered_map<SgPointerMemberType *, uint64_t>
    pointerMemberTypeDeserializationReverseIdentities;

thread_local bool functionTypeDeserializationIdentityActive = false;
struct FunctionTypeDeserializationIdentity {
  JsonValue serialized_type;
  SgFunctionType *type = nullptr;
};
thread_local std::unordered_map<
    uint64_t, std::vector<FunctionTypeDeserializationIdentity>>
    functionTypeDeserializationIdentities;
thread_local bool arrayTypeDeserializationIdentityActive = false;
struct ArrayTypeDeserializationIdentity {
  JsonValue serialized_type;
  SgArrayType *type = nullptr;
};
thread_local std::unordered_map<uint64_t, ArrayTypeDeserializationIdentity>
    arrayTypeDeserializationIdentities;
thread_local SemanticArrayConstruction *activeSemanticArrays = nullptr;
thread_local std::unordered_map<SgArrayType *, SemanticArrayConstruction *>
    pendingSemanticArrays;
struct SourceTypeDeserializationIdentity {
  JsonValue serialized_type;
  SgType *type = nullptr;
};
thread_local std::unordered_map<uint64_t, SourceTypeDeserializationIdentity>
    sourceTypeDeserializationIdentities;

thread_local bool templateTypeDeserializationIdentityActive = false;
struct TemplateTypeDeserializationIdentity {
  JsonValue serialized_type;
  SgTemplateType *type = nullptr;
};
thread_local std::unordered_map<
    uint64_t, std::vector<TemplateTypeDeserializationIdentity>>
    templateTypeDeserializationIdentities;

thread_local bool externalClassDeserializationIdentityActive = false;
struct ExternalClassDeserializationIdentity {
  JsonValue serialized_declaration;
  SgClassDeclaration *declaration = nullptr;
};
struct ExternalClassModuleIdentity {
  std::string name;
  std::string source_file;
  SgModuleStatement *declaration = nullptr;
  SgClassDefinition *definition = nullptr;
};
thread_local std::unordered_map<
    uint64_t, std::vector<ExternalClassDeserializationIdentity>>
    externalClassDeserializationIdentities;
thread_local std::vector<ExternalClassModuleIdentity>
    externalClassModuleIdentities;

struct TemplateTypeFields {
  SgName name;
  int position;
  int depth;
  bool packed;
  bool is_pack_substitution;
  std::optional<SgTemplateType::canonical_source_identity> source_identity;
};

TemplateTypeFields templateTypeFieldsFromJson(const JsonValue &json) {
  auto coordinate = [&](const char *field) {
    const int64_t value = json.requiredInt(field);
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
      throw std::runtime_error(std::string("AST JSON SgTemplateType ") + field +
                               " is out of range");
    }
    return static_cast<int>(value);
  };
  TemplateTypeFields fields{
      json.requiredName("name"),
      coordinate("template_parameter_position"),
      coordinate("template_parameter_depth"),
      json.requiredBool("packed"),
      json.requiredBool("is_pack_substitution"),
      canonicalSourceIdentityFromJson(json, "SgTemplateType")};
  if (fields.is_pack_substitution &&
      (fields.packed || fields.position < 0 || fields.depth < 0 ||
       !fields.source_identity.has_value())) {
    throw std::runtime_error(
        "AST JSON template substitution has invalid identity");
  }
  return fields;
}

void hashJsonBytes(uint64_t &hash, const void *storage, size_t size) {
  const unsigned char *bytes = static_cast<const unsigned char *>(storage);
  for (size_t index = 0; index < size; ++index) {
    hash ^= static_cast<uint64_t>(bytes[index]);
    hash *= UINT64_C(1099511628211);
  }
}

void hashJsonText(uint64_t &hash, const std::string &text) {
  const uint64_t size = static_cast<uint64_t>(text.size());
  hashJsonBytes(hash, &size, sizeof(size));
  if (!text.empty()) {
    hashJsonBytes(hash, text.data(), text.size());
  }
}

void hashJsonValue(uint64_t &hash, const JsonValue &value) {
  const unsigned int kind = static_cast<unsigned int>(value.kind);
  hashJsonBytes(hash, &kind, sizeof(kind));
  hashJsonBytes(hash, &value.bool_value, sizeof(value.bool_value));
  hashJsonText(hash, value.text);

  const uint64_t array_size = static_cast<uint64_t>(value.array.size());
  hashJsonBytes(hash, &array_size, sizeof(array_size));
  for (const JsonValue &entry : value.array) {
    hashJsonValue(hash, entry);
  }

  const uint64_t object_size = static_cast<uint64_t>(value.object.size());
  hashJsonBytes(hash, &object_size, sizeof(object_size));
  for (const auto &field : value.object) {
    hashJsonText(hash, field.first);
    hashJsonValue(hash, field.second);
  }
}

uint64_t hashJsonValue(const JsonValue &value) {
  uint64_t hash = UINT64_C(14695981039346656037);
  hashJsonValue(hash, value);
  return hash;
}
} // namespace

uint64_t exactJsonValueHash(const JsonValue &value) {
  return hashJsonValue(value);
}

PointerMemberTypeDeserializationIdentityGuard::
    PointerMemberTypeDeserializationIdentityGuard() {
  if (pointerMemberTypeDeserializationIdentityActive ||
      !pointerMemberTypeDeserializationIdentities.empty() ||
      !pointerMemberTypeDeserializationReverseIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON pointer-member deserialization graph is already active");
  }
  pointerMemberTypeDeserializationIdentityActive = true;
}

PointerMemberTypeDeserializationIdentityGuard::
    ~PointerMemberTypeDeserializationIdentityGuard() {
  pointerMemberTypeDeserializationIdentities.clear();
  pointerMemberTypeDeserializationReverseIdentities.clear();
  pointerMemberTypeDeserializationIdentityActive = false;
}

FunctionTypeDeserializationIdentityGuard::
    FunctionTypeDeserializationIdentityGuard() {
  if (functionTypeDeserializationIdentityActive ||
      !functionTypeDeserializationIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON function-type deserialization graph is already active");
  }
  functionTypeDeserializationIdentityActive = true;
}

FunctionTypeDeserializationIdentityGuard::
    ~FunctionTypeDeserializationIdentityGuard() {
  functionTypeDeserializationIdentities.clear();
  functionTypeDeserializationIdentityActive = false;
}

FullTypeDeserializationIdentityGuard::FullTypeDeserializationIdentityGuard()
    : owner_(!arrayTypeDeserializationIdentityActive) {
  if (!owner_)
    return;
  if (!arrayTypeDeserializationIdentities.empty() ||
      !sourceTypeDeserializationIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON full type deserialization graph has stale identities");
  }
  arrayTypeDeserializationIdentityActive = true;
}

FullTypeDeserializationIdentityGuard::~FullTypeDeserializationIdentityGuard() {
  if (!owner_)
    return;
  arrayTypeDeserializationIdentities.clear();
  sourceTypeDeserializationIdentities.clear();
  arrayTypeDeserializationIdentityActive = false;
}

TemplateTypeDeserializationIdentityGuard::
    TemplateTypeDeserializationIdentityGuard() {
  if (templateTypeDeserializationIdentityActive ||
      !templateTypeDeserializationIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON template-type deserialization graph is already active");
  }
  templateTypeDeserializationIdentityActive = true;
}

TemplateTypeDeserializationIdentityGuard::
    ~TemplateTypeDeserializationIdentityGuard() {
  templateTypeDeserializationIdentities.clear();
  templateTypeDeserializationIdentityActive = false;
}

ExternalClassDeserializationIdentityGuard::
    ExternalClassDeserializationIdentityGuard() {
  if (externalClassDeserializationIdentityActive ||
      !externalClassDeserializationIdentities.empty() ||
      !externalClassModuleIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON external-class deserialization graph is already active");
  }
  externalClassDeserializationIdentityActive = true;
}

ExternalClassDeserializationIdentityGuard::
    ~ExternalClassDeserializationIdentityGuard() {
  externalClassDeserializationIdentities.clear();
  externalClassModuleIdentities.clear();
  externalClassDeserializationIdentityActive = false;
}

SgClassDeclaration *restoredExternalClassFromJson(const JsonValue &json) {
  if (!externalClassDeserializationIdentityActive)
    throw std::runtime_error(
        "AST JSON external class identity lookup is outside its transaction");
  auto bucket =
      externalClassDeserializationIdentities.find(exactJsonValueHash(json));
  if (bucket == externalClassDeserializationIdentities.end())
    return nullptr;
  for (const auto &identity : bucket->second) {
    if (identity.serialized_declaration.exactlyEquals(json))
      return identity.declaration;
  }
  return nullptr;
}

std::optional<FileInfoFields> fileInfoFieldsFromJson(const JsonValue &json) {
  if (!json.requiredBool("present")) {
    if (json.object.size() != 1)
      throw std::runtime_error("AST JSON absent file info has a payload");
    return std::nullopt;
  }
  const std::string raw_filename = json.requiredString("raw_filename");
  auto required_int = [&](const char *field) {
    const int64_t value = json.requiredInt(field);
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
      throw std::runtime_error(std::string("AST JSON file info ") + field +
                               " is out of range");
    }
    return static_cast<int>(value);
  };
  const int raw_line = required_int("raw_line");
  const int raw_column = required_int("raw_column");
  if (raw_column < 0)
    throw std::runtime_error("AST JSON file info raw_column is out of range");
  const int file_id = required_int("file_id");
  const int raw_file_id = required_int("raw_file_id");
  const int physical_line = required_int("physical_line");
  const int physical_file_id = required_int("physical_file_id");
  const int physical_internal_file_id =
      required_int("physical_internal_file_id");
  auto required_unsigned = [&](const char *field) {
    const int64_t value = json.requiredInt(field);
    if (value < 0 || static_cast<uint64_t>(value) >
                         std::numeric_limits<unsigned int>::max()) {
      throw std::runtime_error(std::string("AST JSON file info ") + field +
                               " is out of range");
    }
    return static_cast<unsigned int>(value);
  };
  const unsigned int source_sequence = required_unsigned("source_sequence");
  const unsigned int physical_file_occurrence =
      required_unsigned("physical_file_occurrence_id");
  const std::string physical_raw_filename =
      json.requiredString("physical_raw_filename");
  const std::string physical_filename =
      json.requiredString("physical_filename");
  unsigned int classification = 0;
  for (const auto &[field, bit] :
       {std::pair{"compiler_generated", Sg_File_Info::e_compiler_generated},
        {"transformation", Sg_File_Info::e_transformation},
        {"frontend_specific", Sg_File_Info::e_frontend_specific},
        {"shared", Sg_File_Info::e_shared},
        {"source_position_unavailable_in_frontend",
         Sg_File_Info::e_source_position_unavailable_in_frontend},
        {"comment_or_directive", Sg_File_Info::e_comment_or_directive},
        {"token", Sg_File_Info::e_token},
        {"default_argument", Sg_File_Info::e_default_argument},
        {"implicit_cast", Sg_File_Info::e_implicit_cast},
        {"output_in_code_generation",
         Sg_File_Info::e_output_in_code_generation}})
    if (json.requiredBool(field))
      classification |= bit;
  const bool compiler_generated =
      classification & Sg_File_Info::e_compiler_generated;
  const bool transformation = classification & Sg_File_Info::e_transformation;
  const bool frontend_specific =
      classification & Sg_File_Info::e_frontend_specific;
  if (raw_file_id >= 0) {
    if (filenameForFileId(raw_file_id, "file info raw_file_id") !=
        raw_filename) {
      throw std::runtime_error(
          "AST JSON file info raw_file_id disagrees with raw_filename");
    }
  } else {
    if (raw_file_id != Sg_File_Info::COPY_FILE_ID &&
        raw_file_id != Sg_File_Info::NULL_FILE_ID &&
        raw_file_id != Sg_File_Info::TRANSFORMATION_FILE_ID &&
        raw_file_id != Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      throw std::runtime_error("AST JSON file info has invalid raw_file_id");
    }
    if (Sg_File_Info::getFilenameFromID(raw_file_id) != raw_filename) {
      throw std::runtime_error(
          "AST JSON file info raw_file_id disagrees with raw_filename");
    }
  }
  if (physical_file_id != physical_internal_file_id) {
    throw std::runtime_error("AST JSON file info physical_file_id disagrees "
                             "with physical_internal_file_id");
  }
  std::string registered_physical_filename;
  if (physical_file_id >= 0) {
    registered_physical_filename =
        filenameForFileId(physical_file_id, "file info physical_file_id");
  } else {
    if (physical_file_id != Sg_File_Info::NULL_FILE_ID &&
        !(physical_file_id == Sg_File_Info::TRANSFORMATION_FILE_ID &&
          transformation) &&
        !(physical_file_id == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
          compiler_generated)) {
      throw std::runtime_error(
          "AST JSON file info has invalid physical_file_id");
    }
    registered_physical_filename =
        Sg_File_Info::getFilenameFromID(physical_file_id);
  }
  if (physical_filename != registered_physical_filename ||
      physical_raw_filename != registered_physical_filename) {
    throw std::runtime_error(
        "AST JSON file info physical filename disagrees with physical_file_id");
  }
  int expected_file_id = raw_file_id;
  if (transformation)
    expected_file_id = Sg_File_Info::TRANSFORMATION_FILE_ID;
  else if (compiler_generated && !frontend_specific)
    expected_file_id = Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  if (expected_file_id == Sg_File_Info::COPY_FILE_ID ||
      (!transformation && compiler_generated && frontend_specific &&
       raw_file_id == Sg_File_Info::NULL_FILE_ID)) {
    throw std::runtime_error(
        "AST JSON file info raw_file_id disagrees with typed classification");
  }
  if (file_id != expected_file_id) {
    throw std::runtime_error(
        "AST JSON file info file_id disagrees with typed classification");
  }
  const std::string filename = json.requiredString("filename");
  const int line = required_int("line");
  const int column = required_int("column");
  const bool unavailable =
      transformation || compiler_generated ||
      (classification &
       Sg_File_Info::e_source_position_unavailable_in_frontend);
  if (!unavailable && raw_line < 0)
    throw std::runtime_error("AST JSON file info raw_line is out of range");
  if (Sg_File_Info::getFilenameFromID(expected_file_id) != filename ||
      (unavailable ? 0 : raw_line) != line ||
      (unavailable ? 0 : raw_column) != column)
    throw std::runtime_error("AST JSON file info effective coordinates "
                             "disagree with raw coordinates "
                             "and typed classification");
  return FileInfoFields{raw_file_id,     raw_line,
                        raw_column,      physical_file_id,
                        physical_line,   physical_file_occurrence,
                        source_sequence, classification,
                        file_id,         line,
                        column};
}

Sg_File_Info *constructFileInfo(const std::optional<FileInfoFields> &fields,
                                SgNode *parent) {
  if (!fields)
    return nullptr;
  std::unique_ptr<Sg_File_Info> info(new Sg_File_Info());
  info->set_file_id(fields->raw_file_id);
  info->set_line(fields->raw_line);
  info->set_col(fields->raw_column);
  info->set_classificationBitField(fields->classification);
  info->set_physical_file_id(fields->physical_file_id);
  info->set_physical_file_occurrence_id(fields->physical_file_occurrence);
  info->set_physical_line(fields->physical_line);
  info->set_source_sequence_number(fields->source_sequence);
  info->set_parent(parent);
  return info.release();
}

namespace {
SourcePositionFields sourcePositionFieldsFromJson(const JsonValue &location,
                                                  bool expression) {
  if (location.kind != JsonValue::Kind::Object)
    throw std::runtime_error("AST JSON location must be an object");
  SourcePositionFields positions;
  positions.start = fileInfoFieldsFromJson(location.at("start"));
  positions.end = fileInfoFieldsFromJson(location.at("end"));
  if (expression)
    positions.operator_position =
        fileInfoFieldsFromJson(location.at("operator"));
  else if (location.find("operator") != nullptr)
    throw std::runtime_error(
        "AST JSON non-expression location has an operator field");
  return positions;
}

bool sourcePositionRecordIsExpression(const NodeRecord &record) {
  static const VariantVector expression_variants(V_SgExpression);
  return kindInVariants(record.kind, expression_variants);
}
} // namespace

Sg_File_Info *buildFileInfo(const JsonValue &json, SgNode *parent) {
  return constructFileInfo(fileInfoFieldsFromJson(json), parent);
}

SourcePositions constructSourcePositions(const SourcePositionFields &fields) {
  return {
      std::unique_ptr<Sg_File_Info>(constructFileInfo(fields.start, nullptr)),
      std::unique_ptr<Sg_File_Info>(constructFileInfo(fields.end, nullptr)),
      std::unique_ptr<Sg_File_Info>(
          constructFileInfo(fields.operator_position, nullptr))};
}

SourcePositions sourcePositionsFromJson(const JsonValue &location,
                                        bool expression) {
  return constructSourcePositions(
      sourcePositionFieldsFromJson(location, expression));
}

NodeConstructionFlags validateConstructionFlags(const NodeRecord &record) {
  return {
      record.flags.requiredBool("is_modified"),
      record.flags.requiredBool("contains_transformation"),
      record.flags.requiredBool(
          "contains_transformation_to_surrounding_whitespace"),
      record.flags.requiredBool("source_range_ends_in_macro_expansion"),
      record.flags.requiredBool("source_range_is_macro_expansion_fragment")};
}

SourcePositionFields validateSourcePositionRecord(const NodeRecord &record) {
  auto fields = sourcePositionFieldsFromJson(
      record.location, sourcePositionRecordIsExpression(record));
  static const VariantVector located_variants(V_SgLocatedNode);
  static const VariantVector pragma_variants(V_SgPragma);
  static const VariantVector file_variants(V_SgFile);
  const bool file = kindInVariants(record.kind, file_variants);
  if (file && fields.end)
    throw std::runtime_error("AST JSON " + record.kind +
                             " file has no endOfConstruct position");
  if ((fields.start || fields.end) && !file &&
      !kindInVariants(record.kind, located_variants) &&
      !kindInVariants(record.kind, pragma_variants))
    throw std::runtime_error(
        "AST JSON " + record.kind +
        " node kind has no restorable source position: " + record.kind);
  return fields;
}

static SgClassDeclaration *canonicalClassDeclaration(SgClassDeclaration *decl) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON class type requires a declaration");
  }

  SgClassDeclaration *first_nondefining =
      isSgClassDeclaration(decl->get_firstNondefiningDeclaration());
  if (first_nondefining == nullptr ||
      first_nondefining->get_firstNondefiningDeclaration() !=
          first_nondefining) {
    throw std::runtime_error(
        "AST JSON class declaration has no self-canonical first "
        "nondefining declaration");
  }
  SgClassDeclaration *defining =
      isSgClassDeclaration(first_nondefining->get_definingDeclaration());
  if (first_nondefining->get_definingDeclaration() != defining ||
      (first_nondefining->get_definition() != nullptr &&
       defining != first_nondefining) ||
      (defining != nullptr &&
       (defining->get_firstNondefiningDeclaration() != first_nondefining ||
        defining->get_definingDeclaration() != defining ||
        defining->get_definition() == nullptr ||
        defining->get_definition()->get_declaration() != defining)) ||
      (decl->get_definingDeclaration() != nullptr &&
       decl->get_definingDeclaration() != defining) ||
      (decl != first_nondefining && decl != defining &&
       decl->get_definingDeclaration() != defining)) {
    throw std::runtime_error(
        "AST JSON class declaration is not in its canonical declaration "
        "family");
  }
  auto symbol_scope = [](SgClassDeclaration *declaration) {
    if (declaration->get_scope() == nullptr)
      throw std::runtime_error(
          "AST JSON class declaration has no semantic scope");
    return SageBuilder::template_class_declaration_scopes::
        fromExactSemanticScope(declaration->get_scope())
            .getCanonicalSymbolScope();
  };
  SgScopeStatement *scope = symbol_scope(decl);
  if (symbol_scope(first_nondefining) != scope ||
      (defining != nullptr && symbol_scope(defining) != scope)) {
    std::ostringstream message;
    message << "AST JSON class declaration family name="
            << decl->get_name().getString() << " declaration=" << decl
            << " scope=" << decl->get_scope() << " first=" << first_nondefining
            << " first-scope=" << first_nondefining->get_scope()
            << " defining=" << defining << " defining-scope="
            << (defining != nullptr ? defining->get_scope() : nullptr)
            << " has no common canonical symbol scope";
    throw std::runtime_error(message.str());
  }

  return first_nondefining;
}

SgClassType *classTypeForDeclaration(SgClassDeclaration *decl) {
  SgClassDeclaration *canonical = canonicalClassDeclaration(decl);
  SgClassType *type = canonical->get_type();
  auto *defining = isSgClassDeclaration(canonical->get_definingDeclaration());
  if (type == nullptr || decl->get_type() != type ||
      (defining != nullptr && defining->get_type() != type) ||
      !SageInterface::isExactTagTypeIdentity(type, decl)) {
    throw std::runtime_error(
        "AST JSON class declaration has no constructed canonical class type");
  }
  return type;
}

SgClassType *constructClassTypeForDeclaration(SgClassDeclaration *decl) {
  SgClassDeclaration *canonical = canonicalClassDeclaration(decl);
  if (decl->get_type() != nullptr &&
      decl->get_type() != canonical->get_type()) {
    throw std::runtime_error(
        "AST JSON class redeclaration does not share its canonical class "
        "type");
  }
  SgClassType *type = SgClassType::createType(canonical);
  decl->set_type(type);
  return type;
}

SgNode *nodeById(const NodeMap &nodes, uint64_t id) {
  auto found = nodes.find(id);
  if (found == nodes.end()) {
    throw std::runtime_error("AST JSON references node id that was not built " +
                             std::to_string(id));
  }
  return found->second;
}

struct PointerMemberSourceFields {
  bool present;
  bool injected;
  bool global;
  SgStringList tokens;
  bool canonical;
  bool source_syntax;
};

PointerMemberSourceFields
pointerMemberSourceFieldsFromJson(const JsonValue &json) {
  const bool canonical = json.requiredBool("semantic_canonical");
  const bool source_syntax = json.requiredBool("fortran_source_syntax");
  const bool present =
      json.requiredBool("source_base_type_qualification_present");
  const bool source_class_type_is_unqualified_injected_name =
      json.requiredBool("source_class_type_is_unqualified_injected_name");
  const bool global =
      json.requiredBool("source_base_type_global_qualification");
  const SgStringList tokens =
      stringListFromJson(json.at("source_base_type_qualification_tokens"),
                         "source_base_type_qualification_tokens");
  if (!present && (global || !tokens.empty())) {
    throw std::runtime_error(
        "AST JSON pointer-member source qualifier payload is present without "
        "its presence bit");
  }
  if (canonical && source_syntax)
    throw std::runtime_error(
        "AST JSON canonical semantic pointer-member type has source syntax");
  if (canonical && source_class_type_is_unqualified_injected_name)
    throw std::runtime_error("AST JSON canonical semantic pointer-member type "
                             "has source-injected class spelling");
  if (canonical && present)
    throw std::runtime_error("AST JSON pointer-member source qualifiers "
                             "changed its serialized semantic/source identity");
  return {present,   source_class_type_is_unqualified_injected_name,
          global,    tokens,
          canonical, source_syntax};
}

uint64_t requiredPointerMemberTypeSerializationIdentity(const JsonValue &json) {
  if (!pointerMemberTypeDeserializationIdentityActive) {
    throw std::runtime_error(
        "AST JSON pointer-member type has no active deserialization graph");
  }
  const int64_t raw_identity = json.requiredInt("pointer_member_identity");
  if (raw_identity <= 0) {
    throw std::runtime_error(
        "AST JSON pointer-member type identity is not positive");
  }
  return static_cast<uint64_t>(raw_identity);
}

SgPointerMemberType *
restoredPointerMemberTypeIdentity(const JsonValue &json,
                                  uint64_t serialized_identity,
                                  const PointerMemberSourceFields &fields) {
  auto existing =
      pointerMemberTypeDeserializationIdentities.find(serialized_identity);
  if (existing == pointerMemberTypeDeserializationIdentities.end()) {
    return nullptr;
  }

  const PointerMemberTypeDeserializationIdentity &identity = existing->second;
  SgPointerMemberType *member_pointer = identity.type;
  if (!identity.serialized_type.exactlyEquals(json) ||
      preservedPointerMemberJsonIdentity(member_pointer) !=
          serialized_identity) {
    throw std::runtime_error(
        "AST JSON pointer-member identity names conflicting serialized type "
        "graphs");
  }
  if (member_pointer == nullptr ||
      member_pointer->get_base_type() != identity.base_type ||
      member_pointer->get_class_type() != identity.class_type ||
      SgPointerMemberType::isCanonicalSemanticType(member_pointer) !=
          fields.canonical ||
      member_pointer->get_fortran_source_syntax() != fields.source_syntax ||
      member_pointer->get_source_base_type_qualification_present() !=
          fields.present ||
      member_pointer->get_source_base_type_global_qualification() !=
          fields.global ||
      member_pointer->get_source_base_type_qualification_tokens() !=
          fields.tokens ||
      member_pointer->get_source_class_type_is_unqualified_injected_name() !=
          fields.injected) {
    throw std::runtime_error(
        "AST JSON pointer-member identity no longer names its exact restored "
        "type graph");
  }
  return member_pointer;
}

void rejectRemovedQualifiedNameState(const JsonValue &properties) {
  if (properties.find("qualified_name_state") != nullptr) {
    throw std::runtime_error(
        "AST JSON contains removed qualified_name_state; source qualifiers "
        "must be represented by typed node fields");
  }
}

void requireRestoredKind(SgNode *node, const NodeRecord &record) {
  const std::string actual =
      node != nullptr ? node->sage_class_name() : std::string("<null>");
  if (actual != record.kind) {
    throw std::runtime_error("AST JSON restored node kind mismatch for id " +
                             std::to_string(record.id) + ": expected " +
                             record.kind + ", got " + actual);
  }
}

uint64_t defaultInitializerDeclarationNodeFromJson(const JsonValue &json) {
  const auto *node = json.find("node");
  const auto *function = json.find("external_function");
  if ((node == nullptr) == (function == nullptr))
    throw std::runtime_error(
        "AST JSON default argument requires one exact parameter identity");
  if (node != nullptr) {
    if (json.find("source_syntax") != nullptr ||
        json.find("parameter_index") != nullptr)
      throw std::runtime_error(
          "AST JSON default argument mixes parameter reference forms");
    return requiredPositiveId(*node, "default argument parameter");
  }
  if (!function->requiredBool("present"))
    throw std::runtime_error(
        "AST JSON default argument requires an external function header");
  const bool source_syntax = json.requiredBool("source_syntax");
  const bool distinct =
      function->requiredBool("parameter_list_syntax_is_distinct");
  if (source_syntax && !distinct &&
      !function->requiredBool("parameter_list_syntax_aliases_parameter_list"))
    throw std::runtime_error(
        "AST JSON default argument references absent source parameters");
  const auto &parameters = function->at(
      source_syntax && distinct ? "syntax_parameters" : "parameters");
  const int64_t index = json.requiredInt("parameter_index");
  if (parameters.kind != JsonValue::Kind::Array || index < 0 ||
      static_cast<uint64_t>(index) >= parameters.array.size())
    throw std::runtime_error(
        "AST JSON default argument has no exact external header parameter");
  return 0;
}

SgCxxDefaultInitExp::source_kind_enum
defaultInitializerKindFromJson(const JsonValue &properties) {
  return requiredEnum<SgCxxDefaultInitExp::source_kind_enum>(
      properties, "source_kind", "default initializer",
      {SgCxxDefaultInitExp::e_parameter, SgCxxDefaultInitExp::e_member});
}

SgInitializedName *defaultInitializerDeclarationFromJson(const JsonValue &json,
                                                         const NodeMap &nodes) {
  if (const uint64_t node = defaultInitializerDeclarationNodeFromJson(json))
    return nodeByIdAs<SgInitializedName>(nodes, node);
  const auto *function = json.find("external_function");
  auto *declaration = externalFunctionFromJson(*function, nodes);
  if (declaration == nullptr)
    throw std::runtime_error(
        "AST JSON default argument has no external function identity");
  auto *list = json.requiredBool("source_syntax")
                   ? declaration->get_parameterList_syntax()
                   : declaration->get_parameterList();
  const int64_t index = json.requiredInt("parameter_index");
  if (list == nullptr || list->get_parent() != declaration || index < 0 ||
      static_cast<uint64_t>(index) >= list->get_args().size())
    throw std::runtime_error(
        "AST JSON default argument has no exact external header parameter");
  return list->get_args()[static_cast<size_t>(index)];
}

bool isJsonScalarLiteralKind(const std::string &kind) {
  return kind == "SgBoolValExp" || kind == "SgShortVal" ||
         kind == "SgUnsignedShortVal" || kind == "SgIntVal" ||
         kind == "SgUnsignedIntVal" || kind == "SgLongIntVal" ||
         kind == "SgUnsignedLongVal" || kind == "SgLongLongIntVal" ||
         kind == "SgUnsignedLongLongIntVal" || kind == "SgInteger128Val" ||
         kind == "SgFortranBOZVal" || kind == "SgCharVal" ||
         kind == "SgSignedCharVal" || kind == "SgUnsignedCharVal" ||
         kind == "SgWcharVal" || kind == "SgChar16Val" ||
         kind == "SgChar32Val" || kind == "SgFloat16Val" ||
         kind == "SgBFloat16Val" || kind == "SgFloatVal" ||
         kind == "SgFloat32Val" || kind == "SgFloat64Val" ||
         kind == "SgDoubleVal" || kind == "SgLongDoubleVal" ||
         kind == "SgFloat80Val" || kind == "SgFloat128Val" ||
         kind == "SgStringVal";
}

bool isJsonValueExpressionKind(const std::string &kind) {
  static const VariantVector variants(V_SgValueExp);
  return kindInVariants(kind, variants);
}

SgValueExp::literal_spelling_form_enum
literalSpellingFormFromJson(const JsonValue &properties) {
  return requiredEnum<SgValueExp::literal_spelling_form_enum>(
      properties, "literal_spelling_form", "SgValueExp",
      {SgValueExp::e_literal_source_spelled,
       SgValueExp::e_literal_canonical_generated});
}

OmpContextPropertyFields
contextPropertyFieldsFromRecord(const NodeRecord &record) {
  const auto &p = record.properties;
  const auto context_kind =
      requiredEnum<SgOmpClause::omp_when_context_kind_enum>(
          p, "context_kind", "SgOmpContextSelectorProperty",
          {SgOmpClause::e_omp_when_context_kind_unknown,
           SgOmpClause::e_omp_when_context_kind_host,
           SgOmpClause::e_omp_when_context_kind_nohost,
           SgOmpClause::e_omp_when_context_kind_any,
           SgOmpClause::e_omp_when_context_kind_cpu,
           SgOmpClause::e_omp_when_context_kind_gpu,
           SgOmpClause::e_omp_when_context_kind_fpga},
          "context kind");
  const auto vendor = requiredEnum<SgOmpClause::omp_when_context_vendor_enum>(
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
       SgOmpClause::e_omp_when_context_vendor_unknown});
  const auto atomic_order =
      requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
          p, "atomic_default_mem_order", "SgOmpContextSelectorProperty",
          {SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed},
          "atomic_default_mem_order kind");
  const auto requires_kind =
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
          "requires kind");
  const auto requires_atomic_order =
      requiredEnum<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
          p, "requires_atomic_default_mem_order",
          "SgOmpContextSelectorProperty",
          {SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_release,
           SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed},
          "requires atomic_default_mem_order kind");
  const SgName extension = p.requiredName("requires_extension");
  validateOmpContextPropertyPayload(
      singleEdgeTarget(record, "expression") != 0, context_kind, vendor,
      atomic_order, requires_kind, requires_atomic_order,
      singleEdgeTarget(record, "requires_expression") != 0,
      !extension.getString().empty());
  return {context_kind,          vendor,   atomic_order, requires_kind,
          requires_atomic_order, extension};
}

void OmpContextPropertyFields::initialize(
    SgOmpContextSelectorProperty *property, SgExpression *expression,
    SgExpression *requires_expression) const {
  property->set_context_kind(kind);
  property->set_context_vendor(vendor);
  property->set_atomic_default_mem_order(atomic_order);
  property->set_requires_kind(requires_kind);
  property->set_requires_atomic_default_mem_order(requires_atomic_order);
  property->set_requires_extension(requires_extension);
  property->set_expression(expression);
  property->set_requires_expression(requires_expression);
}

std::optional<VariantT>
ordinaryOmpVariablesClauseVariant(const std::string &kind) {
  for (VariantT variant :
       {V_SgOmpLinkClause, V_SgOmpEnterClause, V_SgOmpLocalClause,
        V_SgOmpPrivateClause, V_SgOmpFirstprivateClause, V_SgOmpCopyinClause,
        V_SgOmpLastprivateClause, V_SgOmpSharedClause, V_SgOmpCopyprivateClause,
        V_SgOmpUniformClause, V_SgOmpNontemporalClause,
        V_SgOmpIsDevicePtrClause, V_SgOmpUseDevicePtrClause,
        V_SgOmpUseDeviceAddrClause, V_SgOmpHasDeviceAddrClause,
        V_SgOmpExclusiveClause, V_SgOmpInclusiveClause}) {
    if (kind == roseGlobalVariantNameList[variant])
      return variant;
  }
  return std::nullopt;
}

OmpContextSelectorFields contextSelectorFieldsFromJson(const JsonValue &p) {
  const auto selector_kind =
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
           SgOmpClause::e_omp_context_trait_implementation_user});
  const SgName implementation_name =
      p.requiredName("implementation_defined_name");
  return {selector_kind, implementation_name};
}

SgOmpClause::omp_context_selector_set_kind_enum
contextSelectorSetKindFromJson(const JsonValue &p) {
  return requiredEnum<SgOmpClause::omp_context_selector_set_kind_enum>(
      p, "set_kind", "SgOmpContextSelectorSet",
      {SgOmpClause::e_omp_context_selector_set_user,
       SgOmpClause::e_omp_context_selector_set_construct,
       SgOmpClause::e_omp_context_selector_set_device,
       SgOmpClause::e_omp_context_selector_set_target_device,
       SgOmpClause::e_omp_context_selector_set_implementation},
      "set kind");
}

std::optional<VariantT> ompContextHierarchyVariant(const std::string &kind) {
  for (VariantT variant :
       {V_SgOmpContextSelectorProperty, V_SgOmpContextSelector,
        V_SgOmpContextSelectorSet, V_SgOmpWhenClause, V_SgOmpMatchClause})
    if (kind == roseGlobalVariantNameList[variant])
      return variant;
  return std::nullopt;
}

bool requiresDelayedRebuild(const NodeRecord &record) {
  if (ompContextHierarchyVariant(record.kind))
    return true;
  if (ordinaryOmpVariablesClauseVariant(record.kind))
    return true;
  static const VariantVector omp_bodies(V_SgOmpBodyStatement);
  static const VariantVector omp_clause_statements(V_SgOmpClauseStatement);
  if (kindInVariants(record.kind, omp_bodies) ||
      kindInVariants(record.kind, omp_clause_statements))
    return true;
  if (isJsonScalarLiteralKind(record.kind))
    return true;
  if (record.kind == "SgGotoStatement" &&
      singleEdgeTarget(record, "selector_expression") != 0)
    return true;
  if (record.kind == "SgAttributeSpecificationStatement" &&
      record.properties.requiredInt("attribute_kind") ==
          SgAttributeSpecificationStatement::e_dataStatement)
    return true;
  if (record.kind == "SgTemplateParameter") {
    (void)templateParameterSourceFieldsFromJson(record.properties);
    return true;
  }
  return record.kind == "SgSyncAllStatement" ||
         record.kind == "SgSyncImagesStatement" ||
         record.kind == "SgSyncMemoryStatement" ||
         record.kind == "SgSyncTeamStatement" ||
         record.kind == "SgFormTeamStatement" ||
         record.kind == "SgLockStatement" ||
         record.kind == "SgUnlockStatement" ||
         record.kind == "SgStaticAssertionDeclaration" ||
         record.kind == "SgStatementFunctionStatement" ||
         record.kind == "SgAsmOp" || record.kind == "SgAsmStmt" ||
         record.kind == "SgMacroExpansionExp" ||
         record.kind == "SgCxxDefaultInitExp" ||
         record.kind == "SgCxxArrayInitBinding" ||
         record.kind == "SgCxxArrayInitLoop" ||
         record.kind == "SgCxxArrayInitReference" ||
         record.kind == "SgCxxSubobjectDestruction" ||
         record.kind == "SgCxxDecompositionRefExp" ||
         record.kind == "SgOpaqueValueBinding" ||
         record.kind == "SgOpaqueValueReference" ||
         record.kind == "SgPseudoObjectExp" ||
         record.kind == "SgExprStatement" || record.kind == "SgReturnStmt" ||
         record.kind == "SgAssignStatement" ||
         record.kind == "SgAssignedGotoStatement" ||
         record.kind == "SgComputedGotoStatement" ||
         record.kind == "SgFortranDo" || record.kind == "SgForAllStatement" ||
         record.kind == "SgWhereStatement" ||
         record.kind == "SgElseWhereStatement" ||
         record.kind == "SgCaseOptionStmt" ||
         record.kind == "SgDefaultOptionStmt" ||
         record.kind == "SgAllocateStatement" ||
         record.kind == "SgFortranImplicitRange" ||
         record.kind == "SgFortranDataInitializer" ||
         record.kind == "SgDataStatementValue" ||
         record.kind == "SgDataStatementObject" ||
         record.kind == "SgDataStatementGroup" ||
         record.kind == "SgFormatItem" || record.kind == "SgFormatItemList" ||
         record.kind == "SgFormatStatement" || record.kind == "SgNameGroup" ||
         record.kind == "SgNamelistStatement" ||
         record.kind == "SgReadStatement" ||
         record.kind == "SgWriteStatement" ||
         record.kind == "SgImplicitStatement" ||
         record.kind == "SgDeallocateStatement" ||
         record.kind == "SgNullifyStatement" ||
         record.kind == "SgOmpGroupprivateStatement" ||
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
         record.kind == "SgOmpNowaitClause" ||
         record.kind == "SgOmpOrderedClause" ||
         record.kind == "SgOmpSizesClause" ||
         record.kind == "SgOmpAlignClause" ||
         record.kind == "SgOmpMessageClause" ||
         record.kind == "SgOmpGraphIdClause" ||
         record.kind == "SgOmpGraphResetClause" ||
         record.kind == "SgOmpTransparentClause" ||
         record.kind == "SgOmpThreadsetClause" ||
         record.kind == "SgOmpSafesyncClause" ||
         record.kind == "SgOmpLooprangeClause" ||
         record.kind == "SgOmpNoOpenmpConstructsClause" ||
         record.kind == "SgOmpHoldsClause" || record.kind == "SgOmpUseClause" ||
         record.kind == "SgOmpIfClause" ||
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
         record.kind == "SgOmpDeviceClause" ||
         record.kind == "SgOmpDefaultClause" ||
         record.kind == "SgOmpOtherwiseClause" ||
         record.kind == "SgOmpWhenClause" ||
         record.kind == "SgOmpDoacrossClause" ||
         record.kind == "SgOmpScheduleClause" ||
         record.kind == "SgOmpMapClause" || record.kind == "SgOmpToClause" ||
         record.kind == "SgOmpFromClause" ||
         record.kind == "SgOmpDependClause" ||
         record.kind == "SgOmpAffinityClause" ||
         record.kind == "SgOmpReductionClause" ||
         record.kind == "SgOmpInReductionClause" ||
         record.kind == "SgOmpTaskReductionClause" ||
         record.kind == "SgOmpAllocateClause" ||
         record.kind == "SgOmpAllocatorClause" ||
         record.kind == "SgOmpLinearClause" ||
         record.kind == "SgOmpAlignedClause" ||
         record.kind == "SgOmpDistScheduleClause" ||
         record.kind == "SgOmpDestroyClause" || record.kind == "SgOmpMapItem" ||
         record.kind == "SgOmpInductionItem" ||
         record.kind == "SgOmpInductionClause" ||
         record.kind == "SgOmpApplyClause" ||
         record.kind == "SgOmpInitClause" ||
         record.kind == "SgOmpAdjustArgsClause" ||
         record.kind == "SgOmpInitModifierList" ||
         record.kind == "SgOmpAppendArgsClause" ||
         record.kind == "SgOmpAppendArgsOperation" ||
         record.kind == "SgOmpIteratorDefinition" ||
         record.kind == "SgOmpApplyTransformation" ||
         record.kind == "SgOmpInitModifier" ||
         record.kind == "SgOmpMapDistDataPolicy" ||
         record.kind == "SgAttributedStatement" ||
         record.kind == "SgStatementAttribute" ||
         record.kind == "SgStatementAttributeList" ||
         record.kind == "SgProcessControlStatement" ||
         record.kind == "SgAlignmentSpecifier" || record.kind == "SgNewExp" ||
         record.kind == "SgDeleteExp" || record.kind == "SgBaseClass" ||
         record.kind == "SgExpBaseClass" ||
         record.kind == "SgNonrealBaseClass" ||
         isJsonBinaryOpKind(record.kind) || isJsonUnaryOpKind(record.kind) ||
         record.kind == "SgInitializedName" || record.kind == "SgVarRefExp" ||
         record.kind == "SgFunctionParameterRefExp" ||
         record.kind == "SgCompoundLiteralExp" ||
         record.kind == "SgFunctionRefExp" || record.kind == "SgLabelRefExp" ||
         record.kind == "SgMemberFunctionRefExp" ||
         record.kind == "SgTemplateFunctionRefExp" ||
         record.kind == "SgTemplateMemberFunctionRefExp" ||
         record.kind == "SgTypedefDeclaration" ||
         record.kind == "SgNamespaceAliasDeclarationStatement" ||
         record.kind == "SgTemplateTypedefDeclaration" ||
         record.kind == "SgTemplateInstantiationTypedefDeclaration" ||
         record.kind == "SgAggregateInitializer" ||
         record.kind == "SgActualArgumentExpression" ||
         record.kind == "SgFunctionCallExp" ||
         record.kind == "SgConditionalExp" ||
         record.kind == "SgChooseExpression" ||
         record.kind == "SgSubscriptExpression" ||
         record.kind == "SgRangeExp" ||
         record.kind == "SgTemplateParameterVal" ||
         record.kind == "SgEnumVal" || record.kind == "SgNullptrValExp" ||
         record.kind == "SgTypeTraitBuiltinOperator" ||
         record.kind == "SgThisExp" || record.kind == "SgNonrealRefExp" ||
         record.kind == "SgSourceLocationBuiltinExp" ||
         record.kind == "SgPackExpansionExpr" ||
         record.kind == "SgTemplateInstantiationDefn" ||
         record.kind == "SgStatementExpression" ||
         record.kind == "SgFoldExpression" ||
         record.kind == "SgAwaitExpression" || record.kind == "SgComplexVal" ||
         record.kind == "SgTypeExpression" ||
         record.kind == "SgTypeRequirement" ||
         record.kind == "SgSimpleRequirement" ||
         record.kind == "SgCompoundRequirement" ||
         record.kind == "SgNestedRequirement" ||
         record.kind == "SgConstructorInitializer" ||
         record.kind == "SgDesignator" ||
         record.kind == "SgDesignatedInitializer" ||
         record.kind == "SgAssignInitializer" || record.kind == "SgCastExp" ||
         record.kind == "SgSizeOfOp" || record.kind == "SgAlignOfOp" ||
         record.kind == "SgNoexceptOp" || record.kind == "SgVarArgOp" ||
         record.kind == "SgTypeIdOp" || record.kind == "SgBracedInitializer" ||
         record.kind == "SgPseudoDestructorRefExp" ||
         record.kind == "SgOmpDeclareSimdStatement" ||
         record.kind == "SgOmpDeclareMapperStatement" ||
         record.kind == "SgOmpDeclareVariantStatement";
}

SgOmpClause::omp_init_modifier_kind_enum
initModifierKindFromJson(const JsonValue &properties) {
  return requiredEnum<SgOmpClause::omp_init_modifier_kind_enum>(
      properties, "kind", "SgOmpInitModifier",
      {SgOmpClause::e_omp_init_modifier_depobj,
       SgOmpClause::e_omp_init_modifier_interop,
       SgOmpClause::e_omp_init_modifier_prefer_type,
       SgOmpClause::e_omp_init_modifier_depinfo_in,
       SgOmpClause::e_omp_init_modifier_depinfo_out,
       SgOmpClause::e_omp_init_modifier_depinfo_inout,
       SgOmpClause::e_omp_init_modifier_depinfo_inoutset,
       SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset,
       SgOmpClause::e_omp_init_modifier_target,
       SgOmpClause::e_omp_init_modifier_targetsync});
}

static void requireExpressionType(const JsonValue &properties,
                                  const NodeMap &nodes, SgType *type,
                                  const char *context) {
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error("AST JSON " + std::string(context) +
                             " requires an exact semantic type");
  SgType *serialized_type = typeFromJson(properties.at("type"), nodes);
  if (!SageInterface::isEquivalentType(serialized_type, type))
    throw std::runtime_error("AST JSON " + std::string(context) +
                             " type disagrees with its construction input");
}

SgFunctionRefExp *functionReferenceFromProperties(const JsonValue &properties,
                                                  const NodeMap &nodes) {
  const uint64_t implicit_id =
      properties.requiredNonnegativeNodeId("c_implicit_declaration");
  auto *implicit = implicit_id == 0
                       ? nullptr
                       : nodeByIdAs<SgFunctionDeclaration>(nodes, implicit_id);
  const JsonValue &serializedSymbol =
      properties.at("fortran_source_visible_symbol");
  using BindingKind =
      SgFunctionRefExp::fortran_source_visible_binding_kind_enum;
  const auto bindingKind = requiredEnum<BindingKind>(
      properties, "fortran_source_visible_binding_kind", "SgFunctionRefExp",
      {SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable,
       SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed,
       SgFunctionRefExp::e_fortran_source_visible_binding_use_rename,
       SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload,
       SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow,
       SgFunctionRefExp::
           e_fortran_source_visible_binding_semantic_publication});
  SgSymbol *serialized = exactBoundSymbolFromJson(serializedSymbol, nodes);
  SgFunctionSymbol *sourceVisible = isSgFunctionSymbol(serialized);
  if (serialized != nullptr && sourceVisible == nullptr) {
    throw std::runtime_error("AST JSON SgFunctionRefExp Fortran source-visible "
                             "binding is not a function symbol");
  }
  auto *symbol =
      isSgFunctionSymbol(symbolFromJson(properties.at("symbol"), nodes));
  if (symbol == nullptr)
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp semantic symbol is not a function symbol");
  if (bindingKind ==
      SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable) {
    if (sourceVisible != nullptr) {
      throw std::runtime_error(
          "AST JSON SgFunctionRefExp has a source-visible symbol "
          "without a Fortran binding kind");
    }
  } else if (!SageInterface::Detail::hasExactFortranFunctionReferenceBindings(
                 symbol, sourceVisible, bindingKind)) {
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp has no exact Fortran binding for its role");
  }
  const JsonValue &semantic = properties.at("symbol");
  const int64_t declaration = semantic.requiredInt("symbol_declaration");
  if (declaration < 0 ||
      declaration != properties.requiredInt("symbol_declaration") ||
      (declaration != 0 &&
       symbolBasis(symbol) !=
           nodeById(nodes, static_cast<uint64_t>(declaration))) ||
      (declaration == 0 && semantic.find("external_function") == nullptr))
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp has inconsistent function identity");
  validateExternalSymbolBasisOwnership(symbol);
  if (symbol->get_declaration() == nullptr ||
      symbol->get_declaration()->get_type() == nullptr)
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp requires an exact function type");
  SgType *reference_type = typeFromJson(properties.at("type"), nodes);
  auto *signature =
      reference_type != nullptr
          ? isSgFunctionType(reference_type->stripTypedefsAndModifiers())
          : nullptr;
  auto *canonical = symbol->get_declaration()->get_type();
  SgFunctionType *comparison_signature = signature;
  if (auto *member = isSgMemberFunctionType(canonical);
      member != nullptr && signature != nullptr &&
      isSgMemberFunctionType(signature) == nullptr &&
      isSgMemberFunctionDeclaration(symbol->get_declaration()) != nullptr &&
      symbol->get_declaration()
          ->get_declarationModifier()
          .get_storageModifier()
          .isStatic()) {
    if (member->get_mfunc_specifier() != 0)
      throw std::runtime_error(
          "AST JSON static member function has object qualifiers");
    comparison_signature = SageBuilder::buildMemberFunctionType(
        signature, member->get_class_type(), 0);
  }
  if (signature == nullptr ||
      (!SageInterface::isEquivalentType(comparison_signature, canonical) &&
       !SageInterface::isFunctionPrototypeRefinement(signature, canonical) &&
       !SageInterface::isFunctionPrototypeRefinement(canonical, signature))) {
    throw std::runtime_error("AST JSON function reference type conflicts with "
                             "its use-site signature");
  }
  if (implicit != nullptr &&
      (bindingKind !=
           SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable ||
       !implicit->get_is_implicit_function() ||
       implicit->get_name() != symbol->get_declaration()->get_name() ||
       implicit->get_scope() != symbol->get_declaration()->get_scope() ||
       implicit->get_firstNondefiningDeclaration() == nullptr ||
       implicit->get_firstNondefiningDeclaration() !=
           symbol->get_declaration()->get_firstNondefiningDeclaration() ||
       !SageInterface::isEquivalentType(implicit->get_type(), reference_type)))
    throw std::runtime_error(
        "AST JSON function reference has an inconsistent implicit binding");
  auto *reference = new SgFunctionRefExp(symbol, reference_type);
  reference->set_c_implicit_declaration(implicit);
  reference->set_fortran_source_visible_symbol(sourceVisible);
  reference->set_fortran_source_visible_binding_kind(bindingKind);
  return reference;
}

SgMemberFunctionRefExp *
memberFunctionReferenceFromProperties(const JsonValue &properties,
                                      const NodeMap &nodes,
                                      SgTypeExpression *type_id) {
  if (type_id != nullptr && type_id->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON member conversion type-id has a foreign owner");
  auto *symbol =
      isSgMemberFunctionSymbol(symbolFromJson(properties.at("symbol"), nodes));
  const int64_t id = properties.requiredInt("symbol_declaration");
  auto *declaration =
      id > 0 ? nodeByIdAs<SgMemberFunctionDeclaration>(nodes, id) : nullptr;
  if (symbol == nullptr || declaration == nullptr ||
      declaration->get_type() == nullptr ||
      symbol->get_declaration() != declaration)
    throw std::runtime_error(
        "AST JSON member function reference has inconsistent symbol identity");
  const bool virtual_call = requiredIntegerBoolean(properties, "virtual_call");
  const bool need_qualifier =
      requiredIntegerBoolean(properties, "need_qualifier");
  requireExpressionType(properties, nodes, declaration->get_type(),
                        "member function reference");
  auto *reference =
      new SgMemberFunctionRefExp(symbol, virtual_call, need_qualifier);
  reference->set_conversion_type_id(type_id);
  if (type_id != nullptr)
    type_id->set_parent(reference);
  return reference;
}

SgTemplateFunctionRefExp *
templateFunctionReferenceFromProperties(const JsonValue &properties,
                                        const NodeMap &nodes) {
  auto *symbol = isSgTemplateFunctionSymbol(
      symbolFromJson(properties.at("symbol"), nodes));
  const int64_t source_id = properties.requiredInt("symbol_declaration");
  const int64_t semantic_id =
      properties.requiredInt("semantic_function_declaration");
  auto *source =
      source_id > 0
          ? nodeByIdAs<SgTemplateFunctionDeclaration>(nodes, source_id)
          : nullptr;
  auto *semantic = semantic_id > 0
                       ? nodeByIdAs<SgFunctionDeclaration>(nodes, semantic_id)
                       : nullptr;
  if (symbol == nullptr || source == nullptr || semantic == nullptr ||
      semantic->get_type() == nullptr || symbol->get_declaration() != source)
    throw std::runtime_error(
        "AST JSON template function reference has inconsistent "
        "source symbol or semantic declaration");
  requireExpressionType(properties, nodes, semantic->get_type(),
                        "template function reference");
  auto *ref = new SgTemplateFunctionRefExp(symbol);
  ref->set_semantic_function_declaration(semantic);
  return ref;
}

SgTemplateMemberFunctionRefExp *
templateMemberFunctionReferenceFromProperties(const JsonValue &properties,
                                              const NodeMap &nodes,
                                              SgTypeExpression *type_id) {
  if (type_id != nullptr && type_id->get_parent() != nullptr)
    throw std::runtime_error(
        "AST JSON member conversion type-id has a foreign owner");
  auto *symbol = isSgTemplateMemberFunctionSymbol(
      symbolFromJson(properties.at("symbol"), nodes));
  const int64_t source_id = properties.requiredInt("symbol_declaration");
  const int64_t semantic_id =
      properties.requiredInt("semantic_member_function_declaration");
  auto *source =
      source_id > 0
          ? nodeByIdAs<SgTemplateMemberFunctionDeclaration>(nodes, source_id)
          : nullptr;
  auto *semantic =
      semantic_id > 0
          ? nodeByIdAs<SgMemberFunctionDeclaration>(nodes, semantic_id)
          : nullptr;
  if (symbol == nullptr || source == nullptr || semantic == nullptr ||
      semantic->get_type() == nullptr || symbol->get_declaration() != source)
    throw std::runtime_error(
        "AST JSON template member reference has inconsistent "
        "source symbol or semantic declaration");
  const bool virtual_call = requiredIntegerBoolean(properties, "virtual_call");
  const bool need_qualifier =
      requiredIntegerBoolean(properties, "need_qualifier");
  requireExpressionType(properties, nodes, semantic->get_type(),
                        "template member function reference");
  auto *ref =
      new SgTemplateMemberFunctionRefExp(symbol, virtual_call, need_qualifier);
  ref->set_semantic_member_function_declaration(semantic);
  ref->set_conversion_type_id(type_id);
  if (type_id != nullptr)
    type_id->set_parent(ref);
  return ref;
}

SgVarRefExp *variableReferenceFromProperties(const JsonValue &properties,
                                             const NodeMap &nodes) {
  const bool non_odr_use = properties.requiredBool("is_non_odr_use");
  auto *symbol =
      isSgVariableSymbol(symbolFromJson(properties.at("symbol"), nodes));
  if (symbol == nullptr || symbol->get_declaration() == nullptr)
    throw std::runtime_error(
        "AST JSON SgVarRefExp requires an exact variable symbol");
  SgType *type = symbol->get_declaration()->get_type();
  if (SageInterface::containsUnknownType(type))
    throw std::runtime_error(
        "AST JSON SgVarRefExp requires an exact variable type");
  requireExpressionType(properties, nodes, type, "variable reference");
  return new SgVarRefExp(symbol, non_odr_use);
}

SgExpression::unary_type_trait_kind_enum
unaryTraitKindFromProperties(const NodeRecord &record) {
  if (record.properties.find("is_sizeof_pack") != nullptr)
    throw std::runtime_error("AST JSON retired is_sizeof_pack field");
  if (record.kind == "SgTypeIdOp")
    return SgExpression::e_typeid;
  if (record.kind == "SgSizeOfOp")
    return requiredEnum<SgExpression::unary_type_trait_kind_enum>(
        record.properties, "operator_kind", record.kind,
        {SgExpression::e_sizeof, SgExpression::e_sizeof_pack,
         SgExpression::e_datasizeof});
  if (record.kind == "SgAlignOfOp")
    return requiredEnum<SgExpression::unary_type_trait_kind_enum>(
        record.properties, "operator_kind", record.kind,
        {SgExpression::e_alignof, SgExpression::e_preferred_alignof});
  throw std::runtime_error("AST JSON invalid unary trait node kind");
}

UnaryTraitSourceFields
unaryTraitSourceFieldsFromJson(const JsonValue &properties,
                               const std::string &context) {
  return {properties.requiredBool("type_elaboration_required"),
          requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
              properties, "source_type_elaboration_kind", context,
              {SgNonrealDecl::e_source_elaboration_unspecified,
               SgNonrealDecl::e_source_elaboration_none,
               SgNonrealDecl::e_source_elaboration_typename,
               SgNonrealDecl::e_source_elaboration_class,
               SgNonrealDecl::e_source_elaboration_struct,
               SgNonrealDecl::e_source_elaboration_union,
               SgNonrealDecl::e_source_elaboration_enum}),
          unaryTraitEvaluatedValueFromJson(properties.at("evaluated_value"),
                                           context == "SgAlignOfOp")};
}

SgCompoundLiteralExp *compoundLiteralFromProperties(const JsonValue &properties,
                                                    const NodeMap &nodes) {
  auto *name = nodeByIdAs<SgInitializedName>(
      nodes, requiredPositiveId(properties.at("symbol_declaration"),
                                "compound literal variable"));
  auto *symbol =
      isSgVariableSymbol(symbolFromJson(properties.at("symbol"), nodes));
  auto *declaration = isSgVariableDeclaration(name->get_parent());
  auto *initializer = isSgAggregateInitializer(name->get_initializer());
  SgType *type = name->get_type();
  if (symbol == nullptr || symbol->get_declaration() != name ||
      declaration == nullptr || name->get_declptr() != declaration ||
      declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() != name || initializer == nullptr ||
      initializer->get_parent() != name ||
      initializer->get_source_form() !=
          SgAggregateInitializer::
              e_aggregate_initializer_source_compound_literal ||
      SageInterface::containsUnknownType(type) ||
      typeFromJson(properties.at("type"), nodes) != type ||
      !SageInterface::cxxSourceTypeMatchesObjectType(
          name->get_cxx_source_type(), type) ||
      !SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          initializer->get_type(), type))
    throw std::runtime_error("AST JSON SgCompoundLiteralExp requires its exact "
                             "typed initialized variable");
  return new SgCompoundLiteralExp(symbol);
}

SgLabelRefExp *labelReferenceFromProperties(const JsonValue &properties,
                                            const NodeMap &nodes) {
  auto *symbol =
      isSgLabelSymbol(symbolFromJson(properties.at("symbol"), nodes));
  if (symbol == nullptr)
    throw std::runtime_error(
        "AST JSON SgLabelRefExp requires an exact label symbol");
  if (properties.find("type") != nullptr)
    requireExpressionType(properties, nodes, symbol->get_type(),
                          "label reference");
  return new SgLabelRefExp(symbol);
}

SgNonrealRefExp *nonrealReferenceFromProperties(const JsonValue &properties,
                                                const NodeMap &nodes,
                                                SgTypeExpression *type_id) {
  const bool non_odr_use = properties.requiredBool("is_non_odr_use");
  const auto name_kind = requiredEnum<SgNonrealRefExp::name_kind_enum>(
      properties, "name_kind", "SgNonrealRefExp",
      {SgNonrealRefExp::e_ordinary_name,
       SgNonrealRefExp::e_conversion_function_id,
       SgNonrealRefExp::e_destructor_id});
  if (name_kind == SgNonrealRefExp::e_destructor_id &&
      (properties.requiredInt("semantic_role") !=
           SgNonrealRefExp::e_dependent_callable ||
       properties.requiredBool("explicit_template_argument_list") ||
       properties.at("template_arguments").kind != JsonValue::Kind::Array ||
       !properties.at("template_arguments").array.empty() ||
       properties.requiredInt("resolved_function_declaration") != 0 ||
       properties.requiredInt("resolved_variable_declaration") != 0)) {
    throw std::runtime_error(
        "AST JSON dependent destructor has invalid identity");
  }
  if ((name_kind == SgNonrealRefExp::e_conversion_function_id) !=
          (type_id != nullptr) ||
      (type_id != nullptr && type_id->get_parent() != nullptr))
    throw std::runtime_error(
        "AST JSON nonreal reference has inconsistent typed name or owner");
  const auto semantic_role = requiredEnum<SgNonrealRefExp::semantic_role_enum>(
      properties, "semantic_role", "SgNonrealRefExp",
      {SgNonrealRefExp::e_nonreal_reference,
       SgNonrealRefExp::e_dependent_callable});
  if (properties.at("template_arguments").kind != JsonValue::Kind::Array)
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp template_arguments is not an array");
  const bool explicit_arguments =
      properties.requiredBool("explicit_template_argument_list");
  const bool constraint_evaluated =
      properties.requiredBool("constraint_satisfaction_evaluated");
  const bool constraint_satisfied =
      properties.requiredBool("constraint_satisfaction_satisfied");
  const bool constraint_errors =
      properties.requiredBool("constraint_satisfaction_contains_errors");
  const bool constraint_substitution_failure =
      properties.requiredBool("constraint_satisfaction_substitution_failure");
  const std::string constraint_summary =
      properties.requiredString("constraint_satisfaction_summary");
  const bool sfinae_evaluated = properties.requiredBool("sfinae_evaluated");
  const bool sfinae_substitution_failure =
      properties.requiredBool("sfinae_substitution_failure");
  const std::string sfinae_summary =
      properties.requiredString("sfinae_summary");
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
  SgFunctionDeclaration *function =
      function_id != 0 ? nodeByIdAs<SgFunctionDeclaration>(nodes, function_id)
                       : nullptr;
  SgTemplateVariableDeclaration *variable =
      variable_id != 0
          ? nodeByIdAs<SgTemplateVariableDeclaration>(nodes, variable_id)
          : nullptr;
  if (variable != nullptr &&
      (variable->get_variables().size() != 1 ||
       variable->get_variables().front() == nullptr ||
       variable->get_variables().front()->get_parent() != variable ||
       variable->get_variables().front()->get_type() == nullptr))
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp resolved variable template is not an "
        "exact typed specialization declaration");
  const int64_t declaration_id = properties.requiredInt("symbol_declaration");
  if (declaration_id <= 0)
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp requires a positive symbol declaration");
  auto *declaration =
      nodeByIdAs<SgNonrealDecl>(nodes, static_cast<uint64_t>(declaration_id));
  auto *symbol =
      isSgNonrealSymbol(symbolFromJson(properties.at("symbol"), nodes));
  if (symbol == nullptr || symbol->get_declaration() != declaration)
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp symbol cross-edge is inconsistent");
  validateNonrealResolvedTargetIdentity<SgDeclarationStatement *>(
      function, variable, declaration->get_templateDeclaration());
  SgType *type = nullptr;
  if (function != nullptr)
    type = function->get_type();
  else if (variable != nullptr)
    type = variable->get_variables().front()->get_type();
  else
    type = symbol->get_type();
  requireExpressionType(properties, nodes, type, "nonreal reference");
  auto reference = std::make_unique<SgNonrealRefExp>(symbol, non_odr_use);
  reference->set_name_kind(name_kind);
  reference->set_semantic_role(semantic_role);
  reference->set_explicit_template_argument_list(explicit_arguments);
  reference->set_constraintSatisfactionEvaluated(constraint_evaluated);
  reference->set_constraintSatisfactionSatisfied(constraint_satisfied);
  reference->set_constraintSatisfactionContainsErrors(constraint_errors);
  reference->set_constraintSatisfactionSubstitutionFailure(
      constraint_substitution_failure);
  reference->set_constraintSatisfactionSummary(constraint_summary);
  reference->set_sfinaeEvaluated(sfinae_evaluated);
  reference->set_sfinaeSubstitutionFailure(sfinae_substitution_failure);
  reference->set_sfinaeSummary(sfinae_summary);
  reference->set_resolved_function_declaration(function);
  reference->set_resolved_variable_declaration(variable);
  reference->get_templateArguments() = templateArgumentListFromJson(
      properties.at("template_arguments"), nodes, reference.get());
  reference->set_conversion_type_id(type_id);
  if (type_id != nullptr)
    type_id->set_parent(reference.get());
  return reference.release();
}

SgScopeStatement *nearestScope(SgNode *node);

static SgTypeOfType *typeOfOperandOwner(SgExpression *expression) {
  SgNode *owner = expression->get_parent();
  if (owner == nullptr)
    return nullptr;
  auto *type = isSgTypeOfType(owner);
  if (type == nullptr || type->get_base_expression() != expression)
    throw std::runtime_error(
        "AST JSON typeof operand already has a different lexical owner");
  ROSE_ASSERT(type->get_base_type() == expression->get_type());
  return type;
}

SgDeclType *buildDeclType(SgExpression *base_expression, SgType *base_type,
                          bool is_gnu_decltype) {
  if (base_expression == nullptr)
    throw std::runtime_error(
        "AST JSON SgDeclType requires a structured base_expression node");
  if (SageInterface::containsUnknownType(base_type))
    throw std::runtime_error(
        "AST JSON SgDeclType requires one exact semantic base type");
  if (SgNode *owner = base_expression->get_parent()) {
    auto *existing = isSgDeclType(owner);
    if (existing == nullptr ||
        existing->get_base_expression() != base_expression ||
        existing->get_base_type() != base_type ||
        existing->get_is_gnu_decltype() != is_gnu_decltype)
      throw std::runtime_error(
          "AST JSON decltype operand conflicts with its exact lexical owner");
    return existing;
  }
  return SageBuilder::buildDeclType(base_expression, base_type,
                                    is_gnu_decltype);
}

using PrimitiveTypeFactory = SgType *(*)();

PrimitiveTypeFactory primitiveTypeFactoryFromKind(const std::string &kind) {
  if (kind == "SgTypeVoid")
    return +[]() -> SgType * { return SageBuilder::buildVoidType(); };
  if (kind == "SgTypeBool")
    return +[]() -> SgType * { return SageBuilder::buildBoolType(); };
  if (kind == "SgTypeChar")
    return +[]() -> SgType * { return SageBuilder::buildCharType(); };
  if (kind == "SgTypeSignedChar")
    return +[]() -> SgType * { return SageBuilder::buildSignedCharType(); };
  if (kind == "SgTypeUnsignedChar")
    return +[]() -> SgType * { return SageBuilder::buildUnsignedCharType(); };
  if (kind == "SgTypeShort")
    return +[]() -> SgType * { return SageBuilder::buildShortType(); };
  if (kind == "SgTypeSignedShort")
    return +[]() -> SgType * { return SageBuilder::buildSignedShortType(); };
  if (kind == "SgTypeUnsignedShort")
    return +[]() -> SgType * { return SageBuilder::buildUnsignedShortType(); };
  if (kind == "SgTypeInt")
    return +[]() -> SgType * { return SageBuilder::buildIntType(); };
  if (kind == "SgTypeSignedInt")
    return +[]() -> SgType * { return SageBuilder::buildSignedIntType(); };
  if (kind == "SgTypeUnsignedInt")
    return +[]() -> SgType * { return SageBuilder::buildUnsignedIntType(); };
  if (kind == "SgTypeLong")
    return +[]() -> SgType * { return SageBuilder::buildLongType(); };
  if (kind == "SgTypeSignedLong")
    return +[]() -> SgType * { return SageBuilder::buildSignedLongType(); };
  if (kind == "SgTypeUnsignedLong")
    return +[]() -> SgType * { return SageBuilder::buildUnsignedLongType(); };
  if (kind == "SgTypeLongLong")
    return +[]() -> SgType * { return SageBuilder::buildLongLongType(); };
  if (kind == "SgTypeSignedLongLong")
    return +[]() -> SgType * { return SageBuilder::buildSignedLongLongType(); };
  if (kind == "SgTypeUnsignedLongLong")
    return +[]() -> SgType * {
      return SageBuilder::buildUnsignedLongLongType();
    };
  if (kind == "SgTypeFloat")
    return +[]() -> SgType * { return SageBuilder::buildFloatType(); };
  if (kind == "SgTypeDouble")
    return +[]() -> SgType * { return SageBuilder::buildDoubleType(); };
  if (kind == "SgTypeLongDouble")
    return +[]() -> SgType * { return SageBuilder::buildLongDoubleType(); };
  if (kind == "SgTypeFloat16")
    return +[]() -> SgType * { return SageBuilder::buildFloat16Type(); };
  if (kind == "SgTypeFloat80")
    return +[]() -> SgType * { return SageBuilder::buildFloat80Type(); };
  if (kind == "SgTypeFloat128")
    return +[]() -> SgType * { return SageBuilder::buildFloat128Type(); };
  if (kind == "SgTypeFp16")
    return +[]() -> SgType * { return SageBuilder::buildFp16Type(); };
  if (kind == "SgTypeBFloat16")
    return +[]() -> SgType * { return SageBuilder::buildBFloat16Type(); };
  if (kind == "SgTypeFloat32")
    return +[]() -> SgType * { return SageBuilder::buildFloat32Type(); };
  if (kind == "SgTypeFloat64")
    return +[]() -> SgType * { return SageBuilder::buildFloat64Type(); };
  if (kind == "SgTypeFloat32x")
    return +[]() -> SgType * { return SageBuilder::buildFloat32xType(); };
  if (kind == "SgTypeFloat64x")
    return +[]() -> SgType * { return SageBuilder::buildFloat64xType(); };
  if (kind == "SgTypeWchar")
    return +[]() -> SgType * { return SageBuilder::buildWcharType(); };
  if (kind == "SgTypeChar8")
    return +[]() -> SgType * { return SageBuilder::buildChar8Type(); };
  if (kind == "SgTypeChar16")
    return +[]() -> SgType * { return SageBuilder::buildChar16Type(); };
  if (kind == "SgTypeChar32")
    return +[]() -> SgType * { return SageBuilder::buildChar32Type(); };
  return nullptr;
}

namespace {
struct BinaryOpFactory {
  const char *kind;
  SgBinaryOp *(*construct)(SgExpression *lhs, SgExpression *rhs,
                           SgType *result_type);
};
#define REX_JSON_BINARY_OP(name)                                               \
  {#name,                                                                      \
   +[](SgExpression *lhs, SgExpression *rhs, SgType *result_type)              \
       -> SgBinaryOp * { return new name(lhs, rhs, result_type); }}
const BinaryOpFactory binaryOpFactories[] = {
    REX_JSON_BINARY_OP(SgAssignOp),
    REX_JSON_BINARY_OP(SgAddOp),
    REX_JSON_BINARY_OP(SgSubtractOp),
    REX_JSON_BINARY_OP(SgMultiplyOp),
    REX_JSON_BINARY_OP(SgDivideOp),
    REX_JSON_BINARY_OP(SgModOp),
    REX_JSON_BINARY_OP(SgLessThanOp),
    REX_JSON_BINARY_OP(SgLessOrEqualOp),
    REX_JSON_BINARY_OP(SgGreaterThanOp),
    REX_JSON_BINARY_OP(SgGreaterOrEqualOp),
    REX_JSON_BINARY_OP(SgEqualityOp),
    REX_JSON_BINARY_OP(SgNotEqualOp),
    REX_JSON_BINARY_OP(SgLshiftOp),
    REX_JSON_BINARY_OP(SgRshiftOp),
    REX_JSON_BINARY_OP(SgAndOp),
    REX_JSON_BINARY_OP(SgOrOp),
    REX_JSON_BINARY_OP(SgBitAndOp),
    REX_JSON_BINARY_OP(SgBitOrOp),
    REX_JSON_BINARY_OP(SgBitXorOp),
    REX_JSON_BINARY_OP(SgCommaOpExp),
    REX_JSON_BINARY_OP(SgDotExp),
    REX_JSON_BINARY_OP(SgScopeOp),
    REX_JSON_BINARY_OP(SgArrowExp),
    REX_JSON_BINARY_OP(SgDotStarOp),
    REX_JSON_BINARY_OP(SgArrowStarOp),
    REX_JSON_BINARY_OP(SgConcatenationOp),
    REX_JSON_BINARY_OP(SgExponentiationOp),
    {"SgPointerAssignOp",
     +[](SgExpression *lhs, SgExpression *rhs,
         SgType *result_type) -> SgBinaryOp * {
       return new SgPointerAssignOp(static_cast<Sg_File_Info *>(nullptr), lhs,
                                    rhs, result_type);
     }},
    REX_JSON_BINARY_OP(SgPlusAssignOp),
    REX_JSON_BINARY_OP(SgMinusAssignOp),
    REX_JSON_BINARY_OP(SgMultAssignOp),
    REX_JSON_BINARY_OP(SgDivAssignOp),
    REX_JSON_BINARY_OP(SgModAssignOp),
    REX_JSON_BINARY_OP(SgAndAssignOp),
    REX_JSON_BINARY_OP(SgIorAssignOp),
    REX_JSON_BINARY_OP(SgXorAssignOp),
    REX_JSON_BINARY_OP(SgLshiftAssignOp),
    REX_JSON_BINARY_OP(SgRshiftAssignOp),
};
#undef REX_JSON_BINARY_OP
const BinaryOpFactory *binaryOpFactoryFor(const std::string &kind) {
  for (const BinaryOpFactory &factory : binaryOpFactories) {
    if (kind == factory.kind)
      return &factory;
  }
  return nullptr;
}
} // namespace

bool isJsonBinaryOpKind(const std::string &kind) {
  return kind == "SgPntrArrRefExp" || binaryOpFactoryFor(kind) != nullptr;
}

std::optional<SgArrowExp::emission_role_enum>
binaryEmissionRoleFromJson(const JsonValue &properties,
                           const std::string &kind) {
  if (kind != "SgArrowExp")
    return std::nullopt;
  return requiredEnum<SgArrowExp::emission_role_enum>(
      properties, "arrow_emission_role", "SgArrowExp",
      {SgArrowExp::e_emit_arrow_operator,
       SgArrowExp::e_implicit_object_access});
}

std::optional<long long> binaryStorageSizeFromJson(const JsonValue &properties,
                                                   const std::string &kind) {
  if (kind == "SgPntrArrRefExp")
    return storageSizeFromJson(properties, "result_storage_size_bytes");
  if (properties.find("result_storage_size_bytes"))
    throw std::runtime_error(
        "AST JSON storage size requires an array reference");
  return std::nullopt;
}

std::vector<std::optional<long long>>
binarySubscriptStridesFromJson(const JsonValue &properties,
                               const std::string &kind) {
  std::vector<std::optional<long long>> strides;
  if (kind != "SgPntrArrRefExp") {
    if (properties.find("subscript_stride_bits"))
      throw std::runtime_error(
          "AST JSON subscript strides require an array reference");
    return strides;
  }
  const auto &values = properties.at("subscript_stride_bits");
  if (values.kind != JsonValue::Kind::Array)
    throw std::runtime_error("AST JSON subscript strides must be an array");
  for (const auto &stride : values.array) {
    if (stride.kind == JsonValue::Kind::Null)
      strides.emplace_back(std::nullopt);
    else
      strides.emplace_back(stride.asInt());
  }
  return strides;
}

SgBinaryOp *
constructBinaryOp(const std::string &kind, SgExpression *lhs, SgExpression *rhs,
                  SgType *result_type,
                  std::optional<SgArrowExp::emission_role_enum> arrow_role,
                  std::optional<long long> result_storage_size_bytes,
                  const std::vector<std::optional<long long>> &strides) {
  const BinaryOpFactory *factory = binaryOpFactoryFor(kind);
  if (factory == nullptr && kind != "SgPntrArrRefExp")
    throw std::runtime_error("AST JSON " + kind +
                             " is not an exact binary operation");
  if (arrow_role.has_value() != (kind == "SgArrowExp") ||
      (arrow_role && *arrow_role != SgArrowExp::e_emit_arrow_operator &&
       *arrow_role != SgArrowExp::e_implicit_object_access))
    throw std::runtime_error("AST JSON " + kind +
                             " has invalid binary emission metadata");
  if (SageInterface::containsUnknownType(result_type))
    throw std::runtime_error("AST JSON " + kind +
                             " has no exact semantic result type");
  if (result_storage_size_bytes &&
      (kind != "SgPntrArrRefExp" || *result_storage_size_bytes < 0))
    throw std::runtime_error(
        "AST JSON has invalid array-reference storage size");
  if (lhs == nullptr || rhs == nullptr || lhs == rhs ||
      lhs->get_parent() != nullptr || rhs->get_parent() != nullptr)
    throw std::runtime_error("AST JSON " + kind +
                             " requires distinct unowned operands");
  const auto *indices = isSgExprListExp(rhs);
  if (!strides.empty() &&
      (kind != "SgPntrArrRefExp" ||
       strides.size() != (indices ? indices->get_expressions().size() : 1)))
    throw std::runtime_error(
        "AST JSON subscript stride count disagrees with indices");
  // Restore borrowed operands before rejecting a constructor defect.
  std::unique_ptr<SgBinaryOp> result(
      kind == "SgPntrArrRefExp"
          ? new SgPntrArrRefExp(lhs, rhs, result_type,
                                result_storage_size_bytes, strides)
          : factory->construct(lhs, rhs, result_type));
  if (auto *arrow = isSgArrowExp(result.get()))
    arrow->set_emission_role(arrow_role.value());
  if (result->get_lhs_operand_i() != lhs ||
      result->get_rhs_operand_i() != rhs || lhs->get_parent() != result.get() ||
      rhs->get_parent() != result.get() || result->get_type() != result_type) {
    lhs->set_parent(nullptr);
    rhs->set_parent(nullptr);
    throw std::runtime_error("AST JSON " + kind +
                             " did not bind its exact operands and type");
  }
  return result.release();
}

namespace {
struct UnaryOpFactory {
  const char *kind;
  SgUnaryOp *(*construct)(SgExpression *operand,
                          const UnaryConstructionFields &fields,
                          SgType *result_type);
  bool allows_postfix = false;

  void validateMode(SgUnaryOp::Sgop_mode mode) const {
    if (mode != SgUnaryOp::prefix &&
        (mode != SgUnaryOp::postfix || !allows_postfix))
      throw std::runtime_error("AST JSON " + std::string(kind) +
                               " has an invalid unary mode");
  }
};
#define REX_JSON_UNARY_OP(name)                                                \
  {#name,                                                                      \
   +[](SgExpression *operand, const UnaryConstructionFields &fields,           \
       SgType *result_type) -> SgUnaryOp * {                                   \
     return new name(operand, result_type, fields.mode);                       \
   },                                                                          \
   std::is_same_v<name, SgPlusPlusOp> || std::is_same_v<name, SgMinusMinusOp>}
const UnaryOpFactory unaryOpFactories[] = {
    {"SgAddressOfOp",
     +[](SgExpression *operand, const UnaryConstructionFields &fields,
         SgType *result_type) -> SgUnaryOp * {
       return new SgAddressOfOp(operand, result_type, fields.mode,
                                fields.address_form);
     }},
    REX_JSON_UNARY_OP(SgPointerDerefExp),
    REX_JSON_UNARY_OP(SgNotOp),
    REX_JSON_UNARY_OP(SgBitComplementOp),
    REX_JSON_UNARY_OP(SgPlusPlusOp),
    REX_JSON_UNARY_OP(SgMinusMinusOp),
    REX_JSON_UNARY_OP(SgMinusOp),
    REX_JSON_UNARY_OP(SgUnaryAddOp),
    {"SgUserDefinedUnaryOp",
     +[](SgExpression *operand, const UnaryConstructionFields &fields,
         SgType *result_type) -> SgUnaryOp * {
       return new SgUserDefinedUnaryOp(operand, result_type, fields.mode,
                                       fields.defined_operator_name,
                                       fields.defined_operator_symbol);
     }},
    {"SgVectorElementExp",
     +[](SgExpression *operand, const UnaryConstructionFields &fields,
         SgType *result_type) -> SgUnaryOp * {
       return new SgVectorElementExp(
           operand, result_type, fields.mode, fields.vector_components,
           fields.vector_selector, fields.vector_is_arrow);
     }},
    {"SgThrowOp",
     +[](SgExpression *operand, const UnaryConstructionFields &fields,
         SgType *result_type) -> SgUnaryOp * {
       return new SgThrowOp(operand, result_type, fields.mode,
                            fields.throw_kind);
     }},
};
#undef REX_JSON_UNARY_OP
const UnaryOpFactory *unaryOpFactoryFor(const std::string &kind) {
  for (const UnaryOpFactory &factory : unaryOpFactories) {
    if (kind == factory.kind)
      return &factory;
  }
  return nullptr;
}
} // namespace

bool isJsonUnaryOpKind(const std::string &kind) {
  return unaryOpFactoryFor(kind) != nullptr;
}

UnaryConstructionFields
unaryConstructionFieldsFromJson(const JsonValue &properties,
                                const std::string &kind, const NodeMap &nodes) {
  const UnaryOpFactory *factory = unaryOpFactoryFor(kind);
  if (factory == nullptr)
    throw std::runtime_error("AST JSON " + kind +
                             " is not an exact unary operation");
  UnaryConstructionFields fields{
      requiredEnum<SgUnaryOp::Sgop_mode>(
          properties, "mode", kind, {SgUnaryOp::prefix, SgUnaryOp::postfix}),
      SgAddressOfOp::e_address_operand_unclassified,
      SgThrowOp::throw_expression};
  factory->validateMode(fields.mode);
  if (kind == "SgUserDefinedUnaryOp") {
    fields.defined_operator_name = properties.requiredName("operator_name");
    fields.defined_operator_symbol =
        isSgFunctionSymbol(symbolFromJson(properties.at("symbol"), nodes));
    auto *declaration = fields.defined_operator_symbol != nullptr
                            ? fields.defined_operator_symbol->get_declaration()
                            : nullptr;
    if (fields.defined_operator_name.getString().empty() ||
        declaration == nullptr || declaration->get_type() == nullptr ||
        SageInterface::containsUnknownType(declaration->get_type())) {
      throw std::runtime_error("AST JSON SgUserDefinedUnaryOp requires its "
                               "exact typed procedure and prefix source form");
    }
  } else if (kind == "SgAddressOfOp") {
    fields.address_form =
        requiredEnum<SgAddressOfOp::address_operand_form_enum>(
            properties, "address_operand_form", kind,
            {SgAddressOfOp::e_address_operand_object,
             SgAddressOfOp::e_address_operand_qualified_id});
  } else if (kind == "SgThrowOp") {
    fields.throw_kind = requiredEnum<SgThrowOp::e_throw_kind>(
        properties, "throw_kind", "SgThrowOp",
        {SgThrowOp::throw_expression, SgThrowOp::rethrow});
  } else if (kind == "SgVectorElementExp") {
    fields.vector_selector =
        requiredEnum<SgVectorElementExp::selector_kind_enum>(
            properties, "selector_kind", kind,
            {SgVectorElementExp::e_components, SgVectorElementExp::e_hi,
             SgVectorElementExp::e_lo, SgVectorElementExp::e_even,
             SgVectorElementExp::e_odd});
    fields.vector_is_arrow = properties.requiredBool("is_arrow");
    const JsonValue &components = properties.at("components");
    if (components.kind != JsonValue::Kind::Array || components.array.empty())
      throw std::runtime_error(
          "AST JSON SgVectorElementExp has invalid component metadata");
    for (const JsonValue &component : components.array) {
      const int64_t index = component.asInt();
      if (index < 0 ||
          static_cast<uint64_t>(index) >
              std::numeric_limits<unsigned int>::max() ||
          (fields.vector_selector == SgVectorElementExp::e_components &&
           index >= 16))
        throw std::runtime_error(
            "AST JSON SgVectorElementExp component is out of range");
      fields.vector_components.push_back(static_cast<unsigned int>(index));
    }
  }
  return fields;
}

SgUnaryOp *constructUnaryOp(const std::string &kind, SgExpression *operand,
                            const UnaryConstructionFields &fields,
                            SgType *result_type) {
  const UnaryOpFactory *factory = unaryOpFactoryFor(kind);
  if (factory == nullptr)
    throw std::runtime_error("AST JSON " + kind +
                             " is not an exact unary operation");
  factory->validateMode(fields.mode);
  if (SageInterface::containsUnknownType(result_type))
    throw std::runtime_error("AST JSON " + kind +
                             " has no exact semantic result type");
  // The specialized throw result contract aborts inside get_type. Validate it
  // here, before allocation, so malformed input is rejected without aborting.
  if (kind == "SgThrowOp" && isSgTypeVoid(result_type) == nullptr)
    throw std::runtime_error(
        "AST JSON SgThrowOp requires a void semantic result type");
  // The specialized address-of contract aborts when a pointer-to-member type
  // meets the object form. Validate the same rule before allocation.
  if (kind == "SgAddressOfOp") {
    SgType *stripped = result_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                              SgType::STRIP_MODIFIER_TYPE);
    if (stripped == nullptr ||
        (isSgPointerMemberType(stripped) != nullptr &&
         fields.address_form != SgAddressOfOp::e_address_operand_qualified_id))
      throw std::runtime_error("AST JSON SgAddressOfOp form disagrees with "
                               "its exact result type");
  }
  const bool rethrow =
      kind == "SgThrowOp" && fields.throw_kind == SgThrowOp::rethrow;
  if (operand == nullptr) {
    if (!rethrow)
      throw std::runtime_error("AST JSON " + kind +
                               " requires an exact operand");
  } else if (operand->get_parent() != nullptr || rethrow) {
    throw std::runtime_error("AST JSON " + kind +
                             " requires an exclusive matching operand");
  }
  if (kind == "SgVectorElementExp") {
    auto valueType = [](SgType *type) {
      while (type != nullptr) {
        type = type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                               SgType::STRIP_REFERENCE_TYPE |
                               SgType::STRIP_RVALUE_REFERENCE_TYPE);
        auto *modifier = isSgModifierType(type);
        if (modifier == nullptr || modifier->get_typeModifier().isVectorType())
          break;
        type = modifier->get_base_type();
      }
      return type;
    };
    SgType *base = valueType(operand->get_type());
    if (fields.vector_is_arrow) {
      auto *pointer = isSgPointerType(base);
      if (pointer == nullptr)
        throw std::runtime_error(
            "AST JSON SgVectorElementExp arrow requires a pointer operand");
      base = valueType(pointer->get_base_type());
    }
    auto *vector = isSgModifierType(base);
    if (vector == nullptr || !vector->get_typeModifier().isExtendedVectorType())
      throw std::runtime_error(
          "AST JSON SgVectorElementExp requires an extended vector operand");
    SgType *element = valueType(vector->get_base_type());
    SgType *result_element = valueType(result_type);
    size_t result_size = 1;
    if (auto *result_vector = isSgModifierType(result_element)) {
      if (!result_vector->get_typeModifier().isExtendedVectorType() ||
          result_vector->get_typeModifier().get_vector_size() <= 1)
        throw std::runtime_error(
            "AST JSON SgVectorElementExp requires an extended vector result");
      result_size = result_vector->get_typeModifier().get_vector_size();
      result_element = valueType(result_vector->get_base_type());
    }
    if (result_size != fields.vector_components.size() ||
        !SageInterface::isEquivalentType(element, result_element))
      throw std::runtime_error(
          "AST JSON SgVectorElementExp result disagrees with its components");
    const size_t width = vector->get_typeModifier().get_vector_size();
    if (fields.vector_selector == SgVectorElementExp::e_components) {
      for (unsigned index : fields.vector_components) {
        if (index >= width)
          throw std::runtime_error(
              "AST JSON SgVectorElementExp component exceeds its operand");
      }
    } else {
      if (result_size != width / 2 + width % 2)
        throw std::runtime_error(
            "AST JSON SgVectorElementExp group has an invalid result width");
      for (size_t index = 0; index < result_size; ++index) {
        size_t expected = index;
        switch (fields.vector_selector) {
        case SgVectorElementExp::e_hi:
          expected += result_size;
          break;
        case SgVectorElementExp::e_lo:
          break;
        case SgVectorElementExp::e_even:
          expected *= 2;
          break;
        case SgVectorElementExp::e_odd:
          expected = 2 * index + 1;
          break;
        default:
          throw std::runtime_error(
              "AST JSON SgVectorElementExp has an invalid group selector");
        }
        if (fields.vector_components[index] != expected)
          throw std::runtime_error("AST JSON SgVectorElementExp group "
                                   "disagrees with its components");
      }
    }
  }
  // Hold the fresh node in RAII ownership and restore the borrowed operand
  // before any post-allocation failure escapes, so a defect never leaks the
  // result or mutates its operand.
  std::unique_ptr<SgUnaryOp> result(
      factory->construct(operand, fields, result_type));
  if (result->get_operand_i() != operand ||
      (operand != nullptr && operand->get_parent() != result.get()) ||
      result->get_mode() != fields.mode || result->get_type() != result_type) {
    if (operand != nullptr)
      operand->set_parent(nullptr);
    throw std::runtime_error("AST JSON " + kind +
                             " did not bind its exact operand and type");
  }
  return result.release();
}

namespace {

// An inline expression is published only after its complete subtree has been
// decoded. Track every private allocation, including children not yet attached
// to their parent, and distinguish NodeMap references from transferred nodes.
thread_local std::vector<SgCxxArrayInitBinding *> inlineArrayInitBindings;
thread_local std::vector<SgOpaqueValueBinding *> inlineOpaqueBindings;
thread_local unsigned inlineOpaqueDepth = 0;

SgOpaqueValueBinding *opaqueBindingFromJson(const JsonValue &json,
                                            const NodeMap &nodes) {
  const auto *depth = json.find("binding_depth");
  const auto *id = json.find("binding");
  if ((depth == nullptr) == (id == nullptr))
    throw std::runtime_error(
        "AST JSON opaque reference requires one binding identity");
  if (depth != nullptr) {
    const int64_t index = depth->asInt();
    if (index < 0 ||
        static_cast<uint64_t>(index) >= inlineOpaqueBindings.size())
      throw std::runtime_error(
          "AST JSON opaque reference has no active binding");
    return inlineOpaqueBindings[inlineOpaqueBindings.size() - 1 - index];
  }
  auto *binding = nodeByIdAs<SgOpaqueValueBinding>(
      nodes, requiredPositiveId(*id, "opaque binding"));
  if (std::find(inlineOpaqueBindings.begin(), inlineOpaqueBindings.end(),
                binding) != inlineOpaqueBindings.end())
    throw std::runtime_error(
        "AST JSON inline opaque reference requires lexical identity");
  return binding;
}

void requireBorrowedBindings(SgNode *node, const NodeMap &nodes) {
  auto require_opaque = [&](SgOpaqueValueBinding *binding) {
    const bool local =
        std::find(inlineOpaqueBindings.begin(), inlineOpaqueBindings.end(),
                  binding) != inlineOpaqueBindings.end();
    const bool external =
        std::any_of(nodes.begin(), nodes.end(), [binding](const auto &entry) {
          return entry.second == binding;
        });
    if (!local && !external)
      throw std::runtime_error(
          "AST JSON borrowed opaque reference has no active binding");
  };
  if (auto *pseudo = isSgPseudoObjectExp(node)) {
    pseudo->validate();
    for (auto *binding : pseudo->get_captured_bindings())
      require_opaque(binding);
    requireBorrowedBindings(pseudo->get_written_expression(), nodes);
    OpaqueBindingScope scope(inlineOpaqueBindings, inlineOpaqueDepth);
    for (auto *step : pseudo->get_semantic_evaluations()->get_expressions()) {
      requireBorrowedBindings(step, nodes);
      if (auto *binding = isSgOpaqueValueBinding(step))
        inlineOpaqueBindings.push_back(binding);
    }
    return;
  }
  if (auto *reference = isSgOpaqueValueReference(node))
    require_opaque(reference->get_binding());
  if (auto *loop = isSgCxxArrayInitLoop(node)) {
    requireBorrowedBindings(loop->get_binding()->get_common_source(), nodes);
    ArrayInitBindingScope scope(inlineArrayInitBindings, loop->get_binding());
    requireBorrowedBindings(loop->get_element_initializer(), nodes);
    return;
  }
  if (auto *reference = isSgCxxArrayInitReference(node)) {
    auto *binding = reference->get_binding();
    const bool local = std::find(inlineArrayInitBindings.begin(),
                                 inlineArrayInitBindings.end(),
                                 binding) != inlineArrayInitBindings.end();
    const bool external =
        std::any_of(nodes.begin(), nodes.end(), [binding](const auto &entry) {
          return entry.second == binding;
        });
    if ((!local && !external) ||
        (reference->get_reference_kind() ==
             SgCxxArrayInitReference::e_current_index &&
         !inlineArrayInitBindings.empty() &&
         inlineArrayInitBindings.back() != binding))
      throw std::runtime_error("AST JSON borrowed array initialization "
                               "reference has no exact active binding");
  }
  for (SgNode *child : node->get_traversalSuccessorContainer())
    if (child != nullptr)
      requireBorrowedBindings(child, nodes);
}

} // namespace

void InlineExpressionConstruction::DeleteLiteralTokens::operator()(
    SgCxxLiteralTokenList *tokens) const {
  tokens->set_parent(nullptr);
  SageInterface::deleteAST(tokens);
}

void InlineExpressionConstruction::disconnect(SgNode *node) const {
  if (auto *type = isSgTemplateType(node)) {
    auto &arguments = type->get_tpl_args();
    arguments.erase(std::remove_if(arguments.begin(), arguments.end(),
                                   [this, node](SgTemplateArgument *argument) {
                                     if (!owns(argument) &&
                                         !borrowed_parents_.count(argument))
                                       return false;
                                     if (argument->get_parent() == node)
                                       argument->set_parent(nullptr);
                                     return true;
                                   }),
                    arguments.end());
  }
  class Disconnect : public SimpleReferenceToPointerHandler {
    const InlineExpressionConstruction &construction_;
    SgNode *owner_;

  public:
    Disconnect(const InlineExpressionConstruction &construction, SgNode *owner)
        : construction_(construction), owner_(owner) {}
    void operator()(SgNode *&edge, const SgName &, bool) override {
      // Cache retirement requires the type's original semantic signature.
      if (isSgType(owner_) && isSgType(edge))
        return;
      if (construction_.owns(edge) ||
          construction_.borrowed_parents_.count(edge)) {
        if (edge->get_parent() == owner_)
          edge->set_parent(nullptr);
        edge = nullptr;
      }
    }
  } disconnect(*this, node);
  node->processDataMemberReferenceToPointers(&disconnect);
}

SgNode *InlineExpressionConstruction::own(SgNode *node) {
  ROSE_ASSERT(node != nullptr && !owns(node));
  auto delete_node = [](SgNode *node) {
    if (isSgType(node))
      SageInterface::deleteAST(node,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    else
      delete node;
  };
  std::unique_ptr<SgNode, decltype(delete_node)> pending(node, delete_node);
  try {
    owned_.push_back(node);
  } catch (...) {
    // The new owner may already have adopted tracked or borrowed operands.
    // Do not let its destructor consume those on enrollment failure.
    disconnect(node);
    throw;
  }
  return pending.release();
}

SgCxxLiteralTokenList *
InlineExpressionConstruction::ownTokens(SgCxxLiteralTokenList *tokens) {
  std::unique_ptr<SgCxxLiteralTokenList, DeleteLiteralTokens> pending(tokens);
  tokens_.push_back(std::move(pending));
  return tokens;
}

void InlineExpressionConstruction::transferTo(
    InlineExpressionConstruction &owner) noexcept {
  ROSE_ASSERT(&owner != this);
  for (SgNode *node : owned_)
    ROSE_ASSERT(!owner.owns(node));
  owner.owned_.splice(owner.owned_.end(), owned_);
  owner.tokens_.splice(owner.tokens_.end(), tokens_);
  // An enclosing scope keeps the parent observed before any private child
  // was bound. A nested borrow must not replace that original parent.
  owner.borrowed_parents_.merge(borrowed_parents_);
  borrowed_parents_.clear();
}

void InlineExpressionConstruction::forgetDeletedNodes(
    const std::unordered_set<SgNode *> &nodes) {
  owned_.remove_if([&nodes](SgNode *node) { return nodes.count(node) != 0; });
  for (auto token = tokens_.begin(); token != tokens_.end();) {
    if (nodes.count(token->get()) != 0) {
      token->release();
      token = tokens_.erase(token);
    } else {
      ++token;
    }
  }
  for (auto entry = borrowed_parents_.begin();
       entry != borrowed_parents_.end();) {
    if (nodes.count(entry->first) != 0 || nodes.count(entry->second) != 0)
      entry = borrowed_parents_.erase(entry);
    else
      ++entry;
  }
}

void InlineExpressionConstruction::publish() {
  for (auto &tokens : tokens_)
    tokens.release();
  tokens_.clear();
  owned_.clear();
  borrowed_parents_.clear();
}

void InlineExpressionConstruction::detachTypeShapeBorrows() {
  class DetachBorrowed : public SimpleReferenceToPointerHandler {
    const std::map<SgNode *, SgNode *> &borrowed_;

  public:
    explicit DetachBorrowed(const std::map<SgNode *, SgNode *> &borrowed)
        : borrowed_(borrowed) {}
    void operator()(SgNode *&edge, const SgName &, bool) override {
      if (borrowed_.count(edge) != 0)
        edge = nullptr;
    }
  } detach(borrowed_parents_);
  for (SgNode *node : owned_)
    node->processDataMemberReferenceToPointers(&detach);
  for (const auto &entry : borrowed_parents_)
    entry.first->set_parent(entry.second);
  borrowed_parents_.clear();
}

InlineExpressionConstruction::~InlineExpressionConstruction() {
  // Disconnect private expression edges before deleting the forest. This
  // includes completed siblings that were never attached and prevents a
  // generated destructor from following a borrowed NodeMap reference.
  for (SgNode *node : owned_)
    disconnect(node);
  for (const auto &entry : borrowed_parents_)
    entry.first->set_parent(entry.second);
  // Delete disconnected private expressions individually.
  for (SgNode *node : owned_)
    if (isSgType(node) == nullptr)
      delete node;
  // Retire exact derived caches while their fresh bases are still alive.
  // Private expression edges have already been disconnected above.
  for (auto node = owned_.rbegin(); node != owned_.rend(); ++node)
    if (SgNode::isLiveNode(*node) && isSgType(*node) != nullptr)
      SageInterface::deleteAST(*node,
                               SageInterface::DeleteAstMode::kRequireIsolated);
}

// Keep fresh types and their operands private until the enclosing
// expression or complete type description has succeeded.
SourceTypeConstruction *&SourceTypeConstruction::current() {
  static thread_local SourceTypeConstruction *scope = nullptr;
  return scope;
}
SourceTypeConstruction::SourceTypeConstruction() : previous_(current()) {
  current() = this;
}

namespace {
template <class Predicate>
void retireTypeDeserializationIdentities(Predicate deleted) {
  auto retire_buckets = [&deleted](auto &identities) {
    for (auto entry = identities.begin(); entry != identities.end();) {
      auto &bucket = entry->second;
      bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                                  [&deleted](const auto &identity) {
                                    return deleted(identity.type);
                                  }),
                   bucket.end());
      if (bucket.empty())
        entry = identities.erase(entry);
      else
        ++entry;
    }
  };
  retire_buckets(templateTypeDeserializationIdentities);
  retire_buckets(functionTypeDeserializationIdentities);
  auto retire_entries = [&deleted](auto &identities) {
    for (auto entry = identities.begin(); entry != identities.end();) {
      if (deleted(entry->second.type))
        entry = identities.erase(entry);
      else
        ++entry;
    }
  };
  retire_entries(sourceTypeDeserializationIdentities);
  retire_entries(arrayTypeDeserializationIdentities);
  for (auto entry = pointerMemberTypeDeserializationIdentities.begin();
       entry != pointerMemberTypeDeserializationIdentities.end();) {
    if (deleted(entry->second.type)) {
      pointerMemberTypeDeserializationReverseIdentities.erase(
          entry->second.type);
      entry = pointerMemberTypeDeserializationIdentities.erase(entry);
    } else {
      ++entry;
    }
  }
}
} // namespace

SourceTypeConstruction::~SourceTypeConstruction() {
  for (uint64_t identity : identities_)
    sourceTypeDeserializationIdentities.erase(identity);
  arrays_.transferTo(ownership_);
  if (!ownership_.empty())
    retireTypeDeserializationIdentities(
        [this](SgType *type) { return ownership_.owns(type); });
  current() = previous_;
}

void SourceTypeConstruction::take(InlineExpressionConstruction &nodes) {
  nodes.transferTo(ownership_);
}

SgType *SourceTypeConstruction::own(SgType *type) {
  return static_cast<SgType *>(ownership_.own(type));
}

void SourceTypeConstruction::recordIdentity(uint64_t identity,
                                            const JsonValue &json,
                                            SgType *type) {
  ROSE_ASSERT(ownership_.owns(type));
  identities_.reserve(identities_.size() + 1);
  if (!sourceTypeDeserializationIdentities
           .emplace(identity, SourceTypeDeserializationIdentity{json, type})
           .second)
    throw std::runtime_error(
        "AST JSON source type identity was published more than once");
  identities_.push_back(identity);
}

void SourceTypeConstruction::preparePublish() {
  arrays_.preparePublish();
  if (previous_ != nullptr)
    previous_->identities_.reserve(previous_->identities_.size() +
                                   identities_.size());
}

void SourceTypeConstruction::publish() {
  preparePublish();
  if (previous_ != nullptr) {
    ownership_.transferTo(previous_->ownership_);
    previous_->identities_.insert(previous_->identities_.end(),
                                  identities_.begin(), identities_.end());
  }
  arrays_.publish();
  if (previous_ == nullptr) {
    ownership_.publish();
  }
  identities_.clear();
}

SgType *SourceTypeConstruction::publish(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  publish();
  return type;
}

void SourceTypeConstruction::discardAst(SgNode *root) {
  SageInterface::deleteAST(std::vector<SgNode *>{root},
                           SageInterface::DeleteAstMode::kRequireIsolated,
                           forgetDeletedNodes);
}

void SourceTypeConstruction::discardDetachedNode(SgNode *root) {
  std::vector<SgNode *> pending{root};
  std::unordered_set<SgNode *> nodes;
  for (size_t i = 0; i < pending.size(); ++i) {
    SgNode *node = pending[i];
    ROSE_ASSERT(node != nullptr && SgNode::isLiveNode(node));
    if (nodes.insert(node).second)
      node->appendDestructorOwnedNodes(pending);
  }
  forgetDeletedNodes(nodes);
  delete root;
}

void SourceTypeConstruction::forgetDeletedNodes(
    const std::unordered_set<SgNode *> &nodes) {
  for (auto *scope = current(); scope != nullptr; scope = scope->previous_) {
    scope->ownership_.forgetDeletedNodes(nodes);
    auto &identities = scope->identities_;
    identities.erase(
        std::remove_if(identities.begin(), identities.end(),
                       [&nodes](uint64_t id) {
                         auto entry =
                             sourceTypeDeserializationIdentities.find(id);
                         return entry !=
                                    sourceTypeDeserializationIdentities.end() &&
                                nodes.count(entry->second.type) != 0;
                       }),
        identities.end());
  }
  retireTypeDeserializationIdentities(
      [&nodes](SgType *type) { return nodes.count(type) != 0; });
  for (SgNode *node : nodes) {
    if (auto *type = isSgArrayType(node)) {
      const auto owner = pendingSemanticArrays.find(type);
      if (owner != pendingSemanticArrays.end())
        owner->second->forgetDeletedNodes(nodes);
    }
  }
}

namespace {

// Reference identity uses the exact base and category without asking a private
// enclosing declaration for its unfinished mangled signature.
SgType *buildCachedJsonReferenceType(SgType *base, bool rvalue) {
  ROSE_ASSERT(base != nullptr && SourceTypeConstruction::current() != nullptr);
  if (rvalue) {
    SgRvalueReferenceType *reference = base->get_rvalue_ref_to();
    if (reference == nullptr) {
      reference = static_cast<SgRvalueReferenceType *>(
          SourceTypeConstruction::current()->own(
              new SgRvalueReferenceType(base)));
      base->set_rvalue_ref_to(reference);
    }
    if (reference->get_base_type() != base ||
        base->get_rvalue_ref_to() != reference)
      throw std::runtime_error(
          "AST JSON rvalue-reference cache does not preserve its exact base");
    return reference;
  }
  SgReferenceType *reference = base->get_ref_to();
  if (reference == nullptr) {
    reference = static_cast<SgReferenceType *>(
        SourceTypeConstruction::current()->own(new SgReferenceType(base)));
    base->set_ref_to(reference);
  }
  if (reference->get_base_type() != base || base->get_ref_to() != reference)
    throw std::runtime_error(
        "AST JSON lvalue-reference cache does not preserve its exact base");
  return reference;
}

SgPointerMemberType *
restorePointerMemberTypeIdentity(SgType *base, SgType *class_type,
                                 const JsonValue &json,
                                 const PointerMemberSourceFields &fields) {
  if (base == nullptr || class_type == nullptr) {
    throw std::runtime_error(
        "AST JSON pointer-member type has no exact base/class identity");
  }

  const bool semantic_canonical = fields.canonical;
  const uint64_t serialized_identity =
      requiredPointerMemberTypeSerializationIdentity(json);
  if (pointerMemberTypeDeserializationIdentities.count(serialized_identity) !=
      0) {
    throw std::runtime_error(
        "AST JSON pointer-member identity was rebuilt before its existing "
        "type graph was reused");
  }

  ROSE_ASSERT(SourceTypeConstruction::current() != nullptr);
  SgPointerMemberType *member_pointer =
      semantic_canonical
          ? SgPointerMemberType::findCanonicalSemanticType(base, class_type)
          : nullptr;
  if (member_pointer == nullptr) {
    member_pointer = static_cast<SgPointerMemberType *>(
        SourceTypeConstruction::current()->own(
            semantic_canonical
                ? SgPointerMemberType::createType(base, class_type)
                : new SgPointerMemberType(base, class_type)));
  }
  if (member_pointer == nullptr || SgPointerMemberType::isCanonicalSemanticType(
                                       member_pointer) != semantic_canonical) {
    throw std::runtime_error(
        "AST JSON pointer-member semantic/source identity was not restored "
        "exactly");
  }
  const bool fortran_source_syntax = fields.source_syntax;
  if (semantic_canonical &&
      (fortran_source_syntax || member_pointer->get_fortran_source_syntax())) {
    throw std::runtime_error(
        "AST JSON canonical semantic pointer-member type has source syntax");
  }
  if (!semantic_canonical) {
    member_pointer->set_fortran_source_syntax(fortran_source_syntax);
    member_pointer->set_source_base_type_global_qualification(fields.global);
    member_pointer->get_source_base_type_qualification_tokens() = fields.tokens;
    member_pointer->set_source_base_type_qualification_present(fields.present);
    member_pointer->set_source_class_type_is_unqualified_injected_name(
        fields.injected);
  }
  if (SgPointerMemberType::isCanonicalSemanticType(member_pointer) !=
      semantic_canonical) {
    throw std::runtime_error(
        "AST JSON pointer-member source qualifiers changed its serialized "
        "semantic/source identity");
  }
  auto existing_pointer =
      pointerMemberTypeDeserializationReverseIdentities.find(member_pointer);
  if (existing_pointer !=
          pointerMemberTypeDeserializationReverseIdentities.end() &&
      existing_pointer->second != serialized_identity) {
    throw std::runtime_error(
        "AST JSON distinct pointer-member identities restore the same exact "
        "type object");
  }
  PointerMemberTypeDeserializationIdentity identity;
  identity.type = member_pointer;
  identity.base_type = base;
  identity.class_type = class_type;
  identity.serialized_type = json;
  attachPointerMemberJsonIdentity(member_pointer, serialized_identity);
  if (!pointerMemberTypeDeserializationIdentities
           .emplace(serialized_identity, std::move(identity))
           .second ||
      !pointerMemberTypeDeserializationReverseIdentities
           .emplace(member_pointer, serialized_identity)
           .second) {
    throw std::runtime_error(
        "AST JSON pointer-member identity was published more than once");
  }
  return member_pointer;
}

} // namespace

struct SemanticArrayConstruction::State {
  struct Array {
    SgArrayType *type;
    std::shared_ptr<InlineExpressionConstruction> shape;
  };
  SemanticArrayConstruction *parent = activeSemanticArrays;
  std::vector<Array> arrays;
  InlineExpressionConstruction detached_shapes;
};

SemanticArrayConstruction::SemanticArrayConstruction()
    : state_(std::make_unique<State>()) {}

SemanticArrayConstruction::~SemanticArrayConstruction() {
  InlineExpressionConstruction ownership;
  transferTo(ownership);
}

void SemanticArrayConstruction::prepareRollback() {
  state_->detached_shapes.detachTypeShapeBorrows();
  for (const auto &array : state_->arrays)
    array.shape->detachTypeShapeBorrows();
}

void SemanticArrayConstruction::transferTo(
    InlineExpressionConstruction &ownership) {
  state_->detached_shapes.transferTo(ownership);
  while (!state_->arrays.empty()) {
    auto &array = state_->arrays.back();
    array.shape->transferTo(ownership);
    pendingSemanticArrays.erase(array.type);
    for (auto entry = arrayTypeDeserializationIdentities.begin();
         entry != arrayTypeDeserializationIdentities.end();) {
      if (entry->second.type == array.type)
        entry = arrayTypeDeserializationIdentities.erase(entry);
      else
        ++entry;
    }
    state_->arrays.pop_back();
  }
}

void SemanticArrayConstruction::take(
    SgArrayType *type, std::shared_ptr<InlineExpressionConstruction> shape) {
  state_->arrays.push_back({type, std::move(shape)});
  try {
    ROSE_ASSERT(pendingSemanticArrays.emplace(type, this).second);
  } catch (...) {
    state_->arrays.pop_back();
    throw;
  }
}

void SemanticArrayConstruction::forgetDeletedNodes(
    const std::unordered_set<SgNode *> &nodes) {
  state_->detached_shapes.forgetDeletedNodes(nodes);
  for (auto array = state_->arrays.begin(); array != state_->arrays.end();) {
    array->shape->forgetDeletedNodes(nodes);
    if (nodes.count(array->type) != 0) {
      pendingSemanticArrays.erase(array->type);
      array->shape->transferTo(state_->detached_shapes);
      array = state_->arrays.erase(array);
    } else {
      ++array;
    }
  }
}

void SemanticArrayConstruction::preparePublish() {
  if (state_->parent != nullptr) {
    auto &parent = state_->parent->state_->arrays;
    parent.reserve(parent.size() + state_->arrays.size());
  }
}

void SemanticArrayConstruction::publish() {
  preparePublish();
  if (state_->parent != nullptr) {
    state_->detached_shapes.transferTo(state_->parent->state_->detached_shapes);
    auto &parent = state_->parent->state_->arrays;
    for (auto &array : state_->arrays) {
      pendingSemanticArrays.at(array.type) = state_->parent;
      parent.push_back(std::move(array));
    }
  } else {
    state_->detached_shapes.publish();
    for (auto &array : state_->arrays) {
      pendingSemanticArrays.erase(array.type);
      array.shape->publish();
    }
  }
  state_->arrays.clear();
}

SemanticArrayConstruction *SemanticArrayConstruction::current() {
  return activeSemanticArrays;
}

bool SemanticArrayConstruction::owns(SgArrayType *type) const {
  return std::any_of(
             state_->arrays.begin(), state_->arrays.end(),
             [type](const auto &array) { return array.type == type; }) ||
         (state_->parent != nullptr && state_->parent->owns(type));
}

SemanticArrayConstructionScope::SemanticArrayConstructionScope(
    SemanticArrayConstruction *owner)
    : previous_(activeSemanticArrays) {
  if (owner != nullptr)
    activeSemanticArrays = owner;
}

SemanticArrayConstructionScope::~SemanticArrayConstructionScope() {
  activeSemanticArrays = previous_;
}

bool semanticArrayDependenciesReady(const JsonValue &json,
                                    const SemanticArrayConstruction *owner) {
  if (json.kind == JsonValue::Kind::Array)
    return std::all_of(json.array.begin(), json.array.end(),
                       [owner](const auto &value) {
                         return semanticArrayDependenciesReady(value, owner);
                       });
  if (json.kind != JsonValue::Kind::Object)
    return true;
  const auto *kind = json.find("kind");
  const auto *present = json.find("present");
  const auto *source = json.find("fortran_source_syntax");
  const auto *identity_json = json.find("semantic_array_identity");
  if (kind != nullptr && kind->kind == JsonValue::Kind::String &&
      kind->asString() == "SgArrayType" && present != nullptr &&
      present->kind == JsonValue::Kind::Bool && present->bool_value &&
      source != nullptr && source->kind == JsonValue::Kind::Bool &&
      !source->bool_value && identity_json != nullptr) {
    const auto identity =
        requiredPositiveId(*identity_json, "semantic array identity");
    const auto found = arrayTypeDeserializationIdentities.find(identity);
    if (found != arrayTypeDeserializationIdentities.end() &&
        pendingSemanticArrays.count(found->second.type) != 0 &&
        (owner == nullptr || !owner->owns(found->second.type)))
      return false;
  }
  return std::all_of(
      json.object.begin(), json.object.end(), [owner](const auto &field) {
        return semanticArrayDependenciesReady(field.second, owner);
      });
}

static SgExpression *
originalExpressionFromJson(const JsonValue &json, const NodeMap &nodes,
                           InlineExpressionConstruction &ownership) {
  SgExpression *source =
      expressionFromRef(json.at("original_expression"), nodes, ownership);
  if (source != nullptr && (source->get_parent() != nullptr ||
                            source->get_originalExpressionTree() != nullptr))
    throw std::runtime_error(
        "AST JSON original expression requires one unowned source tree");
  return source;
}

static SgExprListExp *
exprListExpFromTypeJson(const JsonValue &json, const NodeMap &nodes,
                        InlineExpressionConstruction &ownership);

static SgExpressionPtrList
expressionListChildrenFromJson(const JsonValue &expressions,
                               const NodeMap &nodes,
                               InlineExpressionConstruction &ownership) {
  if (expressions.kind != JsonValue::Kind::Array)
    throw std::runtime_error("AST JSON expression list is not an array");
  SgExpressionPtrList children;
  children.reserve(expressions.array.size());
  for (const JsonValue &input : expressions.array) {
    auto *child = expressionFromRef(input, nodes, ownership);
    if (child == nullptr)
      throw std::runtime_error(
          "AST JSON expression list contains a null expression");
    children.push_back(child);
  }
  std::unordered_set<SgExpression *> identities;
  for (auto *child : children)
    if (child->get_parent() != nullptr || !identities.insert(child).second)
      throw std::runtime_error(
          "AST JSON expression list requires distinct unowned operands");
  return children;
}

SgType *requireUnaryTraitResultType(SgType *type, const std::string &context) {
  if (SageInterface::containsUnknownType(type)) {
    throw std::runtime_error("AST JSON " + context +
                             " has no exact semantic result type");
  }
  SgType *unqualified =
      type->stripType(SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
  if (isSgTypeUnsignedChar(unqualified) == nullptr &&
      isSgTypeUnsignedShort(unqualified) == nullptr &&
      isSgTypeUnsignedInt(unqualified) == nullptr &&
      isSgTypeUnsignedLong(unqualified) == nullptr &&
      isSgTypeUnsignedLongLong(unqualified) == nullptr) {
    throw std::runtime_error(
        "AST JSON " + context +
        " result type is not the exact unsigned target size_t type");
  }
  return type;
}

SgCastExp *castExpressionFromProperties(const JsonValue &properties,
                                        const NodeMap &nodes,
                                        SgExpression *operand, SgType *type,
                                        const CastConstructionFields &fields) {
  SgType *source_type =
      nullableTypeFromJson(properties.at("source_type"), nodes);
  const bool explicit_cast = fields.cast_type != SgCastExp::e_implicit_cast;
  if (explicit_cast != (source_type != nullptr)) {
    throw std::runtime_error(
        "AST JSON SgCastExp written type disagrees with its cast surface");
  }
  const bool qualification_present =
      properties.requiredBool("explicit_name_qualification_present");
  const bool global_qualification =
      properties.requiredBool("explicit_global_qualification");
  const SgStringList qualification_tokens =
      stringListFromJson(properties.at("explicit_name_qualification_tokens"),
                         "SgCastExp explicit_name_qualification_tokens");
  const auto elaboration_kind =
      requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
          properties, "source_type_elaboration_kind", "SgCastExp",
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
       (elaboration_kind != SgNonrealDecl::e_source_elaboration_unspecified))) {
    throw std::runtime_error(
        "AST JSON SgCastExp has contradictory written type qualification");
  }

  const JsonValue &base_path_json = properties.at("conversion_base_path");
  SgTypePtrList base_path;
  for (const JsonValue &entry : base_path_json.array) {
    base_path.push_back(typeFromJson(entry, nodes));
  }

  const bool type_elaboration_required =
      properties.requiredBool("type_elaboration_required");
  auto cast = std::make_unique<SgCastExp>(
      operand, type, SgUnaryOp::prefix, fields.cast_type,
      fields.conversion_kind, fields.value_category, base_path, source_type);
  cast->set_explicit_name_qualification_present(qualification_present);
  cast->set_explicit_global_qualification(global_qualification);
  cast->set_explicit_name_qualification_tokens(qualification_tokens);
  cast->set_source_type_elaboration_kind(elaboration_kind);
  cast->set_type_elaboration_required(type_elaboration_required);
  return cast.release();
}

ExpressionSourceFields
expressionSourceFieldsFromJson(const std::string &kind,
                               const JsonValue &properties) {
  ExpressionSourceFields fields;
  fields.arrow_role = binaryEmissionRoleFromJson(properties, kind);
  if (isJsonValueExpressionKind(kind))
    fields.spelling = literalSpellingFormFromJson(properties);
  static const VariantVector initializers(V_SgInitializer);
  if (kindInVariants(kind, initializers))
    fields.braced = properties.requiredBool("is_braced_initialized");
  return fields;
}

ExpressionConstructionFields
expressionConstructionFieldsFromJson(const JsonValue &properties) {
  const bool available =
      properties.requiredBool("fortran_integer_constant_value_is_available");
  const std::int64_t value =
      properties.requiredInt("fortran_integer_constant_value");
  if (!available && value != 0)
    throw std::runtime_error(
        "AST JSON expression has a Fortran folded selector value without "
        "availability");
  const bool lvalue = properties.requiredBool("lvalue");
  const bool need_paren = properties.requiredBool("need_paren");
  const bool global_qualified =
      properties.requiredBool("global_qualified_name");
  const auto wrappers = static_cast<SgExpression::semantic_wrapper_mask_enum>(
      properties.requiredIntInRange("semantic_wrapper_mask", 0,
                                    SgExpression::e_all_semantic_wrappers));
  uint64_t destructor = 0;
  if ((wrappers & SgExpression::e_bound_temporary) != 0) {
    const int64_t id = properties.requiredInt("bound_temporary_destructor");
    if (id <= 0)
      throw std::runtime_error(
          "AST JSON bound temporary requires its selected destructor");
    destructor = static_cast<uint64_t>(id);
  } else if (properties.find("bound_temporary_destructor") != nullptr) {
    throw std::runtime_error(
        "AST JSON destructor binding has no bound temporary");
  }
  auto storage = SgExpression::e_temporary_storage_none;
  uint64_t owner = 0;
  if ((wrappers & SgExpression::e_materialized_temporary) != 0) {
    storage = static_cast<SgExpression::temporary_storage_duration_enum>(
        properties.requiredIntInRange(
            "materialized_storage_duration",
            SgExpression::e_temporary_storage_full_expression,
            SgExpression::e_temporary_storage_containing_object));
    owner = properties.requiredNonnegativeNodeId("materialized_lifetime_owner");
    if ((storage == SgExpression::e_temporary_storage_full_expression) !=
        (owner == 0))
      throw std::runtime_error(
          "AST JSON materialization storage has no matching lifetime owner");
  } else if (properties.find("materialized_storage_duration") != nullptr ||
             properties.find("materialized_lifetime_owner") != nullptr) {
    throw std::runtime_error(
        "AST JSON materialization data has no materialized temporary");
  }
  return {available, value,      lvalue,  need_paren, global_qualified,
          wrappers,  destructor, storage, owner,      std::nullopt,
          {}};
}

void validateExpressionLifetime(const ExpressionConstructionFields &fields,
                                const JsonValue &properties,
                                const NodeMap &nodes) {
  if (fields.bound_temporary_destructor != 0 &&
      !SgExpression::is_valid_bound_temporary_destructor(
          typeFromJson(properties.at("type"), nodes),
          nodeByIdAs<SgMemberFunctionDeclaration>(
              nodes, fields.bound_temporary_destructor)))
    throw std::runtime_error(
        "AST JSON bound temporary has no exact typed destructor");
  if (!SgExpression::is_valid_temporary_materialization(
          fields.materialized_storage_duration,
          fields.materialized_lifetime_owner != 0
              ? nodeByIdAs<SgInitializedName>(
                    nodes, fields.materialized_lifetime_owner)
              : nullptr))
    throw std::runtime_error(
        "AST JSON materialized temporary has no exact lifetime owner");
}

void ExpressionConstructionFields::initialize(SgExpression *expression,
                                              const NodeMap &nodes) const {
  ASSERT_not_null(expression);
  const bool qualified =
      isSgVarRefExp(expression) || isSgFunctionRefExp(expression) ||
      isSgTemplateFunctionRefExp(expression) ||
      isSgMemberFunctionRefExp(expression) ||
      isSgTemplateMemberFunctionRefExp(expression) ||
      isSgNonrealRefExp(expression) || isSgEnumVal(expression) ||
      isSgNewExp(expression) || isSgPseudoDestructorRefExp(expression) ||
      isSgTypeExpression(expression);
  ROSE_ASSERT(qualification.has_value() == qualified);
  ROSE_ASSERT(source.spelling.has_value() ==
              (isSgValueExp(expression) != nullptr));
  ROSE_ASSERT(source.arrow_role.has_value() ==
              (isSgArrowExp(expression) != nullptr));
  ROSE_ASSERT(source.braced.has_value() ==
              (isSgInitializer(expression) != nullptr));
  if (auto *value = isSgValueExp(expression)) {
    if (value->get_literal_spelling_form() != *source.spelling)
      throw std::runtime_error(
          "AST JSON literal lost its constructed spelling form");
  }
  if (auto *arrow = isSgArrowExp(expression)) {
    if (arrow->get_emission_role() != *source.arrow_role)
      throw std::runtime_error(
          "AST JSON arrow lost its constructed emission role");
  }
  if (auto *initializer = isSgInitializer(expression)) {
    if (initializer->get_is_braced_initialized() != *source.braced)
      throw std::runtime_error(
          "AST JSON initializer lost its constructed brace form");
  }
  expression->set_lvalue(lvalue);
  expression->set_need_paren(need_paren);
  expression->set_global_qualified_name(global_qualified);
  expression->set_semantic_wrapper_mask(
      wrappers,
      bound_temporary_destructor != 0 ? nodeByIdAs<SgMemberFunctionDeclaration>(
                                            nodes, bound_temporary_destructor)
                                      : nullptr,
      materialized_storage_duration,
      materialized_lifetime_owner != 0
          ? nodeByIdAs<SgInitializedName>(nodes, materialized_lifetime_owner)
          : nullptr);
  expression->set_fortran_integer_constant_value(folded_value);
  expression->set_fortran_integer_constant_value_is_available(
      folded_value_available);
  if (qualification)
    qualification->initialize(expression);
}

void initializeScalarLiteralType(SgValueExp *value, const JsonValue &properties,
                                 const NodeMap &nodes) {
  const bool has_literal_type =
      properties.requiredBool("has_literal_semantic_type");
  const auto *type = validatedExpressionTypeProperty(value, properties);
  if (type == nullptr) {
    if (has_literal_type || value->get_literal_type() != nullptr)
      throw std::runtime_error(
          "AST JSON syntax literal has a semantic literal type");
    return;
  }
  SourceTypeConstruction construction;
  SgType *restored = typeFromJson(
      *type, nodes,
      "AST JSON " + value->class_name() +
          " has no exact semantic value type during deserialization");
  if (has_literal_type)
    value->set_literal_type(restored);
  else if (value->get_literal_type() != nullptr)
    throw std::runtime_error(
        "AST JSON value expression unexpectedly owns a semantic literal type "
        "before reconstruction");
  else if (!SageInterface::isEquivalentType(value->get_type(), restored))
    throw std::runtime_error(
        "AST JSON literal type disagrees with its intrinsic type");
  construction.publish();
}

void initializeTemplateParameterTypes(SgTemplateParameter *parameter,
                                      const NodeRecord &record,
                                      const NodeMap &nodes) {
  SourceTypeConstruction construction;
  SgType *default_type = nullableTypeFromJson(
      record.properties.at("default_type_parameter"), nodes);
  if (parameter->get_parameterType() !=
      SgTemplateParameter::nontype_parameter) {
    const JsonValue &type = record.properties.at("type");
    if (record.id == 0 ||
        type.requiredNonnegativeNodeId("template_parameter") != record.id)
      throw std::runtime_error(
          "AST JSON template parameter has no exact canonical type identity");
    if (type.requiredBool("is_pack_substitution"))
      throw std::runtime_error(
          "AST JSON template parameter owns a substituted type");
    if (parameter->get_type() != nullptr)
      throw std::runtime_error(
          "AST JSON SgTemplateParameter acquired a synthetic canonical type "
          "before exact type reconstruction");
    auto *canonical_type =
        isSgTemplateType(typeFromJson(type, {{record.id, parameter}}));
    if (canonical_type == nullptr ||
        canonical_type->get_template_parameter() != parameter)
      throw std::runtime_error(
          "AST JSON SgTemplateParameter failed canonical type publication");
    parameter->set_type(canonical_type);
  }
  parameter->set_defaultTypeParameter(default_type);
  construction.publish();
}

static SgExpression *finishOwnedExpressionConstruction(
    SgNode *node, const NodeRecord &record, SgExpression *source,
    const NodeConstructionFlags &flags, PreparedAstAttributes attributes,
    PreparedPreprocessingInfo preprocessing) {
  auto *expr = isSgExpression(node);
  if (expr == nullptr)
    throw std::runtime_error(
        "AST JSON type-owned record did not rebuild an expression");
  flags.initialize(expr);
  attachPreprocessingInfo(expr, std::move(preprocessing));
  attachAstAttributes(expr, std::move(attributes));
  if (source != nullptr) {
    if (source == expr || source->get_parent() != nullptr)
      throw std::runtime_error(
          "AST JSON original expression aliases another owned operand");
    ROSE_ASSERT(expr->get_originalExpressionTree() == nullptr);
    expr->set_originalExpressionTree(source);
    source->set_parent(expr);
  }
  const JsonValue &properties = record.properties;
  (void)validatedExpressionTypeProperty(expr, properties);
  if (expressionCarriesSemanticType(expr))
    validateExactSemanticExpressionType(expr, expr->get_type(),
                                        "deserialization");
  // Only the exactly constructed unary family is validated here. Casts keep
  // their own conversion contract; unmigrated unary subclasses cannot be
  // produced by any reader and never reach this boundary.
  SgUnaryOp *unary = isSgUnaryOp(expr);
  if (unary != nullptr && isSgCastExp(unary) == nullptr &&
      isJsonUnaryOpKind(record.kind)) {
    if (properties.find("operand") == nullptr)
      throw std::runtime_error(
          "AST JSON type-owned unary expression is missing its operand");
    SgExpression *operand = unary->get_operand_i();
    const bool rethrow =
        isSgThrowOp(unary) != nullptr &&
        isSgThrowOp(unary)->get_throwKind() == SgThrowOp::rethrow;
    if ((operand == nullptr) != rethrow ||
        (operand != nullptr && operand->get_parent() != unary))
      throw std::runtime_error(
          "AST JSON type-owned unary operand differs from construction");
  }
  if (SgBinaryOp *binary = isSgBinaryOp(expr)) {
    SgExpression *lhs = binary->get_lhs_operand_i();
    SgExpression *rhs = binary->get_rhs_operand_i();
    if (lhs == nullptr || rhs == nullptr || lhs == rhs ||
        lhs->get_parent() != binary || rhs->get_parent() != binary)
      throw std::runtime_error(
          "AST JSON type-owned binary operands differ from construction");
  }
  if (auto *choice = isSgChooseExpression(expr))
    choice->validate();
  if (SgAwaitExpression *await_expression = isSgAwaitExpression(expr)) {
    if (await_expression->get_value() == nullptr ||
        await_expression->get_value()->get_parent() != await_expression) {
      throw std::runtime_error(
          "AST JSON type-owned SgAwaitExpression lost its constructed operand");
    }
  }
  if (SgFoldExpression *fold = isSgFoldExpression(expr)) {
    if (fold->get_operands() == nullptr ||
        fold->get_operands()->get_parent() != fold) {
      throw std::runtime_error(
          "AST JSON type-owned SgFoldExpression lost its constructed operands");
    }
  }
  if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(expr)) {
    if (pack->get_pattern_expression() == nullptr ||
        pack->get_pattern_expression()->get_parent() != pack) {
      throw std::runtime_error("AST JSON type-owned SgPackExpansionExpr lost "
                               "its constructed pattern");
    }
  }
  if (SgNoexceptOp *noexcept_op = isSgNoexceptOp(expr)) {
    if (noexcept_op->get_operand_expr() == nullptr ||
        noexcept_op->get_operand_expr()->get_parent() != noexcept_op) {
      throw std::runtime_error(
          "AST JSON noexcept lost its constructed operand");
    }
  }

  if (SgSizeOfOp *size_of = isSgSizeOfOp(expr)) {
    (void)size_of->get_type();
  }

  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    const JsonValue *symbol_json = properties.find("symbol");
    if (symbol_json == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgVarRefExp has no exact symbol reference");
    }
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve type-owned SgVarRefExp symbol");
    }
    validateAnonymousDataMemberReferenceQualification(ref);
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    cast->validate_semantic_conversion();
  }
  return expr;
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes,
                                InlineExpressionConstruction &ownership);

static SgComplexVal *
complexValueFromProperties(const JsonValue &properties, const NodeMap &nodes,
                           InlineExpressionConstruction &ownership) {
  const auto spelling = literalSpellingFormFromJson(properties);
  const JsonValue &precision_json = properties.at("precision_type");
  const JsonValue &type_json = properties.at("type");
  if (!type_json.requiredBool("present") ||
      type_json.requiredString("kind") != "SgTypeComplex" ||
      type_json.requiredBool("fortran_source_syntax"))
    throw std::runtime_error(
        "AST JSON complex literal type does not match its precision");
  const JsonValue &component_json = type_json.at("base");
  const std::string component_kind = component_json.requiredString("kind");
  if (!component_json.requiredBool("present") ||
      component_json.requiredBool("fortran_source_syntax") ||
      component_kind == "SgTypeUnknown" || component_kind == "SgTypeDefault")
    throw std::runtime_error(
        "AST JSON complex literal type does not match its precision");
  const std::string precision_kind = precision_json.requiredString("kind");
  if (!precision_json.requiredBool("present") ||
      precision_json.requiredBool("fortran_source_syntax") ||
      precision_kind == "SgTypeUnknown" || precision_kind == "SgTypeDefault")
    throw std::runtime_error(
        "AST JSON SgComplexVal requires an exact precision type");
  const bool has_literal_type =
      properties.requiredBool("has_literal_semantic_type");
  const JsonValue &real_json = properties.at("real_value");
  const JsonValue &imaginary_json = properties.at("imaginary_value");
  SgType *precision = typeFromJson(precision_json, nodes);
  if (typeFromJson(component_json, nodes) != precision)
    throw std::runtime_error(
        "AST JSON complex literal type does not match its precision");
  SgType *type = typeFromJson(type_json, nodes);
  SgTypeComplex *complex_type = isSgTypeComplex(type);
  if (complex_type == nullptr || complex_type->get_base_type() != precision ||
      complex_type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON complex literal type does not match its precision");
  }
  SgExpression *real = expressionFromRef(real_json, nodes, ownership);
  SgExpression *imaginary = expressionFromRef(imaginary_json, nodes, ownership);
  if ((real != nullptr && real->get_parent() != nullptr) ||
      (imaginary != nullptr && imaginary->get_parent() != nullptr) ||
      (real != nullptr && real == imaginary))
    throw std::runtime_error("AST JSON SgComplexVal requires "
                             "distinct unowned components");
  auto *value = new SgComplexVal(nullptr, real, imaginary, precision);
  ownership.own(value);
  value->set_literal_spelling_form(spelling);
  if (has_literal_type)
    value->set_literal_type(type);
  return value;
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes,
                                InlineExpressionConstruction &ownership) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON expression reference is not an object");
  }
  if (json.find("node") == nullptr) {
    throw std::runtime_error("AST JSON expression reference is missing node");
  }
  const int64_t node_id = json.requiredInt("node");
  if (node_id < 0)
    throw std::runtime_error(
        "AST JSON expression reference has a negative node ID");
  const uint64_t id = static_cast<uint64_t>(node_id);
  const JsonValue *owned_kind = json.find("owned_kind");
  if ((id != 0 || owned_kind == nullptr) && json.object.size() != 1)
    throw std::runtime_error(
        "AST JSON reference contains inline expression fields");
  if (id != 0) {
    auto *expression = nodeByIdAs<SgExpression>(nodes, id);
    if (!inlineArrayInitBindings.empty() || inlineOpaqueDepth != 0)
      requireBorrowedBindings(expression, nodes);
    return ownership.borrow(expression);
  }
  if (owned_kind != nullptr) {
    NodeRecord record;
    record.kind = owned_kind->asString();
    if (record.kind == "SgMemberFunctionRefExp" ||
        record.kind == "SgTemplateMemberFunctionRefExp")
      for (const char *field : {"virtual_call", "need_qualifier"})
        (void)requiredIntegerBoolean(json.at("properties"), field);
    static const VariantVector expression_variants(V_SgExpression);
    if (!kindInVariants(record.kind, expression_variants))
      throw std::runtime_error(
          "AST JSON type-owned kind is not an expression: " + record.kind);
    record.flags = json.at("flags");
    const auto flags = validateConstructionFlags(record);
    SgNode::SourceStateConstruction construction;
    record.location = json.at("location");
    record.properties = json.at("properties");
    std::optional<SgChooseExpression::selection_kind_enum> choose_selection;
    if (record.kind == "SgChooseExpression") {
      choose_selection = chooseSelectionFromJson(record.properties);
      for (const char *field :
           {"condition", "true_expression", "false_expression"})
        (void)record.properties.at(field);
    }
    (void)validatedExpressionTypeProperty(record);
    if (record.kind == "SgVarRefExp" || record.kind == "SgNonrealRefExp")
      (void)record.properties.requiredBool("is_non_odr_use");
    auto qualification =
        expressionQualificationFieldsFromJson(record.kind, record.properties);
    auto fields = expressionConstructionFieldsFromJson(record.properties);
    validateExpressionLifetime(fields, record.properties, nodes);
    fields.qualification = std::move(qualification);
    fields.source =
        expressionSourceFieldsFromJson(record.kind, record.properties);
    const auto arrow_role = fields.source.arrow_role;
    const auto result_storage_size_bytes =
        binaryStorageSizeFromJson(record.properties, record.kind);
    const auto subscript_strides =
        binarySubscriptStridesFromJson(record.properties, record.kind);
    record.preprocessing = record.properties.at("preprocessing");
    auto attributes = prepareAstAttributes(record);
    auto preprocessing = preparePreprocessingInfo(record);
    const auto position_fields = validateSourcePositionRecord(record);
    SgExpression *source = originalExpressionFromJson(json, nodes, ownership);
    auto conversion_operand = [&]() {
      SgExpression *child = expressionFromRef(
          record.properties.at("conversion_type_id"), nodes, ownership);
      auto *type_id = isSgTypeExpression(child);
      if (child != type_id)
        throw std::runtime_error(
            "AST JSON conversion operand is not a type expression");
      return type_id;
    };
    SgNode *node = nullptr;
    if (record.kind == "SgExprListExp") {
      const auto form = requiredEnum<SgExprListExp::source_form_enum>(
          record.properties, "expression_list_source_form", "SgExprListExp",
          {SgExprListExp::e_expression_list_plain,
           SgExprListExp::e_expression_list_braced});
      const auto children = expressionListChildrenFromJson(
          json.at("expressions"), nodes, ownership);
      auto *list = new SgExprListExp;
      ownership.own(list);
      list->set_source_form(form);
      list->get_expressions().reserve(children.size());
      for (auto *child : children)
        list->append_expression(child);
      node = list;
    } else if (record.kind == "SgOmpInitModifier") {
      const auto kind = initModifierKindFromJson(record.properties);
      auto *expression = expressionFromRef(record.properties.at("expression"),
                                           nodes, ownership);
      if ((expression != nullptr && expression->get_parent() != nullptr) ||
          !Rose::OpenMP::Detail::validateInitModifierExpression(
              kind, expression, nullptr))
        throw std::runtime_error(
            "AST JSON inline init modifier has invalid typed payload");
      node = ownership.own(new SgOmpInitModifier(kind, expression));
      if (expression != nullptr)
        expression->set_parent(node);
    } else if (record.kind == "SgOmpInitModifierList") {
      const auto expressions = expressionListChildrenFromJson(
          record.properties.at("modifiers"), nodes, ownership);
      SgOmpInitModifierPtrList modifiers;
      for (auto *expression : expressions) {
        auto *modifier = isSgOmpInitModifier(expression);
        if (!Rose::OpenMP::Detail::validateInitModifierPayload(
                modifier, nullptr, nullptr))
          throw std::runtime_error(
              "AST JSON inline modifier list has invalid typed child");
        modifiers.push_back(modifier);
      }
      auto *list = new SgOmpInitModifierList();
      ownership.own(list);
      list->get_modifiers() = std::move(modifiers);
      for (auto *modifier : list->get_modifiers())
        modifier->set_parent(list);
      node = list;
    } else if (record.kind == "SgOmpAppendArgsOperation") {
      auto *modifiers = isSgOmpInitModifierList(expressionFromRef(
          record.properties.at("modifier_list"), nodes, ownership));
      if (!Rose::OpenMP::Detail::validateInitModifierList(
              modifiers, nullptr,
              Rose::OpenMP::Detail::InitModifierContext::AppendArgs, nullptr))
        throw std::runtime_error(
            "AST JSON inline append_args operation has invalid modifiers");
      node = ownership.own(new SgOmpAppendArgsOperation(modifiers));
      modifiers->set_parent(node);
    } else if (record.kind == "SgCxxDecompositionRefExp") {
      auto *owner = nodeByIdAs<SgInitializedName>(
          nodes, record.properties.requiredNonnegativeNodeId("decomposition"));
      auto *type = typeFromJson(record.properties.at("type"), nodes);
      if (!SgCxxDecompositionRefExp::is_valid_target(owner, type))
        throw std::runtime_error(
            "AST JSON inline decomposition reference has invalid storage");
      node = ownership.own(new SgCxxDecompositionRefExp(owner, type));
    } else if (record.kind == "SgVarRefExp") {
      node = ownership.own(
          variableReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgCompoundLiteralExp") {
      node = ownership.own(
          compoundLiteralFromProperties(record.properties, nodes));
    } else if (record.kind == "SgLabelRefExp") {
      node =
          ownership.own(labelReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgFunctionRefExp") {
      node = ownership.own(
          functionReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgMemberFunctionRefExp") {
      node = ownership.own(memberFunctionReferenceFromProperties(
          record.properties, nodes, conversion_operand()));
    } else if (record.kind == "SgTemplateFunctionRefExp") {
      node = ownership.own(
          templateFunctionReferenceFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTemplateMemberFunctionRefExp") {
      node = ownership.own(templateMemberFunctionReferenceFromProperties(
          record.properties, nodes, conversion_operand()));
    } else if (record.kind == "SgNonrealRefExp") {
      node = ownership.own(nonrealReferenceFromProperties(
          record.properties, nodes, conversion_operand()));
    } else if (record.kind == "SgConstructorInitializer") {
      const bool elidable = constructorElisionFromProperties(record.properties);
      if (constructorArgumentSourceFromProperties(record.properties) !=
          SgConstructorInitializer::e_constructor_argument_expressions)
        throw std::runtime_error(
            "AST JSON inherited constructor argument context is invalid");
      const int64_t declaration_id =
          record.properties.requiredInt("declaration");
      if (declaration_id < 0)
        throw std::runtime_error(
            "AST JSON type-owned SgConstructorInitializer has incomplete "
            "construction input");
      const JsonValue &argument_json = record.properties.at("args");
      if (elidable &&
          (declaration_id <= 0 || !argument_json.requiredBool("present") ||
           argument_json.at("expressions").kind != JsonValue::Kind::Array ||
           argument_json.at("expressions").array.empty()))
        throw std::runtime_error(
            "AST JSON elidable constructor requires its selected declaration "
            "and one owned argument");
      if (elidable) {
        const auto is_default_reference = [&](const JsonValue &reference) {
          const int64_t id = reference.requiredInt("node");
          if (id < 0)
            throw std::runtime_error(
                "AST JSON expression reference has a negative node ID");
          if (id != 0)
            return isSgCxxDefaultInitExp(nodeByIdAs<SgExpression>(
                       nodes, static_cast<uint64_t>(id))) != nullptr;
          const auto *kind = reference.find("owned_kind");
          return kind != nullptr && kind->asString() == "SgCxxDefaultInitExp";
        };
        const auto &operands = argument_json.at("expressions").array;
        if (is_default_reference(operands.front()) ||
            !std::all_of(operands.begin() + 1, operands.end(),
                         is_default_reference))
          throw std::runtime_error(
              "AST JSON elidable constructor requires its selected declaration "
              "and one owned argument");
      }
      SgMemberFunctionDeclaration *declaration =
          declaration_id != 0
              ? nodeByIdAs<SgMemberFunctionDeclaration>(
                    nodes, static_cast<uint64_t>(declaration_id))
              : nullptr;
      if (declaration != nullptr &&
          declaration->get_functionModifier().isDeleted())
        throw std::runtime_error(
            "AST JSON constructor selects a deleted declaration");
      if (elidable &&
          (!declaration->get_specialFunctionModifier().isConstructor() ||
           isSgClassDefinition(declaration->get_scope()) == nullptr))
        throw std::runtime_error(
            "AST JSON elidable constructor has no selected constructor");
      if (elidable) {
        const auto role = declaration->get_functionModifier().getCopyMoveKind();
        if (role != SgFunctionModifier::e_copy_constructor &&
            role != SgFunctionModifier::e_move_constructor)
          throw std::runtime_error("AST JSON elidable constructor has no "
                                   "selected copy/move constructor");
      }
      SgExprListExp *arguments =
          exprListExpFromTypeJson(argument_json, nodes, ownership);
      SgType *destination_type =
          typeFromJson(record.properties.at("type"), nodes);
      if (arguments == nullptr || arguments->get_parent() != nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned SgConstructorInitializer has incomplete "
            "construction input");
      }
      node = ownership.own(constructorInitializerFromProperties(
          record.properties, declaration, arguments,
          arguments->get_expressions(), destination_type));
    } else if (record.kind == "SgAssignInitializer") {
      const auto source =
          assignmentInitializerSourceFieldsFromJson(record.properties);
      SgExpression *operand =
          expressionFromRef(record.properties.at("operand"), nodes, ownership);
      SgType *destination_type =
          typeFromJson(record.properties.at("type"), nodes);
      if (operand == nullptr || operand->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON type-owned SgAssignInitializer requires an exact "
            "unowned operand and destination type");
      auto *initializer = new SgAssignInitializer(operand, destination_type);
      initializer->set_source_form(source.source_form);
      initializer->set_is_braced_initialized(source.braced);
      node = ownership.own(initializer);
    } else if (record.kind == "SgEnumVal") {
      node = ownership.own(enumValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgNullptrValExp") {
      node =
          ownership.own(nullptrValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTemplateParameterVal") {
      node = ownership.own(
          templateParameterValueFromProperties(record.properties, nodes));
    } else if (record.kind == "SgThisExp") {
      node =
          ownership.own(thisExpressionFromProperties(record.properties, nodes));
    } else if (record.kind == "SgDeleteExp") {
      requireExpressionType(record.properties, nodes,
                            SageBuilder::buildVoidType(), "delete expression");
      const int64_t is_array = record.properties.requiredInt("is_array");
      const int64_t global =
          record.properties.requiredInt("need_global_specifier");
      const int64_t declaration_id =
          record.properties.requiredInt("delete_operator_declaration");
      if ((is_array != 0 && is_array != 1) || (global != 0 && global != 1) ||
          declaration_id < 0)
        throw std::runtime_error(
            "AST JSON type-owned SgDeleteExp has invalid construction fields");
      auto *declaration =
          declaration_id == 0
              ? nullptr
              : nodeByIdAs<SgFunctionDeclaration>(
                    nodes, static_cast<uint64_t>(declaration_id));
      SgExpression *operand =
          expressionFromRef(record.properties.at("variable"), nodes, ownership);
      if (operand == nullptr || operand->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON type-owned SgDeleteExp requires an exclusive operand");
      auto *expression =
          new SgDeleteExp(operand, static_cast<short>(is_array),
                          static_cast<short>(global), declaration);
      ownership.own(expression);
      operand->set_parent(expression);
      node = expression;
    } else if (record.kind == "SgNewExp") {
      const auto source_form =
          newArrayBoundSourceFormFromJson(record.properties);
      const auto &bound_json = record.properties.at("array_bound");
      const bool bound_absent = bound_json.requiredInt("node") == 0 &&
                                bound_json.find("owned_kind") == nullptr;
      if (bound_absent != (source_form == SgNewExp::e_no_array_bound))
        throw std::runtime_error(
            "AST JSON new-expression array bound and source form disagree");
      const auto fields =
          newConstructionFieldsFromJson(record.properties, nodes);
      const int64_t declaration_id =
          record.properties.requiredInt("new_operator_declaration");
      if (declaration_id < 0)
        throw std::runtime_error(
            "AST JSON new-expression has a negative operator declaration");
      auto *declaration =
          declaration_id == 0
              ? nullptr
              : nodeByIdAs<SgFunctionDeclaration>(
                    nodes, static_cast<uint64_t>(declaration_id));
      auto *array_bound = expressionFromRef(bound_json, nodes, ownership);
      auto *placement = exprListExpFromTypeJson(
          record.properties.at("placement_args"), nodes, ownership);
      auto *constructor_expression = expressionFromRef(
          record.properties.at("constructor_args"), nodes, ownership);
      auto *constructor = isSgConstructorInitializer(constructor_expression);
      if (constructor_expression != nullptr && constructor == nullptr)
        throw std::runtime_error("AST JSON new-expression constructor_args is "
                                 "not a constructor initializer");
      auto *builtin = expressionFromRef(record.properties.at("builtin_args"),
                                        nodes, ownership);
      node = ownership.own(constructNewExpression(
          fields, array_bound, placement, constructor, builtin, declaration,
          placement != nullptr ? placement->get_expressions()
                               : SgExpressionPtrList{}));
    } else if (record.kind == "SgSubscriptExpression" ||
               record.kind == "SgRangeExp") {
      const bool range = record.kind == "SgRangeExp";
      SgExpression *lower = expressionFromRef(
          record.properties.at(range ? "start" : "lower_bound"), nodes,
          ownership);
      SgExpression *upper =
          expressionFromRef(record.properties.at(range ? "end" : "upper_bound"),
                            nodes, ownership);
      SgExpression *stride =
          expressionFromRef(record.properties.at("stride"), nodes, ownership);
      if ((range &&
           (lower == nullptr || upper == nullptr || stride == nullptr)) ||
          (lower != nullptr && (lower->get_parent() != nullptr ||
                                lower == upper || lower == stride)) ||
          (upper != nullptr &&
           (upper->get_parent() != nullptr || upper == stride)) ||
          (stride != nullptr && stride->get_parent() != nullptr))
        throw std::runtime_error(
            "AST JSON type-owned bounds are not distinct unowned "
            "expressions");
      SgExpression *bounds =
          range ? static_cast<SgExpression *>(
                      new SgRangeExp(lower, upper, stride))
                : new SgSubscriptExpression(lower, upper, stride);
      ownership.own(bounds);
      for (SgExpression *child : {lower, upper, stride})
        if (child != nullptr)
          child->set_parent(bounds);
      node = bounds;
    } else if (record.kind == "SgAggregateInitializer") {
      const auto fields = aggregateConstructionFieldsFromJson(
          record.properties, nodes,
          aggregateSourceElementRolesFromJson(record.properties));
      auto *list = exprListExpFromTypeJson(record.properties.at("initializers"),
                                           nodes, ownership);
      if (list == nullptr)
        throw std::runtime_error(
            "AST JSON aggregate requires an initializer list");
      node = ownership.own(
          constructAggregateInitializer(fields, list, list->get_expressions()));
    } else if (record.kind == "SgFunctionCallExp") {
      const auto fields = functionCallSourceFieldsFromJson(record.properties);
      SgType *type = typeFromJson(
          record.properties.at("type"), nodes,
          "AST JSON call requires an exact non-callable result type");
      SgExpression *function =
          expressionFromRef(record.properties.at("function"), nodes, ownership);
      SgExprListExp *arguments = exprListExpFromTypeJson(
          record.properties.at("args"), nodes, ownership);
      SgCxxLiteralTokenList *tokens = nullptr;
      if (fields.surface == SgFunctionCallExp::e_user_defined_literal_surface)
        tokens = ownership.ownTokens(typeOwnedLiteralTokensFromJson(
            record.properties.at("source_user_defined_literal_operands")));
      else if (record.properties.find("source_user_defined_literal_operands") !=
               nullptr)
        throw std::runtime_error(
            "AST JSON non-UDL call contains literal operands");
      node = ownership.own(
          constructFunctionCall(fields, function, arguments, type, tokens));
    } else if (record.kind == "SgChooseExpression") {
      ROSE_ASSERT(choose_selection.has_value());
      const auto &condition_ref = record.properties.at("condition");
      const auto &true_ref = record.properties.at("true_expression");
      const auto &false_ref = record.properties.at("false_expression");
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      auto *condition = expressionFromRef(condition_ref, nodes, ownership);
      auto *true_expression = expressionFromRef(true_ref, nodes, ownership);
      auto *false_expression = expressionFromRef(false_ref, nodes, ownership);
      node = ownership.own(constructChooseExpression(type, *choose_selection,
                                                     condition, true_expression,
                                                     false_expression));
    } else if (record.kind == "SgConditionalExp") {
      const auto kind = conditionalOperatorKindFromJson(record.properties);
      SgExpression *condition = expressionFromRef(
          record.properties.at("conditional_exp"), nodes, ownership);
      SgExpression *true_expression =
          expressionFromRef(record.properties.at("true_exp"), nodes, ownership);
      SgExpression *false_expression = expressionFromRef(
          record.properties.at("false_exp"), nodes, ownership);
      node = ownership.own(
          conditionalFromProperties(record.properties, nodes, kind, condition,
                                    true_expression, false_expression));
    } else if (record.kind == "SgTypeTraitBuiltinOperator") {
      const JsonValue &args = record.properties.at("args");
      if (args.kind != JsonValue::Kind::Array)
        throw std::runtime_error(
            "AST JSON type-trait operands are not an array");
      SgExpressionPtrList arguments;
      for (const JsonValue &argument : args.array)
        arguments.push_back(expressionFromRef(argument, nodes, ownership));
      node = ownership.own(
          typeTraitBuiltinFromProperties(record.properties, nodes, arguments));
    } else if (record.kind == "SgSourceLocationBuiltinExp") {
      node = ownership.own(
          sourceLocationBuiltinFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTypeIdOp") {
      const bool evaluated =
          record.properties.requiredBool("operand_is_evaluated");
      if (evaluated &&
          record.properties.at("operand_type").requiredBool("present"))
        throw std::runtime_error(
            "AST JSON evaluated typeid requires an expression operand");
      SgExpression *operand = expressionFromRef(
          record.properties.at("operand_expr"), nodes, ownership);
      SgType *operand_type =
          nullableTypeFromJson(record.properties.at("operand_type"), nodes);
      node = ownership.own(typeIdFromProperties(
          record.properties, nodes, operand, operand_type, evaluated));
    } else if (record.kind == "SgNoexceptOp" || record.kind == "SgVarArgOp") {
      SgExpression *operand = expressionFromRef(
          record.properties.at("operand_expr"), nodes, ownership);
      if (record.kind == "SgNoexceptOp")
        node = ownership.own(
            noexceptFromProperties(record.properties, nodes, operand));
      else
        node = ownership.own(
            varArgFromProperties(record.properties, nodes, operand));
    } else if (record.kind == "SgPackExpansionExpr") {
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *pattern = expressionFromRef(
          record.properties.at("pattern_expression"), nodes, ownership);
      if (pattern == nullptr || pattern->get_parent() != nullptr) {
        throw std::runtime_error("AST JSON type-owned SgPackExpansionExpr "
                                 "requires an exclusive pattern");
      }
      auto *pack = new SgPackExpansionExpr(pattern, type);
      ownership.own(pack);
      node = pack;
    } else if (record.kind == "SgAwaitExpression") {
      const auto keyword = coroutineKeywordKindFromJson(record.properties);
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *operand =
          expressionFromRef(record.properties.at("value"), nodes, ownership);
      if (operand == nullptr || operand->get_parent() != nullptr) {
        throw std::runtime_error("AST JSON type-owned SgAwaitExpression "
                                 "requires an exclusive operand");
      }
      auto *await_expression = new SgAwaitExpression(operand, type, keyword);
      ownership.own(await_expression);
      node = await_expression;
    } else if (record.kind == "SgFoldExpression") {
      const auto fields = foldConstructionFieldsFromJson(record.properties);
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *operands =
          expressionFromRef(record.properties.at("operands"), nodes, ownership);
      if (operands == nullptr || operands->get_parent() != nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned SgFoldExpression requires exclusive operands");
      }
      auto *fold = new SgFoldExpression(operands, fields.operator_kind,
                                        fields.is_left_associative, type);
      ownership.own(fold);
      node = fold;
    } else if (record.kind == "SgSizeOfOp" || record.kind == "SgAlignOfOp") {
      const auto operator_kind = unaryTraitKindFromProperties(record);
      const auto source =
          unaryTraitSourceFieldsFromJson(record.properties, record.kind);
      const bool objectless_member =
          record.kind == "SgSizeOfOp" &&
          record.properties.requiredBool(
              "is_objectless_nonstatic_data_member_reference");
      SgType *result_type = requireUnaryTraitResultType(
          typeFromJson(record.properties.at("type"), nodes), record.kind);
      SgType *operand_type =
          nullableTypeFromJson(record.properties.at("operand_type"), nodes);
      SgExpression *operand = expressionFromRef(
          record.properties.at("operand_expr"), nodes, ownership);
      if ((operand == nullptr) == (operand_type == nullptr) ||
          (operand != nullptr && operand->get_parent() != nullptr)) {
        throw std::runtime_error(
            "AST JSON type-owned unary trait requires one exclusive operand");
      }
      if (record.kind == "SgSizeOfOp") {
        auto *size_of = new SgSizeOfOp(operand, operand_type, result_type,
                                       operator_kind, source.evaluated_value);
        size_of->set_type_elaboration_required(source.elaboration);
        size_of->set_source_type_elaboration_kind(source.kind);
        size_of->set_is_objectless_nonstatic_data_member_reference(
            objectless_member);
        node = ownership.own(size_of);
      } else {
        auto *align_of = new SgAlignOfOp(operand, operand_type, result_type,
                                         operator_kind, source.evaluated_value);
        align_of->set_type_elaboration_required(source.elaboration);
        align_of->set_source_type_elaboration_kind(source.kind);
        node = ownership.own(align_of);
      }
      if (operand != nullptr)
        operand->set_parent(node);
    } else if (record.kind == "SgCastExp") {
      const auto fields = castConstructionFieldsFromJson(record.properties);
      SgType *destination = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *operand =
          expressionFromRef(record.properties.at("operand"), nodes, ownership);
      if (operand == nullptr || operand->get_parent() != nullptr)
        throw std::runtime_error("AST JSON type-owned SgCastExp requires an "
                                 "exclusive constructor-time operand");
      auto *cast = castExpressionFromProperties(record.properties, nodes,
                                                operand, destination, fields);
      ownership.own(cast);
      operand->set_parent(cast);
      node = cast;
    } else if (record.kind == "SgComplexVal") {
      node = complexValueFromProperties(record.properties, nodes, ownership);
    } else if (record.kind == "SgSimpleRequirement" ||
               record.kind == "SgCompoundRequirement" ||
               record.kind == "SgNestedRequirement") {
      const bool compound = record.kind == "SgCompoundRequirement";
      const bool nested = record.kind == "SgNestedRequirement";
      const bool noexcept_required =
          compound && record.properties.requiredBool("noexcept_required");
      SgExpression *operand = expressionFromRef(
          record.properties.at(nested ? "constraint" : "expression"), nodes,
          ownership);
      SgExpression *constraint =
          compound ? expressionFromRef(record.properties.at("type_constraint"),
                                       nodes, ownership)
                   : nullptr;
      node = ownership.own(constructOperandRequirement(
          compound ? OperandRequirementKind::compound
          : nested ? OperandRequirementKind::nested
                   : OperandRequirementKind::simple,
          operand, constraint, noexcept_required));
    } else if (record.kind == "SgTypeRequirement") {
      node = ownership.own(
          typeRequirementFromProperties(record.properties, nodes));
    } else if (record.kind == "SgTypeExpression") {
      SgType *represented_type =
          typeFromJson(record.properties.at("represented_type"), nodes);
      if (represented_type->has_source_prototype() &&
          represented_type->get_parent() != nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned SgTypeExpression requires an exact "
            "constructor-time represented type");
      }
      ownership.borrow(represented_type);
      node = ownership.own(new SgTypeExpression(represented_type));
    } else if (record.kind == "SgAsmOp") {
      const auto fields = asmOperandFieldsFromJson(record.properties);
      auto *expression = expressionFromRef(record.properties.at("expression"),
                                           nodes, ownership);
      node = ownership.own(constructAsmOperand(fields, expression));
    } else if (record.kind == "SgOpaqueValueBinding") {
      const auto fields = opaqueValueFieldsFromJson(record.properties);
      if (inlineOpaqueDepth == 0)
        throw std::runtime_error("AST JSON inline opaque binding has no owner");
      auto *type = typeFromJson(record.properties.at("type"), nodes);
      auto *source = expressionFromRef(
          record.properties.at("source_expression"), nodes, ownership);
      if (!SgOpaqueValueBinding::is_valid_binding(source, type, fields.category,
                                                  fields.object_kind) ||
          source->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON opaque binding has invalid owned source");
      auto *binding = new SgOpaqueValueBinding(source, type, fields.category,
                                               fields.object_kind);
      node = ownership.own(binding);
      inlineOpaqueBindings.push_back(binding);
    } else if (record.kind == "SgOpaqueValueReference") {
      auto *binding = opaqueBindingFromJson(
          record.properties.at("binding_reference"), nodes);
      if (!SgOpaqueValueReference::is_valid_reference(binding) ||
          record.properties.requiredBool("lvalue") != binding->get_lvalue())
        throw std::runtime_error(
            "AST JSON opaque reference has invalid binding");
      requireExpressionType(record.properties, nodes, binding->get_type(),
                            "opaque reference");
      node = ownership.own(new SgOpaqueValueReference(binding));
    } else if (record.kind == "SgPseudoObjectExp") {
      const auto fields = opaqueValueFieldsFromJson(record.properties);
      auto *type = typeFromJson(record.properties.at("type"), nodes);
      const auto &capture_json = record.properties.at("captured_bindings");
      if (capture_json.kind != JsonValue::Kind::Array)
        throw std::runtime_error(
            "AST JSON pseudo-object captures are not an array");
      SgOpaqueValueBindingPtrList captures;
      for (const auto &capture : capture_json.array)
        captures.push_back(opaqueBindingFromJson(capture, nodes));
      auto *written = expressionFromRef(
          record.properties.at("written_expression"), nodes, ownership);
      OpaqueBindingScope scope(inlineOpaqueBindings, inlineOpaqueDepth);
      auto *evaluations = isSgExprListExp(expressionFromRef(
          record.properties.at("semantic_evaluations"), nodes, ownership));
      if (written == nullptr || evaluations == nullptr ||
          written->get_parent() != nullptr ||
          evaluations->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON pseudo-object requires exclusive operands");
      SgExpression *result = nullptr;
      const auto &result_json = record.properties.at("result_index");
      if (result_json.kind != JsonValue::Kind::Null) {
        const int64_t index = result_json.asInt();
        const auto &steps = evaluations->get_expressions();
        if (index < 0 || static_cast<uint64_t>(index) >= steps.size())
          throw std::runtime_error(
              "AST JSON pseudo-object result index is out of range");
        result = steps[index];
      }
      if (!SgPseudoObjectExp::is_valid_sequence(written, evaluations, result,
                                                captures, type, fields.category,
                                                fields.object_kind))
        throw std::runtime_error(
            "AST JSON pseudo-object has invalid evaluation sequence");
      node = ownership.own(
          new SgPseudoObjectExp(written, evaluations, result, captures, type,
                                fields.category, fields.object_kind));
    } else if (record.kind == "SgCxxArrayInitLoop") {
      const uint64_t count =
          unsigned64ValueFromJson(record.properties.at("element_count"));
      if (record.properties.requiredBool("lvalue"))
        throw std::runtime_error(
            "AST JSON array initialization loop is not a prvalue");
      const auto &input = record.properties.at("binding");
      const auto category = arrayInitBindingCategoryFromJson(input);
      NodeRecord binding_record;
      binding_record.kind = "SgCxxArrayInitBinding";
      binding_record.flags = input.at("flags");
      const auto binding_flags = validateConstructionFlags(binding_record);
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      if (!SgCxxArrayInitLoop::is_valid_extent(type, count))
        throw std::runtime_error("AST JSON array initialization count "
                                 "disagrees with its result type");
      SgExpression *source =
          expressionFromRef(input.at("common_source"), nodes, ownership);
      if (!SgCxxArrayInitBinding::is_valid_source(source, category) ||
          source->get_parent() != nullptr)
        throw std::runtime_error("AST JSON array initialization binding "
                                 "requires an unowned common source");
      auto *binding = static_cast<SgCxxArrayInitBinding *>(
          ownership.own(new SgCxxArrayInitBinding(source, category)));
      binding_flags.initialize(binding);
      ArrayInitBindingScope scope(inlineArrayInitBindings, binding);
      SgExpression *element = expressionFromRef(
          record.properties.at("element_initializer"), nodes, ownership);
      if (!SgCxxArrayInitLoop::is_valid_loop(binding, element, type, count) ||
          element->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON array initialization loop has invalid typed operands");
      node =
          ownership.own(new SgCxxArrayInitLoop(binding, element, type, count));
    } else if (record.kind == "SgCxxDefaultInitExp") {
      const auto kind = defaultInitializerKindFromJson(record.properties);
      auto *type = typeFromJson(record.properties.at("type"), nodes);
      auto *parameter = defaultInitializerDeclarationFromJson(
          record.properties.at("source_declaration"), nodes);
      auto *expression = expressionFromRef(record.properties.at("expression"),
                                           nodes, ownership);
      if (!SgCxxDefaultInitExp::is_valid_use(parameter, expression, kind) ||
          expression->get_parent() != nullptr ||
          expression->get_lvalue() !=
              record.properties.requiredBool("lvalue") ||
          !SageInterface::isEquivalentType(expression->get_type(), type))
        throw std::runtime_error(
            "AST JSON default argument has invalid parameter or expansion");
      node =
          ownership.own(new SgCxxDefaultInitExp(parameter, expression, kind));
    } else if (record.kind == "SgCxxArrayInitReference") {
      const auto fields = arrayInitReferenceFieldsFromJson(record.properties);
      const auto *depth = record.properties.find("binding_depth");
      const auto *id = record.properties.find("binding");
      if ((depth == nullptr) == (id == nullptr))
        throw std::runtime_error("AST JSON array initialization reference "
                                 "requires exactly one binding identity");
      SgCxxArrayInitBinding *binding = nullptr;
      if (depth != nullptr) {
        const int64_t index = depth->asInt();
        if (index < 0 ||
            static_cast<uint64_t>(index) >= inlineArrayInitBindings.size())
          throw std::runtime_error("AST JSON array initialization reference "
                                   "has no enclosing binding");
        binding =
            inlineArrayInitBindings[inlineArrayInitBindings.size() - 1 - index];
      } else {
        binding = nodeByIdAs<SgCxxArrayInitBinding>(
            nodes, requiredPositiveId(*id, "array initialization binding"));
        if (std::find(inlineArrayInitBindings.begin(),
                      inlineArrayInitBindings.end(),
                      binding) != inlineArrayInitBindings.end())
          throw std::runtime_error("AST JSON inline array initialization "
                                   "binding requires lexical identity");
      }
      if (fields.kind == SgCxxArrayInitReference::e_current_index &&
          !inlineArrayInitBindings.empty() &&
          inlineArrayInitBindings.back() != binding)
        throw std::runtime_error("AST JSON array initialization index does not "
                                 "name the innermost binding");
      SgType *type = typeFromJson(record.properties.at("type"), nodes);
      if (!SgCxxArrayInitReference::is_valid_reference(binding, fields.kind,
                                                       type, fields.category))
        throw std::runtime_error(
            "AST JSON array initialization reference has invalid typed input");
      node = ownership.own(new SgCxxArrayInitReference(binding, fields.kind,
                                                       type, fields.category));
    } else if (record.kind == "SgMacroExpansionExp") {
      const std::string spelling = record.properties.requiredString("spelling");
      const bool modified =
          record.properties.requiredBool("expansion_is_modified");
      if (spelling.empty())
        throw std::runtime_error(
            "AST JSON SgMacroExpansionExp has empty spelling");
      SgExpression *expanded = expressionFromRef(
          record.properties.at("expanded_expression"), nodes, ownership);
      if (expanded == nullptr || expanded->get_parent() != nullptr)
        throw std::runtime_error(
            "AST JSON type-owned SgMacroExpansionExp requires an exclusive "
            "expanded_expression");
      requireExpressionType(record.properties, nodes, expanded->get_type(),
                            "macro expansion");
      auto *macro = new SgMacroExpansionExp(spelling, expanded, modified);
      ownership.own(macro);
      node = macro;
    } else if (isJsonBinaryOpKind(record.kind)) {
      SgType *result_type = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *lhs = expressionFromRef(record.properties.at("lhs_operand"),
                                            nodes, ownership);
      if (lhs == nullptr || lhs->get_parent() != nullptr)
        throw std::runtime_error("AST JSON type-owned " + record.kind +
                                 " requires distinct unowned operands");
      SgExpression *rhs = expressionFromRef(record.properties.at("rhs_operand"),
                                            nodes, ownership);
      if (rhs == nullptr || rhs == lhs || rhs->get_parent() != nullptr)
        throw std::runtime_error("AST JSON type-owned " + record.kind +
                                 " requires distinct unowned operands");
      node = ownership.own(
          constructBinaryOp(record.kind, lhs, rhs, result_type, arrow_role,
                            result_storage_size_bytes, subscript_strides));
    } else if (isJsonUnaryOpKind(record.kind)) {
      const auto fields = unaryConstructionFieldsFromJson(record.properties,
                                                          record.kind, nodes);
      SgType *result_type = typeFromJson(record.properties.at("type"), nodes);
      SgExpression *operand =
          expressionFromRef(record.properties.at("operand"), nodes, ownership);
      if ((operand == nullptr) != (record.kind == "SgThrowOp" &&
                                   fields.throw_kind == SgThrowOp::rethrow) ||
          (operand != nullptr && operand->get_parent() != nullptr))
        throw std::runtime_error("AST JSON type-owned " + record.kind +
                                 " requires an exclusive matching operand");
      node = ownership.own(
          constructUnaryOp(record.kind, operand, fields, result_type));
    } else {
      return finishOwnedExpressionConstruction(
          ownership.own(
              createNodeFromRecord(record, nullptr, nodes,
                                   constructSourcePositions(position_fields))),
          record, source, flags, std::move(attributes),
          std::move(preprocessing));
    }
    SgExpression *expr = isSgExpression(node);
    ROSE_ASSERT(expr != nullptr);
    fields.initialize(expr, nodes);
    installSourcePositions(expr, constructSourcePositions(position_fields),
                           record.kind);
    return finishOwnedExpressionConstruction(expr, record, source, flags,
                                             std::move(attributes),
                                             std::move(preprocessing));
  }
  return nullptr;
}

SgExprListExp *
exprListExpFromTypeJson(const JsonValue &json, const NodeMap &nodes,
                        InlineExpressionConstruction &ownership) {
  if (!json.requiredBool("present")) {
    return nullptr;
  }
  SgNode::SourceStateConstruction construction;
  const std::string kind = json.at("kind").asString();
  if (kind != "SgExprListExp") {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info is not SgExprListExp: " + kind);
  }

  NodeRecord record;
  record.kind = kind;
  record.flags = json.at("flags");
  const auto flags = validateConstructionFlags(record);
  record.location = json.at("location");
  record.properties = json.at("properties");
  const auto form = requiredEnum<SgExprListExp::source_form_enum>(
      record.properties, "expression_list_source_form", "SgExprListExp",
      {SgExprListExp::e_expression_list_plain,
       SgExprListExp::e_expression_list_braced});
  (void)validatedExpressionTypeProperty(record);
  const auto fields = expressionConstructionFieldsFromJson(record.properties);
  validateExpressionLifetime(fields, record.properties, nodes);
  record.preprocessing = record.properties.at("preprocessing");
  auto attributes = prepareAstAttributes(record);
  auto preprocessing = preparePreprocessingInfo(record);
  const auto position_fields = validateSourcePositionRecord(record);
  SgExpression *source = originalExpressionFromJson(json, nodes, ownership);
  const auto children =
      expressionListChildrenFromJson(json.at("expressions"), nodes, ownership);

  auto *list = new SgExprListExp;
  ownership.own(list);
  list->set_source_form(form);
  list->get_expressions().reserve(children.size());
  for (SgExpression *child : children)
    list->append_expression(child);
  fields.initialize(list, nodes);
  installSourcePositions(list, constructSourcePositions(position_fields),
                         record.kind);
  finishOwnedExpressionConstruction(list, record, source, flags,
                                    std::move(attributes),
                                    std::move(preprocessing));
  return list;
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  SourceTypeConstruction types;
  InlineExpressionConstruction ownership;
  SgExpression *expression = expressionFromRef(json, nodes, ownership);
  types.publish();
  ownership.publish();
  return expression;
}

SgComplexVal *complexValueFromProperties(const JsonValue &properties,
                                         const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  SourceTypeConstruction types;
  InlineExpressionConstruction ownership;
  SgComplexVal *value =
      complexValueFromProperties(properties, nodes, ownership);
  types.publish();
  ownership.publish();
  return value;
}

SgExprListExp *exprListExpFromTypeJson(const JsonValue &json,
                                       const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  SourceTypeConstruction types;
  InlineExpressionConstruction ownership;
  SgExprListExp *list = exprListExpFromTypeJson(json, nodes, ownership);
  types.publish();
  ownership.publish();
  return list;
}

bool sameAstJsonPath(const std::string &lhs, const std::string &rhs) {
  return !lhs.empty() && lhs == rhs;
}

bool sourceFileMatchesExternalRecord(SgSourceFile *file,
                                     const std::string &source_file) {
  if (file == nullptr || source_file.empty()) {
    return false;
  }
  return sameAstJsonPath(file->getFileName(), source_file) ||
         sameAstJsonPath(file->get_sourceFileNameWithPath(), source_file);
}

std::vector<SgSourceFile *> currentDeserializationSourceFiles() {
  std::vector<SgSourceFile *> sources;
  std::unordered_set<SgSourceFile *> seen;
  if (currentDeserializationProject == nullptr) {
    return sources;
  }
  auto add_source = [&](SgSourceFile *source) {
    if (source != nullptr && seen.insert(source).second) {
      sources.push_back(source);
    }
  };
  for (SgFile *file : currentDeserializationProject->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (sourceIsBeingReplaced(source)) {
      continue;
    }
    add_source(source);
    SgFileList *external_files =
        source != nullptr ? source->get_frontendExternalFileList() : nullptr;
    if (external_files == nullptr) {
      continue;
    }
    if (external_files->get_parent() != source) {
      throw std::runtime_error(
          "AST JSON deserialization found a malformed frontend-external file "
          "owner");
    }
    for (SgFile *external_file : external_files->get_listOfFiles()) {
      SgSourceFile *external_source = isSgSourceFile(external_file);
      if (external_source == nullptr ||
          external_source->get_parent() != external_files) {
        throw std::runtime_error(
            "AST JSON deserialization found a malformed frontend-external "
            "source file");
      }
      if (!sourceIsBeingReplaced(external_source)) {
        add_source(external_source);
      }
    }
  }
  return sources;
}

const std::vector<SgClassDeclaration *> &
structuralClassDeclarationsForSource(SgSourceFile *source) {
  ROSE_ASSERT(source != nullptr);
  auto found = currentDeserializationClassDeclarationCache.find(source);
  if (found != currentDeserializationClassDeclarationCache.end()) {
    return found->second;
  }

  std::vector<SgClassDeclaration *> declarations;
  std::unordered_set<SgClassDeclaration *> seen;
  auto add_candidate = [&](SgClassDeclaration *decl) {
    if (decl != nullptr && seen.insert(decl).second) {
      declarations.push_back(decl);
    }
  };
  auto add_symbol_basis = [&](SgSymbol *symbol) {
    add_candidate(
        isSgClassDeclaration(const_cast<SgNode *>(symbolBasis(symbol))));
    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      add_candidate(isSgClassDeclaration(
          const_cast<SgNode *>(symbolBasis(alias->get_alias()))));
    }
  };

  RoseAst ast(source);
  for (RoseAst::iterator it = ast.begin(); it != ast.end(); ++it) {
    add_candidate(isSgClassDeclaration(*it));
    SgScopeStatement *scope = isSgScopeStatement(*it);
    SgSymbolTable *table =
        scope != nullptr ? scope->get_symbol_table() : nullptr;
    if (table == nullptr || table->get_table() == nullptr) {
      continue;
    }
    for (const std::pair<const SgName, SgSymbol *> &entry :
         *table->get_table()) {
      add_symbol_basis(entry.second);
    }
  }

  auto inserted = currentDeserializationClassDeclarationCache.emplace(
      source, std::move(declarations));
  ROSE_ASSERT(inserted.second);
  return inserted.first->second;
}

void installTransformationSourcePosition(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  SageInterface::setSourcePositionAsTransformation(node);
}

bool nodeSupportsRestoredSourcePosition(SgNode *node) {
  return isSgLocatedNode(node) != nullptr || isSgPragma(node) != nullptr ||
         isSgFile(node) != nullptr;
}

SourcePositionFields
requiredSourcePositionFieldsFromJson(const JsonValue &location,
                                     const std::string &context) {
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + context +
                             " location must be an object");
  }
  if (!location.at("start").requiredBool("present")) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a present startOfConstruct");
  }
  return sourcePositionFieldsFromJson(location, false);
}

SourcePositions requiredSourcePositionsFromJson(const JsonValue &location,
                                                const std::string &context) {
  return constructSourcePositions(
      requiredSourcePositionFieldsFromJson(location, context));
}

void installSourcePositions(SgNode *node, SourcePositions positions,
                            const std::string &context) {
  ASSERT_not_null(node);
  if (!nodeSupportsRestoredSourcePosition(node)) {
    if (positions.start != nullptr || positions.end != nullptr) {
      throw std::runtime_error(
          "AST JSON " + context +
          " node kind has no restorable source position: " +
          node->sage_class_name());
    }
    return;
  }
  if (isSgFile(node) != nullptr && positions.end != nullptr)
    throw std::runtime_error("AST JSON " + context +
                             " file has no endOfConstruct position");

  // Constructor-owned positions are replaced as one transaction. Keep every
  // old position until all fields have changed, then retire each identity once.
  std::set<Sg_File_Info *> previous;
  auto record_previous = [&](Sg_File_Info *position) {
    if (position == nullptr)
      return;
    ROSE_ASSERT(SgNode::isLiveNode(position));
    ROSE_ASSERT(position->get_parent() == node);
    previous.insert(position);
  };
  for (Sg_File_Info *position : {positions.start.get(), positions.end.get(),
                                 positions.operator_position.get()})
    if (position != nullptr)
      position->set_parent(node);
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    record_previous(located->get_startOfConstruct());
    record_previous(located->get_endOfConstruct());
    if (SgExpression *expression = isSgExpression(node))
      record_previous(expression->get_operatorPosition());
    located->set_startOfConstruct(positions.start.release());
    located->set_endOfConstruct(positions.end.release());
    if (SgExpression *expression = isSgExpression(node))
      expression->set_operatorPosition(positions.operator_position.release());
  } else if (SgPragma *pragma = isSgPragma(node)) {
    record_previous(pragma->get_startOfConstruct());
    record_previous(pragma->get_endOfConstruct());
    pragma->set_startOfConstruct(positions.start.release());
    pragma->set_endOfConstruct(positions.end.release());
  } else if (SgFile *file = isSgFile(node)) {
    record_previous(file->get_startOfConstruct());
    file->set_startOfConstruct(positions.start.release());
  }
  for (Sg_File_Info *position : previous)
    delete position;
}

TypeModifierFields typeModifierFieldsFromJson(const JsonValue &json,
                                              const std::string &context,
                                              const std::string &prefix) {
  auto bits = bitVectorFromJson(json.at(prefix + "_modifier_vector"),
                                context + " " + prefix + "_modifier_vector",
                                SgTypeModifier::e_last_modifier);
  if (bits[SgTypeModifier::e_extended_vector_type] &&
      bits[SgTypeModifier::e_vector_type__])
    throw std::runtime_error("AST JSON " + context +
                             " has conflicting vector kinds");
  const auto const_volatile =
      requiredEnum<SgConstVolatileModifier::cv_modifier_enum>(
          json, prefix + "_const_volatile_modifier", context,
          {SgConstVolatileModifier::e_unknown,
           SgConstVolatileModifier::e_default, SgConstVolatileModifier::e_const,
           SgConstVolatileModifier::e_volatile,
           SgConstVolatileModifier::e_const_volatile});
  const auto elaborated =
      requiredEnum<SgElaboratedTypeModifier::elaborated_type_modifier_enum>(
          json, prefix + "_elaborated_modifier", context,
          {SgElaboratedTypeModifier::e_unknown,
           SgElaboratedTypeModifier::e_default,
           SgElaboratedTypeModifier::e_class,
           SgElaboratedTypeModifier::e_struct,
           SgElaboratedTypeModifier::e_union, SgElaboratedTypeModifier::e_enum,
           SgElaboratedTypeModifier::e_typename});

  const int64_t alignment =
      json.requiredInt(prefix + "_gnu_attribute_alignment");
  const int64_t sentinel = json.requiredInt(prefix + "_gnu_attribute_sentinel");
  const int64_t address_space =
      json.requiredInt(prefix + "_address_space_value");
  const int64_t vector_size = json.requiredInt(prefix + "_vector_size");
  if (alignment < std::numeric_limits<int>::min() ||
      alignment > std::numeric_limits<int>::max() ||
      sentinel < std::numeric_limits<long>::min() ||
      sentinel > std::numeric_limits<long>::max() || address_space < 0 ||
      static_cast<uint64_t>(address_space) >
          std::numeric_limits<unsigned>::max() ||
      vector_size < 0 ||
      ((bits[SgTypeModifier::e_vector_type__] ||
        bits[SgTypeModifier::e_extended_vector_type]) &&
       vector_size == 0) ||
      static_cast<uint64_t>(vector_size) >
          std::numeric_limits<unsigned>::max()) {
    throw std::runtime_error("AST JSON " + context +
                             " has an out-of-range type modifier payload");
  }
  return {std::move(bits),
          const_volatile,
          elaborated,
          static_cast<int>(alignment),
          static_cast<long>(sentinel),
          static_cast<unsigned>(address_space),
          static_cast<unsigned>(vector_size)};
}

void TypeModifierFields::initialize(SgTypeModifier &modifier) const {
  modifier.set_modifierVector(bits);
  modifier.get_constVolatileModifier().set_modifier(const_volatile);
  modifier.get_elaboratedTypeModifier().set_modifier(elaborated);
  modifier.set_gnu_attribute_alignment(alignment);
  modifier.set_gnu_attribute_sentinel(sentinel);
  modifier.set_address_space_value(address_space);
  modifier.set_vector_size(vector_size);
}

ExternalDeclarationFields
externalDeclarationFieldsFromJson(const JsonValue &json, VariantT kind,
                                  const std::string &context) {
  const bool first_is_self = json.at("first_nondefining_is_self").asBool();
  const bool first_is_null = json.at("first_nondefining_is_null").asBool();
  const bool defining_is_self =
      json.at("defining_declaration_is_self").asBool();
  const bool defining_is_null =
      json.at("defining_declaration_is_null").asBool();
  if (first_is_self == first_is_null) {
    throw std::runtime_error(
        "AST JSON " + context +
        " first-nondefining declaration state is not exclusive");
  }
  if (defining_is_self == defining_is_null) {
    throw std::runtime_error("AST JSON " + context +
                             " defining declaration state is not exclusive");
  }
  return {declarationFieldsFromJson(json, kind, context,
                                    context + " source name qualifier"),
          first_is_self, defining_is_self};
}

void ExternalDeclarationFields::initialize(SgDeclarationStatement *decl) {
  declaration.initialize(decl);
  decl->set_firstNondefiningDeclaration(first_is_self ? decl : nullptr);
  decl->set_definingDeclaration(defining_is_self ? decl : nullptr);
}

static SgAlignmentSpecifierPtrList
alignmentSpecifiersFromJson(const JsonValue &json, const NodeMap &nodes,
                            InlineExpressionConstruction &ownership) {
  if (json.kind != JsonValue::Kind::Array)
    throw std::runtime_error("AST JSON alignment specifiers are not an array");
  SgAlignmentSpecifierPtrList specifiers;
  specifiers.reserve(json.array.size());
  SgNode::SourceStateConstruction construction;
  for (const JsonValue &entry : json.array) {
    NodeRecord record;
    record.kind = "SgAlignmentSpecifier";
    record.flags = entry.at("flags");
    const auto flags = validateConstructionFlags(record);
    record.location = entry.at("location");
    record.properties = entry.at("properties");
    record.preprocessing = record.properties.at("preprocessing");
    const JsonValue &operand_json = entry.at("operand");
    (void)alignmentSpellingFromProperties(
        record.properties, operand_json.requiredInt("node") != 0 ||
                               operand_json.find("owned_kind") != nullptr);
    auto positions = sourcePositionsFromJson(record.location, false);
    SgExpression *operand = expressionFromRef(operand_json, nodes, ownership);
    auto *specifier =
        alignmentSpecifierFromProperties(record.properties, operand);
    ownership.own(specifier);
    installSourcePositions(specifier, std::move(positions), record.kind);
    attachPreprocessingInfo(specifier, record);
    attachAstAttributes(specifier, record);
    flags.initialize(specifier);
    specifiers.push_back(specifier);
  }
  return specifiers;
}

void constructExternalFunctionOwnedAttributes(
    SgFunctionDeclaration *declaration, const JsonValue &json,
    const NodeMap &nodes) {
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_alignment_specifiers().empty());
  ROSE_ASSERT(declaration->get_cuda_launch_bounds_expression() == nullptr);
  InlineExpressionConstruction ownership;
  auto *launch_bounds = expressionFromRef(
      json.at("cuda_launch_bounds_expression"), nodes, ownership);
  auto specifiers = alignmentSpecifiersFromJson(json.at("alignment_specifiers"),
                                                nodes, ownership);
  if (launch_bounds != nullptr &&
      (launch_bounds->get_parent() != nullptr ||
       !SgFunctionDeclaration::cuda_launch_bounds_operands_are_valid(
           launch_bounds)))
    throw std::runtime_error("AST JSON external CUDA launch bounds require an "
                             "unowned typed expression");
  declaration->get_alignment_specifiers().swap(specifiers);
  for (auto *specifier : declaration->get_alignment_specifiers())
    specifier->set_parent(declaration);
  declaration->set_cuda_launch_bounds_expression(launch_bounds);
  if (launch_bounds != nullptr)
    launch_bounds->set_parent(declaration);
  declaration->validate_cuda_launch_bounds_expression();
  ownership.publish();
}

void InitializedNameMetadata::initialize(SgInitializedName *name) const {
  name->set_fortran_procedure_component_binding(procedure_component_binding);
  name->set_storage_size_bytes(storage_size_bytes);
  name->set_cxx_field_index(cxx_field_index);
  name->set_fortran_type_spec(fortran_type_spec);
  name->set_fortran_c_argument_passing(argument_passing);
  name->set_is_predefined_identifier(predefined_identifier);
  name->set_is_braced_initialized(braced_initialized);
  name->set_using_assignment_copy_constructor_syntax(
      assignment_copy_constructor_syntax);
  name->set_is_parameter_pack(parameter_pack);
  name->set_is_pack_element(pack_element);
  name->set_deduction_guide_default_available(
      deduction_guide_default_available);
  name->set_generated_variable_role(generated_role);
  name->get_storageModifier().set_modifier(storage_modifier);
  if (!section.empty())
    name->set_gnu_attribute_section_name(section);
  name->set_name_qualification_length(name_qualification_length);
  name->set_type_elaboration_required(type_elaboration_required);
  name->set_global_qualification_required(global_qualification_required);
  name->set_name_qualification_length_for_type(
      name_qualification_length_for_type);
  name->set_type_elaboration_required_for_type(
      type_elaboration_required_for_type);
  name->set_global_qualification_required_for_type(
      global_qualification_required_for_type);
  name->set_source_type_global_qualification(source_type_global);
  name->get_source_type_qualification_tokens() = source_type_tokens;
  name->set_source_type_qualification_present(source_type_present);
  name->set_source_name_global_qualification(source_name_global);
  name->get_source_name_qualification_tokens() = source_name_tokens;
  name->set_source_name_qualification_present(source_name_present);
}

SgInitializedName *externalInitializedNameFromJson(
    const JsonValue &json, const NodeMap &nodes, SgDeclarationStatement *decl,
    SgNode *parent, SgScopeStatement *scope, const std::string &context) {
  return constructExternalInitializedName(
      json, nodes, externalInitializedNameFieldsFromJson(json, nodes, context),
      decl, parent, scope, context);
}

SgInitializedName *constructExternalInitializedName(
    const JsonValue &json, const NodeMap &nodes,
    const ExternalInitializedNameFields &fields, SgDeclarationStatement *decl,
    SgNode *parent, SgScopeStatement *scope, const std::string &context) {
  const InitializedNameMetadata &metadata = fields.metadata;
  const JsonValue &procedure_interface_json =
      json.at("fortran_procedure_interface");
  auto positions = constructSourcePositions(fields.positions);
  SgType *type = fields.type();
  requireFortranProcedureComponentBinding(metadata.procedure_component_binding,
                                          type, scope);
  SgType *auto_decltype = fields.auto_decltype();
  SgType *fortran_source_type = fields.fortran_source_type();
  SgSymbol *source_symbol = fields.source_symbol;
  SgType *cxx_source_type = fields.cxx_source_type();
  if (cxx_source_type != nullptr &&
      !(isSgVariableDeclaration(decl) != nullptr
            ? SageInterface::cxxSourceTypeMatchesObjectType(cxx_source_type,
                                                            type)
            : SageInterface::cxxSourceTypeMatchesSemanticType(cxx_source_type,
                                                              type)))
    throw std::runtime_error("AST JSON initializedName has contradictory "
                             "C++ source and semantic types");
  SourceTypeConstruction interface_types;
  InlineExpressionConstruction ownership;
  SgExpression *fortran_procedure_interface =
      expressionFromRef(procedure_interface_json, nodes, ownership);
  if (auto *source = isSgTypeExpression(fortran_procedure_interface))
    if (source->get_represented_type()->get_fortran_source_syntax())
      ownership.borrow(source->get_represented_type());
  requireFortranProcedureInterface(fortran_procedure_interface, type,
                                   metadata.fortran_type_spec, source_symbol);
  if (fortran_procedure_interface != nullptr &&
      (!ownership.owns(fortran_procedure_interface) ||
       fortran_procedure_interface->get_parent() != nullptr))
    throw std::runtime_error("AST JSON external initializedName has no exact "
                             "unowned procedure interface");
  auto alignments = alignmentSpecifiersFromJson(json.at("alignment_specifiers"),
                                                nodes, ownership);
  SgExpression *initializer_expression =
      expressionFromRef(json.at("initializer"), nodes, ownership);
  SgInitializer *initializer = isSgInitializer(initializer_expression);
  if ((initializer_expression != nullptr && initializer == nullptr) ||
      (initializer != nullptr &&
       (!ownership.owns(initializer) || initializer->get_parent() != nullptr)))
    throw std::runtime_error(
        "AST JSON " + context +
        " initializedName has no exact unowned initializer");
  auto *initialized_name = static_cast<SgInitializedName *>(
      ownership.own(new SgInitializedName(fields.name, type, initializer)));
  installSourcePositions(initialized_name, std::move(positions),
                         context + " initializedName " +
                             fields.name.getString());
  metadata.initialize(initialized_name);
  initialized_name->set_auto_decltype(auto_decltype);
  initialized_name->set_fortran_source_type(fortran_source_type);
  initialized_name->set_fortran_source_derived_type_symbol(source_symbol);
  initialized_name->set_cxx_source_type(cxx_source_type);
  initialized_name->set_parent(parent);
  initialized_name->set_declptr(decl);
  initialized_name->set_scope(scope);
  initialized_name->set_fortran_procedure_interface(
      fortran_procedure_interface);
  initialized_name->get_alignment_specifiers().swap(alignments);
  for (SgAlignmentSpecifier *specifier :
       initialized_name->get_alignment_specifiers())
    specifier->set_parent(initialized_name);
  interface_types.publish();
  ownership.publish();
  return initialized_name;
}

SgVariableDeclaration *
externalVariableDeclarationFromJson(const JsonValue &json, const NodeMap &nodes,
                                    SgFunctionParameterScope *scope,
                                    const std::string &function_name) {
  const std::string context =
      "external_function " + function_name + " variableDeclaration";
  auto declaration_fields =
      externalDeclarationFieldsFromJson(json, V_SgVariableDeclaration, context);
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration has no location");
  }
  const auto position_fields = requiredSourcePositionFieldsFromJson(
      *location, "external_function variableDeclaration " + function_name);
  const bool is_constexpr = json.requiredBool("is_constexpr");
  const bool is_thread_local = json.requiredBool("is_thread_local");
  const bool requires_global_name_qualification_on_type =
      json.requiredBool("requires_global_name_qualification_on_type");
  const JsonValue &variables = json.at("variables");
  if (variables.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration variables is not an array");
  }
  std::vector<ExternalInitializedNameFields> variable_fields;
  variable_fields.reserve(variables.array.size());
  for (const JsonValue &variable : variables.array)
    variable_fields.push_back(
        externalInitializedNameFieldsFromJson(variable, nodes, context));
  auto positions = constructSourcePositions(position_fields);
  InlineExpressionConstruction alignment_ownership;
  auto alignments = alignmentSpecifiersFromJson(json.at("alignment_specifiers"),
                                                nodes, alignment_ownership);
  auto owner = std::make_unique<SgVariableDeclaration>();
  SgVariableDeclaration *decl = owner.get();
  installSourcePositions(decl, std::move(positions),
                         "external_function variableDeclaration " +
                             function_name);
  decl->set_is_constexpr(is_constexpr);
  decl->set_is_thread_local(is_thread_local);
  decl->set_parent(scope);
  decl->set_scope(scope);
  declaration_fields.initialize(decl);
  decl->set_requiresGlobalNameQualificationOnType(
      requires_global_name_qualification_on_type);
  auto discard_name = [decl](SgInitializedName *name) {
    auto &variables = decl->get_variables();
    variables.erase(std::remove(variables.begin(), variables.end(), name),
                    variables.end());
    name->set_parent(nullptr);
    name->set_declptr(nullptr);
    SageInterface::deleteAST(name,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  };
  using OwnedName = std::unique_ptr<SgInitializedName, decltype(discard_name)>;
  std::vector<OwnedName> names;
  for (size_t i = 0; i < variable_fields.size(); ++i) {
    OwnedName initialized_name(
        constructExternalInitializedName(variables.array[i], nodes,
                                         variable_fields[i], decl, decl, scope,
                                         context),
        discard_name);
    names.push_back(std::move(initialized_name));
    decl->get_variables().push_back(names.back().get());
  }
  decl->get_alignment_specifiers().swap(alignments);
  for (SgAlignmentSpecifier *specifier : decl->get_alignment_specifiers())
    specifier->set_parent(decl);
  alignment_ownership.publish();
  for (auto &name : names)
    name.release();
  return owner.release();
}

SgUseStatement *externalUseStatementFromJson(const JsonValue &json,
                                             const NodeMap &nodes,
                                             SgFunctionParameterScope *scope,
                                             const std::string &function_name) {
  const std::string context =
      "external_function " + function_name + " useStatement";
  auto declaration_fields =
      externalDeclarationFieldsFromJson(json, V_SgUseStatement, context);
  const uint64_t module_id = json.requiredNonnegativeNodeId("module");
  const JsonValue &external_module = json.at("external_module");
  const bool external_module_present = external_module.requiredBool("present");
  if (module_id != 0 && external_module_present) {
    throw std::runtime_error(
        "AST JSON external use statement has two module identities");
  }
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " use statement has no location");
  }
  const JsonValue &rename_list = json.at("rename_list");
  if (rename_list.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename_list is not an array");
  }
  const SgName name = json.requiredName("name");
  const bool only_option = json.requiredBool("only_option");
  const std::string module_nature = json.requiredString("module_nature");
  struct RenameFields {
    SgName local_name;
    SgName use_name;
    SourcePositionFields positions;
  };
  std::vector<RenameFields> rename_fields;
  rename_fields.reserve(rename_list.array.size());
  for (const JsonValue &rename : rename_list.array) {
    const std::string kind = rename.requiredString("kind");
    if (kind != "SgRenamePair")
      throw std::runtime_error("AST JSON external_function " + function_name +
                               " SgUseStatement rename_list entry has "
                               "unsupported kind: " +
                               kind);
    const JsonValue *location = rename.find("location");
    if (location == nullptr)
      throw std::runtime_error("AST JSON external_function " + function_name +
                               " SgUseStatement rename pair has no location");
    rename_fields.push_back({rename.requiredName("local_name"),
                             rename.requiredName("use_name"),
                             sourcePositionFieldsFromJson(*location, false)});
  }
  auto positions = requiredSourcePositionsFromJson(
      *location, "external_function useStatement " + function_name);
  SgModuleStatement *module =
      module_id != 0 ? nodeByIdAs<SgModuleStatement>(nodes, module_id)
                     : externalModuleFromJson(external_module);
  InlineExpressionConstruction alignment_ownership;
  auto alignments = alignmentSpecifiersFromJson(json.at("alignment_specifiers"),
                                                nodes, alignment_ownership);
  auto owner = std::make_unique<SgUseStatement>(name, only_option,
                                                module_nature, module);
  SgUseStatement *stmt = owner.get();
  installSourcePositions(stmt, std::move(positions),
                         "external_function useStatement " + function_name);
  stmt->set_parent(scope);
  stmt->set_scope(scope);
  declaration_fields.initialize(stmt);

  std::vector<std::unique_ptr<SgRenamePair>> renames;
  for (const auto &fields : rename_fields) {
    auto rename =
        std::make_unique<SgRenamePair>(fields.local_name, fields.use_name);
    installSourcePositions(
        rename.get(), constructSourcePositions(fields.positions),
        "external_function " + function_name + " SgUseStatement rename pair");
    rename->set_parent(stmt);
    renames.push_back(std::move(rename));
    stmt->get_rename_list().push_back(renames.back().get());
  }
  stmt->get_alignment_specifiers().swap(alignments);
  for (SgAlignmentSpecifier *specifier : stmt->get_alignment_specifiers())
    specifier->set_parent(stmt);
  alignment_ownership.publish();
  for (auto &rename : renames)
    rename.release();
  return owner.release();
}

SgDeclarationStatement *externalParameterScopeDeclarationFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind == "SgVariableDeclaration") {
    return externalVariableDeclarationFromJson(json, nodes, scope,
                                               function_name);
  }
  if (kind == "SgUseStatement") {
    return externalUseStatementFromJson(json, nodes, scope, function_name);
  }
  throw std::runtime_error(
      "AST JSON external_function " + function_name +
      " functionParameterScope declaration kind is unsupported: " + kind);
}

void constructExternalFunctionParameterScopeDeclarationsFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name,
    std::optional<SourcePositions> positions) {
  if (!json.requiredBool("present")) {
    ROSE_ASSERT(scope == nullptr && !positions);
    return;
  }
  ROSE_ASSERT(scope != nullptr &&
              isSgFunctionDeclaration(scope->get_parent()) != nullptr &&
              positions);
  for (SgInitializedName *parameter : parameters) {
    if (parameter == nullptr || parameter->get_scope() != scope)
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " has a parameter without its exact construction scope");
  }
  const JsonValue &declarations = json.at("declarations");
  if (declarations.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " functionParameterScope declarations is not an "
                             "array");
  }
  installSourcePositions(scope, std::move(*positions),
                         "external_function functionParameterScope " +
                             function_name);

  for (const JsonValue &declaration : declarations.array) {
    SgDeclarationStatement *decl = externalParameterScopeDeclarationFromJson(
        declaration, nodes, scope, function_name);
    scope->append_declaration(decl);
  }
}

void populateExternalFunctionParameterScopeFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name) {
  std::optional<SourcePositions> positions;
  if (json.requiredBool("present"))
    positions = requiredSourcePositionsFromJson(
        json.at("location"),
        "external_function functionParameterScope " + function_name);
  constructExternalFunctionParameterScopeDeclarationsFromJson(
      json, nodes, scope, parameters, function_name, std::move(positions));
  if (scope != nullptr)
    constructExternalFunctionParameterScopeSymbolsFromJson(
        json, nodes, scope, parameters, function_name);
}

void constructExternalFunctionParameterScopeSymbolsFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name) {
  ASSERT_not_null(scope);
  const bool symbol_table_present = json.requiredBool("symbol_table_present");
  const JsonValue *symbol_table = nullptr;
  bool case_insensitive = false;
  if (symbol_table_present) {
    case_insensitive = json.requiredBool("symbol_table_case_insensitive");
    symbol_table = &json.at("symbol_table");
    if (symbol_table->kind != JsonValue::Kind::Array ||
        static_cast<int64_t>(symbol_table->array.size()) !=
            json.requiredInt("symbol_table_size"))
      throw std::runtime_error("AST JSON external function parameter scope has "
                               "an invalid symbol table");
  }
  const SgDeclarationStatementPtrList &declarations_by_index =
      scope->get_declarations();

  if (symbol_table_present) {
    SgSymbolTable *table = scope->get_symbol_table();
    ROSE_ASSERT(table != nullptr && table->get_parent() == scope &&
                table->get_table()->empty() && table->get_symbols().empty());
    std::vector<std::function<SgSymbol *()>> prepared;
    std::vector<std::optional<size_t>> dependencies(symbol_table->array.size());
    std::vector<ExpectedSymbolTablePreference> symbol_preferences;
    for (const JsonValue &entry : symbol_table->array) {
      const bool lookup_preferred = entry.requiredBool("lookup_preferred");
      const std::string symbol_kind = entry.requiredString("symbol_kind");
      const VariantT symbol_variant = symbolVariantFromJsonKind(symbol_kind);
      const SgName entry_name = entry.requiredName("entry_name");
      symbol_preferences.emplace_back(table, entry_name, symbol_variant,
                                      lookup_preferred);
      const bool parameter_basis = entry.find("parameter_index") != nullptr;
      const bool declaration_basis = entry.find("declaration_index") != nullptr;
      const bool variable_index = entry.find("variable_index") != nullptr;
      const bool structured_basis = entry.find("symbol") != nullptr;
      if (static_cast<int>(parameter_basis) + declaration_basis +
                  structured_basis !=
              1 ||
          declaration_basis != variable_index)
        throw std::runtime_error(
            "AST JSON external parameter-scope entry has conflicting or "
            "incomplete symbol bases");
      if (const JsonValue *parameter_index_value =
              entry.find("parameter_index")) {
        const int64_t parameter_index = parameter_index_value->asInt();
        if (parameter_index < 0 ||
            static_cast<uint64_t>(parameter_index) >= parameters.size() ||
            symbol_variant != V_SgVariableSymbol) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope parameter symbol index or kind is "
              "invalid");
        }
        SgInitializedName *parameter =
            parameters[static_cast<size_t>(parameter_index)];
        if (parameter == nullptr || parameter->get_scope() != scope) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope parameter symbol has no exact basis");
        }
        prepared.push_back(
            [parameter] { return new SgVariableSymbol(parameter); });
        continue;
      }
      if (const JsonValue *declaration_index_value =
              entry.find("declaration_index")) {
        const int64_t declaration_index = declaration_index_value->asInt();
        const int64_t variable_index = entry.requiredInt("variable_index");
        SgVariableDeclaration *variable =
            declaration_index >= 0 && static_cast<uint64_t>(declaration_index) <
                                          declarations_by_index.size()
                ? isSgVariableDeclaration(
                      declarations_by_index[static_cast<size_t>(
                          declaration_index)])
                : nullptr;
        if (variable == nullptr || variable_index < 0 ||
            static_cast<uint64_t>(variable_index) >=
                variable->get_variables().size()) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope symbol_table indexes are out of range");
        }
        if (symbol_variant != V_SgVariableSymbol) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope indexed symbol kind is unsupported: " +
              symbol_kind);
        }
        SgInitializedName *declaration =
            variable->get_variables()[static_cast<size_t>(variable_index)];
        prepared.push_back(
            [declaration] { return new SgVariableSymbol(declaration); });
        continue;
      }
      const JsonValue *symbol_json = entry.find("symbol");
      ASSERT_not_null(symbol_json);
      const std::string referenced_kind =
          symbol_json->at("symbol_kind").asString();
      if (referenced_kind != symbol_kind) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol_table kind mismatch for " +
            entry_name.getString() + ": entry=" + symbol_kind +
            " symbol=" + referenced_kind);
      }
      if (symbol_variant == V_SgAliasSymbol ||
          symbol_variant == V_SgRenameSymbol) {
        SgNodePtrList causes;
        const JsonValue &causal_nodes = entry.at("alias_causal_nodes");
        if (causal_nodes.kind != JsonValue::Kind::Array ||
            (symbol_variant == V_SgAliasSymbol && causal_nodes.array.empty())) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope alias_causal_nodes is not a non-empty "
              "array");
        }
        for (const JsonValue &causal_node : causal_nodes.array) {
          if (causal_node.kind != JsonValue::Kind::Object) {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope alias causal node is not an object");
          }
          const uint64_t node_id =
              causal_node.requiredNonnegativeNodeId("node");
          const int64_t declaration_index =
              causal_node.requiredInt("declaration_index");
          if (declaration_index < -1 ||
              (declaration_index >= 0 &&
               static_cast<uint64_t>(declaration_index) >=
                   declarations_by_index.size())) {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope alias causal declaration index is "
                "out of range");
          }
          if (node_id != 0) {
            SgNode *cause = nodeById(nodes, node_id);
            if (declaration_index >= 0 &&
                cause != declarations_by_index[static_cast<size_t>(
                             declaration_index)]) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope alias causal node and declaration "
                  "index disagree");
            }
            causes.push_back(cause);
          } else if (declaration_index >= 0) {
            causes.push_back(
                declarations_by_index[static_cast<size_t>(declaration_index)]);
          } else {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope alias causal node cannot be "
                "resolved");
          }
        }
        VisibilitySymbolFields fields(entry, table);
        if (fields.target_binding != nullptr &&
            fields.target_binding->find("binding_local") != nullptr)
          dependencies[prepared.size()] = exactSymbolBindingEntryIndex(
              *fields.target_binding, *symbol_table);
        if (fields.basis_id != 0)
          nodeById(nodes, fields.basis_id);
        if (fields.kind == V_SgRenameSymbol && fields.basis_id == 0) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope SgRenameSymbol has no collected "
              "function declaration basis: " +
              entry_name.getString());
        }
        if (fields.basis_id == 0 && fields.target_binding == nullptr)
          fields.construct_external_target =
              prepareExternalSymbolFromJson(*fields.target, nodes, nullptr);
        prepared.push_back([fields, causes = std::move(causes), &nodes] {
          return constructVisibilitySymbol(fields, nodes, causes).release();
        });
        continue;
      }
      const uint64_t basis_id =
          symbol_json->requiredNonnegativeNodeId("symbol_declaration");
      SgNode *basis = basis_id != 0 ? nodeById(nodes, basis_id) : nullptr;
      if (basis != nullptr) {
        prepared.push_back(
            prepareSymbolForKindAndBasis(symbol_variant, basis, *symbol_json));
      } else {
        prepared.push_back(
            prepareExternalSymbolFromJson(*symbol_json, nodes, table));
      }
    }

    ROSE_ASSERT(prepared.size() == symbol_preferences.size());
    validateSymbolTablePreferenceGroups(symbol_preferences, case_insensitive);
    auto order = symbolDependencyOrder(dependencies);
    std::stable_partition(order.begin(), order.end(), [&](size_t index) {
      const auto kind = symbolVariantFromJsonKind(
          symbol_table->array[index].requiredString("symbol_kind"));
      return kind != V_SgAliasSymbol && kind != V_SgRenameSymbol;
    });
    table->setCaseInsensitive(case_insensitive);
    for (auto index : order) {
      auto &entry = symbol_preferences[index];
      SgSymbol *raw_symbol = prepared[index]();
      if (raw_symbol != nullptr && raw_symbol->get_parent() != nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol is already owned");
      }
      std::unique_ptr<SgSymbol> symbol(raw_symbol);
      if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope cannot reconstruct symbol_table entry " +
            entry.entry_name.getString());
      }
      table->insert(entry.entry_name, raw_symbol);
      symbol.release();
      entry.symbol = raw_symbol;
      validateExternalSymbolBasisOwnership(raw_symbol);
    }

    enforceSymbolTablePreferences(symbol_preferences);
    validateSymbolTablePreferences(symbol_preferences);
  }
}

std::function<SgClassDeclaration *()>
decodeExternalClassDeclaration(const JsonValue &json) {
  if (!json.requiredBool("present")) {
    return []() -> SgClassDeclaration * { return nullptr; };
  }
  if (!externalClassDeserializationIdentityActive) {
    throw std::runtime_error(
        "AST JSON external class declaration was reconstructed outside its "
        "exact identity transaction");
  }
  if (requiredTranslationUnitSourceOrder(json, "external class declaration")
          .has_value()) {
    throw std::runtime_error(
        "AST JSON external class declaration must be semantic-only and "
        "unordered");
  }

  const std::string kind = json.at("kind").asString();
  const std::string name = json.at("name").asString();
  const std::string source_file = json.at("source_file").asString();
  if (kind.empty() || name.empty() || name.find('\0') != std::string::npos ||
      source_file.empty() || source_file.find('\0') != std::string::npos) {
    throw std::runtime_error("AST JSON external class declaration requires "
                             "kind, name, and source_file");
  }

  const auto class_type = requiredClassType(json, "external class declaration");
  const std::string module_name = json.at("module_name").asString();
  if (module_name.find('\0') != std::string::npos)
    throw std::runtime_error(
        "AST JSON external class declaration has an invalid module_name");
  const bool has_definition = json.at("has_definition").asBool();
  const bool is_first_nondefining = json.at("is_first_nondefining").asBool();
  using Factory =
      SgClassDeclaration *(*)(const SgName &, SgClassDeclaration::class_types);
  Factory factory = nullptr;
  VariantT expected_variant;
  const bool module_record = kind == "SgModuleStatement";
  if (kind == "SgDerivedTypeStatement") {
    expected_variant = V_SgDerivedTypeStatement;
    factory = +[](const SgName &n,
                  SgClassDeclaration::class_types t) -> SgClassDeclaration * {
      return new SgDerivedTypeStatement(n, t, nullptr, nullptr);
    };
  } else if (module_record) {
    expected_variant = V_SgModuleStatement;
    factory = +[](const SgName &n,
                  SgClassDeclaration::class_types t) -> SgClassDeclaration * {
      return new SgModuleStatement(n, t, nullptr, nullptr);
    };
  } else if (kind == "SgClassDeclaration") {
    expected_variant = V_SgClassDeclaration;
    factory = +[](const SgName &n,
                  SgClassDeclaration::class_types t) -> SgClassDeclaration * {
      return new SgClassDeclaration(n, t, nullptr, nullptr);
    };
  } else {
    throw std::runtime_error("AST JSON external class declaration has "
                             "unsupported declaration kind: " +
                             kind);
  }
  if (expected_variant == V_SgDerivedTypeStatement &&
      class_type != SgClassDeclaration::e_struct)
    throw std::runtime_error(
        "AST JSON external derived type has an invalid class_type");
  if (module_record && (class_type != SgClassDeclaration::e_fortran_module ||
                        !has_definition || !is_first_nondefining))
    throw std::runtime_error("AST JSON external module declaration has no "
                             "exact canonical definition");
  if (currentDeserializationProject != nullptr) {
    SgClassDeclaration *matched = nullptr;
    bool saw_source_file = false;
    for (SgSourceFile *source : currentDeserializationSourceFiles()) {
      if (!sourceFileMatchesExternalRecord(source, source_file))
        continue;
      saw_source_file = true;
      for (SgClassDeclaration *candidate :
           structuralClassDeclarationsForSource(source)) {
        if (candidate == nullptr || candidate->variantT() != expected_variant ||
            candidate->get_name().getString() != name ||
            candidate->get_class_type() != class_type ||
            moduleNameForNode(candidate) != module_name ||
            classDeclarationHasDefinition(candidate) != has_definition ||
            classDeclarationIsFirstNondefining(candidate) !=
                is_first_nondefining)
          continue;
        if (matched != nullptr)
          throw std::runtime_error("AST JSON external class declaration is "
                                   "ambiguous in source file " +
                                   source_file + ": " + kind + " " + name);
        matched = candidate;
      }
    }
    if (saw_source_file && matched == nullptr)
      throw std::runtime_error("AST JSON external class declaration was not "
                               "found in loaded source file " +
                               source_file + ": " + kind + " " + name);
  }
  const uint64_t identity_hash = exactJsonValueHash(json);
  return [json, name, source_file, class_type, module_name, has_definition,
          is_first_nondefining, factory, module_record,
          identity_hash]() -> SgClassDeclaration * {
    auto &identity_bucket =
        externalClassDeserializationIdentities[identity_hash];
    for (const auto &identity : identity_bucket) {
      if (identity.declaration == nullptr)
        throw std::runtime_error(
            "AST JSON external class identity cache is malformed");
      if (identity.serialized_declaration.exactlyEquals(json))
        return identity.declaration;
    }
    auto find_external_module = [&](const std::string &module_identity_name)
        -> const ExternalClassModuleIdentity * {
      for (const ExternalClassModuleIdentity &identity :
           externalClassModuleIdentities) {
        if (identity.name == module_identity_name &&
            identity.source_file == source_file) {
          return &identity;
        }
      }
      return nullptr;
    };
    if (module_record) {
      if (const ExternalClassModuleIdentity *identity =
              find_external_module(name)) {
        if (identity->declaration == nullptr ||
            identity->definition == nullptr ||
            identity->declaration->get_definition() != identity->definition ||
            identity->definition->get_declaration() != identity->declaration) {
          throw std::runtime_error(
              "AST JSON external module declaration conflicts with its "
              "canonical reconstructed module");
        }
        identity_bucket.push_back({json, identity->declaration});
        return identity->declaration;
      }
    }

    SgModuleStatement *module = nullptr;
    SgClassDefinition *module_definition = nullptr;
    SgGlobal *external_global = nullptr;
    if (!module_name.empty() && !module_record) {
      if (const ExternalClassModuleIdentity *identity =
              find_external_module(module_name)) {
        module = identity->declaration;
        module_definition = identity->definition;
      }
      if (module == nullptr) {
        external_global = new SgGlobal();
        installTransformationSourcePosition(external_global);
        module = new SgModuleStatement(SgName(module_name),
                                       SgClassDeclaration::e_fortran_module,
                                       nullptr, nullptr);
        installTransformationSourcePosition(module);
        module->set_firstNondefiningDeclaration(module);
        module->set_definingDeclaration(module);
        module->set_parent(external_global);
        module->set_scope(external_global);
        module_definition = new SgClassDefinition(module);
        installTransformationSourcePosition(module_definition);
        external_global->get_declarations().push_back(module);
        constructClassTypeForDeclaration(module);
        markAstJsonExternalModule(module, source_file);
        externalClassModuleIdentities.push_back(
            {module_name, source_file, module, module_definition});
      } else if (module_definition == nullptr ||
                 module->get_definition() != module_definition ||
                 module_definition->get_declaration() != module) {
        throw std::runtime_error(
            "AST JSON external class module identity cache is malformed");
      }
    } else {
      external_global = new SgGlobal();
      installTransformationSourcePosition(external_global);
    }

    SgClassDeclaration *decl = factory(SgName(name), class_type);
    installTransformationSourcePosition(decl);
    SgScopeStatement *declaration_scope =
        module_definition != nullptr
            ? static_cast<SgScopeStatement *>(module_definition)
            : static_cast<SgScopeStatement *>(external_global);
    decl->set_parent(declaration_scope);
    decl->set_scope(declaration_scope);
    markAstJsonExternalClassDeclaration(decl, source_file);

    SgClassDeclaration *first_nondefining = decl;
    SgClassDeclaration *defining = has_definition ? decl : nullptr;
    if (has_definition && !is_first_nondefining) {
      first_nondefining = factory(SgName(name), class_type);
      installTransformationSourcePosition(first_nondefining);
      first_nondefining->set_parent(declaration_scope);
      first_nondefining->set_scope(declaration_scope);
      markAstJsonExternalClassDeclaration(first_nondefining, source_file);
      first_nondefining->set_firstNondefiningDeclaration(first_nondefining);
      first_nondefining->set_definingDeclaration(decl);
    }

    decl->set_firstNondefiningDeclaration(first_nondefining);
    decl->set_definingDeclaration(defining);
    if (has_definition) {
      SgClassDefinition *definition = new SgClassDefinition(decl);
      installTransformationSourcePosition(definition);
    }

    if (module_definition != nullptr) {
      module_definition->get_members().push_back(first_nondefining);
      if (decl != first_nondefining) {
        module_definition->get_members().push_back(decl);
      }
    } else {
      if (external_global == nullptr) {
        throw std::runtime_error(
            "AST JSON external class has no isolated semantic scope");
      }
      external_global->get_declarations().push_back(first_nondefining);
      if (decl != first_nondefining) {
        external_global->get_declarations().push_back(decl);
      }
    }
    constructClassTypeForDeclaration(first_nondefining);
    if (SgModuleStatement *external_module =
            isSgModuleStatement(first_nondefining)) {
      SgClassDefinition *definition = external_module->get_definition();
      if (definition == nullptr ||
          definition->get_declaration() != external_module) {
        throw std::runtime_error(
            "AST JSON external module class record has no exact definition");
      }
      externalClassModuleIdentities.push_back(
          {external_module->get_name().getString(), source_file,
           external_module, definition});
    }

    identity_bucket.push_back({json, decl});

    return decl;
  };
}

SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json) {
  return decodeExternalClassDeclaration(json)();
}

namespace {
// A decoded type keeps required input fields and nested construction private.
// No enclosing reader enters a semantic type factory until its complete type
// description has decoded successfully.
struct FullTypeRecipe {
  std::function<SgType *()> construct;
  bool source_syntax;
  bool named;
  bool function;
  SgExpression *source_kind_selector = nullptr;
  bool top_level_qualified = false;

  SgType *operator()() const {
    SourceTypeConstruction construction;
    return construction.publish(construct());
  }
};

// Serialized identities are reserved while decoding the entire enclosing
// description, before any node can enter the published semantic type tables.
// Nested expression readers participate in the same private description.
class FullTypeDecoding {
  FullTypeDecoding *previous_;
  std::optional<FunctionTypeDeserializationIdentityGuard> function_identity_;
  std::unordered_map<uint64_t, JsonValue> arrays_;

public:
  struct Source {
    JsonValue json;
    std::optional<FullTypeRecipe> recipe;
  };
  std::unordered_map<uint64_t, Source> sources;

  static FullTypeDecoding *&current() {
    static thread_local FullTypeDecoding *scope = nullptr;
    return scope;
  }
  FullTypeDecoding() : previous_(current()) {
    if (!functionTypeDeserializationIdentityActive)
      function_identity_.emplace();
    if (previous_ == nullptr)
      current() = this;
  }
  ~FullTypeDecoding() {
    if (previous_ == nullptr)
      current() = nullptr;
  }
  FullTypeDecoding(const FullTypeDecoding &) = delete;
  FullTypeDecoding &operator=(const FullTypeDecoding &) = delete;

  void reserveArray(uint64_t identity, const JsonValue &json) {
    const auto published = arrayTypeDeserializationIdentities.find(identity);
    if (published != arrayTypeDeserializationIdentities.end() &&
        (published->second.type == nullptr ||
         !published->second.serialized_type.exactlyEquals(json) ||
         preservedSemanticArrayJsonIdentity(published->second.type) !=
             identity))
      throw std::runtime_error(
          "AST JSON repeated semantic array identity is contradictory");
    const auto inserted = arrays_.emplace(identity, json);
    if (!inserted.second && !inserted.first->second.exactlyEquals(json))
      throw std::runtime_error(
          "AST JSON repeated semantic array identity is contradictory");
  }
};

enum class TypeRole { semantic, cxx_source };

FullTypeRecipe decodeFullType(
    const JsonValue &, const NodeMap &,
    const char *exact_type_error = "AST JSON type requires an exact type",
    TypeRole = TypeRole::semantic);
FullTypeRecipe decodeFullTypeUncached(const JsonValue &, const NodeMap &,
                                      TypeRole = TypeRole::semantic);
FullTypeRecipe decodeSemanticFunctionType(const JsonValue &, const NodeMap &);
bool serializedTypeHasSourcePrototype(const JsonValue &json);

FullTypeRecipe decodeTypeComponent(const JsonValue &json, const NodeMap &nodes,
                                   bool source_owner,
                                   TypeRole role = TypeRole::semantic) {
  if (!source_owner && json.requiredBool("fortran_source_syntax"))
    throw std::runtime_error(
        "AST JSON semantic type has a source-syntax type component");
  return decodeFullType(json, nodes,
                        "AST JSON type component requires an exact type", role);
}

std::optional<FullTypeRecipe> decodeNullableTypeComponent(const JsonValue &json,
                                                          const NodeMap &nodes,
                                                          bool source_owner) {
  if (!json.requiredBool("present"))
    return std::nullopt;
  return decodeTypeComponent(json, nodes, source_owner);
}

std::vector<FullTypeRecipe>
decodeFunctionArguments(const FunctionTypeFields &fields, const NodeMap &nodes,
                        bool source_owner) {
  std::vector<FullTypeRecipe> result;
  result.reserve(fields.arguments.array.size());
  for (const JsonValue &argument : fields.arguments.array) {
    auto type = decodeTypeComponent(argument.at("type"), nodes, source_owner);
    if (fields.canonical && type.top_level_qualified)
      throw std::runtime_error(
          "AST JSON canonical callable argument has top-level qualifiers");
    result.push_back(std::move(type));
  }
  return result;
}

SgTypePtrList
constructFunctionArguments(const std::vector<FullTypeRecipe> &arguments) {
  SgTypePtrList result;
  for (const auto &argument : arguments)
    result.push_back(argument());
  return result;
}

struct DecodedFunctionException {
  SgFunctionType::exception_specification_kind_enum kind;
  SgExpression *expression;
  std::vector<FullTypeRecipe> types;
  std::vector<bool> packs;
  SgFunctionDeclaration *source;
  SgFunctionDeclaration *pattern;
  bool relative_source;
  bool written;
  std::shared_ptr<InlineExpressionConstruction> operands;
  bool borrowed_expression;

  void requireExpressionOwner(const SgFunctionType *cached = nullptr) const {
    if (cached != nullptr) {
      if (borrowed_expression &&
          (cached->get_exception_specification_expression() != expression ||
           expression->get_parent() != cached))
        throw std::runtime_error(
            "AST JSON cached function exception has a conflicting operand");
      return;
    }
    if (expression != nullptr && expression->get_parent() != nullptr)
      throw std::runtime_error(
          "AST JSON exception operand already has an owner");
  }
};

DecodedFunctionException
decodeFunctionException(const FunctionExceptionFields &fields,
                        const NodeMap &nodes, bool source_owner,
                        bool canonical = false) {
  if (canonical)
    for (const JsonValue &entry : fields.types.array)
      if (serializedTypeHasSourcePrototype(entry.at("type")))
        throw std::runtime_error(
            "AST JSON canonical exception type owns a source prototype");
  auto operands = std::make_shared<InlineExpressionConstruction>();
  auto *expression = expressionFromRef(fields.expression, nodes, *operands);
  DecodedFunctionException result{
      fields.kind,
      expression,
      {},
      {},
      fields.source != 0
          ? nodeByIdAs<SgFunctionDeclaration>(nodes, fields.source)
          : nullptr,
      fields.pattern != 0
          ? nodeByIdAs<SgFunctionDeclaration>(nodes, fields.pattern)
          : nullptr,
      fields.relative_source,
      fields.written,
      operands,
      fields.expression.requiredNonnegativeNodeId("node") != 0};
  for (const JsonValue &entry : fields.types.array) {
    result.types.push_back(
        decodeTypeComponent(entry.at("type"), nodes, source_owner));
    result.packs.push_back(entry.requiredBool("is_pack_expansion"));
  }
  return result;
}

std::unique_ptr<SgFunctionType>
constructFunctionType(SgType *result_type, bool ellipsis,
                      SgFunctionParameterTypeList *parameters, bool member,
                      SgType *owner, unsigned int specifier,
                      const DecodedFunctionException &exception,
                      SgFunctionType *semantic_signature, bool hasPrototype) {
  if (semantic_signature != nullptr) {
    auto *semantic_member = isSgMemberFunctionType(semantic_signature);
    if ((semantic_member != nullptr) != member ||
        semantic_signature->get_has_ellipses() != ellipsis ||
        semantic_signature->get_has_prototype() != hasPrototype ||
        semantic_signature->get_arguments().size() !=
            parameters->get_arguments().size() ||
        (semantic_member != nullptr &&
         (semantic_member->get_class_type() != owner ||
          semantic_member->get_mfunc_specifier() != specifier)))
      throw std::runtime_error(
          "AST JSON function type contradicts its semantic signature");
  }
  const auto types = constructFunctionArguments(exception.types);
  for (SgType *type : types)
    if (SageInterface::containsUnknownType(type) ||
        (type->has_source_prototype() && type->get_parent() != nullptr))
      throw std::runtime_error(
          "AST JSON exception type is unresolved or already owned");
  SgFunctionType::validate_exception_specification(
      exception.kind, exception.expression, types, exception.packs,
      exception.source, exception.pattern, exception.relative_source,
      exception.written);
  if (member)
    return std::make_unique<SgMemberFunctionType>(
        result_type, ellipsis, parameters, exception.kind, exception.expression,
        types, exception.packs, exception.source, exception.pattern,
        exception.relative_source, exception.written, semantic_signature,
        hasPrototype, owner, specifier);
  return std::make_unique<SgFunctionType>(
      result_type, ellipsis, parameters, exception.kind, exception.expression,
      types, exception.packs, exception.source, exception.pattern,
      exception.relative_source, exception.written, semantic_signature,
      hasPrototype);
}

void requireExactFunctionComponents(SgType *result_type, SgType *owner,
                                    const SgTypePtrList &arguments) {
  if (SageInterface::containsUnknownType(result_type) ||
      (owner != nullptr && SageInterface::containsUnknownType(owner)))
    throw std::runtime_error(
        "AST JSON function type has an unresolved result or owner type");
  for (SgType *argument : arguments) {
    if (SageInterface::containsUnknownType(argument))
      throw std::runtime_error(
          "AST JSON function type has an unresolved argument type");
  }
}

void publishTypeExpressions(InlineExpressionConstruction &nodes,
                            bool source_syntax) {
  if (source_syntax) {
    ROSE_ASSERT(SourceTypeConstruction::current() != nullptr);
    SourceTypeConstruction::current()->take(nodes);
  } else {
    nodes.publish();
  }
}

bool serializedTypeHasSourcePrototype(const JsonValue &json) {
  const std::string kind = json.requiredString("kind");
  if (kind == "SgFunctionType" || kind == "SgMemberFunctionType")
    return json.requiredNonnegativeNodeId("source_argument_list") != 0;
  if (kind == "SgPointerType" || kind == "SgPointerMemberType" ||
      kind == "SgReferenceType" || kind == "SgRvalueReferenceType" ||
      kind == "SgModifierType" || kind == "SgArrayType")
    return serializedTypeHasSourcePrototype(json.at("base"));
  return false;
}

bool serializedTypeOwnsSourceIdentity(const JsonValue &json) {
  if (json.requiredBool("fortran_source_syntax"))
    return true;
  const std::string kind = json.requiredString("kind");
  return kind == "SgAutoType" ||
         ((kind == "SgPointerType" || kind == "SgReferenceType" ||
           kind == "SgRvalueReferenceType" || kind == "SgModifierType") &&
          serializedTypeHasSourcePrototype(json.at("base")));
}

FullTypeRecipe decodeFullTypeUncached(const JsonValue &json,
                                      const NodeMap &nodes, TypeRole role) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON type record is not an object");
  }
  if (!json.requiredBool("present")) {
    throw std::runtime_error("AST JSON required type record is absent");
  }
  if (json.find("autonomous_declaration") != nullptr)
    throw std::runtime_error(
        "AST JSON named type contains obsolete autonomous_declaration");

  const std::string kind = json.requiredString("kind");
  const bool fortran_source_syntax = json.requiredBool("fortran_source_syntax");
  if (serializedTypeOwnsSourceIdentity(json) !=
      (json.find("source_type_identity") != nullptr))
    throw std::runtime_error(
        "AST JSON source type identity disagrees with source syntax");
  const bool supports_fortran_source_syntax =
      kind == "SgTypeBool" || kind == "SgTypeChar" || kind == "SgTypeInt" ||
      kind == "SgTypeUnsignedInt" || kind == "SgTypeFloat" ||
      kind == "SgTypeDouble" || kind == "SgTypeString" ||
      kind == "SgTypeComplex" || kind == "SgTypeFortranAssumed" ||
      kind == "SgTypeFortranUnlimitedPolymorphic" ||
      kind == "SgTypeCrayPointer" || kind == "SgPointerType" ||
      kind == "SgArrayType" || kind == "SgModifierType" ||
      kind == "SgFunctionType";
  if (fortran_source_syntax && !supports_fortran_source_syntax) {
    throw std::runtime_error("AST JSON type " + kind +
                             " has an invalid Fortran source-syntax identity");
  }
  auto recipe = [&](std::function<SgType *()> construct,
                    SgExpression *kind_selector = nullptr,
                    bool top_level_qualified = false) {
    const bool named = kind == "SgTypedefType" || kind == "SgClassType" ||
                       kind == "SgEnumType" || kind == "SgNonrealType";
    const bool function =
        kind == "SgFunctionType" || kind == "SgMemberFunctionType";
    return FullTypeRecipe{std::move(construct),
                          fortran_source_syntax,
                          named,
                          function,
                          fortran_source_syntax ? kind_selector : nullptr,
                          top_level_qualified};
  };
  if (kind == "SgUnaryTransformType") {
    const int64_t operation = json.requiredInt("operation");
    if (operation < SgUnaryTransformType::e_add_lvalue_reference ||
        operation > SgUnaryTransformType::e_enum_underlying_type)
      throw std::runtime_error(
          "AST JSON unary transform has invalid operation");
    const JsonValue &base = json.at("base");
    const auto operand =
        decodeTypeComponent(base, nodes, fortran_source_syntax);
    return recipe([operand, operation] {
      auto *base = operand();
      const auto kind =
          static_cast<SgUnaryTransformType::operation_enum>(operation);
      const bool fresh = SgUnaryTransformType::findCanonicalSemanticType(
                             base, kind) == nullptr;
      auto *type = SgUnaryTransformType::createType(base, kind);
      if (fresh)
        SourceTypeConstruction::current()->own(type);
      return type;
    });
  }
  if (kind == "SgTypeTargetBuiltin") {
    const int64_t family = json.requiredInt("target_family");
    if (family < SgTypeTargetBuiltin::e_target_builtin_aarch64 ||
        family > SgTypeTargetBuiltin::e_target_builtin_hlsl)
      throw std::runtime_error(
          "AST JSON target builtin type has invalid target family");
    const SgName spelling = json.requiredName("spelling");
    if (spelling.is_null())
      throw std::runtime_error(
          "AST JSON target builtin type has empty exact spelling");
    const auto target =
        static_cast<SgTypeTargetBuiltin::target_family_enum>(family);
    return recipe([spelling, target] {
      return SageBuilder::buildTargetBuiltinType(spelling, target);
    });
  }
  if (kind == "SgTypeFortranAssumed") {
    return recipe([fortran_source_syntax] {
      auto *type = fortran_source_syntax ? new SgTypeFortranAssumed()
                                         : SgTypeFortranAssumed::createType();
      if (fortran_source_syntax) {
        SourceTypeConstruction::current()->own(type);
        type->set_fortran_source_syntax(true);
      }
      return type;
    });
  }
  if (kind == "SgTypeFortranUnlimitedPolymorphic") {
    return recipe([fortran_source_syntax] {
      auto *type = fortran_source_syntax
                       ? new SgTypeFortranUnlimitedPolymorphic()
                       : SgTypeFortranUnlimitedPolymorphic::createType();
      if (fortran_source_syntax) {
        SourceTypeConstruction::current()->own(type);
        type->set_fortran_source_syntax(true);
      }
      return type;
    });
  }
  if (kind == "SgTypeCrayPointer") {
    return recipe([fortran_source_syntax] {
      auto *type = fortran_source_syntax ? new SgTypeCrayPointer()
                                         : SgTypeCrayPointer::createType();
      if (fortran_source_syntax) {
        SourceTypeConstruction::current()->own(type);
        type->set_fortran_source_syntax(true);
      }
      return type;
    });
  }
  if (kind == "SgTypeDependent") {
    return recipe([] { return SgTypeDependent::createType(); });
  }
  if (kind == "SgTypeEllipse") {
    return recipe([] { return SgTypeEllipse::createType(); });
  }
  if (kind == "SgTypeNullptr") {
    return recipe([] { return SageBuilder::buildNullptrType(); });
  }
  if (kind == "SgTypeFortranBOZ") {
    return recipe([] { return SgTypeFortranBOZ::createType(); });
  }
  if (kind == "SgTypeLabel") {
    const SgName name = json.requiredName("name");
    return recipe([name] { return SgTypeLabel::createType(name); });
  }
  if (kind == "SgTypeFloat128") {
    return recipe([] { return SageBuilder::buildFloat128Type(); });
  }
  if (kind == "SgTypeSigned128bitInteger") {
    return recipe([] { return SageBuilder::buildSigned128bitIntegerType(); });
  }
  if (kind == "SgTypeUnsigned128bitInteger") {
    return recipe([] { return SageBuilder::buildUnsigned128bitIntegerType(); });
  }
  if (kind == "SgAutoType") {
    const bool constrained = json.requiredBool("is_constrained");
    auto operands = std::make_shared<InlineExpressionConstruction>();
    SgExpression *expression =
        expressionFromRef(json.at("type_constraint"), nodes, *operands);
    SgNonrealRefExp *constraint = isSgNonrealRefExp(expression);
    if (constrained != (constraint != nullptr) || expression != constraint ||
        (constraint != nullptr &&
         (constraint->get_symbol() == nullptr ||
          constraint->get_symbol()->get_declaration() == nullptr ||
          !constraint->get_symbol()->get_declaration()->get_is_concept())))
      throw std::runtime_error(
          "AST JSON SgAutoType requires an exact concept reference");
    return recipe([constrained, constraint, operands] {
      if (constraint != nullptr && constraint->get_parent() != nullptr) {
        auto *type = isSgAutoType(constraint->get_parent());
        if (type == nullptr || type->get_type_constraint() != constraint ||
            type->get_is_constrained() != constrained)
          throw std::runtime_error(
              "AST JSON auto constraint already has a different lexical owner");
        operands->publish();
        return type;
      }
      auto *type = new SgAutoType(constrained, constraint);
      operands->own(type);
      SourceTypeConstruction::current()->take(*operands);
      return type;
    });
  }
  if (auto factory = primitiveTypeFactoryFromKind(kind)) {
    if (kind == "SgTypeChar" && fortran_source_syntax) {
      return recipe([] {
        auto *type = new SgTypeChar();
        SourceTypeConstruction::current()->own(type);
        type->set_fortran_source_syntax(true);
        return type;
      });
    }
    const bool has_kind = kind == "SgTypeBool" || kind == "SgTypeInt" ||
                          kind == "SgTypeUnsignedInt" || kind == "SgTypeFloat";
    if (!has_kind) {
      if (kind == "SgTypeDouble") {
        const bool available =
            json.requiredBool("fortran_fixed_kind_value_is_available");
        const std::int64_t value = json.requiredInt("fortran_fixed_kind_value");
        if ((!available && value != 0) || (available && value <= 0))
          throw std::runtime_error(
              "AST JSON SgTypeDouble has invalid fixed Fortran KIND metadata");
        if (available != fortran_source_syntax)
          throw std::runtime_error("AST JSON SgTypeDouble source-syntax and "
                                   "fixed KIND metadata differ");
        if (fortran_source_syntax) {
          return recipe([value] {
            auto *type = new SgTypeDouble();
            SourceTypeConstruction::current()->own(type);
            type->set_fortran_fixed_kind_value(value);
            type->set_fortran_fixed_kind_value_is_available(true);
            type->set_fortran_source_syntax(true);
            return type;
          });
        }
      } else if (fortran_source_syntax) {
        throw std::runtime_error(
            "AST JSON non-Fortran primitive type is marked as source syntax");
      }
      return recipe(factory);
    }
    const bool star = json.requiredBool("has_type_kind_star");
    if (star && !fortran_source_syntax)
      throw std::runtime_error(
          "AST JSON semantic intrinsic type has source-only star KIND");
    auto selectors = std::make_shared<InlineExpressionConstruction>();
    SgExpression *selector =
        expressionFromRef(json.at("type_kind"), nodes, *selectors);
    const bool available =
        selector != nullptr &&
        selector->get_fortran_integer_constant_value_is_available();
    if (available && !fortran_source_syntax)
      throw std::runtime_error(
          "AST JSON semantic intrinsic type owns source selector metadata");
    if (fortran_source_syntax && selector != nullptr && !available)
      throw std::runtime_error(
          "AST JSON source intrinsic KIND selector has no folded value");
    if (selector != nullptr &&
        !SageInterface::Detail::isValidFortranKindSelector(selector))
      throw std::runtime_error("AST JSON intrinsic KIND selector is not a "
                               "positive integer constant");
    enum class IntrinsicKind { Logical, Integer, UnsignedInteger, Real };
    const IntrinsicKind intrinsic =
        kind == "SgTypeBool"          ? IntrinsicKind::Logical
        : kind == "SgTypeInt"         ? IntrinsicKind::Integer
        : kind == "SgTypeUnsignedInt" ? IntrinsicKind::UnsignedInteger
                                      : IntrinsicKind::Real;
    return recipe(
        [intrinsic, selector, selectors, star, fortran_source_syntax] {
          if (fortran_source_syntax && selector != nullptr &&
              selector->get_parent() != nullptr)
            throw std::runtime_error(
                "AST JSON source intrinsic KIND selector is already owned");
          SgType *type = nullptr;
          switch (intrinsic) {
          case IntrinsicKind::Logical:
            type = fortran_source_syntax ? new SgTypeBool()
                                         : SageBuilder::buildBoolType(selector);
            break;
          case IntrinsicKind::Integer:
            type = fortran_source_syntax ? new SgTypeInt()
                                         : SageBuilder::buildIntType(selector);
            break;
          case IntrinsicKind::UnsignedInteger:
            type = fortran_source_syntax
                       ? new SgTypeUnsignedInt()
                       : SageBuilder::buildUnsignedIntType(selector);
            break;
          case IntrinsicKind::Real:
            type = fortran_source_syntax
                       ? new SgTypeFloat()
                       : SageBuilder::buildFloatType(selector);
            break;
          }
          ROSE_ASSERT(type != nullptr);
          if (fortran_source_syntax) {
            selectors->own(type);
            type->set_type_kind(selector);
            if (selector != nullptr)
              selector->set_parent(type);
            type->set_hasTypeKindStar(star);
            type->set_fortran_source_syntax(true);
          }
          const bool fresh = !fortran_source_syntax && selector != nullptr &&
                             type->get_type_kind() == selector;
          publishTypeExpressions(*selectors, fortran_source_syntax);
          if (fresh)
            SourceTypeConstruction::current()->own(type);
          return type;
        },
        selector);
  }
  if (kind == "SgTypeString") {
    const bool has_type_kind_star = json.requiredBool("has_type_kind_star");
    const bool dynamic_result_length =
        json.requiredBool("fortran_dynamic_result_length");
    const JsonValue &length_json = json.at("length_expression");
    const JsonValue &kind_json = json.at("type_kind");
    auto selectors = std::make_shared<InlineExpressionConstruction>();
    SgExpression *length = expressionFromRef(length_json, nodes, *selectors);
    SgExpression *type_kind = expressionFromRef(kind_json, nodes, *selectors);
    // A source CHARACTER declaration may omit LEN (for example
    // CHARACTER(KIND=1)). Its separate semantic type supplies the exact length;
    // source syntax must preserve the omission instead of inventing a selector.
    if (length == nullptr && !fortran_source_syntax && !dynamic_result_length) {
      throw std::runtime_error(
          "AST JSON CHARACTER type has no exact length selector");
    }
    if (length != nullptr && length == type_kind) {
      throw std::runtime_error("AST JSON CHARACTER selectors alias one node");
    }
    const bool length_has_source_metadata =
        length != nullptr &&
        length->get_fortran_integer_constant_value_is_available();
    const bool kind_has_source_metadata =
        type_kind != nullptr &&
        type_kind->get_fortran_integer_constant_value_is_available();
    if (!fortran_source_syntax &&
        (length_has_source_metadata || kind_has_source_metadata)) {
      throw std::runtime_error(
          "AST JSON semantic CHARACTER type owns source selector metadata");
    }
    if (fortran_source_syntax && type_kind != nullptr &&
        !kind_has_source_metadata) {
      throw std::runtime_error(
          "AST JSON source CHARACTER KIND selector has no folded value");
    }
    if (type_kind != nullptr &&
        !SageInterface::Detail::isValidFortranKindSelector(type_kind))
      throw std::runtime_error("AST JSON CHARACTER KIND selector is not a "
                               "positive integer constant");
    if (length != nullptr &&
        !SageInterface::Detail::isValidFortranLengthSelector(length))
      throw std::runtime_error(
          "AST JSON CHARACTER LEN selector is not an integer or length shape");
    if (fortran_source_syntax && length != nullptr &&
        isSgAsteriskShapeExp(length) == nullptr &&
        isSgColonShapeExp(length) == nullptr &&
        ((!length_has_source_metadata &&
          length->get_fortran_integer_constant_value() != 0) ||
         (isSgValueExp(length) != nullptr && !length_has_source_metadata))) {
      throw std::runtime_error(
          "AST JSON source CHARACTER LEN selector has invalid exact-value "
          "metadata");
    }
    if (dynamic_result_length && (fortran_source_syntax || length != nullptr ||
                                  type_kind == nullptr || has_type_kind_star))
      throw std::runtime_error("AST JSON dynamic CHARACTER result type has "
                               "contradictory source or selector state");
    const bool dependent_semantic_length =
        !fortran_source_syntax && length != nullptr &&
        isSgValueExp(length) == nullptr &&
        isSgAsteriskShapeExp(length) == nullptr &&
        isSgColonShapeExp(length) == nullptr;
    if (!fortran_source_syntax && !dynamic_result_length &&
        !dependent_semantic_length && has_type_kind_star)
      throw std::runtime_error(
          "AST JSON semantic CHARACTER type has a source-only star KIND");
    return recipe(
        [has_type_kind_star, dynamic_result_length, length, type_kind,
         selectors, fortran_source_syntax, dependent_semantic_length] {
          if (fortran_source_syntax &&
              ((length != nullptr && length->get_parent() != nullptr) ||
               (type_kind != nullptr && type_kind->get_parent() != nullptr)))
            throw std::runtime_error(
                "AST JSON source CHARACTER selector is already owned");
          SgExpression *identity_selector = nullptr;
          if (dependent_semantic_length) {
            identity_selector = length;
          } else if (dynamic_result_length) {
            identity_selector = type_kind;
          }
          if (identity_selector != nullptr &&
              identity_selector->get_parent() != nullptr) {
            auto *existing = isSgTypeString(identity_selector->get_parent());
            if (existing == nullptr ||
                existing->get_lengthExpression() != length ||
                existing->get_type_kind() != type_kind ||
                existing->get_hasTypeKindStar() != has_type_kind_star ||
                existing->get_fortran_source_syntax() ||
                existing->get_fortran_dynamic_result_length() !=
                    dynamic_result_length ||
                existing->get_fortran_dynamic_length_pending() ||
                (type_kind != nullptr && type_kind->get_parent() != existing)) {
              throw std::runtime_error(
                  "AST JSON CHARACTER selector conflicts with its exact owner");
            }
            return existing;
          }
          if (!fortran_source_syntax && !dynamic_result_length &&
              !dependent_semantic_length) {
            auto *type = SgTypeString::createType(length, type_kind);
            const bool fresh = type->get_lengthExpression() == length;
            selectors->publish();
            if (fresh)
              SourceTypeConstruction::current()->own(type);
            return type;
          }
          auto *string_type = static_cast<SgTypeString *>(
              selectors->own(new SgTypeString(length)));
          string_type->set_type_kind(type_kind);
          string_type->set_hasTypeKindStar(has_type_kind_star);
          string_type->set_fortran_source_syntax(fortran_source_syntax);
          string_type->set_fortran_dynamic_result_length(dynamic_result_length);
          if (string_type->get_fortran_dynamic_length_pending()) {
            throw std::runtime_error(
                "AST JSON CHARACTER type has an unresolved semantic length");
          }
          if (length != nullptr)
            length->set_parent(string_type);
          if (type_kind != nullptr)
            type_kind->set_parent(string_type);
          if (dependent_semantic_length &&
              (string_type->get_fortran_dynamic_length_pending() ||
               string_type->get_fortran_dynamic_result_length() ||
               string_type->get_lengthExpression() == nullptr ||
               string_type->get_lengthExpression()->get_parent() !=
                   string_type ||
               (string_type->get_type_kind() != nullptr &&
                string_type->get_type_kind()->get_parent() != string_type))) {
            throw std::runtime_error(
                "AST JSON failed to restore exact dependent semantic CHARACTER "
                "type");
          }
          SourceTypeConstruction::current()->take(*selectors);
          return string_type;
        },
        type_kind);
  }
  if (kind == "SgTypeComplex") {
    const bool fixed_kind_available =
        json.requiredBool("fortran_fixed_kind_value_is_available");
    const std::int64_t fixed_kind =
        json.requiredInt("fortran_fixed_kind_value");
    if ((!fixed_kind_available && fixed_kind != 0) ||
        (fixed_kind_available && fixed_kind <= 0)) {
      throw std::runtime_error(
          "AST JSON SgTypeComplex has invalid fixed Fortran KIND metadata");
    }
    const bool has_type_kind_star = json.requiredBool("has_type_kind_star");
    if ((fixed_kind_available || has_type_kind_star) &&
        !fortran_source_syntax) {
      throw std::runtime_error(
          "AST JSON semantic COMPLEX type has source-only KIND metadata");
    }
    const JsonValue &base_json = json.at("base");
    if (fortran_source_syntax &&
        (!base_json.requiredBool("fortran_source_syntax") ||
         (base_json.requiredString("kind") != "SgTypeFloat" &&
          base_json.requiredString("kind") != "SgTypeDouble")))
      throw std::runtime_error(
          "AST JSON source COMPLEX requires a source REAL component");
    const auto base =
        decodeTypeComponent(base_json, nodes, fortran_source_syntax);
    SgExpression *selector = base.source_kind_selector;
    const bool source_selector =
        selector != nullptr &&
        selector->get_fortran_integer_constant_value_is_available();
    if ((source_selector || fixed_kind_available) && !fortran_source_syntax)
      throw std::runtime_error(
          "AST JSON semantic COMPLEX type owns source syntax metadata");
    if (fortran_source_syntax && selector != nullptr && !source_selector)
      throw std::runtime_error(
          "AST JSON source COMPLEX KIND selector has no folded value");
    if (fortran_source_syntax && fixed_kind_available &&
        (base_json.requiredString("kind") != "SgTypeDouble" ||
         !base_json.requiredBool("fortran_fixed_kind_value_is_available") ||
         base_json.requiredInt("fortran_fixed_kind_value") != fixed_kind))
      throw std::runtime_error("AST JSON fixed source COMPLEX type has a "
                               "contradictory component KIND");
    return recipe([base, selector, fortran_source_syntax, has_type_kind_star,
                   fixed_kind_available, fixed_kind] {
      SgType *component = base();
      // Source selectors retain lexical identity. Semantic factories consume
      // the decoded selector and may return an existing canonical component.
      if (fortran_source_syntax)
        ROSE_ASSERT(component->get_type_kind() == selector);
      SgExpression *component_selector = component->get_type_kind();
      if (component_selector != nullptr &&
          component_selector->get_parent() != component)
        throw std::runtime_error(
            "AST JSON COMPLEX component does not solely own its KIND selector");
      const bool fresh =
          fortran_source_syntax ||
          SgTypeComplex::findCanonicalSemanticType(component) == nullptr;
      SgTypeComplex *complex_type = fortran_source_syntax
                                        ? new SgTypeComplex(component)
                                        : SgTypeComplex::createType(component);
      if (fresh)
        SourceTypeConstruction::current()->own(complex_type);
      if (fortran_source_syntax) {
        complex_type->set_hasTypeKindStar(has_type_kind_star);
        complex_type->set_fortran_fixed_kind_value(fixed_kind);
        complex_type->set_fortran_fixed_kind_value_is_available(
            fixed_kind_available);
        complex_type->set_fortran_source_syntax(true);
      }
      return complex_type;
    });
  }
  if (kind == "SgPointerType") {
    const auto base = decodeTypeComponent(json.at("base"), nodes,
                                          fortran_source_syntax, role);
    if (fortran_source_syntax && !base.source_syntax && !base.named &&
        !base.function)
      throw std::runtime_error(
          "AST JSON source pointer has a semantic base type");
    return recipe([base, fortran_source_syntax, role]() -> SgType * {
      SgType *type = base();
      if (role == TypeRole::semantic && !fortran_source_syntax &&
          !type->has_source_prototype()) {
        const bool fresh = type->get_ptr_to() == nullptr;
        auto *pointer = buildCachedJsonPointerType(type);
        if (fresh)
          SourceTypeConstruction::current()->own(pointer);
        return pointer;
      }
      auto *pointer = new SgPointerType(type);
      SourceTypeConstruction::current()->own(pointer);
      pointer->set_fortran_source_syntax(fortran_source_syntax);
      return pointer;
    });
  }
  if (kind == "SgPointerMemberType") {
    const auto fields = pointerMemberSourceFieldsFromJson(json);
    const uint64_t identity =
        requiredPointerMemberTypeSerializationIdentity(json);
    const auto base = decodeTypeComponent(json.at("base"), nodes,
                                          fortran_source_syntax, role);
    const auto owner = decodeTypeComponent(json.at("class_type"), nodes,
                                           fortran_source_syntax);
    return recipe([base, owner, fields, json, identity] {
      if (auto *existing =
              restoredPointerMemberTypeIdentity(json, identity, fields))
        return existing;
      SgType *base_type = base();
      SgType *class_type = owner();
      return restorePointerMemberTypeIdentity(base_type, class_type, json,
                                              fields);
    });
  }
  if (kind == "SgReferenceType" || kind == "SgRvalueReferenceType") {
    const auto base = decodeTypeComponent(json.at("base"), nodes,
                                          fortran_source_syntax, role);
    const bool rvalue = kind == "SgRvalueReferenceType";
    return recipe([base, rvalue, role] {
      SgType *type = base();
      if (role == TypeRole::semantic && !type->has_source_prototype())
        return buildCachedJsonReferenceType(type, rvalue);
      SgType *reference =
          rvalue ? static_cast<SgType *>(new SgRvalueReferenceType(type))
                 : static_cast<SgType *>(new SgReferenceType(type));
      SourceTypeConstruction::current()->own(reference);
      return reference;
    });
  }
  if (kind == "SgArrayType") {
    const int64_t rank = json.requiredInt("rank");
    const int64_t number_of_elements = json.requiredInt("number_of_elements");
    if (rank < std::numeric_limits<int>::min() ||
        rank > std::numeric_limits<int>::max() ||
        number_of_elements < std::numeric_limits<int>::min() ||
        number_of_elements > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON array rank or element count is out of range");
    }
    const bool is_coarray = json.requiredBool("is_coarray");
    const bool variable_length = json.requiredBool("is_variable_length_array");
    const JsonValue &base_json = json.at("base");
    const JsonValue &index_json = json.at("index");
    const JsonValue &dimensions_json = json.at("dim_info");
    const bool has_dimensions = dimensions_json.requiredBool("present");
    const int64_t index_id = index_json.requiredInt("node");
    if (index_id < 0)
      throw std::runtime_error(
          "AST JSON expression reference has a negative node ID");
    const bool has_index =
        index_id != 0 || index_json.find("owned_kind") != nullptr;
    if (has_dimensions &&
        dimensions_json.at("expressions").kind != JsonValue::Kind::Array)
      throw std::runtime_error("AST JSON expression list is not an array");
    if (rank < 0 ||
        (has_dimensions && (has_index || rank == 0 ||
                            dimensions_json.at("expressions").array.size() !=
                                static_cast<uint64_t>(rank))) ||
        (!has_dimensions && rank != 0))
      throw std::runtime_error(
          "AST JSON array has inconsistent index, dimensions or rank");
    auto shape = std::make_shared<InlineExpressionConstruction>();
    SgExpression *index = expressionFromRef(index_json, nodes, *shape);
    SgExprListExp *dimensions =
        exprListExpFromTypeJson(dimensions_json, nodes, *shape);
    const auto base =
        decodeTypeComponent(base_json, nodes, fortran_source_syntax, role);
    if (fortran_source_syntax && !base.source_syntax && !base.named) {
      throw std::runtime_error(
          "AST JSON source array has a semantic base type");
    }
    return recipe([base, shape, index, dimensions, rank, number_of_elements,
                   is_coarray, variable_length, fortran_source_syntax] {
      auto *array = static_cast<SgArrayType *>(
          shape->own(new SgArrayType(base(), index)));
      ROSE_ASSERT(array->get_index() == index);
      if (index != nullptr) {
        index->set_parent(array);
      }
      array->set_dim_info(dimensions);
      if (dimensions != nullptr) {
        dimensions->set_parent(array);
      }
      array->set_rank(static_cast<int>(rank));
      array->set_number_of_elements(static_cast<int>(number_of_elements));
      array->set_isCoArray(is_coarray);
      array->set_is_variable_length_array(variable_length);
      array->set_fortran_source_syntax(fortran_source_syntax);
      if (!fortran_source_syntax &&
          SemanticArrayConstruction::current() != nullptr)
        SemanticArrayConstruction::current()->take(array, shape);
      else
        publishTypeExpressions(*shape, fortran_source_syntax);
      return array;
    });
  }
  if (kind == "SgModifierType") {
    const auto modifier = typeModifierFieldsFromJson(json, kind, "type");
    const auto base = decodeTypeComponent(json.at("base"), nodes,
                                          fortran_source_syntax, role);
    if (fortran_source_syntax && !base.source_syntax && !base.named)
      throw std::runtime_error(
          "AST JSON source modifier has a semantic base type");
    return recipe(
        [base, modifier, fortran_source_syntax, role] {
          auto candidate = std::make_unique<SgModifierType>(base());
          modifier.initialize(candidate->get_typeModifier());
          candidate->set_fortran_source_syntax(fortran_source_syntax);
          auto *input = candidate.get();
          auto *result =
              role == TypeRole::cxx_source || fortran_source_syntax ||
                      input->has_source_prototype()
                  ? input
                  : SgModifierType::insertModifierTypeIntoTypeTable(input);
          candidate.release();
          if (result == input)
            SourceTypeConstruction::current()->own(result);
          return result;
        },
        nullptr,
        base.top_level_qualified || modifier.bits[SgTypeModifier::e_restrict] ||
            modifier.const_volatile == SgConstVolatileModifier::e_const ||
            modifier.const_volatile == SgConstVolatileModifier::e_volatile ||
            modifier.const_volatile ==
                SgConstVolatileModifier::e_const_volatile);
  }
  if (kind == "SgTypedefType") {
    const auto identity = json.requiredInt("declaration");
    auto *declaration =
        identity > 0 ? isSgTypedefDeclaration(
                           nodeById(nodes, static_cast<uint64_t>(identity)))
                     : nullptr;
    if (declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgTypedefType requires a valid declaration id");
    SgTypedefType *type = declaration->get_type();
    if (type == nullptr || type->get_declaration() != declaration)
      throw std::runtime_error("AST JSON SgTypedefType has no exact "
                               "constructed declaration wrapper");
    if (SageInterface::containsUnknownType(type))
      throw std::runtime_error(
          "AST JSON SgTypedefType has an unresolved declaration base type");
    return recipe([type] { return type; }, nullptr,
                  SageInterface::isConstType(type) ||
                      SageInterface::isVolatileType(type) ||
                      SageInterface::isRestrictType(type));
  }
  if (kind == "SgClassType") {
    const JsonValue *declaration_json = json.find("declaration");
    const JsonValue *external_json = json.find("external_declaration");
    if ((declaration_json == nullptr) == (external_json == nullptr))
      throw std::runtime_error(
          "AST JSON SgClassType requires exactly one declaration identity");
    std::function<SgClassDeclaration *()> declaration;
    if (declaration_json != nullptr) {
      const int64_t identity = declaration_json->asInt();
      if (identity <= 0)
        throw std::runtime_error("AST JSON SgClassType declaration id is zero");
      auto *node = isSgClassDeclaration(
          nodeById(nodes, static_cast<uint64_t>(identity)));
      if (node == nullptr)
        throw std::runtime_error(
            "AST JSON SgClassType declaration id is not a class declaration");
      declaration = [node] { return node; };
    } else {
      if (!external_json->requiredBool("present"))
        throw std::runtime_error(
            "AST JSON SgClassType external declaration is absent");
      declaration = decodeExternalClassDeclaration(*external_json);
    }
    return recipe(
        [declaration] { return classTypeForDeclaration(declaration()); });
  }
  if (kind == "SgEnumType") {
    const auto identity = json.requiredInt("declaration");
    auto *declaration =
        identity > 0 ? isSgEnumDeclaration(
                           nodeById(nodes, static_cast<uint64_t>(identity)))
                     : nullptr;
    if (declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgEnumType requires a valid declaration id");
    SgEnumType *type = declaration->get_type();
    if (type == nullptr || type->get_declaration() != declaration ||
        declaration->get_firstNondefiningDeclaration() != declaration)
      throw std::runtime_error(
          "AST JSON SgEnumType declaration has no exact constructed type");
    return recipe([type] { return type; });
  }
  if (kind == "SgNonrealType") {
    if (json.find("aliased_type") != nullptr)
      throw std::runtime_error("AST JSON SgNonrealType alias projection must "
                               "be stored on its declaration");
    const auto identity = json.requiredInt("declaration");
    auto *declaration =
        identity > 0
            ? isSgNonrealDecl(nodeById(nodes, static_cast<uint64_t>(identity)))
            : nullptr;
    if (declaration == nullptr)
      throw std::runtime_error(
          "AST JSON SgNonrealType requires a valid declaration id");
    auto *type = isSgNonrealType(declaration->get_type());
    if (type == nullptr || type->get_declaration() != declaration ||
        type->get_parent() != declaration)
      throw std::runtime_error("AST JSON SgNonrealType conflicts with its "
                               "exact declaration owner");
    type->validate_aliased_type();
    return recipe([type] { return type; });
  }
  if (kind == "SgDeclType") {
    const bool gnu = json.requiredBool("is_gnu_decltype");
    const JsonValue &base_json = json.at("base_type");
    const auto base =
        decodeTypeComponent(base_json, nodes, fortran_source_syntax);
    auto operands = std::make_shared<InlineExpressionConstruction>();
    SgExpression *expression = nullptr;
    if (const auto *value = json.find("base_expression"))
      expression = expressionFromRef(*value, nodes, *operands);
    if (expression == nullptr)
      throw std::runtime_error(
          "AST JSON SgDeclType requires a structured base_expression node");
    return recipe([base, expression, operands, gnu] {
      SgType *base_type = base();
      const bool fresh = expression->get_parent() == nullptr;
      auto *type = buildDeclType(expression, base_type, gnu);
      if (fresh) {
        operands->own(type);
        SourceTypeConstruction::current()->take(*operands);
      } else {
        operands->publish();
      }
      return type;
    });
  }
  if (kind == "SgTypeOfType") {
    const JsonValue *base_json = json.find("base_type");
    const JsonValue *expression_json = json.find("base_expression");
    if ((base_json == nullptr) == (expression_json == nullptr))
      throw std::runtime_error(
          "AST JSON SgTypeOfType requires exactly one typed operand");
    if (base_json != nullptr) {
      const auto base = decodeTypeComponent(*base_json, nodes, false);
      return recipe([base] {
        return SourceTypeConstruction::current()->own(
            SageBuilder::buildTypeOfType(nullptr, base()));
      });
    }
    auto operands = std::make_shared<InlineExpressionConstruction>();
    SgExpression *expression =
        expressionFromRef(*expression_json, nodes, *operands);
    if (expression == nullptr ||
        SageInterface::containsUnknownType(expression->get_type()))
      throw std::runtime_error(
          "AST JSON SgTypeOfType requires an expression with an exact type");
    (void)typeOfOperandOwner(expression);
    return recipe([expression, operands] {
      // Repeated references to one collected operand name its same lexical
      // type owner. Equal but separately owned expressions remain distinct.
      if (auto *type = typeOfOperandOwner(expression)) {
        operands->publish();
        return type;
      }
      auto *type = SageBuilder::buildTypeOfType(expression, nullptr);
      operands->own(type);
      ROSE_ASSERT(type->get_base_type() == expression->get_type());
      SourceTypeConstruction::current()->take(*operands);
      return type;
    });
  }
  if (kind == "SgTemplateType") {
    const TemplateTypeFields fields = templateTypeFieldsFromJson(json);
    const SgName &name = fields.name;
    const int position = fields.position;
    const int depth = fields.depth;
    const bool packed = fields.packed;
    const JsonValue &serialized_arguments = json.at("substitution_arguments");
    if (serialized_arguments.kind != JsonValue::Kind::Array ||
        (!fields.is_pack_substitution && !serialized_arguments.array.empty()))
      throw std::runtime_error(
          "AST JSON template type has invalid substitution arguments");
    SgTemplateArgumentPtrList arguments;
    std::unordered_set<SgTemplateArgument *> seen;
    for (const JsonValue &entry : serialized_arguments.array) {
      const int64_t id = entry.asInt();
      if (id <= 0)
        throw std::runtime_error(
            "AST JSON substitution argument ID must be positive");
      auto *argument = nodeByIdAs<SgTemplateArgument>(nodes, id);
      if (argument->get_argumentType() != SgTemplateArgument::type_argument ||
          SageInterface::containsUnknownType(argument->get_type()) ||
          !seen.insert(argument).second)
        throw std::runtime_error(
            "AST JSON substitution requires distinct typed arguments");
      arguments.push_back(argument);
    }
    const uint64_t parameter_id =
        json.requiredNonnegativeNodeId("template_parameter");
    SgTemplateParameter *parameter =
        parameter_id != 0 ? nodeByIdAs<SgTemplateParameter>(nodes, parameter_id)
                          : nullptr;

    if (parameter != nullptr && parameter->get_type() != nullptr) {
      SgTemplateType *canonical_type = isSgTemplateType(parameter->get_type());
      if (canonical_type == nullptr ||
          canonical_type->get_is_pack_substitution() ||
          canonical_type->get_template_parameter_position() != position ||
          canonical_type->get_template_parameter_depth() != depth ||
          canonical_type->get_template_parameter() != parameter ||
          !(canonical_type->get_canonical_source_identity() ==
            fields.source_identity)) {
        throw std::runtime_error(
            "AST JSON SgTemplateType disagrees with its exact template "
            "parameter coordinates");
      }
      if (role == TypeRole::semantic && !fields.is_pack_substitution && canonical_type->get_name() == name &&
          canonical_type->get_packed() == packed) {
        return recipe([canonical_type] { return canonical_type; });
      }
    }

    return recipe([name, position, depth, packed, parameter, arguments,
                   substitution = fields.is_pack_substitution,
                   identity = fields.source_identity] {
      InlineExpressionConstruction operands;
      for (SgTemplateArgument *argument : arguments) {
        if (argument->get_parent() != nullptr)
          throw std::runtime_error(
              "AST JSON substitution argument already has an owner");
        operands.borrow(argument);
      }
      auto template_type = std::make_unique<SgTemplateType>(name);
      template_type->set_template_parameter_position(position);
      template_type->set_template_parameter_depth(depth);
      if (identity.has_value()) {
        template_type->initialize_canonical_source_identity(*identity);
      }
      template_type->set_packed(packed);
      template_type->set_is_pack_substitution(substitution);
      template_type->set_template_parameter(parameter);
      template_type->get_tpl_args() = arguments;
      for (SgTemplateArgument *argument : arguments)
        argument->set_parent(template_type.get());
      auto *type = template_type.release();
      operands.own(type);
      SourceTypeConstruction::current()->take(operands);
      return type;
    });
  }
  if (kind == "SgFunctionType" || kind == "SgMemberFunctionType") {
    const bool member = kind == "SgMemberFunctionType";
    const FunctionTypeFields fields = functionTypeFieldsFromJson(json, member);
    auto *prototype = fields.source_argument_list != 0
                          ? nodeByIdAs<SgFunctionParameterTypeList>(
                                nodes, fields.source_argument_list)
                          : nullptr;
    if (prototype != nullptr &&
        (prototype->get_source_parameters() == nullptr ||
         prototype->get_source_parameter_scope() == nullptr ||
         prototype->get_source_parameters()->get_args().size() !=
             fields.arguments.array.size() - (fields.has_ellipses ? 1 : 0)))
      throw std::runtime_error(
          "AST JSON source function type has no constructed prototype owner");
    const auto return_type =
        decodeTypeComponent(fields.return_type, nodes, fortran_source_syntax);
    if (fortran_source_syntax && !return_type.source_syntax &&
        !return_type.named)
      throw std::runtime_error(
          "AST JSON source function type has a semantic return type");
    const auto class_type =
        member ? decodeNullableTypeComponent(*fields.class_type, nodes,
                                             fortran_source_syntax)
               : std::nullopt;
    const auto arguments =
        decodeFunctionArguments(fields, nodes, fortran_source_syntax);
    const auto exception =
        decodeFunctionException(fields.exception, nodes, fortran_source_syntax);
    const std::optional<FullTypeRecipe> semantic_signature =
        fields.semantic_signature.requiredBool("present")
            ? std::optional<FullTypeRecipe>(
                  decodeSemanticFunctionType(fields.semantic_signature, nodes))
            : std::nullopt;
    return recipe([return_type, class_type, arguments, member, prototype, json,
                   exception, semantic_signature,
                   ellipsis = fields.has_ellipses, specifier = fields.specifier,
                   hasPrototype = fields.has_prototype,
                   positions = fields.positions,
                   trailing_return = fields.trailing_return,
                   fortran_source_syntax]() -> SgType * {
      const uint64_t hash = exactJsonValueHash(json);
      if (prototype != nullptr) {
        if (!functionTypeDeserializationIdentityActive)
          throw std::runtime_error(
              "AST JSON prototype has no exact function identity transaction");
        for (const auto &identity : functionTypeDeserializationIdentities[hash])
          if (identity.serialized_type.exactlyEquals(json)) {
            exception.requireExpressionOwner(identity.type);
            return identity.type;
          }
        if (prototype->get_parent() != nullptr ||
            !prototype->get_arguments().empty())
          throw std::runtime_error(
              "AST JSON prototype argument list has a conflicting owner");
      }
      exception.requireExpressionOwner();
      auto *semantic = semantic_signature
                           ? isSgFunctionType((*semantic_signature)())
                           : nullptr;
      if (semantic_signature && semantic == nullptr)
        throw std::runtime_error(
            "AST JSON function semantic signature has a nonfunction type");
      SgType *result_type = return_type();
      SgType *owner = class_type ? (*class_type)() : nullptr;
      const auto parameters = constructFunctionArguments(arguments);
      requireExactFunctionComponents(result_type, owner, parameters);
      std::unique_ptr<SgFunctionParameterTypeList> parameter_list;
      if (prototype == nullptr)
        parameter_list = std::make_unique<SgFunctionParameterTypeList>();
      SgFunctionParameterTypeList *owned =
          prototype != nullptr ? prototype : parameter_list.get();
      InlineExpressionConstruction operands;
      if (prototype != nullptr)
        operands.borrow(prototype);
      installFunctionArguments(owned, parameters, positions);
      auto function =
          constructFunctionType(result_type, ellipsis, owned, member, owner,
                                specifier, exception, semantic, hasPrototype);
      if (prototype == nullptr)
        parameter_list.release();
      function->set_fortran_source_syntax(fortran_source_syntax);
      function->set_source_trailing_return(trailing_return);
      auto *type = function.release();
      operands.own(type);
      exception.operands->transferTo(operands);
      SourceTypeConstruction::current()->take(operands);
      if (prototype != nullptr)
        functionTypeDeserializationIdentities[hash].push_back({json, type});
      return type;
    });
  }
  throw std::runtime_error("AST JSON deserializer does not support Sage type " +
                           kind);
}

FullTypeRecipe decodeSemanticFunctionType(const JsonValue &json,
                                          const NodeMap &nodes) {
  if (!functionTypeDeserializationIdentityActive)
    throw std::runtime_error(
        "AST JSON semantic function type was reconstructed outside its exact "
        "identity transaction");
  const bool member =
      semanticFunctionTypeKindFromJson(json) == V_SgMemberFunctionType;
  if (json.requiredBool("fortran_source_syntax"))
    return decodeFullType(json, nodes);

  const FunctionTypeFields fields = functionTypeFieldsFromJson(json, member);
  if (!fields.canonical)
    return decodeFullTypeUncached(json, nodes);
  const auto return_type =
      decodeTypeComponent(fields.return_type, nodes, false);
  const auto arguments = decodeFunctionArguments(fields, nodes, false);
  const auto class_type =
      member ? decodeNullableTypeComponent(*fields.class_type, nodes, false)
             : std::nullopt;
  const auto exception =
      decodeFunctionException(fields.exception, nodes, false, true);
  const uint64_t hash = exactJsonValueHash(json);
  return {
      [return_type, arguments, class_type, member, json, hash, exception,
       ellipsis = fields.has_ellipses, positions = fields.positions,
       hasPrototype = fields.has_prototype, specifier = fields.specifier] {
        auto &bucket = functionTypeDeserializationIdentities[hash];
        for (const auto &identity : bucket) {
          if (identity.type == nullptr)
            throw std::runtime_error(
                "AST JSON semantic function-type identity cache is malformed");
          if (identity.serialized_type.exactlyEquals(json)) {
            exception.requireExpressionOwner(identity.type);
            return static_cast<SgType *>(identity.type);
          }
        }
        exception.requireExpressionOwner();
        SgType *result_type = return_type();
        const auto decoded = constructFunctionArguments(arguments);
        SgType *owner = class_type ? (*class_type)() : nullptr;
        requireExactFunctionComponents(result_type, owner, decoded);
        auto parameters = std::make_unique<SgFunctionParameterTypeList>();
        installFunctionArguments(parameters.get(), decoded, positions);
        auto candidate = constructFunctionType(
            result_type, ellipsis, parameters.get(), member, owner, specifier,
            exception, nullptr, hasPrototype);
        parameters.release();
        const bool fresh =
            SgNode::get_globalFunctionTypeTable()->lookup_exact_function_type(
                candidate.get()) == nullptr;
        SgFunctionType *type =
            SageBuilder::internFunctionType(std::move(candidate));
        ROSE_ASSERT(type != nullptr);
        if (fresh) {
          exception.operands->own(type);
          SourceTypeConstruction::current()->take(*exception.operands);
        } else {
          exception.operands->publish();
        }
        if (type->get_fortran_source_syntax())
          throw std::runtime_error(
              "AST JSON semantic function type did not reconstruct an exact "
              "semantic function type");
        bucket.push_back({json, type});
        return static_cast<SgType *>(type);
      },
      false, false, true};
}

FullTypeRecipe decodeFullType(const JsonValue &json, const NodeMap &nodes,
                              const char *exact_type_error, TypeRole role) {
  if (json.kind != JsonValue::Kind::Object)
    throw std::runtime_error("AST JSON type record is not an object");
  if (!json.requiredBool("present"))
    throw std::runtime_error("AST JSON required type record is absent");
  const bool source = json.requiredBool("fortran_source_syntax");
  const std::string kind = json.requiredString("kind");
  if (kind == "SgTypeUnknown" || kind == "SgTypeDefault")
    throw std::runtime_error(exact_type_error);
  const bool array = kind == "SgArrayType";
  const JsonValue *identity_json = json.find("semantic_array_identity");
  if (serializedTypeOwnsSourceIdentity(json)) {
    if (identity_json != nullptr)
      throw std::runtime_error(
          "AST JSON source array owns a semantic array identity");
    const int64_t raw_identity = json.requiredInt("source_type_identity");
    if (raw_identity <= 0)
      throw std::runtime_error("AST JSON source type identity is not positive");
    const uint64_t identity = static_cast<uint64_t>(raw_identity);
    const auto published = sourceTypeDeserializationIdentities.find(identity);
    if (published != sourceTypeDeserializationIdentities.end()) {
      SgType *type = published->second.type;
      if (!typeOwnsSourceJsonIdentity(type) ||
          type->get_fortran_source_syntax() != source ||
          preservedSourceTypeJsonIdentity(type) != identity ||
          !published->second.serialized_type.exactlyEquals(json))
        throw std::runtime_error(
            "AST JSON repeated source type identity is contradictory");
      return {[type] { return type; },
              source,
              false,
              isSgFunctionType(type) != nullptr,
              type->get_type_kind(),
              SageInterface::isConstType(type) ||
                  SageInterface::isVolatileType(type) ||
                  SageInterface::isRestrictType(type)};
    }
    ROSE_ASSERT(FullTypeDecoding::current() != nullptr);
    auto &sources = FullTypeDecoding::current()->sources;
    const auto inserted =
        sources.emplace(identity, FullTypeDecoding::Source{json, std::nullopt});
    if (!inserted.second) {
      const auto &existing = inserted.first->second;
      if (!existing.json.exactlyEquals(json) || !existing.recipe)
        throw std::runtime_error("AST JSON repeated source type identity is "
                                 "contradictory or cyclic");
      return *existing.recipe;
    }
    const auto decoded = decodeFullTypeUncached(json, nodes, role);
    FullTypeRecipe recipe = decoded;
    recipe.construct = [decoded, json, identity] {
      const auto existing = sourceTypeDeserializationIdentities.find(identity);
      if (existing != sourceTypeDeserializationIdentities.end())
        return existing->second.type;
      SgType *type = decoded();
      attachSourceTypeJsonIdentity(type, identity);
      ROSE_ASSERT(SourceTypeConstruction::current() != nullptr);
      SourceTypeConstruction::current()->recordIdentity(identity, json, type);
      return type;
    };
    sources.at(identity).recipe = recipe;
    return recipe;
  }
  if (json.find("source_type_identity") != nullptr)
    throw std::runtime_error(
        "AST JSON semantic type owns a source type identity");
  if (kind == "SgFunctionType" || kind == "SgMemberFunctionType")
    return decodeSemanticFunctionType(json, nodes);
  if (kind == "SgTemplateType") {
    const JsonValue &source_identity = json.at("canonical_source_identity");
    if (role == TypeRole::cxx_source || source_identity.kind == JsonValue::Kind::Null)
      return decodeFullTypeUncached(json, nodes, role);
    if (!templateTypeDeserializationIdentityActive)
      throw std::runtime_error("AST JSON canonical template type has no active "
                               "exact identity transaction");
    const uint64_t hash = exactJsonValueHash(source_identity);
    const auto decoded = decodeFullTypeUncached(json, nodes);
    const uint64_t parameter_id =
        json.requiredNonnegativeNodeId("template_parameter");
    auto *parameter = parameter_id != 0
                          ? nodeByIdAs<SgTemplateParameter>(nodes, parameter_id)
                          : nullptr;
    SgTemplateArgumentPtrList arguments;
    for (const JsonValue &id : json.at("substitution_arguments").array)
      arguments.push_back(nodeByIdAs<SgTemplateArgument>(nodes, id.asInt()));
    return {[decoded, json, hash, parameter, arguments] {
              auto &bucket = templateTypeDeserializationIdentities[hash];
              for (const auto &identity : bucket) {
                if (identity.type == nullptr)
                  throw std::runtime_error(
                      "AST JSON template-type identity cache is malformed");
                if (identity.serialized_type.exactlyEquals(json)) {
                  if (identity.type->get_template_parameter() != parameter ||
                      identity.type->get_tpl_args() != arguments)
                    throw std::runtime_error(
                        "AST JSON template-type identity conflicts with its "
                        "exact parameter or argument binding");
                  return static_cast<SgType *>(identity.type);
                }
              }
              auto *type = isSgTemplateType(decoded());
              if (type == nullptr ||
                  !type->get_canonical_source_identity().has_value())
                throw std::runtime_error("AST JSON canonical template type "
                                         "lost its exact source identity");
              bucket.push_back({json, type});
              return static_cast<SgType *>(type);
            },
            false, false, false};
  }
  if (array != (identity_json != nullptr))
    throw std::runtime_error(
        "AST JSON semantic array identity disagrees with the type kind");
  if (!array)
    return decodeFullTypeUncached(json, nodes, role);
  if (!arrayTypeDeserializationIdentityActive)
    throw std::runtime_error(
        "AST JSON semantic array has no active exact identity transaction");
  const int64_t raw_identity = identity_json->asInt();
  if (raw_identity <= 0)
    throw std::runtime_error(
        "AST JSON semantic array identity is not positive");
  const uint64_t identity = static_cast<uint64_t>(raw_identity);
  if (!semanticArrayDependenciesReady(json,
                                      SemanticArrayConstruction::current()))
    throw std::runtime_error(
        "AST JSON type has an uncommitted semantic array dependency");
  ROSE_ASSERT(FullTypeDecoding::current() != nullptr);
  FullTypeDecoding::current()->reserveArray(identity, json);
  const auto decoded = decodeFullTypeUncached(json, nodes, role);
  return {
      [decoded, json, identity] {
        const auto found = arrayTypeDeserializationIdentities.find(identity);
        if (found != arrayTypeDeserializationIdentities.end()) {
          if (found->second.type == nullptr ||
              !found->second.serialized_type.exactlyEquals(json) ||
              preservedSemanticArrayJsonIdentity(found->second.type) !=
                  identity)
            throw std::runtime_error(
                "AST JSON repeated semantic array identity is contradictory");
          return static_cast<SgType *>(found->second.type);
        }
        auto *type = isSgArrayType(decoded());
        if (type == nullptr || type->get_fortran_source_syntax())
          throw std::runtime_error("AST JSON semantic array identity "
                                   "reconstructed a non-semantic array");
        attachSemanticArrayJsonIdentity(type, identity);
        if (!arrayTypeDeserializationIdentities
                 .emplace(identity,
                          ArrayTypeDeserializationIdentity{json, type})
                 .second)
          throw std::runtime_error(
              "AST JSON semantic array identity insertion failed");
        return static_cast<SgType *>(type);
      },
      false, false, false};
}
std::function<SgType *()> decodeExactType(const JsonValue &json,
                                          const NodeMap &nodes,
                                          const std::string &exact_type_error,
                                          TypeRole role = TypeRole::semantic) {
  FullTypeDeserializationIdentityGuard identity_guard;
  FullTypeDecoding decoding;
  const auto recipe =
      decodeFullType(json, nodes, exact_type_error.c_str(), role);
  return [recipe, exact_type_error] {
    FullTypeDeserializationIdentityGuard identity_guard;
    FullTypeDecoding decoding;
    SourceTypeConstruction construction;
    SgType *type = recipe();
    if (SageInterface::containsUnknownType(type))
      throw std::runtime_error(exact_type_error);
    return construction.publish(type);
  };
}
} // namespace

SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes,
                     const std::string &exact_type_error) {
  FullTypeDeserializationIdentityGuard identity_guard;
  FullTypeDecoding decoding;
  SourceTypeConstruction construction;
  return construction.publish(decodeExactType(json, nodes, exact_type_error)());
}

ExternalInitializedNameFields externalInitializedNameFieldsFromJson(
    const JsonValue &json, const NodeMap &nodes, const std::string &context) {
  const SgName name = json.requiredName("name");
  const JsonValue &location = json.at("location");
  (void)json.at("alignment_specifiers");
  (void)json.at("initializer");
  if (requiredPreinitialization(json, context + " initializedName") !=
          SgInitializedName::e_unknown_preinitialization ||
      requiredOptionalUnsignedInteger(json, "ctor_initializer_source_order",
                                      context)
          .has_value() ||
      json.requiredNonnegativeNodeId("ctor_initializer_field_target") != 0)
    throw std::runtime_error(
        "AST JSON " + context +
        " external initializedName has a constructor preinitialization role");
  const auto metadata = initializedNameMetadataFromJson(json);
  const auto positions = requiredSourcePositionFieldsFromJson(
      location, context + " initializedName " + name.getString());
  const auto optional_type =
      [&](const char *field,
          TypeRole role = TypeRole::semantic) -> std::function<SgType *()> {
    const JsonValue &type = json.at(field);
    if (!type.requiredBool("present"))
      return []() -> SgType * { return nullptr; };
    return decodeExactType(
        type, nodes, "AST JSON optional type requires an exact type", role);
  };
  return {name,
          metadata,
          positions,
          decodeExactType(json.at("type"), nodes,
                          "AST JSON type requires an exact type"),
          optional_type("auto_decltype"),
          optional_type("fortran_source_type"),
          optional_type("cxx_source_type", TypeRole::cxx_source),
          exactBoundSymbolFromJson(
              json.at("fortran_source_derived_type_symbol"), nodes)};
}

SgFortranImplicitRange *
constructFortranImplicitRange(char first, std::optional<char> last,
                              SgFortranImplicitRange::type_spec_enum type_spec,
                              const JsonValue &properties,
                              const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  SourceTypeConstruction construction;
  SgType *type = typeFromJson(properties.at("type"), nodes);
  SgSymbol *symbol = exactBoundSymbolFromJson(
      properties.at("source_derived_type_symbol"), nodes);
  if (!SgFortranImplicitRange::is_valid_range(first, last, type, type_spec,
                                              symbol))
    throw std::runtime_error(
        "AST JSON implicit range has invalid type or symbol");
  auto range = std::make_unique<SgFortranImplicitRange>(first, last, type,
                                                        type_spec, symbol);
  construction.publish(type);
  return range.release();
}

VariantT semanticFunctionTypeKindFromJson(const JsonValue &json) {
  if (json.kind != JsonValue::Kind::Object || !json.requiredBool("present"))
    throw std::runtime_error(
        "AST JSON semantic function type is absent or malformed");
  const std::string kind = json.requiredString("kind");
  if (kind == "SgFunctionType")
    return V_SgFunctionType;
  if (kind == "SgMemberFunctionType")
    return V_SgMemberFunctionType;
  throw std::runtime_error(
      "AST JSON semantic function type has non-function kind " + kind);
}

SgFunctionType *semanticFunctionTypeFromJson(const JsonValue &json,
                                             const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  FullTypeDecoding decoding;
  SourceTypeConstruction construction;
  return isSgFunctionType(
      construction.publish(decodeSemanticFunctionType(json, nodes)()));
}

SgFunctionType *nullableFunctionSyntaxTypeFromJson(const JsonValue &json,
                                                   const NodeMap &nodes) {
  FullTypeDeserializationIdentityGuard identity_guard;
  FullTypeDecoding decoding;
  if (!json.requiredBool("present"))
    return nullptr;
  const std::string kind = json.requiredString("kind");
  if (kind != "SgFunctionType" && kind != "SgMemberFunctionType")
    throw std::runtime_error(
        "AST JSON function syntax has a non-function type");
  SourceTypeConstruction construction;
  return isSgFunctionType(
      construction.publish((json.requiredBool("fortran_source_syntax")
                                ? decodeFullType(json, nodes)
                                : decodeFullTypeUncached(json, nodes))()));
}

SgFunctionType *
nullableDeclaredExceptionSignatureFromJson(const JsonValue &json,
                                           const NodeMap &nodes) {
  if (!json.requiredBool("present"))
    return nullptr;
  const bool member =
      semanticFunctionTypeKindFromJson(json) == V_SgMemberFunctionType;
  const auto fields = functionTypeFieldsFromJson(json, member);
  if (fields.canonical || fields.source_argument_list != 0 ||
      fields.trailing_return || json.requiredBool("fortran_source_syntax") ||
      fields.exception.kind == SgFunctionType::e_exception_none)
    throw std::runtime_error("AST JSON declaration exception signature has "
                             "invalid ownership or kind");
  return nullableFunctionSyntaxTypeFromJson(json, nodes);
}

SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (!json.requiredBool("present")) {
    return nullptr;
  }
  return typeFromJson(json, nodes,
                      "AST JSON optional type requires an exact type");
}

SgType *nullableCxxSourceTypeFromJson(const JsonValue &json,
                                      const NodeMap &nodes) {
  if (!json.requiredBool("present"))
    return nullptr;
  return decodeExactType(json, nodes,
                         "AST JSON C++ source type requires an exact type",
                         TypeRole::cxx_source)();
}

SgType *targetSizeTypeFromJson(const JsonValue &json, const NodeMap &nodes,
                               bool target_available) {
  SourceTypeConstruction construction;
  SgType *type = nullableTypeFromJson(json, nodes);
  if (target_available && type == nullptr)
    throw std::runtime_error(
        "AST JSON available target ABI requires its exact size_t type");
  if (type != nullptr && !type->isUnsignedType())
    throw std::runtime_error(
        "AST JSON target size_t requires an exact unsigned integer type");
  construction.publish();
  return type;
}

SgSourceFile *createSourceFileFromRecord(const NodeRecord &record,
                                         SgProject *project, SgFileList *owner,
                                         const NodeMap &dependencies) {
  if (record.kind != "SgSourceFile")
    throw std::runtime_error("AST JSON expected a source-file record");
  (void)record.flags.requiredBool("is_modified");
  const JsonValue &p = record.properties;
  const auto standard = requiredEnum<SgFile::standard_enum>(
      p, "standard", "SgSourceFile",
      {SgFile::e_default_standard, SgFile::e_c89_standard,
       SgFile::e_c90_standard,     SgFile::e_c99_standard,
       SgFile::e_c11_standard,     SgFile::e_c17_standard,
       SgFile::e_c23_standard,     SgFile::e_c2y_standard,
       SgFile::e_cxx98_standard,   SgFile::e_cxx03_standard,
       SgFile::e_cxx11_standard,   SgFile::e_cxx14_standard,
       SgFile::e_cxx17_standard,   SgFile::e_cxx20_standard,
       SgFile::e_cxx23_standard,   SgFile::e_cxx26_standard,
       SgFile::e_f77_standard,     SgFile::e_f90_standard,
       SgFile::e_f95_standard,     SgFile::e_f03_standard,
       SgFile::e_f08_standard,     SgFile::e_f18_standard});
  const bool gnu_standard = p.requiredBool("gnu_standard");
  const auto target_abi = targetAbiFromJson(p.at("target_abi"));
  const auto flags = validateConstructionFlags(record);
  auto ownershipPaths = [&](const char *field) {
    SgStringList paths = stringListFromJson(p.at(field), field);
    std::unordered_set<std::string> unique;
    for (const std::string &path : paths) {
      if (path.find('\0') != std::string::npos ||
          !std::filesystem::path(path).is_absolute() ||
          !unique.insert(path).second)
        throw std::runtime_error("AST JSON " + std::string(field) +
                                 " requires distinct absolute physical paths");
    }
    return paths;
  };
  auto include_paths = ownershipPaths("frontend_include_ownership_paths");
  auto system_paths = ownershipPaths("frontend_system_include_ownership_paths");
  auto external_paths = ownershipPaths("frontend_external_ownership_paths");
  const std::unordered_set<std::string> includes(include_paths.begin(),
                                                 include_paths.end());
  const std::unordered_set<std::string> systems(system_paths.begin(),
                                                system_paths.end());
  const std::unordered_set<std::string> externals(external_paths.begin(),
                                                  external_paths.end());
  for (const std::string &path : system_paths) {
    if (includes.count(path) == 0 || externals.count(path) == 0)
      throw std::runtime_error(
          "AST JSON system include requires include and external ownership");
  }
  for (const std::string &path : external_paths) {
    if (includes.count(path) != 0 && systems.count(path) == 0)
      throw std::runtime_error(
          "AST JSON physical path has conflicting external and application "
          "include ownership");
  }
  std::vector<std::string> args = commandLineFromJson(p);
  auto positions =
      requiredSourcePositionsFromJson(record.location, record.kind);
  if (positions.end != nullptr)
    throw std::runtime_error("AST JSON " + record.kind +
                             " file has no endOfConstruct position");
  SourceTypeConstruction types;
  SgType *target_size_type = targetSizeTypeFromJson(
      p.at("target_size_type"), dependencies, target_abi.available);
  SgNode::SourceStateConstruction construction;
  RecordNodeConstruction ownership;
  const bool generated = p.requiredBool("is_generated_source");
  const bool command_line = p.requiredBool("is_command_line_input_source");
  const std::string filename = p.requiredString("source_filename_with_path");
  if ((generated && command_line) || args.size() < 2 ||
      filename.find('\0') != std::string::npos ||
      !std::filesystem::path(filename).is_absolute() ||
      positions.start == nullptr ||
      p.requiredString("source_filename_without_path") !=
          std::filesystem::path(filename).filename().string())
    throw std::runtime_error(
        "AST JSON source file has an invalid exact identity");
  auto origin = SgSourceFile::e_parsed_source;
  if (generated)
    origin = SgSourceFile::e_generated_source;
  else if (command_line)
    origin = SgSourceFile::e_command_line_source;
  SgSourceFile *file =
      ownership.make<SgSourceFile>(args, project, owner, filename, origin);
  installSourcePositions(file, std::move(positions), record.kind);
  file->set_target_abi(target_abi);
  file->set_target_size_type(target_size_type);
  file->set_standard(standard);
  if (gnu_standard)
    file->set_gnu_standard();
  else
    file->unset_gnu_standard();
  file->get_frontendIncludeOwnershipPathList() = std::move(include_paths);
  file->get_frontendSystemIncludeOwnershipPathList() = std::move(system_paths);
  file->get_frontendExternalOwnershipPathList() = std::move(external_paths);
  file->set_unparse_output_filename(
      p.requiredString("unparse_output_filename"));
  file->set_skip_unparse(p.requiredBool("skip_unparse"));
  file->set_C_only(p.requiredBool("C_only"));
  file->set_Cxx_only(p.requiredBool("Cxx_only"));
  file->set_Fortran_only(p.requiredBool("Fortran_only"));
  file->set_CoArrayFortran_only(p.requiredBool("CoArrayFortran_only"));
  file->set_Cuda_only(p.requiredBool("Cuda_only"));
  file->set_OpenCL_only(p.requiredBool("OpenCL_only"));
  file->set_requires_C_preprocessor(p.requiredBool("requires_C_preprocessor"));
  file->set_processedToIncludeCppDirectivesAndComments(
      p.requiredBool("processedToIncludeCppDirectivesAndComments"));
  file->set_inputFormat(fileOutputFormatFromJson(p, "input_format"));
  file->set_outputFormat(fileOutputFormatFromJson(p, "output_format"));
  file->set_backendCompileFormat(
      fileOutputFormatFromJson(p, "backend_compile_format"));
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
  file->set_openmp(p.requiredBool("openmp"));
  file->set_openmp_parse_only(p.requiredBool("openmp_parse_only"));
  file->set_openmp_ast_only(p.requiredBool("openmp_ast_only"));
  file->set_openmp_analyzing(p.requiredBool("openmp_analyzing"));
  file->set_openmp_lowering(p.requiredBool("openmp_lowering"));
  file->set_openmp_processed(p.requiredBool("openmp_processed"));
  file->set_openacc(p.requiredBool("openacc"));
  file->set_skipfinalCompileStep(p.requiredBool("skipfinalCompileStep"));
  file->set_unparse_tokens(p.requiredBool("unparse_tokens"));
  file->set_isHeaderFile(p.requiredBool("is_header_file"));
  file->set_unparseHeaderFiles(p.requiredBool("unparse_header_files"));
  flags.initialize(file);
  types.publish();
  ownership.publish();
  return file;
}

SgFile::languageOption_enum fileLanguageFromJson(const JsonValue &properties,
                                                 const std::string &key) {
  return requiredEnum<SgFile::languageOption_enum>(
      properties, key, "SgSourceFile",
      {SgFile::e_default_language, SgFile::e_C_language, SgFile::e_Cxx_language,
       SgFile::e_Fortran_language});
}

SgFile::outputFormatOption_enum
fileOutputFormatFromJson(const JsonValue &properties, const std::string &key) {
  return requiredEnum<SgFile::outputFormatOption_enum>(
      properties, key, "SgSourceFile",
      {SgFile::e_unknown_output_format, SgFile::e_fixed_form_output_format,
       SgFile::e_free_form_output_format});
}

} // namespace AstJson
} // namespace Rose
