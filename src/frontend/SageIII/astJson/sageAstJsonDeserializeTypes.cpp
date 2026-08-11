#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

namespace {
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
  const JsonValue *serialized_type = nullptr;
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
  const JsonValue *serialized_declaration = nullptr;
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
thread_local std::unordered_map<SgClassType *, bool>
    classTypeAutonomousDeclarationIdentities;

void restoreTemplateTypeCanonicalSourceIdentity(SgTemplateType *type,
                                                const JsonValue &serialized) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON template type source identity has no target type");
  }
  const JsonValue &identity = serialized.at("canonical_source_identity");
  if (identity.kind == JsonValue::Kind::Null) {
    return;
  }
  if (identity.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON SgTemplateType canonical_source_identity is neither an "
        "object nor null");
  }
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
    throw std::runtime_error(
        "AST JSON SgTemplateType canonical_source_identity is out of range");
  }
  filenameForFileId(static_cast<int>(expansion_file_id),
                    "SgTemplateType expansion identity");
  filenameForFileId(static_cast<int>(spelling_file_id),
                    "SgTemplateType spelling identity");
  type->initialize_canonical_source_identity(
      {static_cast<int>(expansion_file_id),
       static_cast<unsigned int>(expansion_file_offset),
       static_cast<int>(spelling_file_id),
       static_cast<unsigned int>(spelling_file_offset)});
}

struct DeferredTypeOwnedVariableSymbol {
  SgVarRefExp *reference = nullptr;
  JsonValue serialized_symbol;
};
struct DeferredTypeOwnedFunctionSymbol {
  SgFunctionRefExp *reference = nullptr;
  JsonValue serialized_symbol;
  JsonValue serialized_source_visible_symbol;
  int source_visible_binding_kind =
      SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable;
};
thread_local bool typeOwnedSymbolDeserializationActive = false;
thread_local std::vector<DeferredTypeOwnedVariableSymbol>
    deferredTypeOwnedVariableSymbols;
thread_local std::vector<DeferredTypeOwnedFunctionSymbol>
    deferredTypeOwnedFunctionSymbols;

void restoreTypeOwnedFortranSourceVisibleBinding(
    SgFunctionRefExp *reference, const JsonValue &serializedSymbol,
    int bindingKind, const NodeMap &nodes) {
  ASSERT_not_null(reference);
  SgSymbol *serialized = exactBoundSymbolFromJson(serializedSymbol, nodes);
  SgFunctionSymbol *sourceVisible = isSgFunctionSymbol(serialized);
  if (serialized != nullptr && sourceVisible == nullptr) {
    throw std::runtime_error(
        "AST JSON type-owned SgFunctionRefExp Fortran source-visible "
        "binding is not a function symbol");
  }
  switch (bindingKind) {
  case SgFunctionRefExp::e_fortran_source_visible_binding_not_applicable:
    if (sourceVisible != nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgFunctionRefExp has a source-visible symbol "
          "without a Fortran binding kind");
    }
    break;
  case SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed:
  case SgFunctionRefExp::e_fortran_source_visible_binding_use_rename:
  case SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload:
  case SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow:
  case SgFunctionRefExp::e_fortran_source_visible_binding_semantic_publication:
    if (sourceVisible == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgFunctionRefExp has a Fortran binding kind "
          "without a source-visible symbol");
    }
    break;
  default:
    throw std::runtime_error(
        "AST JSON type-owned SgFunctionRefExp has an invalid Fortran "
        "source-visible binding kind");
  }
  reference->set_fortran_source_visible_symbol(sourceVisible);
  reference->set_fortran_source_visible_binding_kind(
      static_cast<SgFunctionRefExp::fortran_source_visible_binding_kind_enum>(
          bindingKind));
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

TypeOwnedSymbolDeserializationGuard::TypeOwnedSymbolDeserializationGuard() {
  if (typeOwnedSymbolDeserializationActive ||
      !deferredTypeOwnedVariableSymbols.empty() ||
      !deferredTypeOwnedFunctionSymbols.empty()) {
    throw std::runtime_error(
        "AST JSON type-owned symbol deserialization is already active");
  }
  typeOwnedSymbolDeserializationActive = true;
}

TypeOwnedSymbolDeserializationGuard::~TypeOwnedSymbolDeserializationGuard() {
  deferredTypeOwnedVariableSymbols.clear();
  deferredTypeOwnedFunctionSymbols.clear();
  typeOwnedSymbolDeserializationActive = false;
}

void TypeOwnedSymbolDeserializationGuard::resolve(const NodeMap &nodes) {
  if (!typeOwnedSymbolDeserializationActive || resolved_) {
    throw std::runtime_error(
        "AST JSON type-owned symbols are not awaiting resolution");
  }
  for (const DeferredTypeOwnedVariableSymbol &deferred :
       deferredTypeOwnedVariableSymbols) {
    SgVarRefExp *reference = deferred.reference;
    SgVariableSymbol *symbol =
        isSgVariableSymbol(symbolFromJson(deferred.serialized_symbol, nodes));
    if (reference == nullptr || symbol == nullptr ||
        symbol->get_symbol_basis() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed deferred type-owned variable symbol resolution");
    }
    SgVariableSymbol *provisional = reference->get_symbol();
    reference->set_symbol(symbol);
    validateAnonymousDataMemberReferenceQualification(reference);
    if (provisional != nullptr && provisional != symbol &&
        provisional->get_parent() == nullptr) {
      provisional->set_declaration(nullptr);
      SageInterface::deleteAST(
          provisional, SageInterface::DeleteAstMode::kSkipExternalReferences);
    }
  }
  deferredTypeOwnedVariableSymbols.clear();
  for (const DeferredTypeOwnedFunctionSymbol &deferred :
       deferredTypeOwnedFunctionSymbols) {
    SgFunctionRefExp *reference = deferred.reference;
    if (reference == nullptr) {
      throw std::runtime_error(
          "AST JSON failed deferred type-owned function symbol resolution");
    }
    SgFunctionSymbol *provisional = reference->get_symbol();
    SgFunctionSymbol *provisionalSourceVisible =
        reference->get_fortran_source_visible_symbol();
    restoreTypeOwnedFortranSourceVisibleBinding(
        reference, deferred.serialized_source_visible_symbol,
        deferred.source_visible_binding_kind, nodes);
    SgFunctionSymbol *sourceVisible =
        reference->get_fortran_source_visible_symbol();
    SgFunctionSymbol *symbol = functionReferenceSemanticSymbolFromJson(
        deferred.serialized_symbol, deferred.serialized_source_visible_symbol,
        sourceVisible, nodes);
    if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed deferred type-owned function symbol resolution");
    }
    reference->set_symbol(symbol);
    if (provisional != nullptr && provisional != symbol &&
        provisional != sourceVisible && provisional->get_parent() == nullptr) {
      provisional->set_declaration(nullptr);
      SageInterface::deleteAST(
          provisional, SageInterface::DeleteAstMode::kSkipExternalReferences);
    }
    if (provisionalSourceVisible != nullptr &&
        provisionalSourceVisible != provisional &&
        provisionalSourceVisible != symbol &&
        provisionalSourceVisible != sourceVisible &&
        provisionalSourceVisible->get_parent() == nullptr) {
      provisionalSourceVisible->set_declaration(nullptr);
      SageInterface::deleteAST(
          provisionalSourceVisible,
          SageInterface::DeleteAstMode::kSkipExternalReferences);
    }
  }
  deferredTypeOwnedFunctionSymbols.clear();
  resolved_ = true;
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

ArrayTypeDeserializationIdentityGuard::ArrayTypeDeserializationIdentityGuard() {
  if (arrayTypeDeserializationIdentityActive ||
      !arrayTypeDeserializationIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON semantic array deserialization graph is already active");
  }
  arrayTypeDeserializationIdentityActive = true;
}

ArrayTypeDeserializationIdentityGuard::
    ~ArrayTypeDeserializationIdentityGuard() {
  arrayTypeDeserializationIdentities.clear();
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
      !externalClassModuleIdentities.empty() ||
      !classTypeAutonomousDeclarationIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON external-class deserialization graph is already active");
  }
  externalClassDeserializationIdentityActive = true;
}

ExternalClassDeserializationIdentityGuard::
    ~ExternalClassDeserializationIdentityGuard() {
  externalClassDeserializationIdentities.clear();
  externalClassModuleIdentities.clear();
  classTypeAutonomousDeclarationIdentities.clear();
  externalClassDeserializationIdentityActive = false;
}

Sg_File_Info *buildFileInfo(const JsonValue &json, SgNode *parent) {
  if (json.kind != JsonValue::Kind::Object || !json.requiredBool("present")) {
    return nullptr;
  }
  auto special_file_id = [](const std::string &name, int &file_id) -> bool {
    if (name == "COPY") {
      file_id = Sg_File_Info::COPY_FILE_ID;
      return true;
    }
    if (name == "NULL_FILE") {
      file_id = Sg_File_Info::NULL_FILE_ID;
      return true;
    }
    if (name == "transformation") {
      file_id = Sg_File_Info::TRANSFORMATION_FILE_ID;
      return true;
    }
    if (name == "compilerGenerated") {
      file_id = Sg_File_Info::COMPILER_GENERATED_FILE_ID;
      return true;
    }
    return false;
  };
  const std::string raw_filename = json.requiredString("raw_filename");
  const int raw_line = static_cast<int>(json.requiredInt("raw_line"));
  const int raw_column = static_cast<int>(json.requiredInt("raw_column"));
  int raw_constructor_file_id = 0;
  std::unique_ptr<Sg_File_Info> info(
      special_file_id(raw_filename, raw_constructor_file_id)
          ? new Sg_File_Info(raw_constructor_file_id, raw_line, raw_column)
          : new Sg_File_Info(raw_filename, raw_line, raw_column));
  const JsonValue *physical_file_id_value = json.find("physical_file_id");
  const int physical_file_id =
      physical_file_id_value != nullptr
          ? static_cast<int>(physical_file_id_value->asInt())
          : 0;
  const int physical_internal_file_id =
      static_cast<int>(json.requiredInt("physical_internal_file_id"));
  const std::string physical_raw_filename =
      json.requiredString("physical_raw_filename");
  const std::string physical_filename =
      json.requiredString("physical_filename");
  if (physical_file_id >= 0) {
    const std::string registered_name = !physical_raw_filename.empty()
                                            ? physical_raw_filename
                                            : physical_filename;
    requireFileIdMapping(physical_file_id, registered_name,
                         "file info physical_file_id");
  }

  if (physical_internal_file_id >= 0) {
    const std::string registered_name = filenameForFileId(
        physical_internal_file_id, "file info physical_internal_file_id");
    requireFileIdMapping(physical_internal_file_id, registered_name,
                         "file info physical_internal_file_id");
    info->set_physical_file_id(physical_internal_file_id);
  } else if (physical_internal_file_id < 0) {
    info->set_physical_file_id(physical_internal_file_id);
  } else if (!physical_raw_filename.empty()) {
    info->set_physical_filename(physical_raw_filename);
  } else if (!physical_filename.empty()) {
    info->set_physical_filename(physical_filename);
  }
  info->set_physical_line(json.requiredInt("physical_line"));
  info->set_source_sequence_number(
      static_cast<unsigned int>(json.requiredInt("source_sequence")));
  if (json.requiredBool("compiler_generated")) {
    info->setCompilerGenerated();
  } else {
    info->unsetCompilerGenerated();
  }
  if (json.requiredBool("transformation")) {
    info->setTransformation();
  } else {
    info->unsetTransformation();
  }
  if (json.requiredBool("frontend_specific")) {
    info->setFrontendSpecific();
  } else {
    info->unsetFrontendSpecific();
  }
  if (json.requiredBool("shared")) {
    info->setShared();
  } else {
    info->unsetShared();
  }
  if (json.requiredBool("source_position_unavailable_in_frontend")) {
    info->setSourcePositionUnavailableInFrontend();
  } else {
    info->unsetSourcePositionUnavailableInFrontend();
  }
  if (json.requiredBool("comment_or_directive")) {
    info->setCommentOrDirective();
  } else {
    info->unsetCommentOrDirective();
  }
  if (json.requiredBool("token")) {
    info->setToken();
  } else {
    info->unsetToken();
  }
  if (json.requiredBool("default_argument")) {
    info->setDefaultArgument();
  } else {
    info->unsetDefaultArgument();
  }
  if (json.requiredBool("implicit_cast")) {
    info->setImplicitCast();
  } else {
    info->unsetImplicitCast();
  }
  const JsonValue *file_id_value = json.find("file_id");
  if (file_id_value != nullptr) {
    const int file_id = static_cast<int>(file_id_value->asInt());
    int raw_special_file_id = 0;
    const bool raw_is_special =
        special_file_id(raw_filename, raw_special_file_id);
    const bool special_reported_by_flags =
        (file_id == Sg_File_Info::TRANSFORMATION_FILE_ID &&
         info->isTransformation()) ||
        (file_id == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
         info->isCompilerGenerated() && !info->isFrontendSpecific());
    if (file_id < 0 && ((raw_is_special && raw_special_file_id == file_id) ||
                        !special_reported_by_flags)) {
      info->set_file_id(file_id);
    }
  }
  if (json.requiredBool("output_in_code_generation")) {
    info->setOutputInCodeGeneration();
  } else {
    info->unsetOutputInCodeGeneration();
  }
  info->set_parent(parent);
  return info.release();
}

SgClassType *ensureClassTypeForDeclaration(SgClassDeclaration *decl) {
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
  if (decl->get_firstNondefiningDeclaration() != first_nondefining ||
      (decl->get_definingDeclaration() != nullptr &&
       decl->get_definingDeclaration() != defining) ||
      (decl != first_nondefining && decl != defining &&
       decl->get_definingDeclaration() != defining)) {
    throw std::runtime_error(
        "AST JSON class declaration is not in its canonical declaration "
        "family");
  }
  if (decl->get_scope() == nullptr ||
      first_nondefining->get_scope() != decl->get_scope() ||
      (defining != nullptr && defining->get_scope() != decl->get_scope())) {
    std::ostringstream message;
    message << "AST JSON class declaration family name="
            << decl->get_name().getString() << " declaration=" << decl
            << " scope=" << decl->get_scope() << " first=" << first_nondefining
            << " first-scope=" << first_nondefining->get_scope()
            << " defining=" << defining << " defining-scope="
            << (defining != nullptr ? defining->get_scope() : nullptr)
            << " has no exact common semantic scope";
    throw std::runtime_error(message.str());
  }

  SgClassType *existing = decl->get_type();
  SgClassType *canonical_existing = first_nondefining->get_type();
  if (existing != nullptr && canonical_existing != nullptr &&
      existing != canonical_existing) {
    throw std::runtime_error(
        "AST JSON class declaration family has conflicting class-type "
        "identity");
  }

  // The canonical factory owns the project-wide interning contract.  A named
  // class in a global scope can legitimately share one type with the same
  // declaration identity in another source file of the active project.  In
  // that case the type's single declaration edge points at the first
  // project-published owner, while every declaration family still publishes
  // that exact type.  Requiring the edge to point at this reconstructed family
  // rejects the factory's already-validated project identity.
  SgClassType *type = SgClassType::createType(first_nondefining);
  if ((existing != nullptr && existing != type) ||
      (canonical_existing != nullptr && canonical_existing != type) ||
      type == nullptr || type->get_declaration() == nullptr ||
      first_nondefining->get_type() != type ||
      (defining != nullptr && defining->get_type() != type)) {
    std::ostringstream message;
    message << "AST JSON class declaration name="
            << first_nondefining->get_name().getString()
            << " did not publish one canonical class type declaration="
            << first_nondefining << " defining=" << defining << " type=" << type
            << " type-declaration="
            << (type != nullptr ? type->get_declaration() : nullptr)
            << " declaration-type=" << first_nondefining->get_type()
            << " defining-type="
            << (defining != nullptr ? defining->get_type() : nullptr);
    throw std::runtime_error(message.str());
  }
  if (decl->get_type() == nullptr) {
    decl->set_type(type);
  } else if (decl->get_type() != type) {
    throw std::runtime_error(
        "AST JSON class redeclaration does not share its canonical class "
        "type");
  }
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

void restorePointerMemberSourceQualification(SgPointerMemberType *type,
                                             const JsonValue &json) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON pointer-member source qualifier has no type");
  }
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
  if (type->get_source_base_type_qualification_present()) {
    if (!present ||
        type->get_source_base_type_global_qualification() != global ||
        type->get_source_base_type_qualification_tokens() != tokens) {
      throw std::runtime_error(
          "AST JSON pointer-member type has conflicting source qualifiers");
    }
  } else {
    type->set_source_base_type_global_qualification(global);
    type->get_source_base_type_qualification_tokens() = tokens;
    type->set_source_base_type_qualification_present(present);
  }
  if (SgPointerMemberType::isCanonicalSemanticType(type) &&
      source_class_type_is_unqualified_injected_name) {
    throw std::runtime_error(
        "AST JSON canonical semantic pointer-member type has source-injected "
        "class spelling");
  }
  type->set_source_class_type_is_unqualified_injected_name(
      source_class_type_is_unqualified_injected_name);
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

SgPointerMemberType *restoredPointerMemberTypeIdentity(const JsonValue &json) {
  const uint64_t serialized_identity =
      requiredPointerMemberTypeSerializationIdentity(json);
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
          json.requiredBool("semantic_canonical") ||
      member_pointer->get_fortran_source_syntax() !=
          json.requiredBool("fortran_source_syntax")) {
    throw std::runtime_error(
        "AST JSON pointer-member identity no longer names its exact restored "
        "type graph");
  }
  restorePointerMemberSourceQualification(member_pointer, json);
  return member_pointer;
}

SgPointerMemberType *restorePointerMemberTypeIdentity(SgType *base,
                                                      SgType *class_type,
                                                      const JsonValue &json,
                                                      bool publish_identity) {
  if (base == nullptr || class_type == nullptr) {
    throw std::runtime_error(
        "AST JSON pointer-member type has no exact base/class identity");
  }

  const bool semantic_canonical = json.requiredBool("semantic_canonical");
  uint64_t serialized_identity = 0;
  if (publish_identity) {
    serialized_identity = requiredPointerMemberTypeSerializationIdentity(json);
    if (pointerMemberTypeDeserializationIdentities.count(serialized_identity) !=
        0) {
      throw std::runtime_error(
          "AST JSON pointer-member identity was rebuilt before its existing "
          "type graph was reused");
    }
  }

  SgPointerMemberType *member_pointer =
      semantic_canonical ? SgPointerMemberType::createType(base, class_type)
                         : new SgPointerMemberType(base, class_type);
  if (member_pointer == nullptr || SgPointerMemberType::isCanonicalSemanticType(
                                       member_pointer) != semantic_canonical) {
    throw std::runtime_error(
        "AST JSON pointer-member semantic/source identity was not restored "
        "exactly");
  }
  const bool fortran_source_syntax = json.requiredBool("fortran_source_syntax");
  if (semantic_canonical && fortran_source_syntax) {
    throw std::runtime_error(
        "AST JSON canonical semantic pointer-member type has source syntax");
  }
  member_pointer->set_fortran_source_syntax(fortran_source_syntax);
  restorePointerMemberSourceQualification(member_pointer, json);
  if (SgPointerMemberType::isCanonicalSemanticType(member_pointer) !=
      semantic_canonical) {
    throw std::runtime_error(
        "AST JSON pointer-member source qualifiers changed its serialized "
        "semantic/source identity");
  }
  if (publish_identity) {
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

bool requiresDelayedRebuild(const NodeRecord &record) {
  return record.kind == "SgTemplateInstantiationDefn" ||
         record.kind == "SgConstructorInitializer" ||
         record.kind == "SgDesignatedInitializer" ||
         record.kind == "SgAssignInitializer" ||
         record.kind == "SgBracedInitializer" ||
         record.kind == "SgPseudoDestructorRefExp" ||
         record.kind == "SgOmpDeclareSimdStatement" ||
         record.kind == "SgOmpDeclareVariantStatement";
}

SgScopeStatement *nearestScope(SgNode *node);

SgDeclType *buildDeclType(SgExpression *base_expression, SgType *base_type,
                          const JsonValue &type) {
  if (base_expression == nullptr) {
    throw std::runtime_error(
        "AST JSON SgDeclType requires a structured base_expression node");
  }
  if (base_type == nullptr || isSgTypeUnknown(base_type) != nullptr) {
    throw std::runtime_error(
        "AST JSON SgDeclType requires one exact semantic base type");
  }
  SgDeclType *decl_type = new SgDeclType(base_expression, base_type);
  base_expression->set_parent(decl_type);
  decl_type->set_is_gnu_decltype(type.requiredBool("is_gnu_decltype"));
  return isSgDeclType(decl_type);
}

SgType *primitiveTypeFromKind(const std::string &kind) {
  if (kind == "SgTypeVoid")
    return SageBuilder::buildVoidType();
  if (kind == "SgTypeBool")
    return SageBuilder::buildBoolType();
  if (kind == "SgTypeChar")
    return SageBuilder::buildCharType();
  if (kind == "SgTypeSignedChar")
    return SageBuilder::buildSignedCharType();
  if (kind == "SgTypeUnsignedChar")
    return SageBuilder::buildUnsignedCharType();
  if (kind == "SgTypeShort")
    return SageBuilder::buildShortType();
  if (kind == "SgTypeSignedShort")
    return SageBuilder::buildSignedShortType();
  if (kind == "SgTypeUnsignedShort")
    return SageBuilder::buildUnsignedShortType();
  if (kind == "SgTypeInt")
    return SageBuilder::buildIntType();
  if (kind == "SgTypeSignedInt")
    return SageBuilder::buildSignedIntType();
  if (kind == "SgTypeUnsignedInt")
    return SageBuilder::buildUnsignedIntType();
  if (kind == "SgTypeLong")
    return SageBuilder::buildLongType();
  if (kind == "SgTypeSignedLong")
    return SageBuilder::buildSignedLongType();
  if (kind == "SgTypeUnsignedLong")
    return SageBuilder::buildUnsignedLongType();
  if (kind == "SgTypeLongLong")
    return SageBuilder::buildLongLongType();
  if (kind == "SgTypeSignedLongLong")
    return SageBuilder::buildSignedLongLongType();
  if (kind == "SgTypeUnsignedLongLong")
    return SageBuilder::buildUnsignedLongLongType();
  if (kind == "SgTypeFloat")
    return SageBuilder::buildFloatType();
  if (kind == "SgTypeDouble")
    return SageBuilder::buildDoubleType();
  if (kind == "SgTypeLongDouble")
    return SageBuilder::buildLongDoubleType();
  if (kind == "SgTypeFloat16")
    return SageBuilder::buildFloat16Type();
  if (kind == "SgTypeFloat80")
    return SageBuilder::buildFloat80Type();
  if (kind == "SgTypeFloat128")
    return SageBuilder::buildFloat128Type();
  if (kind == "SgTypeFp16")
    return SageBuilder::buildFp16Type();
  if (kind == "SgTypeBFloat16")
    return SageBuilder::buildBFloat16Type();
  if (kind == "SgTypeFloat32")
    return SageBuilder::buildFloat32Type();
  if (kind == "SgTypeFloat64")
    return SageBuilder::buildFloat64Type();
  if (kind == "SgTypeFloat32x")
    return SageBuilder::buildFloat32xType();
  if (kind == "SgTypeFloat64x")
    return SageBuilder::buildFloat64xType();
  if (kind == "SgTypeWchar")
    return SageBuilder::buildWcharType();
  if (kind == "SgTypeChar8")
    return SageBuilder::buildChar8Type();
  if (kind == "SgTypeChar16")
    return SageBuilder::buildChar16Type();
  if (kind == "SgTypeChar32")
    return SageBuilder::buildChar32Type();
  return nullptr;
}

SgTypeTargetBuiltin *targetBuiltinTypeFromJson(const JsonValue &json) {
  const int64_t family = json.requiredInt("target_family");
  if (family < SgTypeTargetBuiltin::e_target_builtin_aarch64 ||
      family > SgTypeTargetBuiltin::e_target_builtin_hlsl) {
    throw std::runtime_error(
        "AST JSON target builtin type has invalid target family");
  }
  const std::string spelling = json.requiredString("spelling");
  if (spelling.empty()) {
    throw std::runtime_error(
        "AST JSON target builtin type has empty exact spelling");
  }
  return SageBuilder::buildTargetBuiltinType(
      SgName(spelling),
      static_cast<SgTypeTargetBuiltin::target_family_enum>(family));
}

SgAutoType *autoTypeFromJson(const JsonValue &json) {
  const bool is_constrained = json.requiredBool("is_constrained");
  const std::string source_constraint_spelling =
      json.requiredString("source_constraint_spelling");
  if (is_constrained != !source_constraint_spelling.empty()) {
    throw std::runtime_error(
        "AST JSON SgAutoType constraint state has no exact source spelling");
  }

  SgAutoType *auto_type = SageBuilder::buildAutoType();
  auto_type->set_is_constrained(is_constrained);
  auto_type->set_source_constraint_spelling(source_constraint_spelling);
  return auto_type;
}

SgType *earlyTypeFromJson(const JsonValue &type) {
  if (type.kind != JsonValue::Kind::Object || !type.requiredBool("present")) {
    return SageBuilder::buildUnknownType();
  }

  const std::string kind = type.requiredString("kind");
  if (kind == "SgTypeDefault") {
    return SgTypeDefault::createType();
  }
  if (kind == "SgTypeTargetBuiltin") {
    return targetBuiltinTypeFromJson(type);
  }
  if (kind == "SgTypeFortranAssumed") {
    if (type.requiredBool("fortran_source_syntax")) {
      SgTypeFortranAssumed *restored = new SgTypeFortranAssumed();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeFortranAssumed::createType();
  }
  if (kind == "SgTypeFortranUnlimitedPolymorphic") {
    if (type.requiredBool("fortran_source_syntax")) {
      SgTypeFortranUnlimitedPolymorphic *restored =
          new SgTypeFortranUnlimitedPolymorphic();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeFortranUnlimitedPolymorphic::createType();
  }
  if (kind == "SgTypeCrayPointer") {
    if (type.requiredBool("fortran_source_syntax")) {
      SgTypeCrayPointer *restored = new SgTypeCrayPointer();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeCrayPointer::createType();
  }
  if (kind == "SgTypeUnknown") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgTypeEllipse") {
    return SgTypeEllipse::createType();
  }
  if (kind == "SgTypeNullptr") {
    return SageBuilder::buildNullptrType();
  }
  if (kind == "SgTypeLabel") {
    return SgTypeLabel::createType(SgName(type.requiredString("name")));
  }
  if (kind == "SgTypeFloat128") {
    return SageBuilder::buildFloat128Type();
  }
  if (kind == "SgTypeSigned128bitInteger") {
    return SageBuilder::buildSigned128bitIntegerType();
  }
  if (kind == "SgTypeUnsigned128bitInteger") {
    return SageBuilder::buildUnsigned128bitIntegerType();
  }
  if (kind == "SgAutoType") {
    return autoTypeFromJson(type);
  }
  if (SgType *primitive = primitiveTypeFromKind(kind)) {
    return primitive;
  }
  if (kind == "SgTypeString") {
    return new SgTypeString(nullptr);
  }
  if (kind == "SgTypeComplex") {
    return SgTypeComplex::createType(earlyTypeFromJson(type.at("base")));
  }
  if (kind == "SgPointerMemberType") {
    return restorePointerMemberTypeIdentity(
        earlyTypeFromJson(type.at("base")),
        earlyTypeFromJson(type.at("class_type")), type, false);
  }
  if (kind == "SgPointerType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    return buildCachedJsonPointerType(base);
  }
  if (kind == "SgReferenceType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgReferenceType *reference = new SgReferenceType(base);
    installReferenceCache(base, reference);
    return reference;
  }
  if (kind == "SgRvalueReferenceType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgRvalueReferenceType *reference = new SgRvalueReferenceType(base);
    installRvalueReferenceCache(base, reference);
    return reference;
  }
  if (kind == "SgArrayType") {
    SgArrayType *array =
        new SgArrayType(earlyTypeFromJson(type.at("base")), nullptr);
    array->set_is_variable_length_array(
        type.requiredBool("is_variable_length_array"));
    return array;
  }
  if (kind == "SgModifierType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgModifierType *modifier = new SgModifierType(base);
    SgTypeModifier &type_modifier = modifier->get_typeModifier();
    SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
    if (type.requiredBool("modifier_const")) {
      cv.setConst();
    }
    if (type.requiredBool("modifier_volatile")) {
      cv.setVolatile();
    }
    if (type.requiredBool("modifier_restrict")) {
      type_modifier.setRestrict();
    }
    return modifier;
  }
  if (kind == "SgTypedefType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgClassType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgEnumType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgNonrealType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgDeclType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgTypeOfType") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgTemplateType") {
    SgTemplateType *template_type =
        new SgTemplateType(SgName(type.requiredString("name")));
    template_type->set_template_parameter_position(
        static_cast<int>(type.requiredInt("template_parameter_position")));
    template_type->set_template_parameter_depth(
        static_cast<int>(type.requiredInt("template_parameter_depth")));
    restoreTemplateTypeCanonicalSourceIdentity(template_type, type);
    template_type->set_packed(type.requiredBool("packed"));
    if (const JsonValue *class_json = type.find("class_type")) {
      template_type->set_class_type(class_json->requiredBool("present")
                                        ? earlyTypeFromJson(*class_json)
                                        : nullptr);
    } else {
      template_type->set_class_type(nullptr);
    }
    if (const JsonValue *parent_class_json = type.find("parent_class_type")) {
      template_type->set_parent_class_type(
          parent_class_json->requiredBool("present")
              ? earlyTypeFromJson(*parent_class_json)
              : nullptr);
    } else {
      template_type->set_parent_class_type(nullptr);
    }
    return template_type;
  }
  if (kind == "SgMemberFunctionType") {
    SgType *return_type = earlyTypeFromJson(type.at("return_type"));
    SgType *class_type = earlyTypeFromJson(type.at("class_type"));
    if (return_type == nullptr || class_type == nullptr) {
      throw std::runtime_error(
          "AST JSON member function type has a null return or class type");
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    const JsonValue &argument_json = type.at("arguments");
    if (argument_json.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON member function type arguments field is not an array");
    }
    for (const JsonValue &argument : argument_json.array) {
      SgType *argument_type = earlyTypeFromJson(argument);
      if (argument_type == nullptr) {
        throw std::runtime_error(
            "AST JSON member function type has a null argument type");
      }
      arguments->append_argument(argument_type);
    }
    SgMemberFunctionType *member_type = new SgMemberFunctionType(
        return_type, type.requiredBool("has_ellipses"), class_type,
        static_cast<unsigned int>(type.requiredInt("mfunc_specifier")));
    member_type->set_argument_list(arguments);
    arguments->set_parent(member_type);
    return member_type;
  }
  if (kind == "SgFunctionType") {
    SgType *return_type = earlyTypeFromJson(type.at("return_type"));
    if (return_type == nullptr) {
      throw std::runtime_error("AST JSON function type has a null return type");
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    const JsonValue &argument_json = type.at("arguments");
    if (argument_json.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON function type arguments field is not an array");
    }
    for (const JsonValue &argument : argument_json.array) {
      SgType *argument_type = earlyTypeFromJson(argument);
      if (argument_type == nullptr) {
        throw std::runtime_error(
            "AST JSON function type has a null argument type");
      }
      arguments->append_argument(argument_type);
    }
    SgFunctionType *function_type =
        new SgFunctionType(return_type, type.requiredBool("has_ellipses"));
    function_type->set_argument_list(arguments);
    arguments->set_parent(function_type);
    return function_type;
  }
  throw std::runtime_error("AST JSON deserializer does not support Sage type " +
                           kind + " during early construction");
}

SgType *earlyTypeFromProperties(const JsonValue &properties) {
  const JsonValue *type = properties.find("type");
  if (type == nullptr) {
    return SageBuilder::buildUnknownType();
  }
  return earlyTypeFromJson(*type);
}

bool isJsonBinaryOpKind(const std::string &kind) {
  static const std::unordered_set<std::string> kinds = {"SgAssignOp",
                                                        "SgAddOp",
                                                        "SgSubtractOp",
                                                        "SgMultiplyOp",
                                                        "SgDivideOp",
                                                        "SgModOp",
                                                        "SgLessThanOp",
                                                        "SgLessOrEqualOp",
                                                        "SgGreaterThanOp",
                                                        "SgGreaterOrEqualOp",
                                                        "SgEqualityOp",
                                                        "SgNotEqualOp",
                                                        "SgLshiftOp",
                                                        "SgRshiftOp",
                                                        "SgAndOp",
                                                        "SgOrOp",
                                                        "SgBitAndOp",
                                                        "SgBitOrOp",
                                                        "SgBitXorOp",
                                                        "SgCommaOpExp",
                                                        "SgDotExp",
                                                        "SgArrowExp",
                                                        "SgDotStarOp",
                                                        "SgArrowStarOp",
                                                        "SgPntrArrRefExp",
                                                        "SgPlusAssignOp",
                                                        "SgMinusAssignOp",
                                                        "SgMultAssignOp",
                                                        "SgDivAssignOp",
                                                        "SgModAssignOp",
                                                        "SgAndAssignOp",
                                                        "SgIorAssignOp",
                                                        "SgXorAssignOp",
                                                        "SgLshiftAssignOp",
                                                        "SgRshiftAssignOp",
                                                        "SgConcatenationOp",
                                                        "SgExponentiationOp",
                                                        "SgPointerAssignOp"};
  return kinds.find(kind) != kinds.end();
}

SgBinaryOp *buildBinaryOpForKind(const std::string &kind, SgType *expr_type) {
  SgExpression *null_expr = nullptr;
  if (kind == "SgAssignOp")
    return new SgAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgAddOp")
    return new SgAddOp(null_expr, null_expr, expr_type);
  if (kind == "SgSubtractOp")
    return new SgSubtractOp(null_expr, null_expr, expr_type);
  if (kind == "SgMultiplyOp")
    return new SgMultiplyOp(null_expr, null_expr, expr_type);
  if (kind == "SgDivideOp")
    return new SgDivideOp(null_expr, null_expr, expr_type);
  if (kind == "SgModOp")
    return new SgModOp(null_expr, null_expr, expr_type);
  if (kind == "SgLessThanOp")
    return new SgLessThanOp(null_expr, null_expr, expr_type);
  if (kind == "SgLessOrEqualOp")
    return new SgLessOrEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgGreaterThanOp")
    return new SgGreaterThanOp(null_expr, null_expr, expr_type);
  if (kind == "SgGreaterOrEqualOp")
    return new SgGreaterOrEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgEqualityOp")
    return new SgEqualityOp(null_expr, null_expr, expr_type);
  if (kind == "SgNotEqualOp")
    return new SgNotEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgLshiftOp")
    return new SgLshiftOp(null_expr, null_expr, expr_type);
  if (kind == "SgRshiftOp")
    return new SgRshiftOp(null_expr, null_expr, expr_type);
  if (kind == "SgAndOp")
    return new SgAndOp(null_expr, null_expr, expr_type);
  if (kind == "SgOrOp")
    return new SgOrOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitAndOp")
    return new SgBitAndOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitOrOp")
    return new SgBitOrOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitXorOp")
    return new SgBitXorOp(null_expr, null_expr, expr_type);
  if (kind == "SgCommaOpExp")
    return new SgCommaOpExp(null_expr, null_expr, expr_type);
  if (kind == "SgDotExp")
    return new SgDotExp(null_expr, null_expr, expr_type);
  if (kind == "SgArrowExp")
    return new SgArrowExp(null_expr, null_expr, expr_type);
  if (kind == "SgDotStarOp")
    return new SgDotStarOp(null_expr, null_expr, expr_type);
  if (kind == "SgArrowStarOp")
    return new SgArrowStarOp(null_expr, null_expr, expr_type);
  if (kind == "SgPntrArrRefExp")
    return new SgPntrArrRefExp(null_expr, null_expr, expr_type);
  if (kind == "SgConcatenationOp")
    return new SgConcatenationOp(null_expr, null_expr, expr_type);
  if (kind == "SgExponentiationOp")
    return new SgExponentiationOp(null_expr, null_expr, expr_type);
  if (kind == "SgPointerAssignOp")
    return new SgPointerAssignOp(static_cast<Sg_File_Info *>(nullptr),
                                 null_expr, null_expr, expr_type);
  if (kind == "SgPlusAssignOp")
    return new SgPlusAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgMinusAssignOp")
    return new SgMinusAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgMultAssignOp")
    return new SgMultAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgDivAssignOp")
    return new SgDivAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgModAssignOp")
    return new SgModAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgAndAssignOp")
    return new SgAndAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgIorAssignOp")
    return new SgIorAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgXorAssignOp")
    return new SgXorAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgLshiftAssignOp")
    return new SgLshiftAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgRshiftAssignOp")
    return new SgRshiftAssignOp(null_expr, null_expr, expr_type);
  return nullptr;
}

bool isJsonUnaryOpKind(const std::string &kind) {
  static const std::unordered_set<std::string> kinds = {
      "SgAddressOfOp",     "SgPointerDerefExp", "SgNotOp",
      "SgBitComplementOp", "SgPlusPlusOp",      "SgMinusMinusOp",
      "SgMinusOp",         "SgUnaryAddOp",      "SgThrowOp"};
  return kinds.find(kind) != kinds.end();
}

SgUnaryOp *buildUnaryOpForKind(const std::string &kind, SgType *expr_type,
                               const JsonValue &properties) {
  SgExpression *null_expr = nullptr;
  SgUnaryOp *result = nullptr;
  if (kind == "SgAddressOfOp") {
    result = new SgAddressOfOp(null_expr, expr_type);
  } else if (kind == "SgPointerDerefExp") {
    result = new SgPointerDerefExp(null_expr, expr_type);
  } else if (kind == "SgNotOp") {
    result = new SgNotOp(null_expr, expr_type);
  } else if (kind == "SgBitComplementOp") {
    result = new SgBitComplementOp(null_expr, expr_type);
  } else if (kind == "SgPlusPlusOp") {
    result = new SgPlusPlusOp(null_expr, expr_type);
  } else if (kind == "SgMinusMinusOp") {
    result = new SgMinusMinusOp(null_expr, expr_type);
  } else if (kind == "SgMinusOp") {
    result = new SgMinusOp(null_expr, expr_type);
  } else if (kind == "SgUnaryAddOp") {
    result = new SgUnaryAddOp(null_expr, expr_type);
  } else if (kind == "SgThrowOp") {
    result =
        new SgThrowOp(null_expr, expr_type,
                      requiredEnum<SgThrowOp::e_throw_kind>(
                          properties, "throw_kind", "SgThrowOp",
                          {SgThrowOp::throw_expression, SgThrowOp::rethrow}));
  }
  if (result != nullptr) {
    result->set_mode(requiredEnum<SgUnaryOp::Sgop_mode>(
        properties, "mode", kind, {SgUnaryOp::prefix, SgUnaryOp::postfix}));
  }
  return result;
}

void setOwnedExpressionSourcePosition(SgExpression *expr,
                                      const JsonValue &location) {
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON type-owned expression location is not an object");
  }
  SgLocatedNode *located = isSgLocatedNode(expr);
  if (located == nullptr) {
    throw std::runtime_error("AST JSON type-owned expression is not located");
  }
  const JsonValue *start = location.find("start");
  const JsonValue *end = location.find("end");
  const JsonValue *operator_position = location.find("operator");
  if (start == nullptr || end == nullptr || operator_position == nullptr) {
    throw std::runtime_error(
        "AST JSON type-owned expression is missing source location");
  }
  std::unique_ptr<Sg_File_Info> start_info(buildFileInfo(*start, expr));
  std::unique_ptr<Sg_File_Info> end_info(buildFileInfo(*end, expr));
  std::unique_ptr<Sg_File_Info> operator_info(
      buildFileInfo(*operator_position, expr));
  located->set_startOfConstruct(start_info.release());
  located->set_endOfConstruct(end_info.release());
  expr->set_operatorPosition(operator_info.release());
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
SgExprListExp *exprListExpFromTypeJson(const JsonValue &json,
                                       const NodeMap &nodes);

bool restoreExactStoredExpressionType(SgExpression *expression,
                                      SgType *restored_type) {
  const bool supported = isSgBinaryOp(expression) != nullptr ||
                         (isSgUnaryOp(expression) != nullptr &&
                          isSgCastExp(expression) == nullptr) ||
                         isSgConditionalExp(expression) != nullptr ||
                         isSgFoldExpression(expression) != nullptr ||
                         isSgAwaitExpression(expression) != nullptr ||
                         isSgPackExpansionExpr(expression) != nullptr ||
                         isSgStatementExpression(expression) != nullptr ||
                         isSgThisExp(expression) != nullptr ||
                         isSgSizeOfOp(expression) != nullptr ||
                         isSgAlignOfOp(expression) != nullptr ||
                         isSgNoexceptOp(expression) != nullptr;
  if (!supported) {
    return false;
  }
  if (restored_type == nullptr || isSgTypeUnknown(restored_type) != nullptr ||
      isSgTypeDefault(restored_type) != nullptr) {
    throw std::runtime_error("AST JSON " + expression->class_name() +
                             " has no exact semantic result type");
  }
  if (isSgThisExp(expression) != nullptr &&
      isSgPointerType(restored_type) == nullptr) {
    throw std::runtime_error(
        "AST JSON SgThisExp result type is not an exact pointer type");
  }
  if (isSgNoexceptOp(expression) != nullptr &&
      isSgTypeBool(restored_type) == nullptr) {
    throw std::runtime_error(
        "AST JSON SgNoexceptOp result type is not exact bool");
  }
  if (isSgSizeOfOp(expression) != nullptr ||
      isSgAlignOfOp(expression) != nullptr) {
    SgType *unqualified = restored_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                                   SgType::STRIP_MODIFIER_TYPE);
    const bool unsigned_integer =
        isSgTypeUnsignedChar(unqualified) != nullptr ||
        isSgTypeUnsignedShort(unqualified) != nullptr ||
        isSgTypeUnsignedInt(unqualified) != nullptr ||
        isSgTypeUnsignedLong(unqualified) != nullptr ||
        isSgTypeUnsignedLongLong(unqualified) != nullptr;
    if (!unsigned_integer) {
      throw std::runtime_error("AST JSON " + expression->class_name() +
                               " result type is not the exact unsigned "
                               "target size_t type");
    }
  }
  if (SgBinaryOp *binary = isSgBinaryOp(expression)) {
    binary->set_expression_type(restored_type);
  } else if (SgUnaryOp *unary = isSgUnaryOp(expression)) {
    unary->set_expression_type(restored_type);
  } else if (SgConditionalExp *conditional = isSgConditionalExp(expression)) {
    conditional->set_expression_type(restored_type);
    conditional->validate();
  } else if (SgAwaitExpression *await_expression =
                 isSgAwaitExpression(expression)) {
    await_expression->set_expression_type(restored_type);
  } else if (SgFoldExpression *fold = isSgFoldExpression(expression)) {
    fold->set_expression_type(restored_type);
  } else if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(expression)) {
    pack->set_expression_type(restored_type);
  } else if (SgStatementExpression *statement_expression =
                 isSgStatementExpression(expression)) {
    statement_expression->set_expression_type(restored_type);
  } else if (SgThisExp *this_expression = isSgThisExp(expression)) {
    this_expression->set_expression_type(restored_type);
  } else if (SgSizeOfOp *size_of = isSgSizeOfOp(expression)) {
    size_of->set_expression_type(restored_type);
  } else if (SgAlignOfOp *align_of = isSgAlignOfOp(expression)) {
    align_of->set_expression_type(restored_type);
  }
  return true;
}

void restoreCastExpressionProperties(SgCastExp *cast,
                                     const JsonValue &properties,
                                     const NodeMap &nodes) {
  if (cast == nullptr) {
    throw std::runtime_error(
        "AST JSON cast property restoration has no target expression");
  }

  const int64_t serialized_cast_type = properties.requiredInt("cast_type");
  const auto serialized_conversion_kind =
      requiredClosedEnumRange<SgCastExp::semantic_conversion_kind_enum>(
          properties, "semantic_conversion_kind", "SgCastExp",
          SgCastExp::e_semantic_conversion_unclassified,
          SgCastExp::e_semantic_conversion_last);
  const auto serialized_value_category =
      requiredEnum<SgCastExp::value_category_enum>(
          properties, "value_category", "SgCastExp",
          {SgCastExp::e_value_category_lvalue,
           SgCastExp::e_value_category_xvalue,
           SgCastExp::e_value_category_prvalue});
  if (serialized_cast_type != static_cast<int64_t>(cast->get_cast_type()) ||
      serialized_conversion_kind != cast->get_semantic_conversion_kind() ||
      serialized_value_category != cast->get_value_category()) {
    throw std::runtime_error(
        "AST JSON SgCastExp construction lost exact conversion semantics");
  }

  SgType *source_type =
      nullableTypeFromJson(properties.at("source_type"), nodes);
  const bool explicit_cast =
      cast->get_cast_type() != SgCastExp::e_implicit_cast;
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
  if (base_path_json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON SgCastExp conversion_base_path is not an array");
  }
  SgTypePtrList base_path;
  for (const JsonValue &entry : base_path_json.array) {
    SgType *base_type = typeFromJson(entry, nodes);
    if (base_type == nullptr || isSgTypeUnknown(base_type) != nullptr ||
        isSgTypeDefault(base_type) != nullptr ||
        SageInterface::containsUnknownType(base_type)) {
      throw std::runtime_error(
          "AST JSON SgCastExp has an inexact conversion base path");
    }
    base_path.push_back(base_type);
  }

  cast->set_source_type(source_type);
  cast->set_explicit_name_qualification_present(qualification_present);
  cast->set_explicit_global_qualification(global_qualification);
  cast->set_explicit_name_qualification_tokens(qualification_tokens);
  cast->set_source_type_elaboration_kind(elaboration_kind);
  cast->set_type_elaboration_required(
      properties.requiredBool("type_elaboration_required"));
  cast->set_conversion_base_path(base_path);
}

void restoreOwnedExpressionProperties(SgExpression *expr,
                                      const JsonValue &properties,
                                      const NodeMap &nodes) {
  if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    const JsonValue *symbol_json = properties.find("symbol");
    if (symbol_json == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgFunctionRefExp has no exact symbol "
          "reference");
    }
    const JsonValue &sourceVisibleJson =
        properties.at("fortran_source_visible_symbol");
    const int sourceVisibleBindingKind = static_cast<int>(
        properties.requiredInt("fortran_source_visible_binding_kind"));
    restoreTypeOwnedFortranSourceVisibleBinding(
        ref, sourceVisibleJson, sourceVisibleBindingKind, nodes);
    ref->set_symbol(functionReferenceSemanticSymbolFromJson(
        *symbol_json, sourceVisibleJson,
        ref->get_fortran_source_visible_symbol(), nodes));
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve type-owned SgFunctionRefExp symbol");
    }
    if (typeOwnedSymbolDeserializationActive) {
      deferredTypeOwnedFunctionSymbols.push_back(
          {ref, *symbol_json, sourceVisibleJson, sourceVisibleBindingKind});
    }
  }
  if (SgNewExp *new_expr = isSgNewExp(expr)) {
    SgType *specified_type =
        typeFromJson(properties.at("specified_type"), nodes);
    if (specified_type == nullptr ||
        isSgTypeUnknown(specified_type) != nullptr ||
        isSgTypeDefault(specified_type) != nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgNewExp has no exact specified_type");
    }
    new_expr->set_specified_type(specified_type);
    new_expr->set_need_global_specifier(
        static_cast<short>(properties.requiredInt("need_global_specifier")));
    new_expr->set_type_id_is_parenthesized(
        properties.requiredBool("type_id_is_parenthesized"));
  }
  if (const JsonValue *type =
          validatedExpressionTypeProperty(expr, properties)) {
    SgType *restored_type = typeFromJson(*type, nodes);
    validateExactSemanticExpressionType(expr, restored_type, "deserialization");
    if (SgValueExp *value = isSgValueExp(expr)) {
      const bool hasLiteralSemanticType =
          properties.requiredBool("has_literal_semantic_type");
      if (hasLiteralSemanticType) {
        value->set_literal_type(restored_type);
      } else if (value->get_literal_type() != nullptr) {
        throw std::runtime_error(
            "AST JSON value expression unexpectedly owns a semantic literal "
            "type before reconstruction");
      }
    } else if (restoreExactStoredExpressionType(expr, restored_type)) {
      // Stored by the exact-type node helper.
    } else if (SgCastExp *cast = isSgCastExp(expr)) {
      cast->set_type(restored_type);
      restoreCastExpressionProperties(cast, properties, nodes);
    } else if (SgCallExpression *call = isSgCallExpression(expr)) {
      call->set_expression_type(restored_type);
    } else if (SgSourceLocationBuiltinExp *builtin =
                   isSgSourceLocationBuiltinExp(expr)) {
      builtin->set_expression_type(restored_type);
    } else if (SgTypeTraitBuiltinOperator *builtin =
                   isSgTypeTraitBuiltinOperator(expr)) {
      builtin->set_expression_type(restored_type);
    } else if (SgNewExp *new_expr = isSgNewExp(expr)) {
      installNewExpressionResultType(new_expr, restored_type, *type);
    } else if (SgAggregateInitializer *init = isSgAggregateInitializer(expr)) {
      init->set_expression_type(restored_type);
    } else if (SgConstructorInitializer *init =
                   isSgConstructorInitializer(expr)) {
      init->set_expression_type(restored_type);
    }
  }
  if (SgTypeExpression *type_expression = isSgTypeExpression(expr)) {
    SgType *represented_type =
        typeFromJson(properties.at("represented_type"), nodes);
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
        properties.requiredBool("fortran_has_source_explicit_type");
    SgType *explicit_type = nullableTypeFromJson(
        properties.at("fortran_source_explicit_type"), nodes);
    const bool typed_fortran_constructor =
        init->get_source_form() ==
            SgAggregateInitializer::e_aggregate_initializer_source_fortran ||
        init->get_source_form() ==
            SgAggregateInitializer::
                e_aggregate_initializer_source_fortran_structure;
    if (has_explicit_type != (explicit_type != nullptr) ||
        (!typed_fortran_constructor && has_explicit_type)) {
      throw std::runtime_error(
          "AST JSON SgAggregateInitializer has contradictory Fortran source "
          "type-spec state");
    }
    init->set_fortran_has_source_explicit_type(has_explicit_type);
    init->set_fortran_source_explicit_type(explicit_type);
  }
  expr->set_lvalue(properties.requiredBool("lvalue"));
  expr->set_need_paren(properties.requiredBool("need_paren"));
  expr->set_global_qualified_name(
      properties.requiredBool("global_qualified_name"));
  expr->set_semantic_wrapper_mask(
      static_cast<SgExpression::semantic_wrapper_mask_enum>(
          properties.requiredInt("semantic_wrapper_mask")));
  const bool fortran_integer_value_available =
      properties.requiredBool("fortran_integer_constant_value_is_available");
  const std::int64_t fortran_integer_value =
      properties.requiredInt("fortran_integer_constant_value");
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
            properties, "literal_spelling_form", "SgValueExp",
            {SgValueExp::e_literal_source_spelled,
             SgValueExp::e_literal_canonical_generated}));
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    const int64_t raw_role = properties.requiredInt("arrow_emission_role");
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
        properties.requiredBool("is_braced_initialized"));
  }
  if (SgFunctionCallExp *call = isSgFunctionCallExp(expr)) {
    restoreFunctionCallSourceMetadata(call, properties);
    const JsonValue *function_json = properties.find("function");
    const JsonValue *args_json = properties.find("args");
    if (function_json == nullptr || args_json == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgFunctionCallExp is missing its callee or "
          "argument list");
    }
    SgExpression *function = expressionFromRef(*function_json, nodes);
    SgExprListExp *args = exprListExpFromTypeJson(*args_json, nodes);
    if (function == nullptr || args == nullptr ||
        function->get_parent() != nullptr || args->get_parent() != nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgFunctionCallExp has non-exclusive callee "
          "or argument-list ownership");
    }
    call->set_function(function);
    function->set_parent(call);
    call->set_args(args);
    args->set_parent(call);
  }
  if (SgTypeTraitBuiltinOperator *builtin =
          isSgTypeTraitBuiltinOperator(expr)) {
    const JsonValue *args_json = properties.find("args");
    if (args_json == nullptr || args_json->kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON type-owned SgTypeTraitBuiltinOperator is missing its "
          "expression operands");
    }
    for (const JsonValue &argument_json : args_json->array) {
      SgExpression *argument = expressionFromRef(argument_json, nodes);
      if (argument == nullptr || argument->get_parent() != nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned SgTypeTraitBuiltinOperator has a null or "
            "non-exclusive expression operand");
      }
      builtin->get_args().push_back(argument);
      argument->set_parent(builtin);
    }
  }
  restoreExpressionQualificationFields(expr, properties);

  if (SgNewExp *new_expr = isSgNewExp(expr)) {
    SgExprListExp *placement_args =
        exprListExpFromTypeJson(properties.at("placement_args"), nodes);
    SgExpression *constructor_expression =
        expressionFromRef(properties.at("constructor_args"), nodes);
    SgConstructorInitializer *constructor_args =
        isSgConstructorInitializer(constructor_expression);
    if (constructor_expression != nullptr && constructor_args == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgNewExp constructor_args are not an exact "
          "SgConstructorInitializer");
    }
    SgExpression *builtin_args =
        expressionFromRef(properties.at("builtin_args"), nodes);
    auto require_unowned = [&](SgExpression *child, const std::string &field) {
      if (child != nullptr && child->get_parent() != nullptr) {
        throw std::runtime_error("AST JSON type-owned SgNewExp " + field +
                                 " already has an owner");
      }
    };
    require_unowned(placement_args, "placement_args");
    require_unowned(constructor_args, "constructor_args");
    require_unowned(builtin_args, "builtin_args");
    new_expr->set_placement_args(placement_args);
    new_expr->set_constructor_args(constructor_args);
    new_expr->set_builtin_args(builtin_args);
    if (placement_args != nullptr) {
      placement_args->set_parent(new_expr);
    }
    if (constructor_args != nullptr) {
      constructor_args->set_parent(new_expr);
    }
    if (builtin_args != nullptr) {
      builtin_args->set_parent(new_expr);
    }
    const int64_t declaration_id =
        properties.requiredInt("new_operator_declaration");
    if (declaration_id < 0) {
      throw std::runtime_error(
          "AST JSON type-owned SgNewExp has a negative operator declaration");
    }
    new_expr->set_newOperatorDeclaration(
        declaration_id != 0 ? nodeByIdAs<SgFunctionDeclaration>(
                                  nodes, static_cast<uint64_t>(declaration_id))
                            : nullptr);
  }
  if (SgSubscriptExpression *subscript = isSgSubscriptExpression(expr)) {
    auto restore_child = [&](const std::string &field, auto setter) {
      if (const JsonValue *value = properties.find(field)) {
        SgExpression *child = expressionFromRef(*value, nodes);
        (subscript->*setter)(child);
        if (child != nullptr) {
          child->set_parent(subscript);
        }
      }
    };
    restore_child("lower_bound", &SgSubscriptExpression::set_lowerBound);
    restore_child("upper_bound", &SgSubscriptExpression::set_upperBound);
    restore_child("stride", &SgSubscriptExpression::set_stride);
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    if (const JsonValue *value = properties.find("operand")) {
      SgExpression *operand = expressionFromRef(*value, nodes);
      if (operand == nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned unary expression is missing operand");
      }
      unary->set_operand_i(operand);
      operand->set_parent(unary);
    }
  }
  if (SgBinaryOp *binary = isSgBinaryOp(expr)) {
    auto require_binary_child = [&](const std::string &field) {
      const JsonValue *value = properties.find(field);
      if (value == nullptr) {
        throw std::runtime_error("AST JSON type-owned binary expression is "
                                 "missing " +
                                 field);
      }
      SgExpression *child = expressionFromRef(*value, nodes);
      if (child == nullptr) {
        throw std::runtime_error("AST JSON type-owned binary expression has "
                                 "null " +
                                 field);
      }
      return child;
    };
    SgExpression *lhs = require_binary_child("lhs_operand");
    SgExpression *rhs = require_binary_child("rhs_operand");
    binary->set_lhs_operand_i(lhs);
    lhs->set_parent(binary);
    binary->set_rhs_operand_i(rhs);
    rhs->set_parent(binary);
  }
  auto require_exclusive_expression_child = [&](const std::string &field,
                                                const std::string &owner_kind) {
    const JsonValue *value = properties.find(field);
    if (value == nullptr) {
      throw std::runtime_error("AST JSON type-owned " + owner_kind +
                               " is missing " + field);
    }
    SgExpression *child = expressionFromRef(*value, nodes);
    if (child == nullptr || child->get_parent() != nullptr) {
      throw std::runtime_error("AST JSON type-owned " + owner_kind +
                               " has non-exclusive " + field);
    }
    return child;
  };
  if (SgAwaitExpression *await_expression = isSgAwaitExpression(expr)) {
    SgExpression *value =
        require_exclusive_expression_child("value", "SgAwaitExpression");
    await_expression->set_value(value);
    value->set_parent(await_expression);
  }
  if (SgFoldExpression *fold = isSgFoldExpression(expr)) {
    SgExpression *operands =
        require_exclusive_expression_child("operands", "SgFoldExpression");
    fold->set_operands(operands);
    operands->set_parent(fold);
  }
  if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(expr)) {
    SgExpression *pattern = require_exclusive_expression_child(
        "pattern_expression", "SgPackExpansionExpr");
    pack->set_pattern_expression(pattern);
    pattern->set_parent(pack);
  }
  if (SgNoexceptOp *noexcept_op = isSgNoexceptOp(expr)) {
    SgExpression *operand =
        require_exclusive_expression_child("operand_expr", "SgNoexceptOp");
    noexcept_op->set_operand_expr(operand);
    operand->set_parent(noexcept_op);
  }
  if (SgComplexVal *value = isSgComplexVal(expr)) {
    if (const JsonValue *type = properties.find("precision_type")) {
      value->set_precisionType(typeFromJson(*type, nodes));
    }
    if (const JsonValue *real_json = properties.find("real_value")) {
      SgExpression *real = expressionFromRef(*real_json, nodes);
      value->set_real_value(real);
      if (real != nullptr) {
        real->set_parent(value);
      }
    }
    if (const JsonValue *imag_json = properties.find("imaginary_value")) {
      SgExpression *imag = expressionFromRef(*imag_json, nodes);
      value->set_imaginary_value(imag);
      if (imag != nullptr) {
        imag->set_parent(value);
      }
    }
    value->set_valueString(properties.requiredString("value_string"));
  }

  auto restore_unary_trait_operand = [&](auto *trait,
                                         const std::string &owner_kind) {
    const JsonValue *operand_type_json = properties.find("operand_type");
    const JsonValue *operand_expr_json = properties.find("operand_expr");
    if (operand_type_json == nullptr || operand_expr_json == nullptr) {
      throw std::runtime_error("AST JSON type-owned " + owner_kind +
                               " is missing its exact operand payload");
    }
    SgType *operand_type = operand_type_json->requiredBool("present")
                               ? typeFromJson(*operand_type_json, nodes)
                               : nullptr;
    SgExpression *operand_expr = expressionFromRef(*operand_expr_json, nodes);
    if ((operand_expr == nullptr) == (operand_type == nullptr)) {
      throw std::runtime_error("AST JSON type-owned " + owner_kind +
                               " must have exactly one operand");
    }
    if (operand_expr != nullptr && operand_expr->get_parent() != nullptr) {
      throw std::runtime_error("AST JSON type-owned " + owner_kind +
                               " expression operand already has an owner");
    }
    trait->set_operand_expr(operand_expr);
    trait->set_operand_type(operand_type);
    if (operand_expr != nullptr) {
      operand_expr->set_parent(trait);
    }
  };
  if (SgSizeOfOp *size_of = isSgSizeOfOp(expr)) {
    restore_unary_trait_operand(size_of, "SgSizeOfOp");
    size_of->set_is_objectless_nonstatic_data_member_reference(
        properties.requiredBool(
            "is_objectless_nonstatic_data_member_reference"));
    size_of->set_is_sizeof_pack(properties.requiredBool("is_sizeof_pack"));
  }
  if (SgAlignOfOp *align_of = isSgAlignOfOp(expr)) {
    restore_unary_trait_operand(align_of, "SgAlignOfOp");
  }

  if (SgEnumVal *value = isSgEnumVal(expr)) {
    const uint64_t declaration_id =
        static_cast<uint64_t>(properties.requiredInt("declaration"));
    if (declaration_id == 0) {
      throw std::runtime_error(
          "AST JSON type-owned SgEnumVal has no enum declaration");
    }
    value->set_declaration(
        nodeByIdAs<SgEnumDeclaration>(nodes, declaration_id));
    value->set_requiresNameQualification(
        properties.requiredBool("requires_name_qualification"));
  }

  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    const JsonValue *symbol_json = properties.find("symbol");
    if (symbol_json == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned SgVarRefExp has no exact symbol reference");
    }
    ref->set_symbol(isSgVariableSymbol(symbolFromJson(*symbol_json, nodes)));
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve type-owned SgVarRefExp symbol");
    }
    validateAnonymousDataMemberReferenceQualification(ref);
    if (typeOwnedSymbolDeserializationActive) {
      deferredTypeOwnedVariableSymbols.push_back({ref, *symbol_json});
    }
  }
  if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    const int64_t raw_declaration_id =
        properties.requiredInt("symbol_declaration");
    if (raw_declaration_id <= 0) {
      throw std::runtime_error(
          "AST JSON type-owned SgNonrealRefExp has no exact symbol "
          "declaration");
    }
    SgNonrealDecl *declaration = nodeByIdAs<SgNonrealDecl>(
        nodes, static_cast<uint64_t>(raw_declaration_id));
    SgNonrealSymbol *symbol =
        isSgNonrealSymbol(symbolFromJson(properties.at("symbol"), nodes));
    if (symbol == nullptr || symbol->get_declaration() != declaration ||
        properties.requiredString("symbol_name") !=
            symbol->get_name().getString()) {
      throw std::runtime_error(
          "AST JSON type-owned SgNonrealRefExp symbol cross-edge is "
          "inconsistent");
    }
    ref->set_symbol(symbol);

    const uint64_t resolved_function_id = static_cast<uint64_t>(
        properties.requiredInt("resolved_function_declaration"));
    if (resolved_function_id != 0) {
      SgFunctionDeclaration *resolved_function =
          isSgFunctionDeclaration(nodeById(nodes, resolved_function_id));
      if (resolved_function == nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned SgNonrealRefExp resolved callable is not a "
            "function declaration");
      }
      ref->set_resolved_function_declaration(resolved_function);
    }
    const uint64_t resolved_variable_id = static_cast<uint64_t>(
        properties.requiredInt("resolved_variable_declaration"));
    if (resolved_variable_id != 0) {
      if (resolved_function_id != 0) {
        throw std::runtime_error(
            "AST JSON type-owned SgNonrealRefExp resolves to both a function "
            "and a variable template");
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
            "AST JSON type-owned SgNonrealRefExp resolved variable template "
            "is not an exact typed specialization declaration");
      }
      ref->set_resolved_variable_declaration(resolved_variable);
    }

    const int64_t semantic_role = properties.requiredInt("semantic_role");
    if (semantic_role != SgNonrealRefExp::e_nonreal_reference &&
        semantic_role != SgNonrealRefExp::e_dependent_callable) {
      throw std::runtime_error(
          "AST JSON type-owned SgNonrealRefExp has invalid semantic_role");
    }
    ref->set_semantic_role(
        static_cast<SgNonrealRefExp::semantic_role_enum>(semantic_role));
    ref->get_templateArguments() = templateArgumentListFromJson(
        properties.at("template_arguments"), nodes, ref);
    ref->set_explicit_template_argument_list(
        properties.requiredBool("explicit_template_argument_list"));
    ref->set_constraintSatisfactionEvaluated(
        properties.requiredBool("constraint_satisfaction_evaluated"));
    ref->set_constraintSatisfactionSatisfied(
        properties.requiredBool("constraint_satisfaction_satisfied"));
    ref->set_constraintSatisfactionContainsErrors(
        properties.requiredBool("constraint_satisfaction_contains_errors"));
    ref->set_constraintSatisfactionSubstitutionFailure(properties.requiredBool(
        "constraint_satisfaction_substitution_failure"));
    ref->set_constraintSatisfactionSummary(
        properties.requiredString("constraint_satisfaction_summary"));
    ref->set_sfinaeEvaluated(properties.requiredBool("sfinae_evaluated"));
    ref->set_sfinaeSubstitutionFailure(
        properties.requiredBool("sfinae_substitution_failure"));
    ref->set_sfinaeSummary(properties.requiredString("sfinae_summary"));
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    cast->validate_semantic_conversion();
  }
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON expression reference is not an object");
  }
  const JsonValue *node_value = json.find("node");
  if (node_value == nullptr) {
    throw std::runtime_error("AST JSON expression reference is missing node");
  }
  const uint64_t id = static_cast<uint64_t>(json.requiredInt("node"));
  if (id != 0) {
    return nodeByIdAs<SgExpression>(nodes, id);
  }
  if (const JsonValue *owned_kind = json.find("owned_kind")) {
    NodeRecord record;
    record.kind = owned_kind->asString();
    record.variant = 0;
    record.flags = json.at("flags");
    record.location = json.at("location");
    record.properties = json.at("properties");
    record.preprocessing = record.properties.find("preprocessing") != nullptr
                               ? *record.properties.find("preprocessing")
                               : JsonValue::arrayValue({});
    SgNode *node = nullptr;
    if (record.kind == "SgConstructorInitializer") {
      SgExprListExp *arguments =
          exprListExpFromTypeJson(record.properties.at("args"), nodes);
      SgType *destination_type =
          typeFromJson(record.properties.at("type"), nodes);
      const int64_t declaration_id =
          record.properties.requiredInt("declaration");
      if (arguments == nullptr || arguments->get_parent() != nullptr ||
          destination_type == nullptr ||
          isSgTypeUnknown(destination_type) != nullptr ||
          isSgTypeDefault(destination_type) != nullptr || declaration_id < 0) {
        throw std::runtime_error(
            "AST JSON type-owned SgConstructorInitializer has incomplete "
            "construction input");
      }
      SgMemberFunctionDeclaration *declaration =
          declaration_id != 0
              ? nodeByIdAs<SgMemberFunctionDeclaration>(
                    nodes, static_cast<uint64_t>(declaration_id))
              : nullptr;
      SgConstructorInitializer *initializer = new SgConstructorInitializer(
          declaration, arguments, destination_type,
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
          "type-owned SgConstructorInitializer qualification");
      const auto elaboration_kind =
          requiredEnum<SgNonrealDecl::source_elaboration_kind_enum>(
              record.properties, "source_type_elaboration_kind",
              "type-owned SgConstructorInitializer",
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
           (elaboration_kind !=
            SgNonrealDecl::e_source_elaboration_unspecified))) {
        throw std::runtime_error(
            "AST JSON type-owned SgConstructorInitializer has contradictory "
            "written type qualification");
      }
      initializer->set_explicit_name_qualification_present(
          qualification_present);
      initializer->set_explicit_global_qualification(global_qualification);
      initializer->set_explicit_name_qualification_tokens(qualification_tokens);
      initializer->set_source_type_elaboration_kind(elaboration_kind);
      initializer->set_type_elaboration_required(
          record.properties.requiredBool("type_elaboration_required"));
      node = initializer;
    } else {
      node = createNodeFromRecord(record, nullptr, JsonValue::objectValue({}));
    }
    SgExpression *expr = isSgExpression(node);
    if (expr == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned record did not rebuild an expression");
    }
    if (record.flags.requiredBool("contains_transformation")) {
      expr->set_containsTransformation(true);
    } else {
      expr->set_containsTransformation(false);
    }
    if (SgLocatedNode *located = isSgLocatedNode(expr)) {
      if (record.flags.requiredBool(
              "contains_transformation_to_surrounding_whitespace")) {
        located->set_containsTransformationToSurroundingWhitespace(true);
      } else {
        located->set_containsTransformationToSurroundingWhitespace(false);
      }
    }
    setOwnedExpressionSourcePosition(expr, record.location);
    restoreOwnedExpressionProperties(expr, record.properties, nodes);
    return expr;
  }
  return nullptr;
}

SgExprListExp *exprListExpFromTypeJson(const JsonValue &json,
                                       const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object || !json.requiredBool("present")) {
    return nullptr;
  }
  const std::string kind = json.at("kind").asString();
  if (kind != "SgExprListExp") {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info is not SgExprListExp: " + kind);
  }

  NodeRecord record;
  record.kind = kind;
  record.variant = 0;
  record.flags = json.at("flags");
  record.location = json.at("location");
  record.properties = json.at("properties");
  record.preprocessing = record.properties.find("preprocessing") != nullptr
                             ? *record.properties.find("preprocessing")
                             : JsonValue::arrayValue({});

  SgNode *node =
      createNodeFromRecord(record, nullptr, JsonValue::objectValue({}));
  SgExprListExp *list = isSgExprListExp(node);
  if (list == nullptr) {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info did not rebuild an expression list");
  }

  if (record.flags.requiredBool("contains_transformation")) {
    list->set_containsTransformation(true);
  } else {
    list->set_containsTransformation(false);
  }
  if (record.flags.requiredBool(
          "contains_transformation_to_surrounding_whitespace")) {
    list->set_containsTransformationToSurroundingWhitespace(true);
  } else {
    list->set_containsTransformationToSurroundingWhitespace(false);
  }
  setOwnedExpressionSourcePosition(list, record.location);
  restoreOwnedExpressionProperties(list, record.properties, nodes);

  const JsonValue &expressions = json.at("expressions");
  if (expressions.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info expressions field is not an array");
  }
  for (const JsonValue &expr_json : expressions.array) {
    SgExpression *expr = expressionFromRef(expr_json, nodes);
    if (expr == nullptr) {
      throw std::runtime_error(
          "AST JSON SgArrayType dim_info contains a null expression");
    }
    list->append_expression(expr);
  }
  return list;
}

bool sameAstJsonPath(const std::string &lhs, const std::string &rhs) {
  if (lhs.empty() || rhs.empty()) {
    return false;
  }
  if (lhs == rhs) {
    return true;
  }

  const std::filesystem::path lhs_path(lhs);
  const std::filesystem::path rhs_path(rhs);
  if (lhs_path.lexically_normal() == rhs_path.lexically_normal()) {
    return true;
  }

  std::error_code lhs_error;
  std::error_code rhs_error;
  const bool lhs_exists = std::filesystem::exists(lhs_path, lhs_error);
  const bool rhs_exists = std::filesystem::exists(rhs_path, rhs_error);
  if (!lhs_exists || !rhs_exists || lhs_error || rhs_error) {
    return false;
  }

  std::error_code canonical_lhs_error;
  std::error_code canonical_rhs_error;
  const std::filesystem::path canonical_lhs =
      std::filesystem::weakly_canonical(lhs_path, canonical_lhs_error);
  const std::filesystem::path canonical_rhs =
      std::filesystem::weakly_canonical(rhs_path, canonical_rhs_error);
  return !canonical_lhs_error && !canonical_rhs_error &&
         canonical_lhs == canonical_rhs;
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
      add_source(external_source);
    }
  }
  return sources;
}

bool externalClassCandidateMatches(SgClassDeclaration *candidate,
                                   const JsonValue &json) {
  if (candidate == nullptr) {
    return false;
  }

  const std::string expected_kind = json.at("kind").asString();
  const std::string expected_name = json.at("name").asString();
  const SgClassDeclaration::class_types expected_class_type =
      requiredClassType(json, "external class declaration");
  const std::string expected_module = json.at("module_name").asString();
  const bool expected_has_definition = json.at("has_definition").asBool();
  const bool expected_is_first_nondefining =
      json.at("is_first_nondefining").asBool();

  return candidate->sage_class_name() == expected_kind &&
         candidate->get_name().getString() == expected_name &&
         candidate->get_class_type() == expected_class_type &&
         moduleNameForNode(candidate) == expected_module &&
         classDeclarationHasDefinition(candidate) == expected_has_definition &&
         classDeclarationIsFirstNondefining(candidate) ==
             expected_is_first_nondefining;
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
  for (RoseAst::iterator it = ast.begin().withoutNullValues(); it != ast.end();
       ++it) {
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

void validateExternalClassDeclarationInProject(const JsonValue &json) {
  if (currentDeserializationProject == nullptr) {
    return;
  }

  const std::string source_file = json.at("source_file").asString();
  SgClassDeclaration *matched = nullptr;
  bool saw_source_file = false;
  for (SgSourceFile *source : currentDeserializationSourceFiles()) {
    if (!sourceFileMatchesExternalRecord(source, source_file)) {
      continue;
    }
    saw_source_file = true;

    for (SgClassDeclaration *candidate :
         structuralClassDeclarationsForSource(source)) {
      if (!externalClassCandidateMatches(candidate, json)) {
        continue;
      }
      if (matched != nullptr) {
        throw std::runtime_error(
            "AST JSON external class declaration is ambiguous in source file " +
            source_file + ": " + json.at("kind").asString() + " " +
            json.at("name").asString());
      }
      matched = candidate;
    }
  }

  if (saw_source_file && matched == nullptr) {
    throw std::runtime_error("AST JSON external class declaration was not "
                             "found in loaded source file " +
                             source_file + ": " + json.at("kind").asString() +
                             " " + json.at("name").asString());
  }
}

void installTransformationSourcePosition(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }

  Sg_File_Info *start =
      Sg_File_Info::generateDefaultFileInfoForTransformationNode();
  Sg_File_Info *end =
      Sg_File_Info::generateDefaultFileInfoForTransformationNode();
  start->set_parent(node);
  end->set_parent(node);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  node->set_file_info(start);
}

bool nodeSupportsRestoredSourcePosition(SgNode *node) {
  return isSgLocatedNode(node) != nullptr ||
         isSgInitializedName(node) != nullptr || isSgPragma(node) != nullptr ||
         isSgFile(node) != nullptr;
}

void restoreNodeSourcePositionFromJson(SgNode *node, const JsonValue &location,
                                       const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + context +
                             " location must be an object");
  }
  if (!nodeSupportsRestoredSourcePosition(node)) {
    throw std::runtime_error("AST JSON " + context +
                             " node kind has no restorable source position: " +
                             node->sage_class_name());
  }
  const JsonValue *start = location.find("start");
  if (start == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " location has no start field");
  }
  std::unique_ptr<Sg_File_Info> start_info(buildFileInfo(*start, node));
  if (start_info == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a present startOfConstruct");
  }
  const JsonValue *end = location.find("end");
  std::unique_ptr<Sg_File_Info> end_info(
      end != nullptr ? buildFileInfo(*end, node) : nullptr);
  Sg_File_Info *start_raw = start_info.release();
  Sg_File_Info *end_raw = end_info.release();
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_raw);
    located->set_endOfConstruct(end_raw);
    if (SgExpression *expression = isSgExpression(node)) {
      const JsonValue *operator_position = location.find("operator");
      if (operator_position == nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " location has no operator field");
      }
      expression->set_operatorPosition(
          buildFileInfo(*operator_position, expression));
    } else {
      located->set_file_info(start_raw);
    }
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_raw);
    name->set_endOfConstruct(end_raw);
    name->set_file_info(start_raw);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_raw);
    pragma->set_endOfConstruct(end_raw);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_raw);
  }
}

void restoreOptionalNodeSourcePositionFromJson(SgNode *node,
                                               const JsonValue &location,
                                               const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + context +
                             " location must be an object");
  }
  const JsonValue *start = location.find("start");
  const JsonValue *end = location.find("end");
  if ((start != nullptr || end != nullptr) &&
      !nodeSupportsRestoredSourcePosition(node)) {
    throw std::runtime_error("AST JSON " + context +
                             " node kind has no restorable source position: " +
                             node->sage_class_name());
  }
  std::unique_ptr<Sg_File_Info> start_info(
      start != nullptr ? buildFileInfo(*start, node) : nullptr);
  std::unique_ptr<Sg_File_Info> end_info(
      end != nullptr ? buildFileInfo(*end, node) : nullptr);
  Sg_File_Info *start_raw = start_info.release();
  Sg_File_Info *end_raw = end_info.release();
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_raw);
    located->set_endOfConstruct(end_raw);
    if (SgExpression *expression = isSgExpression(node)) {
      const JsonValue *operator_position = location.find("operator");
      if (operator_position == nullptr) {
        throw std::runtime_error("AST JSON " + context +
                                 " location has no operator field");
      }
      expression->set_operatorPosition(
          buildFileInfo(*operator_position, expression));
    } else {
      located->set_file_info(start_raw);
    }
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_raw);
    name->set_endOfConstruct(end_raw);
    name->set_file_info(start_raw);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_raw);
    pragma->set_endOfConstruct(end_raw);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_raw);
  }
}

SgBitVector bitVectorFromJson(const JsonValue &json,
                              const std::string &field_name);
SgModuleStatement *externalModuleFromJson(const JsonValue &json);
SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json);
void validateExternalSymbolBasisOwnership(SgSymbol *symbol);

void restoreDeclarationTypeModifierFields(SgTypeModifier &modifier,
                                          const JsonValue &json,
                                          const std::string &context) {
  modifier.set_modifierVector(
      bitVectorFromJson(json.at("declaration_type_modifier_vector"),
                        context + " declaration_type_modifier_vector"));
  modifier.get_constVolatileModifier().set_modifier(
      requiredEnum<SgConstVolatileModifier::cv_modifier_enum>(
          json, "declaration_type_const_volatile_modifier", context,
          {SgConstVolatileModifier::e_unknown,
           SgConstVolatileModifier::e_default, SgConstVolatileModifier::e_const,
           SgConstVolatileModifier::e_volatile,
           SgConstVolatileModifier::e_const_volatile}));
  modifier.get_elaboratedTypeModifier().set_modifier(
      requiredEnum<SgElaboratedTypeModifier::elaborated_type_modifier_enum>(
          json, "declaration_type_elaborated_modifier", context,
          {SgElaboratedTypeModifier::e_unknown,
           SgElaboratedTypeModifier::e_default,
           SgElaboratedTypeModifier::e_class,
           SgElaboratedTypeModifier::e_struct,
           SgElaboratedTypeModifier::e_union, SgElaboratedTypeModifier::e_enum,
           SgElaboratedTypeModifier::e_typename}));

  const int64_t alignment =
      json.requiredInt("declaration_type_gnu_attribute_alignment");
  const int64_t sentinel =
      json.requiredInt("declaration_type_gnu_attribute_sentinel");
  const int64_t address_space =
      json.requiredInt("declaration_type_address_space_value");
  const int64_t vector_size = json.requiredInt("declaration_type_vector_size");
  if (alignment < std::numeric_limits<int>::min() ||
      alignment > std::numeric_limits<int>::max() ||
      sentinel < std::numeric_limits<long>::min() ||
      sentinel > std::numeric_limits<long>::max() || address_space < 0 ||
      static_cast<uint64_t>(address_space) >
          std::numeric_limits<unsigned>::max() ||
      vector_size < 0 ||
      static_cast<uint64_t>(vector_size) >
          std::numeric_limits<unsigned>::max()) {
    throw std::runtime_error("AST JSON " + context +
                             " has an out-of-range type modifier payload");
  }
  modifier.set_gnu_attribute_alignment(static_cast<int>(alignment));
  modifier.set_gnu_attribute_sentinel(static_cast<long>(sentinel));
  modifier.set_address_space_value(static_cast<unsigned>(address_space));
  modifier.set_vector_size(static_cast<unsigned>(vector_size));
}

void restoreExternalDeclarationStatementFields(SgDeclarationStatement *decl,
                                               const JsonValue &json,
                                               const std::string &context) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a declaration statement");
  }
  restoreTranslationUnitSourceOrder(decl, json, context);
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(decl)) {
    const auto ownership =
        requiredEnum<SgFunctionDeclaration::frontend_source_ownership_enum>(
            json, "frontend_source_ownership", context,
            {SgFunctionDeclaration::e_frontend_source_unclassified,
             SgFunctionDeclaration::e_frontend_source_main_file,
             SgFunctionDeclaration::e_frontend_source_application_header,
             SgFunctionDeclaration::e_frontend_source_system_header,
             SgFunctionDeclaration::e_frontend_source_external,
             SgFunctionDeclaration::e_frontend_source_support,
             SgFunctionDeclaration::e_frontend_source_pseudo_file,
             SgFunctionDeclaration::e_frontend_source_implicit});
    if (ownership != SgFunctionDeclaration::e_frontend_source_unclassified) {
      function->initialize_frontend_source_ownership(ownership);
    }
    const auto origin =
        requiredEnum<SgFunctionDeclaration::frontend_declaration_origin_enum>(
            json, "frontend_declaration_origin", context,
            {SgFunctionDeclaration::e_frontend_declaration_unclassified,
             SgFunctionDeclaration::e_frontend_declaration_explicit,
             SgFunctionDeclaration::e_frontend_declaration_implicit});
    if (origin != SgFunctionDeclaration::e_frontend_declaration_unclassified) {
      function->initialize_frontend_declaration_origin(origin);
    }
  }
  decl->set_decl_attributes(
      static_cast<unsigned int>(json.at("decl_attributes").asInt()));
  decl->set_linkage(json.at("linkage").asString());
  decl->get_declarationModifier().set_modifierVector(
      bitVectorFromJson(json.at("declaration_modifier_vector"),
                        context + " declaration_modifier_vector"));
  restoreDeclarationTypeModifierFields(
      decl->get_declarationModifier().get_typeModifier(), json, context);
  decl->get_declarationModifier().get_storageModifier().set_modifier(
      requiredStorageModifier(json, "declaration_storage_modifier", context));
  decl->get_declarationModifier()
      .get_storageModifier()
      .set_thread_local_storage(
          json.requiredBool("declaration_thread_local_storage"));
  decl->get_declarationModifier().get_accessModifier().set_modifier(
      requiredDeclarationAccessModifier(json, "declaration_access_modifier",
                                        context));
  decl->get_declarationModifier().get_accessModifier().set_is_explicit(
      json.requiredBool("declaration_access_is_explicit"));
  decl->set_nameOnly(json.at("name_only").asBool());
  decl->set_forward(json.at("forward").asBool());
  decl->set_externBrace(json.at("extern_brace").asBool());
  decl->set_skipElaborateType(json.at("skip_elaborate_type").asBool());
  decl->set_binding_label(json.at("binding_label").asString());
  decl->set_binding_cdefined(json.requiredBool("binding_cdefined"));
  decl->set_unparse_template_ast(json.at("unparse_template_ast").asBool());
  const bool source_name_present =
      json.requiredBool("source_name_qualification_present");
  const bool source_name_global =
      json.requiredBool("source_name_global_qualification");
  const SgStringList source_name_tokens =
      stringListFromJson(json.at("source_name_qualification_tokens"),
                         "source_name_qualification_tokens");
  if (!source_name_present &&
      (source_name_global || !source_name_tokens.empty())) {
    throw std::runtime_error(
        "AST JSON " + context +
        " source name qualifier payload is present without its presence bit");
  }
  decl->set_source_name_global_qualification(source_name_global);
  decl->get_source_name_qualification_tokens() = source_name_tokens;
  decl->set_source_name_qualification_present(source_name_present);
  decl->get_declarationModifier().set_gnu_attribute_visibility(
      requiredGnuDeclarationVisibility(
          json, "declaration_gnu_attribute_visibility", context));
  decl->get_declarationModifier().set_gnu_type_visibility(
      requiredGnuDeclarationVisibility(json, "declaration_gnu_type_visibility",
                                       context));

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
  decl->set_firstNondefiningDeclaration(first_is_self ? decl : nullptr);
  decl->set_definingDeclaration(defining_is_self ? decl : nullptr);
}

SgInitializedName *externalInitializedNameFromJson(const JsonValue &json,
                                                   const NodeMap &nodes,
                                                   SgDeclarationStatement *decl,
                                                   SgScopeStatement *scope,
                                                   const std::string &context) {
  const std::string name = json.at("name").asString();
  SgType *type = typeFromJson(json.at("type"), nodes);
  SgInitializedName *initialized_name =
      new SgInitializedName(nullptr, SgName(name), type);
  initialized_name->set_fortran_source_type(
      nullableTypeFromJson(json.at("fortran_source_type"), nodes));
  initialized_name->set_cxx_source_type(
      nullableTypeFromJson(json.at("cxx_source_type"), nodes));
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " initializedName has no location");
  }
  restoreNodeSourcePositionFromJson(initialized_name, *location,
                                    context + " initializedName " + name);
  initialized_name->set_parent(decl);
  initialized_name->set_declptr(decl);
  initialized_name->set_scope(scope);
  initialized_name->set_fortran_type_spec(
      requiredFortranTypeSpec(json, context + " initializedName"));
  initialized_name->set_fortran_procedure_interface(
      SgName(json.requiredString("fortran_procedure_interface")));
  initialized_name->set_is_predefined_identifier(
      json.requiredBool("is_predefined_identifier"));
  initialized_name->set_preinitialization(
      requiredPreinitialization(json, context + " initializedName"));
  if (initialized_name->get_preinitialization() !=
      SgInitializedName::e_unknown_preinitialization) {
    throw std::runtime_error(
        "AST JSON " + context +
        " external initializedName has a constructor preinitialization role");
  }
  initialized_name->set_generated_variable_role(
      requiredEnum<SgInitializedName::generated_variable_role_enum>(
          json, "generated_variable_role", "external SgInitializedName",
          {SgInitializedName::e_generated_variable_none,
           SgInitializedName::e_generated_loop_tiling_index,
           SgInitializedName::e_generated_loop_tiling_increment}));
  initialized_name->get_storageModifier().set_modifier(requiredStorageModifier(
      json, "storage_modifier", context + " initializedName"));
  const std::string section = json.requiredString("gnu_attribute_section_name");
  if (!section.empty()) {
    initialized_name->set_gnu_attribute_section_name(section);
  }
  initialized_name->set_name_qualification_length(
      static_cast<int>(json.requiredInt("name_qualification_length")));
  initialized_name->set_type_elaboration_required(
      json.requiredBool("type_elaboration_required"));
  initialized_name->set_global_qualification_required(
      json.requiredBool("global_qualification_required"));
  initialized_name->set_name_qualification_length_for_type(
      static_cast<int>(json.requiredInt("name_qualification_length_for_type")));
  initialized_name->set_type_elaboration_required_for_type(
      json.requiredBool("type_elaboration_required_for_type"));
  initialized_name->set_global_qualification_required_for_type(
      json.requiredBool("global_qualification_required_for_type"));
  const bool source_type_present =
      json.requiredBool("source_type_qualification_present");
  const bool source_type_global =
      json.requiredBool("source_type_global_qualification");
  const SgStringList source_type_tokens =
      stringListFromJson(json.at("source_type_qualification_tokens"),
                         "source_type_qualification_tokens");
  if (!source_type_present &&
      (source_type_global || !source_type_tokens.empty())) {
    throw std::runtime_error(
        "AST JSON " + context +
        " initialized-name source type qualifier payload is present without "
        "its presence bit");
  }
  initialized_name->set_source_type_global_qualification(source_type_global);
  initialized_name->get_source_type_qualification_tokens() = source_type_tokens;
  initialized_name->set_source_type_qualification_present(source_type_present);
  const bool source_name_present =
      json.requiredBool("source_name_qualification_present");
  const bool source_name_global =
      json.requiredBool("source_name_global_qualification");
  const SgStringList source_name_tokens =
      stringListFromJson(json.at("source_name_qualification_tokens"),
                         "source_name_qualification_tokens");
  if (!source_name_present &&
      (source_name_global || !source_name_tokens.empty())) {
    throw std::runtime_error(
        "AST JSON " + context +
        " initialized-name source name qualifier payload is present without "
        "its presence bit");
  }
  initialized_name->set_source_name_global_qualification(source_name_global);
  initialized_name->get_source_name_qualification_tokens() = source_name_tokens;
  initialized_name->set_source_name_qualification_present(source_name_present);
  return initialized_name;
}

SgVariableDeclaration *
externalVariableDeclarationFromJson(const JsonValue &json, const NodeMap &nodes,
                                    SgFunctionParameterScope *scope,
                                    const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgVariableDeclaration") {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " functionParameterScope declaration kind is unsupported: " + kind);
  }
  SgVariableDeclaration *decl = new SgVariableDeclaration();
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration has no location");
  }
  restoreNodeSourcePositionFromJson(decl, *location,
                                    "external_function variableDeclaration " +
                                        function_name);
  decl->set_parent(scope);
  decl->set_scope(scope);
  restoreExternalDeclarationStatementFields(
      decl, json,
      "external_function " + function_name + " variableDeclaration");
  decl->set_requiresGlobalNameQualificationOnType(
      json.requiredBool("requires_global_name_qualification_on_type"));
  decl->set_name_qualification_length(
      static_cast<int>(json.requiredInt("name_qualification_length")));
  decl->set_type_elaboration_required(
      json.requiredBool("type_elaboration_required"));
  decl->set_global_qualification_required(
      json.requiredBool("global_qualification_required"));

  const JsonValue &variables = json.at("variables");
  if (variables.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration variables is not an array");
  }
  for (const JsonValue &variable : variables.array) {
    SgInitializedName *initialized_name = externalInitializedNameFromJson(
        variable, nodes, decl, scope,
        "external_function " + function_name + " variableDeclaration");
    decl->get_variables().push_back(initialized_name);
  }
  return decl;
}

SgRenamePair *externalRenamePairFromJson(const JsonValue &json,
                                         SgUseStatement *parent,
                                         const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgRenamePair") {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename_list entry has "
                             "unsupported kind: " +
                             kind);
  }
  SgRenamePair *rename =
      new SgRenamePair(SgName(json.at("local_name").asString()),
                       SgName(json.at("use_name").asString()));
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename pair has no location");
  }
  restoreOptionalNodeSourcePositionFromJson(
      rename, *location,
      "external_function " + function_name + " SgUseStatement rename pair");
  rename->set_parent(parent);
  return rename;
}

SgUseStatement *externalUseStatementFromJson(const JsonValue &json,
                                             const NodeMap &nodes,
                                             SgFunctionParameterScope *scope,
                                             const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgUseStatement") {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " functionParameterScope declaration kind is unsupported: " + kind);
  }
  SgUseStatement *stmt = new SgUseStatement(
      SgName(json.at("name").asString()), json.requiredBool("only_option"),
      json.requiredString("module_nature"));
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " use statement has no location");
  }
  restoreNodeSourcePositionFromJson(
      stmt, *location, "external_function useStatement " + function_name);
  stmt->set_parent(scope);
  stmt->set_scope(scope);
  restoreExternalDeclarationStatementFields(
      stmt, json, "external_function " + function_name + " useStatement");

  const uint64_t module_id = static_cast<uint64_t>(json.requiredInt("module"));
  if (module_id != 0) {
    stmt->set_module(nodeByIdAs<SgModuleStatement>(nodes, module_id));
  } else if (const JsonValue *external_module = json.find("external_module")) {
    stmt->set_module(externalModuleFromJson(*external_module));
  }

  const JsonValue &rename_list = json.at("rename_list");
  if (rename_list.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename_list is not an array");
  }
  for (const JsonValue &rename_json : rename_list.array) {
    stmt->get_rename_list().push_back(
        externalRenamePairFromJson(rename_json, stmt, function_name));
  }
  return stmt;
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

SgFunctionParameterScope *externalFunctionParameterScopeFromJson(
    const JsonValue &json, const NodeMap &nodes,
    const SgInitializedNamePtrList &parameters,
    const std::string &function_name) {
  if (!json.requiredBool("present")) {
    return nullptr;
  }
  SgFunctionParameterScope *scope = new SgFunctionParameterScope();
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " functionParameterScope has no location");
  }
  restoreNodeSourcePositionFromJson(
      scope, *location,
      "external_function functionParameterScope " + function_name);
  for (SgInitializedName *parameter : parameters) {
    if (parameter == nullptr || parameter->get_scope() != nullptr) {
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " has a null parameter or a parameter with a premature scope");
    }
    parameter->set_scope(scope);
  }

  std::vector<std::vector<SgInitializedName *>> variables_by_declaration;
  std::vector<SgDeclarationStatement *> declarations_by_index;
  const JsonValue &declarations = json.at("declarations");
  if (declarations.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " functionParameterScope declarations is not an "
                             "array");
  }
  for (const JsonValue &declaration : declarations.array) {
    SgDeclarationStatement *decl = externalParameterScopeDeclarationFromJson(
        declaration, nodes, scope, function_name);
    scope->append_declaration(decl);
    declarations_by_index.push_back(decl);
    std::vector<SgInitializedName *> names;
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
      for (SgInitializedName *name : variable->get_variables()) {
        names.push_back(name);
      }
    }
    variables_by_declaration.push_back(names);
  }

  if (json.requiredBool("symbol_table_present")) {
    SgSymbolTable *table = new SgSymbolTable(17);
    table->set_parent(scope);
    table->setCaseInsensitive(
        json.requiredBool("symbol_table_case_insensitive"));
    scope->set_symbol_table(table);
    const JsonValue *symbol_table = json.find("symbol_table");
    if (symbol_table == nullptr ||
        symbol_table->kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " functionParameterScope symbol_table is missing or not an array");
    }
    if (static_cast<int64_t>(symbol_table->array.size()) !=
        json.requiredInt("symbol_table_size")) {
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " functionParameterScope symbol_table size does not match metadata");
    }
    std::vector<const JsonValue *> insertion_entries;
    insertion_entries.reserve(symbol_table->array.size());
    for (const JsonValue &entry : symbol_table->array) {
      insertion_entries.push_back(&entry);
    }
    struct ExternalScopeSymbolPreference {
      SgName name;
      SgSymbol *symbol = nullptr;
      bool preferred = false;
    };
    std::vector<ExternalScopeSymbolPreference> symbol_preferences;
    auto insert_symbol = [&](const SgName &entry_name, SgSymbol *symbol,
                             const JsonValue &entry) {
      if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope attempted to insert a malformed symbol");
      }
      table->insert(entry_name, symbol);
      symbol_preferences.push_back(
          {entry_name, symbol, entry.requiredBool("lookup_preferred")});
    };
    for (const JsonValue *entry_ptr : insertion_entries) {
      const JsonValue &entry = *entry_ptr;
      const std::string symbol_kind = entry.at("symbol_kind").asString();
      const SgName entry_name(entry.at("entry_name").asString());
      if (const JsonValue *parameter_index_value =
              entry.find("parameter_index")) {
        const int64_t parameter_index = parameter_index_value->asInt();
        if (parameter_index < 0 ||
            static_cast<size_t>(parameter_index) >= parameters.size() ||
            symbol_kind != "SgVariableSymbol") {
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
        insert_symbol(entry_name, new SgVariableSymbol(parameter), entry);
        continue;
      }
      if (const JsonValue *declaration_index_value =
              entry.find("declaration_index")) {
        const size_t declaration_index =
            static_cast<size_t>(declaration_index_value->asInt());
        const size_t variable_index =
            static_cast<size_t>(entry.at("variable_index").asInt());
        if (declaration_index >= variables_by_declaration.size() ||
            variable_index >=
                variables_by_declaration[declaration_index].size()) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope symbol_table indexes are out of range");
        }
        if (symbol_kind != "SgVariableSymbol") {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope indexed symbol kind is unsupported: " +
              symbol_kind);
        }
        SgInitializedName *declaration =
            variables_by_declaration[declaration_index][variable_index];
        insert_symbol(entry_name, new SgVariableSymbol(declaration), entry);
        continue;
      }
      const JsonValue *symbol_json = entry.find("symbol");
      if (symbol_json == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol_table entry has neither nested "
            "variable indexes nor a structured symbol reference: " +
            entry_name.getString());
      }
      const std::string referenced_kind =
          symbol_json->at("symbol_kind").asString();
      if (referenced_kind != symbol_kind) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol_table kind mismatch for " +
            entry_name.getString() + ": entry=" + symbol_kind +
            " symbol=" + referenced_kind);
      }
      if (symbol_kind == "SgAliasSymbol") {
        const JsonValue *target = entry.find("alias_target");
        if (target == nullptr) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope SgAliasSymbol has no alias_target: " +
              entry_name.getString());
        }
        SgSymbol *target_symbol = symbolFromJson(*target, nodes);
        if (target_symbol == nullptr ||
            target_symbol->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope cannot reconstruct alias target for " +
              entry_name.getString());
        }
        SgAliasSymbol *alias = new SgAliasSymbol(
            target_symbol, entry.requiredBool("alias_is_renamed"),
            SgName(entry.requiredString("alias_new_name")));
        const JsonValue &causal_nodes = entry.at("alias_causal_nodes");
        if (causal_nodes.kind != JsonValue::Kind::Array ||
            causal_nodes.array.empty()) {
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
              static_cast<uint64_t>(causal_node.requiredInt("node"));
          const int64_t declaration_index =
              causal_node.requiredInt("declaration_index");
          if (node_id != 0) {
            alias->get_causal_nodes().push_back(nodeById(nodes, node_id));
          } else if (declaration_index >= 0 &&
                     static_cast<size_t>(declaration_index) <
                         declarations_by_index.size()) {
            alias->get_causal_nodes().push_back(
                declarations_by_index[static_cast<size_t>(declaration_index)]);
          } else {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope alias causal node cannot be "
                "resolved");
          }
        }
        validateExternalSymbolBasisOwnership(alias);
        insert_symbol(entry_name, alias, entry);
        continue;
      }
      if (symbol_kind == "SgRenameSymbol") {
        const JsonValue *original = entry.find("original_symbol");
        if (original == nullptr) {
          throw std::runtime_error("AST JSON external_function " +
                                   function_name +
                                   " functionParameterScope SgRenameSymbol has "
                                   "no original_symbol: " +
                                   entry_name.getString());
        }
        const uint64_t basis_id = static_cast<uint64_t>(
            symbol_json->at("symbol_declaration").asInt());
        if (basis_id == 0) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope SgRenameSymbol has no collected "
              "function declaration basis: " +
              entry_name.getString());
        }
        SgRenameSymbol *rename = new SgRenameSymbol(
            nodeByIdAs<SgFunctionDeclaration>(nodes, basis_id),
            symbolFromJson(*original, nodes),
            SgName(entry.requiredString("rename_new_name")));
        insert_symbol(entry_name, rename, entry);
        continue;
      }
      const uint64_t basis_id =
          static_cast<uint64_t>(symbol_json->at("symbol_declaration").asInt());
      SgSymbol *symbol = nullptr;
      if (basis_id == 0) {
        symbol = createExternalSymbolFromJson(*symbol_json, nodes);
      } else {
        SgNode *basis = nodeById(nodes, basis_id);
        symbol = createSymbolForKindAndBasis(symbol_kind, basis);
        if (SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
          restoreLabelSymbolFields(label_symbol, *symbol_json);
        }
      }
      if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope cannot reconstruct symbol_table entry " +
            entry_name.getString() + " kind=" + symbol_kind);
      }
      validateExternalSymbolBasisOwnership(symbol);
      insert_symbol(entry_name, symbol, entry);
    }

    auto preference_category = [](SgSymbol *symbol) -> std::string {
      if (isSgVariableSymbol(symbol) != nullptr)
        return "variable";
      if (isSgClassSymbol(symbol) != nullptr)
        return "class";
      if (isSgEnumSymbol(symbol) != nullptr)
        return "enum";
      if (isSgEnumFieldSymbol(symbol) != nullptr)
        return "enum_field";
      if (isSgTypedefSymbol(symbol) != nullptr)
        return "typedef";
      if (isSgLabelSymbol(symbol) != nullptr)
        return "label";
      if (isSgNamespaceSymbol(symbol) != nullptr)
        return "namespace";
      if (isSgFunctionSymbol(symbol) != nullptr)
        return "function";
      return "";
    };
    using PreferenceKey = std::pair<std::string, std::string>;
    std::map<PreferenceKey, std::vector<ExternalScopeSymbolPreference *>>
        preference_groups;
    for (ExternalScopeSymbolPreference &entry : symbol_preferences) {
      const std::string category = preference_category(entry.symbol);
      if (!category.empty()) {
        preference_groups[{entry.name.getString(), category}].push_back(&entry);
      }
    }
    for (const auto &[key, group] : preference_groups) {
      if (group.size() < 2) {
        if (!group.empty() &&
            !symbolIsLookupPreferred(table, group.front()->name,
                                     group.front()->symbol)) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope single symbol is not lookup preferred");
        }
        continue;
      }
      const size_t preferred_count = static_cast<size_t>(
          std::count_if(group.begin(), group.end(),
                        [](const ExternalScopeSymbolPreference *entry) {
                          return entry->preferred;
                        }));
      if (preferred_count != 1) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol preference group " + key.first +
            "/" + key.second + " does not identify one exact preferred entry");
      }
      for (ExternalScopeSymbolPreference *entry : group) {
        if (!table->find(entry->name, entry->symbol)) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope cannot reorder a missing symbol");
        }
        table->remove(entry->symbol);
      }
      for (ExternalScopeSymbolPreference *entry : group) {
        if (!entry->preferred) {
          table->insert(entry->name, entry->symbol);
        }
      }
      for (ExternalScopeSymbolPreference *entry : group) {
        if (entry->preferred) {
          table->insert(entry->name, entry->symbol);
        }
      }
      for (ExternalScopeSymbolPreference *entry : group) {
        if (symbolIsLookupPreferred(table, entry->name, entry->symbol) !=
            entry->preferred) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope failed exact symbol preference "
              "reconstruction");
        }
      }
    }
  }
  return scope;
}

SgClassDeclaration *newClassDeclarationForExternalRecord(
    const std::string &kind, const std::string &name,
    SgClassDeclaration::class_types class_type) {
  if (kind == "SgDerivedTypeStatement") {
    return new SgDerivedTypeStatement(SgName(name), class_type, nullptr,
                                      nullptr);
  }
  if (kind == "SgModuleStatement") {
    return new SgModuleStatement(SgName(name), class_type, nullptr, nullptr);
  }
  if (kind == "SgClassDeclaration") {
    return new SgClassDeclaration(SgName(name), class_type, nullptr, nullptr);
  }
  throw std::runtime_error(
      "AST JSON external class declaration has unsupported declaration kind: " +
      kind);
}

SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json) {
  if (!json.requiredBool("present")) {
    return nullptr;
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
  if (kind.empty() || name.empty() || source_file.empty()) {
    throw std::runtime_error("AST JSON external class declaration requires "
                             "kind, name, and source_file");
  }

  const uint64_t identity_hash = exactJsonValueHash(json);
  std::vector<ExternalClassDeserializationIdentity> &identity_bucket =
      externalClassDeserializationIdentities[identity_hash];
  for (const ExternalClassDeserializationIdentity &identity : identity_bucket) {
    if (identity.serialized_declaration == nullptr ||
        identity.declaration == nullptr) {
      throw std::runtime_error(
          "AST JSON external class identity cache is malformed");
    }
    if (identity.serialized_declaration->exactlyEquals(json)) {
      return identity.declaration;
    }
  }
  validateExternalClassDeclarationInProject(json);

  const auto class_type = requiredClassType(json, "external class declaration");
  const std::string module_name = json.at("module_name").asString();
  const bool has_definition = json.at("has_definition").asBool();
  const bool is_first_nondefining = json.at("is_first_nondefining").asBool();
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
  if (kind == "SgModuleStatement") {
    if (const ExternalClassModuleIdentity *identity =
            find_external_module(name)) {
      if (class_type != SgClassDeclaration::e_fortran_module ||
          !has_definition || !is_first_nondefining ||
          identity->declaration == nullptr || identity->definition == nullptr ||
          identity->declaration->get_definition() != identity->definition ||
          identity->definition->get_declaration() != identity->declaration) {
        throw std::runtime_error(
            "AST JSON external module declaration conflicts with its "
            "canonical reconstructed module");
      }
      identity_bucket.push_back({&json, identity->declaration});
      return identity->declaration;
    }
  }

  SgModuleStatement *module = nullptr;
  SgClassDefinition *module_definition = nullptr;
  SgGlobal *external_global = nullptr;
  if (!module_name.empty() && kind != "SgModuleStatement") {
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
      module_definition = new SgClassDefinition();
      installTransformationSourcePosition(module_definition);
      module->set_definition(module_definition);
      module_definition->set_declaration(module);
      module_definition->set_parent(module);
      module->set_firstNondefiningDeclaration(module);
      module->set_definingDeclaration(module);
      module->set_parent(external_global);
      module->set_scope(external_global);
      external_global->get_declarations().push_back(module);
      ensureClassTypeForDeclaration(module);
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

  SgClassDeclaration *decl =
      newClassDeclarationForExternalRecord(kind, name, class_type);
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
  if (has_definition) {
    SgClassDefinition *definition = new SgClassDefinition();
    installTransformationSourcePosition(definition);
    definition->set_declaration(decl);
    definition->set_parent(decl);
    decl->set_definition(definition);
    if (!is_first_nondefining) {
      first_nondefining =
          newClassDeclarationForExternalRecord(kind, name, class_type);
      installTransformationSourcePosition(first_nondefining);
      first_nondefining->set_parent(declaration_scope);
      first_nondefining->set_scope(declaration_scope);
      markAstJsonExternalClassDeclaration(first_nondefining, source_file);
      first_nondefining->set_firstNondefiningDeclaration(first_nondefining);
      first_nondefining->set_definingDeclaration(decl);
    }
  }

  decl->set_firstNondefiningDeclaration(first_nondefining);
  decl->set_definingDeclaration(defining);

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
  ensureClassTypeForDeclaration(first_nondefining);
  if (SgModuleStatement *external_module =
          isSgModuleStatement(first_nondefining)) {
    SgClassDefinition *definition = external_module->get_definition();
    if (definition == nullptr ||
        definition->get_declaration() != external_module) {
      throw std::runtime_error(
          "AST JSON external module class record has no exact definition");
    }
    externalClassModuleIdentities.push_back(
        {external_module->get_name().getString(), source_file, external_module,
         definition});
  }

  identity_bucket.push_back({&json, decl});

  return decl;
}

void restoreClassTypeAutonomousDeclaration(SgClassType *class_type,
                                           bool autonomous) {
  if (!externalClassDeserializationIdentityActive || class_type == nullptr) {
    throw std::runtime_error(
        "AST JSON class type metadata was restored outside its exact identity "
        "transaction");
  }
  auto inserted =
      classTypeAutonomousDeclarationIdentities.emplace(class_type, autonomous);
  if (!inserted.second && inserted.first->second != autonomous) {
    throw std::runtime_error(
        "AST JSON repeated class type identity has contradictory autonomous "
        "declaration state");
  }
  class_type->set_autonomous_declaration(autonomous);
}

SgType *componentTypeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind == JsonValue::Kind::Object && json.requiredBool("present")) {
    const std::string kind = json.requiredString("kind");
    if (kind == "SgFunctionType" || kind == "SgMemberFunctionType") {
      return semanticFunctionTypeFromJson(json, nodes);
    }
  }
  return typeFromJson(json, nodes);
}

SgType *typeFromJsonUncached(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON type record is not an object");
  }
  if (!json.requiredBool("present")) {
    throw std::runtime_error("AST JSON required type record is absent");
  }

  const std::string kind = json.requiredString("kind");
  const bool fortran_source_syntax = json.requiredBool("fortran_source_syntax");
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
  if (kind == "SgTypeDefault") {
    return SgTypeDefault::createType();
  }
  if (kind == "SgTypeTargetBuiltin") {
    return targetBuiltinTypeFromJson(json);
  }
  if (kind == "SgTypeFortranAssumed") {
    if (fortran_source_syntax) {
      SgTypeFortranAssumed *restored = new SgTypeFortranAssumed();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeFortranAssumed::createType();
  }
  if (kind == "SgTypeFortranUnlimitedPolymorphic") {
    if (fortran_source_syntax) {
      SgTypeFortranUnlimitedPolymorphic *restored =
          new SgTypeFortranUnlimitedPolymorphic();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeFortranUnlimitedPolymorphic::createType();
  }
  if (kind == "SgTypeCrayPointer") {
    if (fortran_source_syntax) {
      SgTypeCrayPointer *restored = new SgTypeCrayPointer();
      restored->set_fortran_source_syntax(true);
      return restored;
    }
    return SgTypeCrayPointer::createType();
  }
  if (kind == "SgTypeUnknown") {
    return SageBuilder::buildUnknownType();
  }
  if (kind == "SgTypeEllipse") {
    return SgTypeEllipse::createType();
  }
  if (kind == "SgTypeNullptr") {
    return SageBuilder::buildNullptrType();
  }
  if (kind == "SgTypeLabel") {
    return SgTypeLabel::createType(SgName(json.requiredString("name")));
  }
  if (kind == "SgTypeFloat128") {
    return SageBuilder::buildFloat128Type();
  }
  if (kind == "SgTypeSigned128bitInteger") {
    return SageBuilder::buildSigned128bitIntegerType();
  }
  if (kind == "SgTypeUnsigned128bitInteger") {
    return SageBuilder::buildUnsigned128bitIntegerType();
  }
  if (kind == "SgAutoType") {
    return autoTypeFromJson(json);
  }
  if (SgType *primitive = primitiveTypeFromKind(kind)) {
    if (isSgTypeChar(primitive) != nullptr && fortran_source_syntax) {
      SgTypeChar *source_type = new SgTypeChar();
      source_type->set_fortran_source_syntax(true);
      return source_type;
    }
    const bool hasFortranKind = isSgTypeBool(primitive) != nullptr ||
                                isSgTypeInt(primitive) != nullptr ||
                                isSgTypeUnsignedInt(primitive) != nullptr ||
                                isSgTypeFloat(primitive) != nullptr;
    if (!hasFortranKind) {
      if (isSgTypeDouble(primitive) != nullptr) {
        const bool fixed_kind_available =
            json.requiredBool("fortran_fixed_kind_value_is_available");
        const std::int64_t fixed_kind =
            json.requiredInt("fortran_fixed_kind_value");
        if ((!fixed_kind_available && fixed_kind != 0) ||
            (fixed_kind_available && fixed_kind <= 0)) {
          throw std::runtime_error(
              "AST JSON SgTypeDouble has invalid fixed Fortran KIND "
              "metadata");
        }
        if (fixed_kind_available != fortran_source_syntax) {
          throw std::runtime_error(
              "AST JSON SgTypeDouble source-syntax and fixed KIND metadata "
              "differ");
        }
        if (fortran_source_syntax) {
          SgTypeDouble *source_type = new SgTypeDouble();
          source_type->set_fortran_fixed_kind_value(fixed_kind);
          source_type->set_fortran_fixed_kind_value_is_available(true);
          source_type->set_fortran_source_syntax(true);
          return source_type;
        }
      } else if (fortran_source_syntax) {
        throw std::runtime_error(
            "AST JSON non-Fortran primitive type is marked as source syntax");
      }
      return primitive;
    }

    SgExpression *kind_expression =
        expressionFromRef(json.at("type_kind"), nodes);
    SgType *restored = nullptr;
    const bool selector_metadata_available =
        kind_expression != nullptr &&
        kind_expression->get_fortran_integer_constant_value_is_available();
    if (selector_metadata_available && !fortran_source_syntax) {
      throw std::runtime_error(
          "AST JSON semantic intrinsic type owns source selector metadata");
    }
    if (fortran_source_syntax && kind_expression != nullptr &&
        !selector_metadata_available) {
      throw std::runtime_error(
          "AST JSON source intrinsic KIND selector has no folded value");
    }
    if (fortran_source_syntax) {
      if (isSgTypeBool(primitive) != nullptr) {
        restored = new SgTypeBool();
      } else if (isSgTypeInt(primitive) != nullptr) {
        restored = new SgTypeInt();
      } else if (isSgTypeUnsignedInt(primitive) != nullptr) {
        restored = new SgTypeUnsignedInt();
      } else if (isSgTypeFloat(primitive) != nullptr) {
        restored = new SgTypeFloat();
      }
      restored->set_type_kind(kind_expression);
      if (kind_expression != nullptr) {
        kind_expression->set_parent(restored);
      }
    } else if (isSgTypeBool(primitive) != nullptr) {
      restored = SageBuilder::buildBoolType(kind_expression);
    } else if (isSgTypeInt(primitive) != nullptr) {
      restored = SageBuilder::buildIntType(kind_expression);
    } else if (isSgTypeUnsignedInt(primitive) != nullptr) {
      restored = SageBuilder::buildUnsignedIntType(kind_expression);
    } else if (isSgTypeFloat(primitive) != nullptr) {
      restored = SageBuilder::buildFloatType(kind_expression);
    }
    if (restored == nullptr) {
      throw std::runtime_error("AST JSON failed to restore Fortran intrinsic "
                               "kind for " +
                               kind);
    }
    const bool has_type_kind_star = json.requiredBool("has_type_kind_star");
    if (has_type_kind_star && !fortran_source_syntax) {
      throw std::runtime_error(
          "AST JSON semantic intrinsic type has source-only star KIND");
    }
    if (fortran_source_syntax) {
      restored->set_hasTypeKindStar(has_type_kind_star);
    } else if (restored->get_hasTypeKindStar()) {
      throw std::runtime_error(
          "AST JSON canonical semantic intrinsic has source-only star KIND");
    }
    restored->set_fortran_source_syntax(fortran_source_syntax);
    return restored;
  }
  if (kind == "SgTypeString") {
    SgExpression *length = nullptr;
    if (const JsonValue *length_json = json.find("length_expression")) {
      length = expressionFromRef(*length_json, nodes);
    }
    SgExpression *type_kind = nullptr;
    if (const JsonValue *kind_json = json.find("type_kind")) {
      type_kind = expressionFromRef(*kind_json, nodes);
    }
    SgTypeString *string_type = new SgTypeString(length);
    string_type->set_type_kind(type_kind);
    string_type->set_hasTypeKindStar(json.requiredBool("has_type_kind_star"));
    string_type->set_fortran_source_syntax(fortran_source_syntax);
    string_type->set_fortran_dynamic_result_length(
        json.requiredBool("fortran_dynamic_result_length"));
    if (string_type->get_fortran_dynamic_length_pending()) {
      throw std::runtime_error(
          "AST JSON CHARACTER type has an unresolved semantic length");
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
    if (length != nullptr) {
      length->set_parent(string_type);
    }
    if (type_kind != nullptr) {
      type_kind->set_parent(string_type);
    }
    if (string_type->get_fortran_dynamic_result_length() &&
        (fortran_source_syntax || length != nullptr ||
         string_type->get_hasTypeKindStar())) {
      throw std::runtime_error(
          "AST JSON dynamic CHARACTER result type has contradictory source "
          "or selector state");
    }
    const bool dependent_semantic_length =
        !fortran_source_syntax && length != nullptr &&
        isSgValueExp(length) == nullptr &&
        isSgAsteriskShapeExp(length) == nullptr &&
        isSgColonShapeExp(length) == nullptr;
    if (!fortran_source_syntax &&
        !string_type->get_fortran_dynamic_result_length() &&
        !dependent_semantic_length) {
      const SgName mangled = string_type->get_mangled();
      if (string_type->get_hasTypeKindStar()) {
        throw std::runtime_error(
            "AST JSON semantic CHARACTER type has a source-only star KIND");
      }
      string_type->set_lengthExpression(nullptr);
      string_type->set_type_kind(nullptr);
      if (length != nullptr) {
        length->set_parent(nullptr);
      }
      if (type_kind != nullptr) {
        type_kind->set_parent(nullptr);
      }
      SageInterface::deleteAST(
          string_type, SageInterface::DeleteAstMode::kSkipExternalReferences);
      string_type = SgTypeString::createType(length, type_kind);
      if (string_type == nullptr || string_type->get_mangled() != mangled ||
          string_type->get_fortran_source_syntax() ||
          string_type->get_hasTypeKindStar() ||
          string_type->get_fortran_dynamic_length_pending() ||
          string_type->get_fortran_dynamic_result_length() ||
          string_type->get_fortran_fixed_kind_value_is_available() ||
          string_type->get_fortran_fixed_kind_value() != 0 ||
          (string_type->get_lengthExpression() != nullptr &&
           string_type->get_lengthExpression()->get_parent() != string_type) ||
          (string_type->get_type_kind() != nullptr &&
           string_type->get_type_kind()->get_parent() != string_type)) {
        throw std::runtime_error(
            "AST JSON failed to restore canonical semantic CHARACTER type");
      }
    } else if (dependent_semantic_length &&
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
    return string_type;
  }
  if (kind == "SgTypeComplex") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgExpression *type_kind = base->get_type_kind();
    const bool fixed_kind_available =
        json.requiredBool("fortran_fixed_kind_value_is_available");
    const std::int64_t fixed_kind =
        json.requiredInt("fortran_fixed_kind_value");
    if ((!fixed_kind_available && fixed_kind != 0) ||
        (fixed_kind_available && fixed_kind <= 0)) {
      throw std::runtime_error(
          "AST JSON SgTypeComplex has invalid fixed Fortran KIND metadata");
    }
    const bool source_selector =
        type_kind != nullptr &&
        type_kind->get_fortran_integer_constant_value_is_available();
    if ((source_selector || fixed_kind_available) && !fortran_source_syntax) {
      throw std::runtime_error(
          "AST JSON semantic COMPLEX type owns source syntax metadata");
    }
    if (fortran_source_syntax && type_kind != nullptr && !source_selector) {
      throw std::runtime_error(
          "AST JSON source COMPLEX KIND selector has no folded value");
    }
    if (fortran_source_syntax && !base->get_fortran_source_syntax()) {
      throw std::runtime_error(
          "AST JSON source COMPLEX type has a semantic component type");
    }
    if (type_kind != nullptr && type_kind->get_parent() != base) {
      throw std::runtime_error(
          "AST JSON COMPLEX component does not solely own its KIND selector");
    }
    if (fortran_source_syntax && fixed_kind_available &&
        (isSgTypeDouble(base) == nullptr ||
         !base->get_fortran_fixed_kind_value_is_available() ||
         base->get_fortran_fixed_kind_value() != fixed_kind)) {
      throw std::runtime_error(
          "AST JSON fixed source COMPLEX type has a contradictory component "
          "KIND");
    }
    const bool has_type_kind_star = json.requiredBool("has_type_kind_star");
    if (has_type_kind_star && !fortran_source_syntax) {
      throw std::runtime_error(
          "AST JSON semantic COMPLEX type has source-only star KIND");
    }
    SgTypeComplex *complex_type = fortran_source_syntax
                                      ? new SgTypeComplex(base)
                                      : SgTypeComplex::createType(base);
    if (fortran_source_syntax) {
      complex_type->set_hasTypeKindStar(has_type_kind_star);
    } else if (complex_type->get_hasTypeKindStar()) {
      throw std::runtime_error(
          "AST JSON canonical semantic COMPLEX has source-only star KIND");
    }
    complex_type->set_fortran_fixed_kind_value(fixed_kind);
    complex_type->set_fortran_fixed_kind_value_is_available(
        fixed_kind_available);
    complex_type->set_fortran_source_syntax(fortran_source_syntax);
    return complex_type;
  }
  if (kind == "SgPointerType") {
    SgType *base = componentTypeFromJson(json.at("base"), nodes);
    if (fortran_source_syntax) {
      if (!base->get_fortran_source_syntax() &&
          isSgNamedType(base) == nullptr) {
        throw std::runtime_error(
            "AST JSON source pointer has a semantic base type");
      }
      SgPointerType *pointer = new SgPointerType(base);
      pointer->set_fortran_source_syntax(true);
      return pointer;
    }
    return buildCachedJsonPointerType(base);
  }
  if (kind == "SgPointerMemberType") {
    if (SgPointerMemberType *restored =
            restoredPointerMemberTypeIdentity(json)) {
      return restored;
    }
    SgType *base = componentTypeFromJson(json.at("base"), nodes);
    SgType *class_type = typeFromJson(json.at("class_type"), nodes);
    return restorePointerMemberTypeIdentity(base, class_type, json, true);
  }
  if (kind == "SgReferenceType") {
    SgType *base = componentTypeFromJson(json.at("base"), nodes);
    SgReferenceType *reference = new SgReferenceType(base);
    installReferenceCache(base, reference);
    return reference;
  }
  if (kind == "SgRvalueReferenceType") {
    SgType *base = componentTypeFromJson(json.at("base"), nodes);
    SgRvalueReferenceType *reference = new SgRvalueReferenceType(base);
    installRvalueReferenceCache(base, reference);
    return reference;
  }
  if (kind == "SgArrayType") {
    SgExpression *index = nullptr;
    if (const JsonValue *index_json = json.find("index")) {
      index = expressionFromRef(*index_json, nodes);
    }
    SgType *base = typeFromJson(json.at("base"), nodes);
    if (fortran_source_syntax && !base->get_fortran_source_syntax() &&
        isSgNamedType(base) == nullptr) {
      throw std::runtime_error(
          "AST JSON source array has a semantic base type");
    }
    SgArrayType *array = new SgArrayType(base, index);
    if (index != nullptr && array->get_index() != index) {
      array->set_index(index);
    }
    if (index != nullptr) {
      index->set_parent(array);
    }
    if (const JsonValue *dim_info = json.find("dim_info")) {
      SgExprListExp *dimensions = exprListExpFromTypeJson(*dim_info, nodes);
      array->set_dim_info(dimensions);
      if (dimensions != nullptr) {
        dimensions->set_parent(array);
      }
    }
    array->set_rank(static_cast<int>(json.requiredInt("rank")));
    array->set_number_of_elements(
        static_cast<int>(json.requiredInt("number_of_elements")));
    array->set_isCoArray(json.requiredBool("is_coarray"));
    array->set_is_variable_length_array(
        json.requiredBool("is_variable_length_array"));
    array->set_fortran_source_syntax(fortran_source_syntax);
    const SgName mangled = array->get_mangled();
    if (!fortran_source_syntax) {
      SgTypeTable *type_table = SgNode::get_globalTypeTable();
      ROSE_ASSERT(type_table != nullptr);
      if (type_table->lookup_type(mangled) != nullptr) {
        type_table->remove_type(mangled);
      }
      type_table->insert_type(mangled, array);
    }
    return array;
  }
  if (kind == "SgModifierType") {
    SgType *base = componentTypeFromJson(json.at("base"), nodes);
    if (fortran_source_syntax && !base->get_fortran_source_syntax() &&
        isSgNamedType(base) == nullptr) {
      throw std::runtime_error(
          "AST JSON source modifier has a semantic base type");
    }
    SgModifierType *modifier = new SgModifierType(base);
    SgTypeModifier &type_modifier = modifier->get_typeModifier();
    SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
    if (json.requiredBool("modifier_const")) {
      cv.setConst();
    }
    if (json.requiredBool("modifier_volatile")) {
      cv.setVolatile();
    }
    if (json.requiredBool("modifier_restrict")) {
      type_modifier.setRestrict();
    }
    modifier->set_fortran_source_syntax(fortran_source_syntax);
    return modifier;
  }
  if (kind == "SgTypedefType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.requiredInt("declaration"));
    if (decl_id != 0) {
      if (SgTypedefDeclaration *decl =
              isSgTypedefDeclaration(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          decl->set_type(new SgTypedefType(decl, nullptr));
        }
        decl->get_type()->set_autonomous_declaration(
            json.requiredBool("autonomous_declaration"));
        return decl->get_type();
      }
    }
    throw std::runtime_error(
        "AST JSON SgTypedefType requires a valid declaration id");
  }
  if (kind == "SgClassType") {
    const JsonValue *declaration_json = json.find("declaration");
    const JsonValue *external_declaration_json =
        json.find("external_declaration");
    if ((declaration_json == nullptr) ==
        (external_declaration_json == nullptr)) {
      throw std::runtime_error(
          "AST JSON SgClassType requires exactly one declaration identity");
    }

    const uint64_t decl_id =
        declaration_json != nullptr
            ? static_cast<uint64_t>(declaration_json->asInt())
            : 0;
    if (decl_id != 0) {
      if (SgClassDeclaration *decl =
              isSgClassDeclaration(nodeById(nodes, decl_id))) {
        SgClassType *class_type = ensureClassTypeForDeclaration(decl);
        restoreClassTypeAutonomousDeclaration(
            class_type, json.requiredBool("autonomous_declaration"));
        return class_type;
      }
      throw std::runtime_error(
          "AST JSON SgClassType declaration id is not a class declaration");
    }
    if (external_declaration_json != nullptr) {
      SgClassDeclaration *decl =
          externalClassDeclarationFromJson(*external_declaration_json);
      SgClassType *class_type = ensureClassTypeForDeclaration(decl);
      restoreClassTypeAutonomousDeclaration(
          class_type, json.requiredBool("autonomous_declaration"));
      return class_type;
    }
    throw std::runtime_error("AST JSON SgClassType declaration id is zero");
  }
  if (kind == "SgEnumType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.requiredInt("declaration"));
    if (decl_id != 0) {
      if (SgEnumDeclaration *decl =
              isSgEnumDeclaration(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          if (decl->get_scope() == nullptr) {
            throw std::runtime_error(
                "AST JSON SgEnumType declaration has no exact scope");
          }
          decl->set_type(SgEnumType::createType(decl));
        }
        decl->get_type()->set_autonomous_declaration(
            json.requiredBool("autonomous_declaration"));
        return decl->get_type();
      }
    }
    throw std::runtime_error(
        "AST JSON SgEnumType requires a valid declaration id");
  }
  if (kind == "SgNonrealType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.requiredInt("declaration"));
    if (decl_id != 0) {
      if (SgNonrealDecl *decl = isSgNonrealDecl(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          decl->set_type(new SgNonrealType(decl));
        }
        decl->get_type()->set_autonomous_declaration(
            json.requiredBool("autonomous_declaration"));
        return decl->get_type();
      }
    }
    throw std::runtime_error(
        "AST JSON SgNonrealType requires a valid declaration id");
  }
  if (kind == "SgDeclType") {
    SgExpression *base_expression = nullptr;
    if (const JsonValue *base_json = json.find("base_expression")) {
      base_expression = expressionFromRef(*base_json, nodes);
    }
    SgType *base_type = componentTypeFromJson(json.at("base_type"), nodes);
    return buildDeclType(base_expression, base_type, json);
  }
  if (kind == "SgTypeOfType") {
    SgExpression *base_expression =
        expressionFromRef(json.at("base_expression"), nodes);
    SgType *base_type = componentTypeFromJson(json.at("base_type"), nodes);
    if (base_type == nullptr || isSgTypeUnknown(base_type) != nullptr) {
      throw std::runtime_error(
          "AST JSON SgTypeOfType requires one exact base type");
    }

    SgTypeOfType *typeof_type = nullptr;
    if (base_expression == nullptr) {
      typeof_type = new SgTypeOfType(nullptr, base_type);
    } else {
      typeof_type = new SgTypeOfType(base_expression, nullptr);
      base_expression->set_parent(typeof_type);
      if (isSgFunctionParameterRefExp(base_expression) != nullptr) {
        typeof_type->set_base_type(base_type);
      } else if (base_expression->get_type() == nullptr ||
                 !SageInterface::isEquivalentType(base_expression->get_type(),
                                                  base_type)) {
        throw std::runtime_error(
            "AST JSON SgTypeOfType expression and base type disagree");
      }
    }
    if (typeof_type->get_base_type() == nullptr ||
        (base_expression != nullptr &&
         base_expression->get_parent() != typeof_type)) {
      throw std::runtime_error(
          "AST JSON failed to restore exact SgTypeOfType operand ownership");
    }
    return typeof_type;
  }
  if (kind == "SgTemplateType") {
    const SgName name(json.requiredString("name"));
    const int position =
        static_cast<int>(json.requiredInt("template_parameter_position"));
    const int depth =
        static_cast<int>(json.requiredInt("template_parameter_depth"));
    const bool packed = json.requiredBool("packed");
    const uint64_t parameter_id =
        static_cast<uint64_t>(json.requiredInt("template_parameter"));
    SgTemplateParameter *parameter =
        parameter_id != 0 ? nodeByIdAs<SgTemplateParameter>(nodes, parameter_id)
                          : nullptr;
    if (parameter_id != 0 && parameter == nullptr) {
      throw std::runtime_error(
          "AST JSON SgTemplateType has an invalid template parameter id");
    }

    if (parameter != nullptr && parameter->get_type() != nullptr) {
      SgTemplateType *canonical_type = isSgTemplateType(parameter->get_type());
      if (canonical_type == nullptr || canonical_type->get_name() != name ||
          canonical_type->get_template_parameter_position() != position ||
          canonical_type->get_template_parameter_depth() != depth ||
          canonical_type->get_template_parameter() != parameter) {
        throw std::runtime_error(
            "AST JSON SgTemplateType disagrees with its exact template "
            "parameter coordinates");
      }
      if (canonical_type->get_packed() == packed &&
          !json.at("class_type").requiredBool("present") &&
          !json.at("parent_class_type").requiredBool("present")) {
        return canonical_type;
      }
    }

    SgTemplateType *template_type = new SgTemplateType(name);
    template_type->set_template_parameter_position(position);
    template_type->set_template_parameter_depth(depth);
    restoreTemplateTypeCanonicalSourceIdentity(template_type, json);
    template_type->set_packed(packed);
    template_type->set_template_parameter(parameter);
    if (const JsonValue *class_json = json.find("class_type")) {
      template_type->set_class_type(class_json->requiredBool("present")
                                        ? typeFromJson(*class_json, nodes)
                                        : nullptr);
    } else {
      template_type->set_class_type(nullptr);
    }
    if (const JsonValue *parent_class_json = json.find("parent_class_type")) {
      template_type->set_parent_class_type(
          parent_class_json->requiredBool("present")
              ? typeFromJson(*parent_class_json, nodes)
              : nullptr);
    } else {
      template_type->set_parent_class_type(nullptr);
    }
    if (parameter != nullptr && parameter->get_type() == nullptr) {
      parameter->set_type(template_type);
    }
    if (parameter != nullptr &&
        (template_type->get_name() != name ||
         template_type->get_template_parameter_position() != position ||
         template_type->get_template_parameter_depth() != depth ||
         template_type->get_packed() != packed ||
         template_type->get_template_parameter() != parameter)) {
      throw std::runtime_error(
          "AST JSON SgTemplateType failed exact use-site reconstruction");
    }
    return template_type;
  }
  if (kind == "SgMemberFunctionType") {
    SgType *return_type = typeFromJson(json.at("return_type"), nodes);
    SgType *class_type = typeFromJson(json.at("class_type"), nodes);
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    const JsonValue &argument_json = json.at("arguments");
    if (argument_json.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON member function type arguments field is not an array");
    }
    for (const JsonValue &argument : argument_json.array) {
      arguments->append_argument(typeFromJson(argument, nodes));
    }
    SgMemberFunctionType *member_type = new SgMemberFunctionType(
        return_type, json.requiredBool("has_ellipses"), class_type,
        static_cast<unsigned int>(json.requiredInt("mfunc_specifier")));
    member_type->set_argument_list(arguments);
    arguments->set_parent(member_type);
    return member_type;
  }
  if (kind == "SgFunctionType") {
    SgType *return_type = typeFromJson(json.at("return_type"), nodes);
    if (fortran_source_syntax && !return_type->get_fortran_source_syntax() &&
        isSgNamedType(return_type) == nullptr) {
      throw std::runtime_error(
          "AST JSON source function type has a semantic return type");
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    const JsonValue &argument_json = json.at("arguments");
    if (argument_json.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON function type arguments field is not an array");
    }
    for (const JsonValue &argument : argument_json.array) {
      arguments->append_argument(typeFromJson(argument, nodes));
    }
    SgFunctionType *function_type =
        new SgFunctionType(return_type, json.requiredBool("has_ellipses"));
    SgFunctionParameterTypeList *default_arguments =
        function_type->get_argument_list();
    if (default_arguments == nullptr ||
        default_arguments->get_parent() != function_type ||
        arguments->get_parent() != nullptr) {
      throw std::runtime_error(
          "AST JSON function type has malformed argument-list ownership");
    }
    function_type->set_argument_list(arguments);
    arguments->set_parent(function_type);
    default_arguments->set_parent(nullptr);
    SageInterface::deleteAST(
        default_arguments,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
    function_type->set_fortran_source_syntax(fortran_source_syntax);
    return function_type;
  }
  throw std::runtime_error("AST JSON deserializer does not support Sage type " +
                           kind);
}

SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON type record is not an object");
  }
  if (!json.requiredBool("present")) {
    throw std::runtime_error("AST JSON required type record is absent");
  }
  const bool source_syntax = json.requiredBool("fortran_source_syntax");
  const std::string kind = json.requiredString("kind");
  const bool array_type = kind == "SgArrayType";
  const JsonValue *identity_json = json.find("semantic_array_identity");
  if (source_syntax) {
    if (identity_json != nullptr) {
      throw std::runtime_error(
          "AST JSON source array owns a semantic array identity");
    }
    return typeFromJsonUncached(json, nodes);
  }
  if (kind == "SgTemplateType") {
    const JsonValue &source_identity = json.at("canonical_source_identity");
    if (source_identity.kind == JsonValue::Kind::Null) {
      return typeFromJsonUncached(json, nodes);
    }
    if (!templateTypeDeserializationIdentityActive) {
      throw std::runtime_error(
          "AST JSON canonical template type has no active exact identity "
          "transaction");
    }
    const uint64_t identity_hash = exactJsonValueHash(source_identity);
    std::vector<TemplateTypeDeserializationIdentity> &bucket =
        templateTypeDeserializationIdentities[identity_hash];
    for (const TemplateTypeDeserializationIdentity &identity : bucket) {
      if (identity.type == nullptr) {
        throw std::runtime_error(
            "AST JSON template-type identity cache is malformed");
      }
      if (identity.serialized_type.exactlyEquals(json)) {
        return identity.type;
      }
    }
    SgTemplateType *type = isSgTemplateType(typeFromJsonUncached(json, nodes));
    if (type == nullptr || !type->get_canonical_source_identity().has_value()) {
      throw std::runtime_error(
          "AST JSON canonical template type lost its exact source identity");
    }
    bucket.push_back({json, type});
    return type;
  }
  if (array_type != (identity_json != nullptr)) {
    throw std::runtime_error(
        "AST JSON semantic array identity disagrees with the type kind");
  }
  if (!array_type) {
    return typeFromJsonUncached(json, nodes);
  }
  if (!arrayTypeDeserializationIdentityActive) {
    throw std::runtime_error(
        "AST JSON semantic array has no active exact identity transaction");
  }
  const int64_t signed_identity = identity_json->asInt();
  if (signed_identity <= 0) {
    throw std::runtime_error(
        "AST JSON semantic array identity is not positive");
  }
  const uint64_t identity = static_cast<uint64_t>(signed_identity);
  auto found = arrayTypeDeserializationIdentities.find(identity);
  if (found != arrayTypeDeserializationIdentities.end()) {
    if (found->second.type == nullptr ||
        !found->second.serialized_type.exactlyEquals(json) ||
        preservedSemanticArrayJsonIdentity(found->second.type) != identity) {
      throw std::runtime_error(
          "AST JSON repeated semantic array identity is contradictory");
    }
    return found->second.type;
  }

  SgArrayType *type = isSgArrayType(typeFromJsonUncached(json, nodes));
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON semantic array identity reconstructed a non-semantic "
        "array");
  }
  attachSemanticArrayJsonIdentity(type, identity);
  ArrayTypeDeserializationIdentity restored{json, type};
  if (!arrayTypeDeserializationIdentities.emplace(identity, std::move(restored))
           .second) {
    throw std::runtime_error(
        "AST JSON semantic array identity insertion failed");
  }
  return type;
}

SgFunctionType *semanticFunctionTypeFromJson(const JsonValue &json,
                                             const NodeMap &nodes) {
  if (!functionTypeDeserializationIdentityActive) {
    throw std::runtime_error(
        "AST JSON semantic function type was reconstructed outside its exact "
        "identity transaction");
  }
  if (json.kind != JsonValue::Kind::Object || !json.requiredBool("present")) {
    throw std::runtime_error(
        "AST JSON semantic function type is absent or malformed");
  }
  const std::string kind = json.requiredString("kind");
  if (kind != "SgFunctionType" && kind != "SgMemberFunctionType") {
    throw std::runtime_error(
        "AST JSON semantic function type has non-function kind " + kind);
  }

  // Source-syntax function types can own selector expressions.  They are
  // lexical type surfaces, not canonical semantic identities, and therefore
  // must retain one distinct reconstruction per serialized owner.
  if (json.requiredBool("fortran_source_syntax")) {
    SgFunctionType *source_type = isSgFunctionType(typeFromJson(json, nodes));
    if (source_type == nullptr || !source_type->get_fortran_source_syntax()) {
      throw std::runtime_error(
          "AST JSON source function type lost its exact source identity");
    }
    return source_type;
  }

  const uint64_t identity_hash = exactJsonValueHash(json);
  std::vector<FunctionTypeDeserializationIdentity> &bucket =
      functionTypeDeserializationIdentities[identity_hash];
  for (const FunctionTypeDeserializationIdentity &identity : bucket) {
    if (identity.serialized_type == nullptr || identity.type == nullptr) {
      throw std::runtime_error(
          "AST JSON semantic function-type identity cache is malformed");
    }
    if (identity.serialized_type->exactlyEquals(json)) {
      return identity.type;
    }
  }

  SgFunctionType *type = isSgFunctionType(typeFromJson(json, nodes));
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON semantic function type did not reconstruct an exact "
        "semantic function type");
  }
  bucket.push_back({&json, type});
  return type;
}

SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object || !json.requiredBool("present")) {
    return nullptr;
  }
  return typeFromJson(json, nodes);
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
