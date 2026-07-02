#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

std::filesystem::path temporaryWritePath(const std::filesystem::path &path) {
  static uint64_t counter = 0;
  std::ostringstream name;
  name << "." << path.filename().string() << ".tmp." << processId() << "."
       << ++counter;
  return path.parent_path() / name.str();
}

void writeFile(const std::filesystem::path &path, const std::string &content) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const std::filesystem::path temporary_path = temporaryWritePath(path);
  std::ofstream out(temporary_path);
  if (!out) {
    throw std::runtime_error("failed to open AST JSON file for writing: " +
                             temporary_path.string());
  }
  out << content;
  if (!out) {
    throw std::runtime_error("failed to write AST JSON file: " +
                             temporary_path.string());
  }
  out.close();
  if (!out) {
    throw std::runtime_error("failed to close AST JSON file after writing: " +
                             temporary_path.string());
  }
  std::error_code ec;
  std::filesystem::rename(temporary_path, path, ec);
  if (ec) {
    std::filesystem::remove(temporary_path);
    throw std::runtime_error("failed to publish AST JSON file " +
                             path.string() + ": " + ec.message());
  }
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to read AST JSON file: " + path.string());
  }
  std::ostringstream out;
  out << in.rdbuf();
  if (!in.eof() && in.fail()) {
    throw std::runtime_error("failed while reading AST JSON file: " +
                             path.string());
  }
  return out.str();
}

AstFileRecord parseAstFileJson(const std::string &json,
                               const std::string &expected_checkpoint) {
  JsonValue root = parseJson(json);
  if (root.at("format").asString() != kFormat) {
    throw std::runtime_error("AST JSON format marker is invalid");
  }
  if (root.at("schema_version").asInt() != kSchemaVersion) {
    throw std::runtime_error("AST JSON schema_version is unsupported");
  }

  AstFileRecord result;
  result.root_id = static_cast<uint64_t>(root.at("root_id").asInt());
  result.root_kind = root.at("root_kind").asString();
  result.metadata = root.at("metadata");
  result.checkpoint = result.metadata.at("checkpoint").asString();
  if (result.checkpoint != expected_checkpoint) {
    throw std::runtime_error("AST JSON checkpoint name does not match request");
  }

  const JsonValue &nodes = root.at("nodes");
  if (nodes.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON nodes field is not an array");
  }
  result.nodes.reserve(nodes.array.size());
  for (const JsonValue &node_value : nodes.array) {
    NodeRecord record;
    record.id = static_cast<uint64_t>(node_value.at("id").asInt());
    record.kind = node_value.at("kind").asString();
    record.variant = node_value.at("variant").asInt();
    record.flags = node_value.at("flags");
    record.location = node_value.at("location");
    record.properties = node_value.at("properties");
    record.preprocessing = record.properties.find("preprocessing") != nullptr
                               ? *record.properties.find("preprocessing")
                               : JsonValue::arrayValue({});

    const JsonValue &edges = node_value.at("edges");
    if (edges.kind != JsonValue::Kind::Array) {
      throw std::runtime_error("AST JSON node edges field is not an array");
    }
    for (const JsonValue &edge_value : edges.array) {
      EdgeRecord edge;
      edge.field = edge_value.at("field").asString();
      edge.index = static_cast<uint64_t>(edge_value.intOr("index", 0));
      edge.target = static_cast<uint64_t>(edge_value.at("target").asInt());
      record.edges.push_back(std::move(edge));
    }

    if (!result.index_by_id.emplace(record.id, result.nodes.size()).second) {
      throw std::runtime_error("AST JSON contains duplicate node id " +
                               std::to_string(record.id));
    }
    result.nodes.push_back(std::move(record));
  }

  if (root.at("node_count").asInt() !=
      static_cast<int64_t>(result.nodes.size())) {
    throw std::runtime_error("AST JSON node_count does not match nodes array");
  }
  if (result.root_kind != "SgSourceFile") {
    throw std::runtime_error("AST JSON checkpoint root is not SgSourceFile");
  }
  return result;
}

void appendComparableValue(std::ostream &out, const JsonValue &value);

bool isDerivedTypeTextObject(const JsonValue &value) {
  if (value.kind != JsonValue::Kind::Object ||
      !value.boolOr("present", false)) {
    return false;
  }
  const std::string kind = value.stringOr("kind");
  return kind == "SgPointerType" || kind == "SgPointerMemberType" ||
         kind == "SgReferenceType" || kind == "SgRvalueReferenceType" ||
         kind == "SgArrayType" || kind == "SgModifierType" ||
         kind == "SgFunctionType" || kind == "SgMemberFunctionType";
}

void appendComparableObject(std::ostream &out, const JsonValue &value,
                            const std::set<std::string> &ignored_keys = {}) {
  if (value.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON comparable value is not an object");
  }
  out << '{';
  bool first = true;
  for (const auto &entry : value.object) {
    if (ignored_keys.count(entry.first) != 0 || entry.first == "declaration" ||
        (entry.first == "text" && isDerivedTypeTextObject(value))) {
      continue;
    }
    if (!first) {
      out << ',';
    }
    out << jsonString(entry.first) << ':';
    appendComparableValue(out, entry.second);
    first = false;
  }
  out << '}';
}

void appendComparableValue(std::ostream &out, const JsonValue &value) {
  switch (value.kind) {
  case JsonValue::Kind::Null:
    out << "null";
    break;
  case JsonValue::Kind::Bool:
    out << (value.bool_value ? "true" : "false");
    break;
  case JsonValue::Kind::Number:
    out << value.text;
    break;
  case JsonValue::Kind::String:
    out << jsonString(value.text);
    break;
  case JsonValue::Kind::Array:
    out << '[';
    for (size_t i = 0; i < value.array.size(); ++i) {
      if (i != 0) {
        out << ',';
      }
      appendComparableValue(out, value.array[i]);
    }
    out << ']';
    break;
  case JsonValue::Kind::Object:
    appendComparableObject(out, value);
    break;
  }
}

void appendComparablePreprocessing(std::ostream &out, const JsonValue &value) {
  if (value.kind != JsonValue::Kind::Array) {
    appendComparableValue(out, value);
    return;
  }
  std::vector<std::string> entries;
  entries.reserve(value.array.size());
  for (const JsonValue &entry : value.array) {
    std::ostringstream item;
    appendComparableValue(item, entry);
    entries.push_back(item.str());
  }
  std::sort(entries.begin(), entries.end());
  out << '[';
  for (size_t i = 0; i < entries.size(); ++i) {
    if (i != 0) {
      out << ',';
    }
    out << entries[i];
  }
  out << ']';
}

void appendComparableProperties(std::ostream &out, const JsonValue &value) {
  if (value.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON properties value is not an object");
  }
  out << '{';
  bool first = true;
  for (const auto &entry : value.object) {
    if (entry.first == "unparse") {
      continue;
    }
    if (!first) {
      out << ',';
    }
    out << jsonString(entry.first) << ':';
    if (entry.first == "preprocessing") {
      appendComparablePreprocessing(out, entry.second);
    } else {
      appendComparableValue(out, entry.second);
    }
    first = false;
  }
  out << '}';
}

bool hasEdgeTarget(const NodeRecord &record, const std::string &field,
                   uint64_t target) {
  for (const EdgeRecord &candidate : record.edges) {
    if (candidate.field == field && candidate.target == target) {
      return true;
    }
  }
  return false;
}

uint64_t firstEdgeTarget(const NodeRecord &record, const std::string &field) {
  for (const EdgeRecord &candidate : record.edges) {
    if (candidate.field == field) {
      return candidate.target;
    }
  }
  return 0;
}

bool isScopeDerivedFromParentChain(
    const NodeRecord &record, const EdgeRecord &edge,
    const std::unordered_map<uint64_t, const NodeRecord *> &records_by_id) {
  if (edge.field != "scope") {
    return false;
  }

  std::unordered_set<uint64_t> seen;
  uint64_t current = firstEdgeTarget(record, "parent");
  while (current != 0 && seen.insert(current).second) {
    if (current == edge.target) {
      return true;
    }

    auto found = records_by_id.find(current);
    if (found == records_by_id.end()) {
      return false;
    }
    current = firstEdgeTarget(*found->second, "parent");
  }

  return false;
}

bool isInitializedNameScopeDerivedFromDeclaration(
    const NodeRecord &record, const EdgeRecord &edge,
    const std::unordered_map<uint64_t, const NodeRecord *> &records_by_id) {
  if (record.kind != "SgInitializedName" || edge.field != "scope") {
    return false;
  }
  const uint64_t decl_id = firstEdgeTarget(record, "declptr");
  if (decl_id == 0) {
    return false;
  }
  auto found = records_by_id.find(decl_id);
  if (found == records_by_id.end()) {
    return false;
  }
  const NodeRecord &decl_record = *found->second;
  return hasEdgeTarget(decl_record, "scope", edge.target) ||
         hasEdgeTarget(decl_record, "parent", edge.target);
}

bool ignoreComparableEdge(
    const NodeRecord &record, const EdgeRecord &edge,
    const std::unordered_map<uint64_t, const NodeRecord *> &records_by_id) {
  if (edge.field == "parent") {
    return true;
  }
  if (edge.field == "scope" && hasEdgeTarget(record, "parent", edge.target)) {
    return true;
  }
  if (isScopeDerivedFromParentChain(record, edge, records_by_id)) {
    return true;
  }
  if (isInitializedNameScopeDerivedFromDeclaration(record, edge,
                                                   records_by_id)) {
    return true;
  }
  if (edge.target == record.id &&
      (edge.field == "firstNondefiningDeclaration" ||
       edge.field == "definingDeclaration")) {
    return true;
  }
  return false;
}

std::string semanticSignature(const AstFileRecord &ast) {
  std::ostringstream out;
  out << "format=" << kFormat << '\n';
  out << "schema=" << kSchemaVersion << '\n';
  out << "checkpoint=" << ast.checkpoint << '\n';
  out << "root=" << ast.root_id << ":" << ast.root_kind << '\n';
  out << "metadata=";
  appendComparableObject(out, ast.metadata);
  out << '\n';

  std::unordered_map<uint64_t, const NodeRecord *> records_by_id;
  records_by_id.reserve(ast.nodes.size());
  for (const NodeRecord &record : ast.nodes) {
    records_by_id.emplace(record.id, &record);
  }

  for (const NodeRecord &record : ast.nodes) {
    out << "node " << record.id << ' ' << record.kind << ' ' << record.variant
        << '\n';
    out << "flags=";
    appendComparableObject(out, record.flags);
    out << '\n';
    out << "location=";
    appendComparableObject(out, record.location);
    out << '\n';
    out << "properties=";
    appendComparableProperties(out, record.properties);
    out << '\n';

    std::vector<EdgeRecord> edges;
    for (const EdgeRecord &edge : record.edges) {
      if (!ignoreComparableEdge(record, edge, records_by_id)) {
        edges.push_back(edge);
      }
    }
    std::sort(edges.begin(), edges.end(),
              [](const EdgeRecord &lhs, const EdgeRecord &rhs) {
                if (lhs.field != rhs.field) {
                  return lhs.field < rhs.field;
                }
                if (lhs.index != rhs.index) {
                  return lhs.index < rhs.index;
                }
                return lhs.target < rhs.target;
              });
    out << "edges=[";
    for (size_t i = 0; i < edges.size(); ++i) {
      if (i != 0) {
        out << ',';
      }
      out << edges[i].field << ':' << edges[i].index << "->" << edges[i].target;
    }
    out << "]\n";
  }
  return out.str();
}

std::filesystem::path checkpointPath(SgSourceFile *file, Checkpoint checkpoint,
                                     const Options &options) {
  static uint64_t counter = 0;
  const std::string source_path = file->get_sourceFileNameWithPath();
  const std::string base = sanitizePathComponent(
      StringUtility::stripPathFromFileName(file->get_sourceFileNameWithPath()));
  const std::string source_id = hex64(fnv1a64(source_path));
  std::ostringstream name;
  name << "rex_ast_" << checkpointName(checkpoint) << "_" << base << "_"
       << source_id << "_p" << processId() << "_" << ++counter << ".json";
  return std::filesystem::path(options.outputDirectory) / name.str();
}

SgProject *owningProject(SgSourceFile *file) {
  if (file == nullptr || file->get_parent() == nullptr) {
    return nullptr;
  }
  SgFileList *file_list = isSgFileList(file->get_parent());
  if (file_list == nullptr) {
    return nullptr;
  }
  return isSgProject(file_list->get_parent());
}

bool nodeParentChainReferencesSourceFile(SgNode *node, SgSourceFile *file) {
  if (node == nullptr || file == nullptr) {
    return false;
  }
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (current == file) {
      return true;
    }
  }
  return false;
}

bool symbolReferencesSourceFile(SgSymbol *symbol, SgSourceFile *file) {
  if (symbol == nullptr || file == nullptr) {
    return false;
  }
  if (nodeParentChainReferencesSourceFile(symbol, file)) {
    return true;
  }
  return nodeParentChainReferencesSourceFile(
      const_cast<SgNode *>(symbolBasis(symbol)), file);
}

struct SourceFileReferenceScanner {
  explicit SourceFileReferenceScanner(SgSourceFile *file) : file(file) {}

  bool references(SgType *type) {
    if (type == nullptr || file == nullptr) {
      return false;
    }
    if (!seen_types.insert(type).second) {
      return false;
    }
    if (nodeParentChainReferencesSourceFile(type, file)) {
      return true;
    }

    if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
      return references(member_pointer->get_base_type()) ||
             references(member_pointer->get_class_type());
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      return references(pointer->get_base_type());
    }
    if (SgReferenceType *reference = isSgReferenceType(type)) {
      return references(reference->get_base_type());
    }
    if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
      return references(reference->get_base_type());
    }
    if (SgTypeString *string_type = isSgTypeString(type)) {
      return references(string_type->get_lengthExpression()) ||
             references(string_type->get_type_kind());
    }
    if (SgTypeComplex *complex_type = isSgTypeComplex(type)) {
      return references(complex_type->get_base_type()) ||
             references(complex_type->get_type_kind());
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      return references(array->get_base_type()) ||
             references(array->get_index()) ||
             references(array->get_dim_info());
    }
    if (SgModifierType *modifier = isSgModifierType(type)) {
      return references(modifier->get_base_type());
    }
    if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      return nodeParentChainReferencesSourceFile(
          typedef_type->get_declaration(), file);
    }
    if (SgClassType *class_type = isSgClassType(type)) {
      return nodeParentChainReferencesSourceFile(class_type->get_declaration(),
                                                 file);
    }
    if (SgEnumType *enum_type = isSgEnumType(type)) {
      return nodeParentChainReferencesSourceFile(enum_type->get_declaration(),
                                                 file);
    }
    if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      return nodeParentChainReferencesSourceFile(
          nonreal_type->get_declaration(), file);
    }
    if (SgTemplateType *template_type = isSgTemplateType(type)) {
      return references(template_type->get_class_type()) ||
             references(template_type->get_parent_class_type());
    }
    if (SgMemberFunctionType *member_type = isSgMemberFunctionType(type)) {
      if (references(member_type->get_return_type()) ||
          references(member_type->get_class_type())) {
        return true;
      }
      for (SgType *argument : member_type->get_arguments()) {
        if (references(argument)) {
          return true;
        }
      }
      return false;
    }
    if (SgFunctionType *function_type = isSgFunctionType(type)) {
      if (references(function_type->get_return_type())) {
        return true;
      }
      for (SgType *argument : function_type->get_arguments()) {
        if (references(argument)) {
          return true;
        }
      }
      return false;
    }
    if (SgDeclType *decl_type = isSgDeclType(type)) {
      return references(decl_type->get_base_expression());
    }

    return false;
  }

  bool references(SgExpression *expr) {
    if (expr == nullptr || file == nullptr) {
      return false;
    }
    if (!seen_expressions.insert(expr).second) {
      return false;
    }
    if (nodeParentChainReferencesSourceFile(expr, file)) {
      return true;
    }

    if (referencesExpressionLocalLinks(expr)) {
      return true;
    }

    RoseAst ast(expr);
    for (RoseAst::iterator it = ast.begin().withoutNullValues();
         it != ast.end(); ++it) {
      SgExpression *child = isSgExpression(*it);
      if (child == nullptr || child == expr) {
        continue;
      }
      if (references(child)) {
        return true;
      }
      if (isSgBaseClass(*it) != nullptr) {
        it.skipChildrenOnForward();
      }
    }
    return false;
  }

  bool referencesExpressionLocalLinks(SgExpression *expr) {
    if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol(), file);
    }
    if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol(), file) ||
             references(ref->get_type());
    }
    if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol(), file) ||
             references(ref->get_type());
    }
    if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol_i(), file) ||
             references(ref->get_type());
    }
    if (SgTemplateMemberFunctionRefExp *ref =
            isSgTemplateMemberFunctionRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol_i(), file) ||
             references(ref->get_type());
    }
    if (SgClassNameRefExp *ref = isSgClassNameRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol(), file) ||
             references(ref->get_type());
    }
    if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
      return symbolReferencesSourceFile(ref->get_symbol(), file) ||
             references(ref->get_type());
    }
    if (SgThisExp *this_expr = isSgThisExp(expr)) {
      return symbolReferencesSourceFile(this_expr->get_class_symbol(), file) ||
             symbolReferencesSourceFile(this_expr->get_nonreal_symbol(),
                                        file) ||
             references(this_expr->get_type());
    }
    if (SgEnumVal *value = isSgEnumVal(expr)) {
      return nodeParentChainReferencesSourceFile(value->get_declaration(),
                                                 file) ||
             references(value->get_type());
    }
    if (SgConstructorInitializer *init = isSgConstructorInitializer(expr)) {
      return nodeParentChainReferencesSourceFile(init->get_declaration(),
                                                 file) ||
             references(init->get_type());
    }
    if (SgNewExp *new_expr = isSgNewExp(expr)) {
      return references(new_expr->get_specified_type());
    }
    if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
      return references(type_expr->get_type());
    }
    return references(expr->get_type());
  }

  SgSourceFile *file = nullptr;
  std::unordered_set<SgType *> seen_types;
  std::unordered_set<SgExpression *> seen_expressions;
};

void purgeStaleTypeEntriesFromTable(SgSymbolTable *table,
                                    SgSourceFile *old_file,
                                    bool function_type_table) {
  if (table == nullptr || table->get_table() == nullptr ||
      old_file == nullptr) {
    return;
  }

  std::vector<SgName> stale_names;
  std::set<std::string> seen_names;
  for (const auto &entry : *table->get_table()) {
    SgFunctionTypeSymbol *symbol = isSgFunctionTypeSymbol(entry.second);
    if (symbol == nullptr || symbol->get_type() == nullptr) {
      continue;
    }
    SourceFileReferenceScanner scanner(old_file);
    if (!scanner.references(symbol->get_type())) {
      continue;
    }
    const std::string name = entry.first.getString();
    if (seen_names.insert(name).second) {
      stale_names.push_back(entry.first);
    }
  }

  for (const SgName &name : stale_names) {
    if (function_type_table) {
      table->remove_function_type(name);
    } else {
      SgNode::get_globalTypeTable()->remove_type(name);
    }
  }
}

void purgeStaleCheckpointTypeCaches(SgSourceFile *old_file) {
  if (old_file == nullptr) {
    return;
  }

  SgTypeTable *type_table = SgNode::get_globalTypeTable();
  ROSE_ASSERT(type_table != nullptr);
  purgeStaleTypeEntriesFromTable(type_table->get_type_table(), old_file, false);

  SgFunctionTypeTable *function_type_table =
      SgNode::get_globalFunctionTypeTable();
  ROSE_ASSERT(function_type_table != nullptr);
  purgeStaleTypeEntriesFromTable(function_type_table->get_function_type_table(),
                                 old_file, true);
}

void replaceFileInProject(SgSourceFile *old_file, SgSourceFile *new_file) {
  ROSE_ASSERT(old_file != nullptr);
  ROSE_ASSERT(new_file != nullptr);
  SgProject *project = owningProject(old_file);
  ROSE_ASSERT(project != nullptr);

  SgFileList *file_list_node = isSgFileList(old_file->get_parent());
  ROSE_ASSERT(file_list_node != nullptr);
  SgFilePtrList &files = project->get_fileList();
  bool replaced = false;
  for (SgFile *&entry : files) {
    if (entry == old_file) {
      entry = new_file;
      replaced = true;
      break;
    }
  }
  ROSE_ASSERT(replaced);

  new_file->set_parent(file_list_node);
  if (new_file->get_globalScope() != nullptr) {
    new_file->get_globalScope()->set_parent(new_file);
  }
  old_file->set_parent(nullptr);
  purgeStaleCheckpointTypeCaches(old_file);
}

std::vector<std::string> commandLineFromMetadata(const JsonValue &metadata) {
  const JsonValue *array = metadata.find("command_line");
  std::vector<std::string> result;
  if (array != nullptr && array->kind == JsonValue::Kind::Array) {
    for (const JsonValue &value : array->array) {
      result.push_back(value.asString());
    }
  }
  if (result.empty()) {
    result.push_back("cc");
    result.push_back(metadata.stringOr("source_file"));
  }
  return result;
}

void restoreFileIdMapFromMetadata(const JsonValue &metadata) {
  const JsonValue &entries = metadata.at("file_id_map");
  if (entries.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON metadata file_id_map is not an array");
  }

  std::map<int, std::string> id_to_name;
  std::map<std::string, int> name_to_id;
  int64_t expected_id = 0;
  for (const JsonValue &entry : entries.array) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map entry is not an object");
    }
    const int64_t id64 = entry.at("id").asInt();
    if (id64 != expected_id) {
      std::ostringstream message;
      message << "AST JSON metadata file_id_map must be dense and sorted: "
              << "expected id " << expected_id << ", found " << id64;
      throw std::runtime_error(message.str());
    }
    if (id64 > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map id overflows int");
    }
    const std::string filename = entry.at("filename").asString();
    if (filename.empty()) {
      std::ostringstream message;
      message << "AST JSON metadata file_id_map has empty filename for id "
              << id64;
      throw std::runtime_error(message.str());
    }
    if (!name_to_id.emplace(filename, static_cast<int>(id64)).second) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map has duplicate filename: " + filename);
    }
    id_to_name[static_cast<int>(id64)] = filename;
    ++expected_id;
  }
  Sg_File_Info::set_fileidtoname_map(id_to_name);
  Sg_File_Info::set_nametofileid_map(name_to_id);
}

} // namespace AstJson
} // namespace Rose
