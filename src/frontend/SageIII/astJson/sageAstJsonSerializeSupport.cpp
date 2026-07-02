#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node);

bool edgeTargetIsInParentChain(const SgNode *node, const SgNode *target) {
  if (node == nullptr || target == nullptr) {
    return false;
  }
  for (const SgNode *current = node->get_parent(); current != nullptr;
       current = current->get_parent()) {
    if (current == target) {
      return true;
    }
  }
  return false;
}

bool isAnonymousQualificationPrefix(const std::string &prefix) {
  return prefix.rfind("__anonymous_0x", 0) == 0;
}

bool hasExplicitReferenceQualification(const SgVarRefExp *ref) {
  return ref != nullptr &&
         (ref->get_explicit_name_qualification_length() >= 0 ||
          ref->get_explicit_global_qualification() ||
          !ref->get_explicit_name_qualification_tokens().empty());
}

bool isRightHandSideOfMemberAccess(const SgNode *node) {
  if (node == nullptr) {
    return false;
  }
  const SgNode *parent = node->get_parent();
  if (const SgDotExp *dot = isSgDotExp(parent)) {
    return dot->get_rhs_operand() == node;
  }
  if (const SgArrowExp *arrow = isSgArrowExp(parent)) {
    return arrow->get_rhs_operand() == node;
  }
  return false;
}

bool shouldSerializeNamePrefix(SgNode *node, const std::string &prefix) {
  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    if (isRightHandSideOfMemberAccess(ref) &&
        !hasExplicitReferenceQualification(ref)) {
      return false;
    }
  }
  if (isAnonymousQualificationPrefix(prefix) &&
      isSgVarRefExp(node) != nullptr && isRightHandSideOfMemberAccess(node)) {
    return false;
  }
  return true;
}

bool isAnonymousDataMemberReference(SgVarRefExp *ref) {
  if (ref == nullptr || !isRightHandSideOfMemberAccess(ref)) {
    return false;
  }
  SgVariableSymbol *symbol = ref->get_symbol();
  SgInitializedName *name =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgClassDefinition *definition =
      name != nullptr ? isSgClassDefinition(name->get_scope()) : nullptr;
  SgClassDeclaration *declaration =
      definition != nullptr ? definition->get_declaration() : nullptr;
  if (declaration == nullptr) {
    return false;
  }
  return declaration->get_isUnNamed() ||
         isAnonymousQualificationPrefix(declaration->get_name().getString());
}

void clearReferenceNameQualification(SgVarRefExp *ref) {
  ref->set_name_qualification_length(0);
  ref->set_type_elaboration_required(false);
  ref->set_global_qualification_required(false);
  ref->set_explicit_name_qualification_length(-1);
  ref->set_explicit_global_qualification(false);
  ref->set_explicit_name_qualification_tokens(SgStringList());
  SgNode::get_globalQualifiedNameMapForNames().erase(ref);
}

void normalizeAnonymousDataMemberReference(SgVarRefExp *ref) {
  if (isAnonymousDataMemberReference(ref)) {
    clearReferenceNameQualification(ref);
  }
}

std::string rawQualifiedNameStateJson(
    SgNode *node, const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;

  auto append_simple_map_entry = [&](const std::string &field,
                                     SgUnorderedMapNodeToString &map) {
    auto found = map.find(node);
    if (found != map.end()) {
      if (field == "name_prefix" &&
          !shouldSerializeNamePrefix(node, found->second)) {
        return;
      }
      fields.push_back(rawStringField(field, found->second));
    }
  };

  append_simple_map_entry("name_prefix",
                          SgNode::get_globalQualifiedNameMapForNames());
  append_simple_map_entry("type_prefix",
                          SgNode::get_globalQualifiedNameMapForTypes());
  append_simple_map_entry(
      "template_header",
      SgNode::get_globalQualifiedNameMapForTemplateHeaders());
  append_simple_map_entry("type_name", SgNode::get_globalTypeNameMap());

  auto maps_found =
      SgNode::get_globalQualifiedNameMapForMapsOfTypes().find(node);
  if (maps_found != SgNode::get_globalQualifiedNameMapForMapsOfTypes().end()) {
    std::vector<std::pair<uint64_t, std::string>> entries;
    for (const auto &entry : maps_found->second) {
      const uint64_t target_id = idFor(ids, entry.first);
      if (target_id == 0) {
        std::ostringstream message;
        message << "AST JSON qualified type-map entry target was not collected"
                << " while serializing " << node->sage_class_name();
        if (SgNode *target = entry.first) {
          message << " target=" << target->sage_class_name();
        }
        throw std::runtime_error(message.str());
      }
      entries.emplace_back(target_id, entry.second);
    }
    std::sort(entries.begin(), entries.end());

    std::ostringstream map_out;
    map_out << "[";
    for (size_t i = 0; i < entries.size(); ++i) {
      if (i != 0) {
        map_out << ", ";
      }
      std::vector<std::string> entry_fields;
      entry_fields.push_back(rawIntegerField("node", entries[i].first));
      entry_fields.push_back(rawStringField("prefix", entries[i].second));
      std::ostringstream entry_out;
      writeRawObject(entry_out, 0, entry_fields, false);
      std::string entry_text = entry_out.str();
      if (!entry_text.empty() && entry_text.back() == '\n') {
        entry_text.pop_back();
      }
      map_out << entry_text;
    }
    map_out << "]";
    fields.push_back(jsonString("type_map_prefixes") + ": " + map_out.str());
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma) {
  indent(out, level);
  out << "{";
  if (!fields.empty()) {
    out << '\n';
    for (size_t i = 0; i < fields.size(); ++i) {
      indent(out, level + 2);
      out << fields[i];
      if (i + 1 != fields.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, level);
  }
  out << '}';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids);

std::string safeNodeText(SgNode *node);
bool insideCollectionBoundary(SgNode *node);
std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl);
std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external);

const SgNode *currentTypeSerializationNode = nullptr;
bool serializingTypeOwnedExpression = false;

TypeSerializationContext::TypeSerializationContext(const SgNode *node)
    : previous_(currentTypeSerializationNode) {
  currentTypeSerializationNode = node;
}

TypeSerializationContext::~TypeSerializationContext() {
  currentTypeSerializationNode = previous_;
}

class TypeOwnedExpressionSerializationContext {
public:
  TypeOwnedExpressionSerializationContext()
      : previous_(serializingTypeOwnedExpression) {
    serializingTypeOwnedExpression = true;
  }

  ~TypeOwnedExpressionSerializationContext() {
    serializingTypeOwnedExpression = previous_;
  }

private:
  bool previous_;
};

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node) {
  if (node == nullptr) {
    return 0;
  }
  auto found = ids.find(node);
  return found == ids.end() ? 0 : found->second;
}

uint64_t canonicalTypedefDeclarationId(
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgTypedefDeclaration *decl) {
  if (decl == nullptr) {
    return 0;
  }

  const uint64_t direct_id = idFor(ids, decl);
  if (direct_id != 0) {
    return direct_id;
  }
  if (const SgTypedefDeclaration *first_nondef =
          isSgTypedefDeclaration(decl->get_firstNondefiningDeclaration())) {
    if (const uint64_t id = idFor(ids, first_nondef)) {
      return id;
    }
  }
  if (const SgTypedefDeclaration *defining =
          isSgTypedefDeclaration(decl->get_definingDeclaration())) {
    if (const uint64_t id = idFor(ids, defining)) {
      return id;
    }
  }
  return 0;
}

const char *const kAstJsonTypeTextAttribute = "rex_ast_json_type_text";
const char *const kAstJsonExternalFunctionAttribute =
    "rex_ast_json_external_function";
const char *const kAstJsonExternalModuleAttribute =
    "rex_ast_json_external_module";
const char *const kAstJsonExternalClassDeclarationAttribute =
    "rex_ast_json_external_class_declaration";
const char *const kAstJsonExternalSourceFileAttribute =
    "rex_ast_json_external_source_file";

std::unordered_map<const SgNode *, uint64_t> preservedJsonNodeIds;

class AstJsonTypeTextAttribute : public AstAttribute {
public:
  explicit AstJsonTypeTextAttribute(std::string text)
      : text_(std::move(text)) {}

  AstAttribute *copy() const override {
    return new AstJsonTypeTextAttribute(text_);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "AstJsonTypeTextAttribute";
  }

  const std::string &text() const { return text_; }

private:
  std::string text_;
};

class AstJsonStringAttribute : public AstAttribute {
public:
  explicit AstJsonStringAttribute(std::string value)
      : value_(std::move(value)) {}

  AstAttribute *copy() const override {
    return new AstJsonStringAttribute(value_);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "AstJsonStringAttribute";
  }

  const std::string &value() const { return value_; }

private:
  std::string value_;
};

void setAstJsonStringAttribute(SgNode *node, const char *name,
                               std::string value) {
  if (node == nullptr) {
    return;
  }
  node->setAttribute(name, new AstJsonStringAttribute(std::move(value)));
}

std::string astJsonStringAttribute(SgNode *node, const char *name) {
  if (node == nullptr || !node->attributeExists(name)) {
    return "";
  }
  AstJsonStringAttribute *attribute =
      dynamic_cast<AstJsonStringAttribute *>(node->getAttribute(name));
  return attribute != nullptr ? attribute->value() : "";
}

bool hasAstJsonAttribute(SgNode *node, const char *name) {
  return node != nullptr && node->attributeExists(name);
}

bool isAstJsonExternalFunction(SgFunctionDeclaration *decl) {
  return hasAstJsonAttribute(decl, kAstJsonExternalFunctionAttribute);
}

bool isAstJsonExternalModule(SgModuleStatement *module) {
  return hasAstJsonAttribute(module, kAstJsonExternalModuleAttribute);
}

bool isAstJsonExternalClassDeclaration(SgClassDeclaration *decl) {
  return hasAstJsonAttribute(decl, kAstJsonExternalClassDeclarationAttribute);
}

void markAstJsonExternalFunction(SgFunctionDeclaration *decl,
                                 const std::string &source_file) {
  if (decl == nullptr) {
    return;
  }
  setAstJsonStringAttribute(decl, kAstJsonExternalFunctionAttribute, "true");
  setAstJsonStringAttribute(decl, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

void markAstJsonExternalModule(SgModuleStatement *module,
                               const std::string &source_file) {
  if (module == nullptr) {
    return;
  }
  setAstJsonStringAttribute(module, kAstJsonExternalModuleAttribute, "true");
  setAstJsonStringAttribute(module, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

void markAstJsonExternalClassDeclaration(SgClassDeclaration *decl,
                                         const std::string &source_file) {
  if (decl == nullptr) {
    return;
  }
  setAstJsonStringAttribute(decl, kAstJsonExternalClassDeclarationAttribute,
                            "true");
  setAstJsonStringAttribute(decl, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

SgType *attachJsonTypeText(SgType *type, const JsonValue &json) {
  if (type == nullptr) {
    return type;
  }
  const std::string text = json.stringOr("text");
  if (!text.empty()) {
    type->setAttribute(kAstJsonTypeTextAttribute,
                       new AstJsonTypeTextAttribute(text));
  }
  return type;
}

void attachJsonNodeId(SgNode *node, uint64_t id) {
  if (node == nullptr || id == 0) {
    return;
  }
  preservedJsonNodeIds[node] = id;
}

uint64_t preservedJsonNodeId(SgNode *node) {
  if (node == nullptr) {
    return 0;
  }
  auto found = preservedJsonNodeIds.find(node);
  return found == preservedJsonNodeIds.end() ? 0 : found->second;
}

std::string jsonTypeText(SgType *type) {
  if (type == nullptr) {
    return "";
  }
  if (type->attributeExists(kAstJsonTypeTextAttribute)) {
    if (AstJsonTypeTextAttribute *attribute =
            dynamic_cast<AstJsonTypeTextAttribute *>(
                type->getAttribute(kAstJsonTypeTextAttribute))) {
      return attribute->text();
    }
  }
  if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
    const std::string base = jsonTypeText(member_pointer->get_base_type());
    const std::string class_type =
        jsonTypeText(member_pointer->get_class_type());
    if (!base.empty() && !class_type.empty()) {
      return base + " " + class_type + "::*";
    }
  }
  if (SgPointerType *pointer = isSgPointerType(type)) {
    const std::string base = jsonTypeText(pointer->get_base_type());
    if (!base.empty()) {
      return base + (base.find("::") != std::string::npos ? "*" : " *");
    }
  }
  if (SgReferenceType *reference = isSgReferenceType(type)) {
    const std::string base = jsonTypeText(reference->get_base_type());
    if (!base.empty()) {
      return base + " &";
    }
  }
  if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
    const std::string base = jsonTypeText(reference->get_base_type());
    if (!base.empty()) {
      return base + " &&";
    }
  }
  if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
    if (SgTypedefDeclaration *decl =
            isSgTypedefDeclaration(typedef_type->get_declaration())) {
      return decl->get_name().getString();
    }
    return "";
  }
  if (SgClassType *class_type = isSgClassType(type)) {
    if (SgClassDeclaration *decl =
            isSgClassDeclaration(class_type->get_declaration())) {
      if (isSgTemplateInstantiationDecl(decl)) {
        const std::string unparsed = class_type->unparseToString();
        if (!unparsed.empty()) {
          return unparsed;
        }
      }
      return decl->get_name().getString();
    }
    return "";
  }
  if (SgEnumType *enum_type = isSgEnumType(type)) {
    if (SgEnumDeclaration *decl =
            isSgEnumDeclaration(enum_type->get_declaration())) {
      return decl->get_name().getString();
    }
    return "";
  }
  if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    if (SgNonrealDecl *decl =
            isSgNonrealDecl(nonreal_type->get_declaration())) {
      return decl->get_name().getString();
    }
    return "";
  }
  if (SgTemplateType *template_type = isSgTemplateType(type)) {
    return template_type->get_name().getString();
  }
  return type->unparseToString();
}

void installPointerCache(SgType *base, SgPointerType *pointer) {
  if (base == nullptr || pointer == nullptr) {
    return;
  }
  SgPointerType *cached = base->get_ptr_to();
  if (cached == pointer) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_ptr_to(pointer);
  }
}

void installReferenceCache(SgType *base, SgReferenceType *reference) {
  if (base == nullptr || reference == nullptr) {
    return;
  }
  SgReferenceType *cached = base->get_ref_to();
  if (cached == reference) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_ref_to(reference);
  }
}

void installRvalueReferenceCache(SgType *base,
                                 SgRvalueReferenceType *reference) {
  if (base == nullptr || reference == nullptr) {
    return;
  }
  SgRvalueReferenceType *cached = base->get_rvalue_ref_to();
  if (cached == reference) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_rvalue_ref_to(reference);
  }
}

SgPointerType *buildCachedJsonPointerType(SgType *base, const JsonValue *json) {
  ROSE_ASSERT(base != nullptr);
  SgPointerType *pointer = new SgPointerType(base);
  if (json != nullptr) {
    attachJsonTypeText(pointer, *json);
  }
  installPointerCache(base, pointer);
  return pointer;
}

void installNewExpressionResultType(SgNewExp *expr, SgType *restored_type,
                                    const JsonValue &json) {
  ROSE_ASSERT(expr != nullptr);
  if (json.kind != JsonValue::Kind::Object ||
      json.stringOr("kind") != "SgPointerType") {
    throw std::runtime_error("AST JSON SgNewExp type must be SgPointerType");
  }
  SgPointerType *pointer = isSgPointerType(restored_type);
  if (pointer == nullptr) {
    throw std::runtime_error(
        "AST JSON SgNewExp restored type is not SgPointerType");
  }
  SgType *specified_type = expr->get_specified_type();
  if (specified_type == nullptr) {
    throw std::runtime_error(
        "AST JSON SgNewExp requires specified_type before result type");
  }
  attachJsonTypeText(pointer, json);
  specified_type->set_ptr_to(pointer);
}

void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma);

std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids);

std::string rawFileInfoJson(const Sg_File_Info *info) {
  std::ostringstream out;
  writeFileInfoJson(out, 0, info, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawLocationJson(SgNode *node) {
  std::ostringstream out;
  out << "{\n";
  indent(out, 2);
  out << jsonString("start") << ": "
      << rawFileInfoJson(node != nullptr ? node->get_startOfConstruct()
                                         : nullptr)
      << ",\n";
  indent(out, 2);
  out << jsonString("end") << ": "
      << rawFileInfoJson(node != nullptr ? node->get_endOfConstruct() : nullptr)
      << '\n';
  out << "}";
  return out.str();
}

std::string rawRequiredLocationJson(SgNode *node, const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (node->get_startOfConstruct() == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires startOfConstruct");
  }
  return rawLocationJson(node);
}

std::string rawNodeFlagsJson(SgNode *node) {
  std::vector<std::string> fields;
  fields.push_back(
      rawBoolField("contains_transformation",
                   node != nullptr && node->get_containsTransformation()));
  const SgLocatedNode *located = isSgLocatedNode(node);
  fields.push_back(rawBoolField(
      "contains_transformation_to_surrounding_whitespace",
      located != nullptr &&
          located->get_containsTransformationToSurroundingWhitespace()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

uint64_t
expressionIdFor(SgExpression *expression,
                const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return idFor(ids, expression);
}

std::string
rawExpressionRef(SgExpression *expression,
                 const std::unordered_map<const SgNode *, uint64_t> &ids) {
  const uint64_t id = expressionIdFor(expression, ids);
  if (expression != nullptr && id == 0) {
    std::ostringstream message;
    message << "AST JSON expression reference target was not collected: "
            << expression->sage_class_name();
    if (SgNode *parent = expression->get_parent()) {
      message << " parent=" << parent->sage_class_name();
    }
    if (currentTypeSerializationNode != nullptr) {
      message << " owner=" << currentTypeSerializationNode->sage_class_name()
              << " owner_text="
              << safeNodeText(
                     const_cast<SgNode *>(currentTypeSerializationNode));
    }
    message << " text=" << expression->unparseToString();
    throw std::runtime_error(message.str());
  }
  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("node", id));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawTypeOwnedExpressionRef(
    SgExpression *expression,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (expression == nullptr) {
    return rawExpressionRef(expression, ids);
  }

  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("node", 0));
  fields.push_back(rawStringField("owned_kind", expression->sage_class_name()));
  fields.push_back(jsonString("flags") + ": " + rawNodeFlagsJson(expression));
  fields.push_back(jsonString("location") + ": " + rawLocationJson(expression));
  TypeOwnedExpressionSerializationContext owned_expression_context;
  fields.push_back(jsonString("properties") + ": " +
                   rawNodeProperties(expression, ids));

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawTypeOwnedExpressionListJson(
    const SgExpressionPtrList &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!expressions.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgExpression *expr : expressions) {
      indent(out, 6);
      out << rawTypeOwnedExpressionRef(expr, ids);
      if (++index != expressions.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawTypeOwnedExprListExpJson(
    SgExprListExp *expression_list,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", expression_list != nullptr));
  if (expression_list != nullptr) {
    fields.push_back(
        rawStringField("kind", expression_list->sage_class_name()));
    fields.push_back(jsonString("flags") + ": " +
                     rawNodeFlagsJson(expression_list));
    fields.push_back(jsonString("location") + ": " +
                     rawLocationJson(expression_list));
    fields.push_back(jsonString("properties") + ": " +
                     rawNodeProperties(expression_list, ids));
    fields.push_back(jsonString("expressions") + ": " +
                     rawTypeOwnedExpressionListJson(
                         expression_list->get_expressions(), ids));
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

const SgNode *symbolBasis(const SgSymbol *symbol) {
  if (symbol == nullptr) {
    return nullptr;
  }
  if (const SgAliasSymbol *alias_symbol = isSgAliasSymbol(symbol)) {
    return symbolBasis(alias_symbol->get_alias());
  }
  if (const SgRenameSymbol *rename_symbol = isSgRenameSymbol(symbol)) {
    return rename_symbol->get_declaration();
  }
  if (const SgEnumFieldSymbol *enum_field = isSgEnumFieldSymbol(symbol)) {
    return enum_field->get_declaration();
  }
  if (const SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
    if (const SgLabelStatement *label = label_symbol->get_declaration()) {
      return label;
    }
    return label_symbol->get_symbol_basis();
  }
  if (const SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
    return namespace_symbol->get_declaration();
  }
  if (const SgIntrinsicSymbol *intrinsic_symbol = isSgIntrinsicSymbol(symbol)) {
    return intrinsic_symbol->get_declaration();
  }
  if (const SgModuleSymbol *module_symbol = isSgModuleSymbol(symbol)) {
    return module_symbol->get_declaration();
  }
  if (const SgInterfaceSymbol *interface_symbol = isSgInterfaceSymbol(symbol)) {
    return interface_symbol->get_declaration();
  }
  if (const SgCommonSymbol *common_symbol = isSgCommonSymbol(symbol)) {
    return common_symbol->get_declaration();
  }
  if (const SgVariableSymbol *variable = isSgVariableSymbol(symbol)) {
    return variable->get_declaration();
  }
  if (const SgFunctionSymbol *function = isSgFunctionSymbol(symbol)) {
    return function->get_declaration();
  }
  if (const SgMemberFunctionSymbol *member_function =
          isSgMemberFunctionSymbol(symbol)) {
    return member_function->get_declaration();
  }
  if (const SgClassSymbol *class_symbol = isSgClassSymbol(symbol)) {
    return class_symbol->get_declaration();
  }
  if (const SgEnumSymbol *enum_symbol = isSgEnumSymbol(symbol)) {
    return enum_symbol->get_declaration();
  }
  if (const SgTypedefSymbol *typedef_symbol = isSgTypedefSymbol(symbol)) {
    return typedef_symbol->get_declaration();
  }
  if (const SgNonrealSymbol *nonreal_symbol = isSgNonrealSymbol(symbol)) {
    return nonreal_symbol->get_declaration();
  }
  if (const SgTemplateSymbol *template_symbol = isSgTemplateSymbol(symbol)) {
    return template_symbol->get_declaration();
  }
  if (const SgNode *basis = symbol->get_symbol_basis()) {
    return basis;
  }
  return nullptr;
}

std::string symbolName(const SgSymbol *symbol) {
  const SgNode *basis = symbolBasis(symbol);
  if (const SgInitializedName *name = isSgInitializedName(basis)) {
    return name->get_name().getString();
  }
  if (const SgFunctionDeclaration *decl = isSgFunctionDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgClassDeclaration *decl = isSgClassDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgEnumDeclaration *decl = isSgEnumDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgTypedefDeclaration *decl = isSgTypedefDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgNonrealDecl *decl = isSgNonrealDecl(basis)) {
    return decl->get_name().getString();
  }
  return symbol != nullptr ? symbol->get_name().getString() : "";
}

std::string
rawSymbolRef(SgSymbol *symbol,
             const std::unordered_map<const SgNode *, uint64_t> &ids) {
  const SgNode *basis = symbolBasis(symbol);
  const uint64_t basis_id = idFor(ids, basis);
  const bool external_function =
      basis_id == 0 &&
      (isAstJsonExternalFunction(
           isSgFunctionDeclaration(const_cast<SgNode *>(basis))) ||
       (isSgFunctionDeclaration(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  const bool external_module =
      basis_id == 0 &&
      (isAstJsonExternalModule(
           isSgModuleStatement(const_cast<SgNode *>(basis))) ||
       (isSgModuleStatement(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  const bool external_class =
      basis_id == 0 &&
      (isAstJsonExternalClassDeclaration(
           isSgClassDeclaration(const_cast<SgNode *>(basis))) ||
       (isSgClassDeclaration(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  if (symbol != nullptr && basis_id == 0 && !external_function &&
      !external_module && !external_class) {
    std::ostringstream message;
    message << "AST JSON symbol reference target was not collected: "
            << symbol->get_name().getString();
    if (basis != nullptr) {
      message << " basis=" << basis->sage_class_name()
              << " basis_text=" << safeNodeText(const_cast<SgNode *>(basis));
    }
    throw std::runtime_error(message.str());
  }
  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("symbol_declaration", basis_id));
  fields.push_back(rawStringField("symbol_name", symbolName(symbol)));
  fields.push_back(rawStringField("symbol_kind", symbol->class_name()));
  if (const SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
    fields.push_back(rawIntegerField("label_numeric_label_value",
                                     label_symbol->get_numeric_label_value()));
    fields.push_back(rawIntegerField(
        "label_type", static_cast<int>(label_symbol->get_label_type())));
  }
  if (external_function) {
    fields.push_back(
        jsonString("external_function") + ": " +
        rawExternalFunctionJson(
            isSgFunctionDeclaration(const_cast<SgNode *>(basis)), ids));
  }
  if (external_module) {
    fields.push_back(
        jsonString("external_module") + ": " +
        rawExternalModuleJson(isSgModuleStatement(const_cast<SgNode *>(basis)),
                              ids));
  }
  if (external_class) {
    fields.push_back(jsonString("external_class") + ": " +
                     rawExternalClassDeclarationJson(
                         isSgClassDeclaration(const_cast<SgNode *>(basis))));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool symbolIsLookupPreferred(SgSymbolTable *table, const SgName &name,
                             SgSymbol *symbol) {
  if (table == nullptr || symbol == nullptr) {
    return false;
  }
  if (isSgVariableSymbol(symbol) != nullptr &&
      table->find_variable(name) == symbol) {
    return true;
  }
  if (isSgClassSymbol(symbol) != nullptr && table->find_class(name) == symbol) {
    return true;
  }
  if (isSgEnumSymbol(symbol) != nullptr && table->find_enum(name) == symbol) {
    return true;
  }
  if (isSgEnumFieldSymbol(symbol) != nullptr &&
      table->find_enum_field(name) == symbol) {
    return true;
  }
  if (isSgTypedefSymbol(symbol) != nullptr &&
      table->find_typedef(name) == symbol) {
    return true;
  }
  if (isSgLabelSymbol(symbol) != nullptr && table->find_label(name) == symbol) {
    return true;
  }
  if (isSgNamespaceSymbol(symbol) != nullptr &&
      table->find_namespace(name) == symbol) {
    return true;
  }
  if (isSgFunctionSymbol(symbol) != nullptr &&
      table->find_function(name) == symbol) {
    return true;
  }
  return false;
}

std::string
rawSymbolTableJson(SgScopeStatement *scope,
                   const std::unordered_map<const SgNode *, uint64_t> &ids) {
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  if (table == nullptr || table->get_table() == nullptr) {
    return "[]";
  }

  std::vector<SymbolTableEntryJson> entries;
  for (const std::pair<const SgName, SgSymbol *> &entry : *table->get_table()) {
    SgSymbol *symbol = entry.second;
    if (symbol == nullptr) {
      throw std::runtime_error(
          "AST JSON encountered a null symbol table entry");
    }
    const SgNode *basis = symbolBasis(symbol);
    const uint64_t basis_id = idFor(ids, basis);
    const bool external_basis =
        basis_id == 0 &&
        (isAstJsonExternalFunction(
             isSgFunctionDeclaration(const_cast<SgNode *>(basis))) ||
         isAstJsonExternalModule(
             isSgModuleStatement(const_cast<SgNode *>(basis))) ||
         isAstJsonExternalClassDeclaration(
             isSgClassDeclaration(const_cast<SgNode *>(basis))) ||
         (basis != nullptr &&
          !insideCollectionBoundary(const_cast<SgNode *>(basis)) &&
          (isSgFunctionDeclaration(basis) != nullptr ||
           isSgModuleStatement(basis) != nullptr ||
           isSgClassDeclaration(basis) != nullptr)));
    if (basis_id == 0 && !external_basis) {
      std::ostringstream message;
      message << "AST JSON symbol table target was not collected";
      message << " scope=" << scope->sage_class_name();
      message << " entry=" << entry.first.getString();
      message << " symbol=" << symbol->class_name();
      message << " symbol_name=" << symbol->get_name().getString();
      if (basis != nullptr) {
        message << " basis=" << basis->sage_class_name()
                << " basis_text=" << safeNodeText(const_cast<SgNode *>(basis));
      }
      throw std::runtime_error(message.str());
    }

    std::vector<std::string> fields;
    const std::string entry_name = entry.first.getString();
    const std::string symbol_kind = symbol->class_name();
    const bool lookup_preferred =
        symbolIsLookupPreferred(table, entry.first, symbol);
    fields.push_back(rawStringField("entry_name", entry_name));
    fields.push_back(rawStringField("symbol_kind", symbol_kind));
    fields.push_back(rawBoolField("lookup_preferred", lookup_preferred));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));

    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      if (alias->get_alias() == nullptr) {
        throw std::runtime_error(
            "AST JSON encountered an SgAliasSymbol without an alias target");
      }
      fields.push_back(jsonString("alias_target") + ": " +
                       rawSymbolRef(alias->get_alias(), ids));
      fields.push_back(
          rawBoolField("alias_is_renamed", alias->get_isRenamed()));
      fields.push_back(
          rawStringField("alias_new_name", alias->get_new_name().getString()));
      std::ostringstream causal_nodes;
      causal_nodes << "[";
      for (size_t i = 0; i < alias->get_causal_nodes().size(); ++i) {
        SgNode *causal_node = alias->get_causal_nodes()[i];
        const uint64_t causal_id = idFor(ids, causal_node);
        if (causal_node != nullptr && causal_id == 0) {
          throw std::runtime_error(
              "AST JSON SgAliasSymbol causal node was not collected");
        }
        if (i != 0) {
          causal_nodes << ", ";
        }
        causal_nodes << causal_id;
      }
      causal_nodes << "]";
      fields.push_back(jsonString("alias_causal_nodes") + ": " +
                       causal_nodes.str());
    }
    if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
      if (rename->get_original_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON encountered an SgRenameSymbol without an original "
            "symbol");
      }
      fields.push_back(jsonString("original_symbol") + ": " +
                       rawSymbolRef(rename->get_original_symbol(), ids));
      fields.push_back(rawStringField("rename_new_name",
                                      rename->get_new_name().getString()));
    }
    if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
      fields.push_back(rawStringField(
          "namespace_name", namespace_symbol->get_name().getString()));
      fields.push_back(
          rawBoolField("namespace_is_alias", namespace_symbol->get_isAlias()));
      fields.push_back(rawIntegerField(
          "namespace_alias_declaration",
          idFor(ids, namespace_symbol->get_aliasDeclaration())));
    }

    std::ostringstream entry_out;
    writeRawObject(entry_out, 0, fields, false);
    std::string entry_json = entry_out.str();
    if (!entry_json.empty() && entry_json.back() == '\n') {
      entry_json.pop_back();
    }

    SymbolTableEntryJson serialized;
    serialized.entry_name = entry_name;
    serialized.symbol_kind = symbol_kind;
    serialized.basis_id = basis_id;
    serialized.lookup_preferred = lookup_preferred;
    serialized.json = std::move(entry_json);
    entries.push_back(std::move(serialized));
  }

  std::stable_sort(
      entries.begin(), entries.end(),
      [](const SymbolTableEntryJson &lhs, const SymbolTableEntryJson &rhs) {
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
          return lhs.lookup_preferred && !rhs.lookup_preferred;
        }
        if (lhs.entry_name != rhs.entry_name) {
          return lhs.entry_name < rhs.entry_name;
        }
        return lhs.symbol_kind < rhs.symbol_kind;
      });

  std::ostringstream out;
  out << "[";
  if (!entries.empty()) {
    out << '\n';
    for (size_t i = 0; i < entries.size(); ++i) {
      indent(out, 4);
      out << entries[i].json;
      if (i + 1 != entries.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 2);
  }
  out << "]";
  return out.str();
}

} // namespace AstJson
} // namespace Rose
