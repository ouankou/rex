#ifndef ROSE_SAGE_AST_JSON_PRIVATE_H
#define ROSE_SAGE_AST_JSON_PRIVATE_H

#include "sageAstJson.h"

#include "RoseAst.h"
#include "sage3basic.h"
#include "sageBuilder.h"
#include "sageInterface.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <unistd.h>

namespace Rose {
namespace AstJson {

extern const char *const kFormat;
extern const int kSchemaVersion;

extern const char *const kAstJsonExternalFunctionAttribute;
extern const char *const kAstJsonExternalModuleAttribute;
extern const char *const kAstJsonExternalClassDeclarationAttribute;
extern const char *const kAstJsonExternalSourceFileAttribute;

extern SgSourceFile *collectionBoundaryFile;
extern SgNode *collectionBoundaryRoot;
extern SgProject *currentDeserializationProject;
extern std::unordered_map<SgSourceFile *, std::vector<SgClassDeclaration *>>
    currentDeserializationClassDeclarationCache;
extern const SgNode *currentTypeSerializationNode;
extern bool serializingTypeOwnedExpression;

struct JsonValue {
  enum class Kind { Null, Bool, Number, String, Array, Object };

  Kind kind = Kind::Null;
  bool bool_value = false;
  std::string text;
  std::vector<JsonValue> array;
  std::map<std::string, JsonValue> object;

  static JsonValue null() { return JsonValue(); }

  static JsonValue boolean(bool value) {
    JsonValue result;
    result.kind = Kind::Bool;
    result.bool_value = value;
    return result;
  }

  static JsonValue number(std::string value) {
    JsonValue result;
    result.kind = Kind::Number;
    result.text = std::move(value);
    return result;
  }

  static JsonValue string(std::string value) {
    JsonValue result;
    result.kind = Kind::String;
    result.text = std::move(value);
    return result;
  }

  static JsonValue arrayValue(std::vector<JsonValue> value) {
    JsonValue result;
    result.kind = Kind::Array;
    result.array = std::move(value);
    return result;
  }

  static JsonValue objectValue(std::map<std::string, JsonValue> value) {
    JsonValue result;
    result.kind = Kind::Object;
    result.object = std::move(value);
    return result;
  }

  bool exactlyEquals(const JsonValue &other) const {
    if (kind != other.kind || bool_value != other.bool_value ||
        text != other.text || array.size() != other.array.size() ||
        object.size() != other.object.size()) {
      return false;
    }
    for (size_t index = 0; index < array.size(); ++index) {
      if (!array[index].exactlyEquals(other.array[index])) {
        return false;
      }
    }
    auto this_field = object.begin();
    auto other_field = other.object.begin();
    for (; this_field != object.end(); ++this_field, ++other_field) {
      if (this_field->first != other_field->first ||
          !this_field->second.exactlyEquals(other_field->second)) {
        return false;
      }
    }
    return true;
  }

  const JsonValue &at(const std::string &key) const {
    if (kind != Kind::Object) {
      throw std::runtime_error(
          "AST JSON value is not an object while reading " + key);
    }
    auto found = object.find(key);
    if (found == object.end()) {
      throw std::runtime_error("AST JSON object is missing key: " + key);
    }
    return found->second;
  }

  const JsonValue *find(const std::string &key) const {
    if (kind != Kind::Object) {
      return nullptr;
    }
    auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
  }

  std::string asString() const {
    if (kind != Kind::String) {
      throw std::runtime_error("AST JSON value is not a string");
    }
    return text;
  }

  std::string requiredString(const std::string &key) const {
    return at(key).asString();
  }

  int64_t asInt() const {
    if (kind != Kind::Number) {
      throw std::runtime_error("AST JSON value is not a number");
    }
    std::size_t consumed = 0;
    int64_t value = 0;
    try {
      value = std::stoll(text, &consumed, 10);
    } catch (const std::invalid_argument &) {
      throw std::runtime_error("AST JSON number is not an integer: " + text);
    } catch (const std::out_of_range &) {
      throw std::runtime_error("AST JSON integer is out of range: " + text);
    }
    if (consumed != text.size()) {
      throw std::runtime_error("AST JSON number is not an integer: " + text);
    }
    return value;
  }

  int64_t requiredInt(const std::string &key) const { return at(key).asInt(); }

  bool asBool() const {
    if (kind != Kind::Bool) {
      throw std::runtime_error("AST JSON value is not a bool");
    }
    return bool_value;
  }

  bool requiredBool(const std::string &key) const { return at(key).asBool(); }
};

template <typename Enum>
inline Enum requiredEnum(const JsonValue &object, const std::string &field,
                         const std::string &context,
                         std::initializer_list<Enum> legal_values,
                         const std::string &field_description = {}) {
  static_assert(std::is_enum_v<Enum>,
                "AST JSON enum decoder requires an enum type");
  const int64_t raw = object.requiredInt(field);
  for (Enum legal_value : legal_values) {
    if (raw == static_cast<int64_t>(legal_value)) {
      return legal_value;
    }
  }
  throw std::runtime_error(
      "AST JSON " + context + " has an invalid " +
      (field_description.empty() ? field : field_description) + " enum value");
}

template <typename Enum>
inline Enum
requiredClosedEnumRange(const JsonValue &object, const std::string &field,
                        const std::string &context, Enum first, Enum last) {
  static_assert(std::is_enum_v<Enum>,
                "AST JSON closed-range decoder requires an enum type");
  const int64_t raw = object.requiredInt(field);
  if (raw <= static_cast<int64_t>(first) || raw >= static_cast<int64_t>(last)) {
    throw std::runtime_error("AST JSON " + context + " has an invalid " +
                             field + " enum value");
  }
  return static_cast<Enum>(raw);
}

inline SgClassDeclaration::class_types
requiredClassType(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgClassDeclaration::class_types>(
      object, "class_type", context,
      {SgClassDeclaration::e_class, SgClassDeclaration::e_struct,
       SgClassDeclaration::e_union, SgClassDeclaration::e_template_parameter,
       SgClassDeclaration::e_fortran_module});
}

inline SgStorageModifier::storage_modifier_enum
requiredStorageModifier(const JsonValue &object, const std::string &field,
                        const std::string &context) {
  return requiredEnum<SgStorageModifier::storage_modifier_enum>(
      object, field, context,
      {SgStorageModifier::e_default,
       SgStorageModifier::e_extern,
       SgStorageModifier::e_static,
       SgStorageModifier::e_auto,
       SgStorageModifier::e_unspecified,
       SgStorageModifier::e_register,
       SgStorageModifier::e_mutable,
       SgStorageModifier::e_typedef,
       SgStorageModifier::e_asm,
#ifdef FORTRAN_SUPPORTED
       SgStorageModifier::e_local,
       SgStorageModifier::e_common,
       SgStorageModifier::e_associated,
       SgStorageModifier::e_intrinsic,
       SgStorageModifier::e_pointer_based,
#endif
       SgStorageModifier::e_contiguous,
       SgStorageModifier::e_cuda_global,
       SgStorageModifier::e_cuda_constant,
       SgStorageModifier::e_cuda_shared,
       SgStorageModifier::e_cuda_dynamic_shared,
       SgStorageModifier::e_cuda_device_memory,
       SgStorageModifier::e_cuda_managed,
       SgStorageModifier::e_cuda_unified,
       SgStorageModifier::e_cuda_pinned,
       SgStorageModifier::e_cuda_texture});
}

inline SgAccessModifier::access_modifier_enum
requiredDeclarationAccessModifier(const JsonValue &object,
                                  const std::string &field,
                                  const std::string &context) {
  return requiredEnum<SgAccessModifier::access_modifier_enum>(
      object, field, context,
      {SgAccessModifier::e_private, SgAccessModifier::e_protected,
       SgAccessModifier::e_public, SgAccessModifier::e_default,
       SgAccessModifier::e_not_applicable, SgAccessModifier::e_undefined});
}

inline SgAccessModifier::access_modifier_enum
requiredBaseClassAccessModifier(const JsonValue &object,
                                const std::string &field,
                                const std::string &context) {
  return requiredEnum<SgAccessModifier::access_modifier_enum>(
      object, field, context,
      {SgAccessModifier::e_private, SgAccessModifier::e_protected,
       SgAccessModifier::e_public, SgAccessModifier::e_default,
       SgAccessModifier::e_undefined});
}

inline SgInitializedName::fortran_type_spec_enum
requiredFortranTypeSpec(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgInitializedName::fortran_type_spec_enum>(
      object, "fortran_type_spec", context,
      {SgInitializedName::e_fortran_type_spec_default,
       SgInitializedName::e_fortran_type_spec_type,
       SgInitializedName::e_fortran_type_spec_class,
       SgInitializedName::e_fortran_type_spec_type_star,
       SgInitializedName::e_fortran_type_spec_class_star});
}

inline SgInitializedName::preinitialization_enum
requiredPreinitialization(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgInitializedName::preinitialization_enum>(
      object, "preinitialization", context,
      {SgInitializedName::e_unknown_preinitialization,
       SgInitializedName::e_virtual_base_class,
       SgInitializedName::e_nonvirtual_base_class,
       SgInitializedName::e_data_member,
       SgInitializedName::e_delegation_constructor});
}

inline SgTemplateArgument::template_argument_enum
requiredTemplateArgumentType(const JsonValue &object,
                             const std::string &context) {
  return requiredEnum<SgTemplateArgument::template_argument_enum>(
      object, "argument_type", context,
      {SgTemplateArgument::type_argument, SgTemplateArgument::nontype_argument,
       SgTemplateArgument::template_template_argument,
       SgTemplateArgument::start_of_pack_expansion_argument});
}

inline SgTemplateParameter::template_parameter_enum
requiredTemplateParameterType(const JsonValue &object,
                              const std::string &context) {
  return requiredEnum<SgTemplateParameter::template_parameter_enum>(
      object, "parameter_type", context,
      {SgTemplateParameter::type_parameter,
       SgTemplateParameter::nontype_parameter,
       SgTemplateParameter::template_parameter});
}

inline SgDeclarationModifier::gnu_declaration_visibility_enum
requiredGnuDeclarationVisibility(const JsonValue &object,
                                 const std::string &field,
                                 const std::string &context) {
  const int64_t raw = object.requiredInt(field);
  switch (raw) {
  case SgDeclarationModifier::e_unspecified_visibility:
  case SgDeclarationModifier::e_hidden_visibility:
  case SgDeclarationModifier::e_protected_visibility:
  case SgDeclarationModifier::e_internal_visibility:
  case SgDeclarationModifier::e_default_visibility:
    return static_cast<SgDeclarationModifier::gnu_declaration_visibility_enum>(
        raw);
  default:
    throw std::runtime_error("AST JSON " + context + " " + field +
                             " is not a valid source visibility");
  }
}

struct EdgeRecord {
  std::string field;
  uint64_t target = 0;
  uint64_t index = 0;
};

struct NodeRecord {
  uint64_t id = 0;
  std::string kind;
  int64_t variant = 0;
  JsonValue flags;
  JsonValue location;
  JsonValue properties;
  JsonValue preprocessing;
  std::vector<EdgeRecord> edges;
};

struct AstFileRecord {
  uint64_t root_id = 0;
  std::string root_kind;
  std::string checkpoint;
  JsonValue metadata;
  std::vector<NodeRecord> nodes;
  std::unordered_map<uint64_t, size_t> index_by_id;

  const NodeRecord &node(uint64_t id) const;
};

using NodeMap = std::unordered_map<uint64_t, SgNode *>;

struct SymbolTableEntryJson {
  std::string entry_name;
  std::string symbol_kind;
  uint64_t basis_id = 0;
  bool lookup_preferred = false;
  std::string json;
};

class TypeSerializationContext {
public:
  explicit TypeSerializationContext(const SgNode *node);
  ~TypeSerializationContext();

private:
  const SgNode *previous_;
};

class CollectionBoundaryGuard {
public:
  explicit CollectionBoundaryGuard(SgSourceFile *file);
  ~CollectionBoundaryGuard();

private:
  SgSourceFile *previous_;
};

class SubtreeBoundaryGuard {
public:
  explicit SubtreeBoundaryGuard(SgNode *root);
  ~SubtreeBoundaryGuard();

  SubtreeBoundaryGuard(const SubtreeBoundaryGuard &) = delete;
  SubtreeBoundaryGuard &operator=(const SubtreeBoundaryGuard &) = delete;

private:
  SgNode *previous_;
};

class DeserializationProjectGuard {
public:
  explicit DeserializationProjectGuard(SgProject *project);
  ~DeserializationProjectGuard();

private:
  SgProject *previous_;
};

bool sameAstJsonPath(const std::string &lhs, const std::string &rhs);
bool sourceFileMatchesExternalRecord(SgSourceFile *file,
                                     const std::string &source_file);
std::vector<SgSourceFile *> currentDeserializationSourceFiles();
bool startsWith(const std::string &s, const std::string &prefix);
std::string sanitizePathComponent(std::string name);
uint64_t fnv1a64(const std::string &value);
std::string hex64(uint64_t value);
uint64_t processId();
std::vector<std::string> commandLine(const SgSourceFile *file);
bool argumentSelectsCheckpoint(const std::string &value, Checkpoint checkpoint);
bool hasCheckpointArgument(const SgSourceFile *file, Checkpoint checkpoint);

void validateFortranProgramNameMetadata(const std::string &opening_name,
                                        int64_t statement_kind,
                                        bool named_in_end_statement,
                                        const std::string &end_statement_name);
std::string commandLineValue(const SgSourceFile *file,
                             const std::string &option);
std::string defaultOutputDirectory(const SgSourceFile *file);
std::string trim(const std::string &input);
JsonValue parseJson(const std::string &json);
uint64_t exactJsonValueHash(const JsonValue &value);
std::string jsonString(const std::string &input);
void indent(std::ostream &out, int level);
void writeStringField(std::ostream &out, int level, const char *name,
                      const std::string &value, bool comma = true);
void writeIntegerField(std::ostream &out, int level, const char *name,
                       int64_t value, bool comma = true);
void writeBoolField(std::ostream &out, int level, const char *name, bool value,
                    bool comma = true);
void requireFileIdMapping(int id, const std::string &filename,
                          const std::string &context);
std::string filenameForFileId(int id, const std::string &context);
std::string rawStringField(const std::string &name, const std::string &value);
std::string rawIntegerField(const std::string &name, int64_t value);
std::string
rawOptionalUnsignedIntegerField(const std::string &name,
                                const std::optional<unsigned int> &value);
std::string rawBoolField(const std::string &name, bool value);
std::string rawBitVectorJson(const SgBitVector &bits);
std::string rawStringListJson(const SgStringList &values);
SgStringList stringListFromJson(const JsonValue &value,
                                const std::string &context);
void writeFileIdMapJson(std::ostream &out, int level, bool comma = true);
void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma = true);

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node);
uint64_t canonicalTypedefDeclarationId(
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgTypedefDeclaration *decl);
bool edgeTargetIsInParentChain(const SgNode *node, const SgNode *target);
bool isRightHandSideOfMemberAccess(const SgNode *node);
bool isAnonymousDataMemberReference(SgVarRefExp *ref);
void validateAnonymousDataMemberReferenceQualification(SgVarRefExp *ref);
std::string astJsonStringAttribute(SgNode *node, const char *name);
bool hasAstJsonAttribute(SgNode *node, const char *name);
bool isAstJsonExternalFunction(SgFunctionDeclaration *decl);
bool isAstJsonExternalModule(SgModuleStatement *module);
bool isAstJsonExternalClassDeclaration(SgClassDeclaration *decl);
void markAstJsonExternalFunction(SgFunctionDeclaration *decl,
                                 const std::string &source_file);
void markAstJsonExternalModule(SgModuleStatement *module,
                               const std::string &source_file);
void markAstJsonExternalClassDeclaration(SgClassDeclaration *decl,
                                         const std::string &source_file);
void attachJsonNodeId(SgNode *node, uint64_t id);
uint64_t preservedJsonNodeId(SgNode *node);
uint64_t semanticArrayJsonIdentity(SgArrayType *type);
uint64_t preservedSemanticArrayJsonIdentity(const SgArrayType *type);
void attachSemanticArrayJsonIdentity(SgArrayType *type, uint64_t identity);
uint64_t pointerMemberJsonIdentity(SgPointerMemberType *type);
uint64_t preservedPointerMemberJsonIdentity(const SgPointerMemberType *type);
void attachPointerMemberJsonIdentity(SgPointerMemberType *type,
                                     uint64_t identity);
void installPointerCache(SgType *base, SgPointerType *pointer);
void installReferenceCache(SgType *base, SgReferenceType *reference);
void installRvalueReferenceCache(SgType *base,
                                 SgRvalueReferenceType *reference);
SgPointerType *buildCachedJsonPointerType(SgType *base);
void installNewExpressionResultType(SgNewExp *expr, SgType *restored_type,
                                    const JsonValue &json);
void restoreCastExpressionProperties(SgCastExp *cast,
                                     const JsonValue &properties,
                                     const NodeMap &nodes);
void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma = true);
std::string rawFileInfoJson(const Sg_File_Info *info);
std::string rawLocationJson(SgNode *node);
std::string rawRequiredLocationJson(SgNode *node, const std::string &context);
std::string rawNodeFlagsJson(SgNode *node);
std::string rawTypeOwnedExpressionRef(
    SgExpression *expr,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawTypeOwnedExpressionListJson(
    const SgExpressionPtrList &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawTypeOwnedExprListExpJson(
    SgExprListExp *expr_list,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExpressionRef(SgExpression *expression,
                 const std::unordered_map<const SgNode *, uint64_t> &ids);
const SgNode *symbolBasis(const SgSymbol *symbol);
std::string symbolName(const SgSymbol *symbol);
std::string
rawSymbolRef(SgSymbol *symbol,
             const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExactBoundSymbolRef(SgSymbol *symbol,
                       const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawSymbolTableJson(SgScopeStatement *scope,
                   const std::unordered_map<const SgNode *, uint64_t> &ids);
bool symbolIsLookupPreferred(SgSymbolTable *table, const SgName &name,
                             SgSymbol *symbol);

uint64_t varRefSymbolDeclarationId(
    SgVarRefExp *ref, const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids);
class PointerMemberTypeSerializationIdentityGuard {
public:
  PointerMemberTypeSerializationIdentityGuard();
  ~PointerMemberTypeSerializationIdentityGuard();

  PointerMemberTypeSerializationIdentityGuard(
      const PointerMemberTypeSerializationIdentityGuard &) = delete;
  PointerMemberTypeSerializationIdentityGuard &
  operator=(const PointerMemberTypeSerializationIdentityGuard &) = delete;
};
class ArrayTypeSerializationIdentityGuard {
public:
  ArrayTypeSerializationIdentityGuard();
  ~ArrayTypeSerializationIdentityGuard();

  ArrayTypeSerializationIdentityGuard(
      const ArrayTypeSerializationIdentityGuard &) = delete;
  ArrayTypeSerializationIdentityGuard &
  operator=(const ArrayTypeSerializationIdentityGuard &) = delete;
};
std::string rawTemplateArgumentListJson(
    const SgTemplateArgumentPtrList &arguments,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawOmpUsesAllocatorsDefinitionsJson(
    const SgOmpUsesAllocatorsDefinationPtrList &definitions,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawAstAttributesJson(SgNode *node);
void addExpressionType(std::vector<std::string> &fields, SgExpression *expr,
                       const std::unordered_map<const SgNode *, uint64_t> &ids);
void addExpressionQualificationFields(std::vector<std::string> &fields,
                                      SgExpression *expr);
void restoreExpressionQualificationFields(SgExpression *expr,
                                          const JsonValue &properties);
void addLocatedPreprocessing(std::vector<std::string> &fields,
                             const SgLocatedNode *node);
bool insideCollectionBoundary(SgNode *node);
std::vector<SgNode *> collectNodes(SgNode *root,
                                   SgSourceFile *collectionBoundary = nullptr);
std::string safeNodeText(SgNode *node);
std::string sourceFileNameForNode(SgNode *node);
std::string sourceFileNameForExternalFunction(SgFunctionDeclaration *decl);
std::string moduleNameForNode(SgNode *node);
bool classDeclarationHasDefinition(SgClassDeclaration *decl);
bool classDeclarationIsFirstNondefining(SgClassDeclaration *decl);
bool isStructuralAstChildOfParent(SgNode *node);
bool hasExternalMarkerAncestor(SgNode *node);
void validateTemplateParameterContract(const SgTemplateParameter *parameter,
                                       const std::string &context);
void validateOmpContextSelectorProperty(
    const SgOmpContextSelectorProperty *property,
    const SgOmpContextSelector *selector);
void validateOmpContextSelector(const SgOmpContextSelector *selector);
void validateOmpContextSelectorSet(const SgOmpContextSelectorSet *set);
void validateOmpContextSelectorSets(const SgOmpContextSelectorSetPtrList &sets,
                                    const SgNode *owner);
bool expressionCarriesSemanticType(const SgExpression *expression);
void validateExactSemanticExpressionType(const SgExpression *expression,
                                         const SgType *type,
                                         const std::string &phase);
const JsonValue *validatedExpressionTypeProperty(const SgExpression *expression,
                                                 const JsonValue &properties);

std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl);
std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external = false);
std::string rawExternalFunctionParameterScopeJson(
    SgFunctionDeclaration *function, SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
bool declarationNeedsExternalReferenceRecord(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string externalFunctionParameterScopeSource(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
bool functionParameterScopeNeedsExternalReferenceRecord(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalDeclarationReferenceJson(
    SgDeclarationStatement *decl, SgDeclarationStatement *owner,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalInitializedNameJson(
    SgInitializedName *name,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
void appendRawExternalDeclarationStatementFields(
    std::vector<std::string> &fields, SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalVariableDeclarationJson(
    SgVariableDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalUseStatementJson(
    SgUseStatement *stmt,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
bool isExternalUseModuleEdge(
    SgNode *source, SgNode *target, const std::string &field,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids);

void writeNodeJson(std::ostream &out, SgNode *node,
                   const std::unordered_map<const SgNode *, uint64_t> &ids,
                   bool comma);
void writeMetadataJson(std::ostream &out, SgSourceFile *file,
                       Checkpoint checkpoint, bool comma);
std::string buildJson(SgNode *root, Checkpoint checkpoint, SgSourceFile *file);

std::filesystem::path temporaryWritePath(const std::filesystem::path &path);
void writeFile(const std::filesystem::path &path, const std::string &content);
std::string readFile(const std::filesystem::path &path);
AstFileRecord parseAstFileJson(const std::string &json,
                               const std::string &expected_checkpoint);
bool hasEdgeTarget(const NodeRecord &record, const std::string &field,
                   uint64_t target);
uint64_t firstEdgeTarget(const NodeRecord &record, const std::string &field);
std::string semanticSignature(const AstFileRecord &ast);
std::filesystem::path checkpointPath(SgSourceFile *file, Checkpoint checkpoint,
                                     const Options &options);
void replaceFileInProject(SgSourceFile *old_file, SgSourceFile *new_file);
void purgeStaleCheckpointTypeCaches(SgSourceFile *old_file);
SgProject *owningProject(SgSourceFile *file);
std::vector<std::string> commandLineFromMetadata(const JsonValue &metadata);
void restoreFileIdMapFromMetadata(const JsonValue &metadata);

SgNode *nodeById(const NodeMap &nodes, uint64_t id);

template <typename T> T *nodeByIdAs(const NodeMap &nodes, uint64_t id) {
  SgNode *raw_node = nodeById(nodes, id);
  T *node = dynamic_cast<T *>(raw_node);
  if (node == nullptr) {
    throw std::runtime_error(
        "AST JSON node id has unexpected Sage type " + std::to_string(id) +
        ": expected " + typeid(T).name() + ", got " +
        (raw_node != nullptr ? raw_node->sage_class_name() : "<null>"));
  }
  return node;
}

bool sameClassDeclarationFamily(SgClassDeclaration *lhs,
                                SgClassDeclaration *rhs);
void rejectRemovedQualifiedNameState(const JsonValue &properties);
std::optional<unsigned int>
requiredTranslationUnitSourceOrder(const JsonValue &properties,
                                   const std::string &context);
std::optional<unsigned int>
requiredOptionalUnsignedInteger(const JsonValue &properties,
                                const std::string &name,
                                const std::string &context);
std::optional<unsigned int> requiredOmpDeclareVariantRegionOrdinal(
    const JsonValue &properties, const std::string &semantic_function_name,
    const std::string &context);
void restoreTranslationUnitSourceOrder(SgDeclarationStatement *declaration,
                                       const JsonValue &properties,
                                       const std::string &context);
void requireRestoredKind(SgNode *node, const NodeRecord &record);
bool requiresDelayedRebuild(const NodeRecord &record);
class PointerMemberTypeDeserializationIdentityGuard {
public:
  PointerMemberTypeDeserializationIdentityGuard();
  ~PointerMemberTypeDeserializationIdentityGuard();

  PointerMemberTypeDeserializationIdentityGuard(
      const PointerMemberTypeDeserializationIdentityGuard &) = delete;
  PointerMemberTypeDeserializationIdentityGuard &
  operator=(const PointerMemberTypeDeserializationIdentityGuard &) = delete;
};
class FunctionTypeDeserializationIdentityGuard {
public:
  FunctionTypeDeserializationIdentityGuard();
  ~FunctionTypeDeserializationIdentityGuard();

  FunctionTypeDeserializationIdentityGuard(
      const FunctionTypeDeserializationIdentityGuard &) = delete;
  FunctionTypeDeserializationIdentityGuard &
  operator=(const FunctionTypeDeserializationIdentityGuard &) = delete;
};
class ArrayTypeDeserializationIdentityGuard {
public:
  ArrayTypeDeserializationIdentityGuard();
  ~ArrayTypeDeserializationIdentityGuard();

  ArrayTypeDeserializationIdentityGuard(
      const ArrayTypeDeserializationIdentityGuard &) = delete;
  ArrayTypeDeserializationIdentityGuard &
  operator=(const ArrayTypeDeserializationIdentityGuard &) = delete;
};
class TemplateTypeDeserializationIdentityGuard {
public:
  TemplateTypeDeserializationIdentityGuard();
  ~TemplateTypeDeserializationIdentityGuard();

  TemplateTypeDeserializationIdentityGuard(
      const TemplateTypeDeserializationIdentityGuard &) = delete;
  TemplateTypeDeserializationIdentityGuard &
  operator=(const TemplateTypeDeserializationIdentityGuard &) = delete;
};
class ExternalClassDeserializationIdentityGuard {
public:
  ExternalClassDeserializationIdentityGuard();
  ~ExternalClassDeserializationIdentityGuard();

  ExternalClassDeserializationIdentityGuard(
      const ExternalClassDeserializationIdentityGuard &) = delete;
  ExternalClassDeserializationIdentityGuard &
  operator=(const ExternalClassDeserializationIdentityGuard &) = delete;
};
class ExternalFunctionDeserializationIdentityGuard {
public:
  ExternalFunctionDeserializationIdentityGuard();
  ~ExternalFunctionDeserializationIdentityGuard();

  ExternalFunctionDeserializationIdentityGuard(
      const ExternalFunctionDeserializationIdentityGuard &) = delete;
  ExternalFunctionDeserializationIdentityGuard &
  operator=(const ExternalFunctionDeserializationIdentityGuard &) = delete;
};
class TypeOwnedSymbolDeserializationGuard {
public:
  TypeOwnedSymbolDeserializationGuard();
  ~TypeOwnedSymbolDeserializationGuard();

  TypeOwnedSymbolDeserializationGuard(
      const TypeOwnedSymbolDeserializationGuard &) = delete;
  TypeOwnedSymbolDeserializationGuard &
  operator=(const TypeOwnedSymbolDeserializationGuard &) = delete;

  void resolve(const NodeMap &nodes);

private:
  bool resolved_ = false;
};
SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes);
SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes);
SgFunctionType *semanticFunctionTypeFromJson(const JsonValue &json,
                                             const NodeMap &nodes);
SgType *earlyTypeFromJson(const JsonValue &type);
SgType *earlyTypeFromProperties(const JsonValue &properties);
bool restoreExactStoredExpressionType(SgExpression *expression,
                                      SgType *restored_type);
bool isJsonBinaryOpKind(const std::string &kind);
SgBinaryOp *buildBinaryOpForKind(const std::string &kind, SgType *expr_type);
bool isJsonUnaryOpKind(const std::string &kind);
SgUnaryOp *buildUnaryOpForKind(const std::string &kind, SgType *expr_type,
                               const JsonValue &properties);
SgFile::languageOption_enum fileLanguageFromJson(const JsonValue &properties,
                                                 const std::string &key);
SgFile::outputFormatOption_enum
fileOutputFormatFromJson(const JsonValue &properties, const std::string &key);
Sg_File_Info *buildFileInfo(const JsonValue &json, SgNode *parent);
void installTransformationSourcePosition(SgLocatedNode *node);
void restoreNodeSourcePositionFromJson(SgNode *node, const JsonValue &location,
                                       const std::string &context);
void restoreOptionalNodeSourcePositionFromJson(SgNode *node,
                                               const JsonValue &properties,
                                               const std::string &field);
SgBitVector bitVectorFromJson(const JsonValue &json,
                              const std::string &context);

SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const JsonValue &metadata);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes);
void restoreExternalDeclarationStatementFields(SgDeclarationStatement *decl,
                                               const JsonValue &json,
                                               const std::string &context);
void restoreDeclarationTypeModifierFields(SgTypeModifier &modifier,
                                          const JsonValue &json,
                                          const std::string &context);
SgClassType *ensureClassTypeForDeclaration(SgClassDeclaration *decl);
SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
SgSymbol *exactBoundSymbolFromJson(const JsonValue &json, const NodeMap &nodes);
SgSymbol *resolveExistingSymbolFromJson(const JsonValue &json,
                                        const NodeMap &nodes);
SgFunctionSymbol *functionReferenceSemanticSymbolFromJson(
    const JsonValue &semantic_symbol,
    const JsonValue &source_visible_symbol_record,
    SgFunctionSymbol *source_visible_symbol, const NodeMap &nodes);
SgModuleStatement *externalModuleFromJson(const JsonValue &json);
SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes);
SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json);
SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue *json, const NodeMap &nodes, const std::string &context);
SgInitializedName *externalInitializedNameFromJson(const JsonValue &json,
                                                   const NodeMap &nodes,
                                                   SgDeclarationStatement *decl,
                                                   SgScopeStatement *scope,
                                                   const std::string &context);
SgFunctionParameterScope *externalFunctionParameterScopeFromJson(
    const JsonValue &json, const NodeMap &nodes,
    const SgInitializedNamePtrList &parameters,
    const std::string &function_name);
SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
std::vector<EdgeRecord> edgesFor(const NodeRecord &record,
                                 const std::string &field);
uint64_t singleEdgeTarget(const NodeRecord &record, const std::string &field);
uint64_t requiredSingleEdgeTarget(const NodeRecord &record,
                                  const std::string &field);
void setNodeSourcePosition(SgNode *node, const NodeRecord &record);
void setNodeFlags(SgNode *node, const NodeRecord &record);
void restoreAvailableSourcePositionsAndScopes(const AstFileRecord &ast,
                                              const NodeMap &nodes);
void restoreAvailableAuxiliaryNamespaceOwnership(const AstFileRecord &ast,
                                                 const NodeMap &nodes);
void attachPreprocessingInfo(SgNode *node, const NodeRecord &record);
void attachAstAttributes(SgNode *node, const NodeRecord &record);
void restoreFunctionCallSourceMetadata(SgFunctionCallExp *call,
                                       const JsonValue &properties);
void linkNodeEdges(const NodeRecord &record, const NodeMap &nodes);
void validateRestoredOmpOwnedClausePayloads(const AstFileRecord &ast,
                                            const NodeMap &nodes);
void linkDeclarationGroupEdges(const AstFileRecord &ast, const NodeMap &nodes);
void linkStatementAttributeEdges(const AstFileRecord &ast,
                                 const NodeMap &nodes);
SgScopeStatement *nearestScope(SgNode *node);

void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json);
SgClassSymbol *classSymbolForDeclaration(SgClassDeclaration *decl);
SgNonrealSymbol *nonrealSymbolForDeclaration(SgNonrealDecl *decl);
void validateExternalSymbolBasisOwnership(SgSymbol *symbol);
void restoreSerializedSymbolTables(const AstFileRecord &ast,
                                   const NodeMap &nodes);
bool declarationHasDefinition(SgDeclarationStatement *decl);
SgTemplateArgumentPtrList templateArgumentListFromJson(const JsonValue &json,
                                                       const NodeMap &nodes,
                                                       SgNode *parent);
void applyOmpAuxiliaryState(const AstFileRecord &ast, const NodeMap &nodes);

void applyTypesAndSymbols(const AstFileRecord &ast, const NodeMap &nodes);
void rebuildConstructorOnlyNodes(const AstFileRecord &ast, NodeMap &nodes);
void restoreDeclarationIdentityEdges(const AstFileRecord &ast,
                                     const NodeMap &nodes);
void restoreRecordedParentEdges(const AstFileRecord &ast, const NodeMap &nodes);
void restoreRecordedScopeEdges(const AstFileRecord &ast, const NodeMap &nodes);
SgSourceFile *reconstructSourceFile(const AstFileRecord &ast,
                                    SgSourceFile *old_file);

} // namespace AstJson
} // namespace Rose

#endif
