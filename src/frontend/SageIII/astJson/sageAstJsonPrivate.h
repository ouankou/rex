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
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
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

extern const char *const kAstJsonTypeTextAttribute;
extern const char *const kAstJsonExternalFunctionAttribute;
extern const char *const kAstJsonExternalModuleAttribute;
extern const char *const kAstJsonExternalClassDeclarationAttribute;
extern const char *const kAstJsonExternalSourceFileAttribute;

extern SgSourceFile *collectionBoundaryFile;
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

  std::string stringOr(const std::string &key,
                       const std::string &fallback = "") const {
    const JsonValue *value = find(key);
    if (value == nullptr || value->kind == Kind::Null) {
      return fallback;
    }
    return value->asString();
  }

  int64_t asInt() const {
    if (kind != Kind::Number) {
      throw std::runtime_error("AST JSON value is not a number");
    }
    return std::stoll(text);
  }

  int64_t intOr(const std::string &key, int64_t fallback = 0) const {
    const JsonValue *value = find(key);
    if (value == nullptr || value->kind == Kind::Null) {
      return fallback;
    }
    return value->asInt();
  }

  bool asBool() const {
    if (kind != Kind::Bool) {
      throw std::runtime_error("AST JSON value is not a bool");
    }
    return bool_value;
  }

  bool boolOr(const std::string &key, bool fallback = false) const {
    const JsonValue *value = find(key);
    if (value == nullptr || value->kind == Kind::Null) {
      return fallback;
    }
    return value->asBool();
  }
};

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

class AstJsonMarkerAttribute : public AstAttribute {
public:
  AstAttribute *copy() const override { return new AstJsonMarkerAttribute(); }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "AstJsonMarkerAttribute";
  }
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

class DeserializationProjectGuard {
public:
  explicit DeserializationProjectGuard(SgProject *project);
  ~DeserializationProjectGuard();

private:
  SgProject *previous_;
};

struct GlobalQualificationStateSnapshot {
  typedef typename std::remove_reference<
      decltype(SgNode::get_globalQualifiedNameMapForNames())>::type NamesMap;
  typedef typename std::remove_reference<
      decltype(SgNode::get_globalQualifiedNameMapForTypes())>::type TypesMap;
  typedef typename std::remove_reference<
      decltype(SgNode::get_globalQualifiedNameMapForTemplateHeaders())>::type
      TemplateHeadersMap;
  typedef typename std::remove_reference<
      decltype(SgNode::get_globalTypeNameMap())>::type TypeNamesMap;
  typedef typename std::remove_reference<
      decltype(SgNode::get_globalQualifiedNameMapForMapsOfTypes())>::type
      TypeMapsMap;

  NamesMap names;
  TypesMap types;
  TemplateHeadersMap template_headers;
  TypeNamesMap type_names;
  TypeMapsMap type_maps;
  bool restored = false;

  GlobalQualificationStateSnapshot();
  ~GlobalQualificationStateSnapshot();
  void restore();
};

bool startsWith(const std::string &s, const std::string &prefix);
std::string sanitizePathComponent(std::string name);
uint64_t fnv1a64(const std::string &value);
std::string hex64(uint64_t value);
uint64_t processId();
std::vector<std::string> commandLine(const SgSourceFile *file);
bool argumentSelectsCheckpoint(const std::string &value, Checkpoint checkpoint);
bool hasCheckpointArgument(const SgSourceFile *file, Checkpoint checkpoint);
std::string commandLineValue(const SgSourceFile *file,
                             const std::string &option);
std::string defaultOutputDirectory(const SgSourceFile *file);
std::string trim(const std::string &input);
JsonValue parseJson(const std::string &json);
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
bool isAnonymousQualificationPrefix(const std::string &prefix);
bool hasExplicitReferenceQualification(const SgVarRefExp *ref);
bool isRightHandSideOfMemberAccess(const SgNode *node);
bool shouldSerializeNamePrefix(SgNode *node, const std::string &prefix);
bool isAnonymousDataMemberReference(SgVarRefExp *ref);
void clearReferenceNameQualification(SgVarRefExp *ref);
void normalizeAnonymousDataMemberReference(SgVarRefExp *ref);
std::string rawQualifiedNameStateJson(
    SgNode *node, const std::unordered_map<const SgNode *, uint64_t> &ids);
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
std::string jsonTypeText(SgType *type);
void installPointerCache(SgType *base, SgPointerType *pointer);
void installReferenceCache(SgType *base, SgReferenceType *reference);
void installRvalueReferenceCache(SgType *base,
                                 SgRvalueReferenceType *reference);
SgType *attachJsonTypeText(SgType *type, const JsonValue &json);
SgPointerType *buildCachedJsonPointerType(SgType *base,
                                          const JsonValue *json = nullptr);
void installNewExpressionResultType(SgNewExp *expr, SgType *restored_type,
                                    const JsonValue &json);
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
std::string
rawTypeTraitArgsJson(const SgNodePtrList &args,
                     const std::unordered_map<const SgNode *, uint64_t> &ids);
const SgNode *symbolBasis(const SgSymbol *symbol);
std::string symbolName(const SgSymbol *symbol);
std::string
rawSymbolRef(SgSymbol *symbol,
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
std::string rawOmpArrayDimensionsJson(
    const std::map<SgSymbol *,
                   std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawOmpDistDataPoliciesJson(
    const std::map<SgSymbol *,
                   std::vector<std::pair<SgOmpClause::omp_map_dist_data_enum,
                                         SgExpression *>>> &policies,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawOmpExpressionListJson(
    const std::list<SgExpression *> &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawOmpIteratorJson(const std::list<std::list<SgExpression *>> &iterators,
                   const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawTemplateArgumentListJson(
    const SgTemplateArgumentPtrList &arguments,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawOmpUsesAllocatorsDefinitionsJson(
    const std::list<SgOmpUsesAllocatorsDefination *> &definitions,
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
std::vector<SgNode *> collectNodes(SgNode *root);
void clearGlobalQualificationState();
std::string safeNodeText(SgNode *node);
std::string sourceFileNameForNode(SgNode *node);
std::string sourceFileNameForExternalFunction(SgFunctionDeclaration *decl);
std::string moduleNameForNode(SgNode *node);
bool classDeclarationHasDefinition(SgClassDeclaration *decl);
bool classDeclarationIsFirstNondefining(SgClassDeclaration *decl);
bool isStructuralAstChildOfParent(SgNode *node);
bool hasNonStructuralExternalMarkerAncestor(SgNode *node);

std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl);
std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external = false);
std::string rawExternalFunctionParameterScopeJson(
    SgFunctionParameterScope *scope,
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
    SgDeclarationStatement *decl,
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
void restoreQualifiedNameState(SgNode *node, const JsonValue &properties,
                               const NodeMap &nodes);
void requireRestoredKind(SgNode *node, const NodeRecord &record);
bool requiresDelayedRebuild(const NodeRecord &record);
SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes);
SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes);
SgType *earlyTypeFromJson(const JsonValue &type);
SgType *earlyTypeFromProperties(const JsonValue &properties);
bool isJsonBinaryOpKind(const std::string &kind);
SgBinaryOp *buildBinaryOpForKind(const std::string &kind, SgType *expr_type);
bool isJsonUnaryOpKind(const std::string &kind);
SgUnaryOp *buildUnaryOpForKind(const std::string &kind, SgType *expr_type,
                               const JsonValue &properties);
SgFile::languageOption_enum
fileLanguageFromJson(const JsonValue &properties, const std::string &key,
                     SgFile::languageOption_enum fallback);
SgFile::outputFormatOption_enum
fileOutputFormatFromJson(const JsonValue &properties, const std::string &key,
                         SgFile::outputFormatOption_enum fallback);
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
                                               const NodeMap &nodes);
SgClassType *ensureClassTypeForDeclaration(SgClassDeclaration *decl);
SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
SgModuleStatement *externalModuleFromJson(const JsonValue &json);
SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes);
SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json);
SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue *json, const NodeMap &nodes, const std::string &context);
SgFunctionParameterScope *
externalFunctionParameterScopeFromJson(const JsonValue &json,
                                       const NodeMap &nodes,
                                       const std::string &function_name);
SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
std::vector<EdgeRecord> edgesFor(const NodeRecord &record,
                                 const std::string &field);
uint64_t singleEdgeTarget(const NodeRecord &record, const std::string &field);
void setNodeSourcePosition(SgNode *node, const NodeRecord &record);
void setNodeFlags(SgNode *node, const NodeRecord &record);
void restoreAvailableSourcePositionsAndScopes(const AstFileRecord &ast,
                                              const NodeMap &nodes);
void attachPreprocessingInfo(SgNode *node, const NodeRecord &record);
void attachAstAttributes(SgNode *node, const NodeRecord &record);
void linkNodeEdges(const NodeRecord &record, const NodeMap &nodes);
SgScopeStatement *nearestScope(SgNode *node);

void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json);
SgClassSymbol *classSymbolForDeclaration(SgClassDeclaration *decl);
SgNonrealSymbol *nonrealSymbolForDeclaration(SgNonrealDecl *decl);
void attachExternalSymbolBasisToScope(SgSymbol *symbol,
                                      SgScopeStatement *scope);
void restoreSerializedSymbolTables(const AstFileRecord &ast,
                                   const NodeMap &nodes);
bool declarationHasDefinition(SgDeclarationStatement *decl);
std::list<std::list<SgExpression *>> iteratorFromJson(const JsonValue &json,
                                                      const NodeMap &nodes);
std::list<SgExpression *> expressionListFromJson(const JsonValue &json,
                                                 const NodeMap &nodes);
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
